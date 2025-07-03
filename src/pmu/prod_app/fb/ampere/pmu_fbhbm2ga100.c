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
 * @file   pmu_fbhbm2ga100.c
 * @brief  FB Hal Functions for HBM on GA100
 *
 * FB Hal Functions implement FB related functionalities for GA100
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "main.h"
#include "dev_fuse.h"
#include "dev_top.h"
#include "dev_fbpa_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objfb.h"
#include "pmu_objfuse.h"
#include "config/g_fb_private.h"
#include "therm/objtherm.h"
#include "lib/lib_fxp.h"

/* ------------------------- Static function Prototypes ---------------------*/
static FLCN_STATUS s_applyTemperatureOffset(LwTemp lwrrTemp, LwTemp *pOffsettedTemp);

/* ------------------------- Macros -----------------------------------------*/
#define FB_FBPAS_PER_HBM_SITE_CNT  4

/* ------------------------- Static Variables ------------------------------- */
/*
    HBM site to FBPA mapping:
        Site A (site ID 0): FBPA  0/ 1/ 4/ 5
        Site B (site ID 1): FBPA  2/ 3/ 8/ 9
        Site C (site ID 2): FBPA  6/ 7/10/11
        Site D (site ID 3): FBPA 14/15/18/19
        Site E (site ID 4): FBPA 12/13/22/23
        Site F (site ID 5): FBPA 16/17/20/21
*/
GCC_ATTRIB_SECTION("dmem_therm", "siteIdxFbpaIdxMapping")
static const LwU8 siteIdxFbpaIdxMapping[LW_PFB_HBM_SITE_COUNT][FB_FBPAS_PER_HBM_SITE_CNT] = {
    {  0,  1,  4,  5, },
    {  2,  3,  8,  9, },
    {  6,  7, 10, 11, },
    { 14, 15, 18, 19, },
    { 12, 13, 22, 23, },
    { 16, 17, 20, 21, },
};

/* ------------------------- Public Functions  ------------------------------ */
/*!
 * @brief Gets the FBPA index corresponding to the first non floorswept FBPA
 *        on a given HBM site.
 *
 * This function gets the index corresponding to the first non floorswept
 * FBPA on a given HBM site
 *
 * @param[in] siteIndex     HBM site index
 * 
 */
LwU8
fbHBMSiteNonFloorsweptFBPAIdxGet_GA100
(
    LwU8 siteIndex
)
{
    LwU32 fbpaCount;
    LwU32 activeFBPAMask;
    LwU8  i;

    // Retrieve the total number of FBPAs.
    fbpaCount = REG_RD32(BAR0, LW_PTOP_SCAL_NUM_FBPAS);
    fbpaCount = DRF_VAL(_PTOP, _SCAL_NUM_FBPAS, _VALUE, fbpaCount);

    //
    // Retrieve the active FBPA/FBIO bitmask. LW_FUSE_STATUS_OPT_FBPA
    // is removed from GA10X onwards (bug 200228617) so do not use it.
    //
    activeFBPAMask = fuseRead(LW_FUSE_STATUS_OPT_FBIO);
    activeFBPAMask = DRF_VAL(_FUSE, _STATUS_OPT_FBIO, _DATA, activeFBPAMask);

    // Value is flipped since the value of an enabled FBPA is 0 in HW.
    activeFBPAMask = ~activeFBPAMask;

    // Mask away disabled FBPAs.
    activeFBPAMask &= (BIT(fbpaCount) - 1);

    for (i = 0; i < FB_FBPAS_PER_HBM_SITE_CNT; i++)
    {
        if ((BIT(siteIdxFbpaIdxMapping[siteIndex][i]) & activeFBPAMask) != 0)
        {
            return siteIdxFbpaIdxMapping[siteIndex][i];
        }
    }

    return FB_FBPA_INDEX_ILWALID;
}

/*!
 * @brief Apply a temperature offset
 *
 * This function will callwlate and apply an offset to the reported
 * temperature based on desired reported TJ.
 *
 * @param[in]  lwrrTemp          Current reported temperature to offset
 * @param[out] pOffsettedTemp    Final offsetted temperature
 *
 * @return FLCN_OK
 *      Offsetted temperature succesfully
 * @return FLCN_ERR_ILWALID_INDEX
 *      Memory Policy Index was incorrect
 */
static FLCN_STATUS
s_applyTemperatureOffset
(
    LwTemp lwrrTemp,
    LwTemp *pOffsettedTemp
)
{
    FLCN_STATUS status = FLCN_OK;
    THERM_POLICY *pThermPolicy;
    LwU32 vendorTjOffset;
    LwU32 maxMemSpecTj;
    LwU32 maxMemReportTj;
    LwU32 quotient;
    LwU32 offset;
    LwU8 memPolicyIdx;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
    };

    memPolicyIdx = Therm.policyMetadata.memPolicyIdx;
    if (memPolicyIdx == LW2080_CTRL_THERMAL_POLICY_INDEX_ILWALID)
    {
        *pOffsettedTemp = lwrrTemp;
        goto s_applyTemperatureOffset_exit;
    }

    pThermPolicy = THERM_POLICY_GET(Therm.policyMetadata.memPolicyIdx);
    if (pThermPolicy == NULL)
    {
        *pOffsettedTemp = lwrrTemp;
        goto s_applyTemperatureOffset_exit;
    }

    maxMemSpecTj   = thermPolicyGetMaxMemSpecTj(pThermPolicy);
    maxMemReportTj = thermPolicyGetMaxMemReportTj(pThermPolicy);
    vendorTjOffset = maxMemSpecTj - maxMemReportTj;

    //
    // Skip offset if parameters are not set, or if temperature is under 0
    //
    if ((maxMemSpecTj == 0)   ||
        (maxMemReportTj == 0) ||
        (vendorTjOffset == 0) ||
        (lwrrTemp < RM_PMU_LW_TEMP_0_C))
    {
        *pOffsettedTemp = lwrrTemp;
        goto s_applyTemperatureOffset_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Callwlate the offsetted temperature according to this equation:
        // reported_temp = IEEE1500_readout -
        //     max(roundup(IEEE1500_readout/maxHbmSpecTj * vendorOffset), 0)
        //
        quotient = (lwrrTemp * (1 << 8)) / maxMemSpecTj;
        offset   = multUFXP32(8, quotient, vendorTjOffset);

        *pOffsettedTemp = lwrrTemp - LW_MAX(0, offset);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

s_applyTemperatureOffset_exit:
    return status;
}

/*!
 * @brief Gets the temperature in celsius of a given HBM site and applies
 *        a Thermal offset.
 *
 * This function gets the temperature in 24.8 fixed point notation in [C]
 * of a given HBM site using IEEE1500 interface and applies any necessary
 * offset to the final reported temperature.
 *
 * @param[in]  fbpaIndex     Index of first non floorswept FBPA within a HBM site
 * @param[in]  provIdx       Provider index. Ignored on GV100
 * @param[out] pLwTemp       Current temperature
 *
 * @return FLCN_OK
 *      Temperature successfully obtained.
 * @return FLCN_ERR_ILWALID_STATE
 *      Error to acquire mutex or an invalid temperature data.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
fbHBMSiteTempGet_GA100
(
    LwU8    fbpaIndex,
    LwU8    provIdx,
    LwTemp *pLwTemp
)
{
    FLCN_STATUS status = FLCN_OK;
    LwTemp lwrrTemp;

    status = fbHBMSiteTempGet_GV10X(fbpaIndex, provIdx, &lwrrTemp);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto fbHBMSiteTempGet_GA100_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_TEMPERATURE_OFFSET))
    {
        status = s_applyTemperatureOffset(lwrrTemp, pLwTemp);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto fbHBMSiteTempGet_GA100_exit;
        }
    }

fbHBMSiteTempGet_GA100_exit:
    return status;
}

