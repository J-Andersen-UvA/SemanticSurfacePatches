#pragma once

#include "Components/ArrowComponent.h"
#include "CoreMinimal.h"
#include "SemanticPatchProximityTypes.h"
#include "SemanticInteractionPointComponent.generated.h"

UCLASS(
    BlueprintType,
    ClassGroup = (SemanticPatchProximity),
    meta = (BlueprintSpawnableComponent))
class SEMANTICPATCHPROXIMITY_API USemanticInteractionPointComponent
    : public UArrowComponent
{
    GENERATED_BODY()

public:
    USemanticInteractionPointComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    FName pointId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    FName interactionGroupId;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Semantic Proximity",
        meta = (ClampMin = "0.0", Units = "cm"))
    double pointRadius = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    bool enabled = true;

    UFUNCTION(BlueprintPure, Category = "Semantic Patch Proximity")
    FSemanticInteractionPoint toInteractionPoint() const;
};
