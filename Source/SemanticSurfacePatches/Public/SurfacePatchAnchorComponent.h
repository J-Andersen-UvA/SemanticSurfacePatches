#pragma once

#include "Components/ArrowComponent.h"
#include "CoreMinimal.h"
#include "SurfacePatchTypes.h"
#include "SurfacePatchAnchorComponent.generated.h"

UCLASS(
    BlueprintType,
    ClassGroup = (SemanticSurfacePatches),
    meta = (BlueprintSpawnableComponent))
class SEMANTICSURFACEPATCHES_API USurfacePatchAnchorComponent
    : public UArrowComponent
{
    GENERATED_BODY()

public:
    USurfacePatchAnchorComponent();

    virtual void OnRegister() override;

    virtual void TickComponent(
        float deltaTime,
        ELevelTick tickType,
        FActorComponentTickFunction* tickFunction) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(
        FPropertyChangedEvent& propertyChangedEvent) override;
#endif

    void refreshSurfacePatchPreview();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    FName patchId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    ESurfacePatchSide side = ESurfacePatchSide::Center;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    bool enabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Patch")
    bool useSurfacePatch = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Surface Patch",
        meta = (ClampMin = "0.1", EditCondition = "useSurfacePatch", Units = "cm"))
    double surfacePatchRadius = 4.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Surface Patch",
        meta = (ClampMin = "-1.0", ClampMax = "1.0", EditCondition = "useSurfacePatch"))
    double surfacePatchNormalThreshold = 0.25;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Surface Patch",
        meta = (ClampMin = "-1.0", ClampMax = "1.0", EditCondition = "useSurfacePatch"))
    double surfacePatchEdgeNormalThreshold = 0.8;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Surface Patch",
        meta = (EditCondition = "useSurfacePatch"))
    bool showSurfacePatch = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Patch")
    TArray<FSurfacePatchTriangle> surfacePatch;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Patch")
    FVector boneLocalSurfaceNormal = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surface Patch")
    FVector boneLocalSemanticUp = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, Category = "Surface Patch")
    TObjectPtr<class UProceduralMeshComponent> surfacePatchPreview;

    FSurfacePatchAnchor toPatchData() const;

private:
    void updateSurfacePatchPreviewVisibility();

    UPROPERTY(Transient)
    TObjectPtr<class UMaterialInstanceDynamic> surfacePatchMaterial;
};
