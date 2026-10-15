#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LegacyFacePatchImporter.generated.h"

class UFacePatchConfiguration;

UCLASS()
class SEMANTICSURFACEPATCHESEDITOR_API ULegacyFacePatchImporter : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Context Optimizing|Face Patches")
    static bool importLegacyConfigurationJson(
        UFacePatchConfiguration* configuration,
        const FString& filePath,
        FString& errorMessage);
};

