#include <Tui/CivSimTui.h>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;


int main() {
    CivSimTui repl;
    repl.Run();

    return 0;
}


CivSimTui::CivSimTui()
{
	// Seed some sample command definitions
	commands = {"help", "quit", "clear", "status", "version", "config"};
}


std::string CivSimTui::Eval(const std::string &line, ftxui::ScreenInteractive &screen)
{
	std::stringstream ss(line);
	std::string cmd;
	ss >> cmd;

	if (cmd == "quit")
	{
		screen.ExitLoopClosure()();
		return "Quitting system...";
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
		    // 1. Tab Completion
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

		    // 2. History Navigation: Up Arrow
		    if (event == Event::ArrowUp)
		    {
			    if (!commandHistory.empty() && historyIndex < (int)commandHistory.size() - 1)
			    {
				    historyIndex++;
				    inputData = commandHistory[commandHistory.size() - 1 - historyIndex];
			    }
			    return true;
		    }

		    // 3. History Navigation: Down Arrow
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

		    // 4. Return/Enter: Execute Command
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

	// Dynamic layout rendering
	auto renderer = Renderer(
	    inputHandler,
	    [&]
	    {
		    Elements logElements;
		    // Display history lines inside the top panel
		    for (const auto &line : historyLog)
		    {
			    logElements.push_back(text(line));
		    }

		    return vbox(
		        {vbox(std::move(logElements)) | frame | flex | border, hbox({
		                                                                   text(" repl> ") | bold | color(Color::Cyan),
		                                                                   inputBox->Render() | flex,
		                                                               }) | border});
	    });

	screen.Loop(renderer);
}
