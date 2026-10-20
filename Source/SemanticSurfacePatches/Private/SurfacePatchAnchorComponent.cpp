#include "SurfacePatchAnchorComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

USurfacePatchAnchorComponent::USurfacePatchAnchorComponent()
{
#if WITH_EDITOR
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.1f;
    bTickInEditor = true;
#endif

    ArrowColor = FColor(255, 196, 32);
    ArrowLength = 8.0f;
    ArrowSize = 0.6f;
    bIsScreenSizeScaled = true;
    ScreenSize = 0.0025f;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetHiddenInGame(true);

    surfacePatchPreview = CreateDefaultSubobject<UProceduralMeshComponent>(
        TEXT("SurfacePatchPreview"));
    surfacePatchPreview->SetupAttachment(this);
    surfacePatchPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    surfacePatchPreview->SetGenerateOverlapEvents(false);
    surfacePatchPreview->SetCastShadow(false);
    surfacePatchPreview->SetHiddenInGame(true);
    surfacePatchPreview->bUseEditorCompositing = true;
    surfacePatchPreview->SetTranslucentSortPriority(100);
#if WITH_EDITORONLY_DATA
    surfacePatchPreview->SetIsVisualizationComponent(true);
#endif
}

void USurfacePatchAnchorComponent::OnRegister()
{
    Super::OnRegister();

    if (!IsValid(surfacePatchMaterial))
    {
        UMaterialInterface* baseMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            TEXT("/Engine/EngineDebugMaterials/GeomMaterial"));
        if (baseMaterial != nullptr)
        {
            surfacePatchMaterial = UMaterialInstanceDynamic::Create(
                baseMaterial,
                this);
            const FLinearColor yellow =
                FLinearColor::FromSRGBColor(FColor(244, 210, 96));
            surfacePatchMaterial->SetVectorParameterValue(
                TEXT("Color"),
                FLinearColor(yellow.R, yellow.G, yellow.B, 0.24f));
        }
    }

    refreshSurfacePatchPreview();
}

void USurfacePatchAnchorComponent::TickComponent(
    const float deltaTime,
    const ELevelTick tickType,
    FActorComponentTickFunction* tickFunction)
{
    Super::TickComponent(deltaTime, tickType, tickFunction);
#if WITH_EDITOR
    updateSurfacePatchPreviewVisibility();
#endif
}

#if WITH_EDITOR
void USurfacePatchAnchorComponent::PostEditChangeProperty(
    FPropertyChangedEvent& propertyChangedEvent)
{
    Super::PostEditChangeProperty(propertyChangedEvent);
    refreshSurfacePatchPreview();
}
#endif

void USurfacePatchAnchorComponent::refreshSurfacePatchPreview()
{
    if (!IsValid(surfacePatchPreview))
    {
        return;
    }

    surfacePatchPreview->ClearAllMeshSections();
    surfacePatchPreview->SetVisibility(false);
    if (!useSurfacePatch || !showSurfacePatch || surfacePatch.IsEmpty())
    {
        return;
    }

    TArray<FVector> vertices;
    TArray<int32> indices;
    TArray<FVector> normals;
    TArray<FVector2D> textureCoordinates;
    TArray<FLinearColor> vertexColors;
    TArray<FProcMeshTangent> tangents;
    vertices.Reserve(surfacePatch.Num() * 3);
    indices.Reserve(surfacePatch.Num() * 3);
    normals.Reserve(surfacePatch.Num() * 3);
    textureCoordinates.Reserve(surfacePatch.Num() * 3);
    vertexColors.Reserve(surfacePatch.Num() * 3);
    tangents.Reserve(surfacePatch.Num() * 3);

    const FLinearColor yellow =
        FLinearColor::FromSRGBColor(FColor(244, 210, 96));
    constexpr double localSurfaceOffset = 0.2;
    for (const FSurfacePatchTriangle& triangle : surfacePatch)
    {
        const FVector normal = FVector::CrossProduct(
            triangle.second - triangle.first,
            triangle.third - triangle.first).GetSafeNormal();
        if (normal.IsNearlyZero())
        {
            continue;
        }

        const int32 firstIndex = vertices.Num();
        vertices.Add(triangle.first + normal * localSurfaceOffset);
        vertices.Add(triangle.second + normal * localSurfaceOffset);
        vertices.Add(triangle.third + normal * localSurfaceOffset);
        indices.Append({firstIndex, firstIndex + 2, firstIndex + 1});
        normals.Append({normal, normal, normal});
        textureCoordinates.Append({
            FVector2D(0.0, 0.0),
            FVector2D(1.0, 0.0),
            FVector2D(0.0, 1.0)});
        vertexColors.Append({yellow, yellow, yellow});
        const FProcMeshTangent tangent(
            (triangle.second - triangle.first).GetSafeNormal(),
            false);
        tangents.Append({tangent, tangent, tangent});
    }

    if (vertices.IsEmpty())
    {
        return;
    }

    surfacePatchPreview->CreateMeshSection_LinearColor(
        0,
        vertices,
        indices,
        normals,
        textureCoordinates,
        vertexColors,
        tangents,
        false);
    if (surfacePatchMaterial != nullptr)
    {
        surfacePatchPreview->SetMaterial(0, surfacePatchMaterial);
    }
    updateSurfacePatchPreviewVisibility();
}

void USurfacePatchAnchorComponent::updateSurfacePatchPreviewVisibility()
{
    if (!IsValid(surfacePatchPreview))
    {
        return;
    }

    bool isSelected = false;
#if WITH_EDITOR
    isSelected = IsSelectedInEditor() || IsComponentIndividuallySelected();
#endif
    surfacePatchPreview->SetVisibility(
        useSurfacePatch &&
        showSurfacePatch &&
        !surfacePatch.IsEmpty() &&
        isSelected);
}

FSurfacePatchAnchor USurfacePatchAnchorComponent::toPatchData() const
{
    FSurfacePatchAnchor patch;
    patch.patchId = patchId;
    patch.displayName = displayName;
    patch.side = side;
    patch.enabled = enabled;
    patch.useSurfacePatch = useSurfacePatch;
    patch.surfacePatchRadius = surfacePatchRadius;
    patch.surfacePatchNormalThreshold = surfacePatchNormalThreshold;
    patch.surfacePatchEdgeNormalThreshold = surfacePatchEdgeNormalThreshold;
    patch.showSurfacePatch = showSurfacePatch;
    patch.boneLocalTransform = GetRelativeTransform();
    patch.surfacePatch = surfacePatch;

    if (const USkeletalMeshComponent* parentMesh =
        Cast<USkeletalMeshComponent>(GetAttachParent()))
    {
        patch.parentBone = GetAttachSocketName();
    }

    return patch;
}
