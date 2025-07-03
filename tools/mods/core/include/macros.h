/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

// Don't expose MODS function names in release builds
#if defined(ENABLE__FUNCTION__)
#   ifdef __GNUC__
#       define MODS_FUNCTION __PRETTY_FUNCTION__
#   elif defined(_MSC_VER)
#       define MODS_FUNCTION __FUNCSIG__
#   else
#       define MODS_FUNCTION __FUNCTION__
#   endif
#else
#   define MODS_FUNCTION "<func>"
#endif

// Don't expose source file names in release builds
#if defined(ENABLE__FUNCTION__)
#   define MODS_FILE __FILE__
#else
#   define MODS_FILE "<file>"
#endif
