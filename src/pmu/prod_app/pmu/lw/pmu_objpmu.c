/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objpmu.c
 * @brief  General-purpose chip-specific routines which do not exactly fit
 *         within the bounds a particular engine in the GPU.
 *
 * @section _pmu_objpmu OBJPMU Notes
 * PMU object is created to serve general purpose chip specific routines that
 * can't be cleanly handled in task* codes without getting tangled up with
 * multiple includes of the same #defines.
 *
 * An engine specific code should still be handled in each engine hal layer,
 * but all other PMU features varying from chip to chip should all go in here.
 *
 * Ideally, we do not want to directly refer to chip specific #defines in
 * the main pmu app source codes.
 * @endsection
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "main.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "pmu_objfb.h"

#include "config/g_pmu_private.h"
#if(PMU_MOCK_FUNCTIONS_GENERATED)
#include "config/g_pmu_mock.c"
#endif

/* ------------------------- Macros and Defines ----------------------------- */
#define PMU_REG_POLL_PARAM_SIZE     (4)
#define PMU_REG_POLL_PARAM_ADDR     (0)
#define PMU_REG_POLL_PARAM_MASK     (1)
#define PMU_REG_POLL_PARAM_VAL      (2)
#define PMU_REG_POLL_PARAM_MODE     (3)

/* ------------------------- External Definitions --------------------------- */
extern LwU32 PmuPrivLevelCtrlRegAddr;

/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS  s_pmuPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "s_pmuPreInit");
static FLCN_STATUS  s_pmuChipInfoInitAndValidate(void)
    GCC_ATTRIB_SECTION("imem_init", "s_pmuChipInfoInitAndValidate");
static LwBool       pmuPollReg32Func(void *pArgs)
    GCC_ATTRIB_SECTION("imem_resident", "pmuPollReg32Func");

OBJPMU Pmu;

//
// This variable won't be initialized if the feature isn't enabled., so it's
// `#if`d out so that the linker will produce an error if some code tries to
// access it.
//
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SELWRITY_BYPASS)
LwU32  PmuVSBCache = LW_U32_MIN;
#endif

/*!
 * Construct the PMU object.  This includes the HAL interface as well as all
 * software objects (ex. semaphores) used by the PMU module.
 */
FLCN_STATUS
constructPmu_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // Initialize DMEM_VA. This can be done as soon as the initialization
    // arguments are cached. RM copies those to the top of DMEM, and once we
    // have cached them into our local structure, all of the memory above
    // DMEM_VA_BOUND can be used as the swappable region.
    //
#ifdef DMEM_VA_SUPPORTED
    status = pmuDmemVaInit_HAL(&Pmu, PROFILE_DMEM_VA_BOUND);
    if (status != FLCN_OK)
    {
        goto pmuPreInit_exit;
    }
#endif

    // Pre-init PMU HW.
    status = s_pmuPreInit();

    if (status != FLCN_OK)
    {
        goto pmuPreInit_exit;
    }

pmuPreInit_exit:
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_WAIT_UNTIL_DONE_OR_TIMEOUT)
/*!
 * Wait for an operation to complete or until timeout (specified in OS ticks).
 *
 * @param[in]  pFunc
 *    Pointer to the function to call that determines if the operation has
 *    completed. The function must return LW_TRUE when complete.
 *
 * @param[in]  pArgs
 *     Arguments to pass to the function
 *
 * @param[in]  maxTicks
 *     Minimum number of OS ticks that must elapse before timing-out.
 *
 * @return 'LW_TRUE'   operation completed before the timeout
 * @return 'LW_FALSE'  operation did not complete before the timeout
 */
LwBool
pmuWaitUntilDoneOrTimeout
(
    LwBool (*pFunc)(LwS32 *pArgs),
    LwS32   *pArgs,
    LwU32    maxTicks
)
{
    LwU32 ticks = 0;
    while (!pFunc(pArgs) && ticks < maxTicks)
    {
        (void)lwrtosLwrrentTaskDelayTicks(1);
        ticks++;
    }
    return ticks >= maxTicks;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_WAIT_UNTIL_DONE_OR_TIMEOUT)

/*!
 * @brief   Secure write to BAR0 register that cannot elevate privilege level.
 *
 * @param[in]   addr    BAR0 address to be written
 * @param[in]   data    Value to be written
 */
void
pmuBar0Wr32Safe
(
    LwU32 addr,
    LwU32 data
)
{
    if (addr != PmuPrivLevelCtrlRegAddr)
    {
        REG_WR32(BAR0, addr, data);
    }
    else
    {
        PMU_HALT();
    }
}

/*!
 * Poll on a csb/bar0 register until the specified condition is hit.
 *
 * @param[in]  addr      The BAR0 address to read
 * @param[in]  mask      Masks the value to compare
 * @param[in]  val       Value used to compare the bar0 register value
 * @param[in]  timeoutNs Timeout value in ns.
 * @param[in]  mode      Poll mode - see pmuPollReg32Func
 *
 * @return LW_TRUE  - if the specificied condition is met
 * @return LW_FALSE - if timed out.
 */
LwBool
pmuPollReg32Ns_IMPL
(
    LwU32  addr,
    LwU32  mask,
    LwU32  val,
    LwU32  timeoutNs,
    LwU32  mode
)
{
    LwU32 params[PMU_REG_POLL_PARAM_SIZE];
    params[PMU_REG_POLL_PARAM_ADDR] = addr;
    params[PMU_REG_POLL_PARAM_MASK] = mask;
    params[PMU_REG_POLL_PARAM_VAL ] = val;
    params[PMU_REG_POLL_PARAM_MODE] = mode;

    return OS_PTIMER_SPIN_WAIT_NS_COND(pmuPollReg32Func, params, timeoutNs);
}

/*!
 * with @ref TIMER_SPIN_WAIT_NS as way of polling on a specific field
 * in a register until the register reports a specific value or until a timeout
 * oclwrs.
 *
 * pParams[0] PMU_REG_POLL_PARAM_ADDR  - address
 * pParams[1] PMU_REG_POLL_PARAM_MASK  - mask
 * pParams[2] PMU_REG_POLL_PARAM_VAL   - value to compare
 * pParams[3] PMU_REG_POLL_PARAM_MODE  - see below
 *
 *   PMU_REG_POLL_REG_TYPE_CSB  - poll from a CSB register
 *   PMU_REG_POLL_REG_TYPE_BAR0 - poll from a BAR0 register
 *   PMU_REG_POLL_OP_EQ         - returns LW_TRUE if the masked register value
 *                                is equal to the passed value,
 *                                LW_FALSE otherwise
 *   PMU_REG_POLL_OP_NEQ        - returns LW_TRUE if the masked register value
 *                                is not equal to the passed value,
 *                                LW_FALSE otherwise
 *
 * @return LW_TRUE  - if the specificied condition is met
 * @return LW_FALSE - otherwise
 */
static LwBool
pmuPollReg32Func
(
    void *pArgs
)
{
    LwU32 *pParams = (LwU32*)pArgs;
    LwU32  reg32;
    LwBool bReturn;

    reg32 = FLD_TEST_DRF(_PMU, _REG_POLL, _REG_TYPE, _BAR0,
                            pParams[PMU_REG_POLL_PARAM_MODE]) ?
                         REG_RD32(BAR0, pParams[PMU_REG_POLL_PARAM_ADDR]):
                         CSB_REG_RD32 (pParams[PMU_REG_POLL_PARAM_ADDR]);
    bReturn = (reg32 & pParams[PMU_REG_POLL_PARAM_MASK]) ==
                       pParams[PMU_REG_POLL_PARAM_VAL ];

    return FLD_TEST_DRF(_PMU, _REG_POLL, _OP, _EQ,
                            pParams[PMU_REG_POLL_PARAM_MODE]) ?
                         (LwBool) bReturn : !bReturn;
}

/*!
 * @brief   Exelwtes PMU pre-init.
 *
 * Any general initialization code not specific to particular engines should
 * be done here. This function must be called as early as possible.
 */
static FLCN_STATUS
s_pmuPreInit(void)
{
    FLCN_STATUS status = FLCN_OK;

    // DMEM aperture must be configured before any BAR0 access!!!
    pmuPreInitDmemAperture_HAL(&Pmu);

    // Chip info requires BAR0 access so must happen after DMEM aperture init!
    status = s_pmuChipInfoInitAndValidate();
    if (status != FLCN_OK)
    {
        goto s_pmuPreInit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_VERIFY_VERSION))
    {
        status = pmuVerifyVersion();
        if (status  != FLCN_OK)
        {
            PMU_HALT();
        }
    }

    pmuPreInitFlcnPrivLevelCacheAndConfigure_HAL(&Pmu);
    pmuPreInitDebugFeatures_HAL(&Pmu);
    pmuPreInitGPTimer_HAL(&Pmu);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SELWRITY_BYPASS))
    {
        pmuPreInitVbiosSelwrityBypassRegisterCache_HAL(&Pmu);
    }

    pmuPreInitWarForFecsBug1333388_HAL(&Pmu);
    pmuPreInitEnablePreciseExceptions_HAL(&Pmu);

    if (PMUCFG_FEATURE_ENABLED(PMU_FB_WAR_BUG_2805650))
    {
        fbWarBug2805650_HAL(&Pmu);
    }

s_pmuPreInit:
    return status;
}

static FLCN_STATUS
s_pmuChipInfoInitAndValidate(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CHIP_INFO))
    {
        pmuPreInitChipInfo_HAL(&Pmu);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_VERIFY_CHIP_FOR_PROFILE))
    {
        status = pmuPreInitVerifyChip_HAL(&Pmu);
    }

    return status;
}
