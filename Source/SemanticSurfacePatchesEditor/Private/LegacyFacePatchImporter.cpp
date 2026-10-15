#include "LegacyFacePatchImporter.h"

#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "FacePatchConfiguration.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool readVector(
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

bool readTriangle(
    const TSharedPtr<FJsonObject>& object,
    FFacePatchTriangle& triangle)
{
    return readVector(object, TEXT("first"), triangle.first) &&
        readVector(object, TEXT("second"), triangle.second) &&
        readVector(object, TEXT("third"), triangle.third);
}

bool readAttachment(
    const TSharedPtr<FJsonObject>& point,
    const TCHAR* fieldName,
    FFacePatchAttachment& attachment)
{
    const TSharedPtr<FJsonObject>* attachmentObject = nullptr;
    if (!point.IsValid() ||
        !point->TryGetObjectField(fieldName, attachmentObject) ||
        attachmentObject == nullptr ||
        !attachmentObject->IsValid())
    {
        return false;
    }

    FString boneName;
    if (!(*attachmentObject)->TryGetStringField(TEXT("boneName"), boneName) ||
        !readVector(*attachmentObject, TEXT("localPosition"), attachment.localPosition) ||
        !readVector(*attachmentObject, TEXT("localNormal"), attachment.localNormal))
    {
        return false;
    }
    attachment.boneName = FName(*boneName);

    const TArray<TSharedPtr<FJsonValue>>* triangles = nullptr;
    if (!(*attachmentObject)->TryGetArrayField(TEXT("surfacePatch"), triangles) ||
        triangles == nullptr)
    {
        return false;
    }

    attachment.triangles.Reset(triangles->Num());
    for (const TSharedPtr<FJsonValue>& triangleValue : *triangles)
    {
        FFacePatchTriangle triangle;
        if (!triangleValue.IsValid() ||
            !readTriangle(triangleValue->AsObject(), triangle))
        {
            return false;
        }
        attachment.triangles.Add(triangle);
    }
    return true;
}
}

bool ULegacyFacePatchImporter::importLegacyConfigurationJson(
    UFacePatchConfiguration* configuration,
    const FString& filePath,
    FString& errorMessage)
{
    errorMessage.Reset();
    if (configuration == nullptr)
    {
        errorMessage = TEXT("Assign a face patch configuration.");
        return false;
    }

    FString jsonText;
    if (!FFileHelper::LoadFileToString(jsonText, *filePath))
    {
        errorMessage = FString::Printf(TEXT("Could not read '%s'."), *filePath);
        return false;
    }

    TSharedPtr<FJsonObject> root;
    const TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(jsonText);
    if (!FJsonSerializer::Deserialize(reader, root) || !root.IsValid())
    {
        errorMessage = TEXT("The file does not contain valid JSON.");
        return false;
    }

    FString sourceMeshPath;
    FString targetMeshPath;
    root->TryGetStringField(TEXT("sourceSkeletalMesh"), sourceMeshPath);
    root->TryGetStringField(TEXT("targetSkeletalMesh"), targetMeshPath);

    const TArray<TSharedPtr<FJsonValue>>* points = nullptr;
    if (!root->TryGetArrayField(TEXT("contextPoints"), points) || points == nullptr)
    {
        errorMessage = TEXT("The file has no contextPoints array.");
        return false;
    }

    TArray<FPairedFacePatch> importedPatches;
    for (const TSharedPtr<FJsonValue>& pointValue : *points)
    {
        const TSharedPtr<FJsonObject> point = pointValue.IsValid()
            ? pointValue->AsObject()
            : nullptr;
        FString pointName;
        FPairedFacePatch patch;
        if (!point.IsValid() ||
            !point->TryGetStringField(TEXT("pointName"), pointName) ||
            !readAttachment(point, TEXT("source"), patch.source) ||
            !readAttachment(point, TEXT("target"), patch.target))
        {
            continue;
        }
        if (patch.source.triangles.IsEmpty() || patch.target.triangles.IsEmpty())
        {
            continue;
        }

        patch.patchName = FName(*pointName);
        importedPatches.Add(MoveTemp(patch));
    }

    if (importedPatches.IsEmpty())
    {
        errorMessage = TEXT("The file contains no paired source and target face patches.");
        return false;
    }

    configuration->Modify();
    configuration->sourceSkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(sourceMeshPath));
    configuration->targetSkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(targetMeshPath));
    configuration->facePatches = MoveTemp(importedPatches);
    configuration->MarkPackageDirty();
    return true;
}

