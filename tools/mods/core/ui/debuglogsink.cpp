/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/debuglogsink.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include <algorithm>
#include <string.h>

DebugLogSink::DebugLogSink()
: m_IsEnabled(false)
 ,m_BufferSize(1024*1024)
 ,m_BufferLock(0)
 ,m_Thread(Tasker::NULL_THREAD)
 ,m_BufferQueueMutex(0)
 ,m_BufferQueueCountSema(0)
 ,m_pFile(0)
 ,m_FileLimitMb(NO_FILE_SIZE_LIMIT)
{
}

DebugLogSink::~DebugLogSink()
{
    Uninitialize();
}

RC DebugLogSink::SetFileName(const char* filename)
{
    if (IsInitialized())
    {
        return RC::SOFTWARE_ERROR;
    }
    m_FileName = filename;
    return OK;
}

void DebugLogSink::SetFilterString(const char* filter)
{
    m_Filter = filter;
}

RC DebugLogSink::Initialize()
{
    MASSERT(!IsInitialized());
    if (IsInitialized())
    {
        return RC::SOFTWARE_ERROR;
    }

    // Open log file
    m_WriteError.Clear();
    if (m_FileName.empty())
    {
        return RC::SOFTWARE_ERROR;
    }
    const char* openMode = "w+";
    RC rc;
    CHECK_RC(Utility::OpenFile(m_FileName, &m_pFile, openMode));

    // Reserve buffer
    {
        Tasker::SpinLock lock(&m_BufferLock);
        m_Buffer.clear();
        m_Buffer.reserve(m_BufferSize);
        m_IsEnabled = true;
    }

    Printf(Tee::PriDebug, "Debug log opened\n");

    // Create printing thread
    if (Tasker::IsInitialized())
    {
        PostTaskerInit();
    }
    return OK;
}

void DebugLogSink::Uninitialize()
{
    m_Filter = "";

    if (!IsInitialized())
    {
        m_FileName = "";
        return;
    }

    // Kill the printing thread
    KillThread();

    // Flush all remaining printouts
    Printf(Tee::PriDebug, "Closing Debug log\n");
    Flush();

    // Close file
    fclose(m_pFile);
    m_pFile = 0;
    m_FileName = "";

    // Release buffer
    {
        Tasker::SpinLock lock(&m_BufferLock);
        vector<char> empty;
        m_Buffer.swap(empty);
        m_IsEnabled = false;
    }

    Printf(Tee::PriDebug, "Debug log closed\n");
}

RC DebugLogSink::PostTaskerInit()
{
    MASSERT(Tasker::IsInitialized());
    MASSERT(IsInitialized());
    if (!Tasker::IsInitialized() || !IsInitialized())
    {
        return RC::SOFTWARE_ERROR;
    }

    if (m_Thread != Tasker::NULL_THREAD)
    {
        return OK;
    }

    m_BufferQueueMutex = Tasker::AllocMutex("DebugLogSink::m_BufferQueueMutex",
                                            Tasker::mtxUnchecked);
    MASSERT(m_BufferQueueMutex);

    m_BufferQueueCountSema = Tasker::CreateSemaphore(0, "DebugLogSink");
    MASSERT(m_BufferQueueCountSema);

    m_Thread = Tasker::CreateThread(PrintThreadFunc, this, 0, "DebugLogSink");
    MASSERT(m_Thread != Tasker::NULL_THREAD);

    Printf(Tee::PriDebug, "Debug log printing thread created\n");

    return OK;
}

void DebugLogSink::KillThread()
{
    if (!Tasker::IsInitialized())
    {
        return;
    }

    if (m_Thread == Tasker::NULL_THREAD)
    {
        return;
    }

    // Flush pending messages
    Flush();

    // Push an null pointer onto the queue to signal end of processing
    {
        Tasker::MutexHolder lock(m_BufferQueueMutex);
        m_BufferQueue.push_back(0);
        Tasker::ReleaseSemaphore(m_BufferQueueCountSema);
    }

    // Wait for the thread to stop
    Printf(Tee::PriDebug, "Waiting for Debug log printing thread to exit\n");
    if (OK != Tasker::Join(m_Thread))
    {
        MASSERT(0);
    }
    MASSERT(m_BufferQueue.empty());

    // Destroy objects
    m_Thread = Tasker::NULL_THREAD;
    Tasker::DestroySemaphore(m_BufferQueueCountSema);
    m_BufferQueueCountSema = 0;
    Tasker::FreeMutex(m_BufferQueueMutex);
    m_BufferQueueMutex = 0;

    Printf(Tee::PriDebug, "Debug log printing thread killed\n");
}

void DebugLogSink::Flush()
{
    if (!IsInitialized())
    {
        return;
    }

    // Lock queue mutex
    Tasker::MutexHolder lock;
    if (m_BufferQueueMutex)
    {
        lock.Acquire(m_BufferQueueMutex);
    }

    // Write all pending items from the queue
    DumpAllPendingBuffers();

    // Write current buffer and reserve a new one
    vector<char> buf;
    buf.reserve(m_BufferSize);
    {
        Tasker::SpinLock lock(&m_BufferLock);
        buf.swap(m_Buffer);
    }
    WriteBuffer(buf);

    // Flush all disk operations
    Xp::FlushFstream(m_pFile);
}

void DebugLogSink::DoPrint(const char* str, size_t strLen, Tee::ScreenPrintState sps)
{
    MASSERT(str);

    // Don't do anything if sink not initialized
    if (!IsInitialized())
    {
        return;
    }

    // Don't do anything if there was a write error
    if (OK != m_WriteError)
    {
        return;
    }

    // Don't print if filter does not match
    if (!m_Filter.empty() && (0 == memmem(str, strLen, m_Filter.data(), m_Filter.size())))
    {
        return;
    }

    while (strLen > 0)
    {
        // Add portion of the string to the buffer,
        // but do not exceed the buffer's capacity
        size_t printSize = 0;
        {
            Tasker::SpinLock lock(&m_BufferLock);
            printSize = min(strLen, m_Buffer.capacity() - m_Buffer.size());
            if (printSize > 0)
            {
                m_Buffer.insert(m_Buffer.end(), str, str + printSize);
            }
        }
        strLen -= printSize;
        str += printSize;

        // If the buffer is full, push it to the queue and create a new one
        if (strLen > 0)
        {
            // Lock queue mutex
            Tasker::MutexHolder lock;
            if (m_BufferQueueMutex)
            {
                lock.Acquire(m_BufferQueueMutex);
            }

            // Push empty buffer with reserved capacity onto the queue
            // and then swap it with current buffer
            m_BufferQueue.push_back(new vector<char>());
            m_BufferQueue.back()->reserve(m_BufferSize);
            {
                Tasker::SpinLock lock(&m_BufferLock);
                m_BufferQueue.back()->swap(m_Buffer);
            }

            // If the queue semaphore was initialized,
            // signal that there is new item in the queue
            if (m_BufferQueueCountSema)
            {
                Tasker::ReleaseSemaphore(m_BufferQueueCountSema);
            }
        }
    }
}

void DebugLogSink::PrintThreadFunc(void* arg)
{
    static_cast<DebugLogSink*>(arg)->PrintThread();
}

void DebugLogSink::PrintThread()
{
    // Run the thread as detached to minimize impact on tests
    Tasker::DetachThread detach;

    bool quit = false;
    while (!quit)
    {
        // Wait for something to appear in the queue
        // Time out and flush everything occasionally
        Tasker::AcquireSemaphore(m_BufferQueueCountSema, 10000);

        // Lock queue mutex
        Tasker::MutexHolder lock(m_BufferQueueMutex);

        // Flush current buffer if there is not enough action
        if (m_BufferQueue.empty())
        {
            Flush();

            // Print message to inform that a forced flush oclwrred
            const char msg[] = "Debug log forced flush\n";
            WriteBuffer(msg, sizeof(msg)-1);
        }

        // Write all pending items from the queue
        DumpAllPendingBuffers();

        // Set quit condition if the item in the queue is a null pointer
        quit = !m_BufferQueue.empty() && (m_BufferQueue.front() == 0);

        // Pop the null pointer from the queue
        if (quit)
        {
            m_BufferQueue.pop_front();
        }
    }
}

void DebugLogSink::DumpAllPendingBuffers()
{
    MASSERT(IsInitialized());
    while (!m_BufferQueue.empty() && (m_BufferQueue.front() != 0))
    {
        DumpOldestPendingBuffer();
    }
}

void DebugLogSink::DumpOldestPendingBuffer()
{
    MASSERT(!m_BufferQueue.empty());
    vector<char>* const pBuf = m_BufferQueue.front();
    m_BufferQueue.pop_front();
    if (pBuf)
    {
        WriteBuffer(*pBuf);
        delete pBuf;
    }
}

bool DebugLogSink::CheckLimit(size_t size, size_t* pOutSize) const
{
    *pOutSize = size;

    if (m_FileLimitMb == NO_FILE_SIZE_LIMIT)
    {
        return false;
    }

    long offset = 0;
    if (OK != Tell(&offset))
    {
        return false;
    }

    const size_t maxSize =
        static_cast<size_t>(m_FileLimitMb) * 1024 * 1024;
    if (offset + size <= maxSize)
    {
        return false;
    }

    if (static_cast<size_t>(offset) < maxSize)
    {
        *pOutSize = maxSize - offset;
    }
    else
    {
        *pOutSize = 0;
    }
    return true;
}

void DebugLogSink::WriteBuffer(const void* buf, size_t size)
{
    if (OK == m_WriteError)
    {
        // Do not write more than the limit allows
        const bool limitReached = CheckLimit(size, &size);

        // Write buffer to file
        if (size > 0)
        {
            m_WriteError = Utility::FWrite(buf, size, 1, m_pFile);
        }

        // Print a message if a write error oclwrred
        if (OK != m_WriteError)
        {
            Printf(Tee::PriHigh, "Error writing to %s - %s\n",
                   m_FileName.c_str(), m_WriteError.Message());
        }

        // Stop writing and flush file buffers if limit reached
        else if (limitReached)
        {
            m_WriteError = RC::FILE_WRITE_ERROR;
            Xp::FlushFstream(m_pFile);
        }
    }
}

void DebugLogSink::WriteBuffer(const vector<char>& buf)
{
    WriteBuffer(&buf[0], buf.size());
}

RC DebugLogSink::Tell(long* pOffset) const
{
    MASSERT(pOffset);

    if (!IsInitialized())
    {
        *pOffset = 0;
        return OK;
    }

    const long offset = ftell(m_pFile);

    if (offset == -1L)
    {
        return Utility::InterpretFileError();
    }

    *pOffset = offset;

    return OK;
}
