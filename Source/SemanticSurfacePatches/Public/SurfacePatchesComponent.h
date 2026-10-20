#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "SurfacePatchTypes.h"
#include "SurfacePatchesComponent.generated.h"

class USkeletalMeshComponent;
class USurfacePatchAnchorComponent;

UCLASS(BlueprintType, ClassGroup = (SemanticSurfacePatches), meta = (BlueprintSpawnableComponent))
class SEMANTICSURFACEPATCHES_API USurfacePatchesComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USurfacePatchesComponent();

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Surface Patches",
        meta = (
            DisplayName = "Selected Target Mesh",
            ToolTip = "The skeletal mesh selected when the preset was loaded."))
    FName targetMeshComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface Patches")
    FName defaultParentBone = TEXT("head");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Patches")
    TArray<TObjectPtr<USurfacePatchAnchorComponent>> surfacePatches;

    UFUNCTION(BlueprintPure, Category = "Surface Patches")
    bool hasPatch(FName patchId) const;

    UFUNCTION(BlueprintPure, Category = "Surface Patches")
    bool getPatch(FName patchId, FSurfacePatchAnchor& outPatch) const;

    UFUNCTION(BlueprintPure, Category = "Surface Patches")
    bool getPatchWorldTransform(FName patchId, FTransform& outTransform) const;

    UFUNCTION(BlueprintPure, Category = "Surface Patches")
    TArray<FSurfacePatchAnchor> getEnabledPatches() const;

    UFUNCTION(BlueprintPure, Category = "Surface Patches")
    TArray<USurfacePatchAnchorComponent*> getSurfacePatchComponents() const;

    UFUNCTION(BlueprintPure, Category = "Surface Patches")
    USkeletalMeshComponent* getTargetMeshComponent() const;

    UFUNCTION(BlueprintPure, Category = "Surface Patches")
    bool findClosestPatch(
        FVector worldPosition,
        FName& outPatchId,
        FTransform& outTransform,
        float& outDistance) const;

    bool getBoneWorldTransform(FName boneName, FTransform& outTransform) const;
};
