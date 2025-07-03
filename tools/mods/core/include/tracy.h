/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifdef TRACY_ENABLE
#   ifdef __cplusplus
#       include "Tracy.hpp"
#       define PROFILE_MALLOC(ptr, size)         TracyAlloc(ptr, size)
#       define PROFILE_FREE(ptr)                 if (!tracy::GetProfiler().HasShutdownFinished()) TracyFree(ptr)
#       define PROFILE_ZONE(color)               ZoneScopedC(PROFILE_COLOR_ ## color)
#       define PROFILE_NAMED_ZONE(color, name)   ZoneScopedNC(name, PROFILE_COLOR_ ## color)
#       define PROFILE_ZONE_SET_NAME(name, size) ZoneName(name, size)
#       define PROFILE_SET_THREAD_NAME(name)     tracy::SetThreadName(name);
#       define PROFILE_PLOT_INIT(name, type)     TracyPlotConfig(name, tracy::PlotFormatType::type)
#       define PROFILE_PLOT(name, value)         TracyPlot(name, value)
#   else
#       include "TracyC.h"
#       define PROFILE_ZONE_BEGIN(color)         TracyCZoneC(tracy_zone_ctx, PROFILE_COLOR_ ## color, 1)
#       define PROFILE_ZONE_END()                TracyCZoneEnd(tracy_zone_ctx)
#   endif
#   define PROFILE_COLOR_GENERIC                 0x818283
#   define PROFILE_COLOR_RM                      0x4142D3
// MODS redefines MIN/MAX macros, which are already defined in sys/param.h pulled by Tracy
#   ifdef MIN
#       undef MIN
#   endif
#   ifdef MAX
#       undef MAX
#   endif
#else
#   ifdef __cplusplus
#       define PROFILE_MALLOC(ptr, size)
#       define PROFILE_FREE(ptr)
#       define PROFILE_ZONE(color)
#       define PROFILE_NAMED_ZONE(color, name)
#       define PROFILE_ZONE_SET_NAME(name, size)
#       define PROFILE_SET_THREAD_NAME(name)
#       define PROFILE_PLOT_INIT(name, type)
#       define PROFILE_PLOT(name, value)
#   else
#       define PROFILE_ZONE_BEGIN(color)
#       define PROFILE_ZONE_END()
#   endif
#endif
