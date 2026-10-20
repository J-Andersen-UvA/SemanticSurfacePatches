#pragma once

#include "CoreMinimal.h"

class USurfacePatchesComponent;

namespace SurfacePatchBuilder
{
bool build(
    USurfacePatchesComponent* component,
    FText& outResult,
    FName patchId = NAME_None);

bool rotateNormalsOutwards(
    USurfacePatchesComponent* component,
    FText& outResult,
    FName patchId = NAME_None);
}
