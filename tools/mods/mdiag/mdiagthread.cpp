/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2015, 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/xp.h"
#include "mdiag/utils/types.h"
#include "thread.h"
#include "core/include/tasker.h"

// static objects for operations Thread performs but Tasker does not
// make use of the following object...

Thread *Thread::s_pMainThread = 0;
map<int, Thread *> Thread::s_ChildMap;


// This creates the original "thread".  The MDIAG compatibility layer
// has the concept of a hierarchy of threads, which Tasker does not
// have.

Thread::Thread()
{
    MASSERT(s_pMainThread == 0);
    s_pMainThread = this;
    s_ChildMap.clear();
    m_Children.clear();
    m_pParent = nullptr;
    m_ArgList = {};
    m_TaskId = Tasker::NULL_THREAD;
    m_pJoinEvent = nullptr;
    m_pSleepEvent = nullptr;
}

  // 2. create a new thread, whose life consists of a call to the given
  //  'mainfunc' (however long that takes)
Thread::Thread(Thread *myParent, MainFunc mainfunc, int argc, const char *argv[],
                unsigned stack_size, bool stack_chk, const char *name /* = NULL */)
{
    MASSERT(myParent != nullptr);
    if (!s_pMainThread) {
        s_pMainThread = new Thread();
    }
    m_pParent = myParent;
    m_Children.clear();

    m_pJoinEvent = nullptr;
    m_pSleepEvent = nullptr;
    m_pParent->m_Children.push_back(this);  // add to hierarchical list of members
    m_TaskId = CreateTaskerThread(mainfunc, argc, argv, stack_size, name);
    s_ChildMap[m_TaskId] = this;  // add to master hashtable
}

Thread::~Thread()
{
//    MASSERT (Thread::LwrrentThread() != this);
    if (m_pSleepEvent) {
        Tasker::FreeEvent(m_pSleepEvent);
        m_pSleepEvent = 0;
    }
    if (this == s_pMainThread)   s_pMainThread = 0;
    // remove from master hashtable
    for (map<int, Thread*>::iterator it = s_ChildMap.begin(); it != s_ChildMap.end();  it++) {
        if (it->second == this) {
            s_ChildMap.erase(it);
            break;
        }
    }
    // to be redundant, we could remove from parent list --
    // but something would be very wrong if the parent still had a pointer
    // to a Thread being deleted; better that this is an assert
    if (m_pParent) {
        for (list<Thread*>::iterator it =m_pParent->m_Children.begin(); it !=m_pParent->m_Children.end(); it++) {
            MASSERT(*it != this);
        }
    }
}

// this is the only nonstatic public member function; static members
// implicitly use lwrrent_thread
void Thread::Awaken()
{
    if (m_pSleepEvent != 0) {
        Tasker::SetEvent(m_pSleepEvent);
    }
}

// all the other public functions are static.

// yields and takes the thread out of the scheduling ring - something else
//  better wake it up
// static
void Thread::Sleep()
{
    Thread *lwrrent_thread = Thread::LwrrentThread();
    if (!lwrrent_thread->m_pSleepEvent) {
        lwrrent_thread->m_pSleepEvent = Tasker::AllocEvent();
    }
    Tasker::WaitOnEvent(lwrrent_thread->m_pSleepEvent);
    Tasker::FreeEvent(lwrrent_thread->m_pSleepEvent);
    lwrrent_thread->m_pSleepEvent = 0;
}

// static
void Thread::Yield(unsigned min_delay_in_usec)
{
//   min_delay_in_usec is not used or implemented in mdiag either
//    if (min_delay_in_usec > 0) {
//        Tasker::WaitOnEvent(eventID, min_delay_in_usec / 1000.0);  // for Tasker, this is wall-clock
//    }
    Tasker::Yield();
}

// Blocking: -- all threads register as block participants, and then
// all threads wake up at once when the last one calls Block.

// This mechanism is copied from mdiag. It is safe only in a cooperative multitasking
// environment.

Thread::BlockSyncPoint::BlockSyncPoint(const string& syncPointName) :
    m_SyncPointName(syncPointName)
{
}

// map of test stage to list of threads
map<Thread::BlockSyncPoint*, list<Thread*>> Thread::s_RegisteredBlockingList;
map<Thread::BlockSyncPoint*, list<Thread*>> Thread::s_NotYetBlockedList;

// called once per thread, ever
void Thread::RegisterForBlock(BlockSyncPoint* syncPoint)
{
    Thread *lwrrent_thread = Thread::LwrrentThread();
    s_RegisteredBlockingList[syncPoint].push_back(lwrrent_thread);
    s_NotYetBlockedList[syncPoint].push_back(lwrrent_thread);
}

void Thread::Block(BlockSyncPoint* syncPoint, BlockSyncPoint* nextSyncPoint)
{
    Thread *lwrrent_thread = Thread::LwrrentThread();
    s_NotYetBlockedList[syncPoint].remove(lwrrent_thread);
    if (s_NotYetBlockedList[syncPoint].empty())
    {
        list<Thread*>::iterator itr = s_RegisteredBlockingList[syncPoint].begin();
        while (itr != s_RegisteredBlockingList[syncPoint].end())
        {
            Thread *awakenThread = *itr;
            itr = s_RegisteredBlockingList[syncPoint].erase(itr);
            s_RegisteredBlockingList[nextSyncPoint].push_back(awakenThread);
            s_NotYetBlockedList[nextSyncPoint].push_back(awakenThread);
            awakenThread->Awaken();
        }
    }
    else 
    {
        Sleep();
    }
}

void Thread::Terminate()
{
    Thread *lwrrent_thread = Thread::LwrrentThread();
    // if a parent is terminated before its children, it has to wait
    //  for them all to perish
    if(!lwrrent_thread->m_Children.empty()) {
        JoinAll();
    }

    Thread *parent = lwrrent_thread->m_pParent;
    if (parent) {
        if (parent->m_pJoinEvent) {
            Tasker::SetEvent(parent->m_pJoinEvent);
        }
    }
    Tasker::ExitThread();
    Yield(); // unnecessary, never gets called
}

Thread *Thread::Fork(MainFunc mainfunc, int argc, const char *argv[],
                     unsigned stack_size, bool stack_check,
                     const char *name /* = NULL */)
{
    return new Thread(Thread::LwrrentThread(),
                      mainfunc, argc, argv,
                      stack_size, stack_check, name);
}

  // causes a thread to sleep until 'child_thread' (or all m_Children) finishes
    // this is not used in mdiag, so it is not implemented here
// int Thread::Join(Thread *child_thread)

// static
int Thread::JoinAll()
{
    Thread *lwrrent_thread = Thread::LwrrentThread();
    if (lwrrent_thread->m_Children.empty())
        return 0;

    while (!lwrrent_thread->m_Children.empty()) {
        list<Thread *>::iterator first_child = lwrrent_thread->m_Children.begin();
        Tasker::Join((*first_child)->m_TaskId);
        lwrrent_thread->m_Children.erase(first_child);
    }
    return 1;
}

  // wait until at least one child finishes (returns immediately if no m_Children)
// static
int Thread::JoinAny()
{
    Thread *lwrrent_thread = Thread::LwrrentThread();
    if (lwrrent_thread->m_Children.empty())
        return 0;

    lwrrent_thread->m_pJoinEvent = Tasker::AllocEvent();
    Tasker::WaitOnEvent(lwrrent_thread->m_pJoinEvent);
    // this event was triggered; it means one thread terminated
    Tasker::FreeEvent(lwrrent_thread->m_pJoinEvent);
    lwrrent_thread->m_pJoinEvent = 0;

    // JoinAll is always called after this, so do not bother to Join the child here
    return 1;
}

Tasker::ThreadID Thread::CreateTaskerThread(MainFunc main, int argc, const char *argv[], int stack_size,
                                            const char *name)
{
    m_ArgList.fn = main;
    m_ArgList.argc = argc;
    m_ArgList.argv = argv;
    return Tasker::CreateThread(Thread::TaskerThread, static_cast<void *>(&m_ArgList), stack_size, name);
}
// static
void Thread::TaskerThread(void *arg)
{
    ThreadFuncArgs *funcarg = static_cast<ThreadFuncArgs *>(arg);
    (funcarg->fn)(funcarg->argc, funcarg->argv);
    Thread::Terminate();
}

// static
Thread *Thread::LwrrentThread() {
    map<int, Thread *>::iterator it = s_ChildMap.find(Tasker::LwrrentThread());
    return (it == s_ChildMap.end()) ? s_pMainThread : it->second;
}

const char * Thread::GetName()
{
    return (s_pMainThread == this) ? "" : Tasker::ThreadName(this->m_TaskId);
}
