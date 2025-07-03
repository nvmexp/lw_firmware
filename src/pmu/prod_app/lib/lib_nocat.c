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
 * @file    lib_nocat.c
 * @brief   Implementation of NOCAT related interfaces.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu/ssurface.h"
#include "lib/lib_nocat.h"
#include "cmdmgmt/cmdmgmt_rpc_impl.h"

#include "g_lwconfig.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define NOCAT_DIAG_BUFFER_LEN_DWORDS    16

/* ------------------------- Global Variables ------------------------------- */
/*!
 * @brief   Buffer of user-defined diagnostic data to include in NOCAT logging.
 *
 */
static LwU32 NocatDiagBuffer[NOCAT_DIAG_BUFFER_LEN_DWORDS]
    GCC_ATTRIB_SECTION("alignedData64", "NocatDiagBuffer");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Get the Physical DMEM offset of the NOCAT buffer
 *
 * TODO expand
 */
LwU32
nocatBufferDmemPhysOffsetGet(void)
{
    LwU32 bufferPhysOffset = 0U;
    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_NOCAT))
    {
        lwosResVAddrToPhysMemOffsetMap(NocatDiagBuffer, &bufferPhysOffset);
    }
    return bufferPhysOffset;
}

/*!
 * @brief   Copy data into the DMEM's resident NOCAT buffer to be picked up
 *          by the RM at the time of crash processing.
 *
 * Data passed in will be truncated if size > NOCAT_DIAG_BUFFER_LEN_DWORDS.
 */
void
nocatDiagBufferSet(LwU32 *pData, LwU32 size)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_NOCAT))
    {
        // Truncate data if too long, PMU crashing soon so no point in BP here
        LwU32 sizeToCopy = LW_MIN(size, NOCAT_DIAG_BUFFER_LEN_DWORDS);

        (void)memcpy(NocatDiagBuffer, pData, sizeToCopy * sizeof(LwU32));
    }
}

/*!
 * @brief   Log the data using the NOCAT infrastructure.
 *
 * @note    This interface issues PMU->RM message notifying the RM of
 *          non-critical (supressed) failure. It is caller's responsibilty
 *          to assure that the RM is not flooded with these notifications
 *          and that regualr PMU->RM communication is not impaired.
 *
 * @note    This is a non-blocking call so it does not assure that the RM
 *          will receive the log request -> callers must check return value.
 *
 * NJ-TODO: Allow for a timeout value to be passed in
 */
FLCN_STATUS
nocatDiagBufferLog
(
    LwU32 *pData,
    LwU32  bufferSizeDWords,
    LwBool bRmAssert
)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_NOCAT_LOG rpc;
    FLCN_STATUS status = FLCN_OK;

    if (bufferSizeDWords > RM_PMU_NOCAT_FB_BUFFER_SIZE_DWORDS)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_INDEX;
        goto nocatDiagBufferLog_exit;
    }

    // Suppress the PMU's DMEM/stack leaks to the RM.
    (void)memset(&rpc, 0x00, sizeof(rpc));

    rpc.callerIP = (LwU32)(LwLength)__builtin_return_address(0);
    rpc.bufferSizeDWords = (LwU16)bufferSizeDWords;
    rpc.taskId = osTaskIdGet();
    rpc.bRmAssert = bRmAssert;

    PMU_ASSERT_OK_OR_GOTO(status,
        ssurfaceWr(pData,
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(RM_PMU_NOCAT_FB_BUFFER_SS_LOCATION),
            bufferSizeDWords * sizeof(LwU32)),
        nocatDiagBufferLog_exit);

    PMU_RM_RPC_EXELWTE_NON_BLOCKING(status, CMDMGMT, NOCAT_LOG, &rpc);

nocatDiagBufferLog_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
