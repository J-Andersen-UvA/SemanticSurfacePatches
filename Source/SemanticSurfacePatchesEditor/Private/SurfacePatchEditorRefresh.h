#pragma once

class UBlueprint;

namespace SurfacePatchEditorRefresh
{
void refreshBlueprintPreviewInstances(UBlueprint* blueprint);
void scheduleBlueprintPreviewRefresh(UBlueprint* blueprint);
}
