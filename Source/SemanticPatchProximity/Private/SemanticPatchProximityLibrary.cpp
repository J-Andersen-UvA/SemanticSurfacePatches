#include "SemanticPatchProximityLibrary.h"

#include "SemanticPatchProximityEvaluator.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "SemanticInteractionPointComponent.h"

FSemanticPose USemanticPatchProximityLibrary::makePoseFromSkeletalMesh(
    const USkeletalMeshComponent* skeletalMesh)
{
    FSemanticPose pose;
    if (skeletalMesh == nullptr)
    {
        return pose;
    }

    const int32 boneCount = skeletalMesh->GetNumBones();
    pose.componentSpaceBoneTransforms.Reserve(boneCount);
    for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
    {
        pose.componentSpaceBoneTransforms.Add(
            skeletalMesh->GetBoneName(boneIndex),
            skeletalMesh->GetBoneTransform(boneIndex, FTransform::Identity));
    }
    return pose;
}

TArray<FSemanticInteractionPoint>
USemanticPatchProximityLibrary::getInteractionPoints(const AActor* actor)
{
    TArray<FSemanticInteractionPoint> points;
    if (actor == nullptr)
    {
        return points;
    }

    TArray<USemanticInteractionPointComponent*> components;
    actor->GetComponents(components);
    points.Reserve(components.Num());
    for (const USemanticInteractionPointComponent* component : components)
    {
        if (component != nullptr)
        {
            points.Add(component->toInteractionPoint());
        }
    }
    return points;
}

TArray<FPatchProximityResult>
USemanticPatchProximityLibrary::evaluatePatchProximity(
    const FSemanticPose& pose,
    const TArray<FSurfacePatchAnchor>& patches,
    const TArray<FSemanticInteractionPoint>& interactionPoints,
    const FPatchProximitySettings& settings)
{
    FSemanticPatchProximityEvaluator evaluator;
    evaluator.setPatches(patches);
    return evaluator.evaluate(pose, interactionPoints, settings);
}

FPatchProximityTarget
USemanticPatchProximityLibrary::mapPatchProximityToTarget(
    const FPatchProximityResult& sourceResult,
    const FSemanticPose& targetPose,
    const TArray<FSurfacePatchAnchor>& targetPatches,
    const double targetNormalOffset)
{
    FSemanticPatchProximityEvaluator evaluator;
    evaluator.setPatches(targetPatches);
    return evaluator.mapToTarget(
        targetPose,
        sourceResult,
        targetNormalOffset);
}
