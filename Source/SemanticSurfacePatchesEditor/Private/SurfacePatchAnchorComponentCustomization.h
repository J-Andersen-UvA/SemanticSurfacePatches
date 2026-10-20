#pragma once

#include "IDetailCustomization.h"

class USurfacePatchAnchorComponent;

class FSurfacePatchAnchorComponentCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> makeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& detailBuilder) override;

private:
    FReply rebuildPatch();
    FReply rotateNormalOutwards();

    TWeakObjectPtr<USurfacePatchAnchorComponent> anchor;
};
