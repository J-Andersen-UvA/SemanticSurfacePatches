#include "SurfacePatchAnchorComponentCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "SurfacePatchAnchorComponent.h"
#include "SurfacePatchBuilder.h"
#include "SurfacePatchesComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SurfacePatchAnchorComponentCustomization"

namespace
{
UBlueprint* getAnchorOwningBlueprint(const UObject* object)
{
    if (object == nullptr)
    {
        return nullptr;
    }

    const UBlueprintGeneratedClass* generatedClass =
        object->GetTypedOuter<UBlueprintGeneratedClass>();
    if (generatedClass != nullptr)
    {
        return Cast<UBlueprint>(generatedClass->ClassGeneratedBy);
    }

    if (const UActorComponent* actorComponent = Cast<UActorComponent>(object))
    {
        const AActor* owner = actorComponent->GetOwner();
        if (owner != nullptr)
        {
            return Cast<UBlueprint>(owner->GetClass()->ClassGeneratedBy);
        }
    }
    return object->GetTypedOuter<UBlueprint>();
}

USurfacePatchesComponent* findAnchorSurfacePatchesComponent(
    UBlueprint* blueprint)
{
    if (blueprint == nullptr || blueprint->SimpleConstructionScript == nullptr)
    {
        return nullptr;
    }

    for (USCS_Node* node : blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (USurfacePatchesComponent* component =
            Cast<USurfacePatchesComponent>(node->ComponentTemplate))
        {
            return component;
        }
    }
    return nullptr;
}
}

TSharedRef<IDetailCustomization>
FSurfacePatchAnchorComponentCustomization::makeInstance()
{
    return MakeShared<FSurfacePatchAnchorComponentCustomization>();
}

void FSurfacePatchAnchorComponentCustomization::CustomizeDetails(
    IDetailLayoutBuilder& detailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> objects;
    detailBuilder.GetObjectsBeingCustomized(objects);
    anchor = objects.IsEmpty()
        ? nullptr
        : Cast<USurfacePatchAnchorComponent>(objects[0].Get());

    IDetailCategoryBuilder& category =
        detailBuilder.EditCategory(TEXT("Surface Patch"));
    category.AddCustomRow(LOCTEXT("IndividualPatchTools", "Individual Patch Tools"))
        .WholeRowContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("RebuildThisPatch", "Rebuild This Patch"))
                .OnClicked(
                    this,
                    &FSurfacePatchAnchorComponentCustomization::rebuildPatch)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("RotateThisNormalOutwards", "Rotate Normal Outwards"))
                .OnClicked(
                    this,
                    &FSurfacePatchAnchorComponentCustomization::rotateNormalOutwards)
            ]
        ];
}

FReply FSurfacePatchAnchorComponentCustomization::rebuildPatch()
{
    USurfacePatchAnchorComponent* selectedAnchor = anchor.Get();
    USurfacePatchesComponent* component = findAnchorSurfacePatchesComponent(
        getAnchorOwningBlueprint(selectedAnchor));
    if (selectedAnchor == nullptr || component == nullptr)
    {
        return FReply::Handled();
    }

    const FScopedTransaction transaction(
        LOCTEXT("RebuildThisPatchTransaction", "Rebuild Surface Patch"));
    FText result;
    SurfacePatchBuilder::build(component, result, selectedAnchor->patchId);
    FMessageDialog::Open(EAppMsgType::Ok, result);
    return FReply::Handled();
}

FReply FSurfacePatchAnchorComponentCustomization::rotateNormalOutwards()
{
    USurfacePatchAnchorComponent* selectedAnchor = anchor.Get();
    USurfacePatchesComponent* component = findAnchorSurfacePatchesComponent(
        getAnchorOwningBlueprint(selectedAnchor));
    if (selectedAnchor == nullptr || component == nullptr)
    {
        return FReply::Handled();
    }

    const FScopedTransaction transaction(
        LOCTEXT("RotateThisNormalTransaction", "Rotate Surface Patch Normal Outwards"));
    FText result;
    SurfacePatchBuilder::rotateNormalsOutwards(
        component,
        result,
        selectedAnchor->patchId);
    FMessageDialog::Open(EAppMsgType::Ok, result);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
