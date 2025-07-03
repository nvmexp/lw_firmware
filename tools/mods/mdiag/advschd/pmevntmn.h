/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2018 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Defines PmEventManager which is in charge of registering
//! the events and triggering the actions.

#ifndef INCLUDED_PMEVNTMN_H
#define INCLUDED_PMEVNTMN_H

#ifndef INCLUDED_TASKER_H
#include "core/include/tasker.h"
#endif

#ifndef INCLUDED_EVNTTHRD_H
#include "core/include/evntthrd.h"
#endif

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#ifndef INCLUDED_PMUTILS_H
#include "pmutils.h"
#endif

#ifndef INCLUDED_PMSURF_H
#include "pmsurf.h"
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_STL_SET
#include <set>
#define INCLUDED_STL_SET
#endif

#include <functional>
#include <unordered_map>

class PMU;

//--------------------------------------------------------------------
//! \brief Class that handles events and manages triggers & actionBlocks
//!
//! This class has several responsibilities:
//! - It handles all advanced-scheduling events, by checking if the
//!   event activated any triggers, and exelwting the corresponding
//!   actionBlock.  See HandleEvent().
//! - It owns all the triggers & actionBlocks that were parsed
//!   from the policyfile.  See AddTrigger() and AddActionBlock().
//! - It manages the subtask(s) that wait for an event from the RM,
//!   and translate it to a PmEvent that gets passed to HandleEvent().
//!
class PmEventManager
{
public:
    PmEventManager(PolicyManager *pPolicyManager);
    ~PmEventManager();

    PolicyManager *GetPolicyManager() const { return m_pPolicyManager; }

    RC             AddSurfaceDesc(PmSurfaceDesc *pSurfaceDesc);
    bool           IsMatchSurfaceDesc(const string & id) const;
    PmSurfaceDesc *GetSurfaceDesc(const string &id) const;

    RC             AddChannelDesc(PmChannelDesc *pSurfaceDesc);
    PmChannelDesc *GetChannelDesc(const string &id) const;

    RC             AddVfDesc(PmVfTestDesc *pVfDesc);
    PmVfTestDesc       *GetVfDesc(const string &id) const;

    RC             AddSmcEngineDesc(PmSmcEngineDesc *pSmcEngineDesc);
    PmSmcEngineDesc       *GetSmcEngineDesc(const string &id) const;

    RC             AddVaSpaceDesc(PmVaSpaceDesc *pVaSpaceDesc);
    PmVaSpaceDesc *GetVaSpaceDesc(const string &id) const;

    RC             AddGpuDesc(PmGpuDesc *pGpuDesc);
    PmGpuDesc     *GetGpuDesc(const string &id) const;

    RC             AddRunlistDesc(PmRunlistDesc *pRunlistDesc);
    PmRunlistDesc *GetRunlistDesc(const string &id) const;

    RC             AddTestDesc(PmTestDesc *pTestDesc);
    PmTestDesc    *GetTestDesc(const string &id) const;

    RC             AddActionBlock(PmActionBlock *pActionBlock);
    PmActionBlock *GetActionBlock(const string &name) const;

    RC             AddTrigger(PmTrigger *pTrigger, PmActionBlock *pActionBlk);
    RC             AddTrigger(PmTrigger *pTrigger, PmAction *pAction);
    UINT32         GetNumTriggers() const { return (UINT32)m_Triggers.size(); }

    RC             HandleEvent(const PmEvent *pEvent);
    RC             StartTest();
    RC             EndTest();

    RC HookResmanEvent(LwRm::Handle hParent, UINT32 Index,
                       RC (*pFunc)(void*), void *pData, UINT32 Size, LwRm* pLwRm);
    RC HookResmanEvent(GpuSubdevice *pSubdevice, UINT32 Index,
                       RC (*pFunc)(void*), void *pData, UINT32 Size, LwRm* pLwRm);
    RC UnhookResmanEvent(RC (*pFunc)(void*), void *pData, UINT32 Size);
    RC HookMemoryEvent(UINT32 *pMemAddress, UINT32 memValue,
                       RC (*pFunc)(void*), void *pData, UINT32 Size);
    RC UnhookMemoryEvent(RC (*pFunc)(void*), void *pData, UINT32 Size);
    RC HookSurfaceEvent(PmPageDesc *pPageDesc,
                        const vector<UINT08> &surfaceData,
                        RC (*pFunc)(void*, const PmMemRanges &,
                                    PmPageDesc*, vector<UINT08>*),
                        void *pData, UINT32 Size);
    RC UnhookSurfaceEvent(RC (*pFunc)(void*, const PmMemRanges &,
                                      PmPageDesc*, vector<UINT08>*),
                          void *pData, UINT32 Size);
    RC HookTimerEvent(UINT64 TimeUS,
                      RC (*pFunc)(void*), void *pData, UINT32 Size);
    RC UnhookTimerEvent(RC (*pFunc)(void*), void *pData, UINT32 Size);
    RC HookErrorLoggerEvent(RC (*pFunc)(void*), void *pData, UINT32 Size);
    RC UnhookErrorLoggerEvent(RC (*pFunc)(void*), void *pData, UINT32 Size);
    void HookMethodIdEvent(const PmChannelDesc *pChannelDesc, UINT32 ClassId,
                           UINT32 Method, bool AfterWrite);
    bool NeedMethodIdEvent(Channel *pChannel, UINT32 Subch, UINT32 Method,
                           bool AfterWrite, UINT32 *pClassId);

private:
    class Subtask
    {
    public:
        Subtask(PmEventManager *pEM) : m_pEventManager(pEM) { MASSERT(pEM); }
        virtual ~Subtask() {}
        PmEventManager *GetEventManager() const { return m_pEventManager; }
        static void StaticHandler(void *pThis);
        virtual RC Handler() = 0;
        virtual RC Start() = 0;
        virtual RC Stop() = 0;
    private:
        PmEventManager *m_pEventManager;
    };

    class RmEventSubtask : public Subtask
    {
    public:
        RmEventSubtask(PmEventManager *pEventManager,
                       LwRm::Handle hParent, UINT32 Index,
                       LwRm* pLwRm);
        LwRm::Handle GetParent() const { return m_hParent; }
        UINT32       GetIndex()  const { return m_Index; }
        virtual RC Handler();
        virtual RC Start();
        virtual RC Stop();
        struct Hook
        {
            Hook() {}
            Hook(RC (*pFunc)(void*), void *pData, UINT32 Size);
            bool operator<(const Hook &rhs) const;
            RC (*m_pFunc)(void*);
            vector<UINT08> m_Data;
        };
        void AddHook(Hook hook) { m_Hooks.insert(hook); }
        void DelHook(Hook hook) { m_Hooks.erase(hook); }
    private:
        LwRm::Handle m_hParent;
        UINT32       m_Index;
        set<Hook>    m_Hooks;
        EventThread  m_EventThread;
        LwRm::Handle m_hRmEvent;
    protected:
        LwRm*        m_pLwRm;
    };

    class SubdeviceSubtask : public RmEventSubtask
    {
    public:
        SubdeviceSubtask(PmEventManager *pEventManager,
                         LwRm::Handle hSubdevice, UINT32 Index,
                         LwRm* pLwRm);
        virtual RC Start();
        virtual RC Stop();
    };

    class PmuSubtask : public Subtask
    {
    public:
        PmuSubtask(PmEventManager *pEventManager,
                   GpuSubdevice *pSubdevice, PMU *pPmu);
        virtual RC Start();
        virtual RC Stop();
        virtual RC Handler();
    private:
        GpuSubdevice   *m_pSubdevice;
        PMU            *m_pPmu;
        EventThread     m_EventThread;
    };

    class MemorySubtask : public Subtask
    {
    public:
        MemorySubtask(PmEventManager *pEventManager);
        virtual ~MemorySubtask() { Stop(); }
        virtual RC Start();
        virtual RC Stop();
        virtual RC Handler();
        struct Hook
        {
            Hook() {}
            Hook(UINT32 *pMemAddress, UINT32 MemValue,
                 RC (*pFunc)(void*), void *pData, UINT32 Size);
            bool operator<(const Hook &rhs) const;
            UINT32        *m_pMemAddress;
            UINT32         m_MemValue;
            RC           (*m_pFunc)(void*);
            vector<UINT08> m_Data;
        };
        void AddHook(const Hook &hook) { m_Hooks.insert(hook); }
        void DelHook(const Hook &hook) { m_Hooks.erase(hook); }
        const set<Hook> &GetHooks() { return m_Hooks; }
    private:
        set<Hook>        m_Hooks;
        Tasker::ThreadID m_ThreadID;
    };

    class SurfaceSubtask : public Subtask
    {
    public:
        SurfaceSubtask(PmEventManager *pEventManager);
        virtual ~SurfaceSubtask() { Stop(); }
        virtual RC Start();
        virtual RC Stop();
        virtual RC Handler();
        struct Hook
        {
            Hook() {}
            Hook(PmPageDesc *pPageDesc,
                 const vector<UINT08> &SurfaceData,
                 RC (*pFunc)(void*, const PmMemRanges &,
                             PmPageDesc*, vector<UINT08>*),
                 void *pData, UINT32 Size);
            bool operator<(const Hook &rhs) const;
            PmPageDesc           *m_pPageDesc;
            vector<UINT08>        m_SurfaceData;
            RC                  (*m_pFunc)(void*, const PmMemRanges &,
                                           PmPageDesc*, vector<UINT08>*);
            vector<UINT08>        m_Data;
            set< pair< PmMemRange,vector<UINT08> > >    m_TriggeredPairs;
        };
        void AddHook(const Hook &hook) { m_Hooks.insert(hook); }
        void DelHook(const Hook &hook) { m_Hooks.erase(hook); }
        const set<Hook> &GetHooks() { return m_Hooks; }

        struct Watcher
        {
            Watcher();
            Watcher(const Watcher&) = delete;
            void watch(const PmMemRange& pmMemRange, const vector<UINT08>& data);
            ~Watcher() = default;
            unordered_map<const PmMemRange, vector<UINT08>, function<size_t(const PmMemRange&)>,
                function<bool(const PmMemRange&, const PmMemRange&)>> m_WatchList;
        };
    private:
        RC IsHookTriggered(const Hook &hook, bool *pbHookTriggered,
                           PmMemRanges* pMatchingRanges, Hook *pNewHook);
        RC UpdateTriggeredRanges(Hook *pHook);
        RC IsMemRangeTriggered(const Hook       &hook,
                               const PmMemRange &memRange,
                               const vector<UINT08> triggerData,
                               bool *pbTriggered);

        set<Hook>            m_Hooks;
        Tasker::ThreadID     m_ThreadID;
        Watcher              m_Watcher;
        const static string  m_logTag;
    };

    class TimerSubtask : public Subtask
    {
    public:
        TimerSubtask(PmEventManager *pEventManager);
        virtual ~TimerSubtask();
        virtual RC Start();
        virtual RC Stop();
        virtual RC Handler();
        struct Hook
        {
            Hook() {}
            Hook(UINT64 TimeUS, RC (*pFunc)(void*), void *pData, UINT32 Size);
            bool operator<(const Hook &rhs) const;
            bool operator==(const Hook &rhs) const;
            UINT64         m_TimeUS;
            RC           (*m_pFunc)(void*);
            vector<UINT08> m_Data;
        };
        void AddHook(const Hook &hook);
        void DelHook(const Hook &hook);
        const vector<Hook> &GetHooks() { return m_Hooks; }
    private:
        vector<Hook>     m_Hooks;
        Tasker::ThreadID m_ThreadID;
        ModsEvent       *m_pEvent;
    };

    class ErrorLoggerSubtask : public Subtask
    {
    public:
        ErrorLoggerSubtask(PmEventManager *pEventManager);
        virtual ~ErrorLoggerSubtask();
        virtual RC Start();
        virtual RC Stop();
        virtual RC Handler();
        struct Hook
        {
            Hook() {}
            Hook(RC (*pFunc)(void*), void *pData, UINT32 Size);
            bool operator<(const Hook &rhs) const;
            RC           (*m_pFunc)(void*);
            vector<UINT08> m_Data;
        };
        void AddHook(const Hook &hook) { m_Hooks.insert(hook); }
        void DelHook(const Hook &hook) { m_Hooks.erase(hook); }
        const set<Hook> &GetHooks() { return m_Hooks; }
    private:
        set<Hook>        m_Hooks;
        EventThread      m_EventThread;
    };

    // Used to store hooks set by HookMethodIdEvent()
    //
    typedef map<UINT32, map<UINT32, set<const PmChannelDesc*> > >MethodIdHooks;

    friend void Subtask::StaticHandler(void*);

    PolicyManager              *m_pPolicyManager;    //!< Owns this object
    set<PmSurfaceDesc*>         m_SurfaceDescs;      //!< All surfaceDescs
    map<string, PmSurfaceDesc*> m_String2SurfaceDesc;//!< Named surfaceDescs
    set<PmVaSpaceDesc*>         m_VaSpaceDescs;      //!< All vaspaceDescs
    map<string, PmVaSpaceDesc*> m_String2VaSpaceDesc;//!< Named vaspaceDescs
    set<PmVfTestDesc*>          m_VfDescs;           //!< All vfDescs
    map<string, PmVfTestDesc*>  m_String2VfDesc;     //!< Named vfDescs
    set<PmSmcEngineDesc*>           m_SmcEngineDescs;           //!< All SmcEngineDescs
    map<string, PmSmcEngineDesc*>   m_String2SmcEngineDesc;     //!< Named SmcEngineDescs
    set<PmChannelDesc*>         m_ChannelDescs;      //!< All channelDescs
    map<string, PmChannelDesc*> m_String2ChannelDesc;//!< Named channelDescs
    set<PmGpuDesc*>             m_GpuDescs;          //!< All gpuDescs
    map<string, PmGpuDesc*>     m_String2GpuDesc;    //!< Named gpuDescs
    set<PmRunlistDesc*>         m_RunlistDescs;      //!< All runlistDescs
    map<string, PmRunlistDesc*> m_String2RunlistDesc;//!< Named runlistDescs
    set<PmTestDesc*>            m_TestDescs;         //!< All testDescs
    map<string, PmTestDesc*>    m_String2TestDesc;   //!< Named testDescs
    set<PmActionBlock*>         m_ActionBlocks;      //!< All actionBlocks
    map<string, PmActionBlock*> m_String2ActionBlock;//!< Named actionBlocks
    vector<PmTrigger*>          m_Triggers;          //!< All triggers, sort
                                                     //!< in policyfile order
    map<PmTrigger*, PmActionBlock*> m_TriggerActions;//!< Trigger->action map

    set<Subtask*>               m_Subtasks;          //!< All subtasks
    set<RmEventSubtask*>        m_RmEventSubtasks;   //!< All RM event subtasks
    MemorySubtask              *m_pMemorySubtask;    //!< The memory subtask
    SurfaceSubtask             *m_pSurfaceSubtask;   //!< The surface subtask
    TimerSubtask               *m_pTimerSubtask;     //!< The timer subtask
    ErrorLoggerSubtask         *m_pErrorLoggerSubtask; //!< The error logger subtask
    RC                          m_SubtaskRc;         //!< Set to non-OK if a
                                                     //!< subtask gets an err
    MethodIdHooks               m_MethodIdHooks;     //!< For HookMethodIdEvent
};

#endif // INCLUDED_PMEVNTMN_H
