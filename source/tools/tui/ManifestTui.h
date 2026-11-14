#pragma once

#include <ftxui/component/screen_interactive.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <manifest/config/SimulationConfig.h>
#include <manifest/progression/ProgressionManager.h>
#include <manifest/sim/Simulation.h>
#include <manifest/universe/Universe.h>


// Terminal front-end used to exercise the core during development: it loads the
// generated configuration, generates a universe, advances it and runs trades.
//
// The command handling is separate from the rendering, so the behaviour can be used
// from a script and the front-end stays a thin shell.
namespace Manifest::Tui
{
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
	std::string Evaluate(const std::string &line);

	bool IsQuitRequested() const
	{
		return m_quitRequested;
	}

	// Prompt marker: a tick after a good command, a cross after a failed one.
	const std::string &PromptMark() const
	{
		return m_promptMark;
	}

	// Commands for completion and for the sidebar, in the order the help lists them.
	const std::vector<std::string> &Commands() const
	{
		return m_commands;
	}

  private:
	std::string CommandHelp();
	std::string CommandVersion();
	std::string CommandStatus();
	std::string CommandLoad(const std::string &directory);
	std::string CommandGenerate(const std::string &argument);
	std::string CommandList();
	std::string CommandShow(const std::string &argument);
	std::string CommandPrices(const std::string &argument);
	std::string CommandPlan(const std::string &argument);
	std::string CommandTick(const std::string &argument);
	std::string CommandSkills();
	std::string CommandUpgrade(const std::string &argument);

	bool LoadConfiguration(const std::string &directory, std::string &errorMessage);
	static unsigned ParseCount(const std::string &text, unsigned fallback, unsigned maximum);

	void StopRefreshThread();

	// One line per log entry, flushed as it is written: watching the file from tmux
	// while the UI runs is how a UI bug gets caught, and a crash mid-frame must not
	// take the transcript with it.
	static std::ofstream OpenSessionLog();
	void AppendSessionLog(const std::string &text);

	std::ofstream m_logFile;

	std::vector<std::string> m_commands;

	Manifest::Core::Ages m_ages;
	Manifest::Core::Categories m_categories;
	Manifest::Core::GameModeNode m_gameMode;
	Manifest::Core::UniverseNamePools m_names;
	Manifest::Core::Simulation m_simulation;
	Manifest::Core::ProgressionManager m_progression;

	std::string m_dataDirectory{MANIFEST_DATA_DIRECTORY};
	std::string m_universeFile{MANIFEST_UNIVERSE_FILE};
	bool m_configLoaded{false};
	bool m_quitRequested{false};
	std::string m_promptMark{" \u2714 "};

	std::chrono::steady_clock::time_point m_startTime;

	// Log line the frame follows: the newest line until the player scrolls back.
	int m_logFocusIndex{0};

	std::thread m_refreshThread;
	std::atomic<bool> m_runThread{true};
};

} // namespace Manifest::Tui
