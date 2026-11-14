#include "ManifestTui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/flexbox_config.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace Manifest::Tui
{
using namespace ftxui;
using Jettison::DevTools::ConsoleLine;

namespace
{
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

std::string LineText(const ConsoleLine &line)
{
	return line.category.empty() ? line.text : std::format("[{}] {}", line.category, line.text);
}
} // namespace

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


namespace
{
Decorator LineColour(const ConsoleLine &line)
{
	switch (line.kind)
	{
		case ConsoleLine::Kind::Input: return Mocha::Blue;
		case ConsoleLine::Kind::Error: return Mocha::Red;
		case ConsoleLine::Kind::Output: return Mocha::Text;
		case ConsoleLine::Kind::Log: break;
	}
	switch (line.severity)
	{
		case Jettison::Core::Severity::Trace:
		case Jettison::Core::Severity::Debug: return Mocha::Subtext0;
		case Jettison::Core::Severity::Warning: return Mocha::Yellow;
		case Jettison::Core::Severity::Error:
		case Jettison::Core::Severity::Fatal: return Mocha::Red;
		case Jettison::Core::Severity::Info: return Mocha::Text;
	}
	return Mocha::Text;
}
} // namespace

ManifestTui::ManifestTui()
    : m_session{MANIFEST_DATA_DIRECTORY, MANIFEST_UNIVERSE_FILE}, m_logFile{OpenSessionLog()}
{
}

std::ofstream ManifestTui::OpenSessionLog()
{
	// Keep the transcript across clear and the shared console's bounded scrollback.
	return std::ofstream{"build/tui-session.log", std::ios::app};
}

void ManifestTui::AppendSessionLog(const std::string &text)
{
	if (m_logFile.is_open())
	{
		m_logFile << text << '\n';
		m_logFile.flush();
	}
}

void ManifestTui::Run()
{
	ScreenInteractive screen = ScreenInteractive::Fullscreen();
	auto &console = m_session.GetConsole();
	const auto startTime = std::chrono::steady_clock::now();
	std::string inputData;
	int inputCursorPosition = 0;
	int historyIndex = -1;
	int logFocusIndex = 0;
	bool followOutput = true;
	bool failed = false;

	auto print = [&](const std::string &text)
	{
		console.Print(text);
		AppendSessionLog(text);
	};
	auto execute = [&](const std::string &command)
	{
		AppendSessionLog("> " + command);
		console.Execute(command);
		failed = false;
		const auto lines = console.Lines();
		// Locate this command's echo rather than counting lines: the console may
		// have evicted older output, or clear may have removed the echo itself.
		const auto echo = std::find_if(
		    lines.rbegin(), lines.rend(),
		    [](const ConsoleLine &line) { return line.kind == ConsoleLine::Kind::Input; });
		for (auto line = echo == lines.rend() ? lines.begin() : echo.base(); line != lines.end();
		     ++line)
		{
			AppendSessionLog(LineText(*line));
			failed = failed || line->kind == ConsoleLine::Kind::Error;
		}
	};

	print("Manifest development console. Type help for commands, then generate 8.");
	print(
	    "Tab: complete; Up/Down: history; PageUp/PageDown: scroll; End: follow output; Ctrl-D: quit.");
	execute("load");

	InputOption inputOptions = InputOption::Default();
	inputOptions.cursor_position = &inputCursorPosition;
	Component inputBox = Input(&inputData, "Type a command...", inputOptions);
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
			    const std::string completed = console.CompleteLine(inputData);
			    if (completed != inputData)
			    {
				    inputData = completed;
				    inputCursorPosition = static_cast<int>(inputData.size());
			    }
			    else if (const auto candidates = console.Complete(inputData); candidates.size() > 1)
			    {
				    std::string list;
				    for (const std::string &candidate : candidates)
					    list += (list.empty() ? "" : "  ") + candidate;
				    print(list);
			    }
			    return true;
		    }
		    if (event == Event::PageUp || event == Event::PageDown || event == Event::End)
		    {
			    const int last = std::max(0, static_cast<int>(console.Lines().size()) - 1);
			    if (event == Event::PageUp)
				    logFocusIndex = std::max(0, logFocusIndex - 5);
			    else if (event == Event::PageDown)
				    logFocusIndex = std::min(last, logFocusIndex + 5);
			    else
				    logFocusIndex = last;
			    followOutput = logFocusIndex == last;
			    return true;
		    }
		    if (event == Event::ArrowUp || event == Event::ArrowDown)
		    {
			    const auto &history = console.History();
			    const int count = static_cast<int>(history.size());
			    const int nextHistoryIndex = event == Event::ArrowUp
			                                     ? std::min(historyIndex + 1, count - 1)
			                                     : std::max(historyIndex - 1, -1);
			    if (nextHistoryIndex == historyIndex)
				    return true;
			    historyIndex = nextHistoryIndex;
			    inputData = historyIndex < 0
			                    ? std::string{}
			                    : history[static_cast<std::size_t>(count - 1 - historyIndex)];
			    inputCursorPosition = static_cast<int>(inputData.size());
			    return true;
		    }
		    if (event == Event::Return)
		    {
			    if (inputData.find_first_not_of(" \t") == std::string::npos)
				    return true;
			    const std::string command = std::exchange(inputData, {});
			    inputCursorPosition = 0;
			    historyIndex = -1;
			    execute(command);
			    followOutput = true;
			    if (m_session.IsQuitRequested())
				    screen.ExitLoopClosure()();
			    return true;
		    }
		    return false;
	    });

	auto renderer = Renderer(
	    inputHandler,
	    [&]
	    {
		    const auto lines = console.Lines();
		    const int last = std::max(0, static_cast<int>(lines.size()) - 1);
		    logFocusIndex = followOutput ? last : std::clamp(logFocusIndex, 0, last);
		    Elements logElements;
		    for (std::size_t index = 0; index < lines.size(); ++index)
		    {
			    Element element = WrapLogText(LineText(lines[index])) | LineColour(lines[index]);
			    if (static_cast<int>(index) == logFocusIndex)
				    element = element | focus;
			    logElements.push_back(std::move(element));
		    }
		    const std::string promptMark = failed ? " \u2718 " : " \u2714 ";
		    auto leftPane =
		        vbox(
		            {vbox(std::move(logElements)) | yframe | flex | border,
		             hbox(
		                 {text(promptMark + " manifest \u276f ") | bold |
		                      (failed ? Mocha::Red : (inputData.empty() ? Mocha::Mauve : Mocha::Green)),
		                  inputBox->Render() | flex | Mocha::Text}) |
		                 border | Mocha::Surface0}) |
		        flex | Mocha::BaseBg;

		    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		                             std::chrono::steady_clock::now() - startTime)
		                             .count();
		    const auto commands = console.Complete("");
		    Elements commandColumn;
		    for (std::size_t index = 0; index < commands.size(); index += 2)
		    {
			    std::string line = "   " + commands[index];
			    if (index + 1 < commands.size())
				    line += "  " + commands[index + 1];
			    commandColumn.push_back(text(line) | Mocha::Subtext0);
		    }
		    const auto &simulation = m_session.GetSimulation();
		    auto rightPane =
		        vbox(
		            {text(" universe ") | bold | center | Mocha::Lavender,
		             separatorDashed() | Mocha::Surface0,
		             hbox({text("  session: "), text(std::to_string(elapsed) + "s") | Mocha::Yellow}),
		             hbox({text("  log lines: "), text(std::to_string(lines.size())) | Mocha::Text}),
		             hbox(
		                 {text("  data: "),
		                  text(
		                      m_session.IsConfigurationLoaded() ? m_session.DataDirectory()
		                                                        : "not loaded") |
		                      Mocha::Text}),
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

	// Local RAII joins before the screen is destroyed, including if Loop throws.
	std::jthread refreshThread(
	    [&screen](std::stop_token stop)
	    {
		    while (!stop.stop_requested())
		    {
			    std::this_thread::sleep_for(std::chrono::milliseconds(250));
			    if (!stop.stop_requested())
				    screen.PostEvent(Event::Custom);
		    }
	    });
	screen.Loop(renderer);
}
} // namespace Manifest::Tui
