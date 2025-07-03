/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_objlsf.c
 * @brief  Container object for the SOE Light Secure Falcon routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objsoe.h"
#include "soe_objlsf.h"

#include "rmlsfm.h"

#include "dev_soe_csb.h"

#include "config/g_lsf_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
LSF_HAL_IFACES LsfHal;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

void
constructLsf(void)
{
    IfaceSetup->lsfHalIfacesSetupFn(&LsfHal);
}

/*!
 * @brief Initializes Light Secure Falcon settings.
 */
void
lsfPreInit(void)
{
    //HALT if HS was not run.
    if (!lsfVerifyFalconSelwreRunMode_HAL())
    {
        // Write a falcon mode token to signal the inselwre condition.
        REG_WR32(CSB, LW_CSOE_FALCON_MAILBOX0,
             LSF_FALCON_MODE_TOKEN_FLCN_INSELWRE);

        SOE_HALT();
    }

    // Setup aperture settings (protected TRANSCFG registers)
    lsfInitApertureSettings_HAL();

    // Setup MTHDCTX PRIV level mask
    lsfSetupMthdctxPrivMask_HAL();

    // Ensure that PRIV_SEC is enabled
    (void)lsfVerifyPrivSecEnabled_HAL();
}
