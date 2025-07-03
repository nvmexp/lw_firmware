/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "thread_mill.h"
#include "core/include/massert.h"

using Tasker::Private::ThreadMill;

bool ThreadMill::ThreadState::IsActive() const
{
    return m_Active;
}

void ThreadMill::ThreadState::Activate()
{
    m_Active = true;
    m_SchedCond.Signal();
}

void ThreadMill::ThreadState::MarkInactive()
{
    m_Active = false;
}

void ThreadMill::ThreadState::Wait(Tasker::Private::NativeMutex& mutex)
{
    while (!m_Active)
    {
        m_SchedCond.Wait(mutex);
    }
}

ThreadMill::~ThreadMill()
{
    NativeLock lock(m_Mutex);

    MASSERT(m_pAllThreads == nullptr);
    MASSERT(m_pScheduledHead == nullptr);
    MASSERT(m_pScheduledTail == nullptr);
}

void ThreadMill::Initialize(ThreadState* pThread, State state)
{
    NativeLock lock(m_Mutex);

    pThread->pNextScheduled = nullptr;
    pThread->state          = Detached;
    pThread->MarkInactive();

    // Connect to the list of all threads
    pThread->pNextExisting = m_pAllThreads;
    m_pAllThreads          = pThread;

    if (state == Attached)
    {
        // Schedule thread to be run at some point in the attached regime
        Schedule(pThread);
    }
}

void ThreadMill::InitLwrrentThread(ThreadState* pThread)
{
    MASSERT(m_TlsThreadPtr.Get() == nullptr);
    m_TlsThreadPtr.Set(pThread);
}

void ThreadMill::Destroy(ThreadState* pThread)
{
    NativeLock lock(m_Mutex);

    // Preferably, threads should detach before exiting, but if an attached thread
    // manages to exit for whatever reason, we need to remove it from the list.
    if (pThread->state == Attached)
    {
        // If there are no other threads to schedule, just leave the list empty
        if (pThread->pNextScheduled == pThread)
        {
            MASSERT(m_pScheduledHead == pThread);
            m_pScheduledHead = nullptr;
            m_pScheduledTail = nullptr;
        }
        // If the deleted thread has just been scheduled, schedule another one
        else if (m_pScheduledHead == pThread)
        {
            MASSERT(m_pScheduledTail->pNextScheduled == pThread);

            ThreadState* const pNext = pThread->pNextScheduled;
            m_pScheduledTail->pNextScheduled = pNext;
            m_pScheduledHead                 = pNext;

            // Release next thread
            pNext->Activate();
        }
        // The thread is somewhere on the list, search for it and remove it
        else
        {
            ThreadState* pPrev = m_pScheduledHead;
            while (pThread != pPrev->pNextScheduled)
            {
                pPrev = pPrev->pNextScheduled;
            }

            pPrev->pNextScheduled = pThread->pNextScheduled;

            if (m_pScheduledTail == pThread)
            {
                m_pScheduledTail = pPrev;
            }
        }
    }

    // Delete the thread from the list of all threads
    ThreadState** pNextPtr = &m_pAllThreads;
    for (;;)
    {
        ThreadState* const pLwr = *pNextPtr;
        if (pLwr == pThread)
        {
            break;
        }
        pNextPtr = &pLwr->pNextExisting;
    }
    *pNextPtr = pThread->pNextExisting;
}

ThreadMill::ThreadState* ThreadMill::GetThreadState()
{
    return static_cast<ThreadState*>(m_TlsThreadPtr.Get());
}

bool ThreadMill::SwitchToNextThread()
{
    ThreadState* const pThread = GetThreadState();

    // If this thread has not been registered or if it's not part of the
    // attached regime, just return
    if (!pThread || pThread->state == Detached)
    {
        return false;
    }

    // Update the list of threads while holding the global lock
    {
        NativeLock lock(m_Mutex);

        MASSERT(pThread == m_pScheduledHead);
        MASSERT(m_pScheduledTail);
        MASSERT(pThread->IsActive());

        // There are no other threads to schedule, just return
        if (pThread->pNextScheduled == pThread)
        {
            return false;
        }

        // Put the thread on the back of the list
        MASSERT(m_pScheduledTail->pNextScheduled == pThread);
        m_pScheduledHead = pThread->pNextScheduled;
        m_pScheduledTail = pThread;

        // Mark self as inactive
        pThread->MarkInactive();

        // Release next thread
        m_pScheduledHead->Activate();

        // Wait for this thread's next turn
        pThread->Wait(m_Mutex);
    }

    return true;
}

void ThreadMill::Schedule(ThreadState* pThread)
{
    const bool makeActive = m_pScheduledTail == nullptr;

    ThreadState** ppPrev = makeActive ? &m_pScheduledHead : &m_pScheduledTail->pNextScheduled;

    *ppPrev                 = pThread;
    m_pScheduledTail        = pThread;
    pThread->pNextScheduled = m_pScheduledHead;
    pThread->state          = Attached;

    if (makeActive)
    {
        pThread->Activate();
    }
}

bool ThreadMill::Attach()
{
    ThreadState* const pThread = GetThreadState();

    // If this thread has not been registered or if it's already part of the
    // attached regime, just return
    if (!pThread || pThread->state == Attached)
    {
        return false;
    }

    // Update the list of threads while holding the global lock
    {
        NativeLock lock(m_Mutex);

        MASSERT(pThread->pNextScheduled == nullptr);

        Schedule(pThread);

        // If this is the only thread scheduled, then the scheduling mutex
        // is already locked, so just continue running
        if (pThread->pNextScheduled == pThread)
        {
            MASSERT(pThread->IsActive());
            return true;
        }
        MASSERT(!pThread->IsActive());

        // Wait for this thread's next turn
        pThread->Wait(m_Mutex);
    }

    return true;
}

bool ThreadMill::Detach()
{
    ThreadState* const pThread = GetThreadState();

    // If this thread has not been registered or if it's not part of the
    // attached regime, just return
    if (!pThread || pThread->state == Detached)
    {
        return false;
    }

    // Update the list of threads while holding the global lock
    NativeLock lock(m_Mutex);

    MASSERT(pThread == m_pScheduledHead);
    MASSERT(m_pScheduledTail);
    MASSERT(pThread->IsActive());

    // Mark self as inactive
    pThread->MarkInactive();

    // If there are no other threads to schedule, just leave the list empty
    if (pThread->pNextScheduled == pThread)
    {
        m_pScheduledHead = nullptr;
        m_pScheduledTail = nullptr;
    }
    // Otherwise release the next thread
    else
    {
        MASSERT(m_pScheduledTail->pNextScheduled == pThread);

        ThreadState* const pNext = pThread->pNextScheduled;
        m_pScheduledTail->pNextScheduled = pNext;
        m_pScheduledHead                 = pNext;

        // Release next thread
        pNext->Activate();
    }

    pThread->state          = Detached;
    pThread->pNextScheduled = nullptr;

    return true;
}
