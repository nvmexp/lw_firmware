/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_unload_gh100.c
 */
#include "acr.h"
#include "dev_fb.h"
#include "mmu/mmucmn.h"
#include "dev_pwr_pri.h"
#include "dev_falcon_v4.h"
#include "dev_master.h"
#include "dev_top.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

/*!
 * @brief Unlocks MMU regions
 *
 * @return     ACR_OK     If programming MMU registers are successful
 */
ACR_STATUS
acrUnLockAcrRegions_GH100
(
    void
)
{
    ACR_STATUS       status = ACR_OK;
    LwU32            data   = 0;

#if defined(LWDEC_RISCV_EB_PRESENT) || defined(LWJPG_PRESENT)
    LwU32            i;
    LwU32            instanceCount = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    LwBool           bInstanceEnabled = LW_FALSE;
    ACR_FLCN_CONFIG  flcnCfg;
#endif

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

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC_RISCV, LWDEC_RISCV_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC_RISCV, LWDEC_RISCV_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC_RISCV, LWDEC_RISCV_SUB_WPR_ID_2_PLAYREADY_SHARED_DATA_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_1_PLAYREADY_SHARED_DATA_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_3_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_4_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

#ifdef FBFALCON_PRESENT
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_2_FRTS_DATA_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

#ifdef GSP_PRESENT
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconInstanceCount_HAL(pAcrlib, LSF_FALCON_ID_LWDEC_RISCV_EB, &instanceCount));

    for (i = 0; i < instanceCount; i++)
    {
        // RISCV-EB is for LWDEC 1 ~ 7, so we need plus 1 to get physical instance status.  
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconInstanceStatus_HAL(pAcrlib, LSF_FALCON_ID_LWDEC_RISCV_EB, i + 1 , &bInstanceEnabled));

        if (bInstanceEnabled)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, LSF_FALCON_ID_LWDEC_RISCV_EB, i, &flcnCfg));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, LWDEC_RISCV_EB_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                              ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_FALSE));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, LWDEC_RISCV_EB_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                              ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_FALSE));
        }
    }
#endif

#ifdef LWJPG_PRESENT
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconInstanceCount_HAL(pAcrlib, LSF_FALCON_ID_LWJPG, &instanceCount));

    for (i = 0; i < instanceCount; i++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconInstanceStatus_HAL(pAcrlib, LSF_FALCON_ID_LWJPG, i, &bInstanceEnabled));

        if (bInstanceEnabled)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, LSF_FALCON_ID_LWJPG, i, &flcnCfg));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, LWJPG_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                              ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_FALSE));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, LWJPG_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                              ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_FALSE));
        }
    }
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

/*!
 * @brief Reset falcon with given ID
 *        Note that here we reset and bring the falcons out of reset, so that falcons are accessible
 *        This is done for usecases like we use SEC2 mutex for general purpose on GP10X, so if we just
 *        reset sec2 and don't bring it back, the mutex acquire/release would fail
 *
 * @return     ACR_OK     If reset falcon is successful
 */
ACR_STATUS
acrResetFalcon_GH100
(
    LwU32 falconId
)
{
    ACR_STATUS       status   = ACR_OK;
    ACR_FLCN_CONFIG  flcnCfg;
    LwU32            instanceCount = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    LwU32            i;
    LwBool           bInstanceEnabled = LW_TRUE;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconInstanceCount_HAL(pAcrlib, falconId, &instanceCount));

    for (i = 0; i < instanceCount; i++)
    {
#if defined(LWDEC_RISCV_EB_PRESENT) || defined (LWJPG_PRESENT)
        LwU32  physicalInsance;

        // LWDEC-RISCV-EB is from LWDEC 1 ~ 7, so instance = 0 , we need to check LWDEC 1 in fuse register.
        if (falconId == LSF_FALCON_ID_LWDEC_RISCV_EB)
        {
            physicalInsance = i + 1;
        }
        else
        {
            physicalInsance = i;
        } 
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconInstanceStatus_HAL(pAcrlib, falconId, physicalInsance, &bInstanceEnabled));
#endif
       
        if (bInstanceEnabled)
        {
            // Get the specifics of this falconID
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, i, &flcnCfg));

            //
            // Bring falcon out of reset so that we are able to access its registers
            // It could be the case that falcon was already in reset
            //
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibBringFalconOutOfReset_HAL(pAcrlib, &flcnCfg));


            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPutFalconInReset_HAL(pAcrlib, &flcnCfg));

            //
            // Bring the falcon back from reset state
            // So that later non-secure usage of falcon can be done
            // NOTE: We would have modified FALCON_SCTL_RESET_LVLM_EN regardless
            //
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibBringFalconOutOfReset_HAL(pAcrlib, &flcnCfg));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCoreSelect_HAL(pAcrlib, &flcnCfg));
        }

    }

    return status;
}
