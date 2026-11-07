#pragma once

#include "Blueprints/CivSimSimulationConfig.generated.h"
#include "CoreMinimal.h"


USTRUCT(BlueprintType)
struct FCivSimSimulationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CivSim|Configuration")
	FString Hostname;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CivSim|Configuration")
	int32 Port = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CivSim|Configuration")
	bool bEnabled = true;
};
