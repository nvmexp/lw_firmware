/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008,2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    cnslmgr.h
//! @brief   Declare ConsoleManager -- Console Manager.

#ifndef INCLUDED_CNSLMGR_H
#define INCLUDED_CNSLMGR_H

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_SET
#include <set>
#define INCLUDED_STL_SET
#endif

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef INCLUDED_TASKER_H
#include "tasker.h"
#endif

#ifndef INCLUDED_OUTCWRAP_H
#include "outcwrap.h"
#endif

#ifndef INCLUDED_CNSLBUF_H
#include "cnslbuf.h"
#endif

class Console;

//!
//! @class(ConsoleManager)  Console Manager.
//!
//! This class arbitrates among multiple threads for access to the
//! current Console object.
//!
//! Multiple threads may have access to the current Console object
//! to do input or output, however when changing the current console
//! only the thread actively changing the console may have access.
//! I.e. it must wait for all threads that lwrrently have Console
//! access to release their access, as well as preventing any new
//! threads from acquiring access while waiting / changing the
//! console.
//!
//! During a console change, any thread which requests a console
//! (without requiring a real console) will retrieve a "null"
//! console
//!
//! The ConsoleManager is a singleton class.
//!
class ConsoleManager
{
public:
    ~ConsoleManager();

    static ConsoleManager *Instance();
    static void Shutdown();
    static ConsoleBuffer *GetConsoleBuffer() { return &m_ConsoleBuf; }

    RC Initialize();
    RC Cleanup();
    RC RegisterConsole(string name, Console *pConsole, bool bDefaultConsole);
    bool IsConsoleRegistered(string name) const;
    RC SetConsole(string name, bool bDefault);
    void SetConsoleLocked(bool bLocked);
    string GetDefaultConsole() { return m_DefaultConsole; }
    RC SetConsoleDisplayDevice(GpuDevice *pDevice);
    Console *GetNullConsole();
    Console *GetModsConsole();

    // Method to capture the state of a console, and temporarily disable it
    // (before GPU goes into standby)
    RC PrepareForSuspend(GpuDevice *pDevice);

    // Method to restore the state of a console (after GPU resumes)
    RC ResumeFromSuspend(GpuDevice *pDevice);

    //!
    //! @class(ConsoleContext)  Context for console usage.
    //!
    //! This class is used to arbitrate console usage among threads.  In order
    //! to use a console for either input or output, the following step should
    //! be taken.
    //!
    //! 1.  Create an instance of this class
    //! 2.  Depending upon requirements call either AcquireConsole
    //!     (non-blocking) to get either the real on null console or call
    //!     AcquireRealConsole (blocking) to ensure that the real console
    //!     is obtained
    //!
    //! The console is automatically released when the instance of this class
    //! is destroyed (or it can be manually released via a call to
    //! ReleaseConsole
    class ConsoleContext
    {
    public:
        ConsoleContext ();
        ~ConsoleContext ();

        Console * AcquireConsole(bool bInputConsole);
        Console * AcquireRealConsole(bool bInputConsole);
        Console * GetConsole() const { return m_pConsole; }
        Tasker::ThreadID GetThreadId()const { return m_ThreadId; }
        void ReleaseConsole();

    private:
        ConsoleManager * m_pConsoleMgr;       //!< Console Manager pointer
        Console        * m_pConsole;          //!< Pointer to the console in
                                              //!< by this context, 0 if no
                                              //!< console has been acquired
        bool             m_bHasConsoleLocked; //!< True if this context has the
                                              //!< console under exlusive lock
        bool             m_bInputConsole;     //!< True if this context
                                              //!< references the input console
        Tasker::ThreadID m_ThreadId;          //!< ThreadID that holds the
                                              //!< context
        OutputConsoleWrapper m_OutputWrapper; //!< Wrapper to return for non
                                              //!< input consoles

        RC AcquireRealConsoleExclusive();

        friend class ConsoleManager;
    };
private:
    // Private constructor since the constructor is only called by
    // ConsoleManager::Instance()
    ConsoleManager();

    static ConsoleManager *m_Instance;  //!< The instance of this singleton
    static ConsoleBuffer   m_ConsoleBuf;//!< Console buffer for all consoles
    void * m_Mutex;                     //!< Mutex for SetConsole operations.
    void * m_InputConsoleMutex;         //!< Mutex for acquiring a console for
                                        //!< input
    map<string, Console *> m_ConsoleMap;//!< Map of console name to instance
    string m_LwrrentConsole;            //!< Current console name
    string m_DefaultConsole;            //!< Default console name
    bool   m_bConsoleLocked;            //!< True to indicate that the console
                                        //!< cannot be changed with SetConsole
    bool   m_bInitialized;              //!< True if the Console Manager is
                                        //!< initialized
    set<ConsoleContext *> m_Contexts;   //!< List of console contexts that may
                                        //!< hold a reference to the current
                                        //!< console

    typedef set<ConsoleContext*>::iterator CtxIter;

    Console *LwrrentConsole();

    static bool PollNoConsoleRef(void *pvArgs);
    RC SetConsoleInternal(string name, bool bDefault);

    friend class ConsoleManager::ConsoleContext;
};

// Class used to create console objects via "new" so that the consoles are only
// destroyed when the console manager is and they are created at static
// construction time
class ConsoleFactory
{
public:
    typedef Console * (*ConsoleFactoryFunc)(void);
    ConsoleFactory(ConsoleFactoryFunc FactoryFunc, string name, bool bDefault);
    ~ConsoleFactory() {}
};

#endif // INCLUDED_CNSLMGR_H
