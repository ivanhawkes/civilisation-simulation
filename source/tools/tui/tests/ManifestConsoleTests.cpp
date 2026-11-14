#include "ManifestConsole.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <jettison/core/Log.h>

#include "TestHelpers.h"

using Jettison::DevTools::ConsoleLine;
using Manifest::Tui::ManifestConsole;

namespace
{
ConsoleLine Execute(ManifestConsole &session, std::string_view command)
{
	session.GetConsole().Execute(command);
	const auto lines = session.GetConsole().Lines();
	REQUIRE_FALSE(lines.empty());
	return lines.back();
}

class ConfigDirectory
{
  public:
	ConfigDirectory() : m_path{m_marker.Path() + " config with spaces"}
	{
		std::filesystem::create_directory(m_path);
	}
	~ConfigDirectory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_path, ignored);
	}
	ConfigDirectory(const ConfigDirectory &) = delete;
	ConfigDirectory &operator=(const ConfigDirectory &) = delete;

	const std::filesystem::path &Path() const
	{
		return m_path;
	}

  private:
	Manifest::Test::TempFile m_marker{""};
	std::filesystem::path m_path;
};
} // namespace

TEST_CASE("Manifest console: help, completion, history and clear use the shell's console")
{
	ManifestConsole session{MANIFEST_DATA_DIRECTORY, MANIFEST_UNIVERSE_FILE};
	auto &console = session.GetConsole();
	CHECK(console.Complete("s") == std::vector<std::string>{"show", "skills", "status"});
	CHECK(console.CompleteLine("gen") == "generate ");
	CHECK(console.CompleteLine("generate 2") == "generate 2");
	CHECK(Execute(session, "help generate").text.find("generate [count] [seed]") != std::string::npos);
	CHECK(Execute(session, "vars").kind == ConsoleLine::Kind::Input);

	Execute(session, "status");
	Execute(session, "status");
	CHECK(console.History() == std::vector<std::string>{"help generate", "vars", "status"});
	console.Execute("clear");
	CHECK(console.Lines().empty());
	CHECK(console.History().back() == "clear");
	CHECK(Execute(session, "version").kind == ConsoleLine::Kind::Output);
}

TEST_CASE(
    "Manifest console: malformed and unavailable commands are errors and leave the session usable")
{
	ManifestConsole session{MANIFEST_DATA_DIRECTORY, MANIFEST_UNIVERSE_FILE};
	for (const std::string command :
	     {"unknown", "load \"unfinished", "generate", "tick", "list", "skills", "show", "upgrade",
	      "status extra", "quit extra"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
		CHECK_FALSE(session.IsQuitRequested());
	}
	CHECK(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	CHECK(session.IsConfigurationLoaded());
}

TEST_CASE("Manifest console: quoted paths and civilisation names reach the command as one argument")
{
	ConfigDirectory directory;
	for (const std::string file : {"ages.yaml", "categories.yaml", "full.yaml"})
		std::filesystem::create_symlink(
		    std::filesystem::path{MANIFEST_DATA_DIRECTORY} / file, directory.Path() / file);

	ManifestConsole session{MANIFEST_DATA_DIRECTORY, MANIFEST_UNIVERSE_FILE};
	CHECK(
	    Execute(session, std::format("  load \"{}\"  ", directory.Path().string())).kind ==
	    ConsoleLine::Kind::Output);
	CHECK(session.DataDirectory() == directory.Path().string());
	REQUIRE(Execute(session, "generate 3 42").kind == ConsoleLine::Kind::Output);
	const auto civilisation = session.GetSimulation().GetCivilisations().front();
	const auto shown = Execute(session, std::format("show \"{}\"", civilisation.name));
	CHECK(shown.kind == ConsoleLine::Kind::Output);
	CHECK(shown.text.find(civilisation.id + " " + civilisation.name) != std::string::npos);
	CHECK(Execute(session, "show nonexistent").kind == ConsoleLine::Kind::Error);
}

TEST_CASE("Manifest console: invalid numeric arguments never silently advance or regenerate")
{
	ManifestConsole session{MANIFEST_DATA_DIRECTORY, MANIFEST_UNIVERSE_FILE};
	REQUIRE(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "generate 2 0").kind == ConsoleLine::Kind::Output);
	for (const std::string command :
	     {"tick -1", "tick 0", "tick 2junk", "tick 1 2", "tick 1001", "tick 99999999999999999999",
	      "tick \"\"", "generate 0", "generate 501", "generate 3 -1", "generate 3 4294967296",
	      "generate 3 1 extra", "prices nope", "plan 101"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Error);
		CHECK(session.GetSimulation().GetTick() == 0);
		CHECK(session.GetSimulation().GetCivilisations().size() == 2);
	}
	CHECK(Execute(session, "tick 2").kind == ConsoleLine::Kind::Output);
	CHECK(session.GetSimulation().GetTick() == 2);
}

TEST_CASE(
    "Manifest console: the development workflow reports output and rejected upgrades report errors")
{
	ManifestConsole session{MANIFEST_DATA_DIRECTORY, MANIFEST_UNIVERSE_FILE};
	for (const std::string command :
	     {"load", "generate", "list", "show Civ-01", "prices 3", "tick", "plan 3", "skills",
	      "upgrade SkillMining"})
	{
		CAPTURE(command);
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Output);
	}
	CHECK(Execute(session, "upgrade unknown").kind == ConsoleLine::Kind::Error);
	CHECK(Execute(session, "status").text.find("resources 19") != std::string::npos);
}

TEST_CASE(
    "Manifest console: a failed reload preserves the session and a successful reload resets its world")
{
	ManifestConsole session{MANIFEST_DATA_DIRECTORY, MANIFEST_UNIVERSE_FILE};
	REQUIRE(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "generate 2 42").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "tick").kind == ConsoleLine::Kind::Output);
	REQUIRE(Execute(session, "upgrade SkillMining").kind == ConsoleLine::Kind::Output);
	const auto status = Execute(session, "status").text;

	ConfigDirectory partial;
	std::ofstream{partial.Path() / "ages.yaml"} << "ages: []\n";
	CHECK(
	    Execute(session, std::format("load \"{}\"", partial.Path().string())).kind ==
	    ConsoleLine::Kind::Error);
	CHECK(session.IsConfigurationLoaded());
	CHECK(session.DataDirectory() == MANIFEST_DATA_DIRECTORY);
	CHECK(Execute(session, "status").text == status);
	CHECK(session.GetSimulation().GetTick() == 1);

	CHECK(Execute(session, "load").kind == ConsoleLine::Kind::Output);
	CHECK_FALSE(session.GetSimulation().IsGenerated());
	CHECK(Execute(session, "tick").kind == ConsoleLine::Kind::Error);
	CHECK(Execute(session, "generate").kind == ConsoleLine::Kind::Output);
}

TEST_CASE(
    "Manifest console: engine logs enter the shared scrollback and both exit names end the session")
{
	for (const std::string command : {"quit", "exit"})
	{
		ManifestConsole session{MANIFEST_DATA_DIRECTORY, MANIFEST_UNIVERSE_FILE};
		constexpr Jettison::Core::Category kTest{"manifest_console"};
		Jettison::Core::Log(Jettison::Core::Severity::Warning, kTest, "test warning");
		const auto lines = session.GetConsole().Lines();
		REQUIRE(lines.size() == 1);
		CHECK(lines[0].kind == ConsoleLine::Kind::Log);
		CHECK(lines[0].severity == Jettison::Core::Severity::Warning);
		CHECK(lines[0].category == "manifest_console");
		CHECK(Execute(session, command).kind == ConsoleLine::Kind::Output);
		CHECK(session.IsQuitRequested());
	}
}
