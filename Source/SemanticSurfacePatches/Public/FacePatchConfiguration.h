#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FacePatchConfiguration.generated.h"

class USkeletalMesh;

USTRUCT(BlueprintType)
struct SEMANTICSURFACEPATCHES_API FFacePatchTriangle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FVector first = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FVector second = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FVector third = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct SEMANTICSURFACEPATCHES_API FFacePatchAttachment
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FName boneName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FVector localPosition = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FVector localNormal = FVector::ForwardVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    TArray<FFacePatchTriangle> triangles;
};

USTRUCT(BlueprintType)
struct SEMANTICSURFACEPATCHES_API FPairedFacePatch
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FName patchName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FFacePatchAttachment source;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patch")
    FFacePatchAttachment target;
};

UCLASS(BlueprintType)
class SEMANTICSURFACEPATCHES_API UFacePatchConfiguration : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patches")
    TSoftObjectPtr<USkeletalMesh> sourceSkeletalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patches")
    TSoftObjectPtr<USkeletalMesh> targetSkeletalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Patches")
    TArray<FPairedFacePatch> facePatches;
};

