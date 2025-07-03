/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010,2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/abort.h"
#include "core/include/xp.h"
#include "core/include/globals.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/errormap.h"
#include "core/include/log.h"

Abort* Abort::s_Abort = 0;

void AbortThreadFunc(void* a)
{
    static_cast<Abort*>(a)->AbortThread();
}

Abort::Abort(UINT32 CtrlCGracefulDelayMs)
: m_Deadline(0),
  m_Mutex(Tasker::AllocMutex("Abort::m_Mutex", Tasker::mtxUnchecked)),
  m_Thread(Tasker::NULL_THREAD),
  m_CtrlCGracefulDelayMs(CtrlCGracefulDelayMs),
  m_CtrlC(false),
  m_CtrlCOneTime(false),
  m_Exit(false)
{
    MASSERT(s_Abort == 0);
    s_Abort = this;
    m_Thread = Tasker::CreateThread(AbortThreadFunc, this, Tasker::MIN_STACK_SIZE, "CtrlCHandler");
}

Abort::~Abort()
{
    s_Abort = 0;
    Quit();
    Tasker::FreeMutex(m_Mutex);
}

void Abort::HandleCtrlC()
{
    Abort* const pAbort = s_Abort;
    if (pAbort)
    {
        pAbort->HandleCtrlCInternal();
    }
}

void Abort::HandleCtrlCInternal()
{
    if (!m_CtrlC)
    {
        m_Deadline = Xp::QueryPerformanceCounter() +
            (m_CtrlCGracefulDelayMs * Xp::QueryPerformanceFrequency() / 1000);
        m_CtrlC = true;
    }
    m_CtrlCOneTime = true;
}

RC Abort::Check()
{
    if (s_Abort && s_Abort->m_CtrlC)
    {
        return RC::USER_ABORTED_SCRIPT;
    }
    return OK;
}

RC Abort::CheckAndReset()
{
    if (s_Abort && s_Abort->m_CtrlCOneTime)
    {
        s_Abort->m_CtrlCOneTime = false;
        return RC::USER_ABORTED_SCRIPT;
    }
    return OK;
}

void Abort::Quit()
{
    Tasker::MutexHolder Lock(m_Mutex);
    if (m_Thread != Tasker::NULL_THREAD)
    {
        m_Exit = true;
        Tasker::Join(m_Thread);
        m_Exit = false;
        m_Thread = Tasker::NULL_THREAD;
    }
}

void Abort::AbortThread()
{
    while (!m_Exit && (!m_CtrlC || (Xp::QueryPerformanceCounter() < m_Deadline)))
    {
        Tasker::Sleep(100);
    }
    if (!m_Exit)
    {
        // Unhook Ctrl-C if timeout expired letting the user to hard-kill MODS
        Printf(Tee::PriHigh, "Unable to interrupt the test. Press Ctrl-C again to kill MODS.\n");
        JavaScriptPtr pJavaScript;
        pJavaScript->UnhookCtrlC();
    }
}

C_(Global_CheckUserAbort)
{
    MASSERT(pContext != 0);
    ErrorMap Error(Abort::Check());
    Log::SetFirstError(Error);
    RETURN_RC(Abort::Check());
}

static STest Global_CheckUserAbort
(
    "CheckUserAbort",
    C_Global_CheckUserAbort,
    0,
    "Check whether user pressed Ctrl+C and return appropriate RC."
);
