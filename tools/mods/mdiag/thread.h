/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015, 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_THREAD_H
#define INCLUDED_THREAD_H

#include "mdiag/utils/types.h"
#include <list>
#include <map>

typedef int (*MainFunc)(int argc, const char *argv[]);

struct ThreadFuncArgs {
    MainFunc fn;
    int argc;
    const char **argv;
};
//   typedef void (* ThreadFunc)(void *);

class ModsEvent;  // forward declaration, see core/include/tasker.h, tasker_p.h

class Thread {
public:

    class BlockSyncPoint
    {   
        public:
            BlockSyncPoint(const string& syncPointName);

            BlockSyncPoint() = delete;
            BlockSyncPoint(BlockSyncPoint&) = delete;
            BlockSyncPoint &operator=(BlockSyncPoint&) = delete;
        private:
            string m_SyncPointName;
    };
    
    // several constructors, depending on what you want the thread to do

    // 1. create the original thread, hijacking the current stack
    Thread();

    // 2. create a new thread, whose life consists of a call to the given
    //  'mainfunc' (however long that takes)
    Thread(Thread *parent, MainFunc mainfunc, int argc, const char *argv[],
           unsigned stack_size, bool stack_chk, const char *name = "");

    ~Thread();

    static Thread *LwrrentThread();

    // a bunch of static calls that all implicitly use the current thread
    static void Yield(unsigned min_delay_in_usec = 0);

    // yields and takes the thread out of the scheduling ring - something else
    //  better wake it up
    static void Sleep();
    void Awaken();

    // some threads need to be able to synchronize - they need to register
    // calling block() does the actual synchronization
    static void RegisterForBlock(BlockSyncPoint* syncPoint);
    static void Block(BlockSyncPoint* syncPoint, BlockSyncPoint* nextSyncPoint);

    static void Terminate();

    static Thread *Fork(MainFunc mainfunc, int argc, const char *argv[],
                        unsigned stack_size, bool stack_check, const char *name = "");

    // causes a thread to sleep until 'child_thread' (or all children) finishes
    //  static int Join(Thread *child_thread);
    static int JoinAll();

    // wait until at least one child finishes (returns immediately if no children)
    static int JoinAny();

    const char *GetName();

protected:
    static Thread*            s_pMainThread;
    static map<int, Thread *> s_ChildMap;

    list<Thread *>            m_Children;
    Thread*                   m_pParent;

    ThreadFuncArgs            m_ArgList;

    int                       m_TaskId;  // Tasker::ThreadID
    ModsEvent*                m_pSleepEvent;  // Tasker::EventID
    ModsEvent*                m_pJoinEvent;  // Tasker::EventID
    int CreateTaskerThread(MainFunc main, int argc, const char *argv[], int stack_size, const char *name);  // Tasker::ThreadID
    static void TaskerThread(void *arg);

private:
    static map<Thread::BlockSyncPoint*, list<Thread*>> s_RegisteredBlockingList;
    static map<Thread::BlockSyncPoint*, list<Thread*>> s_NotYetBlockedList;
};

#endif
