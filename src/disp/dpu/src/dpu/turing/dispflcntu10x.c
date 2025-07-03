/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dispflcntu10x.c
 * @brief  GSP(TU10X) Hal Functions
 *
 * GSP HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_master.h"
#include "dev_fifo.h"
#include "dev_gsp_csb.h"
#include "dev_bus.h"
#include "mscg_wl_bl_init.h"
#include "dispflcn_regmacros.h"
#include "dev_fuse.h"

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "gsp_bar0_hs.h"
#include "gsp_csb.h"
#include "lwosselwreovly.h"

/* ------------------------- Type Definitions ------------------------------- */
#define GSP_TU10X_UCODE_VERSION                (0x1)
// GSP ucode version 0x7 is blacklisted for turing
#define HDCP_BLACKLISTED_TURING_SKU_GSP_FUSE_VERSION  (0x7)

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define MSCG_POLL_DELAY_TICKS 64
#define MSCG_POLL_TIMEOUT_NS 1000000000

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*
 * @brief Program timeout value for BAR0 transactions
 */
void
dpuBar0TimeoutInit_TU10X(void)
{
    //
    // Initialize the timeout value within which the host has to ack a read or
    // write transaction.
    //
    // BUG 2198976: increase timeout to 10ms. On R400 this will be a magic number.
    // The value is callwlated from a maximum LWDCLK of 2000MHz
    // 10 / 1000 / (1/LWDCLK_MAX) = 20000000
    //
    // TODO: Ampere is using Turing value but HW needs to re-evaluate value after
    //       FNL.
    //
    DENG_REG_WR32(BAR0_TMOUT,
             DENG_DRF_NUM(BAR0_TMOUT, _CYCLES, LW_CGSP_BAR0_TMOUT_CYCLES_PROD ));
}

/*
 * @brief Make sure the chip is allowed to do HDCP
 */
void
dpuEnforceAllowedChipsForHdcpHS_TU10X(void)
{
    LwU32 chip;
    LwU32 data32 = 0;

    if (FLCN_OK != EXT_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &data32))
    {
        DPU_HALT();
    }

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);

    if ((chip != LW_PMC_BOOT_42_CHIP_ID_TU102) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_TU104) &&
        (chip != LW_PMC_BOOT_42_CHIP_ID_TU106))
    {
        DPU_HALT();
    }
}

/*!
 * @brief Get the SW Ucode version
 */
FLCN_STATUS
dpuGetUcodeSWVersionHS_TU10X(LwU32* pUcodeVersion)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       chip;
    LwU32       data32;

    if (pUcodeVersion == NULL)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &data32));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, data32);

    if (chip == LW_PMC_BOOT_42_CHIP_ID_TU102 ||
        chip == LW_PMC_BOOT_42_CHIP_ID_TU104 ||
        chip == LW_PMC_BOOT_42_CHIP_ID_TU106)
    {
        *pUcodeVersion = GSP_TU10X_UCODE_VERSION;
    }
    else
    {
        if (OS_SEC_FALC_IS_DBG_MODE())
        {
            *pUcodeVersion = GSP_TU10X_UCODE_VERSION;
        }
        else
        {
            flcnStatus = FLCN_ERR_HS_PROD_MODE_NOT_YET_SUPPORTED;
        }
    }

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief Get the HW fuse version
 */
FLCN_STATUS
dpuGetHWFuseVersionHS_TU10X(LwU32* pFuseVersion)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       data32;

    if (pFuseVersion == NULL)
    {
        flcnStatus = FLCN_ERR_HS_CHK_ILWALID_INPUT;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(EXT_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_FUSE_UCODE_GSP_REV, &data32));
    *pFuseVersion = DRF_VAL(_FUSE, _OPT_FUSE_UCODE_GSP_REV, _DATA, data32);

    //
    // This is extra check to make sure HDCP22 doesn't run on TU102-DEV-A1 SKU
    // Where HDCP needs to be disabled.
    // GSP FUSE verion for the this SKU is made 7, so that it can be blacklisted
    //
    if(*pFuseVersion == HDCP_BLACKLISTED_TURING_SKU_GSP_FUSE_VERSION)
    {
        flcnStatus = FLCN_ERR_HS_CHK_HDCP_BLACKLISTED_SKU;
        goto ErrorExit;
    }

ErrorExit:
    return flcnStatus;
}

#if (DPUCFG_FEATURE_ENABLED(DPUJOB_MSCG_TEST))
#if (DPUCFG_FEATURE_ENABLED(DPUJOB_MSCG_FBBLOCKER_TEST))
/*!
 * @brief: Issue FB access when GSP is in MSCG
 *
 * @param[in] offset:    FB offset (low 32 bits)
 * @param[in] offsetH:   FB offset (upper 32 bits)
 * @param[in] fbOp:      ISSUE_READ: issue read
 *                       ISSUE_WRITE: Issue write
 * @return:
 *     ISSUE_READ:  FLCN_OK if FB region specified contains test pattern
 *                  FLCN_ERR_DMA_GENERIC if pattern inaccessible or wrong.
 *     ISSUE_WRITE: FLCN_OK if write of test pattern to FB region succeeds
 *                  FLCN_ERR_DMA_GENERIC if dma write to FB region fails.
 */
FLCN_STATUS
dpuMscgIssueFbAccess_TU10X
(
    LwU32 offset,
    LwU32 offsetH,
    LwU8  fbOp
)
{
    LwU8             buf[MSCG_ISSUE_FB_SIZE+DMA_MIN_READ_ALIGNMENT];
    LwU8             *pBuf = (LwU8 *)LW_ALIGN_UP((LwUPtr)buf, DMA_MIN_READ_ALIGNMENT);
    LwU32            index;
    RM_FLCN_MEM_DESC memDesc;

    memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_GSP_DMAIDX_PHYS_VID_FN0 , 0);
    memDesc.address.lo = offset;
    memDesc.address.hi = offsetH;

    // To reduce stack pressure, we wait for MSCG to engage before calling this function.

    if (fbOp == MSCG_ISSUE_FB_ACCESS_WRITE)
    {
        for (index = 0; index < MSCG_ISSUE_FB_SIZE; index++)
        {
            pBuf[index] = (LwU8) index;
        }
        if (dmaWrite(pBuf, &memDesc, 0, MSCG_ISSUE_FB_SIZE) != FLCN_OK)
        {
             return FLCN_ERR_DMA_GENERIC;
        }
    }
    else
    {
        if (dmaRead(pBuf, &memDesc, 0, MSCG_ISSUE_FB_SIZE) != FLCN_OK)
        {
            return FLCN_ERR_DMA_GENERIC;
        }
        else
        {
            for (index = 0; index < MSCG_ISSUE_FB_SIZE; index++)
            {
                if (pBuf[index] != (LwU8) index)
                {
                    return FLCN_ERR_DMA_GENERIC;
                }
            }
        }
    }

    return FLCN_OK;
}
#endif  // (DPUCFG_FEATURE_ENABLED(DPUJOB_MSCG_FBBLOCKER_TEST))

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
dpuWaitMscgEngaged_TU10X(void)
{
    FLCN_STATUS     flcnStatus = FLCN_ERR_TIMEOUT;
    FLCN_TIMESTAMP  timeStart;
    LwU32           privBlockerStatus;

    osPTimerTimeNsLwrrentGet(&timeStart);
    do
    {
        CHECK_FLCN_STATUS(CSB_REG_RD32_ERRCHK(LW_CGSP_PRIV_BLOCKER_CTRL, &privBlockerStatus));
        if (FLD_TEST_DRF(_CGSP, _PRIV_BLOCKER_CTRL, _BLOCKMODE, _BLOCK_MS, privBlockerStatus) ||
            FLD_TEST_DRF(_CGSP, _PRIV_BLOCKER_CTRL, _BLOCKMODE, _BLOCK_GR_MS, privBlockerStatus))
        {
            flcnStatus = FLCN_OK;
            break;
        }
        CHECK_FLCN_STATUS(lwrtosLwrrentTaskDelayTicks(MSCG_POLL_DELAY_TICKS));
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
 * @param[out] *pBlacklistedRegVal the value of the register read, which is
 *                                 supposed to trigger MSCG exit.
 *
 * @returns    FLCN_OK on register read, whether it succeeds or not.
 */
FLCN_STATUS
dpuMscgBlacklistedRegRead_TU10X
(
    LwU32 *pBlacklistedRegVal
)
{
    if (pBlacklistedRegVal == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    //GSP does not have error checking BAR0 read.
    *pBlacklistedRegVal = EXT_REG_RD32(PRIV_BLOCKER_MS_BL_RANGE_START_4);
    return FLCN_OK;
}
#endif // (DPUCFG_FEATURE_ENABLED(DPUJOB_MSCG_TEST))
