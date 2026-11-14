#include "ManifestConsole.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Manifest::Tui
{
namespace sim = Manifest::Core;
using Jettison::DevTools::Console;

namespace
{
unsigned ParseCount(
    std::span<const std::string> arguments, std::size_t index, unsigned fallback, unsigned minimum,
    unsigned maximum)
{
	if (index >= arguments.size())
		return fallback;
	const std::string &text = arguments[index];
	unsigned value{};
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size() || value < minimum || value > maximum)
		throw std::runtime_error{
		    std::format("expected a whole number from {} to {}, got '{}'", minimum, maximum, text)};
	return value;
}

std::string PadRight(const std::string &text, std::size_t width)
{
	return text.size() >= width ? text : text + std::string(width - text.size(), ' ');
}
} // namespace

ManifestConsole::ManifestConsole(std::string dataDirectory, std::string universeFile)
    : m_dataDirectory{std::move(dataDirectory)}, m_universeFile{std::move(universeFile)}
{
	RegisterCommands();
	m_console.CaptureLog();
}

void ManifestConsole::RegisterCommand(
    std::string name, std::string help, std::size_t minimum, std::size_t maximum, Handler run)
{
	m_console.RegisterCommand(
	    std::move(name), help,
	    [minimum, maximum, help, run = std::move(run)](Console &console, Arguments arguments)
	    {
		    if (arguments.size() < minimum || arguments.size() > maximum)
			    throw std::runtime_error{std::format("usage: {}", help)};
		    console.Print(run(arguments));
	    });
}

void ManifestConsole::RegisterCommands()
{
	RegisterCommand(
	    "version", "version: frontend version", 0, 0, [this](Arguments) { return CommandVersion(); });
	RegisterCommand(
	    "status", "status: configuration, universe and skill tree state", 0, 0,
	    [this](Arguments) { return CommandStatus(); });
	RegisterCommand(
	    "load", "load [directory]: reads the generated configuration and names", 0, 1,
	    [this](Arguments arguments) { return CommandLoad(arguments.empty() ? "" : arguments[0]); });
	RegisterCommand(
	    "generate", "generate [count] [seed]: generates worlds (default 8, seed 1)", 0, 2,
	    [this](Arguments arguments) { return CommandGenerate(arguments); });
	RegisterCommand(
	    "list", "list: lists every civilisation", 0, 0, [this](Arguments) { return CommandList(); });
	RegisterCommand(
	    "show", "show <id or name>: describes a civilisation; quote names with spaces", 1, 1,
	    [this](Arguments arguments) { return CommandShow(arguments[0]); });
	RegisterCommand(
	    "prices", "prices [count]: most overpriced goods (default 10, maximum 1000)", 0, 1,
	    [this](Arguments arguments) { return CommandPrices(arguments); });
	RegisterCommand(
	    "plan", "plan [count]: best trade routes (default 10, maximum 100)", 0, 1,
	    [this](Arguments arguments) { return CommandPlan(arguments); });
	RegisterCommand(
	    "tick", "tick [count]: advances the simulation (default 1, maximum 1000)", 0, 1,
	    [this](Arguments arguments) { return CommandTick(arguments); });
	RegisterCommand(
	    "skills", "skills: skill tiers and costs", 0, 0, [this](Arguments) { return CommandSkills(); });
	RegisterCommand(
	    "upgrade", "upgrade <skillId>: buys one tier", 1, 1,
	    [this](Arguments arguments) { return CommandUpgrade(arguments[0]); });
	const auto quit = [this](Arguments)
	{
		m_quitRequested = true;
		return std::string{"Exiting."};
	};
	RegisterCommand("quit", "quit: ends the session (Ctrl-D works too)", 0, 0, quit);
	RegisterCommand("exit", "exit: alias for quit", 0, 0, quit);
}

std::string ManifestConsole::CommandVersion()
{
	std::ostringstream line;
	line << "Manifest core development front-end, C++" << __cplusplus / 100;

	return line.str();
}


std::string ManifestConsole::CommandStatus()
{
	std::ostringstream status;
	status << "data " << m_dataDirectory << (m_configLoaded ? " (loaded)" : " (not loaded)") << "\n"
	       << "ages " << m_ages.size() << ", categories " << m_categories.size() << ", items "
	       << m_gameMode.items.size() << ", skills " << m_gameMode.skills.size() << ", names "
	       << m_names.NameCount() << " in " << m_names.PoolCount() << " pools\n"
	       << (m_configLoaded ? m_simulation.StatusLine() : std::string("no universe: run generate"))
	       << "\n"
	       << "skill tree: " << m_progression.Summary();

	return status.str();
}


std::string ManifestConsole::CommandLoad(const std::string &directory)
{
	const std::string target = directory.empty() ? m_dataDirectory : directory;
	sim::Ages ages;
	sim::Categories categories;
	sim::GameModeNode gameMode;
	sim::UniverseNamePools names;
	sim::ProgressionManager progression;
	std::string errorMessage;
	if (!sim::LoadConfigAges(target + "/ages.yaml", ages, errorMessage) ||
	    !sim::LoadConfigCategories(target + "/categories.yaml", categories, errorMessage) ||
	    !sim::LoadConfigGameMode(target + "/full.yaml", gameMode, errorMessage) ||
	    !sim::LoadUniverseNamePools(m_universeFile, names, errorMessage) ||
	    !progression.Build(gameMode.skills, errorMessage))
	{
		throw std::runtime_error{errorMessage};
	}

	// A failed reload preserves the working session. A successful one replaces all
	// configuration together and discards the universe generated from the previous data.
	progression.SetResources(20);
	m_ages = std::move(ages);
	m_categories = std::move(categories);
	m_gameMode = std::move(gameMode);
	m_names = std::move(names);
	m_progression = std::move(progression);
	m_simulation = {};
	m_dataDirectory = target;
	m_configLoaded = true;

	return std::format(
	    "Loaded {}: ages {}, categories {}, items {}, skills {}, names {} in {} pools.", target,
	    m_ages.size(), m_categories.size(), m_gameMode.items.size(), m_gameMode.skills.size(),
	    m_names.NameCount(), m_names.PoolCount());
}


std::string ManifestConsole::CommandGenerate(Arguments arguments)
{
	if (!m_configLoaded)
	{
		throw std::runtime_error{"Run load first: the configuration is not loaded."};
	}

	sim::GenerateOptions options;
	options.civilisationCount = ParseCount(arguments, 0, 8, 1, 500);
	options.seed = ParseCount(arguments, 1, 1, 0, std::numeric_limits<unsigned>::max());

	std::string errorMessage;
	if (!m_simulation.Generate(m_ages, m_categories, m_gameMode.items, m_names, options, errorMessage))
	{
		throw std::runtime_error{errorMessage};
	}

	std::ostringstream result;
	result << "Generated " << m_simulation.GetCivilisations().size() << " civilisations with seed "
	       << options.seed << ", " << m_gameMode.items.size() << " goods.\n"
	       << m_simulation.StatusLine();

	return result.str();
}


std::string ManifestConsole::CommandList()
{
	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	std::ostringstream list;
	list << "id      name                      pool     age                   pop      "
	        "goods treasury\n";
	for (const auto &civ : m_simulation.GetCivilisations())
	{
		list << PadRight(civ.id, 8) << PadRight(civ.name, 24) << PadRight(civ.pool, 8)
		     << PadRight(civ.ageId, 22) << "pop " << PadRight(sim::FormatNumber(civ.population), 7)
		     << " goods " << PadRight(std::to_string(civ.production.size()), 4) << " treasury "
		     << civ.treasury << "\n";
	}

	list << "population is in world units: demand and production are units of goods per day.\n";

	return list.str();
}


std::string ManifestConsole::CommandShow(const std::string &argument)
{
	if (argument.empty())
	{
		throw std::runtime_error{"Usage: show <Civ-01>"};
	}

	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	if (!m_simulation.FindCivilisation(argument))
	{
		throw std::runtime_error{std::format("no civilisation '{}'", argument)};
	}

	return m_simulation.DescribeCivilisation(argument);
}


std::string ManifestConsole::CommandPrices(Arguments arguments)
{
	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	struct Priced
	{
		double ratio;
		std::string itemId;
		double basePrice;
		double price;
	};

	std::vector<Priced> priced;
	for (const auto &item : m_simulation.GetItems())
	{
		const double basePrice = sim::Simulation::BasePrice(item);
		if (basePrice <= 0.0)
		{
			continue;
		}

		const double price = m_simulation.GetPrice(item.itemId);

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

	const std::size_t limit = ParseCount(arguments, 0, 10, 1, 1000);

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


std::string ManifestConsole::CommandPlan(Arguments arguments)
{
	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	const std::size_t limit = ParseCount(arguments, 0, 10, 1, 100);
	const std::vector<sim::TradeRoute> routes = m_simulation.PlanTrade(static_cast<unsigned>(limit));

	if (routes.empty())
	{
		return "No route pays its freight: every market is flat. Run tick to move goods.";
	}

	std::ostringstream plan;
	plan << "best routes on tick " << m_simulation.GetTick() << "\n"
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


std::string ManifestConsole::CommandTick(Arguments arguments)
{
	if (!m_simulation.IsGenerated())
	{
		throw std::runtime_error{"No universe: run generate first."};
	}

	const unsigned ticks = ParseCount(arguments, 0, 1, 1, 1000);

	std::string errorMessage;
	for (unsigned index = 0; index < ticks; ++index)
	{
		if (!m_simulation.AdvanceTick(errorMessage))
		{
			throw std::runtime_error{errorMessage};
		}
	}

	std::ostringstream result;
	result << "Advanced " << ticks << " tick" << (ticks == 1 ? "" : "s") << ".\n"
	       << m_simulation.StatusLine();

	return result.str();
}


std::string ManifestConsole::CommandSkills()
{
	if (!m_configLoaded)
	{
		throw std::runtime_error{"Run load first: the configuration is not loaded."};
	}

	std::ostringstream skills;
	skills << m_progression.Summary() << "\n";
	for (const auto &entry : m_progression.GetRegistry())
	{
		skills << "  " << m_progression.Describe(entry.first) << "\n";
	}

	return skills.str();
}


std::string ManifestConsole::CommandUpgrade(const std::string &argument)
{
	if (argument.empty())
	{
		throw std::runtime_error{"Usage: upgrade <skillId>"};
	}

	std::string errorMessage;
	if (!m_progression.TryUpgrade(argument, errorMessage))
	{
		throw std::runtime_error{errorMessage};
	}

	return "Upgraded " + argument + " to tier " +
	       std::to_string(m_progression.GetCurrentTier(argument)) + "/" +
	       std::to_string(m_progression.GetMaxTiers(argument)) + ", " +
	       std::to_string(m_progression.GetResources()) + " resources left.";
}


} // namespace Manifest::Tui
