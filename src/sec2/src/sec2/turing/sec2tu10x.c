/*
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2tu10x.c
 * @brief  SEC2 HAL Functions for TU10X
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_ram.h"
#include "lwosselwreovly.h"
#include "dev_master.h"
#include "sec2_bar0.h"
#include "sec2_csb.h"
#include "sec2_hs.h"
#include "dev_fuse.h"
#include "dev_bus.h"
#include "mscg_wl_bl_init.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "sec2_bar0_hs.h"
#include "config/g_sec2_private.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define MSCG_POLL_DELAY_TICKS 64
#define MSCG_POLL_TIMEOUT_NS 1000000000

//
// This is added to fix SEC2 and PMU preTuring and Ampere_and_later ACRLIB builds
// TODO: Get this define added in Ampere_and_later manuals
// HW bug tracking priv latency improvement - 2117032
// Current value of this timeout is taken from http://lwbugs/2198976/285
//
#ifndef LW_CSEC_BAR0_TMOUT_CYCLES_PROD
#define LW_CSEC_BAR0_TMOUT_CYCLES_PROD    (0x01312d00)
#endif //LW_CSEC_BAR0_TMOUT_CYCLES_PROD

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */

/*!
 * @brief Get the physical DMA index based on the target specified.
 *
 * @return Physical DMA index
 */
LwU8
sec2GetPhysDmaIdx_TU10X
(
    LwU8 target
)
{
    // Ensure that the context targets match expected.
    ct_assert(LW_RAMIN_ENGINE_WFI_TARGET_LOCAL_MEM ==
              LW_CSEC_FALCON_LWRCTX_CTXTGT_LOCAL_FB);

    ct_assert(LW_RAMIN_ENGINE_WFI_TARGET_SYS_MEM_COHERENT ==
              LW_CSEC_FALCON_LWRCTX_CTXTGT_COHERENT_SYSMEM);

    ct_assert(LW_RAMIN_ENGINE_WFI_TARGET_SYS_MEM_NONCOHERENT ==
              LW_CSEC_FALCON_LWRCTX_CTXTGT_NONCOHERENT_SYSMEM);

    switch (target)
    {
        case LW_RAMIN_ENGINE_WFI_TARGET_LOCAL_MEM:
        {
            return RM_SEC2_DMAIDX_GUEST_PHYS_VID_BOUND;
        }
        case LW_RAMIN_ENGINE_WFI_TARGET_SYS_MEM_COHERENT:
        {
            return RM_SEC2_DMAIDX_GUEST_PHYS_SYS_COH_BOUND;
        }
        case LW_RAMIN_ENGINE_WFI_TARGET_SYS_MEM_NONCOHERENT:
        {
            return RM_SEC2_DMAIDX_GUEST_PHYS_SYS_NCOH_BOUND;
        }
        default:
        {
            //
            // Unknown physical target. As long as host behaves properly, we
            // should never get here.
            //
            SEC2_HALT();

            // We can never get here, but need a return to keep compiler happy.
            return RM_SEC2_DMAIDX_GUEST_PHYS_VID_BOUND;
        }
    }
}

/*
 * @brief Get the SW fuse version
 */
#define SEC2_TU10X_UCODE_VERSION    (0x3)
FLCN_STATUS
sec2GetSWFuseVersionHS_TU10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    if (OS_SEC_FALC_IS_DBG_MODE())
    {
        *pFuseVersion = SEC2_TU10X_UCODE_VERSION;
        return FLCN_OK;
    }
    else
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));

        chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

        switch(chip)
        {
            case LW_PMC_BOOT_42_CHIP_ID_TU102:
            case LW_PMC_BOOT_42_CHIP_ID_TU104:
            case LW_PMC_BOOT_42_CHIP_ID_TU106:
            {
                *pFuseVersion = SEC2_TU10X_UCODE_VERSION;
                return FLCN_OK;
                break;
            }
            default:
            {
                return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
                break;
            }
        }
    }

ErrorExit:
    return flcnStatus;
}


/*!
 * @brief Make sure the chip is allowed to do Playready
 *
 * Note that this function should never be prod signed for a chip that doesn't
 * have a silicon.
 *
 * @returns  FLCN_OK if chip is allowedto do Playready
 *           FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED otherwise
 */
FLCN_STATUS
sec2EnforceAllowedChipsForPlayreadyHS_TU10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);
    switch(chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
        case LW_PMC_BOOT_42_CHIP_ID_TU106:
        case LW_PMC_BOOT_42_CHIP_ID_TU116:
        case LW_PMC_BOOT_42_CHIP_ID_TU117:
            return FLCN_OK;
        default:
            return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    }

ErrorExit:
    return flcnStatus;
}

/*
 * @brief Ensure the ucode will not be run on dev version boards
 * Please refer to Bug 200333620 for detailed info
 */
FLCN_STATUS
sec2DisallowDevVersionHS_TU10X(void)
{
    LwU32       devVersion;
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_DEV_VERSION_ON_PROD;
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_FUSE_UCODE_PR_VPR_REV, &devVersion));

    if (devVersion != 0)
    {
        flcnStatus = FLCN_OK;
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get the HW fuse version, this is LS function
 */
FLCN_STATUS
sec2GetHWFuseVersion_TU10X
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILWALID_ARGUMENT;
    if(!pFuseVersion)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_FUSE_OPT_FUSE_UCODE_SEC2_REV, pFuseVersion));
    *pFuseVersion = DRF_VAL(_FUSE, _OPT_FUSE_UCODE_SEC2_REV, _DATA, *pFuseVersion);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief: Program Bar0 timeout value greater than Host timeout.
  *
 */
void
sec2ProgramBar0Timeout_TU10X(void)
{
      REG_WR32(CSB, LW_CSEC_BAR0_TMOUT,
              DRF_NUM(_CSEC, _BAR0_TMOUT, _CYCLES, LW_CSEC_BAR0_TMOUT_CYCLES_PROD));
}

#if (SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_TEST))
/*
 * @brief Waits for MSCG to engage.
 *
 * Checks if MSCG is engaged or not. If it's not engaged then it keeps looping
 * till MSCG gets engaged.
 *
 * Poll timeout is set to MSCG_POLL_TIMEOUT_NS, and we poll once every
 * MSCG_POLL_DELAY_TICKS ticks.
 *
 * @returns    FLCN_OK if MSCG is entered, FLCN_ERROR if MSCG entry timed out,
 *             and whatever *_REG_RD32_ERRCHK returns if we can't read register,
 *             usually FLCN_ERR_BAR0_PRIV_READ_ERROR/FLCN_ERR_CSB_PRIV_READ_ERROR
 */
FLCN_STATUS
sec2WaitMscgEngaged_TU10X(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_TIMEOUT;
    FLCN_TIMESTAMP timeStart;
    LwU32 privBlockerStatus;
    osPTimerTimeNsLwrrentGet(&timeStart);
    do
    {
        CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CSEC_PRIV_BLOCKER_CTRL, &privBlockerStatus));
        if (FLD_TEST_DRF(_CSEC, _PRIV_BLOCKER_CTRL, _BLOCKMODE, _BLOCK_MS, privBlockerStatus) ||
            FLD_TEST_DRF(_CSEC, _PRIV_BLOCKER_CTRL, _BLOCKMODE, _BLOCK_GR_MS, privBlockerStatus))
        {
            flcnStatus = FLCN_OK;
            break;
        }
        lwrtosLwrrentTaskDelayTicks(MSCG_POLL_DELAY_TICKS);
    }
    while (osPTimerTimeNsElapsedGet(&timeStart) < MSCG_POLL_TIMEOUT_NS);

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Reads a Clock-gated domain register to wake-up MSCG from sleep.
 *
 * Read a clock gated domain register, when the system is in MSCG,
 * so that the system comes out of MSCG.
 *
 * @param[out] *pDenylistedRegVal the value of the register read, which is
 *                                 supposed to trigger MSCG exit.
 *
 * @returns    FLCN_OK on successful register read
 *             and whatever *_REG_RD32_ERRCHK returns if we can't read register,
 *             usually FLCN_ERR_BAR0_PRIV_READ_ERROR/FLCN_ERR_CSB_PRIV_READ_ERROR
 */
FLCN_STATUS
sec2MscgDenylistedRegRead_TU10X
(
    LwU32 *pDenylistedRegVal
)
{
    if (pDenylistedRegVal == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    return BAR0_REG_RD32_ERRCHK(PRIV_BLOCKER_MS_BL_RANGE_START_4, pDenylistedRegVal);
}
#endif // (SEC2CFG_FEATURE_ENABLED(SEC2JOB_MSCG_TEST))

LwU32
sec2GetNextCtxGfid_TU10X(void)
{
    return REG_FLD_RD_DRF(CSB, _CSEC, _FALCON_NXTCTX2, _CTXGFID);
}

/*!
 * @brief Acquires ownership over the GP timer, initializes, and enables the timer.
 *        RT timer must be used for OS ticks, in order to use the GP timer.
 *
 * @param[in] timerInterval     Value that GP timer will initialize to,
 *                              and reset to when count hits zero.
 * @param[in] bUseEngClockTicks Flag to use falcon engine clock as source of
 *                              ticks, rather than ptimer.
 * @param[in] mutexTimeout      Amount of ticks to wait for GP timer mutex
 *                              to be acquired. Returns failure on timeout.
 *
 * @returns    FLCN_OK on successful acquire and initialization of timer.
 *             FLCN_ERR_MUTEX_ACQUIRED on timeout trying to acquire semaphore.
 *             FLCN_ERR_NOT_SUPPORTED if GP timer is being used by OS and is
 *             not available, or if usage is otherwise disabled.
 */
FLCN_STATUS
sec2GptmrEnable_TU10X
(
    LwU32  timerInterval,
    LwBool bUseEngClockTicks,
    LwU32  mutexTimeout
)
{
    LwU32 gptmrValData  = 0;
    LwU32 gptmrIntData  = 0;
    LwU32 gptmrCtrlData = 0;

    if (!SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS) ||
        !GptimerLibEnabled)
    {
        // Either OS owns timer, or semaphore init failed
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Try and acquire mutex
    if (lwrtosSemaphoreTake(GptimerLibMutex, mutexTimeout) != FLCN_OK)
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    // We now own GP timer, ensure disabled for configuration change.
    gptmrCtrlData = REG_RD32(CSB, LW_CSEC_FALCON_GPTMRCTL);
    gptmrCtrlData = FLD_SET_DRF(_CSEC, _FALCON_GPTMRCTL,
                                _GPTMREN, _DISABLE, gptmrCtrlData);
    REG_WR32(CSB, LW_CSEC_FALCON_GPTMRCTL, gptmrCtrlData);

    // Force timer reload of interval value.
    gptmrValData = REG_RD32(CSB, LW_CSEC_FALCON_GPTMRVAL);
    gptmrValData = FLD_SET_DRF_NUM(_CSEC, _FALCON_GPTMRVAL,
                                   _VAL, timerInterval, gptmrValData);
    REG_WR32(CSB, LW_CSEC_FALCON_GPTMRVAL, gptmrValData);

    // Ensure we use new interval value going forward.
    gptmrIntData = REG_RD32(CSB, LW_CSEC_FALCON_GPTMRINT);
    gptmrIntData = FLD_SET_DRF_NUM(_CSEC, _FALCON_GPTMRINT,
                                   _VAL, timerInterval, gptmrIntData);
    REG_WR32(CSB, LW_CSEC_FALCON_GPTMRINT, gptmrIntData);

    // Configure timer input source
    gptmrCtrlData = bUseEngClockTicks ?
            FLD_SET_DRF(_CSEC, _FALCON_GPTMRCTL,
                        _GPTMR_SRC_MODE, _ENGCLK, gptmrCtrlData) :
            FLD_SET_DRF(_CSEC, _FALCON_GPTMRCTL,
                        _GPTMR_SRC_MODE, _PTIMER, gptmrCtrlData);

    // Enable timer
    gptmrCtrlData = FLD_SET_DRF(_CSEC, _FALCON_GPTMRCTL,
                                _GPTMREN, _ENABLE, gptmrCtrlData);
    REG_WR32(CSB, LW_CSEC_FALCON_GPTMRCTL, gptmrCtrlData);

    return FLCN_OK;
}

/*!
 * @brief Reads the current value of the GP timer.
 * 
 * @returns Value of GPTMRVAL register.
 */
LwU32
sec2GptmrReadValue_TU10X(void)
{
    return REG_RD32(CSB, LW_CSEC_FALCON_GPTMRVAL);
}

/*!
 * @brief Resets count back to interval value stored in GPTMRINT.
 * 
 *        GptimerLibMutex MUST be held, via prior successful call to
 *        sec2GptmrEnable_TU10X.
 * 
 * @returns FLCN_OK on successful interval reset.
 *          FLCN_ERR_NOT_SUPPORTED if GP timer is in use by OS,
 *          or semaphore initialization failed.
 */
FLCN_STATUS
sec2GptmrResetInterval_TU10X(void)
{
    LwU32 gptmrValData  = 0;
    LwU32 gptmrIntData  = 0;
    LwU32 gptmrCtlData  = 0;
    LwU32 timerInterval = 0;

    if (!SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS) ||
        !GptimerLibEnabled)
    {
        // Either OS owns timer, or semaphore init failed
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Disable GP timer for configuration change
    gptmrCtlData = REG_RD32(CSB, LW_CSEC_FALCON_GPTMRCTL);
    gptmrCtlData = FLD_SET_DRF(_CSEC, _FALCON_GPTMRCTL,
                               _GPTMREN, _DISABLE, gptmrCtlData);
    REG_WR32(CSB, LW_CSEC_FALCON_GPTMRCTL, gptmrCtlData);

    // Get current timer interval, then reset timer count to interval
    gptmrIntData  = REG_RD32(CSB, LW_CSEC_FALCON_GPTMRINT);
    timerInterval = DRF_VAL(_CSEC, _FALCON_GPTMRINT, _VAL, gptmrIntData);

    gptmrValData = REG_RD32(CSB, LW_CSEC_FALCON_GPTMRVAL);
    gptmrValData = FLD_SET_DRF_NUM(_CSEC, _FALCON_GPTMRVAL,
                                   _VAL, timerInterval, gptmrValData);
    REG_WR32(CSB, LW_CSEC_FALCON_GPTMRVAL, gptmrValData);

    // Re-enable GP timer
    gptmrCtlData = FLD_SET_DRF(_CSEC, _FALCON_GPTMRCTL,
                               _GPTMREN, _ENABLE, gptmrCtlData);
    REG_WR32(CSB, LW_CSEC_FALCON_GPTMRCTL, gptmrCtlData);

    return FLCN_OK;
}

/*!
 * @brief Disables GP timer and releases ownership of mutex held.
 * 
 *        GptimerLibMutex MUST be held, via prior successful call to
 *        sec2GptmrEnable_TU10X.
 * 
 * @returns FLCN_OK on successful disabling of GP timer.
 *          FLCN_ERR_NOT_SUPPORTED if GP timer in use by OS,
 *          or mutex failed to initialize.
 */
FLCN_STATUS
sec2GptmrDisable_TU10X(void)
{
    if (!SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS) ||
        !GptimerLibEnabled)
    {
        // either OS owns timer, or semaphore init failed
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Just set disable flag - next user will initialize configuration.
    LwU32 gptmrCtlData = REG_RD32(CSB, LW_CSEC_FALCON_GPTMRCTL);
    gptmrCtlData = FLD_SET_DRF(_CSEC, _FALCON_GPTMRCTL,
                               _GPTMREN, _DISABLE, gptmrCtlData);
    REG_WR32(CSB, LW_CSEC_FALCON_GPTMRCTL, gptmrCtlData);

    // Release mutex
    lwrtosSemaphoreGive(GptimerLibMutex);

    return FLCN_OK;
}
