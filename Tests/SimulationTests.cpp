#include "Core/SimulationConfig.h"
#include <iostream>
#include <string>
#include <vector>

namespace
{
// Reports a load failure and asks for the generated documents.
void ReportFailure(const std::string &FilePath, const std::string &ErrorMessage)
{
	std::cerr << "Failed to load '" << FilePath
	          << "'. Is the generated YAML present? Build the CivSimData target "
	             "(cmake --build <build-dir>) to create data/core-config.\n"
	          << "Error: " << ErrorMessage << '\n';
}

// Fails when a configuration loaded no entries, which would hide a merge problem.
bool CheckNotEmpty(const std::string &Label, std::size_t Count)
{
	if (Count == 0)
	{
		std::cerr << "No " << Label << " entries were loaded.\n";
		return false;
	}

	std::cout << "Number of " << Label << ": " << Count << " entries\n";

	return true;
}
} // namespace

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		std::cerr << "Usage: " << argv[0]
		          << " <data/core-config/ages.yaml> <data/core-config/categories.yaml> "
		             "<data/core-config/full.yaml>\n";

		return 2;
	}

	const std::string agesConfigFilePath{argv[1]};
	const std::string categoriesConfigFilePath{argv[2]};
	const std::string gameModeFullFilePath{argv[3]};

	std::string errorMessage;
	bool passed = true;

	CivSimCore::Ages ages;
	if (!CivSimCore::LoadConfigAges(agesConfigFilePath, ages, errorMessage))
	{
		ReportFailure(agesConfigFilePath, errorMessage);

		return 1;
	}

	passed &= CheckNotEmpty("ages", ages.size());

	CivSimCore::Categories categories;
	if (!CivSimCore::LoadConfigCategories(categoriesConfigFilePath, categories, errorMessage))
	{
		ReportFailure(categoriesConfigFilePath, errorMessage);

		return 1;
	}

	passed &= CheckNotEmpty("categories", categories.size());

	CivSimCore::GameModeNode gameModeConfig;
	if (!CivSimCore::LoadConfigGameMode(gameModeFullFilePath, gameModeConfig, errorMessage))
	{
		ReportFailure(gameModeFullFilePath, errorMessage);

		return 1;
	}

	passed &= CheckNotEmpty("items", gameModeConfig.items.size());
	passed &= CheckNotEmpty("skills", gameModeConfig.skills.size());

	if (!passed)
	{
		return 1;
	}

	std::cout << "CivSimCore YAML test passed.\n";

	return 0;
}
