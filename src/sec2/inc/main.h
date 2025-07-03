/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef MAIN_H
#define MAIN_H

/*!
 * @file main.h
 */

/* ------------------------ System Includes -------------------------------- */
#include "rmsec2cmdif.h"
#include "sec2sw.h"
#ifdef IS_SSP_ENABLED
#include "seapi.h"
#include "setypes.h"
#include "dev_top.h"
#endif // IS_SSP_ENABLED
/* ------------------------ Application includes --------------------------- */
#include "config/g_sec2-config_private.h"

/* ------------------------ Public Functions ------------------------------- */
int         main(int argc, char **ppArgv)
    GCC_ATTRIB_SECTION("imem_resident", "main");
void        InitSec2App(void)
    GCC_ATTRIB_SECTION("imem_init", "InitSec2App");
FLCN_STATUS InitSec2HS(void)
    GCC_ATTRIB_SECTION("imem_init", "InitSec2HS");

/* ------------------------ External definitions --------------------------- */

extern LwrtosQueueHandle Sec2CmdMgmtCmdDispQueue;
extern LwrtosQueueHandle Sec2ChnMgmtCmdDispQueue;
extern LwrtosQueueHandle Disp2QWkrThd;
extern LwrtosQueueHandle HdcpmcQueue;
extern LwrtosQueueHandle GfeQueue;
extern LwrtosQueueHandle LwsrQueue;
extern LwrtosQueueHandle HwvQueue;
extern LwrtosQueueHandle PrQueue;
extern LwrtosQueueHandle VprQueue;
extern LwrtosQueueHandle AcrQueue;
extern LwrtosQueueHandle ApmQueue;
extern LwrtosQueueHandle SpdmQueue;

#ifdef IS_SSP_ENABLED

// variable to store canary for SSP
extern void * __stack_chk_guard;

//
// Initialize canary for LS
// LS canary is initialized to random generated number (TRNG) in LS using SE
// It is better to keep this function inlined so that we don't really need to worry about the overlay loading.
//
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS sec2InitializeStackCanaryLS(void)
{
    FLCN_STATUS status       = FLCN_OK;
    LwU32       platformType = 0;
    LwU32       rand32;
 
    //
    // Since HW does not enforce NONCE mode on emulation and neither does the SE lib enforce
    // this in SW, TRNG random seeding is used on pre-silicon resulting in hard hang.
    // As suggested by HW pre-silicon should enforce NONCE mode. Thus adding a WAR for
    // to move to SE based canary for only silicon here.
    // TODO: Once HW fixes this in emulator we can remove this WAR
    //

    if (FLCN_OK != (status = BAR0_REG_RD32_ERRCHK(LW_PTOP_PLATFORM, &platformType)))
    {
        return status;
    }

    platformType = DRF_VAL(_PTOP, _PLATFORM, _TYPE, platformType);

    if (platformType == LW_PTOP_PLATFORM_TYPE_SILICON)
    {

        //
        // Set the value of stack canary to a random value to ensure adversary
        // can not craft an attack by looking at the assembly to determine the canary value
        //

        ct_assert(FLCN_OK == SE_OK);
        if (SE_OK != (status = seTrueRandomGetNumber(&rand32, sizeof(LwU32))))
        {
            status = FLCN_ERR_SE_TRNG_FAILED;
            return status;
        }

        // Setup stack canary to random variable.
        __stack_chk_guard = (void *)rand32;
    }

    return status;
}

#endif // IS_SSP_ENABLED

#endif // MAIN_H

