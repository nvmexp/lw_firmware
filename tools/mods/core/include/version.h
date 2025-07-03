/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Declarations for the Makefile-generated version.cpp.

#pragma once

enum class FieldDiagMode
{
#define DEFINE_FIELDDIAG_MODE(m) FDM_ ## m,
    #include "core/include/fielddiagmodes.h"
#undef DEFINE_FIELDDIAG_MODE
};

extern const char *         g_Version;
extern const unsigned int   g_Changelist;
extern const char *         g_BuildDate;
extern const char *         g_BuildOS;
extern const char *         g_BuildOSSubtype;
extern const char *         g_BuildCfg;
extern const char *         g_BuildArch;
extern const FieldDiagMode  g_FieldDiagMode;
extern const bool           g_INCLUDE_GPU;
extern const bool           g_INCLUDE_RMTEST;
extern const bool           g_INCLUDE_OGL;
extern const bool           g_INCLUDE_OGL_ES;
extern const bool           g_INCLUDE_VULKAN;
extern const bool           g_INCLUDE_LWOGTEST;
extern const bool           g_INCLUDE_LWDA;
extern const bool           g_INCLUDE_LWDART;
extern const bool           g_INCLUDE_MDIAG;
extern const bool           g_INCLUDE_MDIAGUTL;
extern const bool           g_BUILD_TEGRA_EMBEDDED;
extern const unsigned int   g_BypassHash;
extern const unsigned int g_InternalBypassHash;
