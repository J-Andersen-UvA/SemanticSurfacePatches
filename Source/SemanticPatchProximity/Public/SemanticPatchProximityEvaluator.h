#pragma once

#include "CoreMinimal.h"
#include "SemanticPatchProximityTypes.h"
#include "SurfacePatchTypes.h"

class SEMANTICPATCHPROXIMITY_API FSemanticPatchProximityEvaluator
{
public:
    void setPatches(const TArray<FSurfacePatchAnchor>& patches);

    TArray<FPatchProximityResult> evaluate(
        const FSemanticPose& pose,
        const TArray<FSemanticInteractionPoint>& interactionPoints,
        const FPatchProximitySettings& settings) const;

    FPatchProximityTarget mapToTarget(
        const FSemanticPose& targetPose,
        const FPatchProximityResult& sourceResult,
        double targetNormalOffset = 0.0) const;

private:
    struct FPreparedPatch
    {
        FSurfacePatchAnchor patch;
        double anchorLocalBoundsRadius = 0.0;
    };

    TArray<FPreparedPatch> preparedPatches;
};
