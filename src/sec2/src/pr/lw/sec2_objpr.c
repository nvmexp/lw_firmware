/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_objpr.c
 * @brief  Container-object for the SEC2 PLAYREADY routines.  Contains
 *         generic non-HAL interrupt-routines plus logic required to hook-up
 *         chip-specific HAL-routines.
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "sec2sw.h"
#include "lwuproc.h"
/* ------------------------ Application Includes --------------------------- */
#include "flcntypes.h"
#include "sec2_objhal.h"
#include "sec2_objpr.h"
#include "lwosselwreovly.h"
#include "config/g_pr_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief  Retrieve the device cert template according to the board configuration
 *
 * @param[in]  ppCert           The pointer points the buffer storing the address
 *                              of the cert template
 * @param[in]  pCertSize        The pointer points the buffer storing the size of
 *                              the cert template
 * @param[in]  pDevCertIndices  The pointer points the buffer storing the field
 *                              indices into the cert template
 */
void
prGetDeviceCertTemplate
(
    LwU8  **ppCert,
    LwU32  *pCertSize,
    void   *pDevCertIndices
)
{
#ifndef PR_SL3000_ENABLED
    prGetSL150DeviceCertTemplate_HAL(&PrHal, ppCert, pCertSize, pDevCertIndices);
#else
    if (OS_SEC_FALC_IS_DBG_MODE())
    {
        prGetSL150DeviceCertTemplate_HAL(&PrHal, ppCert, pCertSize, pDevCertIndices);
    }
    else
    {
        prGetSL3000DeviceCertTemplate_HAL(&PrHal, ppCert, pCertSize, pDevCertIndices);
    }
#endif
}
