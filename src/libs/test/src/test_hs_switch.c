/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   test_hs_switch.c
 * @brief  Entry function for the HS switch job
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwrtos.h"
#include "lwoslayer.h"
#include "lwosselwreovly.h"
#include "dev_therm.h"

/* ------------------------- Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "flcntypes.h"
#include "uprociftest.h"

#if defined(GSPLITE_RTOS)
    #include "dpu_os.h"
    #include "gsp_bar0_hs.h"
    #include "gsplite_regmacros.h"
    #include "dev_gsp_csb.h"

    #define LW_SELWRE_SCRATCH_REG LW_CGSP_SELWRE_SCRATCH(0)
#else
    #error !!! Unsupported falcon !!!
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
extern FLCN_STATUS vApplicationCheckFuseRevocationHS(void);
extern void        vApplicationExitHS(void);

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Function Declarations -------------------------- */
FLCN_STATUS hs_switch_entry(LwU32 val, LwU8 regType, LwU32 *pRetValue)
    GCC_ATTRIB_SECTION("imem_testhsswitch", "hs_switch_entry");
FLCN_STATUS testHsEntry(LwU32 val, LwU8 regType, LwU32 *pRetValue)
    GCC_ATTRIB_SECTION("imem_testHs", "start");

FLCN_STATUS hs_switch_entry
(
    LwU32  val,
    LwU8   regType,
    LwU32 *pRetValue
)
{
    FLCN_STATUS status;

    // Attach and load HS overlays
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(testHs));

    // Setup entry into HS mode
    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(testHs), NULL, 0, HASH_SAVING_DISABLE);

    //
    // Call entry function (0th offset of overlay). This is the entry into HS
    // mode.
    //
    status = testHsEntry(val, regType, pRetValue);

    // We're now back out of HS mode

    // Cleanup after returning from HS mode
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(testHs));

    return status;
}

FLCN_STATUS testHsEntry
(
    LwU32  val,
    LwU8   regType,
    LwU32 *pRetValue
)
{
    FLCN_STATUS status = FLCN_OK;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);
    OS_SEC_HS_LIB_VALIDATE(libSecBusHs, HASH_SAVING_DISABLE);

    status = vApplicationEnterHS();
    if (status != FLCN_OK)
    {
        goto testHsEntry_exit;
    }

    // Write priv protected register
    switch (regType)
    {
        case RM_UPROC_PRIV_PROTECTED_REG_I2CS_SCRATCH:
        {
            EXT_REG_WR32_HS_ERRCHK(LW_THERM_I2CS_SCRATCH, val);
            EXT_REG_RD32_HS_ERRCHK(LW_THERM_I2CS_SCRATCH, pRetValue);
            break;
        }
        case RM_UPROC_PRIV_PROTECTED_REG_SELWRE_SCRATCH:
        {
            REG_WR32(CSB, LW_SELWRE_SCRATCH_REG, val);
            *pRetValue = REG_RD32(CSB, LW_SELWRE_SCRATCH_REG);
            break;
        }
        default:
        {
            //
            // Do nothing. No need to return an error since this is test code.
            // Don't halt for sure, since that is a security risk.
            //
        }
    }

testHsEntry_exit:
    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    vApplicationExitHS();

    return status;
}

