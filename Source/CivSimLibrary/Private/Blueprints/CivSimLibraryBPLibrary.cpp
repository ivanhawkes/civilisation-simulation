#include "Blueprints/CivSimLibraryBPLibrary.h"
#include "Core/SimulationConfig.h"
#include <string>


bool UCivSimLibraryBPLibrary::LoadSimulationConfigFromYaml(
    const FString& FilePath,
    FCivSimSimulationConfig& OutConfig,
    FString& OutError
)
{
    const std::string NativePath(TCHAR_TO_UTF8(*FilePath));

    CivSimLibrary::SimulationConfig Parsed;
    std::string Error;

    if (!CivSimLibrary::LoadSimulationConfig(NativePath, Parsed, Error))
    {
        OutError = UTF8_TO_TCHAR(Error.c_str());
        return false;
    }

    OutConfig.Hostname = UTF8_TO_TCHAR(Parsed.Hostname.c_str());
    OutConfig.Port = Parsed.Port;
    OutConfig.bEnabled = Parsed.Enabled;
    OutError.Reset();

    return true;
}
