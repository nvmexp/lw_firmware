
/* LWIDIA_COPYRIGHT_BEGIN                                                 */
/*                                                                        */
/* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All  */
/* information contained herein is proprietary and confidential to LWPU */
/* Corporation.  Any use, reproduction, or disclosure without the written */
/* permission of LWPU Corporation is prohibited.                        */
/*                                                                        */
/* LWIDIA_COPYRIGHT_END                                                   */

#pragma once

#include "core/include/types.h"

namespace UphyRegLogLists
{
#define DEFINE_UPHY(uphyVersion, uphyBlock, verEnum)                                \
    extern const UINT16* const g_Uphy ## uphyVersion ## uphyBlock ## ShortDump;     \
    extern const size_t        g_Uphy ## uphyVersion ## uphyBlock ## ShortDumpSize; \
    extern const UINT16* const g_Uphy ## uphyVersion ## uphyBlock ## LongDump;      \
    extern const size_t        g_Uphy ## uphyVersion ## uphyBlock ## LongDumpSize;
#define DEFINE_C2C_GPHY(uphyVersion, verEnum)                                       \
    extern const UINT16* const g_C2CGphy ## uphyVersion ## ShortDump;               \
    extern const size_t        g_C2CGphy ## uphyVersion ## ShortDumpSize;           \
    extern const UINT16* const g_C2CGphy ## uphyVersion ## LongDump;                \
    extern const size_t        g_C2CGphy ## uphyVersion ## LongDumpSize;
#include "uphy_list.h"
#undef DEFINE_UPHY
#undef DEFINE_C2C_GPHY
}
