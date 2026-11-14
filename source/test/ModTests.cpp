#include "TestHelpers.h"
#include <yaml-cpp/yaml.h>

#include <string>

#include <jettison/core/Version.h>
#include <jettison/vfs/FileSystem.h>

TEST_CASE("Manifest directory and pack expose identical content through the engine VFS")
{
	Jettison::Vfs::FileSystem directory;
	directory.AddMount({"mod:manifest", MANIFEST_MOD_DIRECTORY});
	Jettison::Vfs::FileSystem pack;
	pack.AddMount({"mod:manifest", MANIFEST_MOD_PACK});

	const auto files = directory.Enumerate("");
	REQUIRE(files == pack.Enumerate(""));
	REQUIRE(files.size() == 6);
	for (const auto &path : files)
	{
		CAPTURE(path);
		REQUIRE(directory.Read(path).has_value());
		CHECK(directory.Read(path) == pack.Read(path));
	}
	const auto metadata = pack.Read("mod.yaml");
	REQUIRE(metadata.has_value());
	const YAML::Node mod =
	    YAML::Load(std::string(reinterpret_cast<const char *>(metadata->data()), metadata->size()));
	CHECK(mod["version"].as<int>() == 1);
	CHECK(mod["name"].as<std::string>() == "Manifest");
	CHECK(mod["engine"].as<std::string>() == std::string(Jettison::Core::EngineVersion()));
	CHECK(pack.Exists("manifest/core-config/full.yaml"));
	CHECK(pack.Exists("manifest/universe/solar-system-names.yaml"));
}
