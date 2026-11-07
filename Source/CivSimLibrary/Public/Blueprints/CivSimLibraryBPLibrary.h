#pragma once

#include "Blueprints/CivSimLibraryBPLibrary.generated.h"
#include "Core/SimulationConfig.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"


UCLASS()
class CIVSIMLIBRARY_API UCivSimLibraryBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

  public:
	UFUNCTION(BlueprintCallable, Category = "CivSim|Configuration")
	static bool LoadSimulationConfigFromYaml(
	    const FString &FilePath, FCivSimSimulationConfig &OutConfig, FString &OutError);
};
