/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_THRMSLCT_H
#define PMU_THRMSLCT_H

/*!
 * @file thrmslct.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "therm/thrmintr.h"

/* ------------------------ Defines ---------------------------------------- */
/*!
 * The RM_PMU_SW_SLOW feature reason IDs
 *
 * Starting with the GF11X, HW provides us with a SW_MIN slowdown that is in RM
 * abstracted through the RM_PMU_THERM_EVENT_SW_MIN event. This HW feature can
 * handle only a single slowdown request, so a SW feature (RM_PMU_SW_SLOW) was
 * built on the top of it, allowing almost unlimited number of conlwrrent SW
 * slowdown reasons. This enum defines all lwrrently supported reasons.
 *
 *  THERM_PMU_SW_SLOW_REASON_RM_NORMAL
 *      Used for the low priority RM requests (low priority requests can be
 *      disabled by the slowdown control feature).
 *
 *  THERM_PMU_SW_SLOW_REASON_RM_FORCED
 *      Used for the high priority RM requests (high priority requests can not
 *      be disabled by the slowdown control feature).
 *
 *  THERM_PMU_SW_SLOW_REASON_GPCCLK_SWITCH
 *      Used by the GK10X+ clock code to assert slowdown during GPCCLK switch.
 */
#define THERM_PMU_SW_SLOW_REASON_RM_NORMAL                  0x00
#define THERM_PMU_SW_SLOW_REASON_RM_FORCED                  0x01
#define THERM_PMU_SW_SLOW_REASON_GPCCLK_SWITCH              0x02
// Insert new define(s) here. Keep updated _COUNT the last one.
#define THERM_PMU_SW_SLOW_REASON_COUNT                      0x03

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Data for the RM_PMU_SW_SLOW feature.
 */
typedef struct
{
    /*!
     * Set to LW_TRUE if PMU arbiter is enabled.
     */
    LwBool          bArbiterActive;
    struct
    {
        /*!
         * Current state of the feature (ON/OFF).
         */
        LwBool      bEnabled;
        struct
        {
            /*!
             * Slowdown requested (per reason).
             */
            LwU8    factor;

            /*!
             * Set to LW_TRUE if can not be disabled.
             */
            LwBool  bAlwaysActive;
        } slowdown[THERM_PMU_SW_SLOW_REASON_COUNT];

    } clock[RM_PMU_THERM_CLK_COUNT_MAX];

} THERM_RM_PMU_SW_SLOW;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void thermSlowCtrlSwSlowInitialize(void)
    GCC_ATTRIB_SECTION("imem_init", "thermSlowCtrlSwSlowInitialize");

void thermSlowCtrlHwFsHandleTriggeredSlowIntr(LwU32 maskAsserted);

void thermSlowCtrlSwSlowActivateArbiter(LwBool bActivateArbiter);

#endif // PMU_THRMSLCT_H
