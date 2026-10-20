#pragma once

#include "CoreMinimal.h"
#include "SurfacePatchTypes.generated.h"

UENUM(BlueprintType)
enum class ESurfacePatchSide : uint8
{
    Center,
    Left,
    Right
};

USTRUCT(BlueprintType)
struct SEMANTICSURFACEPATCHES_API FSurfacePatchTriangle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Patch")
    FVector first = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Patch")
    FVector second = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Patch")
    FVector third = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct SEMANTICSURFACEPATCHES_API FSurfacePatchAnchor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    FName patchId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    ESurfacePatchSide side = ESurfacePatchSide::Center;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    FName parentBone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    FTransform boneLocalTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    bool enabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    bool useSurfacePatch = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    double surfacePatchRadius = 4.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    double surfacePatchNormalThreshold = 0.25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    double surfacePatchEdgeNormalThreshold = 0.8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    bool showSurfacePatch = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Patch")
    TArray<FSurfacePatchTriangle> surfacePatch;
};
