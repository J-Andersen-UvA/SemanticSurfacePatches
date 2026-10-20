#pragma once

#include "IDetailCustomization.h"

class USurfacePatchesComponent;

class FSurfacePatchesComponentCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> makeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& detailBuilder) override;

private:
    FReply loadPreset();
    FReply copyFromActor();
    FReply buildSurfacePatches();
    FReply mirrorRightToLeft();
    FReply rotateNormalsOutwards();

    TWeakObjectPtr<USurfacePatchesComponent> component;
};
