/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_prgp10x.c
 * @brief  Contains all PlayReady routines specific to SEC2 GP10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_sec_csb.h"
#include "dev_therm.h"
#include "dev_therm_addendum.h"
#include "sec2_gpuarch.h"

/* ------------------------- Application Includes --------------------------- */
#include "pr/pr_lassahs_hs.h"
#include "pr/pr_common.h"
#include "sec2_hs.h"
#include "sec2_objsec2.h"
#include "sec2_csb.h"
#include "sec2_objpr.h"
#include "oemteecryptointernaltypes.h"
#include "oemcommonlw.h"
#include "drmtypes.h"

#ifndef PR_V0404
#include "playready/3.0/lw_playready.h"
#else
#include "playready/4.4/lw_playready.h"
#endif

#include "config/g_pr_private.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
extern SHARED_DATA_OFFSETS g_sharedDataOffsets;
extern LwU32               _load_start_dmem_prShared[];
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
//
// LWE (kwilson) Initially we will populate manufacturer/model information with hard
// coded values. These strings should be available in the model certificate.  However,
// in the current implementation AllocTeeContext calls GetVersionInformation (through
// _ValidateAndInitializeSystemInfo) before local provisioning takes place so the
// certificate chain is unavailable.  This design makes it impossible to parse the
// certificate chain to retrieve the model information before the call to AllocTeeContext.
// In the future, we desire to add parsing of the model certificate at initialization
// time.  Once that is done we will pull the values from there and drop the hardcoded
// values.  LW Bug 1838465.
//

//
// LWE (kwilson)  Updated strings for our port.
// Manufacturer Name:  "Lwpu" is absolute at the moment.
// ModelName:          "GP10X"
// ModelNumber:        "TESTTESTTEST" (TODO - Lwrrently unsure of what value to put here at
//                     the moment:  Should it come from Boot 42?)
//
static LwU16 s_rgwchManufacturerName_GP10X[] PR_ATTR_DATA_OVLY(_s_rgwchManufacturerName_GP10X) = { DRM_WCHAR_CAST('N'), DRM_WCHAR_CAST('v'), DRM_WCHAR_CAST('i'), DRM_WCHAR_CAST('d'), DRM_WCHAR_CAST('i'), DRM_WCHAR_CAST('a'), DRM_WCHAR_CAST('\0') };
static LwU16 s_rgwchModelName_GP10X[] PR_ATTR_DATA_OVLY(_s_rgwchModelName_GP10X) = { DRM_WCHAR_CAST('G'), DRM_WCHAR_CAST('P'), DRM_WCHAR_CAST('1'), DRM_WCHAR_CAST('0'), DRM_WCHAR_CAST('X'), DRM_WCHAR_CAST('\0') };
static LwU16 s_rgwchModelNumber_GP10X[] PR_ATTR_DATA_OVLY(_s_rgwchModelNumber_GP10X) = { DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('\0') };

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Reads the CSEC_FALCON_STACKCFG register and saves
 *        STACKCFG bottom byte value in global register
 *
 * @return FLCN_OK                         - if there are no errors
 *         FLCN_ERR_CSB_PRIV_READ_ERROR    - if errors olwrred reading registers
 */
FLCN_STATUS
prUpdateStackBottom_GP10X(LwBool *pbStackCfgSupported)
{
    FLCN_STATUS flcnStatus   = FLCN_OK;
    LwU32       stackCfg;

    flcnStatus = CSB_REG_RD32_HS_ERRCHK(LW_CSEC_FALCON_STACKCFG, &stackCfg);
    if (flcnStatus != FLCN_OK)
    {
        // If there are issues with the read,  then set stackCfg as unsupported.
        *pbStackCfgSupported = LW_FALSE;
    }
    else
    {
        *pbStackCfgSupported = LW_TRUE;

        // Get STACKCFG.Bottom. Bottom is in DWORDS,  colwert to bytes
        g_prLassahsData_Hs.stackCfgBottom = DRF_VAL(_CSEC, _FALCON_STACKCFG, _BOTTOM, stackCfg);
        g_prLassahsData_Hs.stackCfgBottom *= 4;
    }

    return flcnStatus;
}

/*!
 * @brief  This is the security check needed by the Playready app (except PR-specific
 *         check, sec2HsPreCheckCommon_HAL is also called for common check)
 *
 * @param[in]  bSkipDmemLv2Chk   Flag indicates whether we want to skip the lv2
 *                               PLM check for some special condition (e.g.
 *                               LASSAHS mode in PR task)
 *
 * @return FLCN_OK  if SEC2 is already running at proper configuration
 *         FLCN_ERR_HS_CHK_CHIP_NOT_SUPPORTED  if the chip doesn't support PR
 *         FLCN_ERR_HS_CHK_INTERNAL_SKU_ON_PROD  if the SKU is not internal on prod board
 *         FLCN_ERR_HS_CHK_DEVID_OVERRIDE_ENABLED_ON_PROD  if the devid override is disabled on prod board
 *         Or just bubble up the error code from the callee
 */
FLCN_STATUS
prHsPreCheck_GP10X
(
    LwBool bSkipDmemLv2Chk
)
{
    FLCN_STATUS flcnStatus       = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32       data32           = 0;
    LwBool      bProdModeEnabled = LW_FALSE;

    //
    // Ensure the chip is allowed to do Playready
    // This step must always be step 1. No bar0 read must be done before this step.
    // CSB reads are also discouraged and must be carefully reviewed before addition.
    //
    CHECK_FLCN_STATUS(sec2EnforceAllowedChipsForPlayreadyHS_HAL(&Sec2));

    //
    // After the PR-specific check is done, then do the common check.
    // This step must always be step 2 since this will check for revocation
    // which is a global property for entire ucode and not task/module specific.
    //
    CHECK_FLCN_STATUS(sec2HsPreCheckCommon_HAL(&Sec2, bSkipDmemLv2Chk));

    // Ensure that SW fusing is disabled on prod board
    CHECK_FLCN_STATUS(sec2CheckDisableSwOverrideFuseOnProdHS_HAL(&Sec2));

    bProdModeEnabled = !OS_SEC_FALC_IS_DBG_MODE();

    //
    // Ensure that SKU is not internal on prod board given that we have issued
    // HULK licenses on internal SKUs that may be detrimental to security
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_INTERNAL_SKU, &data32));
    if (bProdModeEnabled && FLD_TEST_DRF(_FUSE, _OPT_INTERNAL_SKU, _DATA, _ENABLE, data32))
    {
        flcnStatus = FLCN_ERR_HS_CHK_INTERNAL_SKU_ON_PROD;
        goto ErrorExit;
    }

    //
    // Ensure that devid override is disabled on prod board given the VPR
    // rule that a single PCI ID must correspond to a single VPR setup
    // (start and max size) to prevent reset attacks to steal VPR content
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_DEVID_SW_OVERRIDE_DIS, &data32));
    if (bProdModeEnabled && FLD_TEST_DRF(_FUSE, _OPT_DEVID_SW_OVERRIDE_DIS, _DATA, _NO, data32))
    {
        flcnStatus = FLCN_ERR_HS_CHK_DEVID_OVERRIDE_ENABLED_ON_PROD;
        goto ErrorExit;
    }

    flcnStatus = sec2DisallowDevVersionHS_HAL(&Sec2);
    if (bProdModeEnabled && (flcnStatus != FLCN_OK))
    {
        goto ErrorExit;
    }

    flcnStatus = FLCN_OK;

ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  return whether the shared struct data is initialized
 */
LwBool
prIsSharedStructInitialized_GP10X(void)
{
    return ((REG_RD32(BAR0, LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT) != 0) &&
            (g_sharedDataOffsets.offsG_rgoCache     == ((LwU32)g_rgoCache      - (LwU32)&g_sharedDataOffsets)) &&
            (g_sharedDataOffsets.offsG_iHead        == ((LwU32)&g_iHead        - (LwU32)&g_sharedDataOffsets)) &&
            (g_sharedDataOffsets.offsG_iTail        == ((LwU32)&g_iTail        - (LwU32)&g_sharedDataOffsets)) &&
            (g_sharedDataOffsets.offsG_iFree        == ((LwU32)&g_iFree        - (LwU32)&g_sharedDataOffsets)) &&
            (g_sharedDataOffsets.offsG_cCache       == ((LwU32)&g_cCache       - (LwU32)&g_sharedDataOffsets)) &&
            (g_sharedDataOffsets.offsG_fInitialized == ((LwU32)&g_fInitialized - (LwU32)&g_sharedDataOffsets)));
}

/*!
 * @brief  Retrieve the TEE version info
 *
 * @param[in/out]  ppManufacturerName          The pointer points the address of the buffer
 *                                             storing the manufacturer name for this TEE
 * @param[in/out]  pManufacturerNameCharCount  The pointer points the buffer storing the number
 *                                             of characters in manufaturer name string
 * @param[in/out]  ppModelName                 The pointer points the address of the buffer
 *                                             storing the model name for this TEE
 * @param[in/out]  pModelNameCharCount         The pointer points the buffer storing the number
 *                                             of characters in model name string
 * @param[in/out]  ppModelNumber               The pointer points the address of the buffer
 *                                             storing the model number for this TEE
 * @param[in/out]  pModelNumberCharCount       The pointer points the buffer storing the number
 *                                             of characters in model number string
 */
void
prGetTeeVersionInfo_GP10X
(
    LwU16  **ppManufacturerName,
    LwU32   *pManufacturerNameCharCount,
    LwU16  **ppModelName,
    LwU32   *pModelNameCharCount,
    LwU16  **ppModelNumber,
    LwU32   *pModelNumberCharCount
)
{
    //
    // In Playready porting kit, each element (character) in the manufalwture name,
    // model name and model number is cast to WCHAR (2 bytes) by DRM_WCHAR_CAST(x),
    // thus we need to divide with sizeof(LwU16) when counting character count
    //
    *ppManufacturerName         = s_rgwchManufacturerName_GP10X;
    *pManufacturerNameCharCount = sizeof(s_rgwchManufacturerName_GP10X) / sizeof(LwU16);
    *ppModelName                = s_rgwchModelName_GP10X;
    *pModelNameCharCount        = sizeof(s_rgwchModelName_GP10X) / sizeof(LwU16);
    *ppModelNumber              = s_rgwchModelNumber_GP10X;
    *pModelNumberCharCount      = sizeof(s_rgwchModelNumber_GP10X) / sizeof(LwU16);
}

/*!
 * @brief  Fill up the shared data region details
 *         For GP10X_thru_GV100, the shared region is part of the SEC2 LS ucode data region of WPR i.e. dmem variables
 *         This region includes g_sharedDataOffsets struct which contains offsets of shared data and also the actual data(prShared dmem overlay)
 *
 * @param[in]      sharedRegionStartOption - To specify whether to share start address of struct or actual data
 * @param[in/out]  pPrSharedStructOrDataFB - The pointer points to the start address of either struct or actual data
 *
 * @return FLCN_OK                         - if there are no errors
 *         FLCN_ERR_ILWALID_ARGUMENT       - if invalid arguments are passed
 */
FLCN_STATUS
prGetSharedDataRegionDetails_GP10X
(
    SHARED_REGION_START_OPTION  sharedRegionStartOption,
    LwU64                      *pPrSharedStructOrDataFB
)
{
    LwU32 dmb;

    if (pPrSharedStructOrDataFB == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Get the FB base of shared struct by adding the data memory base to the
    // offset of the shared struct from base of data memory (in VA space),
    // And for FB base of data, add offset of actual data(prShared dmem overlay) to the data memory base
    //
    falc_rspr(dmb, DMB);
    *pPrSharedStructOrDataFB = ((LwU64)dmb << LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT_BASE_BIT_ALIGNMENT);

    if (sharedRegionStartOption == PR_GET_SHARED_STRUCT_START)
    {
        *pPrSharedStructOrDataFB = *pPrSharedStructOrDataFB + (LwU32)&g_sharedDataOffsets;
    }
    else if (sharedRegionStartOption == PR_GET_SHARED_DATA_START)
    {
        *pPrSharedStructOrDataFB = *pPrSharedStructOrDataFB + LWOS_DATA_SYM_IMAGE_LOC_TO_DMEM_LOC((LwU32)_load_start_dmem_prShared);
    }
    else
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return FLCN_OK;
}

/*!
 * @brief This is HS version of @ref prGetSharedDataRegionDetailsHS_GP10X 
 */
FLCN_STATUS
prGetSharedDataRegionDetailsHS_GP10X
(
    SHARED_REGION_START_OPTION  sharedRegionStartOption,
    LwU64                      *pPrSharedStructOrDataFB
)   
{
    LwU32 dmb;

    if (pPrSharedStructOrDataFB == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Get the FB base of shared struct by adding the data memory base to the
    // offset of the shared struct from base of data memory (in VA space),
    // And for FB base of data, add offset of actual data(prShared dmem overlay) to the data memory base
    //
    falc_rspr(dmb, DMB);
    *pPrSharedStructOrDataFB = ((LwU64)dmb << LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT_BASE_BIT_ALIGNMENT);

    if (sharedRegionStartOption == PR_GET_SHARED_STRUCT_START)
    {
        *pPrSharedStructOrDataFB = *pPrSharedStructOrDataFB + (LwU32)&g_sharedDataOffsets;
    }
    else if (sharedRegionStartOption == PR_GET_SHARED_DATA_START)
    {
        *pPrSharedStructOrDataFB = *pPrSharedStructOrDataFB + LWOS_DATA_SYM_IMAGE_LOC_TO_DMEM_LOC((LwU32)_load_start_dmem_prShared);
    }
    else
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return FLCN_OK;
}

/*!
 * @brief  Initialize PR shared struct between SEC2 and LWDEC
 *         For GP10X_thru_GV100, the shared region is part of the SEC2 LS ucode data region of WPR i.e. dmem variables
 */
void
prInitializeSharedStructHS_GP10X(void)
{
    g_sharedDataOffsets.offsG_rgoCache     = (LwU32)g_rgoCache      - (LwU32)&g_sharedDataOffsets;
    g_sharedDataOffsets.offsG_iHead        = (LwU32)&g_iHead        - (LwU32)&g_sharedDataOffsets;
    g_sharedDataOffsets.offsG_iTail        = (LwU32)&g_iTail        - (LwU32)&g_sharedDataOffsets;
    g_sharedDataOffsets.offsG_iFree        = (LwU32)&g_iFree        - (LwU32)&g_sharedDataOffsets;
    g_sharedDataOffsets.offsG_cCache       = (LwU32)&g_cCache       - (LwU32)&g_sharedDataOffsets;
    g_sharedDataOffsets.offsG_fInitialized = (LwU32)&g_fInitialized - (LwU32)&g_sharedDataOffsets;
}

