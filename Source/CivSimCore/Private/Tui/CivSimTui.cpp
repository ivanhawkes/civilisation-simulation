#include <Tui/CivSimTui.h>
#include <algorithm>
#include <cctype>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/flexbox_config.hpp>
#include <sstream>
#include <string>
#include <vector>


using namespace ftxui;
namespace sim = CivSimCore;


namespace
{
// One log line at a time, split into whole code points so a wide terminal wraps the
// text without splitting a UTF-8 sequence in half.
Element WrapLogText(const std::string &value)
{
	Elements lines;
	FlexboxConfig config;
	config.SetGap(0, 0);

	std::size_t lineStart = 0;
	while (true)
	{
		const auto lineEnd = value.find('\n', lineStart);
		const std::string_view line(
		    value.data() + lineStart,
		    (lineEnd == std::string::npos ? value.size() : lineEnd) - lineStart);

		Elements characters;
		for (std::size_t offset = 0; offset < line.size();)
		{
			const auto firstByte = static_cast<unsigned char>(line[offset]);
			std::size_t characterLength = 1;
			if ((firstByte & 0xE0) == 0xC0)
				characterLength = 2;
			else if ((firstByte & 0xF0) == 0xE0)
				characterLength = 3;
			else if ((firstByte & 0xF8) == 0xF0)
				characterLength = 4;
			characterLength = std::min(characterLength, line.size() - offset);
			characters.push_back(text(line.substr(offset, characterLength)));
			offset += characterLength;
		}

		if (characters.empty())
			characters.push_back(text(""));

		lines.push_back(flexbox(std::move(characters), config));

		if (lineEnd == std::string::npos)
			break;

		lineStart = lineEnd + 1;
	}

	return vbox(std::move(lines));
}

std::string Trim(const std::string &text)
{
	const auto begin = text.find_first_not_of(" \t");
	if (begin == std::string::npos)
	{
		return {};
	}

	const auto end = text.find_last_not_of(" \t");

	return text.substr(begin, end - begin + 1);
}

std::string PadRight(const std::string &text, std::size_t width)
{
	if (text.size() >= width)
	{
		return text;
	}

	return text + std::string(width - text.size(), ' ');
}
} // namespace


// --- CATPPUCCIN MOCHA PALETTE DEFINITIONS ---
namespace Mocha
{
const auto Mauve = color(Color::RGB(203, 166, 247));
const auto Red = color(Color::RGB(243, 139, 168));
const auto Peach = color(Color::RGB(250, 179, 135));
const auto Yellow = color(Color::RGB(249, 226, 175));
const auto Green = color(Color::RGB(166, 227, 161));
const auto Blue = color(Color::RGB(137, 180, 250));
const auto Lavender = color(Color::RGB(180, 190, 254));

const auto Text = color(Color::RGB(205, 214, 244));
const auto Subtext0 = color(Color::RGB(166, 173, 200));
const auto Surface0 = color(Color::RGB(54, 58, 79));
const auto BaseBg = bgcolor(Color::RGB(30, 30, 46));
} // namespace Mocha


int main()
{
	CivSimTui repl;
	repl.Run();

	return 0;
}


CivSimTui::CivSimTui() : logFile_(OpenSessionLog())
{
	commands = {
	    "help",   "status", "version", "load",   "generate", "list",  "show",
	    "prices", "plan",   "tick",    "skills", "upgrade",  "clear", "quit",
	};

	startTime = std::chrono::steady_clock::now();
}


std::ofstream CivSimTui::OpenSessionLog()
{
	// Relative to the working tree, so tail -f build/tui-session.log follows a running
	// session from another pane. A working tree without a build directory logs to the
	// screen only.
	std::ofstream file("build/tui-session.log", std::ios::app);

	return file;
}


void CivSimTui::AppendSessionLog(const std::string &text)
{
	if (!logFile_.is_open())
	{
		return;
	}

	logFile_ << text << '\n';
	logFile_.flush();
}


CivSimTui::~CivSimTui()
{
	StopRefreshThread();
}


void CivSimTui::StopRefreshThread()
{
	runThread = false;

	if (refreshThread.joinable())
	{
		refreshThread.join();
	}
}


unsigned CivSimTui::ParseCount(const std::string &text, unsigned fallback, unsigned maximum)
{
	if (text.empty())
	{
		return fallback;
	}

	try
	{
		const unsigned long long value = std::stoull(text);

		if (value == 0)
		{
			return fallback;
		}

		return static_cast<unsigned>(std::min(value, static_cast<unsigned long long>(maximum)));
	}
	catch (const std::exception &)
	{
		return fallback;
	}
}


bool CivSimTui::LoadConfiguration(const std::string &directory, std::string &errorMessage)
{
	if (!sim::LoadConfigAges(directory + "/ages.yaml", ages, errorMessage))
	{
		return false;
	}

	if (!sim::LoadConfigCategories(directory + "/categories.yaml", categories, errorMessage))
	{
		return false;
	}

	if (!sim::LoadConfigGameMode(directory + "/full.yaml", gameMode, errorMessage))
	{
		return false;
	}

	if (!sim::LoadUniverseNamePools(universeFile, names, errorMessage))
	{
		return false;
	}

	std::string progressionError;
	if (!progression.Build(gameMode.skills, progressionError))
	{
		errorMessage = "the skill tree was rejected: " + progressionError;
		return false;
	}

	progression.SetResources(20);
	dataDirectory = directory;
	configLoaded = true;

	return true;
}


std::string CivSimTui::Evaluate(const std::string &line)
{
	const auto space = line.find_first_of(" \t");
	const std::string command = Trim(space == std::string::npos ? line : line.substr(0, space));
	const std::string argument =
	    space == std::string::npos ? std::string() : Trim(line.substr(space + 1));

	std::string result;

	if (command == "quit" || command == "exit")
	{
		quitRequested = true;
		result = "Exiting.";
	}
	else if (command == "help")
	{
		result = CommandHelp();
	}
	else if (command == "version")
	{
		result = CommandVersion();
	}
	else if (command == "status")
	{
		result = CommandStatus();
	}
	else if (command == "clear")
	{
		result.clear();
	}
	else if (command == "load")
	{
		result = CommandLoad(argument);
	}
	else if (command == "generate")
	{
		result = CommandGenerate(argument);
	}
	else if (command == "list")
	{
		result = CommandList();
	}
	else if (command == "show")
	{
		result = CommandShow(argument);
	}
	else if (command == "prices")
	{
		result = CommandPrices(argument);
	}
	else if (command == "plan")
	{
		result = CommandPlan(argument);
	}
	else if (command == "tick")
	{
		result = CommandTick(argument);
	}
	else if (command == "skills")
	{
		result = CommandSkills();
	}
	else if (command == "upgrade")
	{
		result = CommandUpgrade(argument);
	}
	else
	{
		promptMark = " \u2718 ";
		return "Unknown command: '" + command + "'. Type help, or press [Tab] to complete.";
	}

	promptMark = command == "quit" || command == "exit" ? " \u2718 " : " \u2714 ";

	return result;
}


std::string CivSimTui::CommandHelp()
{
	return "Commands:\n"
	       "  help            this list\n"
	       "  status          configuration, universe and skill tree state\n"
	       "  version         front-end version\n"
	       "  load [dir]      read ages, categories, full game mode and names (dir "
	       "defaults to data/core-config)\n"
	       "  generate [n] [seed]   generate n civilisations (default 8, seed 1)\n"
	       "  list            every civilisation\n"
	       "  show <Civ-01>   one civilisation in detail\n"
	       "  prices [n]      the n most over priced goods (default 10)\n"
	       "  plan [n]        the n best trade routes (default 10)\n"
	       "  tick [n]        advance n ticks (default 1)\n"
	       "  skills          the skill tree with tiers and costs\n"
	       "  upgrade <id>    buy one tier of a skill\n"
	       "  clear           clear the log\n"
	       "  quit            leave (Ctrl-D works too)";
}


std::string CivSimTui::CommandVersion()
{
	std::ostringstream line;
	line << "CivSim core development front-end, C++" << __cplusplus / 100;

	return line.str();
}


std::string CivSimTui::CommandStatus()
{
	std::ostringstream status;
	status << "data " << dataDirectory << (configLoaded ? " (loaded)" : " (not loaded)") << "\n"
	       << "ages " << ages.size() << ", categories " << categories.size() << ", items "
	       << gameMode.items.size() << ", skills " << gameMode.skills.size() << ", names "
	       << names.NameCount() << " in " << names.PoolCount() << " pools\n"
	       << (configLoaded ? simulation.StatusLine() : std::string("no universe: run generate"))
	       << "\n"
	       << "skill tree: " << progression.Summary();

	return status.str();
}


std::string CivSimTui::CommandLoad(const std::string &directory)
{
	const std::string target = directory.empty() ? dataDirectory : directory;

	std::string errorMessage;
	if (!LoadConfiguration(target, errorMessage))
	{
		return "Load failed: " + errorMessage;
	}

	std::ostringstream result;
	result << "Loaded " << target << ": ages " << ages.size() << ", categories " << categories.size()
	       << ", items " << gameMode.items.size() << ", skills " << gameMode.skills.size() << ", names "
	       << names.NameCount() << " in " << names.PoolCount() << " pools.";

	return result.str();
}


std::string CivSimTui::CommandGenerate(const std::string &argument)
{
	if (!configLoaded)
	{
		return "Run load first: the configuration is not loaded.";
	}

	std::istringstream stream(argument);
	std::string countText;
	std::string seedText;
	stream >> countText >> seedText;

	sim::GenerateOptions options;
	options.civilisationCount = ParseCount(countText, 8, 500);
	options.seed = seedText.empty() ? 1u : ParseCount(seedText, 1, 4000000000u);

	std::string errorMessage;
	if (!simulation.Generate(ages, categories, gameMode.items, names, options, errorMessage))
	{
		return "Generation failed: " + errorMessage;
	}

	std::ostringstream result;
	result << "Generated " << simulation.GetCivilisations().size() << " civilisations with seed "
	       << options.seed << ", " << gameMode.items.size() << " goods.\n"
	       << simulation.StatusLine();

	return result.str();
}


std::string CivSimTui::CommandList()
{
	if (!simulation.IsGenerated())
	{
		return "No universe: run generate first.";
	}

	std::ostringstream list;
	list << "id      name                      pool     age                   pop      "
	        "goods treasury\n";
	for (const auto &civ : simulation.GetCivilisations())
	{
		list << PadRight(civ.id, 8) << PadRight(civ.name, 24) << PadRight(civ.pool, 8)
		     << PadRight(civ.ageId, 22) << "pop " << PadRight(sim::FormatNumber(civ.population), 7)
		     << " goods " << PadRight(std::to_string(civ.production.size()), 4) << " treasury "
		     << civ.treasury << "\n";
	}

	list << "population is in world units: demand and production are units of goods per day.\n";

	return list.str();
}


std::string CivSimTui::CommandShow(const std::string &argument)
{
	if (argument.empty())
	{
		return "Usage: show <Civ-01>";
	}

	if (!simulation.IsGenerated())
	{
		return "No universe: run generate first.";
	}

	return simulation.DescribeCivilisation(argument);
}


std::string CivSimTui::CommandPrices(const std::string &argument)
{
	if (!simulation.IsGenerated())
	{
		return "No universe: run generate first.";
	}

	struct Priced
	{
		double ratio;
		std::string itemId;
		double basePrice;
		double price;
	};

	std::vector<Priced> priced;
	for (const auto &item : simulation.GetItems())
	{
		const double basePrice = sim::Simulation::BasePrice(item);
		if (basePrice <= 0.0)
		{
			continue;
		}

		const double price = simulation.GetPrice(item.itemId);

		priced.push_back({price / basePrice, item.itemId, basePrice, price});
	}

	std::sort(
	    priced.begin(), priced.end(),
	    [](const Priced &left, const Priced &right)
	    {
		    if (left.ratio != right.ratio)
		    {
			    return left.ratio > right.ratio;
		    }

		    return left.itemId < right.itemId;
	    });

	const std::size_t limit = ParseCount(argument, 10, static_cast<unsigned>(priced.size()));

	std::ostringstream table;
	table << "goods by market price over base price\n";
	for (std::size_t index = 0; index < std::min(limit, priced.size()); ++index)
	{
		const Priced &entry = priced[index];
		table << "  " << PadRight(entry.itemId, 26) << "base "
		      << PadRight(sim::FormatNumber(entry.basePrice), 12) << "market "
		      << PadRight(sim::FormatNumber(entry.price), 14) << "x" << sim::FormatNumber(entry.ratio)
		      << "\n";
	}

	// The scarce half of the table alone reads as if everything were unobtainium: the
	// cheap end is the other half of the market.
	if (priced.size() > limit + 5)
	{
		table << "  ... and the cheapest in demand\n";
		for (std::size_t index = priced.size() - 5; index < priced.size(); ++index)
		{
			const Priced &entry = priced[index];
			table << "  " << PadRight(entry.itemId, 26) << "base "
			      << PadRight(sim::FormatNumber(entry.basePrice), 12) << "market "
			      << PadRight(sim::FormatNumber(entry.price), 14) << "x"
			      << sim::FormatNumber(entry.ratio) << "\n";
		}
	}

	return table.str();
}


std::string CivSimTui::CommandPlan(const std::string &argument)
{
	if (!simulation.IsGenerated())
	{
		return "No universe: run generate first.";
	}

	const std::size_t limit = ParseCount(argument, 10, 100);
	const std::vector<sim::TradeRoute> routes = simulation.PlanTrade(static_cast<unsigned>(limit));

	if (routes.empty())
	{
		return "No route pays its freight: every market is flat. Run tick to move goods.";
	}

	std::ostringstream plan;
	plan << "best routes on tick " << simulation.GetTick() << "\n"
	     << "  a merchant buys where the good is cheap and sells where it is scarce\n  "
	     << PadRight("good", 24) << PadRight("seller", 9) << PadRight("buyer", 9) << PadRight("qty", 9)
	     << PadRight("buy at", 12) << PadRight("sell at", 12) << PadRight("freight", 11) << "profit\n";
	for (const auto &route : routes)
	{
		plan << "  " << PadRight(route.itemId, 24) << PadRight(route.sellerId, 9)
		     << PadRight(route.buyerId, 9) << PadRight(sim::FormatNumber(route.quantity), 9)
		     << PadRight(sim::FormatNumber(route.unitSellPrice), 12)
		     << PadRight(sim::FormatNumber(route.unitBuyPrice), 12)
		     << PadRight(sim::FormatNumber(route.freight), 11) << sim::FormatNumber(route.profit)
		     << "\n";
	}

	return plan.str();
}


std::string CivSimTui::CommandTick(const std::string &argument)
{
	if (!simulation.IsGenerated())
	{
		return "No universe: run generate first.";
	}

	const unsigned ticks = ParseCount(argument, 1, 1000);

	std::string errorMessage;
	for (unsigned index = 0; index < ticks; ++index)
	{
		if (!simulation.AdvanceTick(errorMessage))
		{
			return "Tick failed: " + errorMessage;
		}
	}

	std::ostringstream result;
	result << "Advanced " << ticks << " tick" << (ticks == 1 ? "" : "s") << ".\n"
	       << simulation.StatusLine();

	return result.str();
}


std::string CivSimTui::CommandSkills()
{
	if (!configLoaded)
	{
		return "Run load first: the configuration is not loaded.";
	}

	std::ostringstream skills;
	skills << progression.Summary() << "\n";
	for (const auto &entry : progression.GetRegistry())
	{
		skills << "  " << progression.Describe(entry.first) << "\n";
	}

	return skills.str();
}


std::string CivSimTui::CommandUpgrade(const std::string &argument)
{
	if (argument.empty())
	{
		return "Usage: upgrade <skillId>";
	}

	std::string errorMessage;
	if (!progression.TryUpgrade(argument, errorMessage))
	{
		return errorMessage;
	}

	return "Upgraded " + argument + " to tier " + std::to_string(progression.GetCurrentTier(argument)) +
	       "/" + std::to_string(progression.GetMaxTiers(argument)) + ", " +
	       std::to_string(progression.GetResources()) + " resources left.";
}


void CivSimTui::Run()
{
	ScreenInteractive screen = ScreenInteractive::Fullscreen();

	std::string inputData;
	std::vector<std::string> historyLog = {
	    "CivSim development front-end.", "Type help, then load, then generate 8.",
	    "Press [Tab] to complete, [Up/Down] for history, [PageUp/PageDown] to read the log."};

	logFocusIndex = static_cast<int>(historyLog.size()) - 1;

	for (const std::string &line : historyLog)
	{
		AppendSessionLog(line);
	}

	std::vector<std::string> commandHistory;
	int historyIndex = -1;

	Component inputBox = Input(&inputData, "Type a command...");

	auto appendLine = [&](const std::string &text)
	{
		historyLog.push_back(text);
		AppendSessionLog(text);

		// Keep following the newest line: the frame follows the focused element.
		logFocusIndex = static_cast<int>(historyLog.size()) - 1;
	};

	// The generated data is what this front-end exists to exercise: load it on startup,
	// so the first command can be generate, and report it when the data is missing.
	std::string loadError;
	if (!LoadConfiguration(dataDirectory, loadError))
	{
		appendLine(loadError);
	}

	auto inputHandler = CatchEvent(
	    inputBox,
	    [&](Event event)
	    {
		    if (event == Event::CtrlD)
		    {
			    inputData = "quit";
			    event = Event::Return;
		    }

		    if (event == Event::Tab)
		    {
			    if (!inputData.empty())
			    {
				    const auto match = std::find_if(
				        commands.begin(), commands.end(),
				        [&](const std::string &command) { return command.rfind(inputData, 0) == 0; });

				    if (match != commands.end())
				    {
					    inputData = *match;
				    }
			    }

			    return true;
		    }

		    if (event == Event::PageUp)
		    {
			    logFocusIndex = std::max(0, logFocusIndex - 5);

			    return true;
		    }

		    if (event == Event::PageDown)
		    {
			    logFocusIndex = std::min(static_cast<int>(historyLog.size()) - 1, logFocusIndex + 5);

			    return true;
		    }

		    if (event == Event::End)
		    {
			    logFocusIndex = static_cast<int>(historyLog.size()) - 1;

			    return true;
		    }

		    if (event == Event::ArrowUp)
		    {
			    if (!commandHistory.empty() &&
			        historyIndex < static_cast<int>(commandHistory.size()) - 1)
			    {
				    ++historyIndex;
				    inputData = commandHistory
				        [commandHistory.size() - 1 - static_cast<std::size_t>(historyIndex)];
			    }

			    return true;
		    }

		    if (event == Event::ArrowDown)
		    {
			    if (historyIndex > 0)
			    {
				    --historyIndex;
				    inputData = commandHistory
				        [commandHistory.size() - 1 - static_cast<std::size_t>(historyIndex)];
			    }
			    else if (historyIndex == 0)
			    {
				    historyIndex = -1;
				    inputData.clear();
			    }

			    return true;
		    }

		    if (event == Event::Return)
		    {
			    if (inputData.empty())
			    {
				    return true;
			    }

			    const std::string command = inputData;
			    commandHistory.push_back(command);
			    historyIndex = -1;
			    inputData.clear();

			    historyLog.push_back("> " + command);
			    AppendSessionLog("> " + command);
			    logFocusIndex = static_cast<int>(historyLog.size()) - 1;

			    const std::string output = Evaluate(command);
			    if (output.empty() && command == "clear")
			    {
				    historyLog.clear();
				    logFocusIndex = 0;

				    return true;
			    }

			    if (!output.empty())
			    {
				    appendLine(output);
			    }

			    if (IsQuitRequested())
			    {
				    screen.ExitLoopClosure()();
			    }

			    return true;
		    }

		    return false;
	    });

	auto renderer = Renderer(
	    inputHandler,
	    [&]
	    {
		    Elements logElements;
		    for (int index = 0; index < static_cast<int>(historyLog.size()); ++index)
		    {
			    if (index == logFocusIndex)
			    {
				    logElements.push_back(
				        WrapLogText(historyLog[static_cast<std::size_t>(index)]) | focus);
			    }
			    else
			    {
				    logElements.push_back(WrapLogText(historyLog[static_cast<std::size_t>(index)]));
			    }
		    }

		    const bool failed = PromptMark() == " \u2718 ";

		    auto leftPane =
		        vbox(
		            {vbox(std::move(logElements)) | yframe | flex | border | Mocha::Text,
		             hbox(
		                 {text(PromptMark() + " civsim \u276f ") | bold |
		                      (failed ? Mocha::Red : (inputData.empty() ? Mocha::Mauve : Mocha::Green)),
		                  inputBox->Render() | flex | Mocha::Text}) |
		                 border | Mocha::Surface0}) |
		        flex | Mocha::BaseBg;

		    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		                             std::chrono::steady_clock::now() - startTime)
		                             .count();

		    Elements commandColumn;
		    for (std::size_t index = 0; index < commands.size(); index += 2)
		    {
			    std::string line = "   " + commands[index];
			    if (index + 1 < commands.size())
			    {
				    line += "  " + commands[index + 1];
			    }

			    commandColumn.push_back(text(line) | Mocha::Subtext0);
		    }

		    auto rightPane =
		        vbox(
		            {text(" universe ") | bold | center | Mocha::Lavender,
		             separatorDashed() | Mocha::Surface0,
		             hbox({text("  session: "), text(std::to_string(elapsed) + "s") | Mocha::Yellow}),
		             hbox(
		                 {text("  log lines: "),
		                  text(std::to_string(historyLog.size())) | Mocha::Text}),
		             hbox(
		                 {text("  data: "),
		                  text(configLoaded ? dataDirectory : "not loaded") | Mocha::Text}),
		             separatorDashed() | Mocha::Surface0, text(" tick ") | bold | center | Mocha::Peach,
		             hbox(
		                 {text("  "),
		                  text(
		                      simulation.IsGenerated() ? std::to_string(simulation.GetTick())
		                                               : "no universe") |
		                      Mocha::Text}),
		             hbox(
		                 {text("  worlds: "),
		                  text(std::to_string(simulation.GetCivilisations().size())) | Mocha::Text}),
		             separatorDashed() | Mocha::Surface0,
		             text(" commands ") | bold | center | Mocha::Blue,
		             vbox(std::move(commandColumn))}) |
		        Mocha::BaseBg | border | Mocha::Surface0 | size(WIDTH, EQUAL, 34);

		    return hbox({leftPane, rightPane});
	    });

	refreshThread = std::thread(
	    [&]
	    {
		    while (runThread)
		    {
			    std::this_thread::sleep_for(std::chrono::milliseconds(250));

			    // A custom event asks the screen to re-render, so the elapsed time and
			    // the simulation counters stay live.
			    screen.PostEvent(Event::Custom);
		    }
	    });

	screen.Loop(renderer);

	// Any exit from the loop stops the refresh thread: letting it outlive the screen
	// would post events into a dead screen and leave a joinable thread to terminate
	// the process.
	StopRefreshThread();
}
