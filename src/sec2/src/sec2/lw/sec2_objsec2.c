/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_objsec2.c
 * @brief  General-purpose chip-specific routines which do not exactly fit
 *         within the bounds a particular engine in the GPU.
 *
 * @section _sec2_objsec2 OBJSEC2 Notes
 * SEC2 object is created to serve general purpose chip specific routines that
 * can't be cleanly handled in task* codes without getting tangled up with
 * multiple includes of the same #defines.
 *
 * An engine specific code should still be handled in each engine hal layer,
 * but all other SEC2 features varying from chip to chip should all go in here.
 *
 * Ideally, we do not want to directly refer to chip specific #defines in
 * the main SEC2 app source codes.
 * @endsection
 */

/* ------------------------ System includes ------------------------------- */
#include "sec2sw.h"
#include "sec2_hs.h"
#include "lwostimer.h"
/* ------------------------ Application includes --------------------------- */
#include "sec2_objhal.h"
#include "sec2_objsec2.h"
#include "sec2_objic.h"
#include "sec2_cmdmgmt.h"
#include "sec2_objrttimer.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define SEC2_REG_POLL_PARAM_SIZE     (4)
#define SEC2_REG_POLL_PARAM_ADDR     (0)
#define SEC2_REG_POLL_PARAM_MASK     (1)
#define SEC2_REG_POLL_PARAM_VAL      (2)
#define SEC2_REG_POLL_PARAM_MODE     (3)

/* ------------------------- Prototypes ------------------------------------- */
static LwBool sec2PollReg32Func(LwU32 *)
    GCC_ATTRIB_SECTION("imem_resident", "sec2PollReg32Func");

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Global variables ------------------------------- */
OBJSEC2 Sec2;

#if (SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS) && \
     SEC2CFG_FEATURE_ENABLED(SEC2TASK_HWV))
//
// Only define when needed by enabled tasks, as otherwise
// triggers change in HS code due to moving variables in DMEM.
//
LwrtosSemaphoreHandle GptimerLibMutex;
LwBool                GptimerLibEnabled;
#endif

/*!
 * @brief Initialize the GP timer mutex to enforce ownership during usage.
 *        If we fail to initialize mutex, GP timer lib will be disabled.
 *
 *        Should only be used when GP timer is not used for OS ticks,
 *        i.e. SEC2RTTIMER_FOR_OS_TICKS is enabled.
 */
void
sec2GptmrLibPreInit(void)
{
    FLCN_STATUS status;

    status = lwrtosSemaphoreCreateBinaryGiven(&GptimerLibMutex,
                                              OVL_INDEX_OS_HEAP);

    // Only enable library if mutex properly enabled
    GptimerLibEnabled = (status == FLCN_OK);
}

/*!
 * Start the timer used for the OS scheduler
 *
 * Notes:
 *     - This function MUST be called PRIOR to starting the scheduler.
 *     - @ref icPreInit_HAL should be called before calling this function
 *       to set up the SEC2 interrupts for the timer.
 */
void
sec2StartOsTicksTimer(void)
{
    if (SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS))
    {
        FLCN_STATUS status = rttimerSetup_HAL(&Rttimer, RTTIMER_MODE_START,
                                              LWRTOS_TICK_PERIOD_US,
                                              LW_FALSE);
        OS_ASSERT(status == FLCN_OK);
    }
    else
    {
        //
        // **********THIS SHOULD NEVER BE USED IN PRODUCTION**********
        //
        // This should be used in local build for manual RT timer test.
        //
        sec2GptmrOsTicksEnable_HAL();
    }
}

/*!
 * Poll on a csb/bar0 register until the specified condition is hit.
 *
 * @param[in]  addr      The BAR0 address to read
 * @param[in]  mask      Masks the value to compare
 * @param[in]  val       Value used to compare the bar0 register value
 * @param[in]  timeoutNs Timeout value in ns.
 * @param[in]  mode      Poll mode - see sec2PollReg32Func
 *
 * @return LW_TRUE  - if the specified condition is met
 * @return LW_FALSE - if timed out.
 */
LwBool
sec2PollReg32Ns
(
    LwU32  addr,
    LwU32  mask,
    LwU32  val,
    LwU32  timeoutNs,
    LwU32  mode
)
{
    LwU32 params[SEC2_REG_POLL_PARAM_SIZE];
    params[SEC2_REG_POLL_PARAM_ADDR] = addr;
    params[SEC2_REG_POLL_PARAM_MASK] = mask;
    params[SEC2_REG_POLL_PARAM_VAL ] = val;
    params[SEC2_REG_POLL_PARAM_MODE] = mode;

    return OS_PTIMER_SPIN_WAIT_NS_COND(sec2PollReg32Func, params, timeoutNs);
}

/*!
 * with @ref TIMER_SPIN_WAIT_NS as way of polling on a specific field
 * in a register until the register reports a specific value or until a timeout
 * oclwrs.
 *
 * pParams[0] SEC2_REG_POLL_PARAM_ADDR  - address
 * pParams[1] SEC2_REG_POLL_PARAM_MASK  - mask
 * pParams[2] SEC2_REG_POLL_PARAM_VAL   - value to compare
 * pParams[3] SEC2_REG_POLL_PARAM_MODE  - see below
 *
 *   SEC2_REG_POLL_REG_TYPE_CSB  - poll from a CSB register
 *   SEC2_REG_POLL_REG_TYPE_BAR0 - poll from a BAR0 register
 *   SEC2_REG_POLL_OP_EQ         - returns LW_TRUE if the masked register value
 *                                is equal to the passed value,
 *                                LW_FALSE otherwise
 *   SEC2_REG_POLL_OP_NEQ        - returns LW_TRUE if the masked register value
 *                                is not equal to the passed value,
 *                                LW_FALSE otherwise
 *
 * @return LW_TRUE  - if the specificied condition is met
 * @return LW_FALSE - otherwise
 */
static LwBool
sec2PollReg32Func
(
    LwU32 *pParams
)
{
    LwU32  reg32;
    LwBool bReturn;

    reg32 = FLD_TEST_DRF(_SEC2, _REG_POLL, _REG_TYPE, _BAR0,
                            pParams[SEC2_REG_POLL_PARAM_MODE]) ?
                         REG_RD32(BAR0, pParams[SEC2_REG_POLL_PARAM_ADDR]):
                         CSB_REG_RD32 (pParams[SEC2_REG_POLL_PARAM_ADDR]);
    bReturn = (reg32 & pParams[SEC2_REG_POLL_PARAM_MASK]) ==
                       pParams[SEC2_REG_POLL_PARAM_VAL ];

    return FLD_TEST_DRF(_SEC2, _REG_POLL, _OP, _EQ,
                            pParams[SEC2_REG_POLL_PARAM_MODE]) ?
                         (LwBool) bReturn : !bReturn;
}

/*!
 * @brief Ensure that HW fuse version matches SW fuse version
 *
 * @return FLCN_OK  if HW fuse version and SW fuse version matches
 *         FLCN_ERR_HS_CHK_UCODE_REVOKED  if mismatch
 */
FLCN_STATUS
sec2CheckFuseRevocationHS(void)
{
    LwU32       fuseVersionSW = 0;
    FLCN_STATUS flcnStatus    = FLCN_ERROR;

    // Get SW Fuse version
    CHECK_FLCN_STATUS(sec2GetSWFuseVersionHS_HAL(&Sec2, &fuseVersionSW));

    // Compare SW fuse version against HW fpf version
    CHECK_FLCN_STATUS(sec2CheckFuseRevocationAgainstHWFpfVersionHS_HAL(&sec2, fuseVersionSW));

    // Compare SW fuse version against HW fuse Version 
    CHECK_FLCN_STATUS(sec2CheckFuseRevocationAgainstHWFuseVersionHS_HAL(&sec2, fuseVersionSW));

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Check for LS revocation by comparing LS ucode version against SEC2 rev in HW fuse
 *
 * @return FLCN_OK  if HW fuse version and LS ucode version matches
 *         FLCN_ERR_LS_CHK_UCODE_REVOKED  if mismatch
 */
FLCN_STATUS
sec2CheckForLSRevocation(void)
{
    LwU32       fuseVersionHW = 0;
    FLCN_STATUS status        = FLCN_OK;

    // Read the version number from FUSE
    status = sec2GetHWFuseVersion_HAL(&Sec2, &fuseVersionHW);
    if (status != FLCN_OK)
    {
        return status;
    }

    if (SEC2_LS_UCODE_VERSION < fuseVersionHW)
    {
        return FLCN_ERR_LS_CHK_UCODE_REVOKED;
    }
    return FLCN_OK;
}

