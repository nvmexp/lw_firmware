/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_unload_tu10x.c
 */
#include "acr.h"
#include "dev_fb.h"
#include "mmu/mmucmn.h"
#include "dev_pwr_pri.h"
#include "dev_falcon_v4.h"
#include "dev_master.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

/*!
 * @brief Reset falcon with given ID
 *        Note that here we reset and bring the falcons out of reset, so that falcons are accessible
 *        This is done for usecases like we use SEC2 mutex for general purpose on GP10X, so if we just
 *        reset sec2 and don't bring it back, the mutex acquire/release would fail
 *
 * @return     ACR_OK     If reset falcon is successful
 */
ACR_STATUS
acrResetFalcon_TU10X(LwU32 falconId)
{
    ACR_STATUS       status   = ACR_OK;
    ACR_FLCN_CONFIG  flcnCfg;
    LwU32            data     = 0;

    // Get the specifics of this falconID
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    //
    // Step 1: Bring falcon out of reset so that we are able to access its registers
    // It could be the case that falcon was already in reset
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibBringFalconOutOfReset_HAL(pAcrlib, &flcnCfg));

    //
    // Step 2: Set _RESET_LVLM_EN to TRUE to ensure all the falcon PLMs will be reset.
    // If we don't do this step, most of falcon will get reset but PLMs will stay as is
    // We are looking to fully reset the falcon here.
    //
    data = acrlibFlcnRegLabelRead_HAL(pAcrlib, &flcnCfg, REG_LABEL_FLCN_SCTL);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _TRUE, data);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FLCN_SCTL, data);

    //
    // Step 3: Put the falcon into reset
    // NOTE: The call below will automatically pull falcons that don't have either
    //       LW_PMC_ENABLE.<FALCON> or FALCON_ENGINE_RESET out of reset
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPreEngineResetSequenceBug200590866_HAL(pAcrlib, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPutFalconInReset_HAL(pAcrlib, &flcnCfg));

    //
    // Step 4: Bring the falcon back from reset state
    // So that later non-secure usage of falcon can be done
    // NOTE: We would have modified FALCON_SCTL_RESET_LVLM_EN regardless
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibBringFalconOutOfReset_HAL(pAcrlib, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCoreSelect_HAL(pAcrlib, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPostEngineResetSequenceBug200590866_HAL(pAcrlib, &flcnCfg));

    return status;
}

/*!
 * @brief Unlocks MMU regions
 *
 * @return     ACR_OK     If programming MMU registers are successful
 */
ACR_STATUS 
acrUnLockAcrRegions_TU10X(void)
{
    ACR_STATUS status = ACR_OK;
    LwU32      data   = 0;

    // Clear subWpr settings
#ifndef BOOT_FROM_HS_BUILD
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_1_FRTS_DATA_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_2_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

#ifdef ACR_RISCV_LS
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU_RISCV, PMU_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU_RISCV, PMU_SUB_WPR_ID_1_FRTS_DATA_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU_RISCV, PMU_SUB_WPR_ID_2_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FECS, FECS_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FECS, FECS_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LWDEC0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LWDEC0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LWDEC0_SUB_WPR_ID_2_PLAYREADY_SHARED_DATA_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_1_PLAYREADY_SHARED_DATA_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_3_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_4_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

#ifdef AMPERE_PROTECTED_MEMORY
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_5_APM_RTS_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_FALSE));
#endif

#ifdef FBFALCON_PRESENT
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_2_FRTS_DATA_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

#if defined(GSP_PRESENT) && !defined(ACR_GSP_RM_BUILD)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

    //
    // Breakdown WPR1 in MMU and secure scratch settings
    //
    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO);
    data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, ILWALID_WPR_ADDR_LO, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO, data);

    data =  ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI);
    data= FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, ILWALID_WPR_ADDR_HI, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    data =  FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR1, ACR_UNLOCK_READ_MASK, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
    data =  FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR1, ACR_UNLOCK_WRITE_MASK, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, data);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSaveWprInfoToBSISelwreScratch_HAL());

    return status;
}
