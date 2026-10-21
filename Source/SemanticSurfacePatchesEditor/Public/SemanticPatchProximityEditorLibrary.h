#pragma once

#include "AnimPose.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SemanticPatchProximityTypes.h"
#include "SemanticPatchProximityEditorLibrary.generated.h"

UCLASS()
class SEMANTICSURFACEPATCHESEDITOR_API USemanticPatchProximityEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintPure,
        Category = "Semantic Patch Proximity",
        meta = (DisplayName = "Make Semantic Pose From Anim Pose"))
    static FSemanticPose makeSemanticPoseFromAnimPose(
        UPARAM(ref) const FAnimPose& animPose);

    UFUNCTION(
        BlueprintCallable,
        Category = "Semantic Patch Proximity",
        meta = (DisplayName = "Extract Patch Proximity From Animation"))
    static FPatchProximityAnimationData extractPatchProximityFromAnimation(
        const UAnimSequenceBase* animation,
        FAnimPoseEvaluationOptions evaluationOptions,
        const TArray<FSurfacePatchAnchor>& patches,
        const TArray<FSemanticInteractionPoint>& interactionPoints,
        const FPatchProximitySettings& settings);
};
