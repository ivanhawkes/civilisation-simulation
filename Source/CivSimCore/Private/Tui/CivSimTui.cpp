#include <Tui/CivSimTui.h>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>


using namespace ftxui;


// --- CATPPUCCIN MOCHA PALETTE DEFINITIONS ---
namespace Mocha
{
auto Rosewater = color(Color::RGB(245, 224, 220));
auto Flamingo = color(Color::RGB(242, 205, 205));
auto Pink = color(Color::RGB(245, 194, 231));
auto Mauve = color(Color::RGB(203, 166, 247));
auto Red = color(Color::RGB(243, 139, 168));
auto Maroon = color(Color::RGB(235, 160, 172));
auto Peach = color(Color::RGB(250, 179, 135));
auto Yellow = color(Color::RGB(249, 226, 175));
auto Green = color(Color::RGB(166, 227, 161));
auto Teal = color(Color::RGB(148, 226, 213));
auto Sky = color(Color::RGB(137, 220, 235));
auto Sapphire = color(Color::RGB(116, 199, 236));
auto Blue = color(Color::RGB(137, 180, 250));
auto Lavender = color(Color::RGB(180, 190, 254));

// Surfaces & Text
auto Text = color(Color::RGB(205, 214, 244));
auto Subtext0 = color(Color::RGB(166, 173, 200));
auto Overlay0 = color(Color::RGB(108, 112, 134));
auto Surface0 = color(Color::RGB(54, 58, 79));
auto BaseBg = bgcolor(Color::RGB(30, 30, 46));
} // namespace Mocha


int main()
{
	CivSimTui repl;
	repl.Run();

	return 0;
}


CivSimTui::CivSimTui()
{
	// Seed some sample command definitions
	commands = {"help", "quit", "clear", "status", "version", "config"};
	start_time = std::chrono::steady_clock::now();
}


std::string CivSimTui::Eval(const std::string &line, ftxui::ScreenInteractive &screen)
{
	std::stringstream ss(line);
	std::string cmd;
	ss >> cmd;

	// Reset state on every check
	last_command_failed = false;

	if (cmd == "quit")
	{
		// Signal the background thread to stop.
		run_thread = false;
		if (refresh_thread.joinable())
		{
			// Safely clean up the thread resource
			refresh_thread.join();
		}

		screen.ExitLoopClosure()();

		return "Exiting system...";
	}
	if (cmd == "clear")
	{
		return "Clear signal processed. (Implement custom clear states here)";
	}
	if (cmd == "help")
	{
		return "Available commands: help, status, version, config, clear, exit";
	}
	if (cmd == "status")
	{
		return "System Status: Nominal.";
	}
	if (cmd == "version")
	{
		return "FTXUI REPL v2.1.0 (C++17)";
	}
	if (cmd == "config")
	{
		return "Configuration defaults loaded safely.";
	}

	// Set flag to true if the input doesn't match a known command
	last_command_failed = true;
	return "Unknown command: '" + cmd + "'. Press [Tab] to see suggestions.";
}


void CivSimTui::Run()
{
	auto screen = ScreenInteractive::Fullscreen();

	std::string inputData;
	std::vector<std::string> historyLog = {
	    "Welcome to the FTXUI Advanced REPL!",
	    "Type your command below. Press [Tab] to complete, [Up/Down] for history."};

	// Track user's command history for up/down navigation
	std::vector<std::string> commandHistory;
	int historyIndex = -1;

	// Create the input component
	Component inputBox = Input(&inputData, "Type a command...");

	// Intercept keys for Tab-completion and Arrow-key history
	auto inputHandler = CatchEvent(
	    inputBox,
	    [&](Event event)
	    {
		    // Tab Completion
		    if (event == Event::Tab)
		    {
			    if (!inputData.empty())
			    {
				    auto it = std::find_if(
				        commands.begin(), commands.end(),
				        [&](const std::string &cmd)
				        {
					        return cmd.rfind(inputData, 0) == 0; // Starts with
				        });
				    if (it != commands.end())
				    {
					    inputData = *it; // Autofill matching command
				    }
			    }
			    return true;
		    }

		    // History Navigation: Up Arrow
		    if (event == Event::ArrowUp)
		    {
			    if (!commandHistory.empty() && historyIndex < (int)commandHistory.size() - 1)
			    {
				    historyIndex++;
				    inputData = commandHistory[commandHistory.size() - 1 - historyIndex];
			    }
			    return true;
		    }

		    // History Navigation: Down Arrow
		    if (event == Event::ArrowDown)
		    {
			    if (historyIndex > 0)
			    {
				    historyIndex--;
				    inputData = commandHistory[commandHistory.size() - 1 - historyIndex];
			    }
			    else if (historyIndex == 0)
			    {
				    historyIndex = -1;
				    inputData = ""; // Clear if navigated past the newest item
			    }
			    return true;
		    }

		    // Return/Enter: Execute Command
		    if (event == Event::Return)
		    {
			    if (inputData.empty())
				    return true;

			    std::string cmd = inputData;
			    commandHistory.push_back(cmd);
			    historyIndex = -1; // Reset index counter
			    inputData = "";    // Clear input bar

			    historyLog.push_back("> " + cmd);

			    // Evaluate input
			    std::string output = Eval(cmd, screen);
			    if (!output.empty())
			    {
				    historyLog.push_back(output);
			    }
			    return true;
		    }

		    return false;
	    });

	auto renderer = Renderer(
	    inputHandler,
	    [&]
	    {
		    // Pack up your existing history view elements into a distinct Left Pane
		    // component
		    Elements logElements;
		    for (const auto &line : historyLog)
		    { // Use your renamed history container here
			    logElements.push_back(text(line));
		    }
		    // Left Pane Layout Update with Dynamic Prompt Colors
		    auto left_pane =
		        vbox(
		            {vbox(std::move(logElements)) | frame | flex | border | Mocha::Text,
		             hbox({
		                 // Choose the prompt color dynamically based on state
		                 text(last_command_failed ? " ✘ mocha-repl ❯ " : " ✔ mocha-repl ❯ ") | bold |
		                     (inputData.empty() ? (last_command_failed ? Mocha::Red : Mocha::Mauve)
		                                        : Mocha::Green),

		                 inputBox->Render() | flex | Mocha::Text,
		             }) | border |
		                 Mocha::Surface0}) |
		        flex | Mocha::BaseBg;

		    // Calculate dynamic structural calculations for the sidebar layout
		    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		                       std::chrono::steady_clock::now() - start_time)
		                       .count();

		    // Construct your new brand-new Sidebar component
		    auto right_pane =
		        vbox(
		            {text("   SYSTEM METRICS ") | bold | center | color(Color::Green) |
		                 Mocha::Lavender,
		             separator() | Mocha::Surface0,
		             vbox({
		                 hbox(
		                     {text("Game Time:      "), text(std::to_string(elapsed) + "s") |
		                                                    color(Color::Yellow) | Mocha::Text}),
		                 hbox(
		                     {text("History Size:"),
		                      text(std::to_string(commandHistory.size())) | Mocha::Text}),
		             }),
		             separator() | Mocha::Surface0,
		             vbox({
		                 hbox(
		                     {text("Game Time:      "), text(std::to_string(elapsed) + "s") |
		                                                    color(Color::Yellow) | Mocha::Text}),
		                 hbox(
		                     {text("History Size:"),
		                      text(std::to_string(commandHistory.size())) | Mocha::Text}),
		             }),
		             separator(),
		             text("   Commands ") | bold | center | color(Color::Cyan) | Mocha::Peach,
		             separator() | Mocha::Surface0,
		             vbox({
		                 text(" • help    • status") | Mocha::Subtext0,
		                 text(" • version • config") | Mocha::Subtext0,
		                 text(" • clear   • exit") | Mocha::Red,
		             }) | color(Color::GrayDark) |
		                 Mocha::Surface0}) |
		        Mocha::BaseBg | border |
		        size(
		            WIDTH, EQUAL, 30); // Hard lock the sidebar pane size to exactly 30 terminal columns

		    // Combine both components horizontally using 'hbox'
		    return hbox({left_pane, right_pane});
	    });

	// Start the refresh thread to periodically push custom events for re-rendering
	refresh_thread = std::thread(
	    [&]()
	    {
		    while (run_thread)
		    {
			    std::this_thread::sleep_for(std::chrono::milliseconds(250));

			    // Push a fake/custom event to force the main FTXUI thread to re-render
			    screen.PostEvent(Event::Custom);
		    }
	    });

	screen.Loop(renderer);
}
