/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tasker_p.h
 * @brief  Private declarations for tasker.cpp and ostasker.cpp only.
 */

#ifndef INCLUDED_OSTASKER_H
#define INCLUDED_OSTASKER_H

#include "tasker.h"
#include <vector>
#include <list>

namespace Tasker
{
    class ModsThread;  // below in this file
}
using ModsThread = Tasker::ModsThread;
struct JSContext;   // From JS header files
class ModsThreadGroup;// in tasker.cpp

//------------------------------------------------------------------------------
//!
//! @class(TaskerPriv)   the private data inside the Tasker class.
//!
//! Functions with the "Os" prefix in their names are operating-system specific,
//! and are implemented in ostasker.cpp.
//! All other functions are portable and are in tasker.cpp.
//!
//------------------------------------------------------------------------------
class TaskerPriv
{
public:
   // Private data.

   vector<ModsThread *> m_Threads;        //!< Store all threads here.
   ModsThread *         m_pLwrrentThread; //!< The lwrrently chosen mods thread.
   list<ModsThread *>   m_Ready;          //!< Threads that are ready to run.
   list<ModsThread *>::iterator m_ReadyIt;   //!< current m_Ready iterator.

   TaskerPriv ();
   ~TaskerPriv ();

   //! Create Thread.  This is the portable part, lives in tasker.cpp.
   Tasker::ThreadID CreateThread
   (
      Tasker::ThreadFunc func,
      void *             args,
      UINT32             stackSize,
      const char *       name
   );

   //! Create thread, the non-portable part. Called by CreateThread.
   ModsThread * OsCreateThread
   (
      Tasker::ThreadID   id,
      Tasker::ThreadFunc func,
      void *             args,
      UINT32             stacksize,
      const char *       name
   );

   //! Clean up a dead thread, called by Tasker::Join.
   void OsCleanupDeadThread (ModsThread * pThread);

   //! The scheduler.
   //! Choose the next thread to run, switch to it.
   //! We block here if m_Ready is empty, until a WakeThread call oclwrs.
   void OsNextThread();

   //! Put this thread to sleep, remove it from m_Ready.
   //! Called by Tasker wait functions and elsewhere.
   void StupefyThread ();

   //! Wake this thread, put it on the m_Ready list as next to run.
   //! Called when event is set, or timeout, etc.
   void WakeThread (Tasker::ThreadID id, bool timedOut);

   //! Return number of bytes of stack space available in current thread.
   //! Not portable, lives in ostasker.cpp.
   int OsStackBytesFree () const;

   //! Destroy all JS contexts on all threads.
   void DestroyJsContexts();

   static const UINT32 TLS_NUM_INDEXES = 256;
};

//------------------------------------------------------------------------------
//!
//! @class(ModsThreadTimeout) a helper struct for ModsThread.
//!
//------------------------------------------------------------------------------
struct ModsThreadTimeout
{
    FLOAT64 TimeoutMs;  //!< The timeout value in ms for the timeout
    bool    TimedOut;   //!< True if the timeout has timed out
};

//------------------------------------------------------------------------------
//!
//! @class(ModsThread)  a helper struct for Tasker and OsTasker.
//!
//! Only portable parts of the thread structure live here.
//! Each OS will need to add its own extra data in a derived class
//! (i.e. DosModsThread or Win32ModsThread).
//!
//------------------------------------------------------------------------------
class Tasker::ModsThread
{
public:
   Tasker::ThreadID     Id;            //!< This threads identification number.
   Tasker::EventID      DeathEventId;  //!< EventID used by Tasker::Join.
   Tasker::ThreadFunc   Func;          //!< Starting address for thread.
   void *               Args;          //!< Argument for starting function.
   char *               Name;          //!< Name for use in Tasker::PrintAllThreads.
   vector<ModsThreadTimeout> TimeOuts; //!< Lwrrently active timeouts on the thread
   bool                 TimedOut;      //!< True if the thread has timed out
   bool                 Dead;          //!< True if thread is dead but not yet Joined.
   bool                 Awake;         //!< True if thread is !waiting and !dead.
   UINT32               Instance;      //!< Unique number, incremented for each thread created
   JSContext *          pJsContext;    //!< JSContext associated with the thread
   void *               TlsArray[TaskerPriv::TLS_NUM_INDEXES];  //!< TLS data for the thread
   ModsThreadGroup*     pGroup;        //!< Group to which the thread belongs
   UINT32               GThreadIdx;    //!< Thread index within the group

   //! Constructor.  Initialize the portable parts of a thread struct.
   ModsThread
   (
      Tasker::ThreadID   id,
      Tasker::ThreadFunc func,
      void *             args,
      const char *       name
   );
   //! Destructor
   ~ModsThread ();
};

struct ModsMutex
{
   // Which thread lwrrently owns this mutex
   // long is enough both for tasker ThreadID and pthread_t on 64-bit Linux
   long Owner;

   // How many times that thread has acquired this mutex (since a thread is
   // allowed to acquire the same mutex any number of times, as long as it
   // releases it the same number of times)
   INT32 LockCount;
};

struct ModsSemaphore
{
    INT32 Count;
    Tasker::EventID Sema4Event;
    char* Name;
    bool IsZombie;
    INT32 SleepingThreadCount;
};

struct Tasker::RmSpinLock
{
    volatile UINT32 lock;

    RmSpinLock() : lock(0) {};
};

struct ModsCondition::Impl
{
    Tasker::EventID m_Event;
    UINT32 m_WaitingCount = 0;
    bool m_IsZombie = false;
};

//------------------------------------------------------------------------------
//!
//! @class(ModsEvent)   a helper for Tasker.
//!
//! Maps one event to [n] waiting threads (where n is usually 0 or 1).
//! This class is portable, and is entirely in tasker.cpp.
//!
//------------------------------------------------------------------------------
class ModsEvent
{
private:
   // Per-object private data:
   vector<Tasker::ThreadID> m_Listeners;
   char *  m_Name;
   bool    m_IsSet;
   void *  m_OsEvent;
   int     m_Id;
   bool    m_ManualReset;
   UINT32  m_hOsEventClient;
   UINT32  m_hOsEventDevice;

   //! Private constructor.
   //! ModsEvent objects are only created/deleted by calls to Alloc and Free.
   ModsEvent(const char * Name, int Id, bool ManualReset);

public:
   ~ModsEvent();

   void Set();
   void Reset();
   bool IsSet();
   void *GetOsEvent(UINT32 hClient, UINT32 gpuInstance);
   bool AddListener   (Tasker::ThreadID thread);
   void RemoveListener(Tasker::ThreadID thread);

   static RC   Initialize ();
   static void Cleanup ();

   static ModsEvent *Alloc(const char * name, bool ManualReset = true);
   static void Free(ModsEvent *Event);

   static void ProcessPendingSets();
   static void OsProcessPendingSets();

   static void PrintEventsListenedFor(Tasker::ThreadID thread, INT32 pri);
   static void PrintAll(INT32 Priority);

private:

   //! Event table, null for unallocated events.  An EventId is an index into
   //! this table.
   static vector<ModsEvent *> s_Events;

   //! We keep two separate arrays of "pending set" bytes.
   //! The "current" array is pointed to by pPendingSetEvents.
   //!   Tasker::SetEvent writes into an array using pPendingSetEvents.
   //!   Tasker::ProcesPendingSetEvents changes pPendingSetEvents to swap
   //!   over to the spare array before completing the pending sets.
   //!
   //! This prevents race conditions between foreground and ISR code
   //! without having to use spinlocks or disabling interrupts.
   //!
   //! We keep one extra byte in the pending-set arrays that means "at least
   //! one pending-set is present".  This keeps ProcessPendingSetEvents from
   //! having to search the entire array when there is nothing for it to do.
   //! The extra byte is at offset -1 (pPendingSetEvents is pre-offset by +1).
   static volatile UINT08 * s_pPendingSets;
   static volatile UINT08 * s_pSpareSets;
   static size_t s_SetsSize;

   //! Allocate and free an OS event.
   void OsAllocEvent(UINT32 hClient, UINT32 hDevice);
   void OsFreeEvent(UINT32 hClient, UINT32 hDevice);
};

//------------------------------------------------------------------------------
//!
//! @class(ModsTimeout)  a helper for Tasker.
//!
//! Maps a remaining-ticks countdown alarm to a thread that should be woken.
//! Non-portable functions have the "Os" prefix, and are in ostasker.cpp.
//! Everything else is in tasker.cpp.
//!
//------------------------------------------------------------------------------
class ModsTimeout
{
public:
   static RC   Initialize();
   static void Cleanup();

   static void Add (Tasker::ThreadID id, UINT32 threadTimeoutIndex);
   static void Remove (Tasker::ThreadID id);

   static bool HasTimeout (Tasker::ThreadID id);
   static void PrintAll (INT32 Priority);

private:
   // per-object data.
   struct Timeout
   {
      UINT32             Counter; //!< Thread is woken up when counter == 0
      Tasker::ThreadID   Id;
      ModsThread        *pThread; //!< The thread waiting on this timeout
      UINT32             Index;   //!< Index for the timeout in the thread
                                  //!< timeout array
   };

   // class-static data.
   static list<Timeout>     s_Timeouts;
   static FLOAT64           s_TicksPerSec;
   static Tasker::ThreadID  s_TimerThreadId;
   static UINT32            s_OldTickCount;

   //! Private class-static method: the timer thread.
   static void TimerThread (void * pvTaskerPriv);

   static void ProcessTicks ();

   // Platform-specific functions: hook and unhook the isr.
   static RC   OsInitialize();
   static void OsCleanup();

public:
   // Public static static data that is used by the timer tick ISR.
   static FLOAT64         s_SafeTimeoutMs;
   static Tasker::EventID s_TickEvent;
   static volatile UINT32 s_IsrTickCount;
};

#endif // INCLUDED_OSTASKER_H
