#include "SemanticPatchProximityEditorLibrary.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "SemanticPatchProximityEvaluator.h"

namespace
{
TArray<FName> getRequiredBoneNames(
    const UAnimSequenceBase* animation,
    const FAnimPoseEvaluationOptions& evaluationOptions,
    const TArray<FSurfacePatchAnchor>& patches,
    const TArray<FSemanticInteractionPoint>& interactionPoints)
{
    const FReferenceSkeleton* referenceSkeleton = nullptr;
    if (evaluationOptions.OptionalSkeletalMesh != nullptr)
    {
        referenceSkeleton =
            &evaluationOptions.OptionalSkeletalMesh->GetRefSkeleton();
    }
    else if (animation != nullptr && animation->GetSkeleton() != nullptr)
    {
        referenceSkeleton =
            &animation->GetSkeleton()->GetReferenceSkeleton();
    }

    TSet<FName> requiredBoneNames;
    for (const FSurfacePatchAnchor& patch : patches)
    {
        requiredBoneNames.Add(patch.parentBone);
    }
    for (const FSemanticInteractionPoint& interactionPoint : interactionPoints)
    {
        requiredBoneNames.Add(interactionPoint.parentBone);
    }

    TArray<FName> validBoneNames;
    validBoneNames.Reserve(requiredBoneNames.Num());
    for (const FName boneName : requiredBoneNames)
    {
        if (!boneName.IsNone() &&
            referenceSkeleton != nullptr &&
            referenceSkeleton->FindBoneIndex(boneName) != INDEX_NONE)
        {
            validBoneNames.Add(boneName);
        }
    }
    return validBoneNames;
}

FSemanticPose makeSemanticPose(
    const FAnimPose& animPose,
    const TArray<FName>& requiredBoneNames)
{
    FSemanticPose semanticPose;
    if (!UAnimPoseExtensions::IsValid(animPose))
    {
        return semanticPose;
    }

    semanticPose.componentSpaceBoneTransforms.Reserve(
        requiredBoneNames.Num());
    for (const FName boneName : requiredBoneNames)
    {
        semanticPose.componentSpaceBoneTransforms.Add(
            boneName,
            UAnimPoseExtensions::GetBonePose(
                animPose,
                boneName,
                EAnimPoseSpaces::World));
    }
    return semanticPose;
}
}

FSemanticPose
USemanticPatchProximityEditorLibrary::makeSemanticPoseFromAnimPose(
    const FAnimPose& animPose)
{
    FSemanticPose semanticPose;
    if (!UAnimPoseExtensions::IsValid(animPose))
    {
        return semanticPose;
    }

    TArray<FName> boneNames;
    UAnimPoseExtensions::GetBoneNames(animPose, boneNames);
    semanticPose.componentSpaceBoneTransforms.Reserve(boneNames.Num());
    for (const FName boneName : boneNames)
    {
        semanticPose.componentSpaceBoneTransforms.Add(
            boneName,
            UAnimPoseExtensions::GetBonePose(
                animPose,
                boneName,
                EAnimPoseSpaces::World));
    }
    return semanticPose;
}

FPatchProximityAnimationData
USemanticPatchProximityEditorLibrary::extractPatchProximityFromAnimation(
    const UAnimSequenceBase* animation,
    FAnimPoseEvaluationOptions evaluationOptions,
    const TArray<FSurfacePatchAnchor>& patches,
    const TArray<FSemanticInteractionPoint>& interactionPoints,
    const FPatchProximitySettings& settings)
{
    FPatchProximityAnimationData animationData;
    if (animation == nullptr ||
        animation->GetSkeleton() == nullptr ||
        patches.IsEmpty() ||
        interactionPoints.IsEmpty())
    {
        return animationData;
    }

    const int32 frameCount = animation->GetNumberOfSampledKeys();
    const FFrameRate frameRate = animation->GetSamplingFrameRate();
    if (frameCount <= 0 || !frameRate.IsValid())
    {
        return animationData;
    }

    const TArray<FName> requiredBoneNames = getRequiredBoneNames(
        animation,
        evaluationOptions,
        patches,
        interactionPoints);
    if (requiredBoneNames.IsEmpty())
    {
        return animationData;
    }

    evaluationOptions.bEvaluateCurves = false;
    FSemanticPatchProximityEvaluator evaluator;
    evaluator.setPatches(patches);

    animationData.frameRateNumerator = frameRate.Numerator;
    animationData.frameRateDenominator = frameRate.Denominator;
    animationData.durationSeconds = animation->GetPlayLength();
    animationData.frames.Reserve(frameCount);

    constexpr int32 poseBatchSize = 256;
    for (int32 batchStart = 0;
         batchStart < frameCount;
         batchStart += poseBatchSize)
    {
        const int32 batchEnd = FMath::Min(
            batchStart + poseBatchSize,
            frameCount);
        TArray<double> timeIntervals;
        timeIntervals.Reserve(batchEnd - batchStart);
        for (int32 frameIndex = batchStart;
             frameIndex < batchEnd;
             ++frameIndex)
        {
            timeIntervals.Add(frameRate.AsSeconds(frameIndex));
        }

        TArray<FAnimPose> poses;
        poses.Reserve(timeIntervals.Num());
        UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(
            animation,
            timeIntervals,
            evaluationOptions,
            poses);
        if (poses.Num() != timeIntervals.Num())
        {
            animationData.frames.Reset();
            return animationData;
        }

        for (int32 poseIndex = 0;
             poseIndex < poses.Num();
             ++poseIndex)
        {
            FPatchProximityFrame& frame =
                animationData.frames.AddDefaulted_GetRef();
            frame.frameIndex = batchStart + poseIndex;
            frame.timeSeconds = timeIntervals[poseIndex];
            frame.results = evaluator.evaluate(
                makeSemanticPose(poses[poseIndex], requiredBoneNames),
                interactionPoints,
                settings);
        }
    }

    animationData.isValid = animationData.frames.Num() == frameCount;
    return animationData;
}
