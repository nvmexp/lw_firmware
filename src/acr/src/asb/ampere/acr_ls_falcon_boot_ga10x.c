/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_ls_falcon_boot_tu10x.c
 */
#include "acr.h"
#include "acrdrfmacros.h"
#include "bootrom_pkc_parameters.h"
#include "dev_sec_pri.h"
#include "dev_falcon_second_pri.h"
#include "dev_falcon_v4.h"
#include "dev_falcon_v4_addendum.h"
#include "dev_falcon_second_pri.h"
#include "dev_fbif_v4.h"
#include "dev_sec_pri.h"
#include "dev_lwdec_pri.h"
#include "dev_gsp.h"
#include "dev_top.h"
#include "dev_pwr_pri.h"
#include "dev_fbfalcon_pri.h"
#include "hwproject.h"
#include "dev_smcarb_addendum.h"
#include "dev_gsp_addendum.h"
#include "dev_graphics_nobundle.h"
#include "dev_riscv_pri.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_gpc_addendum.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

#ifdef BOOT_FROM_HS_BUILD
#include "dev_sec_addendum.h"
#include "dev_gsp_addendum.h"
#include "dev_lwdec_addendum.h"
#endif

extern ACR_DMA_PROP  g_dmaProp;

#ifdef NEW_WPR_BLOBS
extern LwU8  g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
#endif // NEW_WPR_BLOBS

#ifdef BOOT_FROM_HS_BUILD
static struct pkc_verification_parameters g_HsFmcSignatureVerificationParams ATTR_OVLY(".data");
HS_FMC_PARAMS g_hsFmcParams;
#endif // BOOT_FROM_HS_BUILD
/*!
 * @brief Set up SEC2-specific registers
 */
void
acrlibSetupSec2Registers_GA10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32 plmValue = 0;
    ACR_REG_WR32(BAR0, LW_PSEC_FBIF_CTL2_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    ACR_REG_WR32(BAR0, LW_PSEC_BLOCKER_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);

    // Setting LW_PSEC_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK to ACR_PLMASK_READ_L0_WRITE_L2
    plmValue = ACR_REG_RD32(BAR0, LW_PSEC_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PSEC, _CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                                           _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PSEC, _CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                                           _DISABLE, plmValue);
    ACR_REG_WR32(BAR0, LW_PSEC_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, plmValue);

    // Setting LW_PSEC_CMD_INTREN_PRIV_LEVEL_MASK to ACR_PLMASK_READ_L0_WRITE_L2
    plmValue = ACR_REG_RD32(BAR0, LW_PSEC_CMD_INTREN_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PSEC, _CMD_INTREN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                                           _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PSEC, _CMD_INTREN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                                           _DISABLE, plmValue);
    ACR_REG_WR32(BAR0, LW_PSEC_CMD_INTREN_PRIV_LEVEL_MASK, plmValue);

    // Setting LW_PSEC_TIMER_0_INTERVAL_PRIV_LEVEL_MASK to ACR_PLMASK_READ_L0_WRITE_L2
    plmValue = ACR_REG_RD32(BAR0, LW_PSEC_TIMER_0_INTERVAL_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PSEC, _TIMER_0_INTERVAL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                                           _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PSEC, _TIMER_0_INTERVAL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                                           _DISABLE, plmValue);
    ACR_REG_WR32(BAR0, LW_PSEC_TIMER_0_INTERVAL_PRIV_LEVEL_MASK, plmValue);

    // Setting LW_PSEC_TIMER_CTRL_PRIV_LEVEL_MASK to ACR_PLMASK_READ_L0_WRITE_L2
    plmValue = ACR_REG_RD32(BAR0, LW_PSEC_TIMER_CTRL_PRIV_LEVEL_MASK);
    plmValue = FLD_SET_DRF(_PSEC, _TIMER_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1,
                                           _DISABLE, plmValue);
    plmValue = FLD_SET_DRF(_PSEC, _TIMER_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                                           _DISABLE, plmValue);
    ACR_REG_WR32(BAR0, LW_PSEC_TIMER_CTRL_PRIV_LEVEL_MASK, plmValue);
}

/*!
 * @brief Setup PLMs of target falcons.
 *        PLMs raised per falcon are
 *        For all falcons setting FALCON_PRIVSTATE_PRIV_LEVEL_MASK to Level0 Read and Level3 Write
 *        GSP      -
 *                     LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK,               Level0 Read and Level2 Write
 *                     LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK,              Level0 Read and Level2 Write
 *                     LW_PGSP_IRQTMR_PRIV_LEVEL_MASK                      Level0 Read and Level2 Write
 *
 *        LWDEC    -   LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK              Level0 Read and Level2 Write
 *
 *        PMU      -   LW_PPWR_FALCON_DIODT_PRIV_LEVEL_MASK                Level0 Read and Level2 Write
 *
 *        SEC2     -
 *                     LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK                Level0 Read and Level2 Write
 *                     LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK               Level0 Read and Level2 Write
 *
 *        FBFALCON -   LW_PFBFALCON_FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK,  Level0 Read and Level2 Write
 *
 *
 * @param[out] pFlcnCfg   Structure to hold falcon config
 */
ACR_STATUS
acrlibSetupTargetFalconPlms_GA10X
(
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    LwU32 plmVal = 0;
    //
    // TODO: For ACR_PLMASK_READ_L0_WRITE_L3, either
    // 1. Use DRF_DEF in this RegWrite code  OR
    // 2. Add lots of ct_asserts + build that define using one of the registers using DRF_DEF
    // This is to be done at all paces where this define is used
    //
    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_PRIVSTATE_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L3);

    switch(pFlcnCfg->falconId)
    {
        case LSF_FALCON_ID_PMU:
            plmVal = ACR_REG_RD32(BAR0, LW_PPWR_FALCON_DIODT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PPWR, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PPWR, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PPWR_FALCON_DIODT_PRIV_LEVEL_MASK, plmVal);
            break;

        case LSF_FALCON_ID_LWDEC:
#ifndef BOOT_FROM_HS_BUILD
            plmVal = ACR_REG_RD32(BAR0, LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK(0));
            plmVal = FLD_SET_DRF(_PLWDEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PLWDEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PLWDEC_FALCON_DIODT_PRIV_LEVEL_MASK(0), plmVal);
#endif // ifndef BOOT_FROM_HS_BUILD
            break;

        case LSF_FALCON_ID_GSPLITE:
#ifndef BOOT_FROM_HS_BUILD
            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_FALCON_DIODT_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_FALCON_DIODTA_PRIV_LEVEL_MASK, plmVal);
#endif // ifndef BOOT_FROM_HS_BUILD

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_CMD_HEAD_INTRSTAT_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_CMD_INTREN_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _CMD_INTREN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _CMD_INTREN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_CMD_INTREN_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_TIMER_0_INTERVAL_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _TIMER_0_INTERVAL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _TIMER_0_INTERVAL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_TIMER_0_INTERVAL_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PGSP_TIMER_CTRL_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PGSP, _TIMER_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PGSP, _TIMER_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PGSP_TIMER_CTRL_PRIV_LEVEL_MASK, plmVal);
            break;

        case LSF_FALCON_ID_SEC2:
#ifndef BOOT_FROM_HS_BUILD
            plmVal = ACR_REG_RD32(BAR0, LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PSEC_FALCON_DIODT_PRIV_LEVEL_MASK, plmVal);

            plmVal = ACR_REG_RD32(BAR0, LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PSEC, _FALCON_DIODTA_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PSEC_FALCON_DIODTA_PRIV_LEVEL_MASK, plmVal);
#endif // ifndef BOOT_FROM_HS_BUILD
            break;

        case LSF_FALCON_ID_FBFALCON:
            plmVal = ACR_REG_RD32(BAR0, LW_PFBFALCON_FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK);
            plmVal = FLD_SET_DRF(_PFBFALCON, _FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, plmVal);
            plmVal = FLD_SET_DRF(_PFBFALCON, _FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, plmVal);
            ACR_REG_WR32(BAR0, LW_PFBFALCON_FALCON_DMEM_CARVEOUT_PRIV_LEVEL_MASK, plmVal);
            break;
    }
    return ACR_OK;
}

/*!
 * @brief Check if falconInstance is valid
 */
LwBool
acrlibIsFalconInstanceValid_GA10X
(
    LwU32  falconId,
    LwU32  falconInstance,
    LwBool bIsSmcActive
)
{
    switch (falconId)
    {
        default:
            if ((falconId < LSF_FALCON_ID_END) && (falconInstance == LSF_FALCON_INSTANCE_DEFAULT_0))
            {
                return LW_TRUE;
            }
            break;
    }
    return LW_FALSE;
}

/*!
 * @brief Check if FalconIndexMask is valid
 */
LwBool
acrlibIsFalconIndexMaskValid_GA10X
(
    LwU32  falconId,
    LwU32  falconIndexMask
)
{
    switch (falconId)
    {
        default:
             if ((falconId < LSF_FALCON_ID_END) && (falconIndexMask == LSF_FALCON_INDEX_MASK_DEFAULT_0))
             {
                 return LW_TRUE;
             }
    }
    return LW_FALSE;
}

/*!
 * @brief Program DMA base register
 *
 * @param[in] pFlcnCfg   Structure to hold falcon config
 * @param[in] fbBase     Base address of fb region to be copied
 */
void
acrlibProgramDmaBase_GA10X
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwU64               fbBase
)
{
    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE, (LwU32)LwU64_LO32(fbBase));

    if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS)
    {
        //
        // WAR for bug 2761392. FECS does not have a DMATRFBASE1 register. We
        // could make this more generic by putting the dmaBase registers in
        // ACR_FLCN_CONFIG, and backport this fix to older chips. In its
        // current form, it does the bare minimum to unblock GA10x emulation.
        // Hopper request to unify this for FECS with other falcons: bug 2116810
        //
        LwU32 addr = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_HI_BASE_ADDR, pFlcnCfg->falconInstance);
        ACR_REG_WR32(BAR0, addr, (LwU32)LwU64_HI32(fbBase));
    }
    else
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE1, (LwU32)LwU64_HI32(fbBase));
    }
}

// Max time to wait for RISCV BR return code update
#define ACR_TIMEOUT_OF_150US_FOR_RISCV_BR_RETCODE_UPDATE        (150*1000)

/*!
 * @brief: RISCV pre reset sequence
 *
 * @param[in] pFlcnCfg          FALCONF CONFIG of target engine
 */
ACR_STATUS
acrlibRiscvPreResetSequence_GA10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      val    = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BR_RETCODE);

    if (FLD_TEST_DRF(_PRISCV, _RISCV_BR_RETCODE, _RESULT, _INIT, val))
    {
        LwS32          timeoutLeftNs = 0;
        ACR_TIMESTAMP  startTimeNs;
        // Delay 150 us then continue to reset
        acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
        while (1)
        {
            if (acrlibCheckTimeout_HAL(pAcrlib, (LwU32)ACR_TIMEOUT_OF_150US_FOR_RISCV_BR_RETCODE_UPDATE,
                                        startTimeNs, &timeoutLeftNs) == ACR_ERROR_TIMEOUT)
            {
                break;
            }
        }
    }
    else if (FLD_TEST_DRF(_PRISCV, _RISCV_BR_RETCODE, _RESULT, _FAIL, val))
    {
        falc_halt(); // Shouldn't hit this case after delay, and ideally we should also update error code but this code is common i.e. ACRlib
    }
    else if (FLD_TEST_DRF(_PRISCV, _RISCV_BR_RETCODE, _RESULT, _PASS, val))
    {
        // nothing to do for pre-reset
    }
    else if (FLD_TEST_DRF(_PRISCV, _RISCV_BR_RETCODE, _RESULT, _RUNNING, val))
    {
        falc_halt(); // Shouldn't hit this case after delay, ideally we should also update error code but this code is common i.e. ACRlib
    }
    else
    {
        return ACR_ERROR_ILWALID_OPERATION; // control should not reach here
    }

    return status;
}

/*!
 * @brief: FALCON pre reset sequence
 *
 * @param[in] pFlcnCfg          FALCONF CONFIG of target engine
 */
ACR_STATUS
acrlibFalconPreResetSequence_GA10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status   = ACR_OK;
    LwU32      kfuseReg = 0;
    LwU32      scpReg   = 0;

    if (pFlcnCfg->scpP2pCtl == 0)
    {
        return ACR_OK;
    }

    // Trigger and poll for p2p to finish
    scpReg = ACR_REG_RD32(BAR0, pFlcnCfg->scpP2pCtl);
    while (1)
    {
        kfuseReg = ACR_REG_RD32(BAR0, pFlcnCfg->falcon2RegisterBase + LW_PFALCON2_FALCON_KFUSE_LOAD_CTL);
        scpReg   = ACR_REG_RD32(BAR0, pFlcnCfg->scpP2pCtl);

        if ((FLD_TEST_DRF(_PFALCON2, _FALCON_KFUSE_LOAD_CTL, _HWVER_BUSY,  _FALSE, kfuseReg)) &&
            (FLD_TEST_DRF(_PFALCON2, _FALCON_KFUSE_LOAD_CTL, _HWVER_VALID, _TRUE,  kfuseReg)) &&
            (FLD_TEST_DRF(_PSEC,     _SCP_CTL_P2PRX,         _SFK_LOADED,  _TRUE,  scpReg)))
        {
            break;
        }
    }

    return status;
}

/*!
 * @brief: Falcon post reset sequence
 *
 * @param[in] pFlcnCfg          FALCONF CONFIG of target engine
 */
ACR_STATUS
acrlibFalconPostResetSequence_GA10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32      val    = 0;

    if (pFlcnCfg->scpP2pCtl == 0)
    {
        return ACR_OK; // Nothing to do, as falcon does not have SCP or is not HS capable
    }

    // Trigger and poll for hwfusever p2p transaction to complete
    val = ACR_REG_RD32(BAR0, (pFlcnCfg->falcon2RegisterBase + LW_PFALCON2_FALCON_KFUSE_LOAD_CTL));
    while (1)
    {
        val = ACR_REG_RD32(BAR0, pFlcnCfg->falcon2RegisterBase + LW_PFALCON2_FALCON_KFUSE_LOAD_CTL);
        if (FLD_TEST_DRF(_PFALCON2, _FALCON_KFUSE_LOAD_CTL, _HWVER_BUSY,  _FALSE, val) &&
            FLD_TEST_DRF(_PFALCON2, _FALCON_KFUSE_LOAD_CTL, _HWVER_VALID, _TRUE,  val))
        {
            break;
        }
    }

    return status;
}

/*!
 * @brief: Pre Engine Reset sequence for Bug 200590866
 *
 * @param[in] pFlcnCfg          FALCONF CONFIG of target engine
 */
ACR_STATUS
acrlibPreEngineResetSequenceBug200590866_GA10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status          = ACR_OK;
    LwU32      val             = 0;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Skip for fmodel for Bug 2874425. Once fmodel runs are updated, we can revert this
    // It is ok to have such check, because this is to fix functional issue, for security issue we have GA10X ECO in place
    //
    if (FLD_TEST_DRF_DEF(BAR0, _PTOP, _PLATFORM, _TYPE, _FMODEL))
    {
        return ACR_OK;
    }

    LwBool     bFalconSequence = (pFlcnCfg->riscvRegisterBase == 0) ? LW_TRUE : LW_FALSE;

    // Program core select (if riscv engine exists)
    if(pFlcnCfg->riscvRegisterBase != 0)
    {
        val = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BCR_CTRL);
        if (FLD_TEST_DRF(_PRISCV, _RISCV_BCR_CTRL, _VALID, _FALSE, val))
        {
            val = DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _FALCON);
            acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BCR_CTRL, val);
        }
        val = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BCR_CTRL);
        if (FLD_TEST_DRF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _FALCON, val))
        {
            bFalconSequence = LW_TRUE;
        }
    }

    //
    // If falcon engine with falcon2 space, then engage falcon pre reset sequence
    // If RISCV, then engage RISCV pre reset sequence
    //
    if (bFalconSequence)
    {
        if (pFlcnCfg->falcon2RegisterBase != 0)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibFalconPreResetSequence_HAL(pAcrlib, pFlcnCfg));
        }
        else
        {
            // no riscv and no falcon2 space, meaning no p2p. Nothing to do here.
        }
    }
    else // RISCV if not falcon
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibRiscvPreResetSequence_HAL(pAcrlib, pFlcnCfg));
    }

    return status;
}

/*!
 * @brief: Post Engine Reset sequence for Bug 200590866
 *
 * @param[in] pFlcnCfg          FALCONF CONFIG of target engine
 */
ACR_STATUS
acrlibPostEngineResetSequenceBug200590866_GA10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status          = ACR_OK;
    LwU32      val             = 0;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Skip for fmodel for Bug 2874425. Once fmodel runs are updated, we can revert this
    // It is ok to have such check, because this is to fix functional issue, for security issue we have GA10X ECO in place
    //
    if (FLD_TEST_DRF_DEF(BAR0, _PTOP, _PLATFORM, _TYPE, _FMODEL))
    {
        return ACR_OK;
    }

    LwBool     bFalconSequence = (pFlcnCfg->riscvRegisterBase == 0) ? LW_TRUE : LW_FALSE;

    if (pFlcnCfg->riscvRegisterBase != 0)
    {
        val = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BCR_CTRL);
        if (FLD_TEST_DRF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _FALCON, val))
        {
            bFalconSequence = LW_TRUE;
        }
    }

    //
    // Assumption here is that, core select was already done right after engine reset and before this function call
    // Post engine reset sequence is only required for falcon core
    //
    if (bFalconSequence)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibFalconPostResetSequence_HAL(pAcrlib, pFlcnCfg));
    }

    return status;
}

/*!
 * @brief: Program CORE_SELECT based on the UPROC_TYPE
 *
 * @param[in] pFlcnCfg          FALCONF CONFIG of target engine
 *
 */
ACR_STATUS
acrlibCoreSelect_GA10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32 data32 = 0;

    if(!pFlcnCfg)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // RISCV is not enabled, no need to program core select
    if(!pFlcnCfg->riscvRegisterBase)
    {
        return ACR_OK;
    }

    switch(pFlcnCfg->uprocType)
    {
        case ACR_TARGET_ENGINE_CORE_RISCV:
        case ACR_TARGET_ENGINE_CORE_RISCV_EB:
            data32 = DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _RISCV) | \
                     DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _BRFETCH,     _TRUE);
        break;

        case ACR_TARGET_ENGINE_CORE_FALCON:
            data32 = DRF_DEF(_PRISCV, _RISCV_BCR_CTRL, _CORE_SELECT, _FALCON);
        break;

        default:
            return ACR_ERROR_UNEXPECTED_ARGS;
    }

    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BCR_CTRL, data32);

    return ACR_OK;
}

/*!
 * @brief Poll for IMEM and DMEM scrubbing to complete
 *
 * @param[in] pFlcnCfg Falcon Config
 */
ACR_STATUS
acrlibPollForScrubbing_GA10X
(
    PACR_FLCN_CONFIG  pFlcnCfg
)
{
    ACR_STATUS    status          = ACR_OK;
    LwU32         data            = 0;
    ACR_TIMESTAMP startTimeNs;
    LwS32         timeoutLeftNs;

    if(!pFlcnCfg)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    // Poll for SRESET to complete
    acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
    data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_HWCFG2);
    while(FLD_TEST_DRF(_PFALCON, _FALCON_HWCFG2, _MEM_SCRUBBING, _PENDING, data))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, (LwU32)ACR_DEFAULT_TIMEOUT_NS,
                                                            startTimeNs, &timeoutLeftNs));
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_HWCFG2);
    }

    return status;
}

#ifdef BOOT_FROM_HS_BUILD

static void
_acrCopySignatureToTargetDmem
(
    PACR_FLCN_CONFIG pFlcnCfg,
    void *pParams,
    LwU32 size,
    LwU32 signatureDmemOffset
)
{
    LwU32 i;
    LwU32 *pParamDwords = (LwU32 *)pParams;
    LwU32 sig32bit;

    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMC(0),
                        REF_NUM(LW_PFALCON_FALCON_DMEMC_OFFS, 0) |
                        REF_NUM(LW_PFALCON_FALCON_DMEMC_BLK, signatureDmemOffset >> 8) |
                        REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCW, 1));

    for (i = 0; i < size/4; i++)
    {
        sig32bit = pParamDwords[i];
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMD(0), sig32bit);
    }
}

ACR_STATUS
acrlibSetupHsFmcSignatureValidation_GA102
(
    PACR_FLCN_CONFIG pFlcnCfg,
    HS_FMC_PARAMS *pHsFmcParams,
    LwU32 blImemOffset,
    LwU32 blSize,
    LwU32 signatureDmemOffset
)
{
    ACR_STATUS status = ACR_OK;

    if (pHsFmcParams->bHsFmc)
    {
        // HS Bootloader/FMC is present.
        acrlibMemset_HAL(pAcrlib, (void *)&g_HsFmcSignatureVerificationParams, 0, sizeof(g_HsFmcSignatureVerificationParams));
        // set pkc signature
        acrlibMemcpy_HAL(pAcrlib, (void *)&g_HsFmcSignatureVerificationParams.signature, pHsFmcParams->pkcSignature, LSF_SIGNATURE_SIZE_MAX_BYTE);

        // Copy the signature params to the target Falcon's DMEM
        _acrCopySignatureToTargetDmem(pFlcnCfg, (void *)&g_HsFmcSignatureVerificationParams, sizeof(g_HsFmcSignatureVerificationParams), signatureDmemOffset);

        ACR_REG_WR32(BAR0, pFlcnCfg->falcon2RegisterBase + LW_PFALCON2_FALCON_BROM_PARAADDR(0),  (LwU32)signatureDmemOffset);
        ACR_REG_WR32(BAR0, pFlcnCfg->falcon2RegisterBase + LW_PFALCON2_FALCON_BROM_ENGIDMASK, pHsFmcParams->engIdMask);
        ACR_REG_WR32(BAR0, pFlcnCfg->falcon2RegisterBase + LW_PFALCON2_FALCON_BROM_LWRR_UCODE_ID, pHsFmcParams->ucodeId);
        ACR_REG_WR32(BAR0, pFlcnCfg->falcon2RegisterBase + LW_PFALCON2_FALCON_MOD_SEL , DRF_DEF(_PFALCON2_FALCON, _MOD_SEL, _ALGO, _RSA3K));

    }

    return status;
}

/*
 * @brief Get common scratch group register allocation for particular falcon IMB/IMB1/DMB/DMB1.
 */
ACR_STATUS
acrlibGetFalconScratchGroupAllocationsForCodeDataBase_GA10X
(
    LwU32 falconId,
    LwU32 *pIMBRegister,
    LwU32 *pIMB1Register,
    LwU32 *pDMBRegister,
    LwU32 *pDMB1Register,
    LwU32 *pPLMRegister
)
{
    if ((pIMBRegister == NULL) || (pIMB1Register == NULL) || (pDMBRegister == NULL) || (pDMB1Register == NULL))
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (falconId)
    {
        case LSF_FALCON_ID_SEC2:
                *pIMBRegister  = LW_PSEC_FALCON_IMB_BASE_COMMON_SCRATCH;
                *pIMB1Register = LW_PSEC_FALCON_IMB1_BASE_COMMON_SCRATCH;
                *pDMBRegister  = LW_PSEC_FALCON_DMB_BASE_COMMON_SCRATCH;
                *pDMB1Register = LW_PSEC_FALCON_DMB1_BASE_COMMON_SCRATCH;
                *pPLMRegister  = LW_PSEC_FALCON_IMB_DMB_COMMON_SCRATCH_PLM;
            break;
        case LSF_FALCON_ID_GSPLITE:
                *pIMBRegister  = LW_PGSP_FALCON_IMB_BASE_COMMON_SCRATCH;
                *pIMB1Register = LW_PGSP_FALCON_IMB1_BASE_COMMON_SCRATCH;
                *pDMBRegister  = LW_PGSP_FALCON_DMB_BASE_COMMON_SCRATCH;
                *pDMB1Register = LW_PGSP_FALCON_DMB1_BASE_COMMON_SCRATCH;
                *pPLMRegister  = LW_PGSP_FALCON_IMB_DMB_COMMON_SCRATCH_PLM;
            break;
#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
#endif
        case LSF_FALCON_ID_LWDEC:
                *pIMBRegister  = LW_PLWDEC_FALCON_IMB_BASE_COMMON_SCRATCH;
                *pIMB1Register = LW_PLWDEC_FALCON_IMB1_BASE_COMMON_SCRATCH;
                *pDMBRegister  = LW_PLWDEC_FALCON_DMB_BASE_COMMON_SCRATCH;
                *pDMB1Register = LW_PLWDEC_FALCON_DMB1_BASE_COMMON_SCRATCH;
                *pPLMRegister  = LW_PLWDEC_FALCON_IMB_DMB_COMMON_SCRATCH_PLM;
            break;
        default:
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    return ACR_OK;
}

/*!
 * @brief Writes the value for target falcon's IMB, IMB1, DMB, DMB1 register in
 * the allocated common scratch group register.
 *
 * @param[in] wprBase           WPR regiosn base address
 * @param[in] pLsbHeaderWrapper Target falcon LSB header
 * @param[in] pFlcnCfg          Falcon Config
 *
 * @param[out] ACR_STATUS
 */
ACR_STATUS
acrlibProgramFalconCodeDataBaseInScratchGroup_GA10X
(
    LwU64 wprBase,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper,
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;
    LwU32 ucodeOffset = 0;
    LwU32 blCodeSize = 0;
    LwU32 appDataOffset = 0;
    LwU32 val = 0;
    LwU64 IMBBase = 0;
    LwU64 DMBBase = 0;
    LwU32 IMBRegister;
    LwU32 IMB1Register;
    LwU32 DMBRegister;
    LwU32 DMB1Register;
    LwU32 PLMRegister;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET, pLsbHeaderWrapper, &ucodeOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE, pLsbHeaderWrapper, &blCodeSize));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET, pLsbHeaderWrapper, &appDataOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconScratchGroupAllocationsForCodeDataBase_HAL(pAcrlib, pFlcnCfg->falconId,
                                      &IMBRegister, &IMB1Register, &DMBRegister, &DMB1Register, &PLMRegister));

    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);
    IMBBase = (wprBase << 8) + ucodeOffset + blCodeSize;
    DMBBase = (wprBase << 8) + ucodeOffset + appDataOffset;

    // IMB Programming
    val = (LwU32)LwU64_LO32(IMBBase) >> 8 | (LwU32)LwU64_HI32(IMBBase) << 24;
    ACR_REG_WR32(BAR0, IMBRegister, val);

    // IMB1 Programming
    val = (LwU32)LwU64_HI32(IMBBase) >> 8;
    ACR_REG_WR32(BAR0, IMB1Register, val);

    // DMB Programming
    val = (LwU32)LwU64_LO32(DMBBase) >> 8 | (LwU32)LwU64_HI32(DMBBase) << 24;
    ACR_REG_WR32(BAR0, DMBRegister, val);

    // DMB1 Programming
    val = (LwU32)LwU64_HI32(DMBBase) >> 8;
    ACR_REG_WR32(BAR0, DMB1Register, val);

    // PLM Programming
    ACR_REG_WR32(BAR0, PLMRegister, ACR_PLMASK_READ_L0_WRITE_L3);

    return status;
}
#endif // BOOT_FROM_HS_BUILD

/*
 * @brief: Use Pri Source Isolation to lock the falcon register space to engine
 *         on which ACR is running (GSP for ASB and SEC2 for ACRLIB).
 * @param[in] sourceId               : Source Falcon Id
 * @param[in] pTargetFlcnCfg         : Target Falcon config
 * @param[in] setTrap                : If True  - Lock falcon register space
 *                                     If False - Restore TARGET_MASK to original value
 * @param[in] pTargetMaskPlmOldValue : Used for restoring TARGET_MASK register PLM value.
 * @param[in] pTargetMaskOldValue    : Used for restoring TARGET_MASK register value.
 * @param[out] ACR_STATUS
 */
ACR_STATUS
acrlibLockFalconRegSpace_GA10X
(
    LwU32 sourceId,
    PACR_FLCN_CONFIG pTargetFlcnCfg,
    LwBool setTrap,
    LwU32 *pTargetMaskPlmOldValue,
    LwU32 *pTargetMaskOldValue
)
{
    ACR_STATUS  status                  = ACR_OK;
    LwU32       targetMaskValData       = 0;
    LwU32       targetMaskPlm           = 0;
    LwU32       targetMaskIndexData     = 0;
    LwU32       sourceMask              = 0;
    LwU32       targetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
    LwU32       targetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
    LwU32       targetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;

    if ((pTargetFlcnCfg == NULL) || (pTargetMaskPlmOldValue == NULL) || (pTargetMaskOldValue == NULL))
    {
        status = ACR_ERROR_ILWALID_ARGUMENT;
        goto Cleanup;
    }

#if defined(LWDEC_RISCV_EB_PRESENT) || defined(LWJPG_PRESENT)
    //
    // On GH100 Fmodel, SYS_B LW_PPRIV_SYSB_TARGET_MASK_PRIV_LEVEL_MASK is undefined,
    // and initial value is zero. This will cause register lock function corrupted.
    // LWDEC_RISCV_EB (2 ~ 6) and LWJPG(3 ~ 7) is controlled under SYS_B cluster. So we just return
    // ACR_OK to avoid test failed on Fmodel.
    // We can't use ACR_FMODEL_BUILD, because SEC2 doesn't have FMODLE profile.
    //
    if (FLD_TEST_DRF_DEF(BAR0, _PTOP, _PLATFORM, _TYPE, _FMODEL))
    {
        if (pTargetFlcnCfg->falconId == LSF_FALCON_ID_LWDEC_RISCV_EB ||
            pTargetFlcnCfg->falconId == LSF_FALCON_ID_LWJPG)
        {
            return ACR_OK;
        }
    }
#endif

    //
    // Due to change in GH100 floorplan, Fbfalcon/LWDEC is moved from SYS to SYSB/SYSC cluster
    // HW has kept flexibility of SYSB/SYSC cluster for now. This is handled with the HAL call below.
    // NOTE: We are not changing the field names of LW_PPRIV_SYS*_TARGET_MASK* with the assumption
    // that even if the cluster has changed for FbFalcon/LWDEC, the arrangement of fields inside the register is not changed.
    // Thus we can keep using the LW_PPRIV_SYS_* field names.
    //
    switch(pTargetFlcnCfg->falconId)
    {
        case LSF_FALCON_ID_FBFALCON:
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibGetFbFalconTargetMaskRegisterDetails_HAL(pAcrlib, &targetMaskRegister, &targetMaskPlmRegister, &targetMaskIndexRegister));
        break;

#ifdef LWDEC_RISCV_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV:
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        case LSF_FALCON_ID_LWDEC_RISCV_EB:
#endif
        case LSF_FALCON_ID_LWDEC:
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibGetLwdecTargetMaskRegisterDetails_HAL(pAcrlib, pTargetFlcnCfg, &targetMaskRegister, &targetMaskPlmRegister, &targetMaskIndexRegister));
        break;

        case LSF_FALCON_ID_GPCCS:
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibGetGpccsTargetMaskRegisterDetails_HAL(pAcrlib, pTargetFlcnCfg->falconInstance, &targetMaskRegister, &targetMaskPlmRegister, &targetMaskIndexRegister));
        break;
#ifdef LWJPG_PRESENT
        case LSF_FALCON_ID_LWJPG:
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibGetLwjpgTargetMaskRegisterDetails_HAL(pAcrlib, pTargetFlcnCfg, &targetMaskRegister, &targetMaskPlmRegister, &targetMaskIndexRegister));
        break;
#endif
    }

    // Lock falcon
    if (setTrap == LW_TRUE)
    {
        if (sourceId == pTargetFlcnCfg->falconId)
        {
            status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
            goto Cleanup;
        }

        // Step 1: Read, store and program TARGET_MASK PLM register.
        targetMaskPlm = ACR_REG_RD32(BAR0, targetMaskPlmRegister);

        // Save plm value to be restored during unlock
        *pTargetMaskPlmOldValue = targetMaskPlm;

        switch (sourceId)
        {
#ifdef ACR_BUILD_ONLY
            case LSF_FALCON_ID_GSPLITE:
                sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP;
                break;
#endif
            case LSF_FALCON_ID_SEC2:
                sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2;
                break;

            default:
                status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
                goto Cleanup;
        }

        // Verify if PLM of TARGET_MASK does not allow programming from GSP or SEC2
        if (!(DRF_VAL(_PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, targetMaskPlm) & BIT(sourceMask)))
        {
            status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
            goto Cleanup;
        }

        //
        // Program PLM of TARGET_MASK registers to be accessible only from GSP or SEC2.
        // And Block accesses from other sources
        //
        targetMaskPlm = FLD_SET_DRF_NUM( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(sourceMask), targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCK, targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCK, targetMaskPlm);
        ACR_REG_WR32(BAR0, targetMaskPlmRegister, targetMaskPlm);

        // Step 2: Read and program TARGET_MASK_INDEX register.
        targetMaskIndexData = ACR_REG_RD32(BAR0, targetMaskIndexRegister);

        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, pTargetFlcnCfg->targetMaskIndex, targetMaskIndexData);
        ACR_REG_WR32(BAR0, targetMaskIndexRegister, targetMaskIndexData);

        // Step 3: Read, store and program TARGET_MASK register.
        targetMaskValData  = ACR_REG_RD32(BAR0, targetMaskRegister);
        // Save Target Mask value to be restored.
        *pTargetMaskOldValue = targetMaskValData;

        // Give only Read permission to other PRI sources and Read/Write permission to GSP.
        targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _VAL, _W_DISABLED_R_ENABLED_PL0, targetMaskValData);

        switch (sourceId)
        {
#ifdef ACR_BUILD_ONLY
            case LSF_FALCON_ID_GSPLITE:
                // ASB will lock SEC2 register space while bootstrapping SEC2.
                targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _GSP_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
           break;
#endif
            case LSF_FALCON_ID_SEC2:
                // ACRlib will lock target falcon register space while bootstrapping target falcon.
                targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _SEC2_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
            break;

            default:
                status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
                goto Cleanup;
        }

        ACR_REG_WR32(BAR0, targetMaskRegister, targetMaskValData);
    }
    else // Release lock
    {
        // Reprogram TARGET_MASK_INDEX register.
        targetMaskIndexData = ACR_REG_RD32(BAR0, targetMaskIndexRegister);

        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, pTargetFlcnCfg->targetMaskIndex, targetMaskIndexData);
        ACR_REG_WR32(BAR0, targetMaskIndexRegister, targetMaskIndexData);

        // Restore TARGET_MASK register to previous value.
        targetMaskValData = *pTargetMaskOldValue;
        ACR_REG_WR32(BAR0, targetMaskRegister, targetMaskValData);

        //
        // Restore TARGET_MASK_PLM to previous value to
        // enable all the sources to access TARGET_MASK registers.
        //
        targetMaskPlm = *pTargetMaskPlmOldValue;
        ACR_REG_WR32(BAR0, targetMaskPlmRegister, targetMaskPlm);
    }

Cleanup:
    return status;
}

#ifdef NEW_WPR_BLOBS
ACR_STATUS
acrResetAndPollForSec2Ext_GA10X
(
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper
)
{
    ACR_FLCN_CONFIG  flcnCfg;
    ACR_STATUS       status;
    LwU32            falconId;

    if (!pWprHeaderWrapper)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                        pWprHeaderWrapper, &falconId));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPreEngineResetSequenceBug200590866_HAL(pAcrlib, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPutFalconInReset_HAL(pAcrlib, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibBringFalconOutOfReset_HAL(pAcrlib, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCoreSelect_HAL(pAcrlib, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPostEngineResetSequenceBug200590866_HAL(pAcrlib, &flcnCfg));

    // Poll SEC2 for IMEM|DMEM scrubbing
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPollForScrubbing_HAL(pAcrlib, &flcnCfg));

    return ACR_OK;
}

#if defined(ASB)
ACR_STATUS
acrBootstrapFalconExt_GA10X
(
    void
)
{
    ACR_STATUS              status     = ACR_OK;
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper = NULL;
    LSF_LSB_HEADER_WRAPPER  lsbHeaderWrapper;
    LwU32                   index      = 0;
    LwU32                   falconId;

    // Read the whole WPR header wrappers into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadAllWprHeaderWrappers_HAL(pAcr));

    // Get WPR header for particular falcon index acr.h
    for (index = 0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeaderWrapper = GET_WPR_HEADER_WRAPPER(index);
        acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID, pWprHeaderWrapper, &falconId);

        if (falconId == LSF_FALCON_ID_SEC2)
        {
            break;
        }
    }

    if (index > LSF_FALCON_ID_END)
    {
        return ACR_ERROR_ACRLIB_HOSTING_FALCON_NOT_FOUND;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeaderWrapper_HAL(pAcr, pWprHeaderWrapper, (void *)&lsbHeaderWrapper));

    // Setup the LS falcon
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupLSFalconExt_HAL(pAcr, pWprHeaderWrapper, &lsbHeaderWrapper));

    return status;
}
#endif

#ifdef ACR_BUILD_ONLY
#if !defined(ACR_UNLOAD) && !defined(ACR_UNLOAD_ON_SEC2)
/*!
 * @brief Setup Falcon in LS mode with new WPR blob defines. This ilwolves
 *        - Setting up LS registers (TODO)
 *        - Copying bootloader from FB into target falcon memory
 *        - For HS Bootloader supported Falcons, load the RTOS + App to falcon memory
 *        - Programming carveout registers in target falcon
 *
 * @param[in] pWprHeader    WPR header
 * @param[in] pLsbHeader    LSB header
 */
ACR_STATUS
acrSetupLSFalconExt_GA10X
(
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS       status                = ACR_OK;
    ACR_STATUS       acrStatusCleanup      = ACR_OK;
    ACR_FLCN_CONFIG  flcnCfg;

    LwU64            blWprBase             = 0;
    LwU32            dst                   = 0;
    LwU32            falconId              = LSF_FALCON_ID_ILWALID;
    LwU32            blCodeSize            = 0;
    LwU32            ucodeOffset           = 0;
    LwU32            blImemOffset          = 0;
    LwU32            flags                 = 0;
    LwU32            blDataOffset          = 0;
    LwU32            blDataSize            = 0;
    LwU32            targetMaskPlmOldValue = 0;
    LwU32            targetMaskOldValue    = 0;
#ifdef BOOT_FROM_HS_BUILD
    LwU32            appCodeSize  = 0;
    LwU32            appCodeOffset = 0;
    LwU32            appImemOffset = 0;
    LwU32            appDataSize   = 0;
    LwU32            appDataOffset = 0;
    LwU32            appDmemOffset = 0;
    LwU32            appCodebase   = 0;
    LwU32            appDatabase   = 0;
#endif
    // Configure common settings for decode traps locking falcon reg space
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLockFalconRegSpaceViaDecodeTrapCommon_HAL(pAcr));

    // Get the specifics of this falconID
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfWprHeaderWrapperCtrl_HAL(pAcrlib, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                        pWprHeaderWrapper, &falconId));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    //
    // Lock falcon reg space using decode trap/pri source isolation.
    // For trap based locking:
    //      Reuse the same trap such that the trap lasts only for a single loop iteration
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLockFalconRegSpace_HAL(pAcrlib, LSF_FALCON_ID_GSPLITE, &flcnCfg, LW_TRUE, &targetMaskPlmOldValue, &targetMaskOldValue));

    // Reset the SEC2 falcon.
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrResetAndPollForSec2Ext_HAL(pAcr, pWprHeaderWrapper));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, &blCodeSize));
    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_SET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, &blCodeSize));

#ifdef BOOT_FROM_HS_BUILD
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibProgramFalconCodeDataBaseInScratchGroup_HAL(pAcrlib, g_dmaProp.wprBase, pLsbHeaderWrapper, &flcnCfg));
#endif // BOOT_FROM_HS_BUILD

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibSetupTargetRegisters_HAL(pAcrlib, &flcnCfg));

    // Override the value of IMEM/DMEM PLM to final value for the falcons being booted by this level 3 binary
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, flcnCfg.imemPLM);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, flcnCfg.dmemPLM);

    //
    // Load code into IMEM
    // BL starts at offset, but since the IMEM VA is not zero, we need to make
    // sure FBOFF is equal to the expected IMEM VA. So adjusting the FBBASE to make
    // sure FBOFF equals to VA as expected.
    //
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, &ucodeOffset));
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, &blImemOffset));

    blWprBase = ((g_dmaProp.wprBase) + (ucodeOffset >> FLCN_IMEM_BLK_ALIGN_BITS)) -
                 (blImemOffset >> FLCN_IMEM_BLK_ALIGN_BITS);

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_FLAGS,
                                                                        pLsbHeaderWrapper, &flags));
    // Check if code needs to be loaded at start of IMEM or at end of IMEM
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, flags))
    {
        dst = 0;
    }
    else
    {
        dst = (acrlibFindFarthestImemBl_HAL(pAcrlib, &flcnCfg, blCodeSize) *
               FLCN_IMEM_BLK_SIZE_IN_BYTES);
    }

#ifdef BOOT_FROM_HS_BUILD
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_HS_FMC_PARAMS,
                                                                        pLsbHeaderWrapper, &g_hsFmcParams));

    if (g_hsFmcParams.bHsFmc)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibIssueTargetFalconDma_HAL(pAcrlib,
            dst, blWprBase, blImemOffset, blCodeSize, g_dmaProp.regionID,
            LW_TRUE, LW_TRUE, LW_TRUE, &flcnCfg));
    }
    else
#endif
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibIssueTargetFalconDma_HAL(pAcrlib,
            dst, blWprBase, blImemOffset, blCodeSize, g_dmaProp.regionID,
            LW_TRUE, LW_TRUE, LW_FALSE, &flcnCfg));
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, &blDataOffset));

#ifdef BOOT_FROM_HS_BUILD
    if (g_hsFmcParams.bHsFmc)
    {
        blDataSize = 0; // HS FMC has no data
    }
    else
#endif
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                          pLsbHeaderWrapper, &blDataSize));
    }

    // Load data into DMEM
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibIssueTargetFalconDma_HAL(pAcrlib,
        0, g_dmaProp.wprBase, blDataOffset, blDataSize,
        g_dmaProp.regionID, LW_TRUE, LW_FALSE, LW_FALSE, &flcnCfg));

#ifdef BOOT_FROM_HS_BUILD
    if (g_hsFmcParams.bHsFmc)
    {
        // Load the RTOS + App code to IMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_IMEM_OFFSET,
                                                                            pLsbHeaderWrapper, &appImemOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_OFFSET,
                                                                            pLsbHeaderWrapper, &appCodeOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_SIZE,
                                                                            pLsbHeaderWrapper, &appCodeSize));

        // Ensure that app code does not overlap BL code in IMEM
        if ((appImemOffset >= dst) || ((appImemOffset + appCodeSize) >= dst))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(ACR_ERROR_ILWALID_ARGUMENT);
        }

        // Start the app/OS code DMA with the DMA base set beyond the BL, so the tags are right
        appCodebase = (g_dmaProp.wprBase) + (ucodeOffset >> FLCN_IMEM_BLK_ALIGN_BITS) + (blCodeSize >> FLCN_IMEM_BLK_ALIGN_BITS);

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
            appImemOffset, appCodebase, (appCodeOffset - blCodeSize), appCodeSize, g_dmaProp.regionID,
            LW_TRUE, LW_TRUE, LW_FALSE, &flcnCfg));

        // Load data for App/OS.
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DMEM_OFFSET,
                                                                            pLsbHeaderWrapper, &appDmemOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET,
                                                                            pLsbHeaderWrapper, &appDataOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLsfLsbHeaderWrapperCtrl_HAL(pAcrlib, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_SIZE,
                                                                            pLsbHeaderWrapper, &appDataSize));

        if (appDataSize != 0)
        {
            appDatabase = (g_dmaProp.wprBase) + (ucodeOffset >> FLCN_IMEM_BLK_ALIGN_BITS) + (appDataOffset >> FLCN_IMEM_BLK_ALIGN_BITS);

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(pAcrlib,
                appDmemOffset, appDatabase, 0, appDataSize,
                g_dmaProp.regionID, LW_TRUE, LW_FALSE, LW_FALSE, &flcnCfg));
        }

        //
        // The signature is to be setup in the target falcon right after the App data
        // appDmemOffset and appDataSize should be already aligned
        //
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupHsFmcSignatureValidation_HAL(pAcrlib, &flcnCfg, &g_hsFmcParams, blImemOffset, blCodeSize, appDmemOffset + appDataSize));
    }
#endif

    // Set the BOOTVEC
    acrlibSetupBootvec_HAL(pAcrlib, &flcnCfg, blImemOffset);

    // Check if Falcon wants virtual ctx
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, flags))
    {
        acrlibSetupCtxDma_HAL(pAcrlib, &flcnCfg, flcnCfg.ctxDma, LW_FALSE);
    }
    else
    {
        acrlibSetupCtxDma_HAL(pAcrlib, &flcnCfg, flcnCfg.ctxDma, LW_TRUE);
    }

    // Check if Falcon wants REQUIRE_CTX
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, flags))
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, &flcnCfg, LW_TRUE);
    }

    //
    // Clear decode trap to free falcon reg space
    // The trap from last loop iteration is still around and if we don't clear it then falcon will not be reachable via underprivileged clients
    //
Cleanup:
    acrStatusCleanup = acrlibLockFalconRegSpace_HAL(pAcrlib, LSF_FALCON_ID_GSPLITE, &flcnCfg, LW_FALSE, &targetMaskPlmOldValue, &targetMaskOldValue);

    return ((status != ACR_OK) ? status : acrStatusCleanup);
}

#endif // Not defined ACR_UNLOAD and not defined ACR_UNLOAD_ON_SEC2
#endif // ACR_BUILD_ONLY
#endif // NEW_WPR_BLOBS

