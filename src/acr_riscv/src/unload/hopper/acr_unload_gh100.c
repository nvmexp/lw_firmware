/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
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
#include "dev_fbif_v4.h"
#include "dev_master.h"
#include "dev_se_seb_addendum.h"
#include "config/g_acr_private.h"


/*!
 * @brief Reset falcon with given ID and instance number
 *        Note that here we reset and bring the falcons out of reset, so that falcons are accessible
 *        This is done for usecases like we use SEC2 mutex for general purpose on GP10X, so if we just
 *        reset sec2 and don't bring it back, the mutex acquire/release would fail
 *
 * @return     ACR_OK     If reset falcon is successful
 */
ACR_STATUS
acrResetFalcon_GH100(LwU32 falconId, LwU32 engInstance)
{
    ACR_STATUS       status           = ACR_OK;
    ACR_FLCN_CONFIG  flcnCfg;
    
    // Get the specifics of this falconID 
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, falconId, engInstance, &flcnCfg));

    //
    // Put the engine into reset
    // NOTE: The call below will automatically pull falcons that don't have either
    //       LW_PMC_ENABLE.<FALCON> or FALCON_ENGINE_RESET out of reset
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPutFalconInReset_HAL(pAcr, &flcnCfg));

    // Bring the engine back from reset state
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrBringFalconOutOfReset_HAL(pAcr, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCoreSelect_HAL(pAcr, &flcnCfg));

    return status;
}


/*!
 * @brief Loop to reset all engine instance for engine type
 *        This function is used to reset all engines during ACR Unload 
 *
 * @return     ACR_OK     If reset falcon is successful
 */
ACR_STATUS
acrResetAllEngineInstances_GH100(LwU32 falconId)
{
    ACR_STATUS       status           = ACR_OK;
    LwU32            instanceCount    = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    LwU32            instance         = 0;
    LwBool           bInstanceEnabled = 0;

    //
    // GPCCS engine follows the logical addressing config. This means that SW sees the logical GPC mask changes so instead
    // of validating via physical engineID masks, we reset falcon instances based on number of active instances in HW.
    //
    if (falconId == LSF_FALCON_ID_GPCCS)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetNumActiveGpccsInstances_HAL(pAcrlib, &instanceCount));
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconInstanceCount_HAL(pAcrlib, falconId, &instanceCount));
    }

    for (instance = 0; instance < instanceCount; instance++)
    {
        if (falconId != LSF_FALCON_ID_GPCCS)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconInstanceStatus_HAL(pAcrlib, falconId, instance, &bInstanceEnabled));

            if (!bInstanceEnabled)
            {
                continue;
            }
        }

        acrResetFalcon_HAL(pAcr, falconId, instance);
    }

    return status;
}

/* !
 * @brief: Scrub WPR region
 *
 *@return : ACR_OK
 */
ACR_STATUS
acrUnloadScrubWprRegion_GH100
(
    LwU32 startAddr4k,
    LwU32 endAddr4k
)
{
    ACR_STATUS status       = ACR_OK;
    // Random pattern
    LwU32 pattern           = 0xdeaddead;
    LwU32 trigger           = 0;
    LwU8  hwScrubMutexToken = 0;


    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAcquireSelwreMutex_HAL(pAcr,
                                        SELWRE_MUTEX_HS_HW_SCRUBBER,
                                        &hwScrubMutexToken));

    ACR_REG_WR32(BAR0, LW_PFB_NISO_SCRUB_START_ADDR,    startAddr4k);
    ACR_REG_WR32(BAR0, LW_PFB_NISO_SCRUB_END_ADDR,      endAddr4k);
    ACR_REG_WR32(BAR0, LW_PFB_NISO_SCRUB_PATTERN,       pattern);

    trigger = FLD_SET_DRF(_PFB, _NISO_SCRUB_TRIGGER, _FLAG,      _ENABLED,      trigger);
    trigger = FLD_SET_DRF(_PFB, _NISO_SCRUB_TRIGGER, _DIRECTION, _LOW_TO_HIGH,  trigger);

    ACR_REG_WR32(BAR0, LW_PFB_NISO_SCRUB_TRIGGER, trigger);

    //
    // Wait till this scrubbing is done
    // Not adding timeout considering WPR should not be released untill WPR is
    // completely scrubbed
    //
    while (LW_PFB_NISO_SCRUB_STATUS_FLAG_PENDING ==
            REF_VAL(LW_PFB_NISO_SCRUB_STATUS_FLAG, ACR_REG_RD32(BAR0, LW_PFB_NISO_SCRUB_STATUS)));


    return acrReleaseSelwreMutex_HAL(pAcr, SELWRE_MUTEX_HS_HW_SCRUBBER, hwScrubMutexToken);
}

/*!
 * @brief Unlocks MMU regions associated with the given WPR region
 *
 * @return     ACR_OK     If programming MMU registers are successful
 */
ACR_STATUS
acrUnlockWpr_GH100(LwU32 wprIndex)
{
    ACR_STATUS status       = ACR_OK;
    LwU32      data         = 0;
    LwU32      startAddr4k  = 0;
    LwU64      endAddr4k    = 0;

#if defined(LWDEC_RISCV_EB_PRESENT) || defined(LWJPG_PRESENT)
    LwU32            instance         = 0;
    LwU32            instanceCount    = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;
    LwBool           bInstanceEnabled = LW_FALSE;
    ACR_FLCN_CONFIG  flcnCfg;
#endif

    if (wprIndex == ACR_WPR1_REGION_IDX)
    {
#ifdef ACR_RISCV_LS
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, PMU_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, PMU_SUB_WPR_ID_2_UCODE_DATA_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FECS, LSF_FALCON_INSTANCE_DEFAULT_0, FECS_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FECS, LSF_FALCON_INSTANCE_DEFAULT_0, FECS_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LSF_FALCON_INSTANCE_DEFAULT_0, LWDEC0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LSF_FALCON_INSTANCE_DEFAULT_0, LWDEC0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LSF_FALCON_INSTANCE_DEFAULT_0, LWDEC0_SUB_WPR_ID_2_PLAYREADY_SHARED_DATA_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, LSF_FALCON_INSTANCE_DEFAULT_0, SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, LSF_FALCON_INSTANCE_DEFAULT_0, SEC2_SUB_WPR_ID_1_PLAYREADY_SHARED_DATA_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, LSF_FALCON_INSTANCE_DEFAULT_0, SEC2_SUB_WPR_ID_3_UCODE_CODE_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, LSF_FALCON_INSTANCE_DEFAULT_0, SEC2_SUB_WPR_ID_4_UCODE_DATA_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

#ifdef FBFALCON_PRESENT
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, LSF_FALCON_INSTANCE_DEFAULT_0, FBFALCON_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, LSF_FALCON_INSTANCE_DEFAULT_0, FBFALCON_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

#ifdef LWDEC_RISCV_EB_PRESENT
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconInstanceCount_HAL(pAcr, LSF_FALCON_ID_LWDEC_RISCV_EB, &instanceCount));

        for (instance = 0; instance < instanceCount; instance++)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconInstanceStatus_HAL(pAcr, LSF_FALCON_ID_LWDEC_RISCV_EB, instance , &bInstanceEnabled));

            if (bInstanceEnabled)
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, LSF_FALCON_ID_LWDEC_RISCV_EB, instance, &flcnCfg));

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, LWDEC_RISCV_EB_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                                ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK));

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, LWDEC_RISCV_EB_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                                ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK));

            }
        }
#endif // LWDEC_RISCV_EB_PRESENT

#ifdef LWJPG_PRESENT
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconInstanceCount_HAL(pAcr, LSF_FALCON_ID_LWJPG, &instanceCount));

        for (instance = 0; instance < instanceCount; instance++)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconInstanceStatus_HAL(pAcr, LSF_FALCON_ID_LWJPG, instance, &bInstanceEnabled));

            if (bInstanceEnabled)
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetFalconConfig_HAL(pAcr, LSF_FALCON_ID_LWJPG, instance, &flcnCfg));

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, LWJPG_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1,
                                                ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK));

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWprExt_HAL(pAcr, &flcnCfg, LWJPG_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1,
                                                ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK));
            }
        }
#endif // LWJPG_PRESENT

        //
        // Breakdown WPR1 in MMU and secure scratch settings
        //

        startAddr4k     = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL,
                                ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO));
        endAddr4k       = DRF_VAL(_PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL,
                                ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI));

        // Add ACR_REGION_ALIGN since granularity of WPR is 128KB and get the actual endAddr4k.
        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS((endAddr4k << LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT), ACR_REGION_ALIGN, LW_U64_MAX);
        endAddr4k   = ((endAddr4k << LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT) + ACR_REGION_ALIGN - 1) >> LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT;

        // Scrub WPR region before destroying
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrUnloadScrubWprRegion_HAL(pAcr, startAddr4k, (LwU32)endAddr4k));

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
    }
    else if (wprIndex == ACR_WPR2_REGION_IDX)
    {
#ifdef ACR_RISCV_LS
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, PMU_SUB_WPR_ID_1_FRTS_DATA_WPR2,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

#ifdef FBFALCON_PRESENT
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, LSF_FALCON_INSTANCE_DEFAULT_0, FBFALCON_SUB_WPR_ID_2_FRTS_DATA_WPR2,
                                            ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));
#endif

        // Unlock WPR2 Region

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR2,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, LSF_FALCON_INSTANCE_DEFAULT_0, GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR2,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_TRUE));

        startAddr4k     = DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL,
                                ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO));
        endAddr4k       = DRF_VAL(_PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL,
                                ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI));

        // Add ACR_REGION_ALIGN since granularity of WPR is 128KB and get the actual endAddr4k.
        CHECK_UNSIGNED_INTEGER_ADDITION_AND_RETURN_IF_OUT_OF_BOUNDS((endAddr4k << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT), ACR_REGION_ALIGN, LW_U64_MAX);
        endAddr4k   = ((endAddr4k << LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT) + ACR_REGION_ALIGN - 1) >> LW_PFB_PRI_MMU_WPR2_ADDR_HI_ALIGNMENT;                   

        // Return if WPR2 is not allocated
        if (startAddr4k > endAddr4k)
        {
            return status;
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrUnloadScrubWprRegion_HAL(pAcr, startAddr4k, (LwU32)endAddr4k));

        data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO);
        data = FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, ILWALID_WPR_ADDR_LO, data);
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO, data);

        data =  ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI);
        data= FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, ILWALID_WPR_ADDR_HI, data);
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI, data);

        data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
        data =  FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_READ, _WPR2, ACR_UNLOCK_READ_MASK, data);
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ, data);

        data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);
        data =  FLD_SET_DRF_NUM( _PFB, _PRI_MMU_WPR_ALLOW_WRITE, _WPR2, ACR_UNLOCK_WRITE_MASK, data);
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE, data);
    }    

    return status;
}
