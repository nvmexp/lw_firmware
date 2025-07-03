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
 * @file   sec2_objlsf.c
 * @brief  Container object for the SEC2 Light Secure Falcon routines
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>
#include <dev_top.h>
#include <riscv-intrinsics.h>

// MK TODO: this is required to link with SafeRTOS core (that we must link with).
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>

#include <portfeatures.h>
#include <task.h>
#include <lwrtos.h>
#include "drivers/drivers.h"
/* ------------------------ Module Includes -------------------------------- */

#include "config/g_lsf_hal.h"

/*!
 * @brief Initializes Light Secure Falcon settings.
 */
sysKERNEL_CODE void
lsfPreInit(void)
{
    // Setup aperture settings (protected TRANSCFG registers)
    lsfInitApertureSettings_HAL();

    // Setup MTHD and CTX settings.
    lsfSetupMthdctx_HAL();
}
