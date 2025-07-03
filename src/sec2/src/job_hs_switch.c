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
 * @file   job_hs_switch.c
 * @brief  Entry function for the HS switch job
 */
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "lwosselwreovly.h"

/* ------------------------- Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "sec2_objsec2.h"
#include "sec2_bar0_hs.h"
#include "sec2_hs.h"
#include "scp_rand.h"

#include "dev_sec_csb.h"
#include "dev_therm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Function Declarations -------------------------- */
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
    //
    // Check return PC to make sure that it is not in HS
    // This must always be the first statement in HS entry function
    //
    VALIDATE_RETURN_PC_AND_HALT_IF_HS();

    FLCN_STATUS flcnStatus = FLCN_ERR_ILLEGAL_OPERATION;

    OS_SEC_HS_LIB_VALIDATE(libCommonHs, HASH_SAVING_DISABLE);
    CHECK_FLCN_STATUS(sec2CheckFuseRevocationHS());

    // Write priv protected register
    switch (regType)
    {
        case PRIV_PROTECTED_REG_I2CS_SCRATCH:
        {
            CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(LW_THERM_I2CS_SCRATCH, val));
            CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(LW_THERM_I2CS_SCRATCH,pRetValue));
            break;
        }
        case PRIV_PROTECTED_REG_SELWRE_SCRATCH:
        {
            REG_WR32(CSB, LW_CSEC_SELWRE_SCRATCH(0), val);
            *pRetValue = REG_RD32(CSB, LW_CSEC_SELWRE_SCRATCH(0));
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

    flcnStatus = FLCN_OK;
ErrorExit:
    //
    // Enable the SCP RNG, and disable big hammer lockdown before returning
    // to light secure mode
    //
    EXIT_HS();

    return flcnStatus;
}

