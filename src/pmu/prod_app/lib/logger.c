/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   logger.c
 * @brief  logger code
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "dbgprintf.h"
#include "pmu_oslayer.h"
#include "pmu_objpmu.h"
#include "therm/objtherm.h"
//#include "dma.h"
#include "logger.h"

#include "g_pmurpc.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static LOGGER_BUFFER_INFO LoggerBufferInfo;

static LwU32 LoggerSegmentTracker[RM_PMU_LOGGER_SEGMENT_ID__COUNT];

/* ------------------------- Private Functions ------------------------------ */
/*!
 *  @brief Write data to appropriate segment.
 *
 *  Write data appropriate segment and updates the tracker.
 *
 *  @param[in]  type       segment ID or type
 *  @param[in]  writeSize  size of the data to be written
 *  @param[in]  pLogData   data pointer
 */
void
loggerWrite
(
    LwU8   segmentId,
    LwU16  writeSize,
    void  *pLogData
)
{
    LwU32 putOffset;
    LwU32 chunkSize = writeSize;
    LwU32 lwrPriority;

    // Sanity Check
    if (segmentId >= RM_PMU_LOGGER_SEGMENT_ID__COUNT)
    {
        return;
    }

    //
    // Synchronization of PMU logging feature with Low Power Features
    //
    // This is temporary solution to unblock PMU Logging feature.
    // RFE #1382860 will provide complete solution
    //
    appTaskCriticalEnter();
    {
        //
        // Skip logging if DMA is suspended. Dump samples into FB only if DMA
        // is not suspended.
        //
        if (lwosDmaIsSuspended())
        {
            appTaskCriticalExit();
            return;
        }

        //
        // Ensure that power feature will not engage while doing DMA transfer.
        //
        // GCX and LPWR task manages all power features. By default GCX runs at
        // priority 1 and LPWR at 2. Raise priority of Logger task above LPWR to
        // ensure that power tasks will not get chance to engage power features
        // until we complete logging for current iteration.
        //
        lwrtosLwrrentTaskPriorityGet(&lwrPriority);

        if (lwrPriority <= OSTASK_PRIORITY(LPWR))
        {
            lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(LPWR) + 1);
        }
    }
    appTaskCriticalExit();

    putOffset = LoggerSegmentTracker[segmentId];

    while (writeSize > 0)
    {
        chunkSize = LW_MIN(writeSize,
            LoggerBufferInfo.segments[segmentId].segmentSize - putOffset);
        if (FLCN_OK != dmaWrite(pLogData, &LoggerBufferInfo.dmaSurface,
                LoggerBufferInfo.segments[segmentId].segmentOffset + putOffset,
                chunkSize))
        {
            // Error in DMA
            PMU_BREAKPOINT();
        }

        // Update data pointer and putOffset for next write
        pLogData  = (void *)((LwU8 *)pLogData + chunkSize);
        putOffset = (putOffset + chunkSize ) %
                    LoggerBufferInfo.segments[segmentId].segmentSize;
        writeSize -= chunkSize;
    }

    LoggerSegmentTracker[segmentId] = putOffset;
    // RM uses scratch registers on Pascal+.
    if (thermLoggerTrackerSet_HAL(&Therm, segmentId, putOffset) != FLCN_OK)
    {
        PMU_TRUE_BP();
    }

    // Restore original priority
    lwrtosLwrrentTaskPrioritySet(lwrPriority);
}

/*!
 * Initialize logger code.
 */
FLCN_STATUS
pmuRpcLoggerInit
(
    RM_PMU_RPC_STRUCT_LOGGER_INIT *pParams
)
{
    LwU32 index;

    LoggerBufferInfo.dmaSurface = pParams->dmaSurface;

    for (index = 0; index < RM_PMU_LOGGER_SEGMENT_ID__COUNT; index++)
    {
        // Skip invalid segments.
        if (pParams->segments[index].segmentId == RM_PMU_LOGGER_SEGMENT_ID__ILWALID)
        {
            continue;
        }

        // Copy-in the segment information.
        LoggerBufferInfo.segments[index] = pParams->segments[index];

        //
        // Value at the segment tracker address is the value of put-offset for
        // segment. Initialize it to 0, since there is no data now. As we put
        // the data into buffer this will keep increasing/wrap-around.
        //
        LoggerSegmentTracker[index] = 0;
    }

    // TODO: Not used on Pascal+, remove with Maxwell architecture deprecation.
    pParams->trackerAllocOffset = (LwU32)(LwUPtr)&LoggerSegmentTracker[0];

    return FLCN_OK;
}
