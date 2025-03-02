#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#define IM_VEC2_CLASS_EXTRA													                        \
        ImVec2(const idk::vec2& f) { x = static_cast<float>(f.x); y = static_cast<float>(f.y); }    \
        operator idk::vec2() const { return idk::vec2(x,y); }                                       \
        ImVec2(const idk::ivec2& f) { x = static_cast<float>(f.x); y = static_cast<float>(f.y); }   \
        operator idk::ivec2() const { return idk::ivec2(static_cast<int>(x),static_cast<int>(y)); }

#define IM_VEC4_CLASS_EXTRA                                                 \
        ImVec4(const idk::vec4& f) { x = f.x; y = f.y; z = f.z; w = f.w; }  \
        operator idk::vec4() const { return idk::vec4(x,y,z,w); }

#include <IconsFontAwesome5_c.h>