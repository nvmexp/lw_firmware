/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2011,2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    cnslmgr.cpp
//! @brief   Implement ConsoleManager

#include "core/include/cnslmgr.h"
#include "core/include/console.h"
#include "core/include/nullcnsl.h"
#include "core/include/stdoutsink.h"
#include "core/include/tasker.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/devmgr.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpudev.h"
#if defined(INCLUDE_GPU)
#include "gpu/include/gpumgr.h"
#endif
#include "core/include/utility.h"
#include <map>
#include <memory>
#include <string>

#include "core/include/modsdrv.h"

//! Singleton instance of the console manager
ConsoleManager *ConsoleManager::m_Instance = NULL;

//! Console buffer for all consoles
ConsoleBuffer ConsoleManager::m_ConsoleBuf;

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
ConsoleManager::ConsoleManager() :
    m_Mutex(NULL),
    m_InputConsoleMutex(NULL),
    m_LwrrentConsole(""),
    m_DefaultConsole(""),
    m_bConsoleLocked(false),
    m_bInitialized(false)
{
    RegisterConsole("null", new NullConsole, false);
}

//-----------------------------------------------------------------------------
//! \brief Destructor
//!
ConsoleManager::~ConsoleManager()
{
    if (m_Instance->m_bInitialized)
    {
        m_Instance->Cleanup();
    }
    map<string, Console *>::iterator consoleIter = m_ConsoleMap.begin();

    while (consoleIter != m_ConsoleMap.end())
    {
        delete consoleIter->second;
        consoleIter++;
    }
    m_ConsoleMap.clear();
    m_Contexts.clear();
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the singleton instance of the console manager. Create one
//!        if one does not exist
//!
/* static */ ConsoleManager *ConsoleManager::Instance()
{
    if (m_Instance == NULL)
    {
        m_Instance = new ConsoleManager();
    }
    return m_Instance;
}

//-----------------------------------------------------------------------------
//! \brief Shutdown the console manager by deleting the singleton instance
//!
/* static */ void ConsoleManager::Shutdown()
{
    if (m_Instance != NULL)
    {
        MASSERT(m_Instance->m_Contexts.empty() ||
                !"Console Manager shutdown called with open contexts\n");

        delete m_Instance;
        m_Instance = NULL;
    }
}

//-----------------------------------------------------------------------------
//! \brief Initialize the console manager
//!
//! \return OK if successful, not OK otherwise
RC ConsoleManager::Initialize()
{
    UINT32 numInputConsAcquired = 0;

    MASSERT(Tasker::IsInitialized());
    if (m_Mutex == NULL)
    {
        m_Mutex = Tasker::AllocMutex("ConsoleManager::m_Mutex",
                                     Tasker::mtxUnchecked);
    }
    if (m_InputConsoleMutex == NULL)
    {
        m_InputConsoleMutex = Tasker::AllocMutex(
            "ConsoleManager::m_InputConsoleMutex",
            Tasker::mtxUnchecked);
    }

    m_ConsoleBuf.Initialize();

    // Any contexts created before initialization must have been created
    // on the main thread so set their thread IDs appropriately
    for (CtxIter ppCtx = m_Contexts.begin();
          ppCtx != m_Contexts.end(); ppCtx++)
    {
        if ((*ppCtx)->m_ThreadId == Tasker::NULL_THREAD)
        {
            (*ppCtx)->m_ThreadId = Tasker::LwrrentThread();
            if ((*ppCtx)->m_bInputConsole)
            {
                // Since this should always occur in the main thread, only the
                // main thread should ever have acquired a context.  Ensure that
                // only one context has an input console at this point
                MASSERT(numInputConsAcquired == 0);
                Tasker::AcquireMutex(m_InputConsoleMutex);
                ++numInputConsAcquired;
            }
        }
    }

    m_ConsoleMap[m_LwrrentConsole]->Enable(&m_ConsoleBuf);
    m_bInitialized = true;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Cleanup the console manager
//!
//! \return OK if successful, not OK otherwise
RC ConsoleManager::Cleanup()
{
    m_ConsoleMap[m_LwrrentConsole]->Disable();
    if (m_Mutex != NULL)
    {
        Tasker::FreeMutex(m_Mutex);
        m_Mutex = NULL;
    }
    if (m_InputConsoleMutex != NULL)
    {
        Tasker::FreeMutex(m_InputConsoleMutex);
        m_InputConsoleMutex = NULL;
    }

    m_bInitialized = false;

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Register a console with the console manager
//!
//! \param name is the name to register the console under
//! \param pConsole is a pointer to the console to register
//! \param bDefaultConsole is set to true if the console should be the default
//!        console
//!
//! \return OK if successful, not OK otherwise
RC ConsoleManager::RegisterConsole
(
    string    name,
    Console * pConsole,
    bool      bDefaultConsole
)
{
    if (m_ConsoleMap.count(name) != 0)
    {
        Printf(Tee::PriAlways,
               "ConsoleManager::RegisterConsole : Console %s already "
               "registered!!\n",
               name.c_str());
        return RC::SOFTWARE_ERROR;
    }
    if (bDefaultConsole)
    {
        if (!m_LwrrentConsole.empty())
        {
            Printf(Tee::PriAlways,
                   "Overriding default console.  Old default=%s, "
                   "New default=%s\n",
                   m_LwrrentConsole.c_str(), name.c_str());
        }

        // To avoid losing Printfs during early setup, we enabled
        // StdoutSink until the ConsoleManager got a default console.
        //
        Tee::TeeInit();
        Tee::GetStdoutSink()->Disable();

        m_LwrrentConsole = name;
        m_DefaultConsole = name;
    }
    m_ConsoleMap[name] = pConsole;

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Return true if the specified console is registered
//!
//! \param name is the name of the console to check registration on
//!
//! \return true if the console is registered, false otherwise
bool ConsoleManager::IsConsoleRegistered(string name) const
{
    return (m_ConsoleMap.count(name) != 0);
}

//-----------------------------------------------------------------------------
//! \brief Set the console to the specified console.  This API will block until
//!        all contexts that have a console for use have released their non-
//!        locking hold on the real console.  It also blocks any new contexts
//!        from acquiring the real console.  Once this routine completes, the
//!        old console is disabled and the new console is enabled
//!
//! \param name is the name of the new console to set
//! \param bDefault is true if the console should be the new default console
//!
//! \return OK if successful, not OK otherwise
RC ConsoleManager::SetConsole(string name, bool bDefault)
{
    RC rc;

    MASSERT(m_bInitialized);

    if (m_ConsoleMap.count(name) == 0)
    {
        Printf(Tee::PriHigh,
               "ConsoleManager::SetConsole : Console %s does not exist!!\n",
               name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (bDefault && (name == "mods"))
    {
        Printf(Tee::PriHigh,
               "ConsoleManager::SetConsole : The MODS console may not be the "
               "default console!!\n");
        return RC::SOFTWARE_ERROR;
    }

    ConsoleContext consoleCtx;
    CHECK_RC(consoleCtx.AcquireRealConsoleExclusive());
    CHECK_RC(SetConsoleInternal(name, bDefault));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to set the current console (not thread safe)
//!
//! \return Pointer to the current console
RC ConsoleManager::SetConsoleInternal(string name, bool bDefault)
{
    RC rc;

    if ((m_LwrrentConsole == name) && m_ConsoleMap[name]->IsEnabled())
    {
        if (bDefault && (m_DefaultConsole != name))
        {
            m_DefaultConsole = name;
        }
        return OK;
    }

    if (m_bConsoleLocked)
    {
        Printf(Tee::PriHigh,
               "ConsoleManager::SetConsoleInternal : Cannot change console when"
               " locked!!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_LwrrentConsole != "")
    {
        CHECK_RC(m_ConsoleMap[m_LwrrentConsole]->Disable());
    }

    // If enabling the new console fails, then attempt to restore the old
    // console so the current console is valid
    rc = m_ConsoleMap[name]->Enable(&m_ConsoleBuf);
    if (rc != OK)
    {
        m_ConsoleMap[name]->Disable();
        if (OK != m_ConsoleMap[m_LwrrentConsole]->Enable(&m_ConsoleBuf))
        {
            Printf(Tee::PriHigh, "Unable to reenable %s console "
                    "after enabling %s console failed!\n",
                    m_LwrrentConsole.c_str(), name.c_str());
            MASSERT(!"Console restore failed!");
        }
    }
    else
    {
        m_LwrrentConsole = name;
        if (bDefault)
            m_DefaultConsole = name;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Set whether the console should be locked to the current console
//!
//! \param bLocked is a boolean indicating whether the console should be locked
//!
void ConsoleManager::SetConsoleLocked(bool bLocked)
{
    m_bConsoleLocked = bLocked;
}

//-----------------------------------------------------------------------------
//! \brief Set the display device for the console to the specified device.
//!        This API will block until all contexts that have a console for use
//!        have released their non-locking hold on the real console.  It also
//!        blocks any new contexts from acquiring the real console.  Once this
//!        routine completes, the device where the console is displayted is
//!        changed
//!
//! \param pDevice is pointer to the device on which to display the console
//!
//! \return OK if successful, not OK otherwise
RC ConsoleManager::SetConsoleDisplayDevice(GpuDevice *pDevice)
{
    RC rc;

    MASSERT(m_bInitialized);
    ConsoleContext consoleCtx;
    CHECK_RC(consoleCtx.AcquireRealConsoleExclusive());
    if (m_LwrrentConsole != "")
    {
        CHECK_RC(m_ConsoleMap[m_LwrrentConsole]->SetDisplayDevice(pDevice));
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Return a pointer to the null console
//!
//! \return a pointer to the null console
Console *ConsoleManager::GetNullConsole()
{
    MASSERT(m_ConsoleMap.count("null") != 0);
    return m_ConsoleMap["null"];
}

//-----------------------------------------------------------------------------
//! \brief Return a pointer to the mods console
//!
//! \return a pointer to the mods console
Console *ConsoleManager::GetModsConsole()
{
    if (m_ConsoleMap.count("mods") == 0)
        return NULL;

    return m_ConsoleMap["mods"];
}

//------------------------------------------------------------------------------
// Method to capture the state of console, and temporarily disable it
// (before GPU goes into standby)
RC ConsoleManager::PrepareForSuspend(GpuDevice *pDevice)
{
    RC rc;

    MASSERT(m_bInitialized);
    MASSERT(pDevice);

    ConsoleContext consoleCtx;
    CHECK_RC(consoleCtx.AcquireRealConsoleExclusive());

    if (pDevice != LwrrentConsole()->GetDisplayDevice())
        return OK;

    // Lock the console to current console
    SetConsoleLocked(true);

    // Suspend the current console
    CHECK_RC(LwrrentConsole()->Suspend());

    return rc;
}

//------------------------------------------------------------------------------
// Method to restore the state of console
// (after GPU resumes)
RC ConsoleManager::ResumeFromSuspend(GpuDevice *pDevice)
{
    RC rc;

    MASSERT(m_bInitialized);
    MASSERT(pDevice);

    ConsoleContext consoleCtx;
    CHECK_RC(consoleCtx.AcquireRealConsoleExclusive());

    if (pDevice != LwrrentConsole()->GetDisplayDevice())
        return OK;

    // Unlock console to re-enable the current console
    SetConsoleLocked(false);

    // Restore ModsConsole if enabled before suspend
    CHECK_RC(SetConsoleInternal(
                          m_LwrrentConsole,
                          m_LwrrentConsole == m_DefaultConsole));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function that returns a pointer to the current console
//!
//! \return Pointer to the current console
Console *ConsoleManager::LwrrentConsole()
{
    if (m_LwrrentConsole.empty())
        return m_ConsoleMap["null"];
    else
        return m_ConsoleMap[m_LwrrentConsole];
}

//-----------------------------------------------------------------------------
//! \brief Determine if no contexts other than the provided context have a
//!        reference to the real console
//!
//! \param pvArgs is a pointer to the context requesting exclusive access to
//!        the real console
//!
//! \return true if no other contexts have a reference to the real console,
//!         false otherwise
/* static */ bool ConsoleManager::PollNoConsoleRef(void *pvArgs)
{
    ConsoleContext *pCtx = static_cast<ConsoleContext *>(pvArgs);
    ConsoleManager *pMgr = pCtx->m_pConsoleMgr;

    for (CtxIter ppCtx = pMgr->m_Contexts.begin();
         ppCtx != pMgr->m_Contexts.end(); ppCtx++)
    {
        if ((*ppCtx != pCtx) &&
            ((*ppCtx)->GetConsole() != pMgr->GetNullConsole()))
        {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
ConsoleManager::ConsoleContext::ConsoleContext() :
    m_pConsoleMgr(ConsoleManager::Instance()),
    m_pConsole(NULL),
    m_bHasConsoleLocked(false),
    m_bInputConsole(false)
{
    MASSERT(m_pConsoleMgr);
    m_ThreadId = m_pConsoleMgr->m_bInitialized ? Tasker::LwrrentThread() :
                                                 Tasker::NULL_THREAD;
}

//-----------------------------------------------------------------------------
//! \brief Destructor
//!
ConsoleManager::ConsoleContext::~ConsoleContext()
{
    ReleaseConsole();
}

//-----------------------------------------------------------------------------
//! \brief Non-blocking call to acquire a reference to a console.  Either the
//!        real or null console may be acquired
//!
//! \return pointer to the acquired console
Console * ConsoleManager::ConsoleContext::AcquireConsole(bool bInputConsole)
{
    bool bRealConsole = false;
    unique_ptr<Utility::CleanupOnReturnArg<void, void, void*> > pMutexLock;

    MASSERT(!m_pConsoleMgr->m_ConsoleMap.empty());

    if ((m_pConsole == NULL) ||
        (m_pConsole == m_pConsoleMgr->m_ConsoleMap["null"]) ||
        (bInputConsole != m_bInputConsole))
    {
        if (!m_pConsoleMgr->m_bInitialized)
        {
            bRealConsole = true;
        }
        else if (Tasker::TryAcquireMutex(m_pConsoleMgr->m_Mutex))
        {
            pMutexLock.reset(new Utility::CleanupOnReturnArg<void, void, void*>
                    (Tasker::ReleaseMutex, m_pConsoleMgr->m_Mutex));

            if ((bInputConsole &&
                 Tasker::TryAcquireMutex(m_pConsoleMgr->m_InputConsoleMutex)) ||
                !bInputConsole)
            {
                // Leave the input mutex locked since this context now
                // owns the input
                bRealConsole = true;
            }
        }
    }

    if (bRealConsole)
    {
        if (bInputConsole)
        {
            m_pConsole = m_pConsoleMgr->LwrrentConsole();
        }
        else
        {
            m_OutputWrapper.SetWrappedConsole(m_pConsoleMgr->LwrrentConsole());
            m_pConsole = &m_OutputWrapper;
        }
        m_bInputConsole = bInputConsole;
        m_pConsoleMgr->m_Contexts.insert(this);
    }
    else
    {
        m_OutputWrapper.SetWrappedConsole(NULL);
        m_bInputConsole = false;
        m_pConsole = m_pConsoleMgr->m_ConsoleMap["null"];
    }

    return m_pConsole;
}

//-----------------------------------------------------------------------------
//! \brief Blocking call to acquire a reference to the real console.
//!
//! \return pointer to the real console
Console * ConsoleManager::ConsoleContext::AcquireRealConsole
(
    bool bInputConsole
)
{
    MASSERT(m_pConsoleMgr->m_bInitialized);
    MASSERT(!m_pConsoleMgr->m_ConsoleMap.empty());

    Tasker::MutexHolder mh(m_pConsoleMgr->m_Mutex);

    if (bInputConsole)
    {
        Tasker::AcquireMutex(m_pConsoleMgr->m_InputConsoleMutex);
        m_pConsole = m_pConsoleMgr->LwrrentConsole();
    }
    else
    {
        m_OutputWrapper.SetWrappedConsole(m_pConsoleMgr->LwrrentConsole());
        m_pConsole = &m_OutputWrapper;
    }
    m_bInputConsole = bInputConsole;
    m_pConsoleMgr->m_Contexts.insert(this);

    return m_pConsole;
}

//-----------------------------------------------------------------------------
//! \brief Release a reference to the current console.
//!
void ConsoleManager::ConsoleContext::ReleaseConsole()
{
    Tasker::MutexHolder mh;

    if ((m_pConsole != NULL) &&
        (m_pConsole != m_pConsoleMgr->m_ConsoleMap["null"]))
    {
        if (m_pConsoleMgr->m_bInitialized && !m_bHasConsoleLocked)
        {
            mh.Acquire(m_pConsoleMgr->m_Mutex);
        }

        m_pConsoleMgr->m_Contexts.erase(this);
    }
    m_pConsole = NULL;
    m_OutputWrapper.SetWrappedConsole(NULL);

    if (m_bInputConsole)
    {
        Tasker::ReleaseMutex(m_pConsoleMgr->m_InputConsoleMutex);
        m_bInputConsole = false;
    }

    if (m_bHasConsoleLocked)
    {
        Tasker::ReleaseMutex(m_pConsoleMgr->m_Mutex);
        m_bHasConsoleLocked = false;
    }
}

//-----------------------------------------------------------------------------
//! \brief Blocking call to acquire exclusive access to the real console.
//!
//! \return OK if successful, not OK otherwise
RC ConsoleManager::ConsoleContext::AcquireRealConsoleExclusive()
{
    MASSERT(m_pConsoleMgr->m_bInitialized);

    if (m_bHasConsoleLocked)
        return OK;

    Tasker::AcquireMutex(m_pConsoleMgr->m_Mutex);

    for (CtxIter ppCtx = m_pConsoleMgr->m_Contexts.begin();
         ppCtx != m_pConsoleMgr->m_Contexts.end(); ppCtx++)
    {
        if ((*ppCtx != this) &&
            ((*ppCtx)->GetThreadId() == Tasker::LwrrentThread()) &&
            ((*ppCtx)->GetConsole() != NULL) &&
            ((*ppCtx)->GetConsole() != m_pConsoleMgr->GetNullConsole()))
        {
            Printf(Tee::PriHigh,
              "\n***********************************************************\n"
              "* Attempt to acquire exclusive access to the real console *\n"
              "* when another context in the current thread holds a      *\n"
              "* reference.  Unable to acquire an exclusive lock!!!      *\n"
              "***********************************************************\n");
            Tasker::ReleaseMutex(m_pConsoleMgr->m_Mutex);
            return RC::SOFTWARE_ERROR;
        }
    }

    if ((m_pConsole == NULL) ||
        (m_pConsole == m_pConsoleMgr->m_ConsoleMap["null"]))
    {
        m_pConsole = m_pConsoleMgr->LwrrentConsole();
        m_bInputConsole = false;
        m_pConsoleMgr->m_Contexts.insert(this);
    }

    const RC rc = POLLWRAP(ConsoleManager::PollNoConsoleRef, this, 0);
    if (rc == RC::OK)
    {
        m_bHasConsoleLocked = true;
    }
    else
    {
        Tasker::ReleaseMutex(m_pConsoleMgr->m_Mutex);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief constructor
//!
ConsoleFactory::ConsoleFactory(ConsoleFactoryFunc FactoryFunc, string name, bool bDefault)
{
    ConsoleManager::Instance()->RegisterConsole(name, FactoryFunc(), bDefault);
}

//-----------------------------------------------------------------------------
//! \brief Javascript class for ConsoleManager
//!
JS_CLASS(ConsoleManager);

//-----------------------------------------------------------------------------
//! \brief Javascript object for ConsoleManager
//!
SObject ConsoleManager_Object
(
   "ConsoleManager",
   ConsoleManagerClass,
   0,
   0,
   "Console Manager Interface."
);

//-----------------------------------------------------------------------------
//! \brief Check Whether the specified console is registered
//!
//! \param ConsoleName : Console name to check for
//!
//! \return true if the console is registered, false otherwise
//!
JS_SMETHOD_LWSTOM(ConsoleManager, IsConsoleRegistered,
                  1, "Check whether a console is registered")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;

    // Check the arguments.
    string consoleName;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &consoleName)))
    {
       JS_ReportError(pContext,
                      "Usage: ConsoleManager.IsConsoleRegistered(name)");
       return JS_FALSE;
    }

    bool bReturn = ConsoleManager::Instance()->IsConsoleRegistered(consoleName);

    if (OK != pJs->ToJsval(bReturn, pReturlwalue))
    {
        JS_ReportError(pContext,
                     "Error oclwrred in ConsoleManager.IsConsoleRegistered()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

//-----------------------------------------------------------------------------
//! \brief Set the current console by name
//!
//! \param name     : Console name to set
//! \param bDefault : true if the console should be the new default console
//!
//! \return OK if the console was successfully set, not OK otherwise
//!
JS_STEST_LWSTOM(ConsoleManager, SetConsole,
                2, "Set the current console")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;

    // Check the arguments.
    string consoleName;
    bool   bDefault;
    if ((NumArguments != 2) ||
        (OK != pJs->FromJsval(pArguments[0], &consoleName)) ||
        (OK != pJs->FromJsval(pArguments[1], &bDefault)))
    {
       JS_ReportError(pContext, "Usage: ConsoleManager.SetConsole(name, bDefault)");
       return JS_FALSE;
    }

    RETURN_RC(ConsoleManager::Instance()->SetConsole(consoleName, bDefault));
}

#if defined(INCLUDE_GPU)
//-----------------------------------------------------------------------------
//! \brief Set the device to display the console on
//!
//! \param DeviceInst : DeviceInst to display the console on
//!
//! \return OK if the console display device was successfully set, not OK
//!         otherwise
//!
JS_STEST_LWSTOM(ConsoleManager, SetConsoleDisplayDevice,
                1, "Set the display device for the current console")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;

    // Check the arguments.
    UINT32 devInst;
    if ((NumArguments != 1) ||
        (OK != pJs->FromJsval(pArguments[0], &devInst)))
    {
       JS_ReportError(pContext,
                    "Usage: ConsoleManager.SetConsoleDisplayDevice(devInst)");
       return JS_FALSE;
    }

    Device *pDevice;
    RC rc;
    if (DevMgrMgr::d_GraphDevMgr != NULL)
    {
        C_CHECK_RC(DevMgrMgr::d_GraphDevMgr->GetDevice(devInst, &pDevice));
    }
    else
    {
        JS_ReportError(pContext,
                       "ConsoleManager.SetConsoleDisplayDevice : "
                       "d_GraphDevMgr not present");
        return JS_FALSE;
    }
    RETURN_RC(ConsoleManager::Instance()->SetConsoleDisplayDevice((GpuDevice *)pDevice));
}
#endif

//-----------------------------------------------------------------------------
//! \brief Property to return the current default console
//!
P_(ConsoleManager_Get_DefaultConsole);
P_(ConsoleManager_Get_DefaultConsole)
{
  string result = ConsoleManager::Instance()->GetDefaultConsole();

  if (OK != JavaScriptPtr()->ToJsval(result, pValue))
  {
     JS_ReportError(pContext, "Failed to get ConsoleManager.DefaultConsole");
     *pValue = JSVAL_NULL;
     return JS_FALSE;
  }
  return JS_TRUE;
}
static SProperty ConsoleManager_DefaultConsole
(
  ConsoleManager_Object,
  "DefaultConsole",
  0,
  0,
  ConsoleManager_Get_DefaultConsole,
  0,
  0,
  "Default console for the console manager"
);

