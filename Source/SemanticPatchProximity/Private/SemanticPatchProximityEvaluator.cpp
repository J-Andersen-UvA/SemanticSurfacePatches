#include "SemanticPatchProximityEvaluator.h"

namespace
{
FTransform makePatchComponentTransform(
    const FTransform& boneTransform,
    const FTransform& boneLocalTransform)
{
    FQuat rotation = boneTransform.GetRotation() *
        boneLocalTransform.GetRotation();
    rotation.Normalize();
    return FTransform(
        rotation,
        boneTransform.TransformPosition(boneLocalTransform.GetLocation()),
        boneTransform.GetScale3D() * boneLocalTransform.GetScale3D());
}

struct FSemanticPatchFrame
{
    FTransform geometryTransform = FTransform::Identity;
    FVector origin = FVector::ZeroVector;
    FVector normal = FVector::ForwardVector;
    FVector horizontal = FVector::RightVector;
    FVector up = FVector::UpVector;
};

bool makeSemanticPatchFrame(
    const FSurfacePatchAnchor& patch,
    const FTransform& boneTransform,
    FSemanticPatchFrame& outFrame)
{
    outFrame.geometryTransform = makePatchComponentTransform(
        boneTransform,
        patch.boneLocalTransform);
    outFrame.origin = outFrame.geometryTransform.GetLocation();

    const bool hasStoredFrame =
        !patch.boneLocalSurfaceNormal.IsNearlyZero() &&
        !patch.boneLocalSemanticUp.IsNearlyZero();
    outFrame.normal = hasStoredFrame
        ? boneTransform.TransformVectorNoScale(
            patch.boneLocalSurfaceNormal).GetSafeNormal()
        : outFrame.geometryTransform
            .TransformVectorNoScale(FVector::ForwardVector)
            .GetSafeNormal();
    FVector upHint = hasStoredFrame
        ? boneTransform.TransformVectorNoScale(
            patch.boneLocalSemanticUp).GetSafeNormal()
        : outFrame.geometryTransform
            .TransformVectorNoScale(FVector::UpVector)
            .GetSafeNormal();
    if (outFrame.normal.IsNearlyZero() || upHint.IsNearlyZero())
    {
        return false;
    }

    outFrame.up = upHint - outFrame.normal * FVector::DotProduct(
        upHint,
        outFrame.normal);
    if (!outFrame.up.Normalize())
    {
        return false;
    }
    outFrame.horizontal = FVector::CrossProduct(
        outFrame.up,
        outFrame.normal).GetSafeNormal();
    if (outFrame.horizontal.IsNearlyZero())
    {
        return false;
    }
    outFrame.up = FVector::CrossProduct(
        outFrame.normal,
        outFrame.horizontal).GetSafeNormal();
    return !outFrame.up.IsNearlyZero();
}

FVector projectOntoSemanticPlane(
    const FVector& point,
    const FSemanticPatchFrame& frame)
{
    const FVector offset = point - frame.origin;
    return FVector(
        0.0,
        FVector::DotProduct(offset, frame.horizontal),
        FVector::DotProduct(offset, frame.up));
}

FVector calculateBarycentricCoordinates(
    const FVector& point,
    const FVector& first,
    const FVector& second,
    const FVector& third)
{
    const FVector firstEdge = second - first;
    const FVector secondEdge = third - first;
    const FVector relativePoint = point - first;
    const double firstDot = FVector::DotProduct(firstEdge, firstEdge);
    const double secondDot = FVector::DotProduct(firstEdge, secondEdge);
    const double thirdDot = FVector::DotProduct(secondEdge, secondEdge);
    const double fourthDot = FVector::DotProduct(relativePoint, firstEdge);
    const double fifthDot = FVector::DotProduct(relativePoint, secondEdge);
    const double denominator = firstDot * thirdDot - secondDot * secondDot;
    if (FMath::IsNearlyZero(denominator))
    {
        return FVector::ZeroVector;
    }

    const double secondWeight =
        (thirdDot * fourthDot - secondDot * fifthDot) / denominator;
    const double thirdWeight =
        (firstDot * fifthDot - secondDot * fourthDot) / denominator;
    return FVector(1.0 - secondWeight - thirdWeight, secondWeight, thirdWeight);
}

bool tryCalculateBarycentricCoordinates(
    const FVector& point,
    const FVector& first,
    const FVector& second,
    const FVector& third,
    FVector& outBarycentric)
{
    const FVector firstEdge = second - first;
    const FVector secondEdge = third - first;
    const FVector relativePoint = point - first;
    const double firstDot = FVector::DotProduct(firstEdge, firstEdge);
    const double secondDot = FVector::DotProduct(firstEdge, secondEdge);
    const double thirdDot = FVector::DotProduct(secondEdge, secondEdge);
    const double denominator = firstDot * thirdDot - secondDot * secondDot;
    if (FMath::IsNearlyZero(denominator))
    {
        return false;
    }

    const double fourthDot = FVector::DotProduct(relativePoint, firstEdge);
    const double fifthDot = FVector::DotProduct(relativePoint, secondEdge);
    const double secondWeight =
        (thirdDot * fourthDot - secondDot * fifthDot) / denominator;
    const double thirdWeight =
        (firstDot * fifthDot - secondDot * fourthDot) / denominator;
    outBarycentric = FVector(
        1.0 - secondWeight - thirdWeight,
        secondWeight,
        thirdWeight);
    return true;
}
}

void FSemanticPatchProximityEvaluator::setPatches(
    const TArray<FSurfacePatchAnchor>& patches)
{
    preparedPatches.Reset(patches.Num());
    for (const FSurfacePatchAnchor& patch : patches)
    {
        FPreparedPatch& prepared = preparedPatches.AddDefaulted_GetRef();
        prepared.patch = patch;
        for (const FSurfacePatchTriangle& triangle : patch.surfacePatch)
        {
            prepared.anchorLocalBoundsRadius = FMath::Max(
                prepared.anchorLocalBoundsRadius,
                triangle.first.Size());
            prepared.anchorLocalBoundsRadius = FMath::Max(
                prepared.anchorLocalBoundsRadius,
                triangle.second.Size());
            prepared.anchorLocalBoundsRadius = FMath::Max(
                prepared.anchorLocalBoundsRadius,
                triangle.third.Size());
        }
    }
}

TArray<FPatchProximityResult>
FSemanticPatchProximityEvaluator::evaluate(
    const FSemanticPose& pose,
    const TArray<FSemanticInteractionPoint>& interactionPoints,
    const FPatchProximitySettings& settings) const
{
    TArray<FPatchProximityResult> results;
    for (const FSemanticInteractionPoint& interactionPoint : interactionPoints)
    {
        if (!interactionPoint.enabled)
        {
            continue;
        }

        FTransform pointBoneTransform;
        if (!pose.getBoneTransform(
                interactionPoint.parentBone,
                pointBoneTransform))
        {
            continue;
        }
        const FVector pointPosition = pointBoneTransform.TransformPosition(
            interactionPoint.boneLocalPosition);

        for (const FPreparedPatch& prepared : preparedPatches)
        {
            const FSurfacePatchAnchor& patch = prepared.patch;
            if (!patch.enabled ||
                !patch.useSurfacePatch ||
                patch.surfacePatch.IsEmpty())
            {
                continue;
            }

            FTransform patchBoneTransform;
            if (!pose.getBoneTransform(patch.parentBone, patchBoneTransform))
            {
                continue;
            }
            FSemanticPatchFrame patchFrame;
            if (!makeSemanticPatchFrame(
                    patch,
                    patchBoneTransform,
                    patchFrame))
            {
                continue;
            }
            const double patchBoundsRadius =
                prepared.anchorLocalBoundsRadius *
                patchFrame.geometryTransform.GetMaximumAxisScale();
            const double contactDistance = FMath::Max(
                0.0,
                settings.proximityDistance + interactionPoint.pointRadius);
            const double broadPhaseRadius =
                patchBoundsRadius + contactDistance;
            if (FVector::DistSquared(
                    pointPosition,
                    patchFrame.origin) >
                FMath::Square(broadPhaseRadius))
            {
                continue;
            }

            double closestDistanceSquared = TNumericLimits<double>::Max();
            FVector closestPoint = FVector::ZeroVector;
            FVector closestNormal = FVector::ForwardVector;
            FVector closestBarycentric = FVector::ZeroVector;
            int32 closestTriangleIndex = INDEX_NONE;
            for (int32 triangleIndex = 0;
                 triangleIndex < patch.surfacePatch.Num();
                 ++triangleIndex)
            {
                const FSurfacePatchTriangle& triangle =
                    patch.surfacePatch[triangleIndex];
                const FVector first = patchFrame.geometryTransform.TransformPosition(
                    triangle.first);
                const FVector second = patchFrame.geometryTransform.TransformPosition(
                    triangle.second);
                const FVector third = patchFrame.geometryTransform.TransformPosition(
                    triangle.third);
                FVector normal = FVector::CrossProduct(
                    second - first,
                    third - first).GetSafeNormal();
                if (normal.IsNearlyZero())
                {
                    continue;
                }
                if (FVector::DotProduct(normal, patchFrame.normal) < 0.0)
                {
                    normal = -normal;
                }

                const FVector trianglePoint =
                    FMath::ClosestPointOnTriangleToPoint(
                        pointPosition,
                        first,
                        second,
                        third);
                const double distanceSquared = FVector::DistSquared(
                    pointPosition,
                    trianglePoint);
                if (distanceSquared >= closestDistanceSquared)
                {
                    continue;
                }

                closestDistanceSquared = distanceSquared;
                closestPoint = trianglePoint;
                closestNormal = normal;
                closestBarycentric = calculateBarycentricCoordinates(
                    trianglePoint,
                    first,
                    second,
                    third);
                closestTriangleIndex = triangleIndex;
            }

            if (closestTriangleIndex == INDEX_NONE ||
                closestDistanceSquared > FMath::Square(contactDistance))
            {
                continue;
            }

            FPatchProximityResult& result = results.AddDefaulted_GetRef();
            result.pointId = interactionPoint.pointId;
            result.interactionGroupId = interactionPoint.interactionGroupId;
            result.patchId = patch.patchId;
            result.distance = FMath::Sqrt(closestDistanceSquared);
            result.signedDistance = FVector::DotProduct(
                pointPosition - closestPoint,
                closestNormal);
            result.componentSpaceInteractionPoint = pointPosition;
            result.componentSpacePatchPoint = closestPoint;
            result.componentSpaceSurfaceNormal = closestNormal;
            result.patchLocalClosestPoint = patchFrame.geometryTransform
                .InverseTransformPosition(closestPoint);
            result.normalizedPatchLocation = patch.surfacePatchRadius > SMALL_NUMBER
                ? FVector2D(
                    FVector::DotProduct(
                        closestPoint - patchFrame.origin,
                        patchFrame.horizontal) / patch.surfacePatchRadius,
                    FVector::DotProduct(
                        closestPoint - patchFrame.origin,
                        patchFrame.up) / patch.surfacePatchRadius)
                : FVector2D::ZeroVector;
            result.triangleIndex = closestTriangleIndex;
            result.barycentricCoordinates = closestBarycentric;
        }
    }

    results.Sort(
        [](const FPatchProximityResult& first,
           const FPatchProximityResult& second)
        {
            return first.distance < second.distance;
        });
    return results;
}

FPatchProximityTarget FSemanticPatchProximityEvaluator::mapToTarget(
    const FSemanticPose& targetPose,
    const FPatchProximityResult& sourceResult,
    const double targetNormalOffset) const
{
    FPatchProximityTarget target;
    target.pointId = sourceResult.pointId;
    target.interactionGroupId = sourceResult.interactionGroupId;
    target.patchId = sourceResult.patchId;
    target.requestedNormalizedPatchLocation =
        sourceResult.normalizedPatchLocation;
    target.sourceNormalOffset = sourceResult.signedDistance;
    target.targetNormalOffset = targetNormalOffset;

    const FPreparedPatch* prepared = preparedPatches.FindByPredicate(
        [&sourceResult](const FPreparedPatch& candidate)
        {
            return candidate.patch.patchId == sourceResult.patchId;
        });
    if (prepared == nullptr ||
        !prepared->patch.enabled ||
        !prepared->patch.useSurfacePatch ||
        prepared->patch.surfacePatch.IsEmpty())
    {
        return target;
    }

    FTransform patchBoneTransform;
    if (!targetPose.getBoneTransform(
            prepared->patch.parentBone,
            patchBoneTransform))
    {
        return target;
    }

    const double patchRadius = prepared->patch.surfacePatchRadius;
    if (patchRadius <= SMALL_NUMBER)
    {
        return target;
    }
    FSemanticPatchFrame patchFrame;
    if (!makeSemanticPatchFrame(
            prepared->patch,
            patchBoneTransform,
            patchFrame))
    {
        return target;
    }

    const FVector requestedProjectedPoint(
        0.0,
        sourceResult.normalizedPatchLocation.X * patchRadius,
        sourceResult.normalizedPatchLocation.Y * patchRadius);
    double closestProjectedDistanceSquared = TNumericLimits<double>::Max();
    FVector closestPoint = FVector::ZeroVector;
    FVector closestNormal = FVector::ForwardVector;
    FVector closestBarycentric = FVector::ZeroVector;
    int32 closestTriangleIndex = INDEX_NONE;

    for (int32 triangleIndex = 0;
         triangleIndex < prepared->patch.surfacePatch.Num();
         ++triangleIndex)
    {
        const FSurfacePatchTriangle& triangle =
            prepared->patch.surfacePatch[triangleIndex];
        const FVector first = patchFrame.geometryTransform.TransformPosition(
            triangle.first);
        const FVector second = patchFrame.geometryTransform.TransformPosition(
            triangle.second);
        const FVector third = patchFrame.geometryTransform.TransformPosition(
            triangle.third);
        const FVector projectedFirst = projectOntoSemanticPlane(
            first,
            patchFrame);
        const FVector projectedSecond = projectOntoSemanticPlane(
            second,
            patchFrame);
        const FVector projectedThird = projectOntoSemanticPlane(
            third,
            patchFrame);
        const FVector projectedPoint = FMath::ClosestPointOnTriangleToPoint(
            requestedProjectedPoint,
            projectedFirst,
            projectedSecond,
            projectedThird);
        const double projectedDistanceSquared = FVector::DistSquared(
            requestedProjectedPoint,
            projectedPoint);
        if (projectedDistanceSquared >= closestProjectedDistanceSquared)
        {
            continue;
        }

        FVector barycentric;
        if (!tryCalculateBarycentricCoordinates(
                projectedPoint,
                projectedFirst,
                projectedSecond,
                projectedThird,
                barycentric))
        {
            continue;
        }

        FVector normal = FVector::CrossProduct(
            second - first,
            third - first).GetSafeNormal();
        if (normal.IsNearlyZero())
        {
            continue;
        }
        if (FVector::DotProduct(normal, patchFrame.normal) < 0.0)
        {
            normal = -normal;
        }

        closestProjectedDistanceSquared = projectedDistanceSquared;
        closestPoint =
            first * barycentric.X +
            second * barycentric.Y +
            third * barycentric.Z;
        closestNormal = normal;
        closestBarycentric = barycentric;
        closestTriangleIndex = triangleIndex;
    }

    if (closestTriangleIndex == INDEX_NONE)
    {
        return target;
    }

    target.componentSpacePatchPoint = closestPoint;
    target.componentSpaceSurfaceNormal = closestNormal;
    target.componentSpaceInteractionGoal =
        target.componentSpacePatchPoint +
        target.componentSpaceSurfaceNormal * target.targetNormalOffset;
    target.mappedNormalizedPatchLocation = FVector2D(
        FVector::DotProduct(
            closestPoint - patchFrame.origin,
            patchFrame.horizontal) / patchRadius,
        FVector::DotProduct(
            closestPoint - patchFrame.origin,
            patchFrame.up) / patchRadius);
    target.triangleIndex = closestTriangleIndex;
    target.barycentricCoordinates = closestBarycentric;
    target.isValid = true;
    return target;
}
