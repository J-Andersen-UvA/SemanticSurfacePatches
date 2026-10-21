#include "SemanticPatchProximityTypes.h"

bool FSemanticPose::getBoneTransform(
    const FName boneName,
    FTransform& outTransform) const
{
    const FTransform* transform = componentSpaceBoneTransforms.Find(boneName);
    if (transform == nullptr)
    {
        return false;
    }

    outTransform = *transform;
    return true;
}
