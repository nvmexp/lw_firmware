/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2016,2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/datasink.h"
#include "inc/bytestream.h"
#include "core/include/mle_protobuf.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include <cstring>

DataSink::DataSink()
{
    m_PrevContext.uid        = 0;
    m_PrevContext.timestamp  = 0;
    m_PrevContext.threadId   = 0;
    m_PrevContext.devInst    = 0;
    m_PrevContext.testId     = 0;
}

// If we're creating a MODS Log Encoded file, we need to add a version print
// as a header, before other log prints, so we can perform a version check.
// All MLE newlines are delimited by a null char, so we need to make sure the
// header also starts with it.
bool DataSink::DoPrintMleHeader()
{
    ByteStream bs;
    Mle::MLE(&bs).file_type("MLE");
    return DoPrintBinary(bs.data(), bs.size());
}

void DataSink::Print
(
    const char*           str,
    size_t                strLen,
    const char*           pLinePrefix,
    const ByteStream*     pBinaryPrefix,
    Tee::ScreenPrintState sps
)
{
    if (!IsSinkOpen())
    {
        // If the log file hasn't been opened yet, cache the output so that we
        // can print the text into the log once the log is opened.
        CacheOutput(str, strLen);
        return;
    }
    Tasker::MutexHolder mh;
    if (m_Mutex != nullptr)
    {
        mh.Acquire(m_Mutex.get());
    }
    if (m_bLineStart)
    {
        if (pBinaryPrefix && !pBinaryPrefix->empty())
        {
            DoPrintBinary(pBinaryPrefix->data(), pBinaryPrefix->size());
        }
        if (pLinePrefix && *pLinePrefix)
        {
            DoPrint(pLinePrefix, strlen(pLinePrefix), sps);
        }
    }

    DoPrint(str, strLen, sps);
    m_bLineStart = (strLen > 0) && (str[strLen - 1] == '\n');
}

void DataSink::PrintRaw(const ByteStream& data)
{
    if (!IsSinkOpen())
    {
        return;
    }

    Tasker::MutexHolder mh;
    if (m_Mutex.get())
    {
        mh.Acquire(m_Mutex.get());
    }

    DoPrintBinary(data.data(), data.size());

    const auto size = data.size();
    m_bLineStart = (size > 0) && (data[size - 1] == '\n');
}

void DataSink::PrintMle(ByteStream* pData, Tee::Priority pri, const Mle::Context& ctx)
{
    if (!IsSinkOpen())
    {
        return;
    }

    Tasker::MutexHolder mh;
    if (m_Mutex.get())
    {
        mh.Acquire(m_Mutex.get());
    }

    const auto MakeId = [](INT32 oldId, INT32 newId) -> INT32
    {
        return
            (oldId == newId) ? 0 :
            (newId == -1)    ? -1
                             : (newId + 1);
    };

    auto entry = Mle::MLE::entry(pData, Mle::DumpPos(0));
    entry.uid_delta(static_cast<INT64>(ctx.uid - m_PrevContext.uid - 1));
    entry.timestamp_delta(static_cast<INT64>(ctx.timestamp - m_PrevContext.timestamp));
    entry.thread_id(MakeId(m_PrevContext.threadId, ctx.threadId));
    entry.test_id(MakeId(m_PrevContext.testId, ctx.testId));
    entry.dev_id(MakeId(m_PrevContext.devInst, ctx.devInst));
    if (pri != m_PrevPri)
    {
        entry.priority(static_cast<Mle::Entry::Priority>(pri));
    }
    entry.Finish();

    const auto size = pData->size();
    DoPrintBinary(pData->data(), size);

    // The context fields (written to MLE file above) are defined to have the
    // value of 0 if they did not change between log entries.  This allows us
    // to reduce log size, since these fields very often have the same value
    // in conselwtive log entries, and the value of 0 is not emitted into the
    // log.  To facilitate this, we save the previous value of context fields.
    m_PrevContext = ctx;
    m_PrevPri     = pri;
}

void DataSink::PostTaskerInitialize()
{
    MASSERT(!m_Mutex.get());
    m_Mutex = Tasker::CreateMutex("DataSink Mutex", Tasker::mtxUnchecked);
}

void DataSink::PreTaskerShutdown()
{
    if (m_Mutex.get())
    {
        MASSERT(!Tasker::CheckMutex(m_Mutex.get()));
        m_Mutex = Tasker::NoMutex();
    }
}
