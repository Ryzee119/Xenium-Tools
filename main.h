#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#include "tlsf/tlsf.h"
#include "stb_sprintf/stb_sprintf.h"
#include "xenium/xenium.h"
#include "cimgui.h"
#include "glue/cimgui_impl.h"

#ifdef _MSC_VER
#define _MSC_VER_BACK _MSC_VER
#undef _MSC_VER
#define _MSC_VER 1
#endif
#define STBI_NO_SIMD
#define STBIR_NO_SIMD
#include "stb_image/stb_image.h"

#ifdef _MSC_VER_BACK
#ifndef _MSC_VER
#define _MSC_VER _MSC_VER_BACK
#endif
#endif

#include <SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <pbgl.h>
#include <pbkit/pbkit.h>

#include <stdio.h>
#include <assert.h>

#include <windows.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <nxdk/mount.h>
#include <nxdk/path.h>

#ifndef IM_COL32
#define IM_COL32(R, G, B, A) (((ImU32)(A) << 24) | ((ImU32)(B) << 16) | ((ImU32)(G) << 8) | ((ImU32)(R) << 0))
#endif

typedef enum {
    ALIGN_TOP_LEFT,
    ALIGN_TOP_RIGHT,
    ALIGN_BOTTOM_LEFT,
    ALIGN_BOTTOM_RIGHT,
    ALIGN_CENTER_CENTER,
    ALIGN_CENTER_LEFT,
    ALIGN_CENTER_RIGHT,
    ALIGN_TOP_CENTER,
    ALIGN_BOTTOM_CENTER
} label_align_t;