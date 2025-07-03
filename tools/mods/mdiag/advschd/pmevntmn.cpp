/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Implements PmEventManager which is in charge of registering
//! the events and triggering the actions.

#include "pmevntmn.h"
#include <algorithm>
#include "class/cl0000.h"
#include "class/cl0005.h"
#include "ctrl/ctrl2080.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "Lwcm.h"
#include "lwos.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "mdiag/utils/mdiagsurf.h"
#include "mdiag/sysspec.h"
#include "pmaction.h"
#include "pmchan.h"
#include "pmchwrap.h"
#include "pmevent.h"
#include "pmgilder.h"
#include "pmsurf.h"
#include "pmtrigg.h"
#include "gpu/perf/pmusub.h"
#include "gpu/utility/semawrap.h"
#include "core/include/utility.h"
#include <boost/functional/hash.hpp>
#include <sstream>

//--------------------------------------------------------------------
//! \brief constructor
//!
PmEventManager::PmEventManager(PolicyManager *pPolicyManager) :
    m_pPolicyManager(pPolicyManager),
    m_pMemorySubtask(nullptr),
    m_pSurfaceSubtask(nullptr),
    m_pTimerSubtask(nullptr),
    m_pErrorLoggerSubtask(nullptr)
{
    MASSERT(m_pPolicyManager != nullptr);
}

//--------------------------------------------------------------------
//! \brief destructor
//!
//! Deletes all triggers and actionBlocks.
//!
PmEventManager::~PmEventManager()
{
    // If any subtasks still exist, clean them up
    if (!m_Subtasks.empty())
    {
        EndTest();
    }

    m_String2SurfaceDesc.clear();
    m_String2VaSpaceDesc.clear();
    m_String2ChannelDesc.clear();
    m_String2GpuDesc.clear();
    m_String2RunlistDesc.clear();
    m_String2TestDesc.clear();
    m_String2ActionBlock.clear();
    m_TriggerActions.clear();
    m_String2SmcEngineDesc.clear();

    DeletePtrContainer(m_Triggers);
    DeletePtrContainer(m_ActionBlocks);
    DeletePtrContainer(m_SurfaceDescs);
    DeletePtrContainer(m_VaSpaceDescs);
    DeletePtrContainer(m_ChannelDescs);
    DeletePtrContainer(m_GpuDescs);
    DeletePtrContainer(m_RunlistDescs);
    DeletePtrContainer(m_TestDescs);
    DeletePtrContainer(m_SmcEngineDescs);
}

//--------------------------------------------------------------------
//! \brief Add a new VaSpaceDesc to the EventManager
//!
//! This method is called from PmParser to add a newly-created
//! VaSpaceDesc to the EventManager.  VaSpaceDesc names must be
//! unique, except for "" which is considered "unnamed".
//!
RC PmEventManager::AddVaSpaceDesc(PmVaSpaceDesc *pVaSpaceDesc)
{
    MASSERT(pVaSpaceDesc != NULL);
    const string &id = pVaSpaceDesc->GetId();
    RC rc;

    // Check for duplicates
    //
    if (id.empty() && m_String2VaSpaceDesc.count(id) > 0)
    {
        ErrPrintf("VaSpace \"%s\" defined twice!\n", id.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_VaSpaceDescs.count(pVaSpaceDesc) > 0)
    {
        ErrPrintf("VaSpaceDesc already exists!\n");
        return RC::BAD_PARAMETER;
    }

    // Add the vaspaceDesc
    //
    m_VaSpaceDescs.insert(pVaSpaceDesc);
    if (id != "")
    {
        m_String2VaSpaceDesc[id] = pVaSpaceDesc;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a new VfDesc to the EventManager
//!
//! This method is called from PmParser to add a newly-created
//! VfDesc to the EventManager.  VfDesc names must be
//! unique, except for "" which is considered "unnamed".
//!
RC PmEventManager::AddVfDesc(PmVfTestDesc *pVfDesc)
{
    MASSERT(pVfDesc != NULL);
    const string &id = pVfDesc->GetId();
    RC rc;

    // Check for duplicates
    //
    if (id.empty() && m_String2VfDesc.count(id) > 0)
    {
        ErrPrintf("Vf \"%s\" defined twice!\n", id.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_VfDescs.count(pVfDesc) > 0)
    {
        ErrPrintf("VfDesc already exists!\n");
        return RC::BAD_PARAMETER;
    }

    // Add the VfDesc
    //
    m_VfDescs.insert(pVfDesc);
    if (!id.empty())
    {
        m_String2VfDesc[id] = pVfDesc;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a new SmcEngineDesc to the EventManager
//!
//! This method is called from PmParser to add a newly-created
//! PmSmcEngineDesc to the EventManager.  PmSmcEngineDesc names must be
//! unique, except for "" which is considered "unnamed".
//!
RC PmEventManager::AddSmcEngineDesc(PmSmcEngineDesc *pSmcEngineDesc)
{
    MASSERT(pSmcEngineDesc != NULL);
    const string &id = pSmcEngineDesc->GetId();
    RC rc;
 
    // Check for duplicates
    if (id.empty() && m_String2SmcEngineDesc.count(id) > 0)
    {
        ErrPrintf("SmcEngine \"%s\" defined twice!\n", id.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_SmcEngineDescs.count(pSmcEngineDesc) > 0)
    {
        ErrPrintf("SmcEngineDesc already exists!\n");
        return RC::BAD_PARAMETER;
    }
 
    // Add the SmcEngineDesc
    m_SmcEngineDescs.insert(pSmcEngineDesc);
    if (!id.empty())
    {
        m_String2SmcEngineDesc[id] = pSmcEngineDesc;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a new SurfaceDesc to the EventManager
//!
//! This method is called from PmParser to add a newly-created
//! SurfaceDesc to the EventManager.  SurfaceDesc names must be
//! unique, except for "" which is considered "unnamed".
//!
RC PmEventManager::AddSurfaceDesc(PmSurfaceDesc *pSurfaceDesc)
{
    MASSERT(pSurfaceDesc != NULL);
    string id = pSurfaceDesc->GetId();
    RC rc;

    // Check for duplicates
    //
    if (id != "" && m_String2SurfaceDesc.count(id) > 0)
    {
        ErrPrintf("Surface \"%s\" defined twice!\n", id.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_SurfaceDescs.count(pSurfaceDesc) > 0)
    {
        ErrPrintf("SurfaceDesc already exists!\n");
        return RC::BAD_PARAMETER;
    }

    // Add the surfaceDesc
    //
    m_SurfaceDescs.insert(pSurfaceDesc);
    if (id != "")
        m_String2SurfaceDesc[id] = pSurfaceDesc;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Whether match the surface Name in surfaceDesc
//!
//! Return true  with the indicated name is used in policy file,
//! else return false.
//!
bool PmEventManager::IsMatchSurfaceDesc(const string &id) const
{
    for(set<PmSurfaceDesc *>::const_iterator ppPmSurfaceDesc = m_SurfaceDescs.begin();
        ppPmSurfaceDesc != m_SurfaceDescs.end(); ++ppPmSurfaceDesc)
    {
        const RegExp name = (*ppPmSurfaceDesc)->GetRegName();
        if (name.Matches(id))
        {
            return TRUE;
        }
    }

    return FALSE;
}

//--------------------------------------------------------------------
//! \brief Get a VfDesc by name
//!
//! Return the VfDesc with the indicated name, or NULL if there
//! is none.  This method cannot be be used for unnamed VfDescs.
//!
PmVfTestDesc *PmEventManager::GetVfDesc(const string &id) const
{
    map<string, PmVfTestDesc*>::const_iterator iter =
        m_String2VfDesc.find(id);
    return (iter == m_String2VfDesc.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Get a PmSmcEngineDesc by name
//!
//! Return the PmSmcEngineDesc with the indicated name, or NULL if there
//! is none.  This method cannot be be used for unnamed PmSmcEngineDescs.
//!
PmSmcEngineDesc *PmEventManager::GetSmcEngineDesc(const string &id) const
{
    map<string, PmSmcEngineDesc*>::const_iterator iter =
        m_String2SmcEngineDesc.find(id);
    return (iter == m_String2SmcEngineDesc.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Get a VaSpaceDesc by name
//!
//! Return the vaspaceDesc with the indicated name, or NULL if there
//! is none.  This method cannot be be used for unnamed vaspaceDescs.
//!
PmVaSpaceDesc *PmEventManager::GetVaSpaceDesc(const string &id) const
{
    map<string, PmVaSpaceDesc*>::const_iterator iter =
        m_String2VaSpaceDesc.find(id);
    return (iter == m_String2VaSpaceDesc.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Get a SurfaceDesc by name
//!
//! Return the surfaceDesc with the indicated name, or NULL if there
//! is none.  This method cannot be be used for unnamed surfaceDescs.
//!
PmSurfaceDesc *PmEventManager::GetSurfaceDesc(const string &id) const
{
    map<string, PmSurfaceDesc*>::const_iterator iter =
        m_String2SurfaceDesc.find(id);
    return (iter == m_String2SurfaceDesc.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Add a new ChannelDesc to the EventManager
//!
//! This method is called from PmParser to add a newly-created
//! ChannelDesc to the EventManager.  ChannelDesc names must be
//! unique, except for "" which is considered "unnamed".
//!
RC PmEventManager::AddChannelDesc(PmChannelDesc *pChannelDesc)
{
    MASSERT(pChannelDesc != NULL);
    string id = pChannelDesc->GetId();
    RC rc;

    // Check for duplicates
    //
    if (id != "" && m_String2ChannelDesc.count(id) > 0)
    {
        ErrPrintf("Channel \"%s\" defined twice!\n", id.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_ChannelDescs.count(pChannelDesc) > 0)
    {
        ErrPrintf("ChannelDesc already exists!\n");
        return RC::BAD_PARAMETER;
    }

    // Add the channelDesc
    //
    m_ChannelDescs.insert(pChannelDesc);
    if (id != "")
        m_String2ChannelDesc[id] = pChannelDesc;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Get a ChannelDesc by name
//!
//! Return the channelDesc with the indicated name, or NULL if there
//! is none.  This method cannot be be used for unnamed channelDescs.
//!
PmChannelDesc *PmEventManager::GetChannelDesc(const string &id) const
{
    map<string, PmChannelDesc*>::const_iterator iter =
        m_String2ChannelDesc.find(id);
    return (iter == m_String2ChannelDesc.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Add a new GpuDesc to the EventManager
//!
//! This method is called from PmParser to add a newly-created
//! PmGpuDesc to the EventManager.  GpuDesc names must be unique,
//! except for "" which is considered "unnamed".
//!
RC PmEventManager::AddGpuDesc(PmGpuDesc *pGpuDesc)
{
    MASSERT(pGpuDesc != NULL);
    string id = pGpuDesc->GetId();
    RC rc;

    // Check for duplicates
    //
    if (id != "" && m_String2GpuDesc.count(id) > 0)
    {
        ErrPrintf("Gpu \"%s\" defined twice!\n", id.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_GpuDescs.count(pGpuDesc) > 0)
    {
        ErrPrintf("GpuDesc already exists!\n");
        return RC::BAD_PARAMETER;
    }

    // Add the gpuDesc
    //
    m_GpuDescs.insert(pGpuDesc);
    if (id != "")
        m_String2GpuDesc[id] = pGpuDesc;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Get a GpuDesc by name
//!
//! Return the gpuDesc with the indicated name, or NULL if there
//! is none.  This method cannot be be used for unnamed gpuDescs.
//!
PmGpuDesc *PmEventManager::GetGpuDesc(const string &id) const
{
    map<string, PmGpuDesc*>::const_iterator iter =
        m_String2GpuDesc.find(id);
    return (iter == m_String2GpuDesc.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Add a new RunlistDesc to the EventManager
//!
//! This method is called from PmParser to add a newly-created
//! RunlistDesc to the EventManager.  RunlistDesc names must be
//! unique, except for "" which is considered "unnamed".
//!
RC PmEventManager::AddRunlistDesc(PmRunlistDesc *pRunlistDesc)
{
    MASSERT(pRunlistDesc != NULL);
    string id = pRunlistDesc->GetId();
    RC rc;

    // Check for duplicates
    //
    if (id != "" && m_String2RunlistDesc.count(id) > 0)
    {
        ErrPrintf("Runlist \"%s\" defined twice!\n", id.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_RunlistDescs.count(pRunlistDesc) > 0)
    {
        ErrPrintf("RunlistDesc already exists!\n");
        return RC::BAD_PARAMETER;
    }

    // Add the runlistDesc
    //
    m_RunlistDescs.insert(pRunlistDesc);
    if (id != "")
        m_String2RunlistDesc[id] = pRunlistDesc;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Get a RunlistDesc by name
//!
//! Return the runlistDesc with the indicated name, or NULL if there
//! is none.  This method cannot be be used for unnamed runlistDescs.
//!
PmRunlistDesc *PmEventManager::GetRunlistDesc(const string &id) const
{
    map<string, PmRunlistDesc*>::const_iterator iter =
        m_String2RunlistDesc.find(id);
    return (iter == m_String2RunlistDesc.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Add a new TestDesc to the EventManager
//!
//! This method is called from PmParser to add a newly-created
//! TestDesc to the EventManager.  TestDesc names must be unique,
//! except for "" which is considered "unnamed".
//!
RC PmEventManager::AddTestDesc(PmTestDesc *pTestDesc)
{
    MASSERT(pTestDesc != NULL);
    string id = pTestDesc->GetId();
    RC rc;

    // Check for duplicates
    //
    if (id != "" && m_String2TestDesc.count(id) > 0)
    {
        ErrPrintf("Test \"%s\" defined twice!\n", id.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_TestDescs.count(pTestDesc) > 0)
    {
        ErrPrintf("TestDesc already exists!\n");
        return RC::BAD_PARAMETER;
    }

    // Add the testDesc
    //
    m_TestDescs.insert(pTestDesc);
    if (id != "")
        m_String2TestDesc[id] = pTestDesc;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Get a TestDesc by name
//!
//! Return the testDesc with the indicated name, or NULL if there is
//! none.  This method cannot be be used for unnamed testDescs.
//!
PmTestDesc *PmEventManager::GetTestDesc(const string &id) const
{
    map<string, PmTestDesc*>::const_iterator iter = m_String2TestDesc.find(id);
    return (iter == m_String2TestDesc.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Add a new ActionBlock to the EventManager
//!
//! This method is called from the policyfile parser (PmParser) to add
//! a newly-created ActionBlock to the EventManager.  It is an error
//! to add the same ActionBlock twice, or to have two ActionBlocks
//! with the same name.  Exception: An ActionBlock named "" is
//! considered "unnamed", and it's OK to have many unnamed
//! ActionBlocks.
//!
RC PmEventManager::AddActionBlock(PmActionBlock *pActionBlock)
{
    MASSERT(pActionBlock != NULL);
    string name = pActionBlock->GetName();
    RC rc;

    // Make sure we don't have duplicate actionBlocks
    //
    if (name != "" && m_String2ActionBlock.count(name) > 0)
    {
        ErrPrintf("An ActionBlock named \"%s\" already exists!\n",
               name.c_str());
        return RC::BAD_PARAMETER;
    }
    else if (m_ActionBlocks.count(pActionBlock) > 0)
    {
        ErrPrintf("ActionBlock already exists!\n");
        return RC::BAD_PARAMETER;
    }

    // Add the actionBlock
    //
    m_ActionBlocks.insert(pActionBlock);
    if (name != "")
        m_String2ActionBlock[name] = pActionBlock;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Get an ActionBlock by name
//!
//! Return the actionBlock with the indicated name, or NULL if there
//! is none.  This method cannot be be used for unnamed actionBlocks.
//!
PmActionBlock *PmEventManager::GetActionBlock(const string &name) const
{
    map<string, PmActionBlock*>::const_iterator iter =
        m_String2ActionBlock.find(name);
    return (iter == m_String2ActionBlock.end()) ? NULL : iter->second;
}

//--------------------------------------------------------------------
//! \brief Add a new Trigger to the EventManager
//!
//! This method is called from the policyfile parser (PmParser) to add
//! a newly-created Trigger to the EventManager.  It is an error to
//! add the same Trigger twice.
//!
//! \param pTrigger The trigger to add to the EventManager
//! \param pActionBlock The actionBlock that should be exelwted when
//!     the trigger fires
//!
RC PmEventManager::AddTrigger(PmTrigger *pTrigger, PmActionBlock *pActionBlock)
{
    MASSERT(pTrigger != NULL);
    MASSERT(pActionBlock != NULL);
    RC rc;

    if (find(m_Triggers.begin(), m_Triggers.end(), pTrigger) !=
             m_Triggers.end())
    {
        ErrPrintf("Trigger already exists!\n");
        return RC::BAD_PARAMETER;
    }
    else if (m_ActionBlocks.count(pActionBlock) == 0)
    {
        if (pActionBlock->GetName() != "")
            ErrPrintf("ActionBlock \"%s\" does not exist!\n",
                   pActionBlock->GetName().c_str());
        else
            ErrPrintf("ActionBlock does not exist!\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(pTrigger->IsSupported());
    CHECK_RC(pActionBlock->IsSupported(pTrigger));

    pTrigger->SetEventManager(this);
    m_Triggers.push_back(pTrigger);
    m_TriggerActions[pTrigger] = pActionBlock;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a new Trigger to the EventManager
//!
//! This is a thin wrapper around AddTrigger(PmTrigger*,
//! PmActionBlock*).  It's called by the parser for any newly-created
//! trigger that exelwtes a single PmAction, rather than a full
//! PmActionBlock.
//!
//! Internally, this method simply wraps the PmAction in an unnamed
//! PmActionBlock.
//!
//! \param pTrigger The trigger to add to the EventManager
//! \param pAction The action that should be exelwted when
//!     the trigger fires
//!
RC PmEventManager::AddTrigger(PmTrigger *pTrigger, PmAction *pAction)
{
    RC rc;
    PmActionBlock *pActionBlock = new PmActionBlock(this, "");
    CHECK_RC(pActionBlock->AddAction(pAction));
    CHECK_RC(AddActionBlock(pActionBlock));
    CHECK_RC(AddTrigger(pTrigger, pActionBlock));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Handle an event
//!
//! This method is, arguably, the heart of the entire PolicyManager
//! module.  It takes an event that just oclwrred, finds all triggers
//! that should be fired by the event, and exelwtes the corresponding
//! actionBlocks.
//!
//! Note: If an event matches more than one trigger, then the triggers
//! get fired in whatever order they appeared in the policyfile.
//! That's why m_Triggers is a vector<>, rather than a set<>.
//!
RC PmEventManager::HandleEvent(const PmEvent *pEvent)
{
    MASSERT(pEvent != NULL);
    RC rc;

    // Do a preliminary check with PmTrigger::CouldMatch() to see if
    // we can ignore this event.  This check must be done before the
    // mutex is locked, to avoid some deadlocks.  See
    // PmTrigger::CouldMatch() for details.
    //
    bool SomeTriggerCouldMatch = false;
    for (vector<PmTrigger*>::iterator ppTrigger = m_Triggers.begin();
         ppTrigger != m_Triggers.end(); ++ppTrigger)
    {
        if ((*ppTrigger)->CouldMatch(pEvent))
        {
            SomeTriggerCouldMatch = true;
            break;
        }
    }
    if (!SomeTriggerCouldMatch)
    {
        // For a running test(a test which called StartTest but has not
        // called either EndTest or Action.AbortTest), the gilder needs
        // to know about events even if they are not going to cause
        // a trigger (i.e. PMU events) so that it can gild them
        if (m_pPolicyManager->InTest(pEvent->GetTest()))
        {
            pEvent->NotifyGilder(m_pPolicyManager->GetGilder());
        }
        else
        {
            Printf(Tee::PriDebug, "%s stopped notifying pmgilder because "
                "the test is not running!\n", __FUNCTION__);
        }

        return rc;
    }

    // Lock the mutex
    //
    Tasker::MutexHolder mh(m_pPolicyManager->GetMutex());

    // Abort if we've already called EndTest or Action.AbortTest
    //
    if (!m_pPolicyManager->InUnabortedTest(pEvent->GetTest()))
    {
        Printf(Tee::PriDebug, "%s stopped triggering action because "
            "the test is aborting or not running!\n", __FUNCTION__);
        return rc;
    }

    // Update any objects that need it before we handle the event
    //
    CHECK_RC(m_pPolicyManager->OnEvent());

    // Call any PmGilder::EventOclwrred() and/or
    // PmGilder::EndPotentialEvent() methods that should be called due
    // to this event.
    //
    pEvent->NotifyGilder(m_pPolicyManager->GetGilder());

    // Find all triggers that match this event, and execute the
    // corresponding actionBlock.
    //
    for (vector<PmTrigger*>::iterator ppTrigger = m_Triggers.begin();
         ppTrigger != m_Triggers.end(); ++ppTrigger)
    {
        if ((*ppTrigger)->DoMatch(pEvent))
        {
            PmActionBlock *pActionBlock = m_TriggerActions[*ppTrigger];
            CHECK_RC(pActionBlock->Execute(*ppTrigger, pEvent));
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Do any processing that should occur at the start of the test
//!
//! This method is called by PolicyManager::StartTest() when the test
//! starts.  It enables scheduled events on each GPU device under
//! test, and starts the subtask(s) that handles RM events.
//!
RC PmEventManager::StartTest()
{
    vector<GpuSubdevice*> gpuSubdevices = m_pPolicyManager->GetGpuSubdevices();
    RC rc;

    MASSERT(m_Subtasks.empty());

    for (vector<PmTrigger*>::iterator ppTrigger = m_Triggers.begin();
         ppTrigger != m_Triggers.end(); ++ppTrigger)
    {
        CHECK_RC((*ppTrigger)->StartTest());
    }
    // Start the PMU subtask on each GPU subdevice.
    //
    for (vector<GpuSubdevice*>::iterator ppSubdev = gpuSubdevices.begin();
         ppSubdev != gpuSubdevices.end(); ++ppSubdev)
    {
        // Start the PMU subtask
        //
        PMU *pPmu;
        if ((*ppSubdev)->GetPmu(&pPmu) == OK)
        {
            PmuSubtask *pPmuSubtask = new PmuSubtask(this, *ppSubdev, pPmu);
            CHECK_RC(pPmuSubtask->Start());
            m_Subtasks.insert(pPmuSubtask);
        }
        else
        {
            Printf(Tee::PriDebug,
                   "Warning: Cannot receive PMU events\n");
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Do any processing that should occur at the end of the test
//!
//! This method is called by PolicyManager::EndTest() when the test
//! ends.  It kills the subtask(s) created by StartTest(), and
//! disables the scheduled events that were enabled by StartTest().
//!
//! \return If any subtask got an error, return the error code.
//!
RC PmEventManager::EndTest()
{
    StickyRC firstRc = m_SubtaskRc;

    // Kill all the subtasks
    //
    for (set<Subtask*>::iterator ppSubtask = m_Subtasks.begin();
         ppSubtask != m_Subtasks.end(); ++ppSubtask)
    {
        firstRc = (*ppSubtask)->Stop();
    }

    m_RmEventSubtasks.clear();
    m_pMemorySubtask = nullptr;
    m_pSurfaceSubtask = nullptr;
    m_pTimerSubtask = nullptr;
    m_pErrorLoggerSubtask = nullptr;
    DeletePtrContainer(m_Subtasks);

    // Call EndTest() on all the ActionBlocks
    //
    for (set<PmActionBlock*>::iterator ppActionBlock = m_ActionBlocks.begin();
         ppActionBlock != m_ActionBlocks.end(); ++ppActionBlock)
    {
        firstRc = (*ppActionBlock)->EndTest();
    }

    m_MethodIdHooks.clear();

    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Add a hook that gets called when an RM event oclwrs.
//!
RC PmEventManager::HookResmanEvent
(
    LwRm::Handle hParent,
    UINT32 Index,
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size,
    LwRm* pLwRm
)
{
    RC rc;
    Tasker::MutexHolder mh(m_pPolicyManager->GetMutex());

    // Abort if we've already called EndTest
    //
    if (!m_pPolicyManager->InAnyTest())
    {
        return rc;
    }

    // Find existing RmEvent subtask that's monitoring this event, if
    // any.
    //
    RmEventSubtask *pSubtask = NULL;
    for (set<RmEventSubtask*>::iterator ppSubtask = m_RmEventSubtasks.begin();
         ppSubtask != m_RmEventSubtasks.end(); ++ppSubtask)
    {
        if ((*ppSubtask)->GetParent() == hParent &&
            (*ppSubtask)->GetIndex() == Index)
        {
            pSubtask = *ppSubtask;
            break;
        }
    }

    // If no RmEvent subtask is monitoring this event, then create a
    // new one.
    //
    if (pSubtask == NULL)
    {
        pSubtask = new RmEventSubtask(this, hParent, Index, pLwRm);
        m_Subtasks.insert(pSubtask);
        m_RmEventSubtasks.insert(pSubtask);
        CHECK_RC(pSubtask->Start());
    }

    // Add the new handler function to the subtask
    //
    pSubtask->AddHook(RmEventSubtask::Hook(pFunc, pData, Size));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a hook that gets called when an RM event oclwrs on a subdevice
//!
RC PmEventManager::HookResmanEvent
(
    GpuSubdevice *pSubdevice,
    UINT32 Index,
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size,
    LwRm* pLwRm
)
{
    RC rc;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(pSubdevice);
    Tasker::MutexHolder mh(m_pPolicyManager->GetMutex());

    // Abort if we've already called EndTest
    //
    if (!m_pPolicyManager->InAnyTest())
    {
        return rc;
    }

    // Find existing RmEvent subtask that's monitoring this event, if
    // any.
    //
    RmEventSubtask *pSubtask = NULL;
    for (set<RmEventSubtask*>::iterator ppSubtask = m_RmEventSubtasks.begin();
         ppSubtask != m_RmEventSubtasks.end(); ++ppSubtask)
    {
        if ((*ppSubtask)->GetParent() == hSubdevice &&
            (*ppSubtask)->GetIndex() == Index)
        {
            pSubtask = *ppSubtask;
            break;
        }
    }

    // If no RmEvent subtask is monitoring this event, then create a
    // new one.
    //
    if (pSubtask == NULL)
    {
        pSubtask = new SubdeviceSubtask(this, hSubdevice, Index, pLwRm);
        m_Subtasks.insert(pSubtask);
        m_RmEventSubtasks.insert(pSubtask);
        CHECK_RC(pSubtask->Start());
    }

    // Add the new handler function to the subtask
    //
    pSubtask->AddHook(RmEventSubtask::Hook(pFunc, pData, Size));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Undoes a HookResmanEvent
//!
RC PmEventManager::UnhookResmanEvent
(
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
)
{
    RC rc;
    RmEventSubtask::Hook oldHook(pFunc, pData, Size);

    for (set<RmEventSubtask*>::iterator ppSubtask = m_RmEventSubtasks.begin();
         ppSubtask != m_RmEventSubtasks.end(); ++ppSubtask)
    {
        (*ppSubtask)->DelHook(oldHook);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a hook that gets called when a memory address changes
//!
RC PmEventManager::HookMemoryEvent
(
    UINT32 *pMemAddress,
    UINT32 memValue,
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
)
{
    RC rc;
    Tasker::MutexHolder mh(m_pPolicyManager->GetMutex());

    // Abort if we've already called EndTest
    //
    if (!m_pPolicyManager->InAnyTest())
    {
        return rc;
    }

    // If there isn't a memory subtask, then create one.
    //
    if (m_pMemorySubtask == NULL)
    {
        m_pMemorySubtask = new MemorySubtask(this);
        m_Subtasks.insert(m_pMemorySubtask);
    }

    // Insert the new hook into the memory subtask
    //
    MemorySubtask::Hook newHook(pMemAddress, memValue, pFunc, pData, Size);
    m_pMemorySubtask->AddHook(newHook);

    // Make sure the memory subtask is running.  It tends to shut down
    // when there are no active hooks.
    //
    CHECK_RC(m_pMemorySubtask->Start());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Undoes a HookMemoryEvent
//!
RC PmEventManager::UnhookMemoryEvent
(
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
)
{
    RC rc;

    // Do nothing if there is no memory subtask; there are no memory
    // hooks to delete
    //
    if (m_pMemorySubtask == NULL)
    {
        return rc;
    }

    // Get a list of hooks to delete.
    //
    const set<MemorySubtask::Hook> &hooks = m_pMemorySubtask->GetHooks();
    vector<MemorySubtask::Hook> hooksToDelete;

    for (set<MemorySubtask::Hook>::const_iterator pHook = hooks.begin();
         pHook != hooks.end(); ++pHook)
    {
        if (pHook->m_pFunc == pFunc &&
            pHook->m_Data.size() == Size &&
            equal((UINT08*)pData, (UINT08*)pData+Size, pHook->m_Data.begin()))
        {
            hooksToDelete.push_back(*pHook);
        }
    }

    // Delete the hooks that we found above.  (We didn't do the actual
    // deletion in the above loop, because that would've screwed up
    // the loop iterator.)
    //
    for (vector<MemorySubtask::Hook>::iterator pHook = hooksToDelete.begin();
         pHook != hooksToDelete.end(); ++pHook)
    {
        m_pMemorySubtask->DelHook(*pHook);
    }

    // Stop the subtask if there are no more hooks.
    //
    if (m_pMemorySubtask->GetHooks().empty())
    {
        m_pMemorySubtask->Stop();
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a hook that gets called when data on a surface changes
//!
RC PmEventManager::HookSurfaceEvent
(
    PmPageDesc *pPageDesc,
    const vector<UINT08> &SurfaceData,
    RC (*pFunc)(void*, const PmMemRanges &,
                PmPageDesc*, vector<UINT08>*),
    void *pData,
    UINT32 Size
)
{
    RC rc;
    Tasker::MutexHolder mh(m_pPolicyManager->GetMutex());

    // Abort if we've already called EndTest
    //
    if (!m_pPolicyManager->InAnyTest())
    {
        return rc;
    }

    // If there isn't a surface subtask, then create one.
    //
    if (m_pSurfaceSubtask == NULL)
    {
        m_pSurfaceSubtask = new SurfaceSubtask(this);
        m_Subtasks.insert(m_pSurfaceSubtask);
    }

    // Insert the new hook into the surface subtask
    //
    SurfaceSubtask::Hook newHook(pPageDesc, SurfaceData, pFunc, pData, Size);
    m_pSurfaceSubtask->AddHook(newHook);

    // Make sure the surface subtask is running.  It tends to shut down
    // when there are no active hooks.
    //
    CHECK_RC(m_pSurfaceSubtask->Start());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Undoes a HookSurfaceEvent
//!
RC PmEventManager::UnhookSurfaceEvent
(
    RC (*pFunc)(void*, const PmMemRanges &,
                PmPageDesc*, vector<UINT08>*),
    void *pData,
    UINT32 Size
)
{
    RC rc;

    // Do nothing if there is no surface subtask; there are no surface
    // hooks to delete
    //
    if (m_pSurfaceSubtask == NULL)
    {
        return rc;
    }

    // Get a list of hooks to delete.
    //
    const set<SurfaceSubtask::Hook> &hooks = m_pSurfaceSubtask->GetHooks();
    vector<SurfaceSubtask::Hook> hooksToDelete;

    for (set<SurfaceSubtask::Hook>::const_iterator pHook = hooks.begin();
         pHook != hooks.end(); ++pHook)
    {
        if (pHook->m_pFunc == pFunc &&
            pHook->m_Data.size() == Size &&
            equal((UINT08*)pData, (UINT08*)pData+Size, pHook->m_Data.begin()))
        {
            hooksToDelete.push_back(*pHook);
        }
    }

    // Delete the hooks that we found above.  (We didn't do the actual
    // deletion in the above loop, because that would've screwed up
    // the loop iterator.)
    //
    for (vector<SurfaceSubtask::Hook>::iterator pHook = hooksToDelete.begin();
         pHook != hooksToDelete.end(); ++pHook)
    {
        m_pSurfaceSubtask->DelHook(*pHook);
    }

    // Stop the subtask if there are no more hooks.
    //
    if (m_pSurfaceSubtask->GetHooks().empty())
    {
        m_pSurfaceSubtask->Stop();
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a hook that gets called at a specified time
//!
//! This hook causes the specfied function to be called when the time
//! reaches TimeUS, as measured by Platform::GetTimeUS().
//!
RC PmEventManager::HookTimerEvent
(
    UINT64 TimeUS,
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
)
{
    RC rc;
    Tasker::MutexHolder mh(m_pPolicyManager->GetMutex());

    // Abort if we've already called EndTest
    //
    if (!m_pPolicyManager->InAnyTest())
    {
        return rc;
    }

    // If there isn't a timer subtask, then create one.
    //
    if (m_pTimerSubtask == NULL)
    {
        m_pTimerSubtask = new TimerSubtask(this);
        m_Subtasks.insert(m_pTimerSubtask);
    }

    // Insert the new hook into the timer subtask, and make sure the
    // subtask is running.  (It tends to shut down when there are no
    // active hooks.)
    //
    TimerSubtask::Hook newHook(TimeUS, pFunc, pData, Size);
    m_pTimerSubtask->AddHook(newHook);

    // Make sure the timer subtask is running.
    //
    CHECK_RC(m_pTimerSubtask->Start());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Undoes a HookTimerEvent
//!
RC PmEventManager::UnhookTimerEvent
(
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
)
{
    RC rc;

    // Do nothing if there is no timer subtask; there are no timer
    // hooks to delete
    //
    if (m_pTimerSubtask == NULL)
    {
        return rc;
    }

    // Get a list of hooks to delete.
    //
    const vector<TimerSubtask::Hook> &hooks = m_pTimerSubtask->GetHooks();
    vector<TimerSubtask::Hook> hooksToDelete;

    for (vector<TimerSubtask::Hook>::const_iterator pHook = hooks.begin();
         pHook != hooks.end(); ++pHook)
    {
        if (pHook->m_pFunc == pFunc &&
            pHook->m_Data.size() == Size &&
            equal((UINT08*)pData, (UINT08*)pData+Size, pHook->m_Data.begin()))
        {
            hooksToDelete.push_back(*pHook);
        }
    }

    // Delete the hooks that we found above.  (We didn't do the actual
    // deletion in the above loop, because that would've screwed up
    // the loop iterator.)
    //
    for (vector<TimerSubtask::Hook>::iterator pHook = hooksToDelete.begin();
         pHook != hooksToDelete.end(); ++pHook)
    {
        m_pTimerSubtask->DelHook(*pHook);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a hook that gets called on an error logger event
//!
RC PmEventManager::HookErrorLoggerEvent
(
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
)
{
    RC rc;
    Tasker::MutexHolder mh(m_pPolicyManager->GetMutex());

    // Abort if we've already called EndTest
    //
    if (!m_pPolicyManager->InAnyTest())
    {
        return rc;
    }

    // If there isn't a error logger subtask, then create one.
    //
    if (m_pErrorLoggerSubtask == nullptr)
    {
        m_pErrorLoggerSubtask = new ErrorLoggerSubtask(this);
        m_Subtasks.insert(m_pErrorLoggerSubtask);
    }

    // Insert the new hook into the error logger subtask, and make sure the
    // subtask is running.  (It tends to shut down when there are no
    // active hooks.)
    //
    ErrorLoggerSubtask::Hook newHook(pFunc, pData, Size);
    m_pErrorLoggerSubtask->AddHook(newHook);

    // Make sure the error logger subtask is running.
    //
    CHECK_RC(m_pErrorLoggerSubtask->Start());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Undoes a HookErrorLoggerEvent
//!
RC PmEventManager::UnhookErrorLoggerEvent
(
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
)
{
    RC rc;

    // Do nothing if there is no error logger subtask; there are no error logger
    // hooks to delete
    //
    if (m_pErrorLoggerSubtask == nullptr)
    {
        return rc;
    }

    // Get a list of hooks to delete.
    //
    const set<ErrorLoggerSubtask::Hook> &hooks = m_pErrorLoggerSubtask->GetHooks();
    vector<ErrorLoggerSubtask::Hook> hooksToDelete;

    for (set<ErrorLoggerSubtask::Hook>::const_iterator pHook = hooks.begin();
         pHook != hooks.end(); ++pHook)
    {
        if (pHook->m_pFunc == pFunc &&
            pHook->m_Data.size() == Size &&
            equal((UINT08*)pData, (UINT08*)pData+Size, pHook->m_Data.begin()))
        {
            hooksToDelete.push_back(*pHook);
        }
    }

    // Delete the hooks that we found above.  (We didn't do the actual
    // deletion in the above loop, because that would've screwed up
    // the loop iterator.)
    //
    for (vector<ErrorLoggerSubtask::Hook>::iterator pHook = hooksToDelete.begin();
         pHook != hooksToDelete.end(); ++pHook)
    {
        m_pErrorLoggerSubtask->DelHook(*pHook);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Generate a PmEvent_MethodIdWrite event whenever the
//! indicated class/method is written.
//!
//! This hook tells PmEventManager that we want to generate a
//! PmEvent_MethodIdWrite event whenever the indicated class/method is
//! written to the indicated channel(s).
//!
//! PmEventManager does not generate the actual event; it merely
//! records the information so that NeedMethodIdEvent() returns true
//! when an event need to be generated.  It is up to PmChannelWrapper
//! to poll NeedMethodIdEvent() as methods are written, and generate
//! the event accordingly.
//!
//! \param pChannelDesc The channels to trigger on
//! \param ClassId Send the event when this class is written to
//! \param Method Send the event when this method is written to the class
//! \param AfterWrite If false, send the event before the write.  If
//!     true, send it after.
//!
void PmEventManager::HookMethodIdEvent
(
    const PmChannelDesc *pChannelDesc,
    UINT32 ClassId,
    UINT32 Method,
    bool AfterWrite
)
{
    MASSERT(pChannelDesc != NULL);

    // Minor hack: Combine Method and the AfterWrite bit into a
    // single 32-bit number, to use as the first lookup key.  This
    // should be OK since the last two bits of Method are always 0.
    //
    MASSERT((Method & 0x03) == 0);
    UINT32 Key = Method | (AfterWrite ? 1 : 0);

    m_MethodIdHooks[Key][ClassId].insert(pChannelDesc);
}

//--------------------------------------------------------------------
//! \brief Return true if we should generate a PmEvent_MethodIdWrite event.
//!
//! This method returns true if the arguments match a previous call to
//! HookMethodIdEvent().  Used by PmChannelWrapper to insert the events.
//!
//! \param pChannel The channel being written.  Does not have to be
//!     the outermost channel wrapper, to make it easier to call from
//!     PmChannelWrapper.
//! \param Subch The subchannel being written
//! \param Method The method being written
//! \param AfterWrite Check whether we should generate an event before
//!     (false) or after (true) the write.
//! \param[out] pClassId If this method returns true and this arg is non-NULL,
//!     then on exit, *pClassId will be set to the class that should
//!     be sent in the MethodId event.
//!
bool PmEventManager::NeedMethodIdEvent
(
    Channel *pChannel,
    UINT32 Subch,
    UINT32 Method,
    bool AfterWrite,
    UINT32 *pClassId
)
{
    MASSERT(pChannel != NULL);

    // Combine Method and the AfterWrite bit into a single 32-bit
    // number, as described in HookMethodIdEvent().
    //
    MASSERT((Method & 0x03) == 0);
    UINT32 Key = Method | (AfterWrite ? 1 : 0);

    // Abort if the Method/AfterWrite key doesn't match; no sense
    // doing the more complex ClassId-lookop and channel-matching if
    // this quick & easy lookup fails.
    //
    if (m_MethodIdHooks.count(Key) == 0)
    {
        return false;
    }

    // Get the ClassId.  Methods < 0x100 are host methods, in which
    // case the ClassId is the channel class.  Methods >= 0x100 are
    // "ordinary" methods, in which case we need to query the
    // SemaphoreChannelWrapper to find the class associated with the
    // subchannel.
    //
    UINT32 ClassId = LW01_NULL_OBJECT;

    if (Method < 0x100)
    {
        ClassId = pChannel->GetClass();
    }
    else
    {
        SemaphoreChannelWrapper *pSemaphoreChannelWrapper =
            pChannel->GetWrapper()->GetSemaphoreChannelWrapper();
        MASSERT(pSemaphoreChannelWrapper != NULL);

        ClassId = pSemaphoreChannelWrapper->GetClassOnSubch(Subch);
    }

    MASSERT(ClassId != LW01_NULL_OBJECT);

    // Abort if the ClassId doesn't match.
    //
    if (m_MethodIdHooks[Key].count(ClassId) == 0)
    {
        return false;
    }

    // Abort if pChannel's test isn't running
    //
    PmChannelWrapper *pPmChannelWrapper =
        pChannel->GetWrapper()->GetPmChannelWrapper();

    if (!pPmChannelWrapper->InTest())
    {
        return false;
    }

    // If we get this far, then we are generating events for this
    // class, method, etc on at least one channel.  So now check to
    // see if we would match an event on *this* channel, and return
    // true if we would.
    //
    const set<const PmChannelDesc*> &ChannelDescs =
        m_MethodIdHooks[Key][ClassId];
    PmEvent_MethodIdWrite dummyEvent(pPmChannelWrapper->GetPmChannel(),
                                     ClassId, Method, AfterWrite);

    for (set<const PmChannelDesc*>::const_iterator ppDesc =
             ChannelDescs.begin(); ppDesc !=  ChannelDescs.end(); ++ppDesc)
    {
        if ((*ppDesc)->Matches(&dummyEvent))
        {
            if (pClassId != NULL)
                *pClassId = ClassId;
            return true;
        }
    }

    // None of the PmChannelDescs matched this channel.
    //
    return false;
}

//--------------------------------------------------------------------
//! \brief Static method to use as the thread's entry point or callback
//!
//! This static method calls Handler(), and if Handler() returns an
//! error, this method sets m_SubtaskRc so that the main thread will
//! know which error oclwrred.  It also kill all subtasks to keep the
//! PolicyManager from doing anything else until the main thread
//! notices the error.
//!
void PmEventManager::Subtask::StaticHandler(void *pThis)
{
    Subtask *pSubtask = static_cast<Subtask*>(pThis);
    RC rc;

    rc = pSubtask->Handler();

    if (rc != OK)
    {
        PmEventManager *pEventManager = pSubtask->m_pEventManager;

        Printf(Tee::PriDebug, "Error in advanced scheduling subtask: %s\n",
               rc.Message());
        pEventManager->m_SubtaskRc = rc;

        for (set<Subtask*>::iterator ppSubtask =
                 pEventManager->m_Subtasks.begin();
             ppSubtask != pEventManager->m_Subtasks.end(); ++ppSubtask)
        {
            (*ppSubtask)->Stop();
        }
    }
}

//--------------------------------------------------------------------
//! \brief Constructor for resman-event subtask
//!
PmEventManager::RmEventSubtask::RmEventSubtask
(
    PmEventManager *pEventManager,
    LwRm::Handle hParent,
    UINT32 Index,
    LwRm* pLwRm
) :
    Subtask(pEventManager),
    m_hParent(hParent),
    m_Index(Index),
    m_hRmEvent(0),
    m_pLwRm(pLwRm)
{
    MASSERT(hParent != 0);
    MASSERT(pLwRm);
}

//--------------------------------------------------------------------
//! \brief Start the resman-event subtask
//!
/* virtual */ RC PmEventManager::RmEventSubtask::Start()
{
    RC rc;
    CHECK_RC(m_EventThread.SetHandler(StaticHandler, this));

    const GpuDevices& devs = GetEventManager()->GetPolicyManager()->GetGpuDevices();
    MASSERT(devs.size() == 1);
    if (devs.size() != 1)
    {
        return RC::SOFTWARE_ERROR;
    }
    void* const pOsEvent = m_EventThread.GetOsEvent(
            m_pLwRm->GetClientHandle(),
            m_pLwRm->GetDeviceHandle(devs[0]));

    CHECK_RC(m_pLwRm->AllocEvent(m_hParent,
                               &m_hRmEvent,
                               LW01_EVENT_OS_EVENT,
                               m_Index,
                               pOsEvent));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Stop the RM-event subtask
//!
/* virtual */ RC PmEventManager::RmEventSubtask::Stop()
{
    RC rc;

    if (m_hRmEvent != 0)
    {
        m_pLwRm->Free(m_hRmEvent);
        m_hRmEvent = 0;
    }
    CHECK_RC(m_EventThread.SetHandler(NULL));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Subtask function that handles RM events
//!
/* virtual */ RC PmEventManager::RmEventSubtask::Handler()
{
    RC rc;

    // Call each hook.  To protect us in case one hook deletes
    // another, we first copy the hooks, and then only call each hook
    // if it's still in the "real" list.
    //
    vector<Hook> copiedHooks(m_Hooks.size());
    copy(m_Hooks.begin(), m_Hooks.end(), copiedHooks.begin());

    for (vector<Hook>::iterator pHook = copiedHooks.begin();
         pHook != copiedHooks.end(); ++pHook)
    {
        if (m_Hooks.count(*pHook) > 0)
        {
            CHECK_RC(pHook->m_pFunc(&pHook->m_Data[0]));
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmEventManager::RmEventSubtask::Hook::Hook
(
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
) :
    m_pFunc(pFunc),
    m_Data(Size)
{
    MASSERT(pFunc);
    MASSERT(pData);
    copy((UINT08*)pData, (UINT08*)pData + Size, m_Data.begin());
}

//--------------------------------------------------------------------
//! Define a sorting order for rm-event hooks.  Mostly so we can check
//! whether a hook exists in m_Hooks using set<>::count in O(logN).
//!
bool PmEventManager::RmEventSubtask::Hook::operator<
(
    const PmEventManager::RmEventSubtask::Hook &rhs
) const
{
    if (m_pFunc != rhs.m_pFunc)
        return m_pFunc < rhs.m_pFunc;
    else
        return m_Data < rhs.m_Data;
}

//--------------------------------------------------------------------
//! \brief Constructor for resman-event subtask on subdevice
//!
PmEventManager::SubdeviceSubtask::SubdeviceSubtask
(
    PmEventManager *pEventManager,
    LwRm::Handle hSubdevice,
    UINT32 Index,
    LwRm* pLwRm
) :
    RmEventSubtask(pEventManager, hSubdevice, Index, pLwRm)
{
}

//--------------------------------------------------------------------
//! \brief Start the resman-event subdevice subtask
//!
/* virtual */ RC PmEventManager::SubdeviceSubtask::Start()
{
    RC rc;

    CHECK_RC(RmEventSubtask::Start());

    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS params = {0};
    params.event  = DRF_NUM(0005, _NOTIFY_INDEX, _INDEX, GetIndex());
    params.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;
    CHECK_RC(m_pLwRm->Control(GetParent(),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &params, sizeof(params)));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Stop the resman-event subdevice subtask
//!
/* virtual */ RC PmEventManager::SubdeviceSubtask::Stop()
{
    RC rc;

    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS params = {0};
    params.event  = DRF_NUM(0005, _NOTIFY_INDEX, _INDEX, GetIndex());
    params.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_DISABLE;
    CHECK_RC(m_pLwRm->Control(GetParent(),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &params, sizeof(params)));

    CHECK_RC(RmEventSubtask::Stop());
    return rc;
}

//--------------------------------------------------------------------
//! \brief Constructor for PMU subtask
//!
PmEventManager::PmuSubtask::PmuSubtask
(
    PmEventManager *pEventManager,
    GpuSubdevice *pSubdevice,
    PMU *pPmu
) :
    Subtask(pEventManager),
    m_pSubdevice(pSubdevice),
    m_pPmu(pPmu)
{
    MASSERT(pEventManager);
    MASSERT(pSubdevice);
    MASSERT(pPmu);
}

//--------------------------------------------------------------------
//! \brief Start the PMU subtask
//!
RC PmEventManager::PmuSubtask::Start()
{
    RC rc;
    CHECK_RC(m_EventThread.SetHandler(StaticHandler, this));
    CHECK_RC(m_pPmu->AddEvent(m_EventThread.GetEvent(),
                              PMU::ALL_UNIT_ID,
                              true,
                              RM_PMU_UNIT_INIT));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Stop the PMU subtask
//!
RC PmEventManager::PmuSubtask::Stop()
{
    RC rc;
    if (m_EventThread.GetHandler() != NULL)
    {
        m_pPmu->DeleteEvent(m_EventThread.GetEvent());
        CHECK_RC(m_EventThread.SetHandler(NULL));
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Subtask function that handles PMU events from the RM
//!
//! This method is called in a subtask whenever a PMU event
//! oclwrs on a subdevice.  The boilerplate code that calls
//! this method is is handled by an EventThread.
//!
RC PmEventManager::PmuSubtask::Handler()
{
    RC rc;
    vector<PMU::Message> eventBuffer;

    // Query the resman for the event(s) that oclwrred.  The RM keeps
    // the pending events in a fifo, so we may get several events at
    // once.
    //
    CHECK_RC(m_pPmu->GetMessages(m_EventThread.GetEvent(),
                                 &eventBuffer));

    // Process each event
    //
    for (UINT32 ii = 0; ii < eventBuffer.size(); ii++)
    {
        // Pass the event to HandleEvent()
        //
        PmEvent_PmuEvent event(m_pSubdevice, eventBuffer[ii]);
        CHECK_RC(GetEventManager()->HandleEvent(&event));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Constructor for memory subtask
//!
PmEventManager::MemorySubtask::MemorySubtask
(
    PmEventManager *pEventManager
) :
    Subtask(pEventManager),
    m_ThreadID(Tasker::NULL_THREAD)
{
}

//--------------------------------------------------------------------
//! \brief Start the memory subtask
//!
RC PmEventManager::MemorySubtask::Start()
{
    RC rc;
    MASSERT(!m_Hooks.empty());
    if (m_ThreadID == Tasker::NULL_THREAD)
    {
        m_ThreadID = Tasker::CreateThread(StaticHandler, this,
                                          Tasker::MIN_STACK_SIZE,
                                          "PmEventManager::MemorySubtask");
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Stop the memory subtask
//!
RC PmEventManager::MemorySubtask::Stop()
{
    RC rc;
    Tasker::ThreadID threadID = m_ThreadID;

    m_Hooks.clear();
    m_ThreadID = Tasker::NULL_THREAD;

    if (threadID != Tasker::NULL_THREAD &&
        threadID != Tasker::LwrrentThread())
    {
        CHECK_RC(Tasker::Join(threadID));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Subtask function that detects when a memory address changes
//!
RC PmEventManager::MemorySubtask::Handler()
{
    RC rc;
    vector<Hook> hooksToRun;

    while (!m_Hooks.empty())
    {
        // Get list of hooks to run.  We do this as a separate step
        // because one of the hooks might add/remove other hooks from
        // the list, which could screw up our iterator.
        //
        hooksToRun.clear();
        set<Hook> copiedHooks = m_Hooks;
        for (set<Hook>::iterator pHook = copiedHooks.begin();
             pHook != copiedHooks.end(); ++pHook)
        {
            if (Platform::VirtualRd32(pHook->m_pMemAddress) != pHook->m_MemValue)
            {
                hooksToRun.push_back(*pHook);
            }
        }

        // Run each hook we found above, and erase it from m_Hooks.
        //
        // Before running each hook, we double-check to make sure it's
        // still in m_Hooks, and that it's still in a "triggered"
        // state.
        //
        for (vector<Hook>::iterator pHook = hooksToRun.begin();
             pHook != hooksToRun.end(); ++pHook)
        {
            if (m_Hooks.count(*pHook) > 0 &&
                Platform::VirtualRd32(pHook->m_pMemAddress) != pHook->m_MemValue)
            {
                m_Hooks.erase(*pHook);
                CHECK_RC(pHook->m_pFunc(&pHook->m_Data[0]));
            }
        }

        Tasker::Yield();
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmEventManager::MemorySubtask::Hook::Hook
(
    UINT32 *pMemAddress,
    UINT32 MemValue,
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
) :
    m_pMemAddress(pMemAddress),
    m_MemValue(MemValue),
    m_pFunc(pFunc),
    m_Data(Size)
{
    MASSERT(pMemAddress);
    MASSERT(pFunc);
    MASSERT(pData);
    copy((UINT08*)pData, (UINT08*)pData + Size, m_Data.begin());
}

//--------------------------------------------------------------------
//! Define a sorting order for memory hooks.  Mostly so we can check
//! whether a hook exists in m_Hooks using set<>::count in O(logN).
//!
bool PmEventManager::MemorySubtask::Hook::operator<
(
    const PmEventManager::MemorySubtask::Hook &rhs
) const
{
    if (m_pMemAddress != rhs.m_pMemAddress)
        return m_pMemAddress < rhs.m_pMemAddress;
    else if (m_pFunc != rhs.m_pFunc)
        return m_pFunc < rhs.m_pFunc;
    else if (m_MemValue != rhs.m_MemValue)
        return m_MemValue < rhs.m_MemValue;
    else
        return m_Data < rhs.m_Data;
}

//--------------------------------------------------------------------
//! \brief Initialize PmEventManager::SurfaceSubtask::m_logTag
//!
const string PmEventManager::SurfaceSubtask::m_logTag("[policy manager][surface subtask]");

//--------------------------------------------------------------------
//! \brief Constructor for surface subtask
//!
PmEventManager::SurfaceSubtask::SurfaceSubtask
(
    PmEventManager *pEventManager
) :
    Subtask(pEventManager),
    m_ThreadID(Tasker::NULL_THREAD)
{
}

//--------------------------------------------------------------------
//! \brief Start the surface subtask
//!
RC PmEventManager::SurfaceSubtask::Start()
{
    RC rc;
    MASSERT(!m_Hooks.empty());
    if (m_ThreadID == Tasker::NULL_THREAD)
    {
        m_ThreadID = Tasker::CreateThread(StaticHandler, this,
                                          Tasker::MIN_STACK_SIZE,
                                          "PmEventManager::SurfaceSubtask");
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Stop the surface subtask
//!
RC PmEventManager::SurfaceSubtask::Stop()
{
    RC rc;
    Tasker::ThreadID threadID = m_ThreadID;

    m_Hooks.clear();
    m_ThreadID = Tasker::NULL_THREAD;

    if (threadID != Tasker::NULL_THREAD &&
        threadID != Tasker::LwrrentThread())
    {
        CHECK_RC(Tasker::Join(threadID));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Subtask function that detects when data on a surface changes
//!
RC PmEventManager::SurfaceSubtask::Handler()
{
    RC rc;
    vector<Hook> hooksToRun;
    PmMemRanges matchingMemRanges;
    bool bRunHook;

    while (!m_Hooks.empty())
    {
        // Get list of hooks to run.  We do this as a separate step
        // because one of the hooks might add/remove other hooks from
        // the list, which could screw up our iterator.
        //
        hooksToRun.clear();
        set<Hook> copiedHooks = m_Hooks;
        for (set<Hook>::iterator pHook = copiedHooks.begin();
             pHook != copiedHooks.end(); ++pHook)
        {
            CHECK_RC(IsHookTriggered(*pHook, &bRunHook, NULL, NULL));

            if (bRunHook)
            {
                hooksToRun.push_back(*pHook);
            }
        }

        // Run each hook we found above
        //
        // Before running each hook, we double-check to make sure it's
        // still in m_Hooks, and that it's still in a "triggered"
        // state.
        //
        for (vector<Hook>::iterator pHook = hooksToRun.begin();
             pHook != hooksToRun.end(); ++pHook)
        {
            if (m_Hooks.count(*pHook) > 0)
            {
                Hook newHook;

                // Ensure that the hook remains triggered and retrieve the
                // matching ranges as well as a new hook
                CHECK_RC(IsHookTriggered(*pHook, &bRunHook,
                                         &matchingMemRanges, &newHook));
                if (bRunHook)
                {
                    PmPageDesc updatedDesc(*pHook->m_pPageDesc);
                    vector<UINT08> updatedData(0);
                    CHECK_RC(pHook->m_pFunc(&pHook->m_Data[0],
                                            matchingMemRanges,
                                            &updatedDesc,
                                            &updatedData));

                    // m_pFunc may call Tasker::Yield() to activate other threads.
                    // m_Hooks may be modified/deactivated by others.
                    // Before updating the hook, double-check to make sure
                    // it's still in m_Hooks which means it's still in a
                    // "triggered" state. Bug 1605825.
                    if (m_Hooks.count(*pHook) > 0)
                    {
                        // Update the page desc and data vector in the new hook
                        *newHook.m_pPageDesc = updatedDesc;
                        newHook.m_SurfaceData = updatedData;

                        // Update the lwrrently triggered ranges in the new hook
                        // and replace the old hook with the new one (since the
                        // hook keeps track of triggered memory ranges)
                        CHECK_RC(UpdateTriggeredRanges(&newHook));
                        m_Hooks.erase(*pHook);
                        m_Hooks.insert(newHook);
                    }
                }
            }
        }

        Tasker::Yield();
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Private subtask function that detects when a hook is triggered
//!
//! Which memory ranges have been triggered may also be returned if
//! pMatchingRanges is not NULL.  Also if pNewHook is not NULL then
//! pNewHook will contain an updated version of the triggered hook
//! (with the lwrrently triggered memory ranges)
RC PmEventManager::SurfaceSubtask::IsHookTriggered
(
    const Hook &hook,
    bool *pbHookTriggered,
    PmMemRanges* pMatchingRanges,
    Hook* pNewHook
)
{
    RC                    rc;
    PmMemRanges           memRanges;
    bool                  bRangeTriggered;
    set< pair< PmMemRange, vector<UINT08> > >    newTriggeredPairs;

    if (pNewHook)
        *pNewHook = hook;
    if (pMatchingRanges)
        pMatchingRanges->clear();

    *pbHookTriggered = false;
    memRanges = hook.m_pPageDesc->GetMatchingRangesInTrigger(
                                         GetEventManager()->GetPolicyManager());
    for (UINT32 memRangeIdx = 0; (memRangeIdx < memRanges.size());
         memRangeIdx++)
    {
        CHECK_RC(IsMemRangeTriggered(hook, memRanges[memRangeIdx],
                                     hook.m_SurfaceData,
                                     &bRangeTriggered));
        if (bRangeTriggered)
        {
            pair< PmMemRange, vector<UINT08> > newPair;
            newPair.first = memRanges[memRangeIdx];
            newPair.second = hook.m_SurfaceData;
            newTriggeredPairs.insert(newPair);

            // Triggers only occur if the memory range has already been
            // triggered.  In order to re-trigger, it is necessary for at
            // least one polling cycle to occur where the memory range has
            // not been triggered
            if (hook.m_TriggeredPairs.count(newPair) == 0)
            {
                if (pMatchingRanges)
                    pMatchingRanges->push_back(newPair.first);
                *pbHookTriggered = true;
            }
        }
    }

    if (pNewHook)
    {
        // Surface subtasks append new triggered ranges in order to
        // maintain status of priorly triggered hooks
        set< pair< PmMemRange, vector<UINT08> > >::iterator pPair
            = newTriggeredPairs.begin();
        for( ; pPair != newTriggeredPairs.end(); pPair++)
        {
            pNewHook->m_TriggeredPairs.insert(*pPair);
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Private subtask function that updates the lwrrently triggered
//! ranges in a hook
//!
//! The triggered ranges in the hook may be changed during the callback
//! This API is used to update the lwrrently triggered ranges in the
//! hook so that re-triggering can occur if triggering the hook
//! resets the trigger.
RC PmEventManager::SurfaceSubtask::UpdateTriggeredRanges(Hook *pHook)
{
    RC      rc;
    bool    bRangeTriggered;
    vector< pair< PmMemRange, vector<UINT08> > > nonTriggeredPairs;

    set< pair< PmMemRange, vector<UINT08> > >::iterator pPair =
        pHook->m_TriggeredPairs.begin();

    for ( ; pPair != pHook->m_TriggeredPairs.end(); pPair++)
    {

        CHECK_RC(IsMemRangeTriggered(*pHook,
                                     (*pPair).first,
                                     (*pPair).second,
                                     &bRangeTriggered));
        if (!bRangeTriggered)
        {
            nonTriggeredPairs.push_back(*pPair);
        }
    }
    for (UINT32 idx = 0; (idx < nonTriggeredPairs.size()); idx++)
    {
        pHook->m_TriggeredPairs.erase(nonTriggeredPairs[idx]);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Private subtask function to check whether a specific memory
//! range has been triggered
//!
RC PmEventManager::SurfaceSubtask::IsMemRangeTriggered
(
    const Hook          &hook,
    const PmMemRange    &memRange,
    const vector<UINT08> triggerData,
    bool                *pbTriggered
)
{
    RC               rc;
    const PmGpuDesc *pGpuDesc;
    GpuSubdevices    subdevs;

    pGpuDesc = hook.m_pPageDesc->GetSurfaceDesc()->GetGpuDesc();
    if (pGpuDesc)
    {
        subdevs = pGpuDesc->GetMatchingSubdevicesInTrigger(
                                     GetEventManager()->GetPolicyManager());
    }
    else
    {
        subdevs = GetEventManager()->GetPolicyManager()->GetGpuSubdevices();
    }

    // In order for the hook to be triggered, the data must be valid on all
    // matching subdevices
    *pbTriggered = true;
    for (UINT32 gpuIdx = 0; (gpuIdx < subdevs.size()) && *pbTriggered;
         gpuIdx++)
    {
        GpuSubdevice *pSubdev = subdevs[gpuIdx];

        // Since (at least theoretically) there could be surfaces
        // allocated on different GpuDevices in the matching surfaces
        // it is necessary to ensure that the surface is present on
        // the current subdevice
        if (pSubdev->GetParentDevice() == memRange.GetMdiagSurf()->GetGpuDev())
        {
            vector<UINT08> data;

            rc = memRange.Read(&data, pSubdev);
            if (rc != OK)
            {
                *pbTriggered = false;
                return rc;
            }

            m_Watcher.watch(memRange, data);
            if (!equal(data.begin(), data.end(), triggerData.begin()))
            {
                *pbTriggered = false;
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Constructors
//!
PmEventManager::SurfaceSubtask::Hook::Hook
(
    PmPageDesc* pPageDesc,
    const vector<UINT08> &SurfaceData,
    RC (*pFunc)(void*, const PmMemRanges &,
                PmPageDesc*, vector<UINT08>*),
    void *pData,
    UINT32 Size
) :
    m_pPageDesc(pPageDesc),
    m_SurfaceData(SurfaceData),
    m_pFunc(pFunc),
    m_Data(Size)
{
    MASSERT(pPageDesc);
    MASSERT(pFunc);
    MASSERT(pData);
    MASSERT(SurfaceData.size() == pPageDesc->GetSize());
    copy((UINT08*)pData, (UINT08*)pData + Size, m_Data.begin());
}

//--------------------------------------------------------------------
//! Define a sorting order for memory hooks.  Mostly so we can check
//! whether a hook exists in m_Hooks using set<>::count in O(logN).
//!
bool PmEventManager::SurfaceSubtask::Hook::operator<
(
    const PmEventManager::SurfaceSubtask::Hook &rhs
) const
{
    if (m_pPageDesc != rhs.m_pPageDesc)
        return m_pPageDesc < rhs.m_pPageDesc;
    if (m_pPageDesc->GetOffset() != rhs.m_pPageDesc->GetOffset())
        return m_pPageDesc->GetOffset() < rhs.m_pPageDesc->GetOffset();
    if (m_pPageDesc->GetSize() != rhs.m_pPageDesc->GetSize())
        return m_pPageDesc->GetSize() < rhs.m_pPageDesc->GetSize();
    else if (m_pFunc != rhs.m_pFunc)
        return m_pFunc < rhs.m_pFunc;
    else
        return m_Data < rhs.m_Data;
}

//--------------------------------------------------------------------
//! \brief Constructors for PmEventManager::SurfaceSubtask::Watcher
//!
PmEventManager::SurfaceSubtask::Watcher::Watcher
(
) :
    m_WatchList(10 /*minium bucket count*/,
                [](const PmMemRange& key) -> size_t
                {
                    size_t seed = 0;
                    boost::hash_combine(seed, key.GetSurface());
                    boost::hash_combine(seed, key.GetOffset());
                    boost::hash_combine(seed, key.GetSize());
                    return seed;
                },
                [](const PmMemRange& lhs, const PmMemRange& rhs)
                {
                    return (lhs.GetSurface() == rhs.GetSurface()) &&
                        (lhs.GetOffset() == lhs.GetOffset()) &&
                        (lhs.GetSize() == rhs.GetSize());
                })
{
}

//--------------------------------------------------------------------
//! \brief watch the data in given memory range is changed or not.
//!
//!  Print changed data if debug level message can be printed out
//!
void PmEventManager::SurfaceSubtask::Watcher::watch
(
    const PmMemRange& pmMemRange,
    const vector<UINT08>& data
)
{
    if (!Tee::WillPrint(Tee::PriDebug))
    {
        return ;
    }

    if (m_WatchList.count(pmMemRange) && (m_WatchList[pmMemRange] == data))
    {
        return ;
    }

    m_WatchList[pmMemRange] = data;
    stringstream ss;
    ss << pmMemRange << " data ( ";
    for (auto i : data)
    {
        ss << hex << "0x" << static_cast<UINT32>(i) << " " << dec;
    }
    ss << ")";
    Printf(Tee::PriDebug, "%s %s: %s\n" ,
           PmEventManager::SurfaceSubtask::m_logTag.c_str(), __FUNCTION__, ss.str().c_str());
}

//--------------------------------------------------------------------
//! \brief Constructor for timer subtask
//!
PmEventManager::TimerSubtask::TimerSubtask
(
    PmEventManager *pEventManager
) :
    Subtask(pEventManager),
    m_ThreadID(Tasker::NULL_THREAD),
    m_pEvent(Tasker::AllocEvent("PolicyManager::TimerEvent"))
{
}

//--------------------------------------------------------------------
//! \brief Destructor for timer subtask
//!
PmEventManager::TimerSubtask::~TimerSubtask()
{
    Stop();  // PmEventManager should've already called this, but just in case
    Tasker::FreeEvent(m_pEvent);
}

//--------------------------------------------------------------------
//! \brief Start the timer subtask
//!
RC PmEventManager::TimerSubtask::Start()
{
    RC rc;
    MASSERT(!m_Hooks.empty());
    if (m_ThreadID == Tasker::NULL_THREAD)
    {
        m_ThreadID = Tasker::CreateThread(StaticHandler, this,
                                          Tasker::MIN_STACK_SIZE,
                                          "PmEventManager::TimerSubtask");
    }
    Tasker::SetEvent(m_pEvent);
    return rc;
}

//--------------------------------------------------------------------
//! \brief Stop the timer subtask
//!
RC PmEventManager::TimerSubtask::Stop()
{
    RC rc;
    Tasker::ThreadID threadID = m_ThreadID;

    m_Hooks.clear();
    m_ThreadID = Tasker::NULL_THREAD;

    if (threadID != Tasker::NULL_THREAD &&
        threadID != Tasker::LwrrentThread())
    {
        Tasker::SetEvent(m_pEvent);
        CHECK_RC(Tasker::Join(threadID));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Subtask function that calls hooks at a specified time
//!
RC PmEventManager::TimerSubtask::Handler()
{
    static const FLOAT64 MIN_TIMEOUT_MS = 100.0;  // Minimum timeout, in ms
    RC rc;

    while (m_ThreadID == Tasker::LwrrentThread())
    {
        Tasker::ResetEvent(m_pEvent);

        if (m_Hooks.empty())
        {
            CHECK_RC(Tasker::WaitOnEvent(m_pEvent, Tasker::NO_TIMEOUT));
        }
        else
        {
            INT64 RemainingTimeUs = (INT64)(m_Hooks.front().m_TimeUS -
                                            Platform::GetTimeUS());
            FLOAT64 RemainingTimeMs = RemainingTimeUs / 1000.0;

            if (RemainingTimeMs > MIN_TIMEOUT_MS)
            {
                CHECK_RC(Tasker::WaitOnEvent(
                             m_pEvent, RemainingTimeMs - MIN_TIMEOUT_MS));
            }
            else if (RemainingTimeMs > 0.0)
            {
                CHECK_RC(Tasker::Yield());
            }
            else
            {
                Hook NextHook = m_Hooks.front();
                pop_heap(m_Hooks.begin(), m_Hooks.end());
                m_Hooks.pop_back();
                CHECK_RC(NextHook.m_pFunc(&NextHook.m_Data[0]));
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmEventManager::TimerSubtask::Hook::Hook
(
    UINT64 TimeUS,
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
) :
    m_TimeUS(TimeUS),
    m_pFunc(pFunc),
    m_Data(Size)
{
    MASSERT(pData);
    copy((UINT08*)pData, (UINT08*)pData + Size, m_Data.begin());
}

//--------------------------------------------------------------------
//! \brief Define a sorting order for timer hooks.
//!
//! The sorting order ensures that the next hook that should execute
//! -- i.e. the one with the lowest m_TimeUS -- is at the front of the
//! m_Hooks heap.  Note that STL heaps put the highest element at the
//! front of the heap, not the lowest, so we reverse the expected
//! order of this operator.
//!
bool PmEventManager::TimerSubtask::Hook::operator<
(
    const PmEventManager::TimerSubtask::Hook &rhs
) const
{
    if (rhs.m_TimeUS != m_TimeUS)
        return rhs.m_TimeUS < m_TimeUS;
    else if (rhs.m_pFunc != m_pFunc)
        return rhs.m_pFunc < m_pFunc;
    else
        return rhs.m_Data < m_Data;
}

//--------------------------------------------------------------------
//! \brief Equality operator for timer hooks.
//!
bool PmEventManager::TimerSubtask::Hook::operator==
(
    const PmEventManager::TimerSubtask::Hook &rhs
) const
{
    return (m_TimeUS == rhs.m_TimeUS &&
            m_pFunc == rhs.m_pFunc &&
            m_Data == m_Data);
}

//--------------------------------------------------------------------
//! \brief Add a new Hook object to the subtask's m_Hook heap
//!
void PmEventManager::TimerSubtask::AddHook(const Hook &hook)
{
    m_Hooks.push_back(hook);
    push_heap(m_Hooks.begin(), m_Hooks.end());
    Tasker::SetEvent(m_pEvent);  // In case the new hook runs next
}

//--------------------------------------------------------------------
//! \brief Delete a new Hook object to the subtask's m_Hook heap
//!
void PmEventManager::TimerSubtask::DelHook(const Hook &hook)
{
    vector<Hook>::iterator pEnd = remove(m_Hooks.begin(), m_Hooks.end(), hook);
    m_Hooks.erase(pEnd, m_Hooks.end());
    make_heap(m_Hooks.begin(), m_Hooks.end());
}

//--------------------------------------------------------------------
//! \brief Constructor for error logger subtask
//!
PmEventManager::ErrorLoggerSubtask::ErrorLoggerSubtask
(
    PmEventManager *pEventManager
) :
    Subtask(pEventManager)
{
}

//--------------------------------------------------------------------
//! \brief Destructor for error logger subtask
//!
PmEventManager::ErrorLoggerSubtask::~ErrorLoggerSubtask()
{
    Stop();  // PmEventManager should've already called this, but just in case
}

//--------------------------------------------------------------------
//! \brief Start the error logger subtask
//!
RC PmEventManager::ErrorLoggerSubtask::Start()
{
    RC rc;
    MASSERT(!m_Hooks.empty());
    CHECK_RC(m_EventThread.SetHandler(StaticHandler, this));
    ErrorLogger::AddEvent(m_EventThread.GetEvent());
    return rc;
}

//--------------------------------------------------------------------
//! \brief Stop the error logger subtask
//!
RC PmEventManager::ErrorLoggerSubtask::Stop()
{
    RC rc;
    m_Hooks.clear();
    if (m_EventThread.GetHandler() != nullptr)
    {
        ErrorLogger::RemoveEvent(m_EventThread.GetEvent());
        CHECK_RC(m_EventThread.SetHandler(nullptr));
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Subtask function that calls hooks at a specified time
//!
RC PmEventManager::ErrorLoggerSubtask::Handler()
{
    RC rc;

    // Call each hook.  To protect us in case one hook deletes
    // another, we first copy the hooks, and then only call each hook
    // if it's still in the "real" list.
    //
    vector<Hook> copiedHooks(m_Hooks.size());
    copy(m_Hooks.begin(), m_Hooks.end(), copiedHooks.begin());

    for (vector<Hook>::iterator pHook = copiedHooks.begin();
         pHook != copiedHooks.end(); ++pHook)
    {
        if (m_Hooks.count(*pHook) > 0)
        {
            CHECK_RC(pHook->m_pFunc(&pHook->m_Data[0]));
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmEventManager::ErrorLoggerSubtask::Hook::Hook
(
    RC (*pFunc)(void*),
    void *pData,
    UINT32 Size
) :
    m_pFunc(pFunc),
    m_Data(Size)
{
    MASSERT(pData);
    copy((UINT08*)pData, (UINT08*)pData + Size, m_Data.begin());
}

//--------------------------------------------------------------------
//! Define a sorting order for error logger hooks.  Mostly so we can check
//! whether a hook exists in m_Hooks using set<>::count in O(logN).
//!
bool PmEventManager::ErrorLoggerSubtask::Hook::operator<
(
    const PmEventManager::ErrorLoggerSubtask::Hook &rhs
) const
{
    if (m_pFunc != rhs.m_pFunc)
        return m_pFunc < rhs.m_pFunc;
    else
        return m_Data < rhs.m_Data;
}

