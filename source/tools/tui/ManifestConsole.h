#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>

#include <jettison/devtools/Console.h>

#include <manifest/config/SimulationConfig.h>
#include <manifest/progression/ProgressionManager.h>
#include <manifest/sim/Simulation.h>
#include <manifest/universe/Universe.h>

namespace Manifest::Tui
{
// Host-side development commands, registered with the same console as Jettison's shell.
// Rendering, refresh and transcripts belong to the frontend; no terminal is needed here.
class ManifestConsole
{
  public:
	ManifestConsole(std::string dataDirectory, std::string universeFile);
	ManifestConsole(const ManifestConsole &) = delete;
	ManifestConsole &operator=(const ManifestConsole &) = delete;

	Jettison::DevTools::Console &GetConsole()
	{
		return m_console;
	}

	const Manifest::Core::Simulation &GetSimulation() const
	{
		return m_simulation;
	}

	const std::string &DataDirectory() const
	{
		return m_dataDirectory;
	}

	bool IsConfigurationLoaded() const
	{
		return m_configLoaded;
	}

	bool IsQuitRequested() const
	{
		return m_quitRequested;
	}

  private:
	using Arguments = std::span<const std::string>;
	using Handler = std::function<std::string(Arguments)>;

	void RegisterCommands();
	void RegisterCommand(
	    std::string name, std::string help, std::size_t minimum, std::size_t maximum, Handler run);

	std::string CommandVersion();
	std::string CommandStatus();
	std::string CommandLoad(const std::string &directory);
	std::string CommandGenerate(Arguments arguments);
	std::string CommandList();
	std::string CommandShow(const std::string &argument);
	std::string CommandPrices(Arguments arguments);
	std::string CommandPlan(Arguments arguments);
	std::string CommandTick(Arguments arguments);
	std::string CommandSkills();
	std::string CommandUpgrade(const std::string &argument);

	Manifest::Core::Ages m_ages;
	Manifest::Core::Categories m_categories;
	Manifest::Core::GameModeNode m_gameMode;
	Manifest::Core::UniverseNamePools m_names;
	Manifest::Core::Simulation m_simulation;
	Manifest::Core::ProgressionManager m_progression;

	std::string m_dataDirectory;
	std::string m_universeFile;
	bool m_configLoaded{false};
	bool m_quitRequested{false};

	Jettison::DevTools::Console m_console;
};
} // namespace Manifest::Tui
