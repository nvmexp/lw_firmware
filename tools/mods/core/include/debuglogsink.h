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

#pragma once

#include "datasink.h"
#include "rc.h"
#include "tasker.h"
#include <string>
#include <vector>
#include <list>
#include <stdio.h>

/**
 * @class( DebugLogSink )
 *
 * Print Tee's stream to a full debug log in a file.
 *
 * The DebugLogSink writes logs to a buffer. When the buffer is full, the buffer
 * is pushed into a queue and the current buffer is emptied. There is a background
 * detached thread running, which waits for buffers to appear on the queue and
 * writes the buffers to a disk file, named typically debug.log.
 *
 * The thread occasionally wakes up if no buffers were pushed and writes the current
 * buffer to the file (and empties the buffer). This way the log is saved even if
 * there is a hang condition which is not critical to the system (processes and disk
 * I/O are still alive).
 */

class DebugLogSink : public DataSink
{
public:
    DebugLogSink();
    ~DebugLogSink();

    RC SetFileName(const char* filename);
    const char* const GetFileName() const { return m_FileName.c_str(); }

    void SetFilterString(const char* filter);
    const char* const GetFilterString() const { return m_Filter.c_str(); }

    void SetFileLimitMb(UINT32 fileLimitMb) { m_FileLimitMb = fileLimitMb; }
    UINT32 GetFileLimitMb() const { return m_FileLimitMb; }

    enum { NO_FILE_SIZE_LIMIT = 0 };

    RC   Initialize();
    bool IsInitialized() const { return m_IsEnabled; }
    void Uninitialize();
    RC   PostTaskerInit();
    void KillThread();
    void Flush();

protected:
    // DataSink override.
    void DoPrint(const char* str, size_t strLen, Tee::ScreenPrintState sps) override;

private:
    static void PrintThreadFunc(void* arg);
    void PrintThread();
    void DumpAllPendingBuffers();
    void DumpOldestPendingBuffer();
    bool CheckLimit(size_t size, size_t* pOutSize) const;
    void WriteBuffer(const void* buf, size_t size);
    void WriteBuffer(const vector<char>& buf);
    RC Tell(long* pOffset) const;

    string              m_FileName;
    bool                m_IsEnabled;
    const UINT32        m_BufferSize;
    volatile UINT32     m_BufferLock;
    vector<char>        m_Buffer;
    list<vector<char>*> m_BufferQueue;
    Tasker::ThreadID    m_Thread;
    void*               m_BufferQueueMutex;
    SemaID              m_BufferQueueCountSema;
    FILE*               m_pFile;
    RC                  m_WriteError;
    string              m_Filter;
    UINT32              m_FileLimitMb;
};
