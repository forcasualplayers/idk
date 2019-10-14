// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"
#include <idk.h>
#include <core/Core.h>
#include <reflect/reflect.h>
#include <glad/glad.h>

void _check_gl_error(const char* file, int line);

#define GL_CHECK() _check_gl_error(__FILE__, __LINE__)
#endif //PCH_H