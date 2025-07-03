/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispgp10xonly.c
 * @brief  HAL-interface for the GP10x Display Engine only.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_disp.h"
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_disp.h"  /// XXXEB RECHECK LIST HERE
#include "pmu_objdisp.h"
#include "pmu_objlsf.h"
#include "pmu_objfuse.h"
#include "pmu_objseq.h"
#include "pmu_objvbios.h"
#include "dev_fuse.h"
#include "lw_ref_dev_sr.h"
#include "pmu_i2capi.h"
#include "task_i2c.h"
#include "dev_pmgr.h"
#include "dev_trim.h"
#include "main.h"
#include "displayport/dpcd.h"
#include "config/g_i2c_private.h"
#include "config/g_disp_private.h"
#include "config/g_vbios_private.h"
#include "dev_ihub.h"

/* ------------------------- Type Definitions ------------------------------- */

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/* ---------------------------Private functions ----------------------------- */
static FLCN_STATUS s_dispRestoreOperationalClass_GP10X(void);
static void        s_dispEnableChannelInterrupts_GP10X(void);
static FLCN_STATUS s_dispRestoreCoreChannel_GP10X(void);
static FLCN_STATUS s_dispRestoreBaseChannel_GP10X(void);
static FLCN_STATUS s_dispModeset_GP10X(void);
static void        s_dispRestoreXBarSettings_GP10X(void);
static FLCN_STATUS s_dispPollForSvInterruptAndReset_GP10X(LwU32 dispSvIntrNum);
static void        s_dispResumeSvX_GP10X(void);

/*!
 * @brief  Returns if notebook Gsync is supported on this GPU
 *
 * @return LW_TRUE if the GPU supports Notebook DirectDrive, LW_FALSE otherwise
 */
LwBool
dispIsNotebookGsyncSupported_GP10X(void)
{
    LwU32 gpuDevIdObf;
    LwU32 fuseVal;

    if (Lsf.brssData.bIsValid &&
        FLD_TEST_DRF(_BRSS, _SEC_HULKDATA2_RM_FEATURE, _GSYNC, _ENABLE, Lsf.brssData.hulkData[2]))
    {
        // There is a valid HULK license
        return LW_TRUE;
    }

    // Check fuse BIT_INTERNAL_USE_ONLY
    fuseVal = fuseRead(LW_FUSE_OPT_INTERNAL_SKU);
    if (FLD_TEST_DRF_NUM(_FUSE, _OPT_INTERNAL_SKU, _DATA, 0x1, fuseVal))
    {
        return LW_TRUE;
    }

#define DEVID_OBFUSCATE(d) ((d) * 2 - 3)

    gpuDevIdObf = DEVID_OBFUSCATE(Pmu.gpuDevId);
    //
    // Check if the GPU is in the approved list for Notebook directDrive
    // Obfuscated so it has less chance to appear in clear in binary.
    //
    return ((gpuDevIdObf == DEVID_OBFUSCATE(0x1BE0)) || // GP104
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1BE1)) || // GP104
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1C60)) || // GP106
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1C61)) || // GP106
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1C62)) || // GP106
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1CCC)) || // GP107
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1CCD)));  // GP107
}


/*!
 * Gets offset of LW_PDISP_DSI_DEBUG_CTL, LW_PDISP_DSI_DEBUG_DAT
 *
 * @param[in] dispChnlNum  Display channel number.
 * @param[out] pDsiDebugCtl LW_PDISP_DSI_DEBUG_CTL offset.
 * @param[out] dsiDebugDat LW_PDISP_DSI_DEBUG_DAT offset.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if channel is invalid.
 * @return FLCN_OK otherwise.
 */
FLCN_STATUS
dispGetDebugCtlDatOffsets_GP10X
(
    LwU32   dispChnlNum,
    LwU32   *pDsiDebugCtl,
    LwU32   *pDsiDebugDat
)
{
    if (dispChnlNum >= LW_PDISP_DSI_DEBUG_CTL__SIZE_1)
    {
        // Unexpected channel number.
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Return Debug control and data registers.
    *pDsiDebugCtl = LW_PDISP_DSI_DEBUG_CTL(dispChnlNum);
    *pDsiDebugDat = LW_PDISP_DSI_DEBUG_DAT(dispChnlNum);

    return FLCN_OK;
}

/*!
 * @brief     Run the sequencer scripts which does modeset by PMU.
 *
 * @param[in] modesetCase   Perform BSOD modeset or SR exit only.
 *
 * @return    FLCN_OK is modeset completed.
 */
FLCN_STATUS
dispPmsModeSet_GP10X
(
    LwU32 modesetType,
    LwBool bTriggerSrExit
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, vbios)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispPms)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (Disp.pPmsBsodCtx == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto dispPmsModeSet_GP10X_exit;
        }

        switch (modesetType)
        {
            case DISP_MODESET_BSOD:
            {
                // Check VBIOS validity
                status = vbiosCheckValid(&Disp.pPmsBsodCtx->vbios.imageDesc);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_GP10X_exit;
                }

                // Restore XBAR settings
                s_dispRestoreXBarSettings_GP10X();

                // Restore SOR PWR settings
                status = dispRestoreSorPwrSettings_HAL(&Disp);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_GP10X_exit;
                }

                // Link training
                status = dispRunDpNoLinkTraining_HAL(&Disp);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_GP10X_exit;
                }

                // Restore Core Channel
                status = s_dispRestoreCoreChannel_GP10X();
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_GP10X_exit;
                }

                // Restore Base Channel
                status = s_dispRestoreBaseChannel_GP10X();
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_GP10X_exit;
                }

                s_dispEnableChannelInterrupts_GP10X();

                status = s_dispModeset_GP10X();
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_GP10X_exit;
                }

                //
                // Trigger sparse SR exit (Resync delay = 1).
                // Modeset and SR exit will run parallel here.
                //
                status = dispPsrDisableSparseSr_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_GP10X_exit;
                }

                break;
            }
            case DISP_MODESET_SR_EXIT:
            {
                // Disable Sparse SR
                status = dispPsrDisableSparseSr_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_GP10X_exit;
                }

                break;
            }
            case DISP_GC6_EXIT_MODESET:
            default:
            {
                // Should not get here.
                status = FLCN_ERR_ILWALID_STATE;
                goto dispPmsModeSet_GP10X_exit;
            }
        }

dispPmsModeSet_GP10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief  Restore display operational class
 *
 * @return FLCN_STATUS FLCN_OK if operational class restored
 */
static FLCN_STATUS
s_dispRestoreOperationalClass_GP10X(void)
{
    LwU32 lwPDispClass = 0;

    // Poll for LW_PDISP_CLASS_OKAY2UPDATE == LW_PDISP_CLASS_OKAY2UPDATE_YES
    if (!PMU_REG32_POLL_NS(LW_PDISP_CLASS,
        DRF_SHIFTMASK(LW_PDISP_CLASS_OKAY2UPDATE),
        DRF_DEF(_PDISP, _CLASS, _OKAY2UPDATE, _YES),
        LW_DISP_PMU_REG_POLL_PMS_TIMEOUT, PMU_REG_POLL_MODE_BAR0_EQ))
    {
        PMU_HALT();
        return FLCN_ERR_TIMEOUT;
    }

    //
    // Write LW_PDISP_CLASS_[HW/API/CLASS]_REV, and endianness
    // to the desired values, at the same time write
    // LW_PDISP_CLASS_UPDATE = LW_PDISP_CLASS_UPDATE_TRIGGER
    // (no need to do an extra write).
    //
    lwPDispClass = REG_RD32(BAR0, LW_PDISP_CLASS);

    lwPDispClass = FLD_SET_DRF_NUM(_PDISP, _CLASS, _HW_REV,
        DRF_VAL(_PDISP, _CLASS, _HW_REV, Disp.pPmsBsodCtx->coreChannel.swOperationalClassSet),
        lwPDispClass);
    lwPDispClass = FLD_SET_DRF_NUM(_PDISP, _CLASS, _API_REV,
        DRF_VAL(_PDISP, _CLASSES, _API_REV, Disp.pPmsBsodCtx->coreChannel.swOperationalClassSet),
        lwPDispClass);
    lwPDispClass = FLD_SET_DRF_NUM(_PDISP, _CLASS, _CLASS_REV,
        DRF_VAL(_PDISP, _CLASSES, _CLASS_REV, Disp.pPmsBsodCtx->coreChannel.swOperationalClassSet),
        lwPDispClass);
    lwPDispClass = FLD_SET_DRF_NUM(_PDISP, _CLASS, _ENDIANESS, Disp.pPmsBsodCtx->coreChannel.endianness, lwPDispClass);
    lwPDispClass = FLD_SET_DRF(_PDISP, _CLASS, _UPDATE, _TRIGGER, lwPDispClass);

    PMU_BAR0_WR32_SAFE(LW_PDISP_CLASS, lwPDispClass);

    // Poll for LW_PDISP_CLASS_UPDATE == LW_PDISP_CLASS_UPDATE_DONE
    if (!PMU_REG32_POLL_NS(LW_PDISP_CLASS,
        DRF_SHIFTMASK(LW_PDISP_CLASS_UPDATE),
        DRF_DEF(_PDISP, _CLASS, _UPDATE, _DONE),
        LW_DISP_PMU_REG_POLL_PMS_TIMEOUT, PMU_REG_POLL_MODE_BAR0_EQ))
    {
        PMU_HALT();
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}


/*!
 * @brief  Enable the interrupts for the core and base channels
 *
 * @return void
 */
static void
s_dispEnableChannelInterrupts_GP10X(void)
{
    LwU32       intrEn, i;

    //
    // First enable the AWAKEN and EXCEPTION interrupts.
    // Some might not be needed. To revisit when feature completed.
    //
    intrEn = REG_RD32(BAR0, LW_PDISP_DSI_RM_INTR_EN0_AWAKEN);
    intrEn = FLD_IDX_SET_DRF_DEF(_PDISP, _DSI_RM_INTR_EN0_AWAKEN, _CHN, LW_PDISP_907D_CHN_CORE, _ENABLE, intrEn);
    intrEn = FLD_IDX_SET_DRF_DEF(_PDISP, _DSI_RM_INTR_EN0_AWAKEN, _CHN, Disp.pPmsBsodCtx->baseChannel.num, _ENABLE, intrEn);
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_RM_INTR_EN0_AWAKEN, intrEn);

    intrEn = REG_RD32(BAR0, LW_PDISP_DSI_RM_INTR_EN0_EXCEPTION);
    intrEn = FLD_IDX_SET_DRF_DEF(_PDISP, _DSI_RM_INTR_EN0_EXCEPTION, _CHN, LW_PDISP_907D_CHN_CORE, _ENABLE, intrEn);
    intrEn = FLD_IDX_SET_DRF_DEF(_PDISP, _DSI_RM_INTR_EN0_EXCEPTION, _CHN, Disp.pPmsBsodCtx->baseChannel.num, _ENABLE, intrEn);
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_RM_INTR_EN0_EXCEPTION, intrEn);

    intrEn = REG_RD32(BAR0, LW_PDISP_DSI_RM_INTR_EN0_SV);

    // Enable supervisor intrs
    for (i = 0; i < LW_PDISP_DSI_RM_INTR_EN0_SV_SUPERVISOR__SIZE_1; ++i)
    {
        intrEn = FLD_IDX_SET_DRF(_PDISP, _DSI_RM_INTR_EN0_SV, _SUPERVISOR, i, _ENABLE, intrEn);
    }

    //
    // Enable vbios release/attention.
    // This might not be needed. To revisit when feature completed.
    //
    intrEn = FLD_SET_DRF(_PDISP, _DSI_RM_INTR_EN0_SV, _VBIOS_RELEASE, _ENABLE, intrEn);
    intrEn = FLD_SET_DRF(_PDISP, _DSI_RM_INTR_EN0_SV, _VBIOS_ATTENTION, _ENABLE, intrEn);

    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_RM_INTR_EN0_SV, intrEn);

    intrEn = REG_RD32(BAR0, LW_PDISP_DSI_RM_INTR_EN0_OR);

    // Enable SOR intrs
    for (i = 0; i < LW_PDISP_DSI_RM_INTR_EN0_OR_SOR__SIZE_1; ++i)
    {
        intrEn = FLD_IDX_SET_DRF(_PDISP, _DSI_RM_INTR_EN0_OR, _SOR, i, _ENABLE, intrEn);
    }

    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_RM_INTR_EN0_OR, intrEn);
}

/*!
 * @brief  Restore core channel
 *
 * @return FLCN_STATUS FLCN_OK if core channel restored
 */
static FLCN_STATUS
s_dispRestoreCoreChannel_GP10X(void)
{
    FLCN_STATUS status;
    LwU32       channelCtl;
    LwU32       pushBufCtl;

    // Set display owner
    status = dispSetDisplayOwner_HAL(&Disp);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Restore LW_PDISP_CLASS
    status = s_dispRestoreOperationalClass_GP10X();
    if (status != FLCN_OK)
    {
        return status;
    }

    // Restore LW_PDISP_DSI_INST_MEM0
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_INST_MEM0, Disp.pPmsBsodCtx->coreChannel.instMem0);

    // Restore LW_PDISP_SOR_CAP
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_SOR_CAP(Disp.pPmsBsodCtx->head.orIndex), Disp.pPmsBsodCtx->cap.sorCap);

    // Restore LW_PDISP_SYS_CAP
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_SYS_CAP, Disp.pPmsBsodCtx->cap.sysCap);

    // Restore LW_PDISP_DSI_LOCK_PIN_CAPA/B
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_LOCK_PIN_CAPA, Disp.pPmsBsodCtx->cap.pinCapA);
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_LOCK_PIN_CAPB, Disp.pPmsBsodCtx->cap.pinCapB);

    // Restore LW_PDISP_DSI_HEAD_CAPA/B/C/D
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_HEAD_CAPA(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.headCapA);
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_HEAD_CAPB(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.headCapB);
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_HEAD_CAPC(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.headCapC);
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_HEAD_CAPD(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.headCapD);


    // Restore core channel address
    pushBufCtl = DRF_DEF(_PDISP, _PBCTL0, _PUSHBUFFER_TARGET, _PHYS_LWM) | DRF_NUM(_PDISP, _PBCTL0, _PUSHBUFFER_START_ADDR, Disp.pPmsBsodCtx->coreChannel.pushbufFbPhys4K);
    PMU_BAR0_WR32_SAFE(LW_PDISP_PBCTL0(LW_PDISP_907D_CHN_CORE), pushBufCtl);
    PMU_BAR0_WR32_SAFE(LW_PDISP_PBCTL1(LW_PDISP_907D_CHN_CORE), DRF_NUM(_PDISP, _PBCTL1, _SUBDEVICE_ID, Disp.pPmsBsodCtx->coreChannel.subDeviceMask));
    PMU_BAR0_WR32_SAFE(LW_PDISP_PBCTL2(LW_PDISP_907D_CHN_CORE), DRF_NUM(_PDISP, _PBCTL2, _CLIENT_ID, Disp.pPmsBsodCtx->coreChannel.hClient & DRF_MASK(LW_UDISP_HASH_TBL_CLIENT_ID)));

    // Enable put pointer
    channelCtl = REG_RD32(BAR0, LW_PDISP_CHNCTL_CORE(LW_PDISP_907D_CHN_CORE));
    channelCtl = FLD_SET_DRF(_PDISP, _CHNCTL_CORE, _PUTPTR_WRITE, _ENABLE, channelCtl);
    PMU_BAR0_WR32_SAFE(LW_PDISP_CHNCTL_CORE(LW_PDISP_907D_CHN_CORE), channelCtl);

    // Set initial put pointer
    PMU_BAR0_WR32_SAFE(LW_UDISP_DSI_CHN_BASEADR(LW_PDISP_907D_CHN_CORE), 0);

    // Allocate the channel and connect to push buffer
    channelCtl = (DRF_DEF(_PDISP, _CHNCTL_CORE, _ALLOCATION, _ALLOCATE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _CONNECTION, _CONNECT) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _PUTPTR_WRITE, _ENABLE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _EFI, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _IGNORE_PI, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _SKIP_NOTIF, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _IGNORE_INTERLOCK, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _TRASH_MODE, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _CORE_DRIVER_RESUME, _DONE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _SAT_DRIVER_RESUME, _DONE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _GOTO_LIMBO2, _DONE) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _SHOW_DRIVER_STATE, _AT_UPD) |
        DRF_DEF(_PDISP, _CHNCTL_CORE, _INTR_DURING_SHTDWN, _DISABLE)
        );
    PMU_BAR0_WR32_SAFE(LW_PDISP_CHNCTL_CORE(LW_PDISP_907D_CHN_CORE), channelCtl);

    // Wait for core channel to be idle
    if (!PMU_REG32_POLL_NS(LW_PDISP_CHNCTL_CORE(0), DRF_SHIFTMASK(LW_PDISP_CHNCTL_CORE_STATE),
        REF_DEF(LW_PDISP_CHNCTL_CORE_STATE, _IDLE),
        LW_DISP_PMU_REG_POLL_PMS_TIMEOUT, PMU_REG_POLL_MODE_BAR0_EQ))
    {
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}


/*!
 * @brief  Restore base channel
 *
 * @return FLCN_STATUS FLCN_OK if base channel restored
 */
static FLCN_STATUS
s_dispRestoreBaseChannel_GP10X(void)
{
    LwU32       pushBufCtl;
    LwU32       channelCtl;
    LwU32       headId = Disp.pPmsBsodCtx->head.headId;
    LwU32       baseChannelNum = Disp.pPmsBsodCtx->baseChannel.num;

    // Restore base channel address
    pushBufCtl = DRF_DEF(_PDISP, _PBCTL0, _PUSHBUFFER_TARGET, _PHYS_LWM) | DRF_NUM(_PDISP, _PBCTL0, _PUSHBUFFER_START_ADDR, Disp.pPmsBsodCtx->baseChannel.pushBufAddr4K);
    PMU_BAR0_WR32_SAFE(LW_PDISP_PBCTL0(baseChannelNum), pushBufCtl);
    PMU_BAR0_WR32_SAFE(LW_PDISP_PBCTL1(baseChannelNum), DRF_NUM(_PDISP, _PBCTL1, _SUBDEVICE_ID, Disp.pPmsBsodCtx->baseChannel.subDeviceMask));
    PMU_BAR0_WR32_SAFE(LW_PDISP_PBCTL2(baseChannelNum), DRF_NUM(_PDISP, _PBCTL2, _CLIENT_ID, Disp.pPmsBsodCtx->baseChannel.hClient & DRF_MASK(LW_UDISP_HASH_TBL_CLIENT_ID)));

    // Enable put pointer
    channelCtl = REG_RD32(FECS, LW_PDISP_CHNCTL_BASE(headId));
    channelCtl = FLD_SET_DRF(_PDISP, _CHNCTL_BASE, _PUTPTR_WRITE, _ENABLE, channelCtl);
    PMU_BAR0_WR32_SAFE(LW_PDISP_CHNCTL_BASE(headId), channelCtl);

    // Set initial put pointer
    PMU_BAR0_WR32_SAFE(LW_UDISP_DSI_CHN_BASEADR(baseChannelNum), 0);

    // Allocate the channel and connect to push buffer
    channelCtl = (DRF_DEF(_PDISP, _CHNCTL_BASE, _ALLOCATION, _ALLOCATE) |
        DRF_DEF(_PDISP, _CHNCTL_BASE, _CONNECTION, _CONNECT) |
        DRF_DEF(_PDISP, _CHNCTL_BASE, _PUTPTR_WRITE, _ENABLE) |
        DRF_DEF(_PDISP, _CHNCTL_BASE, _IGNORE_PI, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_BASE, _SKIP_NOTIF, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_BASE, _SKIP_SEMA, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_BASE, _IGNORE_INTERLOCK, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_BASE, _IGNORE_FLIPLOCK, _DISABLE) |
        DRF_DEF(_PDISP, _CHNCTL_BASE, _TRASH_MODE, _DISABLE));
    PMU_BAR0_WR32_SAFE(LW_PDISP_CHNCTL_BASE(headId), channelCtl);

    // Wait for base channel to be idle
    if (!PMU_REG32_POLL_NS(LW_PDISP_CHNCTL_BASE(headId), DRF_SHIFTMASK(LW_PDISP_CHNCTL_BASE_STATE),
        REF_DEF(LW_PDISP_CHNCTL_BASE_STATE, _IDLE),
        LW_DISP_PMU_REG_POLL_PMS_TIMEOUT, PMU_REG_POLL_MODE_BAR0_EQ))
    {
        PMU_HALT();
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}

/*!
 * @brief  Trigger modeset and process
 *         supervisor interrupts.
 *
 * @return FLCN_OK if core channel restored
 */
static FLCN_STATUS
s_dispModeset_GP10X(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       headId = Disp.pPmsBsodCtx->head.headId;
    LwU32       baseChannelNum = Disp.pPmsBsodCtx->baseChannel.num;
    LwU32       dpFrequency10KHz = Disp.pPmsBsodCtx->dpInfo.linkSpeedMul * 2700;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVbios)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Update core channel put pointer
        PMU_BAR0_WR32_SAFE(LW_UDISP_DSI_PUT(LW_PDISP_907D_CHN_CORE), Disp.pPmsBsodCtx->coreChannel.putOffset);

        // Update core base put pointer
        PMU_BAR0_WR32_SAFE(LW_UDISP_DSI_PUT(baseChannelNum), Disp.pPmsBsodCtx->baseChannel.putOffset);

        // SV1
        status = s_dispPollForSvInterruptAndReset_GP10X(RM_PMU_DISP_INTR_SV_1);
        if (status != FLCN_OK)
        {
            goto s_dispModeset_GP10X_exit;
        }

        s_dispResumeSvX_GP10X();

        // SV2
        status = s_dispPollForSvInterruptAndReset_GP10X(RM_PMU_DISP_INTR_SV_2);
        if (status != FLCN_OK)
        {
            goto s_dispModeset_GP10X_exit;
        }

        // Restore VPLLs
        PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_VPLL_SSD0(headId), Disp.pPmsBsodCtx->vplls.regs.analog.ssd0);
        PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_VPLL_SSD1(headId), Disp.pPmsBsodCtx->vplls.regs.analog.ssd1);
        PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_VPLL_CFG(headId), Disp.pPmsBsodCtx->vplls.cfg);
        PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_VPLL_CFG2(headId), Disp.pPmsBsodCtx->vplls.regs.analog.cfg2);
        PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_VPLL_COEFF(headId), Disp.pPmsBsodCtx->vplls.coeff);

        // On SV2 IED script
        status = vbiosIedExelwteScriptTable(&Disp.pPmsBsodCtx->vbios.imageDesc,
            Disp.pPmsBsodCtx->vbios.iedScripts.onSv2,
            Disp.pPmsBsodCtx->vbios.conditionTableOffset,
            Disp.pPmsBsodCtx->dpInfo.portNum,
            Disp.pPmsBsodCtx->head.orIndex,
            Disp.pPmsBsodCtx->dpInfo.linkIndex,
            dpFrequency10KHz, 2, IED_TABLE_COMPARISON_GE);
        if (status != FLCN_OK)
        {
            goto s_dispModeset_GP10X_exit;
        }

        s_dispResumeSvX_GP10X();

        // SV3
        status = s_dispPollForSvInterruptAndReset_GP10X(RM_PMU_DISP_INTR_SV_3);
        if (status != FLCN_OK)
        {
            goto s_dispModeset_GP10X_exit;
        }

        // On SV3 IED script
        status = vbiosIedExelwteScriptTable(&Disp.pPmsBsodCtx->vbios.imageDesc,
            Disp.pPmsBsodCtx->vbios.iedScripts.onSv3,
            Disp.pPmsBsodCtx->vbios.conditionTableOffset,
            Disp.pPmsBsodCtx->dpInfo.portNum,
            Disp.pPmsBsodCtx->head.orIndex,
            Disp.pPmsBsodCtx->dpInfo.linkIndex,
            dpFrequency10KHz, 2, IED_TABLE_COMPARISON_GE);
        if (status != FLCN_OK)
        {
            goto s_dispModeset_GP10X_exit;
        }

        s_dispResumeSvX_GP10X();

s_dispModeset_GP10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief  Wait for and reset a supervisor interrupts
 *
 * @param[in] dispSvIntrNum    Supervisor number
 *
 * @return FLCN_STATUS FLCN_OK if interrupt is found
 */
static FLCN_STATUS
s_dispPollForSvInterruptAndReset_GP10X
(
    LwU32 dispSvIntrNum
)
{
    LwU32           data;
    LwU32           elapsedTime = 0;
    FLCN_TIMESTAMP  lwrrentTime;
    LwBool          bFound = LW_FALSE;

    osPTimerTimeNsLwrrentGet(&lwrrentTime);

    do
    {
        data = REG_RD32(BAR0, LW_PDISP_DSI_RM_INTR_SV);
        switch (dispSvIntrNum)
        {
        case RM_PMU_DISP_INTR_SV_1:
            if (FLD_TEST_DRF(_PDISP, _DSI_RM_INTR_SV, _SUPERVISOR1,
                _PENDING, data))
            {
                bFound = LW_TRUE;
            }
            break;
        case RM_PMU_DISP_INTR_SV_2:
            if (FLD_TEST_DRF(_PDISP, _DSI_RM_INTR_SV, _SUPERVISOR2,
                _PENDING, data))
            {
                bFound = LW_TRUE;
            }
            break;
        case RM_PMU_DISP_INTR_SV_3:
            if (FLD_TEST_DRF(_PDISP, _DSI_RM_INTR_SV, _SUPERVISOR3,
                _PENDING, data))
            {
                bFound = LW_TRUE;
            }
            break;
        default:
            return FLCN_ERR_ILWALID_ARGUMENT;
        }

        if (bFound || (elapsedTime > LW_DISP_TIMEOUT_FOR_SVX_INTR_NSECS))
        {
            break;
        }

        lwrtosYIELD();
        elapsedTime = osPTimerTimeNsElapsedGet(&lwrrentTime);
    } while (LW_TRUE);

    if (!bFound)
    {
        return FLCN_ERR_TIMEOUT;
    }

    data = DRF_IDX_DEF(_PDISP, _DSI_RM_INTR_SV, _SUPERVISOR, dispSvIntrNum - RM_PMU_DISP_INTR_SV_1, _RESET);
    PMU_BAR0_WR32_SAFE(LW_PDISP_DSI_RM_INTR_SV, data);

    return FLCN_OK;
}


/*!
 * @brief  Resume supervisor interrupts
 *
 * @return void
 */
static void
s_dispResumeSvX_GP10X(void)
{
    LwU32 svReg;

    PMU_BAR0_WR32_SAFE(LW_PDISP_SUPERVISOR_HEAD(Disp.pPmsBsodCtx->head.headId), 0);

    svReg = REG_RD32(BAR0, LW_PDISP_SUPERVISOR_MAIN);

    svReg = FLD_SET_DRF(_PDISP, _SUPERVISOR_MAIN, _SKIP_SECOND_INT, _NO, svReg);
    svReg = FLD_SET_DRF(_PDISP, _SUPERVISOR_MAIN, _SKIP_THIRD_INT, _NO, svReg);

    // Trigger the restart.
    svReg = FLD_SET_DRF(_PDISP, _SUPERVISOR_MAIN, _RESTART_MODE, _RESUME, svReg);
    svReg = FLD_SET_DRF(_PDISP, _SUPERVISOR_MAIN, _RESTART, _TRIGGER, svReg);

    PMU_BAR0_WR32_SAFE(LW_PDISP_SUPERVISOR_MAIN, svReg);
}

/*!
 * @brief  restore the Xbar settings
 *
 * @return void
 */
static void
s_dispRestoreXBarSettings_GP10X(void)
{
    LwU32 padlink;

    // Restore padlink state
    for (padlink = 0; padlink < Disp.pPmsBsodCtx->numPadlinks; padlink++)
    {
        if (BIT(padlink) & Disp.pPmsBsodCtx->padLinkMask)
        {
            PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_LINK_CTRL(padlink),
                Disp.pPmsBsodCtx->savedPadlinkState[padlink]);
        }
    }
}

/*!
 * @brief  Configure display SOR settings
 *
 * @param[in] void
 *
 * @return    void
 */
void
dispConfigureSorSettings_GP10X(void)
{
    LwU32 value;

    // put the SOR  to DP safe
    value = REG_RD32(FECS, LW_PDISP_CLK_REM_SOR(Disp.pPmsBsodCtx->head.orIndex));
    value = FLD_SET_DRF(_PDISP, _CLK_REM_SOR, _MODE_BYPASS, _DP_SAFE, value);
    value = FLD_SET_DRF(_PDISP, _CLK_REM_SOR, _CLK_SOURCE, _DIFF_DPCLK, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_SOR(Disp.pPmsBsodCtx->head.orIndex), value);

    // program its DP B/W and ...
    value = FLD_SET_DRF_NUM(_PDISP, _CLK_REM_SOR, _LINK_SPEED, Disp.pPmsBsodCtx->dpInfo.linkSpeedMul, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_SOR(Disp.pPmsBsodCtx->head.orIndex), value);

    // put SOR back to  _DP_NORMAL
    value = FLD_SET_DRF(_PDISP, _CLK_REM_SOR, _MODE_BYPASS, _DP_NORMAL, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_CLK_REM_SOR(Disp.pPmsBsodCtx->head.orIndex), value);
}
