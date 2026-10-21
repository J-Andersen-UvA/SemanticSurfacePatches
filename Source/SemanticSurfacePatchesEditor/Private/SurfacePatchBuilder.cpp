#include "SurfacePatchBuilder.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SurfacePatchAnchorComponent.h"
#include "SurfacePatchesComponent.h"
#include "SurfacePatchEditorRefresh.h"

#define LOCTEXT_NAMESPACE "SurfacePatchBuilder"

namespace
{
struct FSurfaceVertexKey
{
    int64 x = 0;
    int64 y = 0;
    int64 z = 0;

    bool operator==(const FSurfaceVertexKey& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

uint32 GetTypeHash(const FSurfaceVertexKey& vertex)
{
    return HashCombine(
        HashCombine(::GetTypeHash(vertex.x), ::GetTypeHash(vertex.y)),
        ::GetTypeHash(vertex.z));
}

struct FSurfaceEdgeKey
{
    FSurfaceVertexKey first;
    FSurfaceVertexKey second;

    bool operator==(const FSurfaceEdgeKey& other) const
    {
        return first == other.first && second == other.second;
    }
};

uint32 GetTypeHash(const FSurfaceEdgeKey& edge)
{
    return HashCombine(GetTypeHash(edge.first), GetTypeHash(edge.second));
}

struct FSurfacePatchCandidate
{
    uint32 vertexIndices[3] = {0, 0, 0};
    FVector positions[3] = {
        FVector::ZeroVector,
        FVector::ZeroVector,
        FVector::ZeroVector};
    FSurfaceEdgeKey edges[3];
    FVector normal = FVector::ZeroVector;
    int32 sectionIndex = INDEX_NONE;
};

struct FSurfaceMatch
{
    FVector position = FVector::ZeroVector;
    FVector normal = FVector::ZeroVector;
    double distanceSquared = TNumericLimits<double>::Max();
    bool found = false;
};

FSurfaceVertexKey makeSurfaceVertexKey(const FVector& position)
{
    constexpr double weldingTolerance = 0.001;
    constexpr double weldingPrecision = 1.0 / weldingTolerance;
    return FSurfaceVertexKey{
        FMath::RoundToInt(position.X * weldingPrecision),
        FMath::RoundToInt(position.Y * weldingPrecision),
        FMath::RoundToInt(position.Z * weldingPrecision)};
}

bool isBefore(
    const FSurfaceVertexKey& first,
    const FSurfaceVertexKey& second)
{
    if (first.x != second.x)
    {
        return first.x < second.x;
    }
    if (first.y != second.y)
    {
        return first.y < second.y;
    }
    return first.z < second.z;
}

FSurfaceEdgeKey makeSurfaceEdgeKey(
    const FVector& firstPosition,
    const FVector& secondPosition)
{
    const FSurfaceVertexKey first = makeSurfaceVertexKey(firstPosition);
    const FSurfaceVertexKey second = makeSurfaceVertexKey(secondPosition);
    return isBefore(first, second)
        ? FSurfaceEdgeKey{first, second}
        : FSurfaceEdgeKey{second, first};
}

UBlueprint* getSurfacePatchBlueprint(const UObject* object)
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

TArray<FTransform> getReferenceBoneTransforms(
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

USkeletalMeshComponent* findTargetMesh(
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

FSurfaceMatch findNearestSurface(
    const FVector& point,
    const FSkeletalMeshLODRenderData& lodData,
    const TArray<uint32>& indices)
{
    FSurfaceMatch result;
    const int32 vertexCount = lodData.GetNumVertices();
    for (int32 index = 0; index + 2 < indices.Num(); index += 3)
    {
        const uint32 indexA = indices[index];
        const uint32 indexB = indices[index + 1];
        const uint32 indexC = indices[index + 2];
        if (indexA >= static_cast<uint32>(vertexCount) ||
            indexB >= static_cast<uint32>(vertexCount) ||
            indexC >= static_cast<uint32>(vertexCount))
        {
            continue;
        }

        const FVector positionA(
            lodData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(
                indexA));
        const FVector positionB(
            lodData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(
                indexB));
        const FVector positionC(
            lodData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(
                indexC));
        const FVector closest = FMath::ClosestPointOnTriangleToPoint(
            point,
            positionA,
            positionB,
            positionC);
        const double distanceSquared = FVector::DistSquared(point, closest);
        if (distanceSquared >= result.distanceSquared)
        {
            continue;
        }

        FVector normal =
            FVector(lodData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(indexA)) +
            FVector(lodData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(indexB)) +
            FVector(lodData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(indexC));
        normal.Normalize();
        result.position = closest;
        result.normal = normal;
        result.distanceSquared = distanceSquared;
        result.found = !normal.IsNearlyZero();
    }
    return result;
}
}

bool SurfacePatchBuilder::build(
    USurfacePatchesComponent* component,
    FText& outResult,
    const FName patchId)
{
    UBlueprint* blueprint = getSurfacePatchBlueprint(component);
    USkeletalMeshComponent* targetMesh = findTargetMesh(
        blueprint,
        component != nullptr
            ? component->targetMeshComponentName
            : NAME_None);
    USkeletalMesh* mesh = targetMesh != nullptr
        ? targetMesh->GetSkeletalMeshAsset()
        : nullptr;
    const FSkeletalMeshRenderData* renderData = mesh != nullptr
        ? mesh->GetResourceForRendering()
        : nullptr;
    if (component == nullptr ||
        blueprint == nullptr ||
        targetMesh == nullptr ||
        mesh == nullptr ||
        renderData == nullptr ||
        renderData->LODRenderData.IsEmpty())
    {
        outResult = LOCTEXT(
            "MissingMesh",
            "Select a target skeletal mesh with LOD 0 render data first.");
        return false;
    }

    const FSkeletalMeshLODRenderData& lodData = renderData->LODRenderData[0];
    TArray<uint32> indices;
    lodData.MultiSizeIndexContainer.GetIndexBuffer(indices);
    if (indices.IsEmpty())
    {
        outResult = LOCTEXT(
            "MissingTriangles",
            "The target skeletal mesh LOD 0 has no readable triangles.");
        return false;
    }

    const int32 vertexCount = lodData.GetNumVertices();
    TArray<FSurfacePatchCandidate> candidates;
    TMap<FSurfaceEdgeKey, TArray<int32>> candidatesByEdge;
    for (int32 sectionIndex = 0;
         sectionIndex < lodData.RenderSections.Num();
         ++sectionIndex)
    {
        const FSkelMeshRenderSection& section =
            lodData.RenderSections[sectionIndex];
        const uint32 firstIndex = section.BaseIndex;
        const uint32 sectionIndexCount = section.NumTriangles * 3;
        const uint32 lastIndex = FMath::Min<uint32>(
            firstIndex + sectionIndexCount,
            indices.Num());
        for (uint32 index = firstIndex; index + 2 < lastIndex; index += 3)
        {
            const uint32 indexA = indices[index];
            const uint32 indexB = indices[index + 1];
            const uint32 indexC = indices[index + 2];
            if (indexA >= static_cast<uint32>(vertexCount) ||
                indexB >= static_cast<uint32>(vertexCount) ||
                indexC >= static_cast<uint32>(vertexCount))
            {
                continue;
            }

            FSurfacePatchCandidate candidate;
            candidate.vertexIndices[0] = indexA;
            candidate.vertexIndices[1] = indexB;
            candidate.vertexIndices[2] = indexC;
            candidate.positions[0] = FVector(
                lodData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(
                    indexA));
            candidate.positions[1] = FVector(
                lodData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(
                    indexB));
            candidate.positions[2] = FVector(
                lodData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(
                    indexC));
            candidate.normal =
                FVector(lodData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(indexA)) +
                FVector(lodData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(indexB)) +
                FVector(lodData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(indexC));
            candidate.normal.Normalize();
            candidate.sectionIndex = sectionIndex;
            candidate.edges[0] = makeSurfaceEdgeKey(
                candidate.positions[0],
                candidate.positions[1]);
            candidate.edges[1] = makeSurfaceEdgeKey(
                candidate.positions[1],
                candidate.positions[2]);
            candidate.edges[2] = makeSurfaceEdgeKey(
                candidate.positions[2],
                candidate.positions[0]);

            const int32 candidateIndex = candidates.Add(candidate);
            for (const FSurfaceEdgeKey& edge : candidate.edges)
            {
                candidatesByEdge.FindOrAdd(edge).Add(candidateIndex);
            }
        }
    }

    blueprint->Modify();
    blueprint->SimpleConstructionScript->Modify();
    const FReferenceSkeleton& skeleton = mesh->GetRefSkeleton();
    const TArray<FTransform> boneTransforms =
        getReferenceBoneTransforms(skeleton);
    int32 builtPatchCount = 0;
    int32 triangleCount = 0;
    TArray<FString> skipped;
    FString selectedPatchDiagnostics;

    for (USCS_Node* node : blueprint->SimpleConstructionScript->GetAllNodes())
    {
        USurfacePatchAnchorComponent* anchor =
            Cast<USurfacePatchAnchorComponent>(node->ComponentTemplate);
        if (anchor == nullptr ||
            !anchor->useSurfacePatch ||
            (!patchId.IsNone() && anchor->patchId != patchId))
        {
            continue;
        }

        const FName boneName = node->AttachToName.IsNone()
            ? component->defaultParentBone
            : node->AttachToName;
        const int32 boneIndex = skeleton.FindBoneIndex(boneName);
        if (boneIndex == INDEX_NONE || !boneTransforms.IsValidIndex(boneIndex))
        {
            skipped.Add(FString::Printf(
                TEXT("%s: bone '%s' was not found"),
                *anchor->patchId.ToString(),
                *boneName.ToString()));
            continue;
        }

        anchor->Modify();
        const int32 previousTriangleCount = anchor->surfacePatch.Num();
        anchor->surfacePatch.Reset();
        anchor->boneLocalSurfaceNormal = FVector::ZeroVector;
        anchor->boneLocalSemanticUp = FVector::ZeroVector;
        const FTransform& boneTransform = boneTransforms[boneIndex];
        const FTransform anchorTransform = anchor->GetRelativeTransform();
        const FVector anchorPosition = boneTransform.TransformPosition(
            anchorTransform.GetLocation());
        const FVector anchorNormal = boneTransform.TransformVectorNoScale(
            anchorTransform.GetRotation().RotateVector(FVector::ForwardVector))
            .GetSafeNormal();
        const double radiusSquared = FMath::Square(anchor->surfacePatchRadius);

        int32 seedIndex = INDEX_NONE;
        double seedDistanceSquared = TNumericLimits<double>::Max();
        FVector seedPoint = FVector::ZeroVector;
        bool seedWasDirectional = false;
        const FVector inwardDirection = -anchorNormal;
        const double rayLength = FMath::Max(
            10.0,
            anchor->surfacePatchRadius * 4.0);
        const FVector rayEnd =
            anchorPosition + inwardDirection * rayLength;
        double nearestRayDistance = TNumericLimits<double>::Max();
        for (int32 candidateIndex = 0;
             candidateIndex < candidates.Num();
             ++candidateIndex)
        {
            const FSurfacePatchCandidate& candidate = candidates[candidateIndex];
            if (candidate.normal.IsNearlyZero() ||
                FVector::DotProduct(candidate.normal, anchorNormal) <
                    anchor->surfacePatchNormalThreshold)
            {
                continue;
            }

            FVector intersection = FVector::ZeroVector;
            FVector intersectionNormal = FVector::ZeroVector;
            if (FMath::SegmentTriangleIntersection(
                    anchorPosition,
                    rayEnd,
                    candidate.positions[0],
                    candidate.positions[1],
                    candidate.positions[2],
                    intersection,
                    intersectionNormal))
            {
                const double rayDistance = FVector::DotProduct(
                    intersection - anchorPosition,
                    inwardDirection);
                if (rayDistance >= 0.0 && rayDistance < nearestRayDistance)
                {
                    seedIndex = candidateIndex;
                    seedPoint = intersection;
                    nearestRayDistance = rayDistance;
                    seedWasDirectional = true;
                }
            }
        }

        if (seedIndex == INDEX_NONE)
        {
            for (int32 candidateIndex = 0;
                 candidateIndex < candidates.Num();
                 ++candidateIndex)
            {
                const FSurfacePatchCandidate& candidate =
                    candidates[candidateIndex];
                if (candidate.normal.IsNearlyZero() ||
                    FVector::DotProduct(candidate.normal, anchorNormal) <
                        anchor->surfacePatchNormalThreshold)
                {
                    continue;
                }

                const FVector closest = FMath::ClosestPointOnTriangleToPoint(
                    anchorPosition,
                    candidate.positions[0],
                    candidate.positions[1],
                    candidate.positions[2]);
                const double distanceSquared = FVector::DistSquared(
                    anchorPosition,
                    closest);
                if (distanceSquared <= radiusSquared &&
                    distanceSquared < seedDistanceSquared)
                {
                    seedIndex = candidateIndex;
                    seedPoint = closest;
                    seedDistanceSquared = distanceSquared;
                }
            }
        }

        if (seedIndex == INDEX_NONE)
        {
            skipped.Add(FString::Printf(
                TEXT("%s: no seed triangle matched its %.2f cm radius and normal threshold"),
                *anchor->patchId.ToString(),
                anchor->surfacePatchRadius));
            continue;
        }

        const FVector surfaceNormal = candidates[seedIndex].normal.GetSafeNormal();
        FVector semanticUp = FVector::UpVector -
            surfaceNormal * FVector::DotProduct(FVector::UpVector, surfaceNormal);
        if (!semanticUp.Normalize())
        {
            semanticUp = FVector::ForwardVector -
                surfaceNormal * FVector::DotProduct(
                    FVector::ForwardVector,
                    surfaceNormal);
            semanticUp.Normalize();
        }
        anchor->boneLocalSurfaceNormal = boneTransform
            .InverseTransformVectorNoScale(surfaceNormal)
            .GetSafeNormal();
        anchor->boneLocalSemanticUp = boneTransform
            .InverseTransformVectorNoScale(semanticUp)
            .GetSafeNormal();

        const FVector seedOffset = seedPoint - anchorPosition;
        const double seedNormalOffset = FVector::DotProduct(
            seedOffset,
            anchorNormal);
        const double seedTangentialOffset =
            (seedOffset - anchorNormal * seedNormalOffset).Size();
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Surface patch '%s' seed (%s): distance %.4f cm, normal offset %.4f cm, tangential offset %.4f cm, anchor (%s), seed (%s)"),
            *anchor->patchId.ToString(),
            seedWasDirectional ? TEXT("directional") : TEXT("nearest fallback"),
            seedOffset.Size(),
            seedNormalOffset,
            seedTangentialOffset,
            *anchorPosition.ToCompactString(),
            *seedPoint.ToCompactString());

        int32 eligibleTriangleCount = 0;
        for (const FSurfacePatchCandidate& candidate : candidates)
        {
            if (candidate.normal.IsNearlyZero() ||
                FVector::DotProduct(candidate.normal, anchorNormal) <
                    anchor->surfacePatchNormalThreshold)
            {
                continue;
            }

            const FVector closest = FMath::ClosestPointOnTriangleToPoint(
                seedPoint,
                candidate.positions[0],
                candidate.positions[1],
                candidate.positions[2]);
            if (FVector::DistSquared(seedPoint, closest) <= radiusSquared)
            {
                ++eligibleTriangleCount;
            }
        }
        if (!patchId.IsNone())
        {
            selectedPatchDiagnostics = FString::Printf(
                TEXT("\n\nSeed method: %s\nSeed distance: %.4f cm\nNormal offset: %.4f cm\nTangential offset: %.4f cm\nRadius-eligible triangles: %d\nAnchor: %s\nSeed: %s"),
                seedWasDirectional
                    ? TEXT("directional")
                    : TEXT("nearest fallback"),
                seedOffset.Size(),
                seedNormalOffset,
                seedTangentialOffset,
                eligibleTriangleCount,
                *anchorPosition.ToCompactString(),
                *seedPoint.ToCompactString());
        }

        const int32 seedSectionIndex = candidates[seedIndex].sectionIndex;
        TArray<int32> pendingCandidates;
        pendingCandidates.Add(seedIndex);
        TBitArray<> visited(false, candidates.Num());
        while (!pendingCandidates.IsEmpty())
        {
            const int32 candidateIndex =
                pendingCandidates.Pop(EAllowShrinking::No);
            if (!visited.IsValidIndex(candidateIndex) ||
                visited[candidateIndex])
            {
                continue;
            }
            visited[candidateIndex] = true;

            const FSurfacePatchCandidate& candidate = candidates[candidateIndex];
            if (candidate.normal.IsNearlyZero() ||
                FVector::DotProduct(candidate.normal, anchorNormal) <
                    anchor->surfacePatchNormalThreshold)
            {
                continue;
            }

            const FVector closest = FMath::ClosestPointOnTriangleToPoint(
                seedPoint,
                candidate.positions[0],
                candidate.positions[1],
                candidate.positions[2]);
            if (FVector::DistSquared(seedPoint, closest) > radiusSquared)
            {
                continue;
            }

            FVector positionA = candidate.positions[0];
            FVector positionB = candidate.positions[1];
            FVector positionC = candidate.positions[2];
            const FVector geometricNormal = FVector::CrossProduct(
                positionB - positionA,
                positionC - positionA).GetSafeNormal();
            if (FVector::DotProduct(geometricNormal, candidate.normal) < 0.0)
            {
                Swap(positionB, positionC);
            }

            FSurfacePatchTriangle& triangle =
                anchor->surfacePatch.AddDefaulted_GetRef();
            triangle.first = anchorTransform.InverseTransformPosition(
                boneTransform.InverseTransformPosition(positionA));
            triangle.second = anchorTransform.InverseTransformPosition(
                boneTransform.InverseTransformPosition(positionB));
            triangle.third = anchorTransform.InverseTransformPosition(
                boneTransform.InverseTransformPosition(positionC));

            for (const FSurfaceEdgeKey& edge : candidate.edges)
            {
                const TArray<int32>* neighbours = candidatesByEdge.Find(edge);
                if (neighbours == nullptr)
                {
                    continue;
                }
                for (const int32 neighbourIndex : *neighbours)
                {
                    if (visited.IsValidIndex(neighbourIndex) &&
                        !visited[neighbourIndex] &&
                        FVector::DotProduct(
                            candidate.normal,
                            candidates[neighbourIndex].normal) >=
                            anchor->surfacePatchEdgeNormalThreshold)
                    {
                        pendingCandidates.Add(neighbourIndex);
                    }
                }
            }
        }

        if (anchor->surfacePatch.IsEmpty())
        {
            skipped.Add(FString::Printf(
                TEXT("%s: no connected triangles matched its %.2f cm radius"),
                *anchor->patchId.ToString(),
                anchor->surfacePatchRadius));
            continue;
        }

        ++builtPatchCount;
        triangleCount += anchor->surfacePatch.Num();
        anchor->refreshSurfacePatchPreview();
        const FSkelMeshRenderSection& seedSection =
            lodData.RenderSections[seedSectionIndex];
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Surface patch '%s': replaced %d triangle(s) with %d connected triangle(s); seed section %d, material %d"),
            *anchor->patchId.ToString(),
            previousTriangleCount,
            anchor->surfacePatch.Num(),
            seedSectionIndex,
            seedSection.MaterialIndex);
    }

    if (builtPatchCount > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(blueprint);
        FKismetEditorUtilities::CompileBlueprint(blueprint);
        SurfacePatchEditorRefresh::scheduleBlueprintPreviewRefresh(blueprint);
    }

    FString result = FString::Printf(
        TEXT("Built %d surface patch(es) containing %d LOD 0 triangle(s)."),
        builtPatchCount,
        triangleCount);
    if (!skipped.IsEmpty())
    {
        result += FString::Printf(
            TEXT("\n\nSkipped %d anchor(s):\n%s"),
            skipped.Num(),
            *FString::Join(skipped, TEXT("\n")));
    }
    result += selectedPatchDiagnostics;
    outResult = FText::FromString(result);
    return builtPatchCount > 0;
}

bool SurfacePatchBuilder::rotateNormalsOutwards(
    USurfacePatchesComponent* component,
    FText& outResult,
    const FName patchId)
{
    UBlueprint* blueprint = getSurfacePatchBlueprint(component);
    USkeletalMeshComponent* targetMesh = findTargetMesh(
        blueprint,
        component != nullptr
            ? component->targetMeshComponentName
            : NAME_None);
    USkeletalMesh* mesh = targetMesh != nullptr
        ? targetMesh->GetSkeletalMeshAsset()
        : nullptr;
    const FSkeletalMeshRenderData* renderData = mesh != nullptr
        ? mesh->GetResourceForRendering()
        : nullptr;
    if (component == nullptr ||
        blueprint == nullptr ||
        mesh == nullptr ||
        renderData == nullptr ||
        renderData->LODRenderData.IsEmpty())
    {
        outResult = LOCTEXT(
            "SurfaceNormalsNeedMesh",
            "Select a target skeletal mesh with LOD 0 render data first.");
        return false;
    }

    const FSkeletalMeshLODRenderData& lodData = renderData->LODRenderData[0];
    TArray<uint32> indices;
    lodData.MultiSizeIndexContainer.GetIndexBuffer(indices);
    if (indices.IsEmpty())
    {
        outResult = LOCTEXT(
            "SurfaceNormalsNeedTriangles",
            "The target skeletal mesh LOD 0 has no readable triangles.");
        return false;
    }

    blueprint->Modify();
    blueprint->SimpleConstructionScript->Modify();
    const FReferenceSkeleton& skeleton = mesh->GetRefSkeleton();
    const TArray<FTransform> boneTransforms =
        getReferenceBoneTransforms(skeleton);
    const double characterHeight = mesh->GetBounds().BoxExtent.Z * 2.0;
    const double maximumDistance = FMath::Max(1.0, characterHeight * 0.03);
    const double maximumDistanceSquared = FMath::Square(maximumDistance);
    TArray<FString> skipped;
    int32 rotatedCount = 0;

    for (USCS_Node* node : blueprint->SimpleConstructionScript->GetAllNodes())
    {
        USurfacePatchAnchorComponent* anchor =
            Cast<USurfacePatchAnchorComponent>(node->ComponentTemplate);
        if (anchor == nullptr ||
            (!patchId.IsNone() && anchor->patchId != patchId))
        {
            continue;
        }

        const FName boneName = node->AttachToName.IsNone()
            ? component->defaultParentBone
            : node->AttachToName;
        const int32 boneIndex = skeleton.FindBoneIndex(boneName);
        if (boneIndex == INDEX_NONE || !boneTransforms.IsValidIndex(boneIndex))
        {
            skipped.Add(FString::Printf(
                TEXT("%s: bone '%s' was not found"),
                *anchor->patchId.ToString(),
                *boneName.ToString()));
            continue;
        }

        const FTransform& boneTransform = boneTransforms[boneIndex];
        const FVector anchorInMeshSpace = boneTransform.TransformPosition(
            anchor->GetRelativeLocation());
        const FSurfaceMatch surface = findNearestSurface(
            anchorInMeshSpace,
            lodData,
            indices);
        if (!surface.found ||
            surface.distanceSquared > maximumDistanceSquared)
        {
            const double distance = surface.found
                ? FMath::Sqrt(surface.distanceSquared)
                : 0.0;
            skipped.Add(FString::Printf(
                TEXT("%s: nearest surface is %.2f cm away"),
                *anchor->patchId.ToString(),
                distance));
            continue;
        }

        FVector surfaceNormal = surface.normal;
        FVector outwardHint = anchorInMeshSpace - surface.position;
        if (outwardHint.IsNearlyZero())
        {
            outwardHint = anchorInMeshSpace - boneTransform.GetLocation();
        }
        if (!outwardHint.IsNearlyZero() &&
            FVector::DotProduct(surfaceNormal, outwardHint) < 0.0)
        {
            surfaceNormal = -surfaceNormal;
        }

        const FVector normalInBoneSpace = boneTransform
            .InverseTransformVectorNoScale(surfaceNormal)
            .GetSafeNormal();
        if (normalInBoneSpace.IsNearlyZero())
        {
            skipped.Add(FString::Printf(
                TEXT("%s: surface normal is invalid"),
                *anchor->patchId.ToString()));
            continue;
        }

        anchor->Modify();
        FVector semanticUpInMeshSpace = FVector::UpVector -
            surfaceNormal * FVector::DotProduct(
                FVector::UpVector,
                surfaceNormal);
        if (!semanticUpInMeshSpace.Normalize())
        {
            semanticUpInMeshSpace = FVector::ForwardVector -
                surfaceNormal * FVector::DotProduct(
                    FVector::ForwardVector,
                    surfaceNormal);
            semanticUpInMeshSpace.Normalize();
        }
        const FVector semanticUpInBoneSpace = boneTransform
            .InverseTransformVectorNoScale(semanticUpInMeshSpace)
            .GetSafeNormal();
        anchor->SetRelativeRotation(
            FRotationMatrix::MakeFromXZ(
                normalInBoneSpace,
                semanticUpInBoneSpace).Rotator());
        anchor->boneLocalSurfaceNormal = FVector::ZeroVector;
        anchor->boneLocalSemanticUp = FVector::ZeroVector;
        anchor->surfacePatch.Reset();
        anchor->refreshSurfacePatchPreview();
        ++rotatedCount;
    }

    if (rotatedCount > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(blueprint);
        FKismetEditorUtilities::CompileBlueprint(blueprint);
        SurfacePatchEditorRefresh::scheduleBlueprintPreviewRefresh(blueprint);
    }

    FString result = FString::Printf(
        TEXT("Rotated %d anchor normal(s) to outward LOD 0 surface normals."),
        rotatedCount);
    if (!skipped.IsEmpty())
    {
        result += FString::Printf(
            TEXT("\n\nSkipped %d anchor(s):\n%s"),
            skipped.Num(),
            *FString::Join(skipped, TEXT("\n")));
    }
    outResult = FText::FromString(result);
    return rotatedCount > 0;
}

#undef LOCTEXT_NAMESPACE
