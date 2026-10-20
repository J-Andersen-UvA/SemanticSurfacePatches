#include "SurfacePatchesComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "SurfacePatchAnchorComponent.h"

namespace
{
FName getStableComponentName(const UActorComponent* component)
{
    if (component == nullptr)
    {
        return NAME_None;
    }

    FString componentName = component->GetName();
    componentName.RemoveFromEnd(TEXT("_GEN_VARIABLE"));
    return FName(*componentName);
}
}

USurfacePatchesComponent::USurfacePatchesComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

TArray<USurfacePatchAnchorComponent*>
USurfacePatchesComponent::getSurfacePatchComponents() const
{
    TArray<USurfacePatchAnchorComponent*> patches;
    if (const AActor* owner = GetOwner())
    {
        owner->GetComponents(patches);
    }
    if (patches.IsEmpty())
    {
        for (USurfacePatchAnchorComponent* patch : surfacePatches)
        {
            if (patch != nullptr)
            {
                patches.Add(patch);
            }
        }
    }
    return patches;
}

bool USurfacePatchesComponent::hasPatch(const FName patchId) const
{
    return getSurfacePatchComponents().ContainsByPredicate(
        [patchId](const USurfacePatchAnchorComponent* patch)
        {
            return patch != nullptr && patch->patchId == patchId;
        });
}

bool USurfacePatchesComponent::getPatch(
    const FName patchId,
    FSurfacePatchAnchor& outPatch) const
{
    for (const USurfacePatchAnchorComponent* patch :
         getSurfacePatchComponents())
    {
        if (patch != nullptr && patch->patchId == patchId)
        {
            outPatch = patch->toPatchData();
            return true;
        }
    }
    return false;
}

bool USurfacePatchesComponent::getPatchWorldTransform(
    const FName patchId,
    FTransform& outTransform) const
{
    for (const USurfacePatchAnchorComponent* patch :
         getSurfacePatchComponents())
    {
        if (patch != nullptr && patch->patchId == patchId)
        {
            outTransform = patch->GetComponentTransform();
            return true;
        }
    }
    return false;
}

TArray<FSurfacePatchAnchor> USurfacePatchesComponent::getEnabledPatches() const
{
    TArray<FSurfacePatchAnchor> enabledPatches;
    for (const USurfacePatchAnchorComponent* patch :
         getSurfacePatchComponents())
    {
        if (patch != nullptr && patch->enabled)
        {
            enabledPatches.Add(patch->toPatchData());
        }
    }
    return enabledPatches;
}

USkeletalMeshComponent* USurfacePatchesComponent::getTargetMeshComponent() const
{
    if (AActor* owner = GetOwner())
    {
        TArray<USkeletalMeshComponent*> meshes;
        owner->GetComponents(meshes);
        for (USkeletalMeshComponent* mesh : meshes)
        {
            if (targetMeshComponentName.IsNone() ||
                getStableComponentName(mesh) == targetMeshComponentName)
            {
                return mesh;
            }
        }
    }
    return nullptr;
}

bool USurfacePatchesComponent::findClosestPatch(
    const FVector worldPosition,
    FName& outPatchId,
    FTransform& outTransform,
    float& outDistance) const
{
    bool foundPatch = false;
    double closestSquaredDistance = TNumericLimits<double>::Max();

    for (const USurfacePatchAnchorComponent* patch :
         getSurfacePatchComponents())
    {
        if (patch == nullptr || !patch->enabled)
        {
            continue;
        }

        const FTransform patchTransform = patch->GetComponentTransform();
        const double squaredDistance = FVector::DistSquared(
            worldPosition,
            patchTransform.GetLocation());
        if (squaredDistance >= closestSquaredDistance)
        {
            continue;
        }

        foundPatch = true;
        closestSquaredDistance = squaredDistance;
        outPatchId = patch->patchId;
        outTransform = patchTransform;
    }

    outDistance = foundPatch
        ? static_cast<float>(FMath::Sqrt(closestSquaredDistance))
        : TNumericLimits<float>::Max();
    return foundPatch;
}

bool USurfacePatchesComponent::getBoneWorldTransform(
    const FName boneName,
    FTransform& outTransform) const
{
    USkeletalMeshComponent* targetMesh = getTargetMeshComponent();
    if (targetMesh == nullptr || boneName.IsNone())
    {
        return false;
    }

    const int32 boneIndex = targetMesh->GetBoneIndex(boneName);
    if (boneIndex == INDEX_NONE)
    {
        return false;
    }

    outTransform = targetMesh->GetBoneTransform(boneIndex);
    return true;
}
