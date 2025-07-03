/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "core/include/evntthrd.h"
#include "core/include/cpu.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/errcount.h"
#include "core/include/assertinfosink.h"
#include "core/include/bscrsink.h"
#include "core/include/circsink.h"
#include "core/include/coreutility.h"
#include "core/include/datasink.h"
#include "core/include/dbgsink.h"
#include "core/include/ethrsink.h"
#include "core/include/filesink.h"
#include "core/include/remotesink.h"
#include "core/include/sersink.h"
#include "core/include/stdoutsink.h"
#include "core/include/debuglogsink.h"
#include "inc/bytestream.h"
#include <ctime>
#include <cstring>

namespace
{
    enum
    {
        QueueDepth            = (32 * 1024)
        ,TriggerFlushOnQSize  = (31 * 1024)
        ,MaxPrintfSize        = 256
    };

    struct TeeQItem
    {
        Tee::Priority         pri;
        Tee::ScreenPrintState sps;
        bool                  bIsAllocStr;
        char*                 pStr;
        size_t                strLen;
        char                  printStr[MaxPrintfSize];
        char                  prefixStr[Tee::MaxPrintPrefixSize];
        Mle::Context          mleContext;
        INT32                 refCount;
        INT32                 teeQDataIdx;
    };

    bool   d_EnableQueuedPrintfs = false;

    //! Tracking variables to monitor the queue size, eventually these could be
    //! removed
    INT32            d_NoPrintSpaceCount = 0;
    UINT32           d_MaxQueueDepth     = 0;

    volatile UINT32               d_PoolLock = 0;
    map<Tasker::ThreadID, string> d_PrintPools;

    volatile UINT32           d_QueueLock = 0;
    TeeQItem                 *d_TeeQData = NULL;
    volatile INT32           *d_FreeQueueDataIndexes = NULL;
    volatile INT32            d_LwrFreeIndex = 0;
    struct TeeQ
    {
        TeeQItem              *Items[QueueDepth + 1];
        volatile INT32         Put;
        INT32                  Get;
        Tee::QueueDrainMethod  QueueType;
        EventThread           *pPrintfThread;

        TeeQ(Tee::QueueDrainMethod qdm) :
            Put(0), Get(0), QueueType(qdm), pPrintfThread(0) { }
    };
    TeeQ d_AttachedQueue(Tee::qdmATTACHED);
    TeeQ d_DetachedQueue(Tee::qdmDETACHED);

    //--------------------------------------------------------------------
    //! \brief ErrCounter subclass for print queue overruns.
    //!
    //! This class is instantiated in ModsMain.
    //!
    class PrintQueueOverflowErrCounter : public ErrCounter
    {
    protected:
        virtual RC ReadErrorCount(UINT64 *pCount)
        {
            pCount[0] = static_cast<UINT64>(d_NoPrintSpaceCount);
            return OK;
        }

        virtual RC OnError(const ErrorData *pData)
        {
            return RC::SOFTWARE_ERROR;
        }
    };
    PrintQueueOverflowErrCounter *d_pOverflowCounter;

    //-----------------------------------------------------------------------------
    //! \brief Private function to release a tee queue item
    //!
    //! \param pTeeQItem : Pointer to the tee queue item to release
    void ReleaseTeeQItem(TeeQItem * const pTeeQItem)
    {
        const INT32 prevRefCount = Cpu::AtomicAdd(&pTeeQItem->refCount, -1);

        MASSERT(prevRefCount != 0);
        if (prevRefCount == 1)
        {
            const INT32 queueIndex = pTeeQItem->teeQDataIdx;
            if (pTeeQItem->bIsAllocStr)
            {
                delete [] pTeeQItem->pStr;
                pTeeQItem->pStr = pTeeQItem->printStr;
                pTeeQItem->bIsAllocStr = false;
            }
            pTeeQItem->strLen = 0;
            pTeeQItem->mleContext = Mle::Context();
            Tasker::SpinLock lock(&d_QueueLock);
            INT32 pushIdx = Cpu::AtomicAdd(&d_LwrFreeIndex, -1) - 1;
            Cpu::AtomicWrite(&d_FreeQueueDataIndexes[pushIdx], queueIndex);
        }
    }

    //-----------------------------------------------------------------------------
    //! \brief Private function to reserve a tee queue item
    //!
    //! \return pointer to reserved tee queue item (NULL if unable to reserve an
    //!         item
    TeeQItem * ReserveTeeQItem()
    {
        INT32  freeEntry;
        UINT32 remainingTeeQItems;
        {
            Tasker::SpinLock lock(&d_QueueLock);
            if (Cpu::AtomicRead(&d_LwrFreeIndex) == QueueDepth)
            {
                Cpu::AtomicAdd(&d_NoPrintSpaceCount, 1);
                return NULL;
            }
            const INT32  freeEntryIdx = Cpu::AtomicAdd(&d_LwrFreeIndex, 1);
            freeEntry = Cpu::AtomicRead(&d_FreeQueueDataIndexes[freeEntryIdx]);
            remainingTeeQItems = (UINT32)(QueueDepth - freeEntryIdx - 1);
        }
        if ((QueueDepth - remainingTeeQItems) > d_MaxQueueDepth)
        {
            d_MaxQueueDepth = (UINT32)(QueueDepth - remainingTeeQItems);
        }

        MASSERT(d_TeeQData[freeEntry].refCount == 0);
        d_TeeQData[freeEntry].refCount = 1;
        return &d_TeeQData[freeEntry];
    }

    //-----------------------------------------------------------------------------
    //! \brief Processes data from the print queue to the various sinks
    //!
    void PrintfHandler(void *pvArgs)
    {
        TeeQ * const pTeeQ = static_cast<TeeQ *>(pvArgs);

        // Only need to process once, the EventThread sets a counting event and
        // will call this function once per print that is queued
        if (Cpu::AtomicRead(&pTeeQ->Put) != pTeeQ->Get)
        {
            TeeQItem * const pTeeQItem = pTeeQ->Items[pTeeQ->Get];

            // Ensure we see the data written by other threads in the correct order
            Cpu::ThreadFenceAcquire();

            Tee::SinkPriorities SinkPri(pTeeQItem->pri);
            // Write the print to all the various sinks
            Tee::Write(SinkPri,
                       pTeeQItem->pStr,
                       pTeeQItem->strLen,
                       pTeeQItem->prefixStr,
                       pTeeQItem->sps,
                       pTeeQ->QueueType,
                       &pTeeQItem->mleContext);
            // No lock required, only queue-drainer writes GET.
            pTeeQ->Get = (pTeeQ->Get + 1) % (QueueDepth + 1);
            ReleaseTeeQItem(pTeeQItem);
        }
    }

    //-----------------------------------------------------------------------------
    //! \brief Queue a print to the specified queue
    //!
    //! \param pQueue    : Pointer to the queue to add the item to
    //! \param pTeeQItem : Pointer to item to queue
    void QueuePrint(TeeQ *pQueue, TeeQItem *pTeeQItem)
    {
        {
            Cpu::AtomicAdd(&pTeeQItem->refCount, 1);
            Tasker::SpinLock lock(&d_QueueLock);
            const INT32 put = Cpu::AtomicRead(&pQueue->Put);
            pQueue->Items[put] = pTeeQItem;
            Cpu::AtomicWrite(&pQueue->Put, (put + 1) % (QueueDepth + 1));
        }

        // Set a counting event so that the processing thread will be
        // called once per print from the event thread
        pQueue->pPrintfThread->SetCountingEvent();
    }

    //-----------------------------------------------------------------------------
    //! \brief Unconditionally flush the queued prints to all the sinks, even if
    //!        queued printing is disabled.
    //!
    //! \param TimeoutMs : Timeout in milliseconds for flusing the print queue
    //!
    //! \return OK if successful, not OK otherwise
    RC TeeFlushQueuedPrintsInternal(FLOAT64 TimeoutMs)
    {
        INT32 prevIndex = Cpu::AtomicRead(&d_LwrFreeIndex);
        if (prevIndex == 0)
            return OK;

        Tasker::DetachThread detach;

        const UINT64 startTime = Xp::GetWallTimeMS();

        INT32 soughtLevel = 0;
        do
        {
            const UINT64 lwrTime = Xp::GetWallTimeMS();
            if (TimeoutMs >= 0 && startTime + static_cast<UINT64>(TimeoutMs) < lwrTime)
            {
                return RC::TIMEOUT_ERROR;
            }

            // Ensure the threads draining the queues make progress
            d_AttachedQueue.pPrintfThread->SetCountingEvent();
            d_DetachedQueue.pPrintfThread->SetCountingEvent();
            Tasker::Yield();

            // If number of reserved items increases, we try to ignore
            // the newly added items, otherwise we will keep spinning
            // forever if another thread is printing like crazy while
            // we wait for the queues to drain
            const INT32 newIndex = Cpu::AtomicRead(&d_LwrFreeIndex);
            if (newIndex > prevIndex)
            {
                soughtLevel += newIndex - prevIndex;
            }

            prevIndex = newIndex;
        }
        while (prevIndex > soughtLevel);

        return OK;
    }
}

extern "C" {
//-----------------------------------------------------------------------------
//! \brief Queue a printf for processing by the printf thread
//!
//! \param Priority   : Priority for the print (see Tee::Priority enum).
//! \param ModuleCode : Module for the print (Tee::ModuleNone to ignore the
//!                     module
//! \param Sps        : Screen print state (see Tee::ScreenPrintState)
//! \param Format     : Format string for the print
//! \param RestOfArgs : Variable argument list for the format string
//!
//! \return Number of characters queued
//!
INT32 ModsExterlwAPrintf
(
   INT32                     Priority,
   UINT32                    ModuleCode,
   UINT32                    Sps,
   const char              * Format,
   va_list                   RestOfArgs
)
{
    MASSERT(Format != 0);

#ifndef MODS_INCLUDE_DEBUG_PRINTS
    if (Priority <= Tee::PriSecret)
    {
        return 0;
    }
#endif

    // Call TeeInit() the first time we print something.  If TeeInit()
    // fails to initialize, presumably because TeeDestroy() was
    // already called, then fall back on vprintf().
    //
    if (!Tee::IsInitialized())
    {
        Tee::TeeInit();
        if (!Tee::IsInitialized())
        {
            vprintf(Format, RestOfArgs);
            return 0;
        }
    }

    INT32 charactersWritten = 0;
    Tee::SinkPriorities SinkPri(static_cast<Tee::Priority>(Priority));
    const bool bAttachedPrint = Tee::WillPrint(SinkPri, Tee::qdmATTACHED);
    const bool bDetachedPrint = Tee::WillPrint(SinkPri, Tee::qdmDETACHED);

    // If no sinks have a high enough verbosity level to pass this output,
    // save CPU time by skipping the vsprintf.  Unfortunately the AssertSink
    // lwrrently always prints at Debug level which means that all prints
    // will likely pass this check unless ModuleCodes are in use.
    if ((bAttachedPrint || bDetachedPrint) &&
        ((static_cast<Tee::Priority>(Priority) >= Tee::PriNormal) ||
        Tee::IsModuleEnabled(ModuleCode)))
    {
        char stackBuf[MaxPrintfSize];
        char* pPrint = stackBuf;

        charactersWritten = Utility::VsnPrintf(stackBuf, MaxPrintfSize, Format, RestOfArgs);
        if (charactersWritten == 0)
        {
            // Nothing to print
            return 0;
        }

        // If our print didn't fit in our stack buffer, allocate
        // a buffer of the appropriate size and print to it.
        unique_ptr<char[]> pBuffer;
        if (charactersWritten >= MaxPrintfSize)
        {
            pBuffer = make_unique<char[]>(charactersWritten + 1);
            pPrint = pBuffer.get();
            vsnprintf(pPrint, charactersWritten + 1, Format, RestOfArgs);
        }

        // When we have multiple threads (i.e. Tasker's available),
        // store prints per-thread until we see them end with '\n'
        bool holdPartialPrints = Tasker::IsInitialized();
        string* pPrintPool = nullptr;
        string tempPool; // for when Tasker isn't available
        if (holdPartialPrints)
        {
            Tasker::SpinLock lock(&d_PoolLock);
            pPrintPool = &d_PrintPools[Tasker::LwrrentThread()];
        }
        else
        {
            pPrintPool = &tempPool;
        }

        (*pPrintPool) += pPrint;
        if (holdPartialPrints && pPrint[charactersWritten - 1] != '\n')
        {
            // Return characters written for this individual print,
            // even though they're held for the time being
            return charactersWritten;
        }

        // If we ended with '\n', or we're not holding prints, then
        // we add all pending prints for the thread to the main queue

        if (Cpu::AtomicRead(&d_LwrFreeIndex) >= TriggerFlushOnQSize)
        {
            Tee::TeeFlushQueuedPrints(100);
        }
        TeeQItem * const pTeeQItem = ReserveTeeQItem();

        // Skip the print if a TeeQItem could not be reserved.  Note that this
        // will evenually cause MODS to fail since there is a queue full error
        // counter
        if (!pTeeQItem)
            return 0;

        if (pPrintPool->size() >= MaxPrintfSize)
        {
            pTeeQItem->pStr = new char[pPrintPool->size() + 1];
            pTeeQItem->bIsAllocStr = true;
        }
        memcpy(pTeeQItem->pStr, pPrintPool->data(), pPrintPool->size());
        pTeeQItem->strLen = pPrintPool->size();
        pPrintPool->clear();

        pTeeQItem->sps = static_cast<Tee::ScreenPrintState>(Sps);
        pTeeQItem->pri = static_cast<Tee::Priority>(Priority);
        pTeeQItem->prefixStr[0] = '\0';

        Tee::PrintPrefix(Priority, pTeeQItem->prefixStr, (pTeeQItem->pStr[0] != '\n'));

        if (Tee::WillPrintToMle(Priority))
        {
            Tee::SetContext(&pTeeQItem->mleContext);
        }

        // Mle prints are to be immediately written to a log, even if queued
        // prints are enabled
        if (d_EnableQueuedPrintfs && pTeeQItem->pri != Tee::MleOnly)
        {
            // Set a counting event so that the processing thread will be
            // called once per print from the event thread
            if (bAttachedPrint)
                QueuePrint(&d_AttachedQueue, pTeeQItem);

            if (bDetachedPrint)
                QueuePrint(&d_DetachedQueue, pTeeQItem);
        }
        else
        {
            Tee::Write(SinkPri, pTeeQItem->pStr, pTeeQItem->strLen, pTeeQItem->prefixStr,
                       pTeeQItem->sps, Tee::qdmBOTH, &pTeeQItem->mleContext);
        }

        // Ensure the PrintfHandler thread will see the writes we've just performed
        // in the correct order
        Cpu::ThreadFenceRelease();

        ReleaseTeeQItem(pTeeQItem);
    }

    STACK_CHECK();
    return charactersWritten;
}
}

//-----------------------------------------------------------------------------
//! \brief Tee initialization to be done after Tasker is initialized
//!
//! \return OK if successful, not OK otherwise
RC Tee::TeeInitAfterTasker()
{
    RC rc;
    MASSERT(Tasker::IsInitialized());

    d_pOverflowCounter = new PrintQueueOverflowErrCounter;
    CHECK_RC(d_pOverflowCounter->Initialize(
                 "PrintQueueOverflow", 1, 0, nullptr,
                 ErrCounter::MODS_TEST,
                 ErrCounter::PRINTQ_OVERFLOW_PRI));

    d_AttachedQueue.pPrintfThread = new EventThread(Tasker::MIN_STACK_SIZE,
                                              "AttachedPrintfThread", false);
    d_DetachedQueue.pPrintfThread = new EventThread(Tasker::MIN_STACK_SIZE,
                                              "DetachedPrintfThread", true);

    GetScreenSink()->PostTaskerInitialize();
    GetFileSink()->PostTaskerInitialize();
    GetMleSink()->PostTaskerInitialize();
    GetSerialSink()->PostTaskerInitialize();
    GetCirlwlarSink()->PostTaskerInitialize();
    GetDebugLogSink()->PostTaskerInitialize();
    GetDebuggerSink()->PostTaskerInitialize();
    GetEthernetSink()->PostTaskerInitialize();
    GetAssertInfoSink()->PostTaskerInitialize();
    GetStdoutSink()->PostTaskerInitialize();

    return TeeEnableQueuedPrints(true);
}

//-----------------------------------------------------------------------------
//! \brief Tee shutdown to be called immediately prior to shutting down tasker
//!
//! \return OK if successful, not OK otherwise
RC Tee::TeeShutdownBeforeTasker()
{
    if (!Tasker::IsInitialized())
    {
        return RC::WAS_NOT_INITIALIZED;
    }

    StickyRC rc;

    // Disable queued printfs *first*
    rc = TeeEnableQueuedPrints(false);

    delete d_AttachedQueue.pPrintfThread;
    delete d_DetachedQueue.pPrintfThread;
    d_AttachedQueue.pPrintfThread = NULL;
    d_DetachedQueue.pPrintfThread = NULL;

    rc = d_pOverflowCounter->ShutDown(true);
    delete d_pOverflowCounter;
    d_pOverflowCounter = NULL;

    GetScreenSink()->PreTaskerShutdown();
    GetFileSink()->PreTaskerShutdown();
    GetMleSink()->PreTaskerShutdown();
    GetSerialSink()->PreTaskerShutdown();
    GetCirlwlarSink()->PreTaskerShutdown();
    GetDebugLogSink()->PreTaskerShutdown();
    GetDebuggerSink()->PreTaskerShutdown();
    GetEthernetSink()->PreTaskerShutdown();
    GetAssertInfoSink()->PreTaskerShutdown();
    GetStdoutSink()->PreTaskerShutdown();

    GetDebugLogSink()->KillThread();

    Printf(Tee::PriLow, "Max queue depth        = %d\n", d_MaxQueueDepth);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Enable or disable queued prints
//!
//! \param bEnable : true to enable queued prints, false to disable
//!
//! \return OK if successful, not OK otherwise
RC Tee::TeeEnableQueuedPrints(bool bEnable)
{
    RC rc;

    MASSERT(Tasker::IsInitialized());

    if (bEnable != d_EnableQueuedPrintfs)
    {
        if (bEnable)
        {
            // Startup the printf handler prior to enabling queued printfs
            CHECK_RC(d_AttachedQueue.pPrintfThread->SetHandler(PrintfHandler,
                                                               &d_AttachedQueue));
            CHECK_RC(d_DetachedQueue.pPrintfThread->SetHandler(PrintfHandler,
                                                               &d_DetachedQueue));
            d_EnableQueuedPrintfs = true;
        }
        else
        {
            // Disable queued printfs first (so that no more prints are added
            // to the queue) prior to flushing all the queued prints and
            // shutting down the printf queue thread
            d_EnableQueuedPrintfs = false;
            TeeFlushQueuedPrintsInternal(Tasker::NO_TIMEOUT);
            CHECK_RC(d_AttachedQueue.pPrintfThread->SetHandler(NULL));
            CHECK_RC(d_DetachedQueue.pPrintfThread->SetHandler(NULL));
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Return whether queued printing is enabled
//!
//! \return true if queued printing is enabled, false otherwise
bool Tee::QueuedPrintsEnabled()
{
    return d_EnableQueuedPrintfs;
}

//-----------------------------------------------------------------------------
//! \brief Flush the queued prints to all the sinks
//!
//! \param TimeoutMs : Timeout in milliseconds for flusing the print queue
//!
//! \return OK if successful, not OK otherwise
RC Tee::TeeFlushQueuedPrints(FLOAT64 TimeoutMs)
{
    if (d_EnableQueuedPrintfs)
    {
        return TeeFlushQueuedPrintsInternal(TimeoutMs);
    }
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Private initialization function for queue data structures
void Tee::TeeQueueInit()
{
    d_TeeQData = new TeeQItem[QueueDepth];
    d_FreeQueueDataIndexes = new INT32[QueueDepth];
    for (INT32 i = 0; i < QueueDepth; i++)
    {
        Cpu::AtomicWrite(&d_FreeQueueDataIndexes[i], i);
        d_TeeQData[i].pri            = Tee::PriNone;
        d_TeeQData[i].sps            = Tee::SPS_END_LIST;
        d_TeeQData[i].bIsAllocStr    = false;
        d_TeeQData[i].pStr           = d_TeeQData[i].printStr;
        d_TeeQData[i].strLen         = 0;
        d_TeeQData[i].printStr[0]    = '\0';
        d_TeeQData[i].prefixStr[0]   = '\0';
        d_TeeQData[i].refCount       = 0;
        d_TeeQData[i].teeQDataIdx    = i;
    }
}

//-----------------------------------------------------------------------------
//! \brief Private destruction function for queue data structures
void Tee::TeeQueueDestroy()
{
    delete []d_TeeQData;
    d_TeeQData = NULL;

    delete []d_FreeQueueDataIndexes;
    d_FreeQueueDataIndexes = NULL;
}
