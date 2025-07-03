/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "fbflcn_defines.h"
#include "memory.h"

#ifndef FBFLCN_HBM_PERIODIC_TRAINING_H_
#define FBFLCN_HBM_PERIODIC_TRAINING_H_
#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
//#include "fbflcn_hbm_mclk_switch.h"
#endif

#define WIR_DWORD_MODE          0x2000  /* bit 13 = 1, bit 12 = 0 */
#define ONE_DWORD_NEEDED        0x2200  /* bit 13 = 1 (needed), bit 12:9 = 1 */
#define WIR_WOSC_RUN            0x16    /* 0=Stop(default), 1=Start */
#define WIR_WOSC_COUNT          0x17    /* [0], 0=invalid(default), 1=valid; [24:1]=Value */
#define WIR_ECS_ERROR_LOG       0x18    /* channel select in [12:8], WDR_length=216 */
#define LOWSEC_MASK_WOSC_RUN   (1 << WIR_WOSC_RUN)  /* mask is positional; set to 1 to access in pl2 */

#define NS_PER_TIMER_TICKS      32
#define US_SHIFT_TO_TIMER_TICKS  5

#define MAX_MUTEX_TIME_MULTIPLIER   2

extern LwU32 gbl_hbm_dramclk_period;      // from fbflcn_hbm_mclk_switch_gh100.c
extern LwU32 gbl_mclk_freq_final;      // from fbflcn_hbm_mclk_switch_gh100.c
extern LwU32 gbl_p0_actual_mhz;      // from fbflcn_hbm_mclk_switch_gh100.c

typedef struct
{
    LwU32 rtl_count;        // LW_PFB_FBPA_FBIO_HBM_TEST_I1500_WOSC_COUNT
    LwU32 ieee_count;       // LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA
} WOSC_STRUCT;


void clearLastTime(void);
void getBaselineWosc(void);
void runHbmPeriodicTr(void);
void initPeriodicTraining(void);
void startPeriodicTraining(void);
void stopPeriodicTraining(void);


#endif /* FBFLCN_HBM_PERIODIC_TRAINING_H_ */
