/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2gh100.c
 * @brief  SEC2 HAL Functions for GH100
 *
 * SEC2 HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "lwosselwreovly.h"
#include "sec2_cmdmgmt.h"
#include "dev_master.h"
#include "sec2_bar0.h"
#include "sec2_csb.h"
#include "dev_sec_csb.h"
#include "sec2_hs.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "sec2_bar0_hs.h"
#include "dev_master.h"
#include "sec2_hs.h"
#include "dev_top.h"
#include "sec2_chnmgmt.h"
#include "sec2_objic.h"
#include "dev_xtl_ep_pcfg_gpu.h"
#include "class/cl95a1.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_sec2_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
extern LwBool g_isRcRecoveryInProgress;
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

//
// PCI config space (LW_XVE_*) registers are accessed by adding 0x88000 to
// the BAR0 address.
//
#define LW_PCFG_OFFSET   0x88000

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */


/*
 * @brief Get the SW fuse version
 */
#define SEC2_GH100_UCODE_VERSION            (0x0)
#define SEC2_GH100_UCODE_VERSION_DEFAULT    (0x0)

FLCN_STATUS
sec2GetSWFuseVersionHS_GH100
(
    LwU32* pFuseVersion
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    if (OS_SEC_FALC_IS_DBG_MODE())
    {
        *pFuseVersion = SEC2_GH100_UCODE_VERSION_DEFAULT;
        return FLCN_OK;
    }
    else
    {
        CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PMC_BOOT_42, &chip));

        chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

        switch(chip)
        {
            case LW_PMC_BOOT_42_CHIP_ID_GH100 :
            {
                *pFuseVersion = SEC2_GH100_UCODE_VERSION;
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

/*
 * Check if Chip is Supported - LS
 */
FLCN_STATUS
sec2CheckIfChipIsSupportedLS_GH100(void)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;
    LwU32       chip;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PMC_BOOT_42, &chip));

    chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, chip);

    if((chip == LW_PMC_BOOT_42_CHIP_ID_GH100) ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_GH202) ||
       (chip == LW_PMC_BOOT_42_CHIP_ID_G000))
    {
        return FLCN_OK;
    }
    else
    {
        LwU32 platformType = 0;

        CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PTOP_PLATFORM, &platformType));
        platformType = DRF_VAL(_PTOP, _PLATFORM, _TYPE, platformType);

        if (platformType != LW_PTOP_PLATFORM_TYPE_SILICON)
        {
            if (!OS_SEC_FALC_IS_DBG_MODE())
            {
                return FLCN_ERR_PROD_MODE_NOT_YET_SUPPORTED;
            }
    
            return FLCN_OK;
        }
    }

    return FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED;

ErrorExit:
    return flcnStatus;
}

/*
 * @brief Perform Sec2 rc recovery step 8 as mentioned in 
 *        Hopper-187 RFD
 *
 */
void 
sec2ProcessEngineRcRecoveryCmd_GH100
(
    void
)
{
    // b) Sets an internal state to do trivial acks for ctxsw requests from now on
    g_isRcRecoveryInProgress = LW_TRUE;

    // c) Switch the host idle signal to _SW_IDLE.
    sec2HostIdleProgramIdle_HAL();

    // d) If a CTXSW request is pending, send trivial ack immediately
    if (icIsCtxSwIntrPending_HAL())
    {
        sec2SendCtxSwIntrTrivialAck();
    }
    // e) Unmasks the _CTXSW interrupt
    icHostIfIntrCtxswUnmask_HAL();
}

/*
 * @brief Triggers RC recovery process if method is recieved with no
 *        valid context. Refer Hopper-187 RFD
 *
 * returns LW_TRUE is RC recovery was initiated else LW_FALSE
 */
LwBool sec2TriggerRCRecoveryMthdRcvdForIlwalidCtx_GH100(void)
{
    LwU16 mthdId;
    LwU32 mthdData;

    if (!sec2IsLwrrentChannelCtxValid_HAL(&Sec2, NULL))
    {
        //
        // If context is not valid send msg to RM
        // 1. In the absence of a valid context, ucode must drain the fifo of all methods so that MTHD interrupt
        //    does not keep on firing
        //
        while (sec2PopMthd_HAL(&Sec2, &mthdId, &mthdData))
        {
            // NOP
        }

        // 2. Ucode will then clear the MTHD bit 
        icHostIfIntrUnmask_HAL();

        //
        // 3. Ucode will send raise an error using SWGEN0 and send it a ENG_MTHD_NO_CTX error code
        //    Directly Trigger RC recovery instead of sending new error code
        RCRecoverySendTriggerEvent(LW95A1_OS_ERROR_ILWALID_CTXSW_REQUEST);
        return LW_TRUE;
    } 
    else
    {
        return LW_FALSE;
    }
}

/*!
 * Reads DEVICE ID in HS mode
 */
FLCN_STATUS
sec2ReadDeviceIdHs_GH100(LwU32 *pDevId)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       devid      = 0;

    if (pDevId == NULL)
    {
        flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
        goto ErrorExit;
    }

    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PCFG_OFFSET + LW_EP_PCFG_GPU_ID, &devid));
    *pDevId = DRF_VAL(_EP_PCFG_GPU, _ID, _DEVICE, devid);
    devid = 0;

ErrorExit:
    return flcnStatus;
}
