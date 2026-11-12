#pragma once

#include "Core/ProgressionManager.h"
#include "Core/Simulation.h"
#include "Core/SimulationConfig.h"
#include "Core/Universe.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <ftxui/component/screen_interactive.hpp>
#include <string>
#include <thread>
#include <vector>


// Terminal front-end used to exercise the core during development: it loads the
// generated configuration, generates a universe, advances it and runs trades.
//
// The command handling is separate from the rendering, so the behaviour can be used
// from a script and the front-end stays a thin shell.
class ManifestTui
{
  public:
	ManifestTui();

	// Joins the refresh thread: the thread must never outlive this object, and a
	// std::thread destroyed while joinable calls std::terminate.
	~ManifestTui();

	ManifestTui(const ManifestTui &) = delete;
	ManifestTui &operator=(const ManifestTui &) = delete;

	void Run();

	// Runs one command line and returns the text to append to the log. An empty
	// result means the log was cleared. Sets quit_requested for quit and exit.
	std::string Evaluate(const std::string &Line);

	bool IsQuitRequested() const
	{
		return quitRequested;
	}

	// Prompt marker: a tick after a good command, a cross after a failed one.
	const std::string &PromptMark() const
	{
		return promptMark;
	}

	// Commands for completion and for the sidebar, in the order the help lists them.
	const std::vector<std::string> &Commands() const
	{
		return commands;
	}

  private:
	std::string CommandHelp();
	std::string CommandVersion();
	std::string CommandStatus();
	std::string CommandLoad(const std::string &Directory);
	std::string CommandGenerate(const std::string &Argument);
	std::string CommandList();
	std::string CommandShow(const std::string &Argument);
	std::string CommandPrices(const std::string &Argument);
	std::string CommandPlan(const std::string &Argument);
	std::string CommandTick(const std::string &Argument);
	std::string CommandSkills();
	std::string CommandUpgrade(const std::string &Argument);

	bool LoadConfiguration(const std::string &Directory, std::string &ErrorMessage);
	static unsigned ParseCount(const std::string &Text, unsigned Fallback, unsigned Maximum);

	void StopRefreshThread();

	// One line per log entry, flushed as it is written: watching the file from tmux
	// while the UI runs is how a UI bug gets caught, and a crash mid-frame must not
	// take the transcript with it.
	static std::ofstream OpenSessionLog();
	void AppendSessionLog(const std::string &text);

	std::ofstream logFile_;

	std::vector<std::string> commands;

	ManifestCore::Ages ages;
	ManifestCore::Categories categories;
	ManifestCore::GameModeNode gameMode;
	ManifestCore::UniverseNamePools names;
	ManifestCore::Simulation simulation;
	ManifestCore::ProgressionManager progression;

	std::string dataDirectory{"data/core-config"};
	std::string universeFile{"data/universe/solar-system-names.yaml"};
	bool configLoaded{false};
	bool quitRequested{false};
	std::string promptMark{" \u2714 "};

	std::chrono::steady_clock::time_point startTime;

	// Log line the frame follows: the newest line until the player scrolls back.
	int logFocusIndex{0};

	std::thread refreshThread;
	std::atomic<bool> runThread{true};
};
