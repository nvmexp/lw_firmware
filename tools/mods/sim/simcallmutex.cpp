/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include "core/include/tasker.h"
#include "core/include/cpu.h"
#include "core/include/globals.h"
#include "simcallmutex.h"
/* static */unsigned int SimCallMutex::s_ChiplibFPCW = SimCallMutex::UNINITIALIZEDCW;
/* static */unsigned int SimCallMutex::s_ChiplibMxcsr = SimCallMutex::UNINITIALIZEDCW;
/* static */void * SimCallMutex::s_Mutex = 0;

SimCallMutex::SimCallMutex()
{
    if (s_Mutex)
    {
        Tasker::AcquireMutex(s_Mutex);
    }

    g_ChiplibBusy ++;

    if (s_ChiplibFPCW == SimCallMutex::UNINITIALIZEDCW)
    {
        m_FpControlWord = Cpu::GetFpControlWord();
    }
    else
    {
        m_FpControlWord = Cpu::SetFpControlWord(s_ChiplibFPCW);
    }

    if (s_ChiplibMxcsr == SimCallMutex::UNINITIALIZEDCW)
    {
        m_MxcsrControl = Cpu::GetMXCSRControlWord();
    }
    else
    {
        m_MxcsrControl = Cpu::SetMXCSRControlWord(s_ChiplibMxcsr);
    }
}

SimCallMutex::~SimCallMutex()
{
    s_ChiplibMxcsr = Cpu::SetMXCSRControlWord(m_MxcsrControl);
    s_ChiplibFPCW = Cpu::SetFpControlWord(m_FpControlWord);

    g_ChiplibBusy --;

    if (s_Mutex)
    {
        Tasker::ReleaseMutex(s_Mutex);
    }
}

/* static */ void * SimCallMutex::GetMutex()
{
    return s_Mutex;
}

/* static */ void SimCallMutex::SetMutex(void * mutex)
{
    if (0 != mutex)
    {
        // one mutex shared by all instance
        MASSERT(0 == s_Mutex);
    }
    s_Mutex = mutex;
}
