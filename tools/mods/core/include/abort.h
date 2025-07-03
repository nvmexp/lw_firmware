/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Abort - object used for aborting MODS

#ifndef INCLUDED_ABORT_H
#define INCLUDED_ABORT_H

#ifndef INCLUDED_TASKER_H
#include "tasker.h"
#endif

class Abort
{
public:
    explicit Abort(UINT32 CtrlCGracefulDelayMs);
    ~Abort();
    static void HandleCtrlC();
    static void DelayedCtrlC();
    static RC Check();
    static RC CheckAndReset();
    void Quit();

private:
    void HandleCtrlCInternal();
    void DelayedCtrlCInternal();
    void AbortThread();
    friend void AbortThreadFunc(void*);

    static Abort* s_Abort;
    UINT64 m_Deadline;
    void* m_Mutex;
    Tasker::ThreadID m_Thread;
    UINT32 m_CtrlCGracefulDelayMs;
    bool m_CtrlC;
    bool m_CtrlCOneTime;
    bool m_Exit;
};

#endif // INCLUDED_ABORT_H
