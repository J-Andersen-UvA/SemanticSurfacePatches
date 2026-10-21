#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SemanticPatchProximityEvaluator.h"

namespace
{
FSurfacePatchAnchor makeTestPatch()
{
    FSurfacePatchAnchor patch;
    patch.patchId = TEXT("testPatch");
    patch.parentBone = TEXT("head");
    patch.surfacePatchRadius = 2.0;
    patch.boneLocalSurfaceNormal = FVector::ForwardVector;
    patch.boneLocalSemanticUp = FVector::UpVector;
    FSurfacePatchTriangle& triangle = patch.surfacePatch.AddDefaulted_GetRef();
    triangle.first = FVector(0.0, -2.0, -2.0);
    triangle.second = FVector(0.0, 2.0, -2.0);
    triangle.third = FVector(0.0, 0.0, 2.0);
    return patch;
}

FSemanticPose makeTestPose()
{
    FSemanticPose pose;
    pose.componentSpaceBoneTransforms.Add(TEXT("head"), FTransform::Identity);
    pose.componentSpaceBoneTransforms.Add(TEXT("finger"), FTransform::Identity);
    return pose;
}

FSemanticInteractionPoint makeTestPoint(const FVector& position)
{
    FSemanticInteractionPoint point;
    point.pointId = TEXT("leftIndexTip");
    point.interactionGroupId = TEXT("leftIndex");
    point.parentBone = TEXT("finger");
    point.boneLocalPosition = position;
    return point;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSemanticPatchProximityContactTest,
    "SemanticSurfacePatches.Proximity.Contact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSemanticPatchProximityContactTest::RunTest(const FString& parameters)
{
    FSemanticPatchProximityEvaluator evaluator;
    evaluator.setPatches({makeTestPatch()});
    FPatchProximitySettings settings;
    settings.proximityDistance = 0.5;

    const TArray<FPatchProximityResult> results = evaluator.evaluate(
        makeTestPose(),
        {makeTestPoint(FVector(0.25, 1.0, -1.0))},
        settings);

    TestEqual(TEXT("One contact is returned"), results.Num(), 1);
    if (results.Num() != 1)
    {
        return false;
    }

    TestEqual(TEXT("Point ID is retained"), results[0].pointId, FName(TEXT("leftIndexTip")));
    TestEqual(TEXT("Patch ID is retained"), results[0].patchId, FName(TEXT("testPatch")));
    TestTrue(TEXT("Distance is exact"), FMath::IsNearlyEqual(results[0].distance, 0.25, 1e-6));
    TestTrue(TEXT("Signed distance is outward"), FMath::IsNearlyEqual(results[0].signedDistance, 0.25, 1e-6));
    TestTrue(TEXT("Normalized right coordinate is retained"), FMath::IsNearlyEqual(results[0].normalizedPatchLocation.X, 0.5, 1e-6));
    TestTrue(TEXT("Normalized up coordinate is retained"), FMath::IsNearlyEqual(results[0].normalizedPatchLocation.Y, -0.5, 1e-6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSemanticPatchProximityThresholdTest,
    "SemanticSurfacePatches.Proximity.Threshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSemanticPatchProximityThresholdTest::RunTest(const FString& parameters)
{
    FSemanticPatchProximityEvaluator evaluator;
    evaluator.setPatches({makeTestPatch()});
    FPatchProximitySettings settings;
    settings.proximityDistance = 0.5;

    const TArray<FPatchProximityResult> results = evaluator.evaluate(
        makeTestPose(),
        {makeTestPoint(FVector(0.75, 0.0, 0.0))},
        settings);
    TestTrue(TEXT("Point outside threshold is rejected"), results.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSemanticPatchProximityTargetMappingTest,
    "SemanticSurfacePatches.Proximity.TargetMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSemanticPatchProximityTargetMappingTest::RunTest(
    const FString& parameters)
{
    FSemanticPatchProximityEvaluator evaluator;
    evaluator.setPatches({makeTestPatch()});

    FPatchProximityResult sourceResult;
    sourceResult.patchId = TEXT("testPatch");
    sourceResult.normalizedPatchLocation = FVector2D(0.5, -0.5);
    sourceResult.signedDistance = 0.25;

    FSemanticPose targetPose;
    targetPose.componentSpaceBoneTransforms.Add(
        TEXT("head"),
        FTransform(FVector(10.0, 20.0, 30.0)));
    const FPatchProximityTarget target = evaluator.mapToTarget(
        targetPose,
        sourceResult);

    TestTrue(TEXT("Target mapping succeeds"), target.isValid);
    TestTrue(
        TEXT("Patch point follows the target pose"),
        target.componentSpacePatchPoint.Equals(
            FVector(10.0, 21.0, 29.0),
            1e-6));
    TestTrue(
        TEXT("Interaction goal defaults to the target surface"),
        target.componentSpaceInteractionGoal.Equals(
            FVector(10.0, 21.0, 29.0),
            1e-6));
    TestTrue(
        TEXT("Source normal offset remains available for diagnostics"),
        FMath::IsNearlyEqual(target.sourceNormalOffset, 0.25, 1e-6));
    TestTrue(
        TEXT("Mapped coordinates retain the requested location"),
        target.mappedNormalizedPatchLocation.Equals(
            sourceResult.normalizedPatchLocation,
            1e-6));

    const FPatchProximityTarget offsetTarget = evaluator.mapToTarget(
        targetPose,
        sourceResult,
        0.4);
    TestTrue(
        TEXT("An explicit target normal offset is applied"),
        offsetTarget.componentSpaceInteractionGoal.Equals(
            FVector(10.4, 21.0, 29.0),
            1e-6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSemanticPatchProximityCanonicalFrameTest,
    "SemanticSurfacePatches.Proximity.CanonicalFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSemanticPatchProximityCanonicalFrameTest::RunTest(
    const FString& parameters)
{
    FSurfacePatchAnchor targetPatch = makeTestPatch();
    const FTransform rolledAnchor(
        FQuat(FVector::ForwardVector, PI));
    targetPatch.boneLocalTransform = rolledAnchor;
    for (FSurfacePatchTriangle& triangle : targetPatch.surfacePatch)
    {
        triangle.first = rolledAnchor.InverseTransformPosition(triangle.first);
        triangle.second = rolledAnchor.InverseTransformPosition(triangle.second);
        triangle.third = rolledAnchor.InverseTransformPosition(triangle.third);
    }

    FSemanticPatchProximityEvaluator evaluator;
    evaluator.setPatches({targetPatch});

    FPatchProximityResult sourceResult;
    sourceResult.patchId = TEXT("testPatch");
    sourceResult.normalizedPatchLocation = FVector2D(0.5, -0.5);
    const FPatchProximityTarget target = evaluator.mapToTarget(
        makeTestPose(),
        sourceResult);

    TestTrue(TEXT("Target mapping succeeds"), target.isValid);
    TestTrue(
        TEXT("Canonical coordinates ignore anchor roll"),
        target.componentSpacePatchPoint.Equals(
            FVector(0.0, 1.0, -1.0),
            1e-6));
    TestTrue(
        TEXT("Mapped coordinates retain the requested location"),
        target.mappedNormalizedPatchLocation.Equals(
            sourceResult.normalizedPatchLocation,
            1e-6));
    return true;
}

#endif
