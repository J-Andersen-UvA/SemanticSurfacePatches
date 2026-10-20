#include "SurfacePatchEditorRefresh.h"

#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SurfacePatchAnchorComponent.h"
#include "UObject/UObjectIterator.h"

namespace
{
bool belongsToBlueprint(const AActor* actor, const UBlueprint* blueprint)
{
    if (actor == nullptr || blueprint == nullptr)
    {
        return false;
    }

    for (const UClass* actorClass = actor->GetClass();
         actorClass != nullptr;
         actorClass = actorClass->GetSuperClass())
    {
        if (actorClass == blueprint->GeneratedClass ||
            actorClass->ClassGeneratedBy == blueprint)
        {
            return true;
        }
    }
    return false;
}

void copyAnchorState(
    const USurfacePatchAnchorComponent* source,
    USurfacePatchAnchorComponent* target)
{
    target->SetRelativeTransform(source->GetRelativeTransform());
    target->patchId = source->patchId;
    target->displayName = source->displayName;
    target->side = source->side;
    target->enabled = source->enabled;
    target->useSurfacePatch = source->useSurfacePatch;
    target->surfacePatchRadius = source->surfacePatchRadius;
    target->surfacePatchNormalThreshold =
        source->surfacePatchNormalThreshold;
    target->surfacePatchEdgeNormalThreshold =
        source->surfacePatchEdgeNormalThreshold;
    target->showSurfacePatch = source->showSurfacePatch;
    target->surfacePatch = source->surfacePatch;
    target->UpdateComponentToWorld();
    target->refreshSurfacePatchPreview();
    target->MarkRenderTransformDirty();
    target->MarkRenderStateDirty();
}
}

void SurfacePatchEditorRefresh::refreshBlueprintPreviewInstances(
    UBlueprint* blueprint)
{
    if (blueprint == nullptr || blueprint->SimpleConstructionScript == nullptr)
    {
        return;
    }

    TMap<FName, USurfacePatchAnchorComponent*> templatesById;
    for (USCS_Node* node : blueprint->SimpleConstructionScript->GetAllNodes())
    {
        USurfacePatchAnchorComponent* anchor =
            Cast<USurfacePatchAnchorComponent>(node->ComponentTemplate);
        if (anchor != nullptr && !anchor->patchId.IsNone())
        {
            templatesById.Add(anchor->patchId, anchor);
        }
    }

    for (TObjectIterator<USurfacePatchAnchorComponent> iterator;
         iterator;
         ++iterator)
    {
        USurfacePatchAnchorComponent* anchor = *iterator;
        AActor* owner = anchor != nullptr ? anchor->GetOwner() : nullptr;
        UWorld* world = owner != nullptr ? owner->GetWorld() : nullptr;
        if (world == nullptr ||
            world->WorldType != EWorldType::EditorPreview ||
            !belongsToBlueprint(owner, blueprint))
        {
            continue;
        }

        USurfacePatchAnchorComponent* const* source =
            templatesById.Find(anchor->patchId);
        if (source != nullptr && *source != nullptr)
        {
            copyAnchorState(*source, anchor);
        }
    }

    if (GEditor != nullptr)
    {
        GEditor->RedrawAllViewports(true);
    }
}

void SurfacePatchEditorRefresh::scheduleBlueprintPreviewRefresh(
    UBlueprint* blueprint)
{
    refreshBlueprintPreviewInstances(blueprint);

    const TWeakObjectPtr<UBlueprint> weakBlueprint(blueprint);
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [weakBlueprint](float)
            {
                if (UBlueprint* pendingBlueprint = weakBlueprint.Get())
                {
                    refreshBlueprintPreviewInstances(pendingBlueprint);
                }
                return false;
            }));
}
