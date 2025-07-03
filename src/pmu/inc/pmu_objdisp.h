/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJDISP_H
#define PMU_OBJDISP_H

/*!
 * @file pmu_objdisp.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "lib/lib_pwm.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "config/g_disp_hal.h"
#include "pmu/pmuifdisp.h"

/* ------------------------- Types definitions ----------------------------- */
#define LW_DISP_TIMEOUT_NS_NLTS_AUTO_CLEAR          (100*1000*1000) // 100 ms
#define LW_DISP_CNTL_TRIGGER_UDPATE_DONE_TIME_US    50
#define LW_DISP_PMU_REG_POLL_PMS_TIMEOUT            1000000
#define LW_DISP_PADLINK_ADJ_FACTOR                  4
#define LW_DISP_WAIT_FOR_SVX_INTR_USECS             100             // Delay when polling for the supervisor interrupt
#define LW_DISP_TIMEOUT_FOR_SVX_INTR_NSECS          (10*1000*1000)  // Timeout for the supervisor interrupt set to 10 msec

/* ------------------------ External definitions --------------------------- */
extern LwU8 DispVblankControlSemaphore[RM_PMU_DISP_NUMBER_OF_HEADS_MAX];

/*!
 * DISP object Definition
 */
typedef struct
{

    /*!
     * Context used for PMU to perfrom a modeset
     */
    RM_PMU_DISP_PMS_CTX      *pPmsCtx;

    /*!
     * Context used for PMU to perform a modeset 
     * oclwring during a BSOD 
     */
    RM_PMU_DISP_PMS_BSOD_CTX *pPmsBsodCtx;

    RM_PMU_DISP_IMP_GENERAL_SETTINGS rmModesetData GCC_ATTRIB_ALIGN(DMA_MIN_READ_ALIGNMENT);

    // Mutex to manage MSCG enable/disable
    LwrtosSemaphoreHandle     mscgSemaphore;

    /*!
     * Oneshot start delay to be used in case of MCLK/MSCG
     */
    LwU32                      oneShotStartDelayUs;

    //
    // mscgDisableFlags records MSCG disable requests from various sources;
    // MSCG will not be enabled unless all disable requests are cleared.  Bits
    // are defined by RM_PMU_DISP_MSCG_DISABLE_REASON_xxx.
    //
    LwU32                     mscgDisableReasons;

    /*!
     * Number of supported heads (ZERO if display is not present).
     */
    LwU8                      numHeads;

    /*!
     * Number of supported sors (ZERO if display is not present).
     */
    LwU8                      numSors;

    // Variable to decide if MS DISP War is Active or not
    LwBool                    bMsRgLineWarActive;

    /*!
     * Variable to select to use MSCG or DIFR watermarks
     */
    LwBool                    bUseMscgWatermarks;

#if (PMUCFG_FEATURE_ENABLED(PMU_DISP_PROG_ONE_SHOT_START_DELAY))
    /*!
     * Context for One shot mode Mclk switch settings
     */
    RM_PMU_DISP_IMP_OSM_MCLK_SWITCH_SETTINGS DispImpOsmMclkSwitchSettings;
#endif

} OBJDISP;

extern OBJDISP Disp;

/*!
 * Indicates if display is present
 */
#define DISP_IS_PRESENT() FLD_TEST_DRF(_RM_PMU, _CMD_LINE_ARGS_FLAGS,          \
                                       _DISP_PRESENT, _YES, PmuInitArgs.flags)

/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void dispPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "dispPreInit");
LwBool dispRpcLpwrOsmStateChange(RM_PMU_RPC_STRUCT_LPWR_ONESHOT *)
    GCC_ATTRIB_SECTION("imem_disp", "dispRpcLpwrOsmStateChange");

#endif // PMU_OBJDISP_H
