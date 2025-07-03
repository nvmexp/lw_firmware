/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lsft23xd.c
 * @brief  Light Secure Falcon (LSF) T23XD Hal Functions
 *
 * LSF Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "gspsw.h"
#include "rmlsfm.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_lsf_hal.h"
#include "config/g_lsf_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
/*!
 * @brief  Lsf initialization.
 *
 * @return  FLCN_OK on success.
 * @return  Error code from sub-routines.
 */
sysSYSLIB_CODE FLCN_STATUS
lsfInit_T23XD(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Setup aperture settings (protected TRANSCONFIG regs)
    status = lsfInitApertureSettings_HAL(&LsfHal);
    if (status != FLCN_OK)
    {
        goto lsfInit_T23XD_exit;
    }

lsfInit_T23XD_exit:
    return status;
}

/*!
 * @brief  Setting up FBIF apertures.
 *
 * @return  FLCN_OK on success.
 */
sysSYSLIB_CODE FLCN_STATUS
lsfInitApertureSettings_T23XD(void)
{
   
    //TODO 200586859: Need to have CHEETAH implementation here
    return FLCN_OK;
}

/*!
 * Checks whether an access using the specified dmaIdx is permitted.
 *  
 * @param[in]  dmaIdx    DMA index to check
 * 
 * @return     LwBool    LW_TRUE  if the access is permitted
 *                       LW_FALSE if the access is not permitted
 */
sysSYSLIB_CODE LwBool
lsfIsDmaIdxPermitted_T23XD
(
    LwU8 dmaIdx
)
{
   //TODO 200586859: Need to have CHEETAH implementation here
   return LW_TRUE;
}
