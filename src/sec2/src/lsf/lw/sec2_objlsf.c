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
 * @file   sec2_objlsf.c
 * @brief  Container object for the SEC2 Light Secure Falcon routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objhal.h"
#include "sec2_objsec2.h"
#include "sec2_objlsf.h"

#include "config/g_lsf_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initializes Light Secure Falcon settings.
 */
void
lsfPreInit(void)
{
    // Setup aperture settings (protected TRANSCFG registers)
    lsfInitApertureSettings_HAL();

    // Setup MTHD and CTX settings. Update PLM if SEC2 is in LS mode
    lsfSetupMthdctx_HAL();
}
