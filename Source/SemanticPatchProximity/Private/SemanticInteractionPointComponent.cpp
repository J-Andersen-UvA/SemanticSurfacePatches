#include "SemanticInteractionPointComponent.h"

#include "Components/SkeletalMeshComponent.h"

USemanticInteractionPointComponent::USemanticInteractionPointComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ArrowColor = FColor(64, 210, 255);
    ArrowLength = 3.0f;
    ArrowSize = 0.45f;
    bIsScreenSizeScaled = true;
    ScreenSize = 0.0025f;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetHiddenInGame(true);
}

FSemanticInteractionPoint
USemanticInteractionPointComponent::toInteractionPoint() const
{
    FSemanticInteractionPoint point;
    point.pointId = pointId;
    point.interactionGroupId = interactionGroupId;
    point.parentBone = GetAttachSocketName();
    point.boneLocalPosition = GetRelativeLocation();
    point.pointRadius = pointRadius;
    point.enabled = enabled;
    return point;
}
