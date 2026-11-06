#include "Core/SimulationConfig.h"
#include <iostream>
#include <string>


int main()
{
    const std::string agesConfigFilePath {"data/core-config/ages.yaml"};
    const std::string categoriesConfigFilePath {"data/core-config/categories.yaml"};
    const std::string gameModeFullFilePath {"data/core-config/full.yaml"};

    std::string errorMessage;
    
    CivSimCore::Ages ages;
    if (!CivSimCore::LoadConfigAges(agesConfigFilePath, ages, errorMessage))
    {
        std::cerr << "Failed to load test YAML: " << errorMessage << '\n';

        return 1;
    }
    else
    {
        std::cout << "Number of ages: " << ages.size() << " entries\n";
    }

    CivSimCore::Categories categories;
    if (!CivSimCore::LoadConfigCategories(categoriesConfigFilePath, categories, errorMessage))
    {
        std::cerr << "Failed to load test YAML: " << errorMessage << '\n';
        
        return 1;
    }
    else
    {
        std::cout << "Number of categories: " << categories.size() << " entries\n";
    }    

    CivSimCore::GameModeNode gameModeConfig;
    if (!CivSimCore::LoadConfigGameMode(gameModeFullFilePath, gameModeConfig, errorMessage))
    {
        std::cerr << "Failed to load test YAML: " << errorMessage << '\n';
        
        return 1;
    }
    else
    {
        std::cout << "Number of items: " << gameModeConfig.items.size() << " entries\n";
        std::cout << "Number of skills: " << gameModeConfig.skills.size() << " entries\n";
    }
    
    std::cout << "CivSimCore YAML test passed.\n";
    
    return 0;
}