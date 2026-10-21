#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SemanticPatchProximityTypes.h"
#include "SurfacePatchTypes.h"
#include "SemanticPatchProximityLibrary.generated.h"

class AActor;
class USkeletalMeshComponent;

UCLASS()
class SEMANTICPATCHPROXIMITY_API USemanticPatchProximityLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Semantic Patch Proximity")
    static FSemanticPose makePoseFromSkeletalMesh(
        const USkeletalMeshComponent* skeletalMesh);

    UFUNCTION(BlueprintPure, Category = "Semantic Patch Proximity")
    static TArray<FSemanticInteractionPoint> getInteractionPoints(
        const AActor* actor);

    UFUNCTION(BlueprintCallable, Category = "Semantic Patch Proximity")
    static TArray<FPatchProximityResult> evaluatePatchProximity(
        const FSemanticPose& pose,
        const TArray<FSurfacePatchAnchor>& patches,
        const TArray<FSemanticInteractionPoint>& interactionPoints,
        const FPatchProximitySettings& settings);

    UFUNCTION(BlueprintCallable, Category = "Semantic Patch Proximity")
    static FPatchProximityTarget mapPatchProximityToTarget(
        const FPatchProximityResult& sourceResult,
        const FSemanticPose& targetPose,
        const TArray<FSurfacePatchAnchor>& targetPatches,
        double targetNormalOffset = 0.0);
};
