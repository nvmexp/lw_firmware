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
 * @file   sec2_prad10x.c
 * @brief  Contains all PlayReady routines specific to SEC2 AD10X.
 */

/* ------------------------- System Includes -------------------------------- */

/* ------------------------- Application Includes --------------------------- */
#include "oemcommonlw.h"
#include "config/g_pr_private.h"
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
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
// ModelName:          "AD102"
// ModelNumber:        "TESTTESTTEST" (TODO - Lwrrently unsure of what value to put here at
//                     the moment:  Should it come from Boot 42?)
//
static LwU16 s_rgwchManufacturerName_AD10X[] PR_ATTR_DATA_OVLY(_s_rgwchManufacturerName_AD10X) = { DRM_WCHAR_CAST('N'), DRM_WCHAR_CAST('v'), DRM_WCHAR_CAST('i'), DRM_WCHAR_CAST('d'), DRM_WCHAR_CAST('i'), DRM_WCHAR_CAST('a'), DRM_WCHAR_CAST('\0') };
static LwU16 s_rgwchModelName_AD10X[] PR_ATTR_DATA_OVLY(_s_rgwchModelName_AD10X) = { DRM_WCHAR_CAST('G'), DRM_WCHAR_CAST('A'), DRM_WCHAR_CAST('1'), DRM_WCHAR_CAST('0'), DRM_WCHAR_CAST('2'), DRM_WCHAR_CAST('\0') };
static LwU16 s_rgwchModelNumber_AD10X[] PR_ATTR_DATA_OVLY(_s_rgwchModelNumber_AD10X) = { DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('E'), DRM_WCHAR_CAST('S'), DRM_WCHAR_CAST('T'), DRM_WCHAR_CAST('\0') };

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
prGetTeeVersionInfo_AD10X
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
    *ppManufacturerName         = s_rgwchManufacturerName_AD10X;
    *pManufacturerNameCharCount = sizeof(s_rgwchManufacturerName_AD10X) / sizeof(LwU16);
    *ppModelName                = s_rgwchModelName_AD10X;
    *pModelNameCharCount        = sizeof(s_rgwchModelName_AD10X) / sizeof(LwU16);
    *ppModelNumber              = s_rgwchModelNumber_AD10X;
    *pModelNumberCharCount      = sizeof(s_rgwchModelNumber_AD10X) / sizeof(LwU16);
}
