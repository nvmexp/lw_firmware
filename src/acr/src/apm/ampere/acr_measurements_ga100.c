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
 * @file: acr_measurements_ga100.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "acrapm.h"
#include "sw_rts_msr_usage.h"
#include "dev_fuse.h"
#include "dev_fb.h"
#include "dev_sec_pri.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif



/* ------------------------ External Definitions ---------------------------- */
extern LwU32 g_offsetApmRts;

/* ------------------------- Global Variables ------------------------------- */
LwU8  g_newMeasurement[APM_HASH_SIZE_BYTE]    ATTR_ALIGNED(0x10);

/* ------------------------- Private Functions ------------------------------ */
static ACR_STATUS acrMeasureLwDecVersionFuses(void) ATTR_OVLY(OVL_NAME);
static ACR_STATUS acrMeasureSec2VersionFuses(void)  ATTR_OVLY(OVL_NAME);
static ACR_STATUS acrMeasurePmuVersionFuses(void)   ATTR_OVLY(OVL_NAME);
static ACR_STATUS acrMeasureGspVersionFuses(void)   ATTR_OVLY(OVL_NAME);

/**
 * @brief The function measures the state of various fuses and registers set at
 *        boot that are related to Confidential Compute. These measurements
 *        extend APM_RTS_MSR_INDEX_FUSES MSR.
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise
 */
ACR_STATUS
acrMeasureCCState_GA100
(
)
{
    ACR_STATUS status                  = ACR_OK;
    LwU32      *measurement            = (LwU32 *) g_newMeasurement;

    acrMemsetByte(g_newMeasurement, sizeof(g_newMeasurement), 0x00);

    measurement[0] = ACR_REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_20_CC);
    
    //
    // We are using the fuses MSR because this state is static from the
    // viewpoint of ACR. It is set by VBIOS at boot-time.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    return status;
}

/**
 * @brief The function measures the state of LWDEC ucode version fuses and
 *        stores it in APM_RTS_MSR_INDEX_FUSES MSR
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise
 */
static ACR_STATUS
acrMeasureLwDecVersionFuses
()
{
    ACR_STATUS status                  = ACR_OK;
    LwU32      *measurement            = (LwU32 *) g_newMeasurement;

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE1_VERSION);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE2_VERSION);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE3_VERSION);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE4_VERSION);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE5_VERSION);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE6_VERSION);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE7_VERSION);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE8_VERSION);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE9_VERSION);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE10_VERSION);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE11_VERSION);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE12_VERSION);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE13_VERSION);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE14_VERSION);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE15_VERSION);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_LWDEC_UCODE16_VERSION);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    return status;
}

/**
 * @brief The function measures the state of SEC2 ucode version fuses and
 *        stores it in APM_RTS_MSR_INDEX_FUSES MSR
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise
 */
static ACR_STATUS
acrMeasureSec2VersionFuses
()
{
    ACR_STATUS status                  = ACR_OK;
    LwU32      *measurement            = (LwU32 *) g_newMeasurement;

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE1_VERSION);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE2_VERSION);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE3_VERSION);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE4_VERSION);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE5_VERSION);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE6_VERSION);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE7_VERSION);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE8_VERSION);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE9_VERSION);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE10_VERSION);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE11_VERSION);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE12_VERSION);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE13_VERSION);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE14_VERSION);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE15_VERSION);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_SEC2_UCODE16_VERSION);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    return status;
}

/**
 * @brief The function measures the state of PMU ucode version fuses and
 *        stores it in APM_RTS_MSR_INDEX_FUSES MSR
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise
 */
static ACR_STATUS
acrMeasurePmuVersionFuses
()
{
    ACR_STATUS status                  = ACR_OK;
    LwU32      *measurement            = (LwU32 *) g_newMeasurement;

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE1_VERSION);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE2_VERSION);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE3_VERSION);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE4_VERSION);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE5_VERSION);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE6_VERSION);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE7_VERSION);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE8_VERSION);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE9_VERSION);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE10_VERSION);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE11_VERSION);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE12_VERSION);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE13_VERSION);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE14_VERSION);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE15_VERSION);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_PMU_UCODE16_VERSION);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    return status;
}

/**
 * @brief The function measures the state of GSP ucode version fuses and
 *        stores it in APM_RTS_MSR_INDEX_FUSES MSR
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise
 */
static ACR_STATUS
acrMeasureGspVersionFuses
()
{
    ACR_STATUS status                  = ACR_OK;
    LwU32      *measurement            = (LwU32 *) g_newMeasurement;

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE1_VERSION);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE2_VERSION);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE3_VERSION);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE4_VERSION);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE5_VERSION);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE6_VERSION);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE7_VERSION);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE8_VERSION);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE9_VERSION);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE10_VERSION);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE11_VERSION);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE12_VERSION);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE13_VERSION);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE14_VERSION);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE15_VERSION);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_FPF_GSP_UCODE16_VERSION);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    return status;
}

/**
 * @brief The function computes the MSR index where the LS hash should be stored
 * 
 * @param msrIdx[in]   The Falcon Id of the hash being measured
 * @param bIsUcode[in] UCode hash - TRUE; Data hash - FALSE
 * @param msrIdx[out]  The pointer to the LwU32 where the result is stored
 * @return ACR_STATUS  ACR_OK if successful, relevant error otherwise
 */
ACR_STATUS acrFalconIdToMsrId_GA100
(
    LwU32  falconId,
    LwBool bIsUcode,
    LwU32  *msrIdx
)
{
    if (falconId == LSF_FALCON_ID_ILWALID || falconId >= LSF_FALCON_ID_END)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (falconId)
    {
        case LSF_FALCON_ID_PMU:
            *msrIdx = APM_RTS_MSR_INDEX_FALCON_PMU;
            break;
        case LSF_FALCON_ID_FECS:
            *msrIdx = APM_RTS_MSR_INDEX_FALCON_FECS;
            break;
        case LSF_FALCON_ID_GPCCS:
            *msrIdx = APM_RTS_MSR_INDEX_FALCON_GPCCS;
            break;
        case LSF_FALCON_ID_LWDEC:
            *msrIdx = APM_RTS_MSR_INDEX_FALCON_LWDEC;
            break;
        case LSF_FALCON_ID_SEC2:
            *msrIdx = APM_RTS_MSR_INDEX_FALCON_SEC2;
            break;
        default:
            return ACR_ERROR_ILWALID_ARGUMENT;
            break;  
    }
    return ACR_OK;
}


/**
 * @brief This function measures the provided DM hash and extends the MSR
 *        determined by the falconId and the source of the hash (uCode/data)
 * 
 * @param g_dmHash[in] The pointer to the DM hash
 * @param falconId[in] The falcon corresponding to the DM hash
 * @param bIsUcode[in] Is the DM hash of uCode or data?
 * @return ACR_STATUS  ACR_OK if successful, relevant error otherwise
 */
ACR_STATUS
acrMeasureDmhash_GA100
(
    LwU8  *g_dmHash, 
    LwU32  falconId, 
    LwBool bIsUcode
)
{
    ACR_STATUS      status      = ACR_OK;
    LwU32           msrIdx      = 0;
    
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFalconIdToMsrId_HAL(pAcr, falconId, bIsUcode, &msrIdx));

    acrMemsetByte(g_newMeasurement, sizeof(g_newMeasurement), 0x00);
    acrlibMemcpy_HAL(pAcrlib, g_newMeasurement, g_dmHash, ACR_AES128_DMH_SIZE_IN_BYTES);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(msrIdx, g_newMeasurement, LW_FALSE));
    return status;
}

/**
 * @brief The function measures the state of ucode version fuses and stores it
 *        in APM_RTS_MSR_INDEX_FUSES MSR
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise
 */
ACR_STATUS
acrMeasureEngineVersionFuses_GA100
(
)
{
    ACR_STATUS status                  = ACR_OK;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMeasureLwDecVersionFuses());
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMeasureSec2VersionFuses());
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMeasurePmuVersionFuses());
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMeasureGspVersionFuses());

    return status;
}

/**
 * @brief The function measures the state of miscellaneous fuses and stores it
 *        in APM_RTS_MSR_INDEX_FUSES MSR
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise
 */
ACR_STATUS
acrMeasureFuses_GA100
(
)
{
    ACR_STATUS status                  = ACR_OK;
    LwU32      *measurement            = (LwU32 *) g_newMeasurement;
    //
    // Measure the first batch of fuses
    //
    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_PRIV_SEC_EN);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SELWRE_PRI_SOURCE_ISOLATION_EN);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SELWRE_LWDEC_DEBUG_DIS);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SELWRE_MINION_DEBUG_DIS);
    measurement[6] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SELWRE_GSP_DEBUG_DIS);
    measurement[7] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SELWRE_XUSB_DEBUG_DIS);
    
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    //
    // Measure the second batch of fuses
    //
    acrMemsetByte(g_newMeasurement, sizeof(g_newMeasurement), 0x00);

    measurement[0] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_VPR_ENABLED);
    measurement[1] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SW_VPR_ENABLED);
    measurement[2] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_WPR_ENABLED);
    measurement[3] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_SUB_REGION_NO_OVERRIDE);

    measurement[4] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_ECC_EN);
    measurement[5] = ACR_REG_RD32(BAR0, LW_FUSE_OPT_ROW_REMAPPER_EN);
    
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_FUSES, g_newMeasurement, LW_FALSE));

    return status;
}

/**
 * @brief This function measuresWPR MMU registers and extends the 
 *        corresponding MSR
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise.
 */
ACR_STATUS
acrMeasureWprMmuState_GA100
(
)
{
    ACR_STATUS   status               = ACR_OK;
    LwU32       *measurement          = (LwU32 *) g_newMeasurement;

    acrMemsetByte(g_newMeasurement, sizeof(g_newMeasurement), 0x00);

    //
    // Add WPR MMU permissions to the measurement buffer
    //
    measurement[0] = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ);
    measurement[1] = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMsrExtend(APM_RTS_MSR_INDEX_WPR_MMU_STATE, g_newMeasurement, LW_FALSE));

    return status;
}

/**
 * @brief The function measures the fuses, MMU state and decode traps
 * 
 * @return ACR_STATUS ACR_OK if successful, relevant error otherwise
 */
ACR_STATUS
acrMeasureStaticState_GA100
(
)
{
    ACR_STATUS status = ACR_OK;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMeasureFuses_HAL(pAcr));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMeasureCCState_HAL(pAcr));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMeasureEngineVersionFuses_HAL(pAcr));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrMeasureWprMmuState_HAL(pAcr));

    return status;
}