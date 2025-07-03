/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "fbflcn_timer.h"
#include "fbflcn_helpers.h"
#include "falcon-intrinsics.h"
#include "falc_debug.h"
#include "falc_trace.h"

// LOGGING OPTIONS:
//#define TIMER_LOG_CODE 0x52000000
//#define TIMER_LOG
//#define TIMER_LOG_DEBUG


LwU32 fbflcnTimerInterruptHandler(void) {
#ifdef TIMER_LOG
    FW_MBOX_WR32(10, TIMER_LOG_CODE + 0x20000);
#endif // TIMER_LOG

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    enableIntEvent(EVENT_MASK_TIMER_EVENT);
#else
    enableEvent(EVENT_MASK_TIMER_EVENT);
#endif

#ifdef TIMER_LOG
    FW_MBOX_WR32(10, TIMER_LOG_CODE + 0x20000 + 0xffff);
#endif // TIMER_LOG

    return 1;
}

