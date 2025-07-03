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
 * @file   pmu_pglog.c
 * @brief  PMU Power Gating Logging mechanism
 *
 * Features implemented : PGLOG
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objms.h"
#include "dbgprintf.h"
#include "pmu_objtimer.h"
#include "pmu_objpmu.h"
#include "rmcd.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* PG Logging Event Filter Mask. This is a subset of PG_LOGIC_LOGGING_EVENT_MASK
 * which is defined in task_lpwr.c file
 */
#define PG_LOGGING_EVENT_MASK_PG_ENG                                          \
    (BIT(PMU_PG_EVENT_PG_ON)                                                 |\
     BIT(PMU_PG_EVENT_PG_ON_DONE)                                            |\
     BIT(PMU_PG_EVENT_ENG_RST)                                               |\
     BIT(PMU_PG_EVENT_CTX_RESTORE)                                           |\
     BIT(PMU_PG_EVENT_DENY_PG_ON))

#define PG_LOGGING_EVENT_MASK_LPWR_ENG                                        \
    (BIT(PMU_PG_EVENT_PG_ON)                                                 |\
     BIT(PMU_PG_EVENT_PG_ON_DONE)                                            |\
     BIT(PMU_PG_EVENT_ENG_RST)                                               |\
     BIT(PMU_PG_EVENT_CTX_RESTORE)                                           |\
     BIT(PMU_PG_EVENT_DENY_PG_ON)                                            |\
     BIT(PMU_PG_EVENT_POWERED_DOWN)                                          |\
     BIT(PMU_PG_EVENT_POWERING_UP)                                           |\
     BIT(PMU_PG_EVENT_POWERED_UP))

/* ------------------------- Static Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG))
static PG_LOG_EVENTMAP _pgLogEventMap[] =
{
    { PMU_PG_EVENT_PG_ON           , PMU_PG_LOG_EVENT_PG_ON            },
    { PMU_PG_EVENT_PG_ON_DONE      , PMU_PG_LOG_EVENT_PG_ON_DONE       },
    { PMU_PG_EVENT_CTX_RESTORE     , PMU_PG_LOG_EVENT_CTX_RESTORE      },
    { PMU_PG_EVENT_ENG_RST         , PMU_PG_LOG_EVENT_ENG_RST          },
    { PMU_PG_EVENT_DENY_PG_ON      , PMU_PG_LOG_EVENT_DENY_PG_ON       },
    { PMU_PG_EVENT_POWERED_DOWN    , PMU_PG_LOG_EVENT_POWERED_DOWN     },
    { PMU_PG_EVENT_POWERING_UP     , PMU_PG_LOG_EVENT_POWERING_UP      },
    { PMU_PG_EVENT_POWERED_UP      , PMU_PG_LOG_EVENT_POWERED_UP       },
};
#else
static PG_LOG_EVENTMAP _pgLogEventMap[] =
{
    { PMU_PG_EVENT_PG_ON           , PMU_PG_LOG_EVENT_PG_ON            },
    { PMU_PG_EVENT_PG_ON_DONE      , PMU_PG_LOG_EVENT_PG_ON_DONE       },
    { PMU_PG_EVENT_CTX_RESTORE     , PMU_PG_LOG_EVENT_CTX_RESTORE      },
    { PMU_PG_EVENT_ENG_RST         , PMU_PG_LOG_EVENT_ENG_RST          },
    { PMU_PG_EVENT_DENY_PG_ON      , PMU_PG_LOG_EVENT_DENY_PG_ON       },
};
#endif // PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG)
/* ------------------------- Global Variables ------------------------------- */
PG_LOG PgLog;

/* ------------------------- Prototypes ------------------------------------- */
static LwU8 s_pgLogColwertToLogEventId(LwU32);

/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Initialize PgLog
 *
 * This function does the initialization of PgLog before scheduler schedules PG
 * task for the exelwtion.
 */
void
_pgLogPreInit(void)
{
    memset(&PgLog, 0, sizeof(PgLog));
}

/*!
 * Helper function to process PG log init command
 *
 * @param[in]   pDispPgLogCmd   Dispatch wrapper contains PG command
 *
 * @param[out]  pMsg            Return message used for acknowledgement
 */
void
_pgLogInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_LOG_INIT *pParams
)
{
    RM_PMU_PG_LOG_HEADER  header;
    LwU32                 pgLogDmemSize;

    // Initialize the PG log parameters
    PgLog.putOffset    = 0;  // Initial PG log put offset
    PgLog.recordId     = 1;  // Initial record id
    PgLog.bInitialized = LW_TRUE;
    PgLog.params       = pParams->params;

    // Callwlate the size of staging buffer
    pgLogDmemSize = pParams->params.eventsPerRecord *
                    sizeof(RM_PMU_PG_LOG_ENTRY) +
                    sizeof(RM_PMU_PG_LOG_HEADER);

    // Initialize the PG log staging buffer Offset
    if (PgLog.pOffset == NULL)
    {
        PgLog.pOffset = (LwU16 *)lwosCallocResident(pgLogDmemSize);
        PMU_HALT_COND(PgLog.pOffset != NULL);
    }

    // Write the initial header in the DMEM buffer
    header.recordId  = PgLog.recordId;
    header.numEvents = 0;
    header.pPut      = 0;
    header.rsvd      = 0;
    memcpy((void *)PgLog.pOffset, (void *)&header, sizeof(RM_PMU_PG_LOG_HEADER));
}

/*!
 * @brief Log PG events in DMEM
 *
 * Logs PG events in the DMEM staging record area. If the number of events
 * reaches PgLog.eventsPerNSI or PgLog.flushWaterMark, an attempt is made
 * to flush the event entries in the DMEM to the PG log buffer
 *
 * @param[in]  ctrlId
 *      PgCtrl on which the event oclwrred
 * @param[in]  eventId
 *      ID of the event
 * @param[in]  status
 *      Extra information pertaining to the event
 */
void
_pgLogEvent
(
    LwU8   ctrlId,
    LwU32  eventId,
    LwU32  status
)
{
    RM_PMU_PG_LOG_HEADER    pgLogHeader;
    RM_PMU_PG_LOG_ENTRY     pgLogEntry;
    LwU8                   *pNewOffset;
    FLCN_TIMESTAMP          lwrrentTimeNs;
    void                   *pBuf = (void *)PgLog.pOffset;

    // Return early if the log is initialized
    if (!PgLog.bInitialized)
    {
        return;
    }

    // For LPWR_ENG, return early if event is not part of LPWR_ENG logging
    if ((PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG)) &&
        (LPWR_ARCH_IS_SF_SUPPORTED(LPWR_ENG))  &&
        (BIT(eventId) & PG_LOGGING_EVENT_MASK_LPWR_ENG) == 0)

    {
        return;
    }
    
    // For PG_ENG, return early if event is not part of PG_ENG logging
    if ((!PMUCFG_FEATURE_ENABLED(PMU_LPWR_ENG)) &&
        (BIT(eventId) & PG_LOGGING_EVENT_MASK_PG_ENG) == 0)
    {
        return;
    }
    
    eventId = (LwU32)s_pgLogColwertToLogEventId(eventId);

    // Read current header
    memcpy((void *)&pgLogHeader, pBuf, sizeof(RM_PMU_PG_LOG_HEADER));

    // Check if any more records can be added to the DMEM staging buffer
    if (pgLogHeader.numEvents < PgLog.params.eventsPerRecord)
    {
        // Get the current timestamp
        osPTimerTimeNsLwrrentGet(&lwrrentTimeNs);

        // Fill up new record data
        pgLogEntry.eventType = eventId;
        pgLogEntry.ctrlId    = ctrlId;
        pgLogEntry.timeStamp = lwrrentTimeNs.parts.lo;
        pgLogEntry.status    = status;

        // Callwlate new record offset
        pNewOffset = (LwU8 *)pBuf +
                    sizeof(RM_PMU_PG_LOG_HEADER) + pgLogHeader.pPut;

        // Write new record in DMEM
        memcpy((void *)pNewOffset, (void *)&pgLogEntry,
                    sizeof(RM_PMU_PG_LOG_ENTRY));

        // Update header
        pgLogHeader.recordId = PgLog.recordId;
        pgLogHeader.numEvents++;
        pgLogHeader.rsvd = 0;
        pgLogHeader.pPut += sizeof(RM_PMU_PG_LOG_ENTRY);

        // Write back header
        memcpy(pBuf, (void *)&pgLogHeader,
                    sizeof(RM_PMU_PG_LOG_HEADER));

        // Check if DMEM ELPG log should be flushed to FB
        if (pgLogHeader.numEvents >= PgLog.params.eventsPerNSI ||
            pgLogHeader.numEvents >= PgLog.params.flushWaterMark)
        {
            // Attempt to flush the DMEM record to FB
            PgLog.bFlushRequired = LW_TRUE;
            _pgLogFlush();
        }
    }
    else
    {
        // DMEM full
        DBG_PRINTF_PG(("PG: logbuf full\n",
                       0, 0, 0, 0));
        PMU_HALT();
    }
}

/*!
 * @brief Notify PG log about MS DMA suspend
 *
 * MSCG suspends DMA to avoid causing any more FB traffic from PMU. PG Log
 * flush causes FB traffic. This API notifies PG log to skip the flushes until
 * MSCG resumes DMA.
 */
void
_pgLogNotifyMsDmaSuspend(void)
{
    PgLog.bMsDmaSuspend = LW_TRUE;
}

/*!
 * @brief Notify PG log about MS DMA resume
 *
 * This function triggers PG log flush as MSCG has resume the DMA.
 */
void
_pgLogNotifyMsDmaResume(void)
{
    PgLog.bMsDmaSuspend = LW_FALSE;

    _pgLogFlush();
}

/*!
 * @brief Flush PG events
 *
 * This function is called by pgLogEvent() to try and flush the event entries
 * in DMEM to the PG log buffer. This function returns immediately if DMA is
 * suspended by MSCG. Otherwise, the DMEM contents are DMA-ed to the PG log
 * buffer.
 */
void
_pgLogFlush(void)
{
    PMU_RM_RPC_STRUCT_LPWR_PG_LOG_FLUSHED rpc;
    LwU32                                 recordSize;
    LwU32                                 chunkSize;
    RM_PMU_PG_LOG_HEADER                  pgLogHeader;
    void                                 *pBuf = (void *)PgLog.pOffset;
    FLCN_STATUS                           status = FLCN_OK;

    // 
    if (PgLog.bFlushRequired &&
        !PgLog.bMsDmaSuspend)
    {
        // Read current header
        memcpy((void *)&pgLogHeader, pBuf, sizeof(RM_PMU_PG_LOG_HEADER));

        if (pgLogHeader.numEvents == 0)
        {
            //
            // return if there are no events to flush.
            // Can reach here if there was end-of-test flush command from RM.
            //
            return;
        }

        // Callwlate the number of bytes to write
        recordSize = sizeof(RM_PMU_PG_LOG_HEADER) +
                pgLogHeader.numEvents * sizeof(RM_PMU_PG_LOG_ENTRY);

        // Check if size is enough. Max size is lwrrently 4K.
        if (PgLog.putOffset + recordSize >= RM_PMU_LPWR_PG_LOG_SIZE_MAX)
        {
            // Split DMA into 2 chunks
            chunkSize = RM_PMU_LPWR_PG_LOG_SIZE_MAX - PgLog.putOffset;

            // Copy the bufffer to superSurface in 2 chunks
            status = ssurfaceWr(pBuf, RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.lpwr.pgLogSurface.buffer)+ PgLog.putOffset, chunkSize);
            if (status != LW_OK)
            {
               PMU_HALT();
            }

            // DMA the remanining chunk
            status = ssurfaceWr(pBuf, RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.lpwr.pgLogSurface.buffer),  (recordSize - chunkSize));
            if (status != LW_OK)
            {
               PMU_HALT();
            }
        }
        else
        {
            // Copy the bufffer to superSurface in single write
            status = ssurfaceWr(pBuf, RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.lpwr.pgLogSurface.buffer)+ PgLog.putOffset, recordSize);
            if (status != LW_OK)
            {
               PMU_HALT();
            }
        }

        // Update FB put pointer
        PgLog.putOffset = (PgLog.putOffset + recordSize) % RM_PMU_LPWR_PG_LOG_SIZE_MAX;

        //
        // Send message to RM
        // RC-TODO, properly handle status
        //
        PMU_RM_RPC_EXELWTE_BLOCKING(status, LPWR, PG_LOG_FLUSHED, &rpc);

        // Update record id and write new header
        PgLog.recordId++;
        pgLogHeader.recordId  = PgLog.recordId;
        pgLogHeader.numEvents = 0;
        pgLogHeader.rsvd      = 0;
        pgLogHeader.pPut      = 0;
        PgLog.bFlushRequired  = LW_FALSE;

        // Write back new header
        memcpy(pBuf, (void *)&pgLogHeader, sizeof(RM_PMU_PG_LOG_HEADER));
    }
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Colwert PMU PG eventId to RM eventId for Logging
 *
 * @param[in]  pmuEventId
 *      PMU PG event Id
 *
 * @returns    RM PG_EVENT Id
 */
static LwU8
s_pgLogColwertToLogEventId
(
    LwU32  pmuEventId
)
{
    LwU32  evtIdx;

    for (evtIdx = 0; evtIdx < LW_ARRAY_ELEMENTS(_pgLogEventMap); evtIdx++)
    {
        if (_pgLogEventMap[evtIdx].pmuEventId == (LwU8) pmuEventId)
        {
            return _pgLogEventMap[evtIdx].logEventId;
        }
    }

    return RM_EVENT_PG_ILWALID;
}

