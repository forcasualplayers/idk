#pragma once

#include <editor/widgets/InputResource.h>
#include <editor/widgets/InputGameObject.h>
#include <editor/widgets/EnumCombo.h>
#include <editor/widgets/LayerMaskCombo.h>
#include <editor/widgets/DragVec.h>
#include <editor/widgets/DragQuat.h>
#include <editor/widgets/DragRect.h>
#include <editor/widgets/VerticalText.h>
#include <editor/widgets/IconCheckbox.h>

namespace idk
{
    enum class FontType
    {
        Default = 0,
        Smaller,
        Bold,
    };

    namespace ImGuidk
    {
        void PushDisabled();
        void PopDisabled();
        void PushFont(FontType font);
    }
}