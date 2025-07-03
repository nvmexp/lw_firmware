/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_prtu10x.c
 * @brief  Contains all PlayReady routines specific to SEC2 TU10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "mmu/mmucmn.h"
#include "dev_lwdec_pri.h"
#include "dev_fb.h"
#include "dev_therm.h"
#include "dev_therm_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pr/pr_common.h"

#include "dev_lwdec_pri.h"
#include "oemcommonlw.h"
#include "drmtypes.h"

#include "sec2_bar0.h"
#include "sec2_bar0_hs.h"
#include "sec2_objsec2.h"
#include "oemteecryptointernaltypes.h"

#ifndef PR_V0404
#include "playready/3.0/lw_playready.h"
#else
#include "playready/4.4/lw_playready.h"
#endif

#include "config/g_pr_private.h"
#include "sec2_objsec2.h"
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
// ModelName:          "TU102"
// ModelNumber:        "TESTTESTTEST" (TODO - Lwrrently unsure of what value to put here at
//                     the moment:  Should it come from Boot 42?)
//
static LwU16 s_rgwchManufacturerName_TU10X[] PR_ATTR_DATA_OVLY(_s_rgwchManufacturerName_TU10X) = { DRM_WCHAR_CAST('N'), DRM_WCHAR_CAST('v'), DRM_WCHAR_CAST('i'), DRM_WCHAR_CAST('d'), DRM_WCHAR_CAST('i'), DRM_WCHAR_CAST('a'), DRM_WCHAR_CAST('\0') };
static LwU16 s_rgwchModelName_TU10X[] PR_ATTR_DATA_OVLY(_s_rgwchModelName_TU10X) = { DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('U'), DRM_WCHAR_CAST('1'), DRM_WCHAR_CAST('0'), DRM_WCHAR_CAST('2'), DRM_WCHAR_CAST('\0') };
static LwU16 s_rgwchModelNumber_TU10X[] PR_ATTR_DATA_OVLY(_s_rgwchModelNumber_TU10X) = { DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('\0') };

/* ------------------------- Public Functions ------------------------------- */
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
prGetTeeVersionInfo_TU10X
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
    *ppManufacturerName         = s_rgwchManufacturerName_TU10X;
    *pManufacturerNameCharCount = sizeof(s_rgwchManufacturerName_TU10X) / sizeof(LwU16);
    *ppModelName                = s_rgwchModelName_TU10X;
    *pModelNameCharCount        = sizeof(s_rgwchModelName_TU10X) / sizeof(LwU16);
    *ppModelNumber              = s_rgwchModelNumber_TU10X;
    *pModelNumberCharCount      = sizeof(s_rgwchModelNumber_TU10X) / sizeof(LwU16);
}
/*!
 * @brief  Checks whether LWDEC is in LS mode or not
 *
 * @param[out]  pbInLsMode          The pointer stores whether LWDEC in LS mode or not
 *
 * @return:
 *              returns FLN_ERR of register read
 */
FLCN_STATUS
prIsLwdecInLsMode_TU10X
(
    LwBool *pbInLsMode
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32 data32           = 0;

    if(!pbInLsMode)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
    *pbInLsMode = LW_FALSE;

    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PLWDEC_FALCON_SCTL(0), &data32));
    *pbInLsMode = FLD_TEST_DRF(_PLWDEC, _FALCON_SCTL, _LSMODE, _TRUE, data32);

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  Fill up the shared data region details
 *         For Turing_and_later, we have setup a dedicated subWPR to share the data only between SEC2 and LWDEC selwrely
 *         In the subWPR, first we have SHARED_DATA_OFFSETS, and then actual shared data
 *
 * @param[in]      sharedRegionStartOption - To specify whether to share start address of struct or actual data
 * @param[in/out]  pPrSharedStructOrDataFB - The pointer points to the start address of either struct or actual data
 *
 * @return FLCN_OK                         - if there are no errors
 *         FLCN_ERR_ILWALID_ARGUMENT       - if invalid arguments are passed
 *         FLCN_ERR_BAR0_PRIV_READ_ERROR   - if errors olwrred reading registers
 */
FLCN_STATUS
prGetSharedDataRegionDetails_TU10X
(
    SHARED_REGION_START_OPTION  sharedRegionStartOption,
    LwU64                      *pPrSharedStructOrDataFB
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32       prSharedSubWprStartAddr;

    if (pPrSharedStructOrDataFB == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Get the start address of PR shared region from the programmed subWpr
    CHECK_FLCN_STATUS(BAR0_REG_RD32_ERRCHK(LW_PFB_PRI_MMU_FALCON_SEC_CFGA(SEC2_SUB_WPR_ID_1_PLAYREADY_SHARED_DATA_WPR1), &prSharedSubWprStartAddr));
    prSharedSubWprStartAddr = DRF_VAL(_PFB, _PRI_MMU_FALCON_SEC_CFGA, _ADDR_LO, prSharedSubWprStartAddr);

    if (sharedRegionStartOption == PR_GET_SHARED_STRUCT_START)
    {
        *pPrSharedStructOrDataFB = (LwU64)prSharedSubWprStartAddr << SHIFT_4KB;
    }
    else if (sharedRegionStartOption == PR_GET_SHARED_DATA_START)
    {
        *pPrSharedStructOrDataFB = (((LwU64)prSharedSubWprStartAddr << SHIFT_4KB) + sizeof(SHARED_DATA_OFFSETS));
    }
    else
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  This is HS version of @ref prGetSharedDataRegionDetailsHS_TU10X
 */
FLCN_STATUS
prGetSharedDataRegionDetailsHS_TU10X
(
    SHARED_REGION_START_OPTION  sharedRegionStartOption,
    LwU64                      *pPrSharedStructOrDataFB
)
{
    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;
    LwU32       prSharedSubWprStartAddr;

    if (pPrSharedStructOrDataFB == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Get the start address of PR shared region from the programmed subWpr
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_PFB_PRI_MMU_FALCON_SEC_CFGA(SEC2_SUB_WPR_ID_1_PLAYREADY_SHARED_DATA_WPR1), &prSharedSubWprStartAddr));
    prSharedSubWprStartAddr = DRF_VAL(_PFB, _PRI_MMU_FALCON_SEC_CFGA, _ADDR_LO, prSharedSubWprStartAddr);

    if (sharedRegionStartOption == PR_GET_SHARED_STRUCT_START)
    {
        *pPrSharedStructOrDataFB = (LwU64)prSharedSubWprStartAddr << SHIFT_4KB;
    }
    else if (sharedRegionStartOption == PR_GET_SHARED_DATA_START)
    {
        *pPrSharedStructOrDataFB = (((LwU64)prSharedSubWprStartAddr << SHIFT_4KB) + sizeof(SHARED_DATA_OFFSETS));
    }
    else
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    flcnStatus = FLCN_OK;
ErrorExit:
    return flcnStatus;
}

/*!
 * @brief  Initialize PR shared struct between SEC2 and LWDEC
 *         For Turing_and_later, we have setup a dedicated subWPR to share the data only between SEC2 and LWDEC selwrely
 *         In the subWPR, first we have SHARED_DATA_OFFSETS, and then actual shared data 
 */
void
prInitializeSharedStructHS_TU10X(void)
{
    LwU32 prSharedDataStartDMEM = LWOS_DATA_SYM_IMAGE_LOC_TO_DMEM_LOC((LwU32)_load_start_dmem_prShared);

    g_sharedDataOffsets.offsG_rgoCache     = (LwU32)g_rgoCache      - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS);
    g_sharedDataOffsets.offsG_iHead        = (LwU32)&g_iHead        - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS);
    g_sharedDataOffsets.offsG_iTail        = (LwU32)&g_iTail        - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS);
    g_sharedDataOffsets.offsG_iFree        = (LwU32)&g_iFree        - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS);
    g_sharedDataOffsets.offsG_cCache       = (LwU32)&g_cCache       - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS);
    g_sharedDataOffsets.offsG_fInitialized = (LwU32)&g_fInitialized - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS);
}

/*!
 * @brief  return whether the shared struct data is initialized
 */
LwBool
prIsSharedStructInitialized_TU10X(void)
{
    LwU32 prSharedDataStartDMEM = LWOS_DATA_SYM_IMAGE_LOC_TO_DMEM_LOC((LwU32)_load_start_dmem_prShared);

    return ((REG_RD32(BAR0, LW_THERM_SELWRE_WR_SCRATCH_SEC2_LWDEC_SHARED_STRUCT) != 0) &&
            (g_sharedDataOffsets.offsG_rgoCache     == ((LwU32)g_rgoCache      - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS))) &&
            (g_sharedDataOffsets.offsG_iHead        == ((LwU32)&g_iHead        - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS))) &&
            (g_sharedDataOffsets.offsG_iTail        == ((LwU32)&g_iTail        - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS))) &&
            (g_sharedDataOffsets.offsG_iFree        == ((LwU32)&g_iFree        - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS))) &&
            (g_sharedDataOffsets.offsG_cCache       == ((LwU32)&g_cCache       - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS))) &&
            (g_sharedDataOffsets.offsG_fInitialized == ((LwU32)&g_fInitialized - prSharedDataStartDMEM + sizeof(SHARED_DATA_OFFSETS))));
}
