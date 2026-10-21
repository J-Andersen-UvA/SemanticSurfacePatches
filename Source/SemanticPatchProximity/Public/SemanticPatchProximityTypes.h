#pragma once

#include "CoreMinimal.h"
#include "SemanticPatchProximityTypes.generated.h"

USTRUCT(BlueprintType)
struct SEMANTICPATCHPROXIMITY_API FSemanticPose
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    TMap<FName, FTransform> componentSpaceBoneTransforms;

    bool getBoneTransform(
        FName boneName,
        FTransform& outTransform) const;
};

USTRUCT(BlueprintType)
struct SEMANTICPATCHPROXIMITY_API FSemanticInteractionPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    FName pointId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    FName interactionGroupId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    FName parentBone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    FVector boneLocalPosition = FVector::ZeroVector;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Semantic Proximity",
        meta = (ClampMin = "0.0", Units = "cm"))
    double pointRadius = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Semantic Proximity")
    bool enabled = true;
};

USTRUCT(BlueprintType)
struct SEMANTICPATCHPROXIMITY_API FPatchProximitySettings
{
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Semantic Proximity",
        meta = (ClampMin = "0.0", Units = "cm"))
    double proximityDistance = 0.5;
};

USTRUCT(BlueprintType)
struct SEMANTICPATCHPROXIMITY_API FPatchProximityResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FName pointId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FName interactionGroupId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FName patchId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    double distance = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    double signedDistance = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector componentSpaceInteractionPoint = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector componentSpacePatchPoint = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector componentSpaceSurfaceNormal = FVector::ForwardVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector patchLocalClosestPoint = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector2D normalizedPatchLocation = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    int32 triangleIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector barycentricCoordinates = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct SEMANTICPATCHPROXIMITY_API FPatchProximityTarget
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    bool isValid = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FName pointId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FName interactionGroupId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FName patchId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector2D requestedNormalizedPatchLocation = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector2D mappedNormalizedPatchLocation = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    double sourceNormalOffset = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    double targetNormalOffset = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector componentSpacePatchPoint = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector componentSpaceSurfaceNormal = FVector::ForwardVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector componentSpaceInteractionGoal = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    int32 triangleIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    FVector barycentricCoordinates = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct SEMANTICPATCHPROXIMITY_API FPatchProximityFrame
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    int32 frameIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    double timeSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    TArray<FPatchProximityResult> results;
};

USTRUCT(BlueprintType)
struct SEMANTICPATCHPROXIMITY_API FPatchProximityAnimationData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    bool isValid = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    int32 frameRateNumerator = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    int32 frameRateDenominator = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    double durationSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Semantic Proximity")
    TArray<FPatchProximityFrame> frames;
};
