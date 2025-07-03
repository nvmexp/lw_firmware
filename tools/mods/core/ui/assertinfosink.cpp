/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// AssertInfo sink to print debug info during an assertion failure.

#include "core/include/assertinfosink.h"
#include "core/include/errormap.h"
#include "core/include/filesink.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/version.h"

constexpr UINT32 s_NormalBufSize  = static_cast<UINT32>(12_KB);
constexpr UINT32 s_StealthBufSize = static_cast<UINT32>(1_KB);
constexpr UINT32 s_MaxEntries     = 192U;

struct AssertInfoSink::Impl
{
    struct LogEntry
    {
        Mle::Context context;
        UINT32       startOffset = 0;
        UINT32       size        = 0;

        LogEntry()                      = default;
        LogEntry(LogEntry&&)            = default;
        LogEntry& operator=(LogEntry&&) = default;
    };

    vector<LogEntry> entries;
    vector<char>     buffer;
    UINT32           nextEntry  = 0;
    UINT32           firstEntry = 0;
    UINT32           bufferHead = 0;
#ifdef DEBUG
    bool             relwrsive  = false;
#endif

    Impl();
    void AppendText(const char* str, size_t size);
    bool IsEntryOverlapping(UINT32 entryIdx, UINT32 begin, UINT32 end) const;
};

AssertInfoSink::AssertInfoSink()  = default;
AssertInfoSink::~AssertInfoSink() = default;

RC AssertInfoSink::Uninitialize()
{
    m_pImpl.reset(nullptr);
    return RC::OK;
}

RC AssertInfoSink::Erase()
{
    if (m_pImpl.get())
    {
        m_pImpl->entries.clear();
        m_pImpl->buffer.clear();
        m_pImpl->nextEntry  = 0;
        m_pImpl->firstEntry = 0;
        m_pImpl->bufferHead = 0;
    }
    return RC::OK;
}

AssertInfoSink::Impl::Impl()
{
    const bool stealth = g_FieldDiagMode != FieldDiagMode::FDM_NONE &&
                         ! Tee::GetFileSink()->IsEncrypted();

    const auto bufSize = stealth ? s_StealthBufSize : s_NormalBufSize;

    entries.reserve(s_MaxEntries);
    buffer.reserve(bufSize);
}

void AssertInfoSink::Impl::AppendText(const char* str, size_t size)
{
    MASSERT(size);
    const auto bufSize = buffer.size();
    MASSERT(bufferHead < bufSize);

    const size_t firstPartSize = min(bufSize - bufferHead, size);
    memcpy(&buffer[bufferHead], str, firstPartSize);

    bufferHead += static_cast<UINT32>(firstPartSize);
    str        += firstPartSize;
    size       -= firstPartSize;

    MASSERT(bufferHead <= bufSize);
    if (bufferHead == buffer.capacity())
    {
        MASSERT(bufferHead == bufSize);
        bufferHead = 0;
    }

    if (size)
    {
        MASSERT(size < bufSize);
        memcpy(&buffer[0], str, size);
        bufferHead = static_cast<UINT32>(size);
    }
}

bool AssertInfoSink::Impl::IsEntryOverlapping(UINT32 entryIdx, UINT32 begin, UINT32 end) const
{
    const auto& entry       = entries[entryIdx];
    const auto  entryBegin  = entry.startOffset;
    const auto  entryEndOvr = entryBegin + entry.size;
    if (begin < end)
    {
        if (entry.startOffset + entry.size <= buffer.size())
        {
            return (entryBegin < end) && (entryEndOvr > begin);
        }
        else
        {
            return (entryBegin < end) ||
                   (!buffer.empty() && ((entryEndOvr % buffer.size()) > begin));
        }
    }
    else
    {
        if (entry.startOffset + entry.size <= buffer.size())
        {
            return (begin < entryEndOvr) ||
                   (!buffer.empty() && ((end % buffer.size()) > entryBegin));
        }
        else
        {
            return true;
        }
    }
}

void AssertInfoSink::DoPrint(const char* str, size_t size, Tee::ScreenPrintState sps)
{
    MASSERT(!"Incorrect Print variant called on AssertInfoSink!");
}

void AssertInfoSink::Print
(
    const char*          str,
    size_t               strLen,
    const Mle::Context*  pMleContext
)
{
    MASSERT(str != nullptr);

    if (!m_pImpl.get())
    {
        return;
    }

    Tasker::MutexHolder lock;
    if (m_Mutex != nullptr)
    {
        lock.Acquire(m_Mutex.get());
    }

    auto& impl = *m_pImpl;

#ifdef DEBUG
    // Prevent MASSERT from relwrsively reentering this function
    if (impl.relwrsive)
    {
        return;
    }
    Utility::TempValue<bool> setRelwrsive(impl.relwrsive, true);
#endif

    const UINT32 entryIdx = impl.nextEntry;

    const auto entriesSize = impl.entries.size();
    MASSERT(entryIdx <= entriesSize);
    MASSERT(entryIdx < s_MaxEntries);

    if (entryIdx == entriesSize)
    {
        MASSERT(entryIdx <= s_MaxEntries);
        impl.entries.emplace_back();
    }

    const UINT32 nextEntryId = (entryIdx + 1 == s_MaxEntries) ? 0 : (entryIdx + 1);
    impl.nextEntry = nextEntryId;
    if ((entryIdx == impl.firstEntry) && (entriesSize > 0))
    {
        impl.firstEntry = nextEntryId;
    }

    MASSERT(entryIdx < impl.entries.size());
    auto& newEntry = impl.entries[entryIdx];

    if (pMleContext && Tee::HasContext(*pMleContext))
    {
        newEntry.context = *pMleContext;
    }
    else
    {
        newEntry.context = Mle::Context();
        Tee::SetContext(&newEntry.context);
    }

    const auto maxSize    = impl.buffer.capacity();
    const auto strSize    = min(strLen, maxSize);
    const auto oldBufSize = impl.buffer.size();

    if (oldBufSize < maxSize)
    {
        MASSERT(impl.bufferHead == oldBufSize);
        impl.buffer.resize(min(oldBufSize + strSize, maxSize));
    }

    const auto startOffset = impl.bufferHead;
    newEntry.startOffset   = startOffset;
    newEntry.size          = static_cast<UINT32>(strSize);

    if (strSize)
    {
        impl.AppendText(str, strSize);
    }

    MASSERT(impl.bufferHead == (startOffset + strSize) % maxSize);
    const auto endOffset = impl.bufferHead;

    // Prune overlapping entries
    UINT32 firstIdx = impl.firstEntry;
    while ((firstIdx != entryIdx) && impl.IsEntryOverlapping(firstIdx, startOffset, endOffset))
    {
        ++firstIdx;
        if (firstIdx == s_MaxEntries)
        {
            firstIdx = 0;
        }
    }

    impl.firstEntry = firstIdx;
}

RC AssertInfoSink::Initialize()
{
    m_pImpl = make_unique<Impl>();

    return RC::OK;
}

bool AssertInfoSink::IsInitialized()
{
    return m_pImpl.get() != nullptr;
}

RC AssertInfoSink::Dump(INT32 dumpPri)
{
    if (!m_pImpl.get())
    {
        Printf(Tee::PriNormal, "AssertInfo buffer disabled\n");
        return RC::CANNOT_LOG_METHOD;
    }

    RC rc;
    CHECK_RC(Tee::TeeFlushQueuedPrints(Tee::DefPrintfQFlushMs));

    Tee::SinkPriorities sinkPri(static_cast<Tee::Priority>(dumpPri));

    // Keep the AssertInfo buffer from capturing data.  We don't
    // want the dump itself to end up being fed back into the buffer!
    unique_ptr<Impl> pSaved;
    {
        Tasker::MutexHolder lock;
        if (m_Mutex != nullptr)
        {
            lock.Acquire(m_Mutex.get());
        }
        pSaved.swap(m_pImpl);
    }

    const auto GetDevPrefix = [](INT32 devInst) -> string
    {
        string devPrefix;
        if (devInst >= 0)
            devPrefix = Utility::StrPrintf("[%2d] ", devInst);
        else
            devPrefix = "[  ] ";
        return devPrefix;
    };

    Mle::Context ctx;
    static const char beginStr[] =
        "------------------------- BEGIN ASSERT INFO DUMP -------------------------\n";
    if (dumpPri == Tee::MleOnly || dumpPri <= Tee::PriAlways)
    {
        Tee::SetContext(&ctx);
    }
    string failedDevPrefix = "\n";
    failedDevPrefix += GetDevPrefix(ErrorMap::DevInst());
    Tee::Write(sinkPri, beginStr, sizeof(beginStr) - 1, failedDevPrefix.c_str(),
               Tee::SPS_END_LIST, Tee::qdmBOTH, &ctx);

    if (!pSaved->entries.empty())
    {
        UINT32 idx = pSaved->firstEntry;
        const UINT32 lastIdx = (pSaved->nextEntry < pSaved->entries.size())
                             ? pSaved->nextEntry : 0;
        string devPrefix;
        INT32  lastDevInst = -2;
        string text;
        do
        {
            const auto& entry = pSaved->entries[idx];

            ++idx;
            if (idx == pSaved->entries.size())
            {
                idx = 0;
            }

            const auto begin  = entry.startOffset;
            const auto endOvr = begin + entry.size;

            text.clear();

            const INT32 devInst = ctx.devInst;
            if (devInst != lastDevInst)
            {
                devPrefix   = GetDevPrefix(devInst);
                lastDevInst = devInst;
            }

            const auto end1 = min(endOvr, static_cast<UINT32>(pSaved->buffer.size()));
            MASSERT(end1 > begin);
            text.append(&pSaved->buffer[begin], end1 - begin);

            if (end1 < endOvr)
            {
                text.append(&pSaved->buffer[0], endOvr - end1);
            }

            Tee::Write(sinkPri,
                       &text[0],
                       text.size(),
                       devPrefix.c_str(),
                       Tee::SPS_END_LIST,
                       Tee::qdmBOTH,
                       &entry.context);
        } while (idx != lastIdx);
    }

    static const char endStr[] =
        "-------------------------- END ASSERT INFO DUMP --------------------------\n";
    if (dumpPri == Tee::MleOnly || dumpPri <= Tee::PriAlways)
    {
        ctx = Mle::Context();
        Tee::SetContext(&ctx);
    }
    Tee::Write(sinkPri, endStr, sizeof(endStr) - 1, failedDevPrefix.c_str(),
               Tee::SPS_END_LIST, Tee::qdmBOTH, &ctx);

    // Re-enable AssertInfo buffer capture.  See comment above.
    Tasker::MutexHolder lock;
    if (m_Mutex != nullptr)
    {
        lock.Acquire(m_Mutex.get());
    }
    pSaved.swap(m_pImpl);
    return rc;
}

//------------------------------------------------------------------------------
// AssertInfo object, properties and methods.
//------------------------------------------------------------------------------

JS_CLASS(AssertInfo);
static SObject AssertInfo_Object
(
   "AssertInfo",
   AssertInfoClass,
   0,
   0,
   "AssertInfo Buffer Class"
);

C_(AssertInfo_Enable);
static STest AssertInfo_Enable
(
   AssertInfo_Object,
   "Enable",
   C_AssertInfo_Enable,
   0,
   "Enable the AssertInfo buffer."
);

C_(AssertInfo_Disable);
static STest AssertInfo_Disable
(
   AssertInfo_Object,
   "Disable",
   C_AssertInfo_Disable,
   0,
   "Disable the AssertInfo buffer."
);

C_(AssertInfo_Dump);
static STest AssertInfo_Dump
(
   AssertInfo_Object,
   "Dump",
   C_AssertInfo_Dump,
   0,
   "Dump the AssertInfo buffer."
);

C_(AssertInfo_Erase);
static STest AssertInfo_Erase
(
   AssertInfo_Object,
   "Erase",
   C_AssertInfo_Erase,
   0,
   "Erase the AssertInfo buffer."
);

// STest
C_(AssertInfo_Enable)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Colwert argument.
   if (NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: AssertInfo.Enable()");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   Tee::SetAssertInfoLevel(Tee::LevLow);
   RETURN_RC(Tee::GetAssertInfoSink()->Initialize());
}

// STest
C_(AssertInfo_Erase)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // Colwert argument.
   if (NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: AssertInfo.Erase()");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   RETURN_RC(Tee::GetAssertInfoSink()->Erase());
}

// STest
C_(AssertInfo_Disable)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0)
   {
      JS_ReportWarning(pContext, "Usage: AssertInfo.Disable()");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   Tee::SetAssertInfoLevel(Tee::LevNone);
   RETURN_RC(Tee::GetAssertInfoSink()->Uninitialize());
}

C_(AssertInfo_Dump)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 1)
   {
      JS_ReportWarning(pContext, "Usage: AssertInfo.Dump(Priority)");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   INT32 Priority;
   if(OK != JavaScriptPtr()->FromJsval(pArguments[0], &Priority))
   {
      JS_ReportWarning(pContext, "Usage: AssertInfo.Dump(Priority)");
      RETURN_RC(RC::SOFTWARE_ERROR);
   }

   RETURN_RC(Tee::GetAssertInfoSink()->Dump(Priority));
}
