#include "SurfacePatchesComponentCustomization.h"

#include "AssetRegistry/AssetData.h"
#include "Components/SkeletalMeshComponent.h"
#include "DesktopPlatformModule.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SimpleConstructionScript.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "PropertyCustomizationHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SurfacePatchesComponent.h"
#include "SurfacePatchAnchorComponent.h"
#include "SurfacePatchBuilder.h"
#include "SurfacePatchEditorRefresh.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SurfacePatchesComponentCustomization"

namespace
{
struct FMeshOption
{
    TWeakObjectPtr<USkeletalMeshComponent> mesh;
    TWeakObjectPtr<USCS_Node> node;
    FName componentName;
    FString label;
};

UBlueprint* getOwningBlueprint(const UObject* object)
{
    if (object == nullptr)
    {
        return nullptr;
    }

    const UBlueprintGeneratedClass* generatedClass =
        object->GetTypedOuter<UBlueprintGeneratedClass>();
    return generatedClass != nullptr
        ? Cast<UBlueprint>(generatedClass->ClassGeneratedBy)
        : object->GetTypedOuter<UBlueprint>();
}

bool readPresetVector(
    const TSharedPtr<FJsonObject>& object,
    const TCHAR* fieldName,
    FVector& value)
{
    const TSharedPtr<FJsonObject>* vectorObject = nullptr;
    if (!object.IsValid() ||
        !object->TryGetObjectField(fieldName, vectorObject) ||
        vectorObject == nullptr ||
        !vectorObject->IsValid())
    {
        return false;
    }

    return (*vectorObject)->TryGetNumberField(TEXT("x"), value.X) &&
        (*vectorObject)->TryGetNumberField(TEXT("y"), value.Y) &&
        (*vectorObject)->TryGetNumberField(TEXT("z"), value.Z);
}

ESurfacePatchSide parseSide(const FString& side)
{
    if (side.Equals(TEXT("left"), ESearchCase::IgnoreCase))
    {
        return ESurfacePatchSide::Left;
    }
    if (side.Equals(TEXT("right"), ESearchCase::IgnoreCase))
    {
        return ESurfacePatchSide::Right;
    }
    return ESurfacePatchSide::Center;
}

bool readPreset(
    const FString& filePath,
    TArray<FSurfacePatchAnchor>& outPatches,
    FString& outError)
{
    FString jsonText;
    if (!FFileHelper::LoadFileToString(jsonText, *filePath))
    {
        outError = FString::Printf(TEXT("Could not read %s"), *filePath);
        return false;
    }

    TSharedPtr<FJsonObject> root;
    const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonText);
    if (!FJsonSerializer::Deserialize(reader, root) || !root.IsValid())
    {
        outError = TEXT("The selected file is not valid JSON.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* patchValues = nullptr;
    if (!root->TryGetArrayField(TEXT("patches"), patchValues) ||
        patchValues == nullptr)
    {
        outError = TEXT("The preset does not contain a patches array.");
        return false;
    }

    TSet<FName> patchIds;
    TArray<FSurfacePatchAnchor> parsedPatches;
    for (const TSharedPtr<FJsonValue>& patchValue : *patchValues)
    {
        const TSharedPtr<FJsonObject> patchObject =
            patchValue.IsValid() ? patchValue->AsObject() : nullptr;
        FString patchId;
        FString displayName;
        FString side;
        FVector location;
        FVector normal = FVector::ForwardVector;
        if (!patchObject.IsValid() ||
            !patchObject->TryGetStringField(TEXT("patchId"), patchId) ||
            patchId.IsEmpty() ||
            !readPresetVector(patchObject, TEXT("location"), location))
        {
            outError = TEXT("Every patch needs a patchId and location.");
            return false;
        }

        const FName patchName(*patchId);
        if (patchIds.Contains(patchName))
        {
            outError = FString::Printf(
                TEXT("The patchId '%s' occurs more than once."),
                *patchId);
            return false;
        }
        patchIds.Add(patchName);

        patchObject->TryGetStringField(TEXT("displayName"), displayName);
        patchObject->TryGetStringField(TEXT("side"), side);
        readPresetVector(patchObject, TEXT("normal"), normal);

        FSurfacePatchAnchor patch;
        patch.patchId = patchName;
        patch.displayName = FText::FromString(
            displayName.IsEmpty() ? patchId : displayName);
        patch.side = parseSide(side);
        patchObject->TryGetBoolField(
            TEXT("useSurfacePatch"),
            patch.useSurfacePatch);
        patchObject->TryGetNumberField(
            TEXT("surfacePatchRadius"),
            patch.surfacePatchRadius);
        patchObject->TryGetNumberField(
            TEXT("surfacePatchNormalThreshold"),
            patch.surfacePatchNormalThreshold);
        patchObject->TryGetNumberField(
            TEXT("surfacePatchEdgeNormalThreshold"),
            patch.surfacePatchEdgeNormalThreshold);
        patchObject->TryGetBoolField(
            TEXT("showSurfacePatch"),
            patch.showSurfacePatch);
        patch.boneLocalTransform = FTransform(
            FQuat::FindBetweenNormals(
                FVector::ForwardVector,
                normal.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector)),
            location);
        parsedPatches.Add(MoveTemp(patch));
    }

    outPatches = MoveTemp(parsedPatches);
    return true;
}

void collectMeshOptions(
    USurfacePatchesComponent* component,
    TArray<TSharedPtr<FMeshOption>>& outOptions)
{
    if (component == nullptr)
    {
        return;
    }

    TArray<USkeletalMeshComponent*> meshes;
    TMap<USkeletalMeshComponent*, FName> componentNames;
    if (component->GetOwner() != nullptr)
    {
        component->GetOwner()->GetComponents(meshes);
        for (USkeletalMeshComponent* mesh : meshes)
        {
            FString stableName = mesh->GetName();
            stableName.RemoveFromEnd(TEXT("_GEN_VARIABLE"));
            componentNames.Add(mesh, FName(*stableName));
        }
    }

    UBlueprintGeneratedClass* generatedClass =
        component->GetTypedOuter<UBlueprintGeneratedClass>();
    UBlueprint* blueprint = generatedClass != nullptr
        ? Cast<UBlueprint>(generatedClass->ClassGeneratedBy)
        : component->GetTypedOuter<UBlueprint>();
    if (blueprint != nullptr && blueprint->SimpleConstructionScript != nullptr)
    {
        for (USCS_Node* node :
             blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (USkeletalMeshComponent* mesh =
                Cast<USkeletalMeshComponent>(node->ComponentTemplate))
            {
                meshes.AddUnique(mesh);
                componentNames.Add(mesh, node->GetVariableName());
            }
        }
    }

    for (USkeletalMeshComponent* mesh : meshes)
    {
        TSharedPtr<FMeshOption> option = MakeShared<FMeshOption>();
        option->mesh = mesh;
        option->componentName = componentNames.FindRef(mesh);
        option->label = option->componentName.ToString();
        if (blueprint != nullptr && blueprint->SimpleConstructionScript != nullptr)
        {
            option->node = blueprint->SimpleConstructionScript->FindSCSNode(
                option->componentName);
        }
        outOptions.Add(option);
    }
}

TArray<FSurfacePatchAnchor> getBlueprintPatches(UBlueprint* blueprint)
{
    TArray<FSurfacePatchAnchor> patches;
    if (blueprint == nullptr || blueprint->SimpleConstructionScript == nullptr)
    {
        return patches;
    }

    for (USCS_Node* node : blueprint->SimpleConstructionScript->GetAllNodes())
    {
        const USurfacePatchAnchorComponent* anchor =
            Cast<USurfacePatchAnchorComponent>(node->ComponentTemplate);
        if (anchor == nullptr)
        {
            continue;
        }

        FSurfacePatchAnchor patch = anchor->toPatchData();
        patch.parentBone = node->AttachToName;
        patches.Add(MoveTemp(patch));
    }
    return patches;
}

USkeletalMeshComponent* findCopyMeshTemplate(
    UBlueprint* blueprint,
    const FName componentName)
{
    if (blueprint == nullptr || blueprint->SimpleConstructionScript == nullptr)
    {
        return nullptr;
    }

    USCS_Node* node = blueprint->SimpleConstructionScript->FindSCSNode(
        componentName);
    return node != nullptr
        ? Cast<USkeletalMeshComponent>(node->ComponentTemplate)
        : nullptr;
}

TArray<FTransform> getCopyReferenceBoneTransforms(
    const FReferenceSkeleton& skeleton)
{
    const TArray<FTransform>& localTransforms = skeleton.GetRefBonePose();
    TArray<FTransform> componentTransforms;
    componentTransforms.SetNum(localTransforms.Num());
    for (int32 boneIndex = 0; boneIndex < localTransforms.Num(); ++boneIndex)
    {
        componentTransforms[boneIndex] = localTransforms[boneIndex];
        const int32 parentIndex = skeleton.GetParentIndex(boneIndex);
        if (parentIndex != INDEX_NONE)
        {
            componentTransforms[boneIndex] *= componentTransforms[parentIndex];
        }
    }
    return componentTransforms;
}

bool convertCopiedPatchBoneSpace(
    FSurfacePatchAnchor& patch,
    const FName sourceBone,
    const FName targetBone,
    const USkeletalMesh* sourceMesh,
    const USkeletalMesh* targetMesh)
{
    if (sourceMesh == nullptr || targetMesh == nullptr)
    {
        return false;
    }

    const FReferenceSkeleton& sourceSkeleton = sourceMesh->GetRefSkeleton();
    const FReferenceSkeleton& targetSkeleton = targetMesh->GetRefSkeleton();
    const int32 sourceBoneIndex = sourceSkeleton.FindBoneIndex(sourceBone);
    const int32 targetBoneIndex = targetSkeleton.FindBoneIndex(targetBone);
    if (sourceBoneIndex == INDEX_NONE || targetBoneIndex == INDEX_NONE)
    {
        return false;
    }

    const TArray<FTransform> sourceBoneTransforms =
        getCopyReferenceBoneTransforms(sourceSkeleton);
    const TArray<FTransform> targetBoneTransforms =
        getCopyReferenceBoneTransforms(targetSkeleton);
    if (!sourceBoneTransforms.IsValidIndex(sourceBoneIndex) ||
        !targetBoneTransforms.IsValidIndex(targetBoneIndex))
    {
        return false;
    }

    const FTransform& sourceBoneTransform =
        sourceBoneTransforms[sourceBoneIndex];
    const FTransform& targetBoneTransform =
        targetBoneTransforms[targetBoneIndex];
    const FTransform sourceLocalTransform = patch.boneLocalTransform;

    const FVector sourceMeshVector = sourceBoneTransform
        .TransformVectorNoScale(sourceLocalTransform.GetLocation());
    const FVector targetLocalLocation = targetBoneTransform
        .InverseTransformVectorNoScale(sourceMeshVector);
    FQuat targetLocalRotation = targetBoneTransform.GetRotation().Inverse() *
        sourceBoneTransform.GetRotation() *
        sourceLocalTransform.GetRotation();
    targetLocalRotation.Normalize();

    patch.boneLocalTransform = FTransform(
        targetLocalRotation,
        targetLocalLocation,
        sourceLocalTransform.GetScale3D());
    return true;
}

void synchronizeBlueprintPatches(USurfacePatchesComponent* component)
{
    UBlueprint* blueprint = getOwningBlueprint(component);
    if (component == nullptr ||
        blueprint == nullptr ||
        blueprint->SimpleConstructionScript == nullptr)
    {
        return;
    }

    TArray<USurfacePatchAnchorComponent*> anchors;
    bool categoryChanged = false;
    for (USCS_Node* node : blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (USurfacePatchAnchorComponent* anchor =
            Cast<USurfacePatchAnchorComponent>(node->ComponentTemplate))
        {
            anchors.Add(anchor);
            const FText category = LOCTEXT(
                "SurfacePatchComponentCategory",
                "Surface Patches");
            if (!node->CategoryName.EqualTo(category))
            {
                node->CategoryName = category;
                categoryChanged = true;
            }
        }
        else if (Cast<USurfacePatchesComponent>(node->ComponentTemplate) != nullptr)
        {
            const FText category = LOCTEXT(
                "SurfacePatchesComponentCategory",
                "Surface Patches");
            if (!node->CategoryName.EqualTo(category))
            {
                node->CategoryName = category;
                categoryChanged = true;
            }
        }
    }

    bool referencesChanged = component->surfacePatches.Num() != anchors.Num();
    if (!referencesChanged)
    {
        for (int32 index = 0; index < anchors.Num(); ++index)
        {
            if (component->surfacePatches[index] != anchors[index])
            {
                referencesChanged = true;
                break;
            }
        }
    }

    if (referencesChanged)
    {
        component->Modify();
        component->surfacePatches.Reset(anchors.Num());
        for (USurfacePatchAnchorComponent* anchor : anchors)
        {
            component->surfacePatches.Add(anchor);
        }
        component->PostEditChange();
        component->MarkPackageDirty();
    }

    if (referencesChanged || categoryChanged)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(blueprint);
    }
}

bool replaceBlueprintPatches(
    USurfacePatchesComponent* component,
    USkeletalMeshComponent* targetMesh,
    const FName targetMeshComponentName,
    const FName fallbackBone,
    const TArray<FSurfacePatchAnchor>& sourcePatches)
{
    UBlueprint* blueprint = getOwningBlueprint(component);
    if (blueprint == nullptr || blueprint->SimpleConstructionScript == nullptr)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            LOCTEXT(
                "MissingOwningBlueprint",
                "Surface patch anchors can only be authored on a Blueprint component."));
        return false;
    }

    USimpleConstructionScript* constructionScript =
        blueprint->SimpleConstructionScript;
    USCS_Node* meshNode = constructionScript->FindSCSNode(
        targetMeshComponentName);
    if (meshNode == nullptr)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            LOCTEXT(
                "MissingMeshNode",
                "The selected skeletal mesh component is not an editable component in this Blueprint."));
        return false;
    }

    blueprint->Modify();
    constructionScript->Modify();
    component->Modify();
    component->surfacePatches.Reset();

    TArray<USCS_Node*> existingNodes;
    for (USCS_Node* node : constructionScript->GetAllNodes())
    {
        if (Cast<USurfacePatchAnchorComponent>(node->ComponentTemplate) != nullptr)
        {
            existingNodes.Add(node);
        }
    }
    for (USCS_Node* node : existingNodes)
    {
        constructionScript->RemoveNode(node, false);
    }

    for (const FSurfacePatchAnchor& patch : sourcePatches)
    {
        FString variableName = FString::Printf(
            TEXT("SurfacePatch_%s"),
            *patch.patchId.ToString());
        variableName.ReplaceInline(TEXT("."), TEXT("_"));
        variableName.ReplaceInline(TEXT(" "), TEXT("_"));

        USCS_Node* anchorNode = constructionScript->CreateNode(
            USurfacePatchAnchorComponent::StaticClass(),
            FName(*variableName));
        meshNode->AddChildNode(anchorNode);

        USurfacePatchAnchorComponent* anchor =
            CastChecked<USurfacePatchAnchorComponent>(
                anchorNode->ComponentTemplate);
        anchor->patchId = patch.patchId;
        anchor->displayName = patch.displayName;
        anchor->side = patch.side;
        anchor->enabled = patch.enabled;
        anchor->useSurfacePatch = patch.useSurfacePatch;
        anchor->surfacePatchRadius = patch.surfacePatchRadius;
        anchor->surfacePatchNormalThreshold =
            patch.surfacePatchNormalThreshold;
        anchor->surfacePatchEdgeNormalThreshold =
            patch.surfacePatchEdgeNormalThreshold;
        anchor->showSurfacePatch = patch.showSurfacePatch;
        anchor->surfacePatch = patch.surfacePatch;
        anchor->SetRelativeTransform(patch.boneLocalTransform);

        const FName requestedBone = patch.parentBone.IsNone()
            ? fallbackBone
            : patch.parentBone;
        anchorNode->AttachToName =
            targetMesh != nullptr &&
            targetMesh->GetBoneIndex(requestedBone) != INDEX_NONE
                ? requestedBone
                : fallbackBone;
        component->surfacePatches.Add(anchor);
    }

    synchronizeBlueprintPatches(component);

    component->targetMeshComponentName = targetMeshComponentName;
    component->defaultParentBone = fallbackBone;
    component->PostEditChange();
    component->MarkPackageDirty();
    constructionScript->ValidateSceneRootNodes();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(blueprint);
    FKismetEditorUtilities::CompileBlueprint(blueprint);
    SurfacePatchEditorRefresh::scheduleBlueprintPreviewRefresh(blueprint);
    return true;
}

USurfacePatchesComponent* findSurfacePatchesTemplate(UBlueprint* blueprint)
{
    if (blueprint == nullptr)
    {
        return nullptr;
    }

    if (blueprint->SimpleConstructionScript != nullptr)
    {
        for (USCS_Node* node :
             blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (USurfacePatchesComponent* component =
                Cast<USurfacePatchesComponent>(node->ComponentTemplate))
            {
                return component;
            }
        }
    }

    AActor* defaultActor = blueprint->GeneratedClass != nullptr
        ? Cast<AActor>(blueprint->GeneratedClass->GetDefaultObject())
        : nullptr;
    return defaultActor != nullptr
        ? defaultActor->FindComponentByClass<USurfacePatchesComponent>()
        : nullptr;
}

bool showMeshAndBoneDialog(
    USurfacePatchesComponent* component,
    USkeletalMeshComponent*& outMesh,
    FName& outMeshComponentName,
    FName& outBone)
{
    TArray<TSharedPtr<FMeshOption>> meshOptions;
    collectMeshOptions(component, meshOptions);
    if (meshOptions.IsEmpty())
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            LOCTEXT(
                "NoSkeletalMesh",
                "The owning actor does not contain a skeletal mesh component."));
        return false;
    }

    TSharedPtr<FMeshOption> selectedMesh = meshOptions[0];
    for (const TSharedPtr<FMeshOption>& option : meshOptions)
    {
        if (option->componentName == component->targetMeshComponentName)
        {
            selectedMesh = option;
            break;
        }
    }

    bool accepted = false;
    TSharedPtr<SEditableTextBox> boneTextBox;
    TSharedRef<SWindow> window = SNew(SWindow)
        .Title(LOCTEXT("LoadPresetTitle", "Load Surface Patch Preset"))
        .ClientSize(FVector2D(460.0, 180.0))
        .SupportsMaximize(false)
        .SupportsMinimize(false);

    window->SetContent(
        SNew(SBorder)
        .Padding(12.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock).Text(LOCTEXT("MeshLabel", "Skeletal mesh component"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SComboBox<TSharedPtr<FMeshOption>>)
                .OptionsSource(&meshOptions)
                .InitiallySelectedItem(selectedMesh)
                .OnGenerateWidget_Lambda(
                    [](const TSharedPtr<FMeshOption>& option)
                    {
                        return SNew(STextBlock)
                            .Text(FText::FromString(option->label));
                    })
                .OnSelectionChanged_Lambda(
                    [&selectedMesh](
                        const TSharedPtr<FMeshOption>& option,
                        ESelectInfo::Type)
                    {
                        selectedMesh = option;
                    })
                [
                    SNew(STextBlock)
                    .Text_Lambda(
                        [&selectedMesh]()
                        {
                            return selectedMesh.IsValid()
                                ? FText::FromString(selectedMesh->label)
                                : FText::GetEmpty();
                        })
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock).Text(LOCTEXT("BoneLabel", "Parent bone"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                SAssignNew(boneTextBox, SEditableTextBox)
                .Text(FText::FromName(component->defaultParentBone))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Cancel", "Cancel"))
                    .OnClicked_Lambda(
                        [window]()
                        {
                            window->RequestDestroyWindow();
                            return FReply::Handled();
                        })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Load", "Load"))
                    .OnClicked_Lambda(
                        [&accepted, &selectedMesh, &boneTextBox, window]()
                        {
                            USkeletalMeshComponent* mesh = selectedMesh.IsValid()
                                ? selectedMesh->mesh.Get()
                                : nullptr;
                            const FName boneName(*boneTextBox->GetText().ToString());
                            if (mesh == nullptr || mesh->GetBoneIndex(boneName) == INDEX_NONE)
                            {
                                FMessageDialog::Open(
                                    EAppMsgType::Ok,
                                    LOCTEXT(
                                        "InvalidBone",
                                        "The selected skeletal mesh does not contain that bone."));
                                return FReply::Handled();
                            }

                            accepted = true;
                            window->RequestDestroyWindow();
                            return FReply::Handled();
                        })
                ]
            ]
        ]);

    GEditor->EditorAddModalWindow(window);
    if (!accepted || !selectedMesh.IsValid())
    {
        return false;
    }

    outMesh = selectedMesh->mesh.Get();
    outMeshComponentName = selectedMesh->componentName;
    outBone = FName(*boneTextBox->GetText().ToString());
    return outMesh != nullptr;
}

bool showCopyDialog(
    USurfacePatchesComponent* destination,
    USurfacePatchesComponent*& outSource,
    USkeletalMeshComponent*& outMesh,
    FName& outMeshComponentName,
    FName& outFallbackBone)
{
    TArray<TSharedPtr<FMeshOption>> meshOptions;
    collectMeshOptions(destination, meshOptions);
    if (meshOptions.IsEmpty())
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            LOCTEXT(
                "NoDestinationMesh",
                "The destination actor does not contain a skeletal mesh component."));
        return false;
    }

    FAssetData selectedBlueprint;
    TSharedPtr<FMeshOption> selectedMesh = meshOptions[0];
    bool accepted = false;
    TSharedPtr<SEditableTextBox> boneTextBox;
    TSharedRef<SWindow> window = SNew(SWindow)
        .Title(LOCTEXT("CopyTitle", "Copy Surface Patches From Actor Blueprint"))
        .ClientSize(FVector2D(500.0, 250.0))
        .SupportsMaximize(false)
        .SupportsMinimize(false);

    window->SetContent(
        SNew(SBorder)
        .Padding(12.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [ SNew(STextBlock).Text(LOCTEXT("SourceActor", "Source actor Blueprint")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SObjectPropertyEntryBox)
                .AllowedClass(UBlueprint::StaticClass())
                .OnObjectChanged_Lambda(
                    [&selectedBlueprint](const FAssetData& assetData)
                    {
                        selectedBlueprint = assetData;
                    })
                .ObjectPath_Lambda(
                    [&selectedBlueprint]()
                    {
                        return selectedBlueprint.IsValid()
                            ? selectedBlueprint.GetSoftObjectPath().ToString()
                            : FString();
                    })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [ SNew(STextBlock).Text(LOCTEXT("DestinationMesh", "Destination skeletal mesh component")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SComboBox<TSharedPtr<FMeshOption>>)
                .OptionsSource(&meshOptions)
                .InitiallySelectedItem(selectedMesh)
                .OnGenerateWidget_Lambda(
                    [](const TSharedPtr<FMeshOption>& option)
                    {
                        return SNew(STextBlock).Text(FText::FromString(option->label));
                    })
                .OnSelectionChanged_Lambda(
                    [&selectedMesh](const TSharedPtr<FMeshOption>& option, ESelectInfo::Type)
                    {
                        selectedMesh = option;
                    })
                [
                    SNew(STextBlock).Text_Lambda(
                        [&selectedMesh]()
                        {
                            return FText::FromString(selectedMesh->label);
                        })
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [ SNew(STextBlock).Text(LOCTEXT("FallbackBone", "Replacement bone when a copied bone is missing")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                SAssignNew(boneTextBox, SEditableTextBox)
                .Text(FText::FromName(destination->defaultParentBone))
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CopyCancel", "Cancel"))
                    .OnClicked_Lambda(
                        [window]()
                        {
                            window->RequestDestroyWindow();
                            return FReply::Handled();
                        })
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Copy", "Copy"))
                    .OnClicked_Lambda(
                        [&accepted, &selectedBlueprint, &selectedMesh, &boneTextBox, window]()
                        {
                            UBlueprint* blueprint = selectedBlueprint.IsValid()
                                ? Cast<UBlueprint>(selectedBlueprint.GetAsset())
                                : nullptr;
                            USurfacePatchesComponent* sourceComponent =
                                findSurfacePatchesTemplate(blueprint);
                            if (sourceComponent == nullptr)
                            {
                                FMessageDialog::Open(
                                    EAppMsgType::Ok,
                                    LOCTEXT(
                                        "InvalidSourceBlueprint",
                                        "Select an actor Blueprint that contains a Surface Patches component."));
                                return FReply::Handled();
                            }

                            USkeletalMeshComponent* mesh = selectedMesh->mesh.Get();
                            const FName boneName(*boneTextBox->GetText().ToString());
                            if (mesh == nullptr || mesh->GetBoneIndex(boneName) == INDEX_NONE)
                            {
                                FMessageDialog::Open(
                                    EAppMsgType::Ok,
                                    LOCTEXT(
                                        "InvalidFallbackBone",
                                        "The destination skeletal mesh does not contain the replacement bone."));
                                return FReply::Handled();
                            }

                            accepted = true;
                            window->RequestDestroyWindow();
                            return FReply::Handled();
                        })
                ]
            ]
        ]);

    GEditor->EditorAddModalWindow(window);
    if (!accepted || !selectedBlueprint.IsValid() || !selectedMesh.IsValid())
    {
        return false;
    }

    UBlueprint* blueprint = Cast<UBlueprint>(selectedBlueprint.GetAsset());
    outSource = findSurfacePatchesTemplate(blueprint);
    outMesh = selectedMesh->mesh.Get();
    outMeshComponentName = selectedMesh->componentName;
    outFallbackBone = FName(*boneTextBox->GetText().ToString());
    return outSource != nullptr && outMesh != nullptr;
}

bool showMirrorAxisDialog(EAxis::Type& outAxis)
{
    TArray<TSharedPtr<FString>> axisOptions = {
        MakeShared<FString>(TEXT("X")),
        MakeShared<FString>(TEXT("Y")),
        MakeShared<FString>(TEXT("Z"))};
    TSharedPtr<FString> selectedAxis = axisOptions[1];
    bool accepted = false;
    TSharedRef<SWindow> window = SNew(SWindow)
        .Title(LOCTEXT("MirrorAxisTitle", "Mirror Surface Patches"))
        .ClientSize(FVector2D(360.0, 135.0))
        .SupportsMaximize(false)
        .SupportsMinimize(false);

    window->SetContent(
        SNew(SBorder)
        .Padding(12.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "MirrorAxisLabel",
                    "Reference-mesh axis whose coordinate should be negated"))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                SNew(SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&axisOptions)
                .InitiallySelectedItem(selectedAxis)
                .OnGenerateWidget_Lambda(
                    [](const TSharedPtr<FString>& option)
                    {
                        return SNew(STextBlock)
                            .Text(FText::FromString(*option));
                    })
                .OnSelectionChanged_Lambda(
                    [&selectedAxis](
                        const TSharedPtr<FString>& option,
                        ESelectInfo::Type)
                    {
                        selectedAxis = option;
                    })
                [
                    SNew(STextBlock)
                    .Text_Lambda(
                        [&selectedAxis]()
                        {
                            return selectedAxis.IsValid()
                                ? FText::FromString(*selectedAxis)
                                : FText::GetEmpty();
                        })
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CancelMirror", "Cancel"))
                    .OnClicked_Lambda(
                        [window]()
                        {
                            window->RequestDestroyWindow();
                            return FReply::Handled();
                        })
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ApplyMirror", "Mirror"))
                    .OnClicked_Lambda(
                        [&accepted, window]()
                        {
                            accepted = true;
                            window->RequestDestroyWindow();
                            return FReply::Handled();
                        })
                ]
            ]
        ]);

    GEditor->EditorAddModalWindow(window);
    if (!accepted || !selectedAxis.IsValid())
    {
        return false;
    }

    outAxis = *selectedAxis == TEXT("X")
        ? EAxis::X
        : *selectedAxis == TEXT("Z")
            ? EAxis::Z
            : EAxis::Y;
    return true;
}
}

TSharedRef<IDetailCustomization> FSurfacePatchesComponentCustomization::makeInstance()
{
    return MakeShared<FSurfacePatchesComponentCustomization>();
}

void FSurfacePatchesComponentCustomization::CustomizeDetails(
    IDetailLayoutBuilder& detailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> objects;
    detailBuilder.GetObjectsBeingCustomized(objects);
    component = objects.IsEmpty()
        ? nullptr
        : Cast<USurfacePatchesComponent>(objects[0].Get());

    synchronizeBlueprintPatches(component.Get());

    IDetailCategoryBuilder& category =
        detailBuilder.EditCategory(TEXT("Surface Patches"));
    category.AddCustomRow(LOCTEXT("AuthoringSearch", "Surface Patch Authoring"))
        .WholeRowContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("LoadPreset", "Load Preset..."))
                .OnClicked(this, &FSurfacePatchesComponentCustomization::loadPreset)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("CopyFromActor", "Copy Patches From Actor Blueprint..."))
                .OnClicked(this, &FSurfacePatchesComponentCustomization::copyFromActor)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("BuildSurfacePatches", "Build Surface Patches"))
                .ToolTipText(LOCTEXT(
                    "BuildSurfacePatchesTooltip",
                    "Capture connected nearby LOD 0 triangles for every enabled surface patch anchor."))
                .OnClicked(
                    this,
                    &FSurfacePatchesComponentCustomization::buildSurfacePatches)
            ]
        ];

    category.AddCustomRow(LOCTEXT("TransformSearch", "Surface Patch Transform Tools"))
        .WholeRowContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("MirrorRightToLeft", "Mirror Right to Left"))
                .ToolTipText(LOCTEXT(
                    "MirrorRightToLeftTooltip",
                    "Copy each right anchor transform and patch settings to its left partner by reflecting across skeletal-mesh reference-space Y=0. Built triangles on changed left anchors are cleared."))
                .OnClicked(
                    this,
                    &FSurfacePatchesComponentCustomization::mirrorRightToLeft)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .Text(LOCTEXT("RotateNormalsOutwards", "Rotate Normals Outwards"))
                .ToolTipText(LOCTEXT(
                    "RotateNormalsOutwardsTooltip",
                    "Aim every anchor's local X axis along the nearest outward LOD 0 surface normal. Existing built triangles are cleared."))
                .OnClicked(
                    this,
                    &FSurfacePatchesComponentCustomization::rotateNormalsOutwards)
            ]
        ];
}

FReply FSurfacePatchesComponentCustomization::buildSurfacePatches()
{
    USurfacePatchesComponent* surfacePatches = component.Get();
    if (surfacePatches == nullptr)
    {
        return FReply::Handled();
    }

    const FScopedTransaction transaction(
        LOCTEXT("BuildSurfacePatchesTransaction", "Build Surface Patches"));
    FText result;
    SurfacePatchBuilder::build(surfacePatches, result);
    FMessageDialog::Open(EAppMsgType::Ok, result);
    return FReply::Handled();
}

FReply FSurfacePatchesComponentCustomization::mirrorRightToLeft()
{
    USurfacePatchesComponent* surfacePatches = component.Get();
    UBlueprint* blueprint = getOwningBlueprint(surfacePatches);
    if (surfacePatches == nullptr || blueprint == nullptr)
    {
        return FReply::Handled();
    }

    EAxis::Type mirrorAxis = EAxis::Y;
    if (!showMirrorAxisDialog(mirrorAxis))
    {
        return FReply::Handled();
    }

    TArray<USurfacePatchAnchorComponent*> blueprintAnchors;
    TMap<FName, USurfacePatchAnchorComponent*> anchorsById;
    TMap<FName, USCS_Node*> anchorNodesById;
    for (USCS_Node* node : blueprint->SimpleConstructionScript->GetAllNodes())
    {
        USurfacePatchAnchorComponent* anchor =
            Cast<USurfacePatchAnchorComponent>(node->ComponentTemplate);
        if (anchor != nullptr)
        {
            blueprintAnchors.Add(anchor);
            anchorsById.Add(anchor->patchId, anchor);
            anchorNodesById.Add(anchor->patchId, node);
        }
    }

    USkeletalMeshComponent* meshComponent = findCopyMeshTemplate(
        blueprint,
        surfacePatches->targetMeshComponentName);
    USkeletalMesh* mesh = meshComponent != nullptr
        ? meshComponent->GetSkeletalMeshAsset()
        : nullptr;
    if (mesh == nullptr)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            LOCTEXT(
                "MirrorNeedsMesh",
                "Select a target skeletal mesh before mirroring surface patches."));
        return FReply::Handled();
    }

    const FReferenceSkeleton& skeleton = mesh->GetRefSkeleton();
    const TArray<FTransform> boneTransforms =
        getCopyReferenceBoneTransforms(skeleton);

    const FScopedTransaction transaction(
        LOCTEXT("MirrorRightToLeftTransaction", "Mirror Surface Patches Right to Left"));
    blueprint->Modify();
    int32 mirroredCount = 0;
    for (USurfacePatchAnchorComponent* rightAnchor : blueprintAnchors)
    {
        if (rightAnchor == nullptr ||
            rightAnchor->side != ESurfacePatchSide::Right)
        {
            continue;
        }

        FString rightId = rightAnchor->patchId.ToString();
        if (!rightId.StartsWith(TEXT("right"), ESearchCase::IgnoreCase))
        {
            continue;
        }
        const FName leftId(*FString::Printf(
            TEXT("left%s"),
            *rightId.RightChop(5)));
        USurfacePatchAnchorComponent* const* leftAnchorEntry =
            anchorsById.Find(leftId);
        USCS_Node* const* rightNodeEntry =
            anchorNodesById.Find(rightAnchor->patchId);
        USCS_Node* const* leftNodeEntry = anchorNodesById.Find(leftId);
        if (leftAnchorEntry == nullptr ||
            *leftAnchorEntry == nullptr ||
            rightNodeEntry == nullptr ||
            *rightNodeEntry == nullptr ||
            leftNodeEntry == nullptr ||
            *leftNodeEntry == nullptr)
        {
            continue;
        }

        USurfacePatchAnchorComponent* leftAnchor = *leftAnchorEntry;
        const FName rightBoneName = (*rightNodeEntry)->AttachToName.IsNone()
            ? surfacePatches->defaultParentBone
            : (*rightNodeEntry)->AttachToName;
        const FName leftBoneName = (*leftNodeEntry)->AttachToName.IsNone()
            ? surfacePatches->defaultParentBone
            : (*leftNodeEntry)->AttachToName;
        const int32 rightBoneIndex = skeleton.FindBoneIndex(rightBoneName);
        const int32 leftBoneIndex = skeleton.FindBoneIndex(leftBoneName);
        if (!boneTransforms.IsValidIndex(rightBoneIndex) ||
            !boneTransforms.IsValidIndex(leftBoneIndex))
        {
            continue;
        }

        leftAnchor->Modify();

        const FTransform& rightBoneTransform =
            boneTransforms[rightBoneIndex];
        const FTransform& leftBoneTransform =
            boneTransforms[leftBoneIndex];
        const FQuat rightLocalRotation = rightAnchor->GetRelativeRotation()
            .Quaternion();

        FVector mirroredMeshLocation = rightBoneTransform.TransformPosition(
            rightAnchor->GetRelativeLocation());
        FVector mirroredMeshForward = rightBoneTransform
            .TransformVectorNoScale(
                rightLocalRotation.RotateVector(FVector::ForwardVector));
        FVector mirroredMeshUp = rightBoneTransform.TransformVectorNoScale(
            rightLocalRotation.RotateVector(FVector::UpVector));
        auto mirrorVector = [mirrorAxis](FVector value)
        {
            switch (mirrorAxis)
            {
            case EAxis::X:
                value.X *= -1.0;
                break;
            case EAxis::Z:
                value.Z *= -1.0;
                break;
            case EAxis::Y:
            default:
                value.Y *= -1.0;
                break;
            }
            return value;
        };
        mirroredMeshLocation = mirrorVector(mirroredMeshLocation);
        mirroredMeshForward = mirrorVector(mirroredMeshForward);
        mirroredMeshUp = mirrorVector(mirroredMeshUp);

        const FVector mirroredLocation = leftBoneTransform
            .InverseTransformPosition(mirroredMeshLocation);
        const FVector mirroredForward = leftBoneTransform
            .InverseTransformVectorNoScale(mirroredMeshForward)
            .GetSafeNormal();
        const FVector mirroredUp = leftBoneTransform
            .InverseTransformVectorNoScale(mirroredMeshUp)
            .GetSafeNormal();
        const FQuat mirroredRotation = FRotationMatrix::MakeFromXZ(
            mirroredForward,
            mirroredUp).ToQuat();

        leftAnchor->SetRelativeLocation(mirroredLocation);
        leftAnchor->SetRelativeRotation(mirroredRotation);
        leftAnchor->SetRelativeScale3D(rightAnchor->GetRelativeScale3D());
        leftAnchor->enabled = rightAnchor->enabled;
        leftAnchor->useSurfacePatch = rightAnchor->useSurfacePatch;
        leftAnchor->surfacePatchRadius = rightAnchor->surfacePatchRadius;
        leftAnchor->surfacePatchNormalThreshold =
            rightAnchor->surfacePatchNormalThreshold;
        leftAnchor->surfacePatchEdgeNormalThreshold =
            rightAnchor->surfacePatchEdgeNormalThreshold;
        leftAnchor->showSurfacePatch = rightAnchor->showSurfacePatch;
        leftAnchor->surfacePatch.Reset();
        leftAnchor->refreshSurfacePatchPreview();
        ++mirroredCount;
    }

    if (mirroredCount > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(blueprint);
        FKismetEditorUtilities::CompileBlueprint(blueprint);
        SurfacePatchEditorRefresh::scheduleBlueprintPreviewRefresh(blueprint);
    }
    FMessageDialog::Open(
        EAppMsgType::Ok,
        FText::Format(
            LOCTEXT("MirroredCount", "Mirrored {0} right anchor(s) to the left."),
            FText::AsNumber(mirroredCount)));
    return FReply::Handled();
}

FReply FSurfacePatchesComponentCustomization::rotateNormalsOutwards()
{
    USurfacePatchesComponent* surfacePatches = component.Get();
    if (surfacePatches == nullptr)
    {
        return FReply::Handled();
    }

    const FScopedTransaction transaction(
        LOCTEXT("RotateNormalsOutwardsTransaction", "Rotate Surface Patch Normals Outwards"));
    FText result;
    SurfacePatchBuilder::rotateNormalsOutwards(surfacePatches, result);
    FMessageDialog::Open(EAppMsgType::Ok, result);
    return FReply::Handled();
}

FReply FSurfacePatchesComponentCustomization::loadPreset()
{
    USurfacePatchesComponent* surfacePatches = component.Get();
    if (surfacePatches == nullptr)
    {
        return FReply::Handled();
    }

    const TSharedPtr<IPlugin> plugin =
        IPluginManager::Get().FindPlugin(TEXT("SemanticSurfacePatches"));
    const FString presetDirectory = plugin.IsValid()
        ? FPaths::Combine(plugin->GetBaseDir(), TEXT("Resources/Presets"))
        : FPaths::ProjectDir();

    IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
    if (desktopPlatform == nullptr)
    {
        return FReply::Handled();
    }

    TArray<FString> selectedFiles;
    const void* parentWindow =
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
    if (!desktopPlatform->OpenFileDialog(
        parentWindow,
        TEXT("Load Surface Patch Preset"),
        presetDirectory,
        TEXT("humanoidSignLanguageFace.json"),
        TEXT("Surface Patch Preset (*.json)|*.json"),
        EFileDialogFlags::None,
        selectedFiles) ||
        selectedFiles.IsEmpty())
    {
        return FReply::Handled();
    }

    TArray<FSurfacePatchAnchor> patches;
    FString error;
    if (!readPreset(selectedFiles[0], patches, error))
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(error));
        return FReply::Handled();
    }

    USkeletalMeshComponent* targetMesh = nullptr;
    FName targetMeshComponentName;
    FName parentBone;
    if (!showMeshAndBoneDialog(
        surfacePatches,
        targetMesh,
        targetMeshComponentName,
        parentBone))
    {
        return FReply::Handled();
    }

    const FScopedTransaction transaction(
        LOCTEXT("LoadPresetTransaction", "Load Surface Patch Preset"));
    for (FSurfacePatchAnchor& patch : patches)
    {
        patch.parentBone = parentBone;
    }
    replaceBlueprintPatches(
        surfacePatches,
        targetMesh,
        targetMeshComponentName,
        parentBone,
        patches);
    return FReply::Handled();
}

FReply FSurfacePatchesComponentCustomization::copyFromActor()
{
    USurfacePatchesComponent* destination = component.Get();
    if (destination == nullptr)
    {
        return FReply::Handled();
    }

    USurfacePatchesComponent* source = nullptr;
    USkeletalMeshComponent* targetMesh = nullptr;
    FName targetMeshComponentName;
    FName fallbackBone;
    if (!showCopyDialog(
        destination,
        source,
        targetMesh,
        targetMeshComponentName,
        fallbackBone))
    {
        return FReply::Handled();
    }

    UBlueprint* sourceBlueprint = getOwningBlueprint(source);
    USkeletalMeshComponent* sourceMeshComponent = findCopyMeshTemplate(
        sourceBlueprint,
        source->targetMeshComponentName);
    USkeletalMesh* sourceMesh = sourceMeshComponent != nullptr
        ? sourceMeshComponent->GetSkeletalMeshAsset()
        : nullptr;
    USkeletalMesh* destinationMesh = targetMesh != nullptr
        ? targetMesh->GetSkeletalMeshAsset()
        : nullptr;
    TArray<FSurfacePatchAnchor> copiedPatches =
        getBlueprintPatches(sourceBlueprint);
    if (copiedPatches.IsEmpty())
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            LOCTEXT(
                "NoSourcePatches",
                "The source Blueprint does not contain surface patch anchor components."));
        return FReply::Handled();
    }
    for (FSurfacePatchAnchor& patch : copiedPatches)
    {
        const FName sourceBone = patch.parentBone.IsNone()
            ? source->defaultParentBone
            : patch.parentBone;
        const FName targetBone = targetMesh->GetBoneIndex(sourceBone) != INDEX_NONE
            ? sourceBone
            : fallbackBone;
        convertCopiedPatchBoneSpace(
            patch,
            sourceBone,
            targetBone,
            sourceMesh,
            destinationMesh);
        patch.parentBone = targetBone;
        patch.surfacePatch.Reset();
    }

    const FScopedTransaction transaction(
        LOCTEXT("CopyPatchesTransaction", "Copy Surface Patches From Actor Blueprint"));
    replaceBlueprintPatches(
        destination,
        targetMesh,
        targetMeshComponentName,
        fallbackBone,
        copiedPatches);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
