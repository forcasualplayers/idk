#pragma once
#include <idk.h>
#include <core/Component.h>
#include <ui/UISystem.h>

namespace idk
{
    class Image
        : public Component<Image>
    {
    public:
        RscHandle<Texture> texture;
        RscHandle<MaterialInstance> material{ UISystem::default_material_inst };
        color tint{ 1.0f, 1.0f, 1.0f, 1.0f };
        bool preserve_aspect;
    };
}