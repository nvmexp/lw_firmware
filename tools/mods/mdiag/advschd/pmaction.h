/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Defines actions and action blocks

#ifndef INCLUDED_PMACTION_H
#define INCLUDED_PMACTION_H

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_CHANNEL_H
#include "core/include/channel.h"
#endif

#ifndef INCLUDED_PMEVNTMN_H
#include "pmevntmn.h"
#endif

#ifndef INCLUDED_PMUTILS_H
#include "pmutils.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#ifndef INCLUDED_RANDOM_H
#include "random.h"
#endif

#include "mdiag/utils/mdiagsurf.h"

//--------------------------------------------------------------------
//! \brief Container for a series of actions
//!
//! This container holds a series of actions.  It's the PolicyManager
//! equivalent of a subroutine.  An action block get exelwted as the
//! result of a trigger.
//!
//! Every PmActionBlock should be owned by the PmEventManager, and
//! automatically gets deleted with the PmEventManager.
//!
class PmActionBlock
{
public:
    PmActionBlock(PmEventManager *pEventManager, const string &name);
    ~PmActionBlock();
    PolicyManager *GetPolicyManager() const { return m_pEventManager->
                                                         GetPolicyManager(); }
    const string  &GetName()    const { return m_Name; }
    UINT32         GetSize()    const { return (UINT32)m_Actions.size(); }
    RC             AddAction(PmAction *pAction);
    RC             IsSupported(const PmTrigger *pTrigger) const;
    RC             Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
    RC             EndTest();
    Tasker::MutexHolder& CreateMutexHolder();
    void           ReleaseMutexes();

private:
    PmEventManager   *m_pEventManager;  //!< Parent object
    string            m_Name;           //!< ActionBlock name, or "" if unnamed
    vector<PmAction*> m_Actions;        //!< Actions owned by this action block
    vector<Tasker::MutexHolder*> m_MutexHolders; //!< Mutices to release after Execute is done
};

//--------------------------------------------------------------------
//! \brief Represents the base class for an Action
//!
//! An "action" is basically anything that should happen as a result
//! of an "event" (PmEvent) oclwrring, such as unmapping or remapping
//! memory.
//!
//! Every PmAction should be owned by a PmActionBlock, and
//! automatically gets deleted with the PmActionBlock.
//!
class PmAction
{
public:
    PmAction(const string &name) : m_pActionBlock(NULL), m_Name(name),
                                   m_TeeCode(0) {}
    virtual       ~PmAction();
    virtual RC     IsSupported(const PmTrigger *pTrigger) const = 0;
    virtual RC     Execute(PmTrigger *pTrigger, const PmEvent *pEvent) = 0;
    virtual INT32  GetNextAction();
    virtual RC     EndTest();
    PmActionBlock *GetActionBlock()   const { return m_pActionBlock; }
    PolicyManager *GetPolicyManager() const { return m_pActionBlock->
                                                         GetPolicyManager(); }
    const string  &GetName() const { return m_Name; }
    void SetTeeCode(UINT32 teecode) { m_TeeCode = teecode; }

protected:
    PmChannel *GetInbandChannel(PmChannelDesc *pChannelDesc,
        PmTrigger *pTrigger,
        const PmEvent *pEvent,
        const PmMemRange *pMemRange);
    RC GetEnginesList(RegExp engineNames, 
                      PmChannel* pPmChannel, 
                      GpuDevice* pGpuDevice,
                      vector<UINT32>* pEngines);
    map<LwRm*, PmSmcEngines> GetLwRmSmcEngines(const PmTrigger *pTrigger,
                                               const PmEvent   *pEvent,
                                               const PmSmcEngineDesc *pSmcEngineDesc) const;
    UINT32 GetTeeCode() const { return m_TeeCode; }

private:
    friend RC PmActionBlock::AddAction(PmAction*);
    void SetActionBlock(PmActionBlock *ptr) { m_pActionBlock = ptr; }

    PmActionBlock *m_pActionBlock;  //!< Set by PmActionBlock::AddAction()
    string         m_Name;          //!< Descriptive name used for debugging.
                                    //!< Set to "" to disable debug messages.
    UINT32         m_TeeCode;       //!< -message_enable code
};

//--------------------------------------------------------------------
//! PmAction subclass that calls an ActionBlock.
//!
class PmAction_Block : public PmAction
{
public:
    PmAction_Block(PmActionBlock *pActionBlock);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    PmActionBlock *m_pActionBlock;
};

//--------------------------------------------------------------------
//! PmAction subclass that prints a string.  Used for debugging.
//!
class PmAction_Print : public PmAction
{
public:
    PmAction_Print(const string &printString);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    void PrintMmuTable(string * printString, const string &keyword);
    void PrintPhysicalRead(string * printString);
    void DumpSurface(string * printString);
    void DumpFaultBuffer(string * printString, const PmEvent * pEvent);
    string m_PrintString;  // String to print when this action oclwrs
};

//--------------------------------------------------------------------
//! PmAction subclass that transports data to UTL.  
//!
class PmAction_TriggerUtlUserEvent : public PmAction
{
public:
    PmAction_TriggerUtlUserEvent(const vector<string> &data);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    vector<string> m_Data;
};

//--------------------------------------------------------------------
//! PmAction subclass that preempt the indicated runlist.
//!
class PmAction_PreemptRunlist : public PmAction
{
public:
    PmAction_PreemptRunlist(const PmRunlistDesc *pRunlistDesc,
                            const PmSmcEngineDesc *pSmcEngineDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmRunlistDesc *m_pRunlistDesc;
    const PmSmcEngineDesc *m_pSmcEngineDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that preempt the indicated channel.
//!
class PmAction_PreemptChannel : public PmAction
{
public:
    PmAction_PreemptChannel(const PmChannelDesc *pChannelDesc,
                            bool  bPollPreemptComplete,
                            FLOAT64 timeoutms);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    static bool PollPreemptDone(void* pvArgs);
    RC PreemptChannel(PmChannel* pPmChannel);
    RC PreemptTsg(PmChannel* pPmChannel);

    const PmChannelDesc *m_pChannelDesc;
    bool   m_PollPreemptComplete;
    FLOAT64 m_TimeoutMs;
};

//--------------------------------------------------------------------
//! PmAction subclass that remove the n'th to (n+num)'th entries from runlist.
//!
class PmAction_RemoveEntriesFromRunlist : public PmAction
{
public:
    PmAction_RemoveEntriesFromRunlist(const PmRunlistDesc *pRunlistDesc,
                                      UINT32 firstEntry, int numEntries);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmRunlistDesc *m_pRunlistDesc; //!< Preempted runlist to remove entries
    UINT32               m_FirstEntry;   //!< Index of first entry to be removed
    int                  m_NumEntries;   //!< Number of entry to be removed
                                         //!< Positive: direction->from 0 to end
                                         //!< Negetive: direction->from end to 0
};

//--------------------------------------------------------------------
//! PmAction subclass that restore the n'th to (n+num)'th entries to runlist
//! in indicated position.
//!
class PmAction_RestoreEntriesToRunlist : public PmAction
{
public:
    PmAction_RestoreEntriesToRunlist(const PmRunlistDesc *pRunlistDesc,
                        UINT32 insertPos, UINT32 firstEntry, int numEntries);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmRunlistDesc *m_pRunlistDesc; //!< Preempted runlist to restore entries
    UINT32               m_InsertPos;    //!< Insert restored entries to the position
    UINT32               m_FirstEntry;   //!< Index of first entry to be restored
    int                  m_NumEntries;   //!< Number of entry to be restored
                                         //!< Positive: direction->from 0 to end
                                         //!< Negetive: direction->from end to 0
};

//--------------------------------------------------------------------
//! PmAction subclass that remove the n'th to (n+num)'th entries matching
//! specifed channels from runlist.
//!
class PmAction_RemoveChannelFromRunlist : public PmAction
{
public:
    PmAction_RemoveChannelFromRunlist(const PmChannelDesc *pChannelDesc,
                                      UINT32 firstEntry, int numEntries);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Channels to be removed
    UINT32               m_FirstEntry;   //!< Index of first entry to be removed
    int                  m_NumEntries;   //!< Number of entry to be removed
                                         //!< Positive: direction->from 0 to end
                                         //!< Negetive: direction->from end to 0
};

//--------------------------------------------------------------------
//! PmAction subclass that restore the n'th to (n+num)'th entries
//! matching the specified channel in indicated position.
//!
class PmAction_RestoreChannelToRunlist : public PmAction
{
public:
    PmAction_RestoreChannelToRunlist(const PmChannelDesc *pChannelDesc,
                        UINT32 insertPos, UINT32 firstEntry, int numEntries);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Channels to be removed
    UINT32               m_InsertPos;    //!< Insert entries to the position
    UINT32               m_FirstEntry;   //!< Index of first entry to be restored
    int                  m_NumEntries;   //!< Number of entry to be restored
                                         //!< Positive: direction->from 0 to end
                                         //!< Negetive: direction->from end to 0
};

//--------------------------------------------------------------------
//! PmAction subclass that move the n'th entry in runlist by relPri.
//!
class PmAction_MoveEntryInRunlist : public PmAction
{
public:
    PmAction_MoveEntryInRunlist(const PmRunlistDesc *pRunlistDesc,
                                UINT32 entryIndex, int relPri);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmRunlistDesc *m_pRunlistDesc; //!< Preempted runlist to move entries
    UINT32               m_EntryIndex;   //!< Index of the entry to be removed
    int                  m_RelPri;       //!< Move forward or backward m_RelPri
};

//--------------------------------------------------------------------
//! PmAction subclass that move n'th entry matching indicated channel by relPri.
//!
class PmAction_MoveChannelInRunlist : public PmAction
{
public:
    PmAction_MoveChannelInRunlist(const PmChannelDesc *pChannelDesc,
                                  UINT32 entryIndex, int relPri);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Channels to be removed
    UINT32               m_EntryIndex;   //!< n'th entry matching indicated channel
    int                  m_RelPri;       //!< Move forward or backward m_RelPri
};

//--------------------------------------------------------------------
//! PmAction subclass that resubmits the indicated runlist
//!
class PmAction_ResubmitRunlist : public PmAction
{
public:
    PmAction_ResubmitRunlist(const PmRunlistDesc *pRunlistDesc,
                             const PmSmcEngineDesc *pSmcEngineDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmRunlistDesc *m_pRunlistDesc;
    const PmSmcEngineDesc *m_pSmcEngineDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that freezes runlists
//!
class PmAction_FreezeRunlist : public PmAction
{
public:
    PmAction_FreezeRunlist(const PmRunlistDesc *pRunlistDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
    virtual RC EndTest();

private:
    const PmRunlistDesc *m_pRunlistDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that unfreezes runlists
//!
class PmAction_UnfreezeRunlist : public PmAction
{
public:
    PmAction_UnfreezeRunlist(const PmRunlistDesc *pRunlistDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmRunlistDesc *m_pRunlistDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that writes entries to frozen runlists
//!
class PmAction_WriteRunlistEntries : public PmAction
{
public:
    PmAction_WriteRunlistEntries(const PmRunlistDesc *pRunlistDesc,
                                 UINT32 NumEntries);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmRunlistDesc *m_pRunlistDesc;
    UINT32 m_NumEntries;
};

//--------------------------------------------------------------------
//! PmAction subclass that flushes the indicated channel(s)
//!
class PmAction_Flush : public PmAction
{
public:
    PmAction_Flush(const PmChannelDesc *pChannelDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that resets the indicated channel(s)
//!
class PmAction_ResetChannel : public PmAction
{
public:
    PmAction_ResetChannel(const PmChannelDesc *pChannelDesc,
                          const string &EngineName);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Channel(s) to reset
    string m_EngineName;                 //!< Engine to reset.
};

//--------------------------------------------------------------------
//! PmAction subclass that unbinds & rebinds channel(s)
//!
class PmAction_UnbindAndRebindChannel : public PmAction
{
public:
    PmAction_UnbindAndRebindChannel(const PmChannelDesc *pChannelDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Channels to unbind/rebind
};

//--------------------------------------------------------------------
//! PmAction subclass that disables a channel(s)
//!
class PmAction_DisableChannel : public PmAction
{
public:
    PmAction_DisableChannel(const PmChannelDesc *pChannelDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Channels to disable
};

//--------------------------------------------------------------------
//! PmAction subclass that Pauses/Unpause channel(s)
//  By 'pausing' we mean disables flushes
//!
class PmAction_BlockChannelFlush : public PmAction
{
public:
    PmAction_BlockChannelFlush(const PmChannelDesc *pChannelDesc,
                               bool Blocked);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Channels to pause/unpause
    bool  m_Blocked;
};

//--------------------------------------------------------------------
//! Base class for PmAction subclasses that take a PmSurfaceDesc and
//! modify some specific PTE flag(s) in all matching surfaces.
//!
class PmAction_ModifyPteFlagsInSurfaces : public PmAction
{
public:
    PmAction_ModifyPteFlagsInSurfaces(
        const string &Name,
        const PmSurfaceDesc *pSurfaceDesc,
        PolicyManager::PageSize PageSize,
        UINT32 PteFlags,
        UINT32 PteMask,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        PolicyManager::AddressSpaceType TargetVaSpace,
        bool DeferTlbIlwalidate);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmSurfaceDesc *m_pSurfaceDesc;
    PolicyManager::PageSize m_PageSize;
    UINT32 m_PteFlags;
    UINT32 m_PteMask;
    bool m_IsInband;
    bool m_DeferTlbIlwalidate;
    PmChannelDesc *m_pInbandChannel;
    PolicyManager::AddressSpaceType m_TargetVASpace;
};

//--------------------------------------------------------------------
//! Base class for PmAction subclasses that take a (PmSurfaceDesc,
//! start, size) tuple and modify some specific PTE flag(s) in all
//! matching pages.
//!
class PmAction_ModifyMMULevelFlagsInPages : public PmAction
{
public:
    PmAction_ModifyMMULevelFlagsInPages(
        const string &Name,
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 Offset,
        UINT64 Size,
        PolicyManager::PageSize PageSize,
        UINT32 PteFlags,
        UINT32 PteMask,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        PolicyManager::AddressSpaceType TargetVaSpace,
        PolicyManager::Level LevelType,
        bool DeferTlbIlwalidate);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmPageDesc m_PageDesc;
    PolicyManager::PageSize m_PageSize;
    UINT32 m_PteFlags;
    UINT32 m_PteMask;
    bool m_IsInband;
    bool m_DeferTlbIlwalidate;
    PmChannelDesc *m_pInbandChannel;
    PolicyManager::AddressSpaceType m_TargetVASpace;
    PolicyManager::Level m_TargetMmuLevelType;
};

//--------------------------------------------------------------------
//! PmAction subclass that unmaps a set of surfaces
//!
class PmAction_UnmapSurfaces : public PmAction_ModifyPteFlagsInSurfaces
{
public:
    PmAction_UnmapSurfaces(const PmSurfaceDesc *pSurfaceDesc,
                           bool isInband,
                           PmChannelDesc *pInbandChannel,
                           PolicyManager::AddressSpaceType TargetVaSpace,
                           bool DeferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that unmaps pages within some surfaces
//!
class PmAction_UnmapPages : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_UnmapPages(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        PolicyManager::AddressSpaceType TargetVaSpace,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that unmaps pages within some surfaces
//!
class PmAction_UnmapPde : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_UnmapPde(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::Level levelType,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        PolicyManager::AddressSpaceType TargetVaSpace,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that set pages to sparse within some surfaces
//!
class PmAction_SparsifyPages : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SparsifyPages(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        PolicyManager::AddressSpaceType TargetVaSpace,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that remaps a set of surfaces
//!
class PmAction_RemapSurfaces : public PmAction_ModifyPteFlagsInSurfaces
{
public:
    PmAction_RemapSurfaces(const PmSurfaceDesc *pSurfaceDesc,
                           bool isInband,
                           PmChannelDesc *pInbandChannel,
                           PolicyManager::AddressSpaceType TargetVaSpace,
                           bool DeferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that remaps pages within some surfaces
//!
class PmAction_RemapPages : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_RemapPages(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        PolicyManager::AddressSpaceType TargetVaSpace,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that unmaps pages within some surfaces
//!
class PmAction_RemapPde : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_RemapPde(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::Level levelType,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        PolicyManager::AddressSpaceType TargetVaSpace,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that remaps a list of randomly selected pages
//!
class PmAction_RandomRemapIlwalidPages : public PmAction
{
public:
    PmAction_RandomRemapIlwalidPages(const PmSurfaceDesc *pSurfaceDesc,
                                     bool isInband,
                                     PmChannelDesc *pInbandChannel,
                                     PolicyManager::AddressSpaceType TargetVaSpace,
                                     const FancyPicker& percentagePicker,
                                     UINT32 randomSeed,
                                     bool deferTlbIlwalidate);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    PmMemRanges GetRandomMappingRanges(const PmSubsurfaces& surfaces,
                                       UINT32 percentage);

    const PmSurfaceDesc             *m_pSurfaceDesc;
    bool                             m_IsInband;
    bool                             m_DeferTlbIlwalidate;
    PmChannelDesc                   *m_pInbandChannel;
    PolicyManager::AddressSpaceType  m_TargetVASpace;
    FancyPicker                      m_PercentagePicker; //!< percentage number generator
    FancyPicker::FpContext           m_FpContext;
    Random                           m_Rand; //!< psuedo-random number generator
                                             //!< used to decide map or not
    UINT32                           m_RandomSeed;
};

//--------------------------------------------------------------------
//! PmAction subclass that changes pagesize of surface pages
//!
class PmAction_ChangePageSize : public PmAction
{
public:
    PmAction_ChangePageSize(
        const PmSurfaceDesc *pSurfaceDesc,
        PolicyManager::PageSize targetPageSize,
        UINT64 offset,
        UINT64 size,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        PolicyManager::AddressSpaceType targetVaSpace,
        bool deferTlbIlwalidate);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    const PmPageDesc m_PageDesc;
    PolicyManager::PageSize m_TargetPageSize;
    bool m_IsInband;
    bool m_DeferTlbIlwalidate;
    PmChannelDesc *m_pInbandChannel;
    PolicyManager::AddressSpaceType m_TargetVASpace;
};

//--------------------------------------------------------------------
//! PmAction subclass that remaps the faulting surface
//!
class PmAction_RemapFaultingSurface : public PmAction
{
public:
    PmAction_RemapFaultingSurface(bool isInband, PmChannelDesc *pInbandChannel, bool deferTlbIlwalidate);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    bool m_IsInband;
    bool m_DeferTlbIlwalidate;
    PmChannelDesc *m_pInbandChannel;
};

//--------------------------------------------------------------------
//! PmAction subclass that remaps the faulting page
//!
class PmAction_RemapFaultingPage : public PmAction
{
public:
    PmAction_RemapFaultingPage(bool isInband, PmChannelDesc *pInbandChannel, bool deferTlbIlwalidate);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    bool m_IsInband;
    bool m_DeferTlbIlwalidate;
    PmChannelDesc *m_pInbandChannel;
};

//--------------------------------------------------------------------
//! PmAction subclass that creates a vaSpace
//!
class PmAction_CreateVaSpace : public PmAction
{
public:
    PmAction_CreateVaSpace(const string &vaSpaceName,
                           const PmGpuDesc *pGpuDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    string           m_VaSpaceName; //!< VaSpace name
    const PmGpuDesc *m_pGpuDesc; //!< GpuDevice(s) to create vaSpace(s) on
};

//--------------------------------------------------------------------
//! PmAction subclass that creates a surface
//!
class PmAction_CreateSurface : public PmAction
{
public:
    PmAction_CreateSurface(const string &surfaceName, UINT64 size,
                           const PmGpuDesc *pGpuDesc, bool clearFlag,
                           Memory::Location location, bool physContig,
                           UINT32 alignment, bool dualPageSize,
                           PolicyManager::LoopBackPolicy loopback,
                           Surface2D::GpuCacheMode cacheMode,
                           PolicyManager::PageSize pageSize,
                           Surface2D::GpuSmmuMode gpuSmmuMode,
                           Surface2D::VASpace surfAllocType,
                           bool isInband,
                           PmChannelDesc *pInbandChannel,
                           PolicyManager::PageSize physPageSize,
                           const PmVaSpaceDesc * pVaSpaceDesc,
                           const PmSmcEngineDesc *pSmcEngineDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    string        m_SurfaceName; //!< Surface name
    UINT64        m_Size;        //!< Surface size
    const PmGpuDesc *m_pGpuDesc; //!< GpuDevice(s) to create surface(s) on
    bool m_ClearFlag;            //!< Flag to control 0 fill of surface,
                                 //!< default = true (clear surface)
    Memory::Location m_Location; //!< Where to create surface (fb, coh, noncoh)
    bool m_PhysContig;           //!< Whether surface is physically contiguous
    UINT32 m_Alignment;          //!< If nonzero, force surface alignment
    bool m_DualPageSize;         //!< Use both big and small PTEs
    PolicyManager::LoopBackPolicy m_LoopBack; //!< Whether to allocate the
                                              //!< surface in loopback
    Surface2D::GpuCacheMode m_CacheMode;      //!< GPU cacheing mode
    PolicyManager::PageSize m_PageSize;       //!< Force big or small pages
    Surface2D::VASpace m_SurfaceAllocType;    //!< Surface VA space
    Surface2D::GpuSmmuMode m_GpuSmmuMode;     //!< Force Smmu enable on Gpu surface

    bool m_IsInband;                 //!< Use in-band methods to clear surface
    PmChannelDesc *m_pInbandChannel; //!< In-band methods channel
    UINT32 m_PhysPageSize; //!< physical address page size
    const PmVaSpaceDesc * m_pVaSpaceDesc; //!< virtual address space name
    const PmSmcEngineDesc * m_pSmcEngineDesc;    //!< SmcEngine
};

//--------------------------------------------------------------------
//! PmAction subclass that moves a set of surfaces to a new physical
//! address
//!
class PmAction_MoveSurfaces : public PmAction
{
public:
    PmAction_MoveSurfaces(const PmSurfaceDesc *pSurfaceDesc,
                          Memory::Location location,
                          PolicyManager::MovePolicy movePolicy,
                          PolicyManager::LoopBackPolicy loopback,
                          bool DeferTlbIlwalidate,
                          bool isInband,
                          PmChannelDesc *pInbandChannel,
                          bool MoveSurfInDefaultMMU,
                          Surface2D::VASpace surfAllocType,
                          Surface2D::GpuSmmuMode surfGpuSmmuMode,
                          bool DisablePostL2Compression);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmSurfaceDesc      *m_pSurfaceDesc; //!< Surfaces to move
    Memory::Location          m_Location;     //!< Where to create surface
    PolicyManager::MovePolicy m_MovePolicy;   //!< What to do with old phys mem
    PolicyManager::LoopBackPolicy m_LoopBack; //!< Whether to allocate the
                                              //!< surface in loopback
    bool                      m_DeferTlbIlwalidate; //!< Don't ilwalidate TLBs during Execute
    bool                      m_IsInband;     //!< Move surfaces in-band or out-of-band
    PmChannelDesc            *m_pInbandChannel; //!< Channel to use for in-band surface moving
    Surface2D::VASpace        m_SrcSurfAllocType;     //!< Donating surface type
    Surface2D::GpuSmmuMode    m_SrcSurfGpuSmmuMode;   //!< Donating surface smmu mode
    bool                      m_MoveSurfInDefaultMMU; //! Move surface in default mmu
    bool                      m_DisablePostL2Compression;  //!< Disable Post L2 Compression
};

//--------------------------------------------------------------------
//! PmAction subclass that moves pages to a new physical address
//!
class PmAction_MovePages : public PmAction
{
public:
    PmAction_MovePages(const string& name,
                       const PmSurfaceDesc *pSrcSurfaceDesc,
                       UINT64 srcOffset, UINT64 size,
                       Memory::Location location,
                       PolicyManager::MovePolicy movePolicy,
                       PolicyManager::LoopBackPolicy loopback,
                       bool isInband,
                       PmChannelDesc *pInbandChannel,
                       bool MoveSurfInDefaultMMU,
                       Surface2D::VASpace surfAllocType,
                       Surface2D::GpuSmmuMode surfGpuSmmuMode,
                       const PmSurfaceDesc * pDestSurfaceDesc,
                       UINT64 destOffset,
                       bool deferTlbIlwalidate,
                       bool disablePostL2Compression);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmPageDesc          m_PageDesc;   //!< Pages to move
    Memory::Location          m_Location;   //!< Where to create pages
    PolicyManager::MovePolicy m_MovePolicy; //!< What to do with old phys mem
    PolicyManager::LoopBackPolicy m_LoopBack; //!< Whether to allocate the
                                              //!< surface in loopback
    bool                      m_IsInband;     //!< Move surfaces in-band or out-of-band
    PmChannelDesc            *m_pInbandChannel; //!< Channel to use for in-band surface moving
    Surface2D::VASpace        m_SrcSurfAllocType;     //!< Donating surface type
    Surface2D::GpuSmmuMode    m_SrcSurfGpuSmmuMode;   //!< Donating surface smmu mode
    bool                      m_MoveSurfInDefaultMMU; //! Move surface in default mmu
    PmPageDesc               *m_DestPageDesc; //!< Move to the destination surface
    bool                      m_DisablePostL2Compression;  //!< Disable Post L2 Compression
};

//--------------------------------------------------------------------
//! PmAction subclass that moves the faulting surface to a new
//! physical address
//!
class PmAction_MoveFaultingSurface : public PmAction
{
public:
    PmAction_MoveFaultingSurface(Memory::Location location,
                                 PolicyManager::MovePolicy movePolicy,
                                 PolicyManager::LoopBackPolicy loopback,
                                 bool isInband,
                                 PmChannelDesc *pInbandChannel,
                                 bool deferTlbIlwalidate,
                                 bool disablePostL2Compression);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    Memory::Location          m_Location;   //!< Where to create surface
    PolicyManager::MovePolicy m_MovePolicy; //!< What to do with old phys mem
    PolicyManager::LoopBackPolicy m_LoopBack; //!< Whether to allocate the
                                              //!< surface in loopback
    bool                      m_IsInband;     //!< Move surfaces in-band or out-of-band
    bool                      m_DeferTlbIlwalidate;
    PmChannelDesc            *m_pInbandChannel; //!< Channel to use for in-band surface moving
    bool                      m_DisablePostL2Compression;  //!< Disable Post L2 Compression
};

//--------------------------------------------------------------------
//! PmAction subclass that moves the faulting page to a new physical
//! address
//!
class PmAction_MoveFaultingPage : public PmAction
{
public:
    PmAction_MoveFaultingPage(Memory::Location location,
                              PolicyManager::MovePolicy movePolicy,
                              PolicyManager::LoopBackPolicy loopback,
                              bool isInband,
                              PmChannelDesc *pInbandChannel,
                              bool deferTlbIlwalidate,
                              bool disablePostL2Compression);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    Memory::Location          m_Location;   //!< Where to create pages
    PolicyManager::MovePolicy m_MovePolicy; //!< What to do with old phys mem
    PolicyManager::LoopBackPolicy m_LoopBack; //!< Whether to allocate the
                                              //!< surface in loopback
    bool                      m_IsInband;     //!< Move surfaces in-band or out-of-band
    bool                      m_DeferTlbIlwalidate;
    PmChannelDesc            *m_pInbandChannel; //!< Channel to use for in-band surface moving
    bool                      m_DisablePostL2Compression;  //!< Disable Post L2 Compression
};

//--------------------------------------------------------------------
//! PmAction subclass that implements a "goto" in the action block.
//! Used to implement conditional blocks.
//!
class PmAction_Goto : public PmAction
{
public:
    PmAction_Goto(const string& name);
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
    virtual INT32 GetNextAction() { return m_NextAction; }

    void SetSource(size_t source) { m_Source = source; }
    void SetTarget(size_t target) { m_Target = target; }

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pGoto) = 0;

private:
    INT32  m_NextAction;
    size_t m_Source;
    size_t m_Target;
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes when
//! the current trigger gets triggered "N" times.
//!
class PmAction_OnTriggerCount : public PmAction_Goto
{
public:
    PmAction_OnTriggerCount(const FancyPicker& fancyPicker,
                            bool               bUseSeedString,
                            UINT32             randomSeed);
    virtual ~PmAction_OnTriggerCount();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    typedef map<const PmTrigger*, PmPickCounter *> PickCounterMap;
    PickCounterMap  m_Pickers; //!< Per-event pick counters
    FancyPicker     m_FancyPicker;
    bool            m_bUseSeedString;//!< true if the seed should come from
                                     //!< a string instead of m_RandomSeed
    UINT32          m_RandomSeed;    //!< Random seed for the pick counters
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes on
//! the indicated test number(s).
//!
class PmAction_OnTestNum : public PmAction_Goto
{
public:
    PmAction_OnTestNum(const FancyPicker& fancyPicker,
                       bool               bUseSeedString,
                       UINT32             randomSeed);
    virtual ~PmAction_OnTestNum();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    FancyPicker    m_FancyPicker;
    PmPickCounter *m_pPickCounter;
    bool           m_bUseSeedString;//!< true if the seed should come from
                                    //!< a string instead of m_RandomSeed
    UINT32         m_RandomSeed;    //!< Random seed for the pick counters
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes on
//! the indicated test id(s).
//!
class PmAction_OnTestId : public PmAction_Goto
{
public:
    PmAction_OnTestId(UINT32 testId);
    virtual ~PmAction_OnTestId();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    UINT32 m_TestId;
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for an else block that is skipped when
//! a preceding conditional block exelwtes.
//!
class PmAction_Else : public PmAction_Goto
{
public:
    PmAction_Else();
    virtual ~PmAction_Else();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes if
//! one of the designated surfaces is faulting.
//!
class PmAction_OnSurfaceIsFaulting : public PmAction_Goto
{
public:
    PmAction_OnSurfaceIsFaulting(const PmSurfaceDesc *pSurfaceDesc);
    virtual ~PmAction_OnSurfaceIsFaulting();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    const PmSurfaceDesc *m_pSurfaceDesc;
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes if
//! the page is faulting.
//!
class PmAction_OnPageIsFaulting : public PmAction_Goto
{
public:
    PmAction_OnPageIsFaulting(const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset, UINT64 size);
    virtual ~PmAction_OnPageIsFaulting();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    const PmPageDesc m_PageDesc;
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes if
//! the user specified fault type matches the fault type of the of
//! the current replayable fault event.
//!
class PmAction_OnFaultTypeMatchesFault: public PmAction_Goto
{
public:
    PmAction_OnFaultTypeMatchesFault(string faultType);
    virtual ~PmAction_OnFaultTypeMatchesFault();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);
 private:
    void ParseFaultType(string faultType);

    UINT32 m_faultType;
    enum FaultType
    {
        PDE,
        PDE_SIZE,
        PTE,
        VA_LIMIT_VIOLATION,
        UNBOUND_INST_BLOCK,
        PRIV_VIOLATION,
        RO_VIOLATION,
        PITCH_MASK_VIOLATION = 0x00000008,
        WORK_CREATION,
        UNSUPPORTED_APERTURE,
        COMPRESSION_FAILURE,
        UNSUPPORTED_KIND,
        REGION_VIOLATION,
        POISONED,
        ATOMIC_VIOLATION,
        UNSUPPORTED,
    };
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes if
//! the user specified client id matches the fault type of the of
//! the current replayable fault event.
//!
class PmAction_OnClientIDMatchesFault : public PmAction_Goto
{
public:
    PmAction_OnClientIDMatchesFault(UINT32 clientID);
    virtual ~PmAction_OnClientIDMatchesFault();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    const UINT32 m_clientID;
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes if
//! the user specified gpc id matches the fault type of the of
//! the current replayable fault event.
//!
class PmAction_OnGPCIDMatchesFault : public PmAction_Goto
{
public:
    PmAction_OnGPCIDMatchesFault(UINT32 GPCID);
    virtual ~PmAction_OnGPCIDMatchesFault();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    const UINT32 m_GPCID;
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes if
//! one of the designated pages is sending the notification.
//!
class PmAction_OnNotificationFromPage : public PmAction_Goto
{
public:
    PmAction_OnNotificationFromPage(const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset, UINT64 size);
    virtual ~PmAction_OnNotificationFromPage();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    const PmPageDesc m_PageDesc;
};

//--------------------------------------------------------------------
//! PmAction_Goto subclass for a conditional block that exelwtes if
//! the user specified VEID in this tsg matches the fault type of the
//! the current replayable fault event.
//!
class PmAction_OlwEIDMatchesFault : public PmAction_Goto
{
public:
    PmAction_OlwEIDMatchesFault(const string & tsgName, const UINT32 VEID, const PmSmcEngineDesc *pSmcEngineDesc);
    virtual ~PmAction_OlwEIDMatchesFault();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);

private:
    const string    m_TsgName;
    const UINT32    m_VEID;
    const PmSmcEngineDesc *m_pSmcEngineDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that gets inserted at the beginning and end of
//! ActionBlock.AllowOverlappingTriggers.
//!
class PmAction_AllowOverlappingTriggers : public PmAction
{
public:
    PmAction_AllowOverlappingTriggers(bool Allow);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    bool m_Allow;  //!< True at start of ActionBlock.AllowOverlappingTriggers,
                   //!< false at end
};

//--------------------------------------------------------------------
//! PmAction subclass that writes a non-stalling interrupt on the channel
//!
class PmAction_NonStallInt : public PmAction
{
public:
    PmAction_NonStallInt(const string &intName,
                         const PmChannelDesc *pChannelDesc,
                         bool enableSemaphore, bool enableInt);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    string m_IntName;
    const PmChannelDesc *m_pChannelDesc;
    bool m_EnableSemaphore;
    bool m_EnableInt;
};

//--------------------------------------------------------------------
//! PmAction subclass that writes an engine trap on the channel
//!
class PmAction_EngineNonStallInt : public PmAction
{
public:
    PmAction_EngineNonStallInt(const string &intName,
                               const PmChannelDesc *pChannelDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    string m_IntName;
    const PmChannelDesc *m_pChannelDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that writes random methods on the channel
//!
class PmAction_InsertMethods : public PmAction
{
public:
    PmAction_InsertMethods(PmChannelDesc *pChannelDesc,
                           UINT32 subch,
                           UINT32 numMethods,
                           const FancyPicker& methodPicker,
                           const FancyPicker& dataPicker,
                           UINT32 sudevMask);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
    enum {
        LWRRENT_SUBCH = 0xffffffff // m_Subch value meaning "whatever
                                   // subch was used most recently"
    };

private:
    const PmChannelDesc *m_pChannelDesc;
    UINT32 m_Subch;
    UINT32 m_NumMethods;
    FancyPicker  m_MethodPicker;
    FancyPicker  m_DataPicker;
    FancyPicker::FpContext m_FpContext;
    UINT32 m_SubdevMask;
};

//--------------------------------------------------------------------
//! PmAction subclass that writes a method interrupt on the channel
//!
class PmAction_MethodInt : public PmAction
{
public:
    PmAction_MethodInt(bool bWfiOnRelease, bool bWaitEventHandled);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    bool m_WfiOnRelease;    // params to call RequestInterrupt()
    bool m_WaitEventHandled;// params to call RequestInterrupt()
};

//--------------------------------------------------------------------
//! PmAction subclass that ilwalidates the TLB entries
//!
class PmAction_TlbIlwalidate : public PmAction
{
public:
    PmAction_TlbIlwalidate(
        const string & name,
        const PmGpuDesc *pGpuDesc,
        const PmSurfaceDesc *pSurfaceDesc,
        bool IlwalidateGpc,
        PolicyManager::TlbIlwalidateReplay IlwalidateReplay,
        bool IlwalidateSysmembar,
        PolicyManager::TlbIlwalidateAckType IlwalidateAckType,
        bool Inband,
        PmChannelDesc *pInbandChannel,
        UINT32 subdevMask,
        PolicyManager::AddressSpaceType TargetVASpace,
        FancyPicker GPCID,
        FancyPicker ClientUnitID,
        PmChannelDesc * pPdbTargetChannel,
        bool TlbIlwalidateBar1,
        bool TlbIlwalidateBar2,
        PolicyManager::Level TlbIlwalidateLevelNum,
        PolicyManager::TlbIlwalidateIlwalScope TlbIlwalidateLevelLinkTlb);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

protected:
    void SetFaultingGpcId(PmTrigger *pTrigger, const PmEvent *pEvent);
    void SetFaultingClientId(PmTrigger *pTrigger, const PmEvent *pEvent);
    // in band
    virtual RC ExelwteInband(PmTrigger *pTrigger, const PmEvent *pEvent);
    PmChannel * GetIlwalidateChannel(PmTrigger * pTrigger, const PmEvent * pEvent);
    RC SetInbandGPC(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD);
    RC SetInbandSysmemBar(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD);
    RC SetInbandAckType(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD);
    RC GetPdbConfigure(GpuSubdevice * pGpuSubDevice, PmChannel * pPdbTargetChannel, UINT64 * pdbPhyAddrAlign, PmSubsurface * pSurface, UINT32 * pdbAperture, LwRm* pLwRm);
    RC SetInbandPdb(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD, GpuSubdevice * pGpuSubDevice, PmChannel * pPdbTargetChannel, PmSubsurface * pSurface, LwRm* pLwRm);
    RC SetInbandReplay(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD);
    RC CheckConfigureLevel(GpuSubdevice * pGpuSubDevice, bool * isValid, LwRm* pLwRm) const;
    RC SetInbandLevel(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD, GpuSubdevice * pGpuSubDevice, LwRm* pLwRm);
    RC SetInbandIlwalScope(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD);
    virtual RC SetOperation(UINT32 * memOpC, UINT32 * memOpD);

    // out of band
    virtual RC ExelwteOutOfBand(PmTrigger *pTrigger, const PmEvent *pEvent);
    RC RealExelwteOutOfBand(GpuSubdevice * pGpuSubDevice, PmChannel * pPdbTargetChannel, PmSubsurface * pSubsurface);
    virtual RC SetOutOfBandLevel(GpuSubdevice * pGpuSubDevice, UINT32 * value, LwRm* pLwRm);
    virtual RC SetOutOfBandLinkTlb(GpuSubdevice * pGpuSubDevice, UINT32 * value);
    RC SetOutOfBandPdb(UINT32 * valueCommand, GpuSubdevice * pGpuSubDevice, PmChannel * pPdbTargetChannel, PmSubsurface * pSurface, LwRm* pLwRm);
    RC SetOutOfBandSysmembar(UINT32 * value, GpuSubdevice * pGpuSubDevice);
    RC SetOutOfBandReplay(UINT32 * value, GpuSubdevice * pGpuSubDevice, LwRm* pLwRm);
    RC SetOutOfBandGPC(UINT32 * value);
    RC SetOutOfBandAckType(UINT32 * value);
    RC SetOutOfBandTrigger(UINT32 * value);
    virtual RC SetOutOfBandVA(UINT32 * value, PmSubsurface * PmSubsurface, GpuSubdevice * pGpuSubdevice);
    static bool CallbackPollFunction(void * pCallbackPollData);
    static bool CallbackPollFifoEmptyFunction(void * pCallbackPollData);

    const PmGpuDesc *m_pGpuDesc; //!< Gpu's to ilwalidate the TLB on
    const PmSurfaceDesc *m_pSurfaceDesc; //!< Gpu's to ilwalidate the targeted surface
    bool m_IlwalidateGpc;        //!< Ilwalidate GPC-MMU TLBs?
    PolicyManager::TlbIlwalidateReplay m_IlwalidateReplay; //!< Start/cancel replay?
    bool m_IlwalidateSysmembar;  //!< Enable/disable sysmembar
    PolicyManager::TlbIlwalidateAckType m_IlwalidateAckType; //!< Ilwalidate ack type
    bool m_Inband;               //!< Ilwalidate the TLB in-band or out-of-band
    PmChannelDesc *m_pInbandChannel; //!< Channel to use for inband exelwtion
    UINT32 m_SubdevMask;         //!< Subdevice mask for the ilwalidate
    PolicyManager::AddressSpaceType m_TargetVASpace;//!< VA space to be ilwalidated
    FancyPicker   m_GPCID;//!< targeted GPC ID
    FancyPicker   m_ClientUnitID;//!< targeted client unit ID
    FancyPicker::FpContext m_GPCFpContext;
    FancyPicker::FpContext m_UnitFpContext;
    PmChannelDesc * m_pPdbTargetChannel;
    bool  m_TlbIlwalidateBar1;
    bool  m_TlbIlwalidateBar2;
    PolicyManager::Level m_TlbIlwalidateLevelNum;
    PolicyManager::TlbIlwalidateIlwalScope m_TlbIlwalidateIlwalScope;
    UINT32 m_FaultingGpcId;
    UINT32 m_FaultingClientId;
};

class PmAction_TargetedTlbIlwalidate : public PmAction_TlbIlwalidate
{
public:
    PmAction_TargetedTlbIlwalidate(
        const PmGpuDesc *pGpuDesc,
        bool IlwalidateGpc,
        PolicyManager::TlbIlwalidateReplay IlwalidateReplay,
        bool IlwalidateSysmembar,
        PolicyManager::TlbIlwalidateAckType IlwalidateAckType,
        bool Inband,
        PmChannelDesc *pInbandChannel,
        UINT32 subdevMask,
        PolicyManager::AddressSpaceType TargetVASpace,
        FancyPicker GPCID,
        FancyPicker ClientUnitID,
        PmChannelDesc * pPdbTargetChannel,
        bool TlbIlwalidateBar1,
        bool TlbIlwalidateBar2,
        PolicyManager::Level TlbIlwalidateLevelNum,
        PolicyManager::TlbIlwalidateIlwalScope TlbIlwalidateIlwalScope);

private:
    // in band
    virtual RC ExelwteInband(PmTrigger *pTrigger, const PmEvent *pEvent);
    RC SendTlbIlwalidateMethodsLegacy(PmChannel *pChannel);
    RC SendTlbIlwalidateMethodsMaxwell(PmChannel *pChannel);
    RC SendTlbIlwalidateMethodsPascal(PmChannel *pChannel, PmChannel *pPdbTargetChannel,
                                      GpuSubdevice *pGpuSubdevice);
    virtual RC SetOperation(UINT32 * memOpC, UINT32 * memOpD);

    // out of band
    RC ExelwteOutOfBand(PmTrigger *pTrigger, const PmEvent *pEvent);
};

//--------------------------------------------------------------------
//! PmAction subclass that ilwalidate targeted virtual address tlb
//!
class PmAction_TlbIlwalidateVA : public PmAction_TlbIlwalidate
{
public:
    PmAction_TlbIlwalidateVA(
        const PmSurfaceDesc *pSurfaceDesc,
        const UINT64 aOffset,
        bool Inband,
        UINT32 subdevMask,
        PmChannelDesc *pInbandChannel,
        bool IlwalidateGpc,
        PolicyManager::TlbIlwalidateReplay IlwalidateReplay,
        bool IlwalidateSysmembar,
        PolicyManager::TlbIlwalidateAckType IlwalidateAckType,
        PolicyManager::AddressSpaceType TargetVASpace,
        PmChannelDesc * pPdbTargetChannel,
        bool TlbIlwalidateBar1,
        bool TlbIlwalidateBar2,
        PolicyManager::Level TlbIlwalidateLevelNum,
        PolicyManager::TlbIlwalidateIlwalScope TlbIlwalidateIlwalScope,
        FancyPicker GPCID,
        FancyPicker ClientUnitID,
        PolicyManager::TlbIlwalidateAccessType TlbIlwalidateAccessType,
        UINT32 VEID,
        const PmSmcEngineDesc *pSmcEngineDesc);

private:
    // in band
    virtual RC ExelwteInband(PmTrigger *pTrigger, const PmEvent *pEvent);
    RC GetVAConfigure(PmSubsurface * pSurface, UINT64 * virtualAddressAlign);
    RC GetMMUEngineId(GpuSubdevice * pGpuSubdevice);
    RC SetTargetedVA(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD,PmSubsurface * pSubSurface);
    virtual RC SetOperation(UINT32 * memOpC, UINT32 * memOpD);
    RC SetInbandReplay(UINT32 * memOpA, UINT32 * memOpB, UINT32 * memOpC, UINT32 * memOpD, GpuSubdevice * pGpuSubdevice, PmChannel * pChannel);

    // out of band
    RC ExelwteOutOfBand(PmTrigger * PmTrigger, const PmEvent * PmEvent);
    RC SetOutOfBandReplay(UINT32 * value, GpuSubdevice * pGpuSubdevice, LwRm* pLwRm);
    virtual RC SetOutOfBandVA(UINT32 * value, PmSubsurface * PmSubsurface, GpuSubdevice * pGpuSubdevice);

    const UINT64 m_Offset;       //!< Gpus's to ilwalidate the offset in the targeted surface
    UINT32 m_VEID;
    const PmSmcEngineDesc *m_pSmcEngineDesc;
    PmSmcEngine *m_pPmSmcEngine;
    UINT32 m_MMUEngineId;
    PolicyManager::TlbIlwalidateAccessType m_AccessType;
};

//--------------------------------------------------------------------
//! PmAction subclass that wait for all write finished
//!
class PmAction_HostMembar : public PmAction
{
public:
    PmAction_HostMembar(const PmGpuDesc * pGpuDesc,
                        bool isMembar,
                        PmChannelDesc * pInbandChannel,
                        bool inband);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    const PmGpuDesc * m_pGpuDesc;
    bool m_isMembar;             //!< wait for membar or sysmembar write
    PmChannelDesc * m_pInbandChannel; //! < Gpu's ilwalidate channel
    bool m_Inband;
};

//--------------------------------------------------------------------
//! PmAction subclass that wait for all write finished
//!
class PmAction_ChannelWfi : public PmAction
{
public:
    PmAction_ChannelWfi(const PmChannelDesc * PmChannelDesc, PolicyManager::Wfi wfiType);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc * m_pChannelDesc;
    PolicyManager::Wfi m_wfiType;             //!< wait for membar or sysmembar write
};

//--------------------------------------------------------------------
//! PmAction subclass that flushes the L2 cache
//!
class PmAction_L2Flush : public PmAction
{
public:
    PmAction_L2Flush(const PmGpuDesc *pGpuDesc, bool isInband,
                     UINT32 subdevMask);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc; //!< Gpu's to ilwalidate the L2 on
    bool m_Inband;               //!< Flush in-band or out-of-band
    UINT32 m_SubdevMask;         //!< Subdevice mask for the flush
};

//--------------------------------------------------------------------
//! PmAction subclass that ilwalidates the L2 sysmem cache
//!
class PmAction_L2SysmemIlwalidate : public PmAction
{
public:
    PmAction_L2SysmemIlwalidate(const PmGpuDesc *pGpuDesc, bool isInband,
                                UINT32 subdevMask);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc; //!< Gpu's to ilwalidate the L2 on
    bool m_Inband;               //!< Ilwalidate in-band or out-of-band
    UINT32 m_SubdevMask;         //!< Subdevice mask for the ilwalidate
};

//--------------------------------------------------------------------
//! PmAction subclass that ilwalidates the L2 peermem cache
//!
class PmAction_L2PeermemIlwalidate : public PmAction
{
public:
    PmAction_L2PeermemIlwalidate(const PmGpuDesc *pGpuDesc, bool isInband,
                                 UINT32 subdevMask);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc; //!< Gpu's to ilwalidate the L2 on
    bool m_Inband;               //!< Ilwalidate in-band or out-of-band
    UINT32 m_SubdevMask;         //!< Subdevice mask for the ilwalidate
};

//--------------------------------------------------------------------
//! PmAction subclass that L2 wait for sysmem pending reads
//!
class PmAction_L2WaitForSysPendingReads : public PmAction
{
public:
    PmAction_L2WaitForSysPendingReads(const PmGpuDesc *pGpuDesc, bool isInband,
                                 UINT32 subdevMask);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    static bool PollSysPendingRendsFlushDone(void* pPollArgs); // _PENDING && _OUTSTANDING == 0

    const PmGpuDesc *m_pGpuDesc; //!< Gpu's to ilwalidate the L2 on
    bool m_Inband;               //!< Ilwalidate in-band or out-of-band
    UINT32 m_SubdevMask;         //!< Subdevice mask for the ilwalidate
};

//--------------------------------------------------------------------
//! L2Operation base class that ilwalidates or flush the L2 cache
//!
class PmAction_L2Operation : public PmAction
{
public:
    PmAction_L2Operation(const string& name,
            const PmGpuDesc *pGpuDesc,
            UINT32 flags,
            bool isInband,
            UINT32 subdevMask);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc* m_pGpuDesc; //!< Gpu's to ilwalidate or flush the L2 on
    UINT32 m_Flags;
    UINT32 m_SubdevMask;   //!< Subdevice mask for the ilwalidate or flush
    bool m_Inband;     //!< Ilwalidate or flush in-band or out-of-band
};

//--------------------------------------------------------------------
//! PmAction subclass that ilwalidates the vidmem L2 cache
//!
class PmAction_L2VidmemIlwalidate : public PmAction_L2Operation
{
public:
    PmAction_L2VidmemIlwalidate(const PmGpuDesc *pGpuDesc, bool isInband,
                     UINT32 subdevMask);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the read-only bit in pages
//!
class PmAction_SetReadOnly : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetReadOnly(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that clears the read-only bit in pages
//!
class PmAction_ClearReadOnly : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_ClearReadOnly(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the atomic disable bit in pages
//!
class PmAction_SetAtomicDisable : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetAtomicDisable(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that clears the atomic bit in pages
//!
class PmAction_ClearAtomicDisable : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_ClearAtomicDisable(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the read/write-disable bits to default values
//!
class PmAction_SetShaderDefaultAccess : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetShaderDefaultAccess(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the read/write-disable bits
//!
class PmAction_SetShaderReadOnly : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetShaderReadOnly(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the read/write-disable bits
//!
class PmAction_SetShaderWriteOnly : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetShaderWriteOnly(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the read/write-disable bits
//!
class PmAction_SetShaderReadWrite : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetShaderReadWrite(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the PDE's Size field
//!
class PmAction_SetPdeSize : public PmAction
{
public:
    PmAction_SetPdeSize(const PmSurfaceDesc *pSurfaceDesc,
                        UINT64 Offset, UINT64 Size, FLOAT64 PdeSize);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmPageDesc m_PageDesc; //!< Pages to set the PDE size in
    FLOAT64 m_PdeSize; //!< 1.0 for full size, or 0.5, 0.25, or 0.125 for a
                       //!< fraction of full-size
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the PDE's Aperture field
//!
class PmAction_SetPdeAperture : public PmAction
{
public:
    PmAction_SetPdeAperture(const PmSurfaceDesc *pSurfaceDesc,
                            UINT64 Offset, UINT64 Size,
                            PolicyManager::Aperture Aperture,
                            PolicyManager::PageSize PageSize);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmPageDesc m_PageDesc; //!< Pages to set the PDE aperture for
    PolicyManager::Aperture m_Aperture; //!< What to change the aperture to
    PolicyManager::PageSize m_PageSize; //!< Set PDE bits for big or small page
};

//--------------------------------------------------------------------
//! PmAction subclass that sparsify PDE
//!
class PmAction_SparsifyMmuLevel : public PmAction
{
public:
    PmAction_SparsifyMmuLevel(const PmSurfaceDesc *pSurfaceDesc,
                              UINT64 Offset, UINT64 Size,
                              PolicyManager::PageSize PageSize,
                              bool isInband,
                              PmChannelDesc *pInbandChannel,
                              PolicyManager::AddressSpaceType TargetVASpace,
                              PolicyManager::Level targetMmuLevelNum,
                              bool deferTlbIlwalidate);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmPageDesc m_PageDesc; //!< Pages to set the PDE sparse-ness for
    PolicyManager::PageSize m_PageSize; //!< Set PDE bits for big or small page
    bool m_IsInband;                 //!< Use in-band methods to sparsify PDE
    bool m_DeferTlbIlwalidate;
    PmChannelDesc *m_pInbandChannel; //!< In-band methods channel
    PolicyManager::AddressSpaceType m_TargetVASpace; //!< VASpace the PDE is in
    PolicyManager::Level m_TargetMmuLevelType; // MMU level to be sparsed
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the PTE's Aperture field
//!
class PmAction_SetPteAperture : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetPteAperture(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 Offset,
        UINT64 Size,
        PolicyManager::PageSize PageSize,
        PolicyManager::Aperture Aperture,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);

private:
    static UINT32 GetPteFlagsFromAperture(PolicyManager::Aperture Aperture);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the PTE's Volatile (GPU Cacheable) field
//!
class PmAction_SetVolatile : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetVolatile(
        string name,
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 Offset,
        UINT64 Size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool DeferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that clears the PTE's Volatile (GPU Cacheable) field
//!
class PmAction_ClearVolatile : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_ClearVolatile(
        string name,
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 Offset,
        UINT64 Size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool DeferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the PTE's Kind field
//!
class PmAction_SetKind : public PmAction
{
public:
    PmAction_SetKind(const PmSurfaceDesc *pSurfaceDesc,
                     UINT64 Offset, UINT64 Size, UINT32 aKind);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmPageDesc m_PageDesc; //!< Pages to set the PTE kind bit on
    UINT32           m_Kind;     //!< Value to set Kind to
};

//--------------------------------------------------------------------
//! PmAction subclass that ilwalidates the PDB target.
//!
class PmAction_IlwalidatePdbTarget : public PmAction
{
public:
    PmAction_IlwalidatePdbTarget(PmChannelDesc *pChannelDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that reset the engine context(s) on the
//! indicated channel(s) to an invalid value.
//!
class PmAction_ResetEngineContext : public PmAction
{
public:
    PmAction_ResetEngineContext(PmChannelDesc *pChannelDesc,
                                const RegExp &EngineNames);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Which channel(s) to reset
    RegExp m_EngineNames;                //!< Which engine(s) to reset
};

//--------------------------------------------------------------------
//! PmAction subclass that reset the engine context(s) on the
//! indicated channel(s) to an invalid value, without preempting the
//! channel.
//!
class PmAction_ResetEngineContextNoPreempt : public PmAction
{
public:
    PmAction_ResetEngineContextNoPreempt(PmChannelDesc *pChannelDesc,
                                         const RegExp &EngineNames);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Which channel(s) to reset
    RegExp m_EngineNames;                //!< Which engine(s) to reset
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the priv bit in surfaces
//!
class PmAction_SetPrivSurfaces : public PmAction_ModifyPteFlagsInSurfaces
{
public:
    PmAction_SetPrivSurfaces(
        const PmSurfaceDesc *pSurfaceDesc,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool DeferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that clears the priv bit in surfaces
//!
class PmAction_ClearPrivSurfaces : public PmAction_ModifyPteFlagsInSurfaces
{
public:
    PmAction_ClearPrivSurfaces(
        const PmSurfaceDesc *pSurfaceDesc,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool DeferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the priv bit in a set of pages
//!
class PmAction_SetPrivPages : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_SetPrivPages(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that clears the priv bit in a set of pages
//!
class PmAction_ClearPrivPages : public PmAction_ModifyMMULevelFlagsInPages
{
public:
    PmAction_ClearPrivPages(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        PolicyManager::PageSize PageSize,
        bool isInband,
        PmChannelDesc *pInbandChannel,
        bool deferTlbIlwalidate);
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the priv bit in all GPFIFO entries in
//! a channel until otherwise indicated.
//!
class PmAction_SetPrivChannels : public PmAction
{
public:
    PmAction_SetPrivChannels(const PmChannelDesc *pChannelDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc;     //!< Channels to make privileged
};

//--------------------------------------------------------------------
//! PmAction subclass that clears the priv bit in all GPFIFO entries in
//! a channel until otherwise indicated.
//!
class PmAction_ClearPrivChannels : public PmAction
{
public:
    PmAction_ClearPrivChannels(const PmChannelDesc *pChannelDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc;     //!< Channels to make unprivileged
};

//--------------------------------------------------------------------
//! PmAction base class used to do an acquire, release, or reduction
//! on a host semaphore.
//!
class PmAction_HostSemaphoreOperation : public PmAction
{
public:
    PmAction_HostSemaphoreOperation(const string        &name,
                                    const PmChannelDesc *pChannelDesc,
                                    const PmSurfaceDesc *pSurfaceDesc,
                                    const FancyPicker&   offsetPicker,
                                    const FancyPicker&   payloadPicker,
                                    UINT32               subdevMask,
                                    const PmGpuDesc     *pRemoteGpuDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

protected:
    virtual void SetSemaphoreFlags(PmChannel *pChannel) {}
    virtual RC DoSemaphoreOperation(PmChannel *pChannel, UINT64 Payload) = 0;

private:
    const PmChannelDesc   *m_pChannelDesc;   //!< Channel(s) to do the op on
    const PmSurfaceDesc   *m_pSurfaceDesc;   //!< Semaphore surfaces(s)
    FancyPicker            m_OffsetPicker;   //!< Picker for offset
    FancyPicker            m_PayloadPicker;  //!< Picker for payload
    FancyPicker::FpContext m_FpContext;      //!< Context for pickers
    UINT32                 m_SubdevMask;     //!< Subdevice mask
    const PmGpuDesc       *m_pRemoteGpuDesc; //!< Remote Gpu to use for
                                             //!< surface access
};

//--------------------------------------------------------------------
//! PmAction subclass to acquire a host semaphore.
//!
class PmAction_HostSemaphoreAcquire : public PmAction_HostSemaphoreOperation
{
public:
    PmAction_HostSemaphoreAcquire(const PmChannelDesc *pChannelDesc,
                                  const PmSurfaceDesc *pSurfaceDesc,
                                  const FancyPicker&   offsetPicker,
                                  const FancyPicker&   payloadPicker,
                                  UINT32               subdevMask,
                                  const PmGpuDesc     *pRemoteGpuDesc);

protected:
    virtual RC DoSemaphoreOperation(PmChannel *pChannel, UINT64 Payload);
};

//--------------------------------------------------------------------
//! PmAction subclass to release a host semaphore.
//!
class PmAction_HostSemaphoreRelease : public PmAction_HostSemaphoreOperation
{
public:
    PmAction_HostSemaphoreRelease(const PmChannelDesc *pChannelDesc,
                                  const PmSurfaceDesc *pSurfaceDesc,
                                  const FancyPicker&   offsetPicker,
                                  const FancyPicker&   payloadPicker,
                                  UINT32               subdevMask,
                                  UINT32               flags,
                                  const PmGpuDesc     *pRemoteGpuDesc);

protected:
    virtual void SetSemaphoreFlags(PmChannel *pChannel);
    virtual RC DoSemaphoreOperation(PmChannel *pChannel, UINT64 Payload);

private:
    UINT32 m_SemRelFlags;  //!< The sempahore-release flags
};

//--------------------------------------------------------------------
//! PmAction subclass to do a reduction on a host semaphore.
//!
class PmAction_HostSemaphoreReduction : public PmAction_HostSemaphoreOperation
{
public:
    PmAction_HostSemaphoreReduction(const PmChannelDesc *pChannelDesc,
                                    const PmSurfaceDesc *pSurfaceDesc,
                                    const FancyPicker&   offsetPicker,
                                    const FancyPicker&   payloadPicker,
                                    Channel::Reduction   reduction,
                                    Channel::ReductionType   reductionType,
                                    UINT32               subdevMask,
                                    UINT32               flags,
                                    const PmGpuDesc     *pRemoteGpuDesc);

protected:
    virtual void SetSemaphoreFlags(PmChannel *pChannel);
    virtual RC DoSemaphoreOperation(PmChannel *pChannel, UINT64 Payload);

private:
    Channel::Reduction m_Reduction;  //!< The reduction operation
    Channel::ReductionType m_ReductionType;  //!< The reduction operation type
    UINT32 m_SemRelFlags;            //!< The sempahore-release flags
};

//--------------------------------------------------------------------
//! PmAction subclass that releases/acquires an engine semaphore.
//!
class PmAction_EngineSemaphoreOperation : public PmAction
{
public:
    enum SemaphoreOpType
    {
        SEMAPHORE_OPERATION_RELEASE = 0,
        SEMAPHORE_OPERATION_ACQUIRE
    };

    PmAction_EngineSemaphoreOperation(const PmChannelDesc *pChannelDesc,
                                      const PmSurfaceDesc *pSurfaceDesc,
                                      const FancyPicker&   offsetPicker,
                                      const FancyPicker&   payloadPicker,
                                      UINT32               subdevMask,
                                      UINT32               flags,
                                      const PmGpuDesc     *pRemoteGpuDesc,
                                      SemaphoreOpType      operationType,
                                      const string&        name);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc   *m_pChannelDesc; //!< Channel(s) to acquire on
    const PmSurfaceDesc   *m_pSurfaceDesc; //!< Surfaces(s) to acquire on
    FancyPicker            m_OffsetPicker;  //!< Picker for release offset
    FancyPicker            m_PayloadPicker; //!< Picker for release payload
    FancyPicker::FpContext m_FpContext;    //!< Context for pickers
    UINT32                 m_SubdevMask;   //!< Subdevice mask for the release
    UINT32                 m_SemRelFlags;  //!< Release the sempahore flags
    const PmGpuDesc       *m_pRemoteGpuDesc; //!< Remote Gpu to use for surface
                                             //!< access
    SemaphoreOpType        m_SemaphoreOpType; //!< Release or acquire operation
};

//--------------------------------------------------------------------
//! PmAction subclass that waits for power to be gated, wait time
//! depends upon current exelwtion environment
//!
class PmAction_PowerWait : public PmAction
{
public:
    enum
    {
        DEFAULT_MODEL_WAIT_US = 1,
        DEFAULT_RTL_WAIT_US = 1,
        DEFAULT_HW_WAIT_US = 1000
    };
    PmAction_PowerWait(const UINT32 modelWaitUS,
                       const UINT32 rtlWaitUS,
                       const UINT32 hwWaitUS,
                       const bool   busyWait);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    UINT32 m_ModelWaitUS;
    UINT32 m_RTLWaitUS;
    UINT32 m_HWWaitUS;
    bool   m_bBusyWait;
};

//--------------------------------------------------------------------
//! PmAction subclass performs an escape read (results are processed
//! via the gilder
//!
class PmAction_EscapeRead : public PmAction
{
public:
    PmAction_EscapeRead(const PmGpuDesc *pGpuDesc,
                        const string &path,
                        UINT32 index,
                        UINT32 size);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc;
    string m_Path;
    UINT32 m_Index;
    UINT32 m_Size;
};

//--------------------------------------------------------------------
//! PmAction subclass performs an escape read (results of the read
//! are processed via the policy manager gilder)
//!
class PmAction_EscapeWrite : public PmAction
{
public:
    PmAction_EscapeWrite(const PmGpuDesc *pGpuDesc,
                         const string &path, UINT32 index,
                         UINT32 size, UINT32 data);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc;
    string m_Path;
    UINT32 m_Index;
    UINT32 m_Size;
    UINT32 m_Data;
};

//--------------------------------------------------------------------
//! PmAction subclass sets the performance state on specified GPUs
//!
class PmAction_SetPState : public PmAction
{
public:
    PmAction_SetPState(const PmGpuDesc *pGpuDesc, UINT32 pstate,
                       UINT32 fallback);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc;
    UINT32           m_PState;
    UINT32           m_Fallback;
};

//--------------------------------------------------------------------
//! PmAction subclass that aborts the indicated test(s)
//!
class PmAction_AbortTests : public PmAction
{
public:
    PmAction_AbortTests(PmTestDesc *pTestDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    PmTestDesc *m_pTestDesc;   //!< The test(s) to abort
};

//--------------------------------------------------------------------
//! PmAction subclass that aborts the faulting test
//!
class PmAction_AbortTest : public PmAction
{
public:
    PmAction_AbortTest();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
};

//--------------------------------------------------------------------
//! PmAction subclass that waits for a semaphore release to occur on
//! the specified surface(s).
//!
class PmAction_WaitForSemaphoreRelease : public PmAction
{
public:
    PmAction_WaitForSemaphoreRelease(const PmSurfaceDesc *pSurfaceDesc,
                                     UINT32               offset,
                                     UINT64               payload,
                                     FLOAT64              timeoutMs);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    //! Structure containing data necessary for polling for the released
    //! value
    typedef struct
    {
        PmMemRanges   memRanges;  //!< Memory ranges to wait for release on
        GpuSubdevices subdevices; //!< GPU subdevices to wait for release on
        UINT64        payload;    //!< Value to wait for
        RC            waitRc;     //!< RC returned from the wait if an error
                                  //!< oclwrred
    } PollSemReleaseData;
    static bool PollSemRelease(void *pvArgs);

    const PmPageDesc     m_PageDesc;     //!< Matching pages
    const PmGpuDesc     *m_pGpuDesc;     //!< Matching gpus
    UINT64               m_Payload;      //!< Matching payload
    FLOAT64              m_TimeoutMs;    //!< Timeout on the value wait
};

//--------------------------------------------------------------------
//! PmAction subclass that freezes host during a test run.
//!
class PmAction_FreezeHost : public PmAction
{
public:
    PmAction_FreezeHost(const PmGpuDesc *pGpuDesc, FLOAT64 timeoutMs);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
    virtual RC EndTest();

private:
    const PmGpuDesc     *m_pGpuDesc;     //!< Matching gpus
    FLOAT64              m_TimeoutMs;    //!< Timeout on the value wait
    static bool PollFreezeDone(void* pvArgs);
};

//--------------------------------------------------------------------
//! PmAction subclass that unfreezes host during a test run.
//!
class PmAction_UnFreezeHost : public PmAction
{
public:
    PmAction_UnFreezeHost(const PmGpuDesc *pGpuDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    const PmGpuDesc     *m_pGpuDesc;     //!< Matching gpus
};

//--------------------------------------------------------------------
//! PmAction subclass that resets the GPU device
//!
class PmAction_ResetGpuDevice : public PmAction
{
public:
    PmAction_ResetGpuDevice(const string &Name, const PmGpuDesc *pGpuDesc,
                            UINT32 lw0080CtrlBifResetType);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc;     //!< GpuDevice(s) to reset
    UINT32 m_lw0080CtrlBifResetType; //!< Type of reset
};

//--------------------------------------------------------------------
//! PmAction subclass that writes data to specified physical address.
//!
class PmAction_PhysWr32 : public PmAction
{
public:
    PmAction_PhysWr32(UINT64 physAddr, UINT32 data);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    UINT64 m_PhysAddr;
    UINT32 m_Data;
};

//--------------------------------------------------------------------
//! PmAction_PriAccess
//! Base class for PRI register access
class PmAction_PriAccess : public PmAction
{
public:
    PmAction_PriAccess(const PmGpuDesc *pGpuDesc, const PmSmcEngineDesc *pSmcEngineDesc, const PmChannelDesc *pChannelDesc,
                       const string& RegName, const vector<UINT32>& RegIndexes, const string& regSpace, const string& ActionName);
    unique_ptr<PmRegister> GetPmRegister(PmTrigger *pTrigger, const PmEvent *pEvent ,GpuSubdevice *pSubdev);

protected:
    const PmGpuDesc       *m_pGpuDesc;           //!< Matching gpus
    const PmSmcEngineDesc *m_pSmcEngineDesc;     //!< Matching SmcEngine
    const PmChannelDesc   *m_pChannelDesc;       //!< Matching Channel
    string                m_PriRegName;
    vector<UINT32>        m_PriRegIndexes;
    string                m_PriRegSpace;
};

//--------------------------------------------------------------------
//! PmAction subclass that writes data to PRI registers.
//!
class PmAction_PriWriteReg32 : public PmAction_PriAccess
{
public:
    PmAction_PriWriteReg32(const PmGpuDesc *pGpuDesc, const PmSmcEngineDesc *pSmcEngineDesc, const PmChannelDesc *pChannelDesc,
                           const string& RegName, const vector<UINT32>& RegIndexes,
                           UINT32 RegVal, const string& regSpace);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    UINT32                m_RegVal;
};

//--------------------------------------------------------------------
//! PmAction subclass that writes data to PRI registers field.
//!
class PmAction_PriWriteField32 : public PmAction_PriAccess
{
public:
    PmAction_PriWriteField32(const PmGpuDesc *pGpuDesc, const PmSmcEngineDesc *pSmcEngineDesc, const PmChannelDesc *pChannelDesc,
                             const string& RegName, const vector<UINT32>& RegIndexes,
                             string RegFieldName, UINT32 FieldVal, const string& regSpace);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    string                m_FieldName;
    UINT32                m_FieldVal;
};

//--------------------------------------------------------------------
//! PmAction subclass that writes masked data to PRI registers
//! Intended for writing registers with unnamed fields
//!
class PmAction_PriWriteMask32 : public PmAction_PriAccess
{
public:
    PmAction_PriWriteMask32(const PmGpuDesc *pGpuDesc, const PmSmcEngineDesc *pSmcEngineDesc, const PmChannelDesc *pChannelDesc,
                             const string& RegName, const vector<UINT32>& RegIndexes,
                             UINT32 AndMask, UINT32 RegVal, const string& regSpace);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    UINT32                m_AndMask;
    UINT32                m_RegVal;
};


//--------------------------------------------------------------------
//! PmAction subclass that waits for a pri register to be a specific
//! value.
//!
class PmAction_WaitPriReg32 : public PmAction_PriAccess
{
public:
    PmAction_WaitPriReg32(const PmGpuDesc *pGpuDesc, const PmSmcEngineDesc *pSmcEngineDesc, const PmChannelDesc *pChannelDesc,
                          const string& RegName, const vector<UINT32>& RegIndexes,
                          UINT32 RegMask, UINT32 RegVal, FLOAT64 TimeoutMs, const string& regSpace);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    static bool PollRegs(void *pvArgs);

    UINT32                m_RegMask;
    UINT32                m_RegVal;
    FLOAT64               m_TimeoutMs;
    vector< tuple<LWGpuResource*, UINT32, UINT32, PmSmcEngine*> > m_WaitData;
};

//--------------------------------------------------------------------
//! PmAction subclass that enables/disables the indicated engine
//!
class PmAction_EnableEngine : public PmAction_PriAccess
{
public:
    PmAction_EnableEngine(const PmGpuDesc *pGpuDesc,
                          const string &EngineType,
                          UINT32 hwinst,
                          bool enable,
                          const PmSmcEngineDesc *pSmcEngineDesc,
                          const PmChannelDesc *pChannelDesc,
                          const string& RegName,
                          const vector<UINT32>& RegIndexes,
                          const string& RegSpace);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc* m_pGpuDesc;                  
    string m_EngineType;
    UINT32 m_hwinst;
    bool m_Enable;
};

//--------------------------------------------------------------------
//! PmAction subclass that starts a timer.  Used with Trigger.OnTimeUs.
//!
class PmAction_StartTimer : public PmAction
{
public:
    PmAction_StartTimer(const string &TimerName);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    string m_TimerName;
};

//--------------------------------------------------------------------
//! PmAction subclass that remaps a virtual address to the
//  physical memory mapped to another given address
//!
class PmAction_AliasPages : public PmAction
{
public:
    PmAction_AliasPages(const PmSurfaceDesc *pSurfaceDesc,
                        UINT64 aliasDstOffset, UINT64 size,
                        UINT64 aliasSrcOffset,
                        bool deferTlbIlwalidate);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmSurfaceDesc*      m_pSurfaceDesc;
    UINT64                    m_AliasDstOffset;
    UINT64                    m_AliasSrcOffset;
    UINT64                    m_Size;
    bool                      m_DeferTlbIlwalidate;
};

//--------------------------------------------------------------------
//! PmAction subclass that remaps a virtual address to the
//  physical memory mapped to another given address from a different surface
//!
class PmAction_AliasPagesFromSurface : public PmAction
{
public:
    PmAction_AliasPagesFromSurface(const PmSurfaceDesc *pDestSurfaceDesc,
                        UINT64 aliasDstOffset, UINT64 size,
                        const PmSurfaceDesc *pSourceSurfaceDesc,
                        UINT64 aliasSrcOffset,
                        bool deferTlbIlwalidate);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmSurfaceDesc*      m_pDestSurfaceDesc;
    UINT64                    m_AliasDstOffset;
    const PmSurfaceDesc*      m_pSourceSurfaceDesc;
    UINT64                    m_AliasSrcOffset;
    UINT64                    m_Size;
    bool                      m_DeferTlbIlwalidate;
};

//--------------------------------------------------------------------
//! PmAction subclass to fill a given range of a surface with a specified
//! value
//!
class PmAction_FillSurface : public PmAction
{
public:
    PmAction_FillSurface(PmSurfaceDesc     *pSurfaceDesc,
                         const FancyPicker& offset,
                         UINT32             size,
                         const FancyPicker& data,
                         bool               isInband,
                         PmChannelDesc     *pInbandChannel);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmSurfaceDesc        *m_pSurfaceDesc; //!< Pages in matching surfaces
    FancyPicker                 m_OffsetPicker; //!< offset into surface
    UINT32                      m_Size;         //!< size of range to fill
    FancyPicker                 m_DataPicker;   //!< data to write
    FancyPicker::FpContext      m_FpContext;

    bool m_IsInband;                 //!< Use in-band methods to clear surface
    PmChannelDesc *m_pInbandChannel; //!< In-band methods channel
};

//--------------------------------------------------------------------
//! PmAction subclass to set cpu mapping mode for the specified surface(s)
//!
class PmAction_SetCpuMappingMode : public PmAction
{
public:
    PmAction_SetCpuMappingMode(const string &Name,
                               const PmSurfaceDesc *pSurfaceDesc,
                               PolicyManager::CpuMappingMode mappingMode);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmSurfaceDesc          *m_pSurfaceDesc; //!< Pages in matching surfaces
    PolicyManager::CpuMappingMode m_CpuMappingMode;
};

//--------------------------------------------------------------------
//! PmAction subclass to set direct-map mode for the specified surface(s)
//!
class PmAction_CpuMapSurfacesDirect : public PmAction_SetCpuMappingMode
{
public:
    PmAction_CpuMapSurfacesDirect(const PmSurfaceDesc *pSurfaceDesc);
};

//--------------------------------------------------------------------
//! PmAction subclass to set reflected-map mode for the specified surface(s)
//!
class PmAction_CpuMapSurfacesReflected : public PmAction_SetCpuMappingMode
{
public:
    PmAction_CpuMapSurfacesReflected(const PmSurfaceDesc *pSurfaceDesc);
};

//--------------------------------------------------------------------
//! PmAction subclass to set default cpu map mode for the specified surface(s)
//!
class PmAction_CpuMapSurfacesDefault : public PmAction_SetCpuMappingMode
{
public:
    PmAction_CpuMapSurfacesDefault(const PmSurfaceDesc *pSurfaceDesc);
};

//--------------------------------------------------------------------
//! PmAction subclass that changes clk in specified clk domain.
//!
class PmAction_GpuSetClock : public PmAction
{
public:
    PmAction_GpuSetClock(const PmGpuDesc *pGpuDesc,
                         string clkdmnName, UINT32 targetHz);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    typedef vector<Gpu::ClkDomain> ClkDomains;
    ClkDomains GetMatchingClkDomains(const GpuSubdevice* pGpuSubdevice);

    const PmGpuDesc     *m_pGpuDesc;     //!< Matching gpus
    RegExp               m_ClkdmnName;   //!< Clock domain name
    UINT32               m_TargetHz;     //!< Target clock to be set

};

//--------------------------------------------------------------------
//! PmAction subclass that sets the state of the fb engine
//!
class PmAction_FbSetState : public PmAction
{
public:
    PmAction_FbSetState(const string &Name,
                        const PmGpuDesc *pGpuDesc, UINT32 state);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    #ifdef LW_VERIF_FEATURES
        const PmGpuDesc *m_pGpuDesc; //!< Matching gpus
        UINT32           m_State;    //!< The state, from LW2080_CTRL_FB_SET_STATE
    #endif

};

//--------------------------------------------------------------------
//! PmAction subclass that sets the state of the fb engine
//!
class PmAction_EnableElpg : public PmAction
{
public:
    PmAction_EnableElpg(const string &Name,
                        const PmGpuDesc *pGpuDesc,
                        UINT32 EngineId,
                        bool ToEnableElpg);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc; //!< Matching gpus
    UINT32           m_EngineId; //! taken from pmusub.h PgTypes
    bool             m_ToEnableElpg;
};

//--------------------------------------------------------------------
//! PmAction subclass that do gpu railgate and unrailgate
//!
class PmAction_GpuRailGateAction : public PmAction
{
public:
    PmAction_GpuRailGateAction(const string &Name,
                               const PmGpuDesc *pGpuDesc,
                               UINT32 action);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    RC RailGateGpu(GpuSubdevice *pGpuSubdevice);
    RC UnRailGateGpu(GpuSubdevice *pGpuSubdevice);

    #ifdef LW_VERIF_FEATURES
        const PmGpuDesc *m_pGpuDesc; //!< Matching gpus
        UINT32 m_Action; //!< from LW2080_CTRL_GPU_POWER_ON_OFF_PARAMS.action
                     //!< use it to identify railgate/unrailgate
    #endif
};

//--------------------------------------------------------------------
//! PmAction subclass that set gpio port up
//!
class PmAction_SetGpioUp : public PmAction
{
public:
    PmAction_SetGpioUp(const PmChannelDesc *pChannelDesc,
                       const vector<UINT32> gpioPorts);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Matching channel
    vector<UINT32> m_GpioPorts; //!< Gpio ports to up.
};

//--------------------------------------------------------------------
//! PmAction subclass that set gpio port down
//!
class PmAction_SetGpioDown : public PmAction
{
public:
    PmAction_SetGpioDown(const PmChannelDesc *pChannelDesc,
                         const vector<UINT32> gpioPorts);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmChannelDesc *m_pChannelDesc; //!< Matching channel
    vector<UINT32> m_GpioPorts; //!< Gpio ports to down.
};

//--------------------------------------------------------------------
//! PmAction subclass that creates a channel
//!
class PmAction_CreateChannel : public PmAction
{
public:
    PmAction_CreateChannel(
        const string &channelName,
        const PmGpuDesc *pGpuDesc,
        const PmTestDesc *pTestDesc,
        const PmVaSpaceDesc * pVaSpaceDesc,
        const string &engineName,
        const PmSmcEngineDesc *pSmcEngineDesc);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    string m_Name;                 //!< Channel name
    const PmGpuDesc *m_pGpuDesc;   //!< GpuDevice(s) to create channel(s) on
    const PmTestDesc *m_pTestDesc; //!< Test(s) to create channel(s) on
    const PmVaSpaceDesc * m_pVaSpaceDesc;    //!< Virtual address Space name
    const string m_EngineName;
    const PmSmcEngineDesc * m_pSmcEngineDesc;    //!< SmcEngine
};

//--------------------------------------------------------------------
//! PmAction subclass that creates a subchannel
//!
class PmAction_CreateSubchannel : public PmAction
{
public:
    PmAction_CreateSubchannel(
        const PmChannelDesc *pChannelDesc,
        const UINT32 hwClass,
        const UINT32 subchannelNumber,
        const PmSmcEngineDesc *pSmcEngineDesc);

    PmAction_CreateSubchannel(
        const PmChannelDesc *pChannelDesc,
        const string& actionName,
        const PmSmcEngineDesc *pSmcEngineDesc);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

protected:
    RC CreateSubchannel(PmChannel* pPmChannel,
                        void * objParams);
    const PmChannelDesc *GetPmChannelDesc() const { return m_pChannelDesc; }

private:
    const PmChannelDesc *m_pChannelDesc; //!< Channel(s)

protected:
    UINT32 m_HwClass;              //!< HW class for the subchannel
    UINT32 m_SubchannelNumber;     //!< Subchannel number
    const PmSmcEngineDesc * m_pSmcEngineDesc;    //!< SmcEngine
};

//--------------------------------------------------------------------
//! PmAction subclass that creates a CE subchannel
//! Note: Unlike PmAction_CreateSubchannel, this action is for CE subchannel specially.
//!
class PmAction_CreateCESubchannel : public PmAction_CreateSubchannel
{
public:
    PmAction_CreateCESubchannel(
        const PmChannelDesc *pChannelDesc,
        UINT32 ceAllocInstNum,
        PolicyManager::CEAllocMode allocMode,
        const PmSmcEngineDesc *pSmcEngineDesc);

    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:

    RC GetSubchCreationParams(PmChannel *pPmChannel,
                              vector<UINT08> *objParams,
                              UINT32 subchClass,
                              UINT32 *pSubChNum,
                              UINT32 *engineId,
                              bool *IsGrCE);

    UINT32 m_CEAllocInstNum;
    PolicyManager::CEAllocMode m_CEAllocMode;
};

//--------------------------------------------------------------------
//! PmAction subclass for enabling/disabling the update of the get pointer
//! when processing fault buffer entries.
//!
class PmAction_SetUpdateFaultBufferGetPointer : public PmAction
{
public:
    PmAction_SetUpdateFaultBufferGetPointer(bool flag);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    bool m_Flag;
};

//--------------------------------------------------------------------
//! PmAction subclass for clearing the fault buffer by setting the get pointer
//! equal to the put pointer.
//!
class PmAction_ClearFaultBuffer : public PmAction
{
public:
    PmAction_ClearFaultBuffer(const PmGpuDesc *pGpuDesc);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmGpuDesc *m_pGpuDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass for sending trace_3d event to plugin
//!
class PmAction_SendTraceEvent : public PmAction
{
public:
    PmAction_SendTraceEvent(const string &eventName, const PmSurfaceDesc *pSurfaceDesc);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const string m_EventName;
    const PmSurfaceDesc *m_pSurfaceDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that sets the read/write permission for ATS mapped pages.
//!
class PmAction_SetAtsPermission : public PmAction
{
public:
    PmAction_SetAtsPermission(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        string permissionType,
        bool permissiolwalue);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmPageDesc m_PageDesc;
    string m_PermissionType;
    bool m_Permissiolwalue;
};

//! PmAction subclass that updates the mapping of ATS pages (remaps or unmaps).
//!
class PmAction_UpdateAtsMapping : public PmAction
{
public:
    PmAction_UpdateAtsMapping(const string &name,
        IommuDrv::MappingType updateType);

    virtual PmMemRanges GetUpdateRanges(const PmTrigger *pTrigger,
        const PmEvent *pEvent) = 0;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    IommuDrv::MappingType m_UpdateType;
};

//! PmAction_UpdateAtsMapping subclass that updates the mapping of ATS pages
//! which are specified by a surface description and range.
//!
class PmAction_UpdateAtsPages : public PmAction_UpdateAtsMapping
{
public:
    PmAction_UpdateAtsPages(
        const string &name,
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        IommuDrv::MappingType updateType);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual PmMemRanges GetUpdateRanges(const PmTrigger *pTrigger,
        const PmEvent *pEvent);

private:
    const PmPageDesc m_PageDesc;
};

//! PmAction_UpdateAtsMapping subclass that updates the mapping of the
//! lwrrently faulting ATS page.
//!
class PmAction_UpdateFaultingAtsPage : public PmAction_UpdateAtsMapping
{
public:
    PmAction_UpdateFaultingAtsPage(const string &name,
        IommuDrv::MappingType updateType);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual PmMemRanges GetUpdateRanges(const PmTrigger *pTrigger,
        const PmEvent *pEvent);

private:
};

//! PmAction_Goto subclass for a conditional block that exelwtes when
//! specific channel support ATS.
//!
class PmAction_OnAtsIsEnabledOnChannel : public PmAction_Goto
{
public:
    PmAction_OnAtsIsEnabledOnChannel(const PmChannelDesc *pChannelDesc);
    virtual ~PmAction_OnAtsIsEnabledOnChannel();

    virtual RC IsSupported(const PmTrigger *pTrigger) const;

protected:
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent,
                         bool *pBranch);
private:
    const PmChannelDesc *m_pChannelDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that clear engine/pbdma fault bit for channel.
//! Abstract class.
//!
class PmAction_RestartFaultedChannel : public PmAction
{
public:
    enum FaultType
    {
        UNKNOWN,
        ENGINE,
        PBDMA
    };

    PmAction_RestartFaultedChannel (const string& actionName,
        const PmChannelDesc *pChannelDesc, bool  isInBand,
        PmChannelDesc *pInbandChannelDesc, FaultType type);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
    virtual ~PmAction_RestartFaultedChannel() = 0;

private:
    RC InbandClearFault(const PmChannels& pmChannels, PmChannel *pInbandChannel);
    RC OutOfBandClearFaultAmpere(PmChannel *pPmChannel);
    RC OutOfBandClearFault(PmChannel *pPmChannel);
    RC OutOfBandClearFault(const PmChannels& pmChannels);
    PmChannel* GetInbandChannel(PmTrigger *pTrigger, const PmEvent *pEvent,
                                GpuDevice *pGpuDevice);
    RC SendMethodsForVolta(PmChannel *pFaultingChannel, PmChannel *pInbandChannel);

    const PmChannelDesc *m_pChannelDesc;
    bool  m_IsInband;
    PmChannelDesc *m_pInbandChannelDesc;
    FaultType m_FaultType;
};

//--------------------------------------------------------------------
//! PmAction subclass that clear engine fault bit for channel.
//!
class PmAction_RestartEngineFaultedChannel : public PmAction_RestartFaultedChannel
{
public:
    PmAction_RestartEngineFaultedChannel(const PmChannelDesc *pChannelDesc,
                                         bool  isInBand,
                                         PmChannelDesc *pInbandChannelDesc);
};

//--------------------------------------------------------------------
//! PmAction subclass that clear pbdma fault bit for channel.
//!
class PmAction_RestartPbdmaFaultedChannel : public PmAction_RestartFaultedChannel
{
public:
    PmAction_RestartPbdmaFaultedChannel(const PmChannelDesc *pChannelDesc,
                                         bool  isInBand,
                                         PmChannelDesc *pInbandChannelDesc);
};

//--------------------------------------------------------------------
//! PmAction subclass that clear access counter for specified GPU.
//!
class PmAction_ClearAccessCounter : public PmAction
{
public:
    PmAction_ClearAccessCounter(PmGpuDesc *pGpuDesc,
            const bool bInBand,
            const string& counterType);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    RC InbandClear(GpuSubdevice* pSubdev);
    RC OutOfBandClear(GpuSubdevice* pSubdev);

    void MemOps(UINT32* pMemOpC, UINT32* pMemOpD) const;
    void MemOpsTargeted(UINT32* pMemOpC, UINT32* pMemOpD)const;
    RC WriteMethod(PmChannel *pChannel);
    RC ColwertStrType();
    RC CheckType() const;

    PmGpuDesc* m_pGpuDesc;
    string m_StrCounterType;
    UINT32 m_CounterType;
    UINT32 m_TargetedBank;
    UINT32 m_TargetedNotifyTag;
    bool m_bInBand;
    bool m_bTargeted;
};

//-------------------------------------------------------------------------------------
//! PmAction subclass that set cached GET pointer of access counter for specified GPU.
//!
class PmAction_ResetAccessCounterCachedGetPointer : public PmAction
{
public:
    PmAction_ResetAccessCounterCachedGetPointer(PmGpuDesc *pGpuDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    PmGpuDesc* m_pGpuDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that set GET = PUT + 1 % SIZE
//! for non-replayable buffer.
//!
class PmAction_ForceNonReplayableFaultBufferOverflow : public PmAction
{
public:
    PmAction_ForceNonReplayableFaultBufferOverflow();
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
};

//! PmAction subclass that sets the specified physical addresses bits of
//! a set of pages.
//!
class PmAction_SetPhysAddrBits : public PmAction
{
public:
    PmAction_SetPhysAddrBits(
        const PmSurfaceDesc *pSurfaceDesc,
        UINT64 offset,
        UINT64 size,
        UINT64 addrValue,
        UINT64 addrMask,
        bool isInband,
        PmChannelDesc *inbandChannelDesc,
        bool deferTlbIlwalidate);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmPageDesc m_PageDesc;
    UINT64 m_AddrValue;
    UINT64 m_AddrMask;
    bool m_IsInband;
    PmChannelDesc *m_InbandChannelDesc;
    bool m_DeferTlbIlwalidate;
};

//--------------------------------------------------------------------
//! PmAction subclass that starts a timer.  Used with Trigger.OnTimeUs.
//!
class PmAction_StartVFTest : public PmAction
{
public:
    PmAction_StartVFTest(const PmVfTestDesc * pVfDesc);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmVfTestDesc * m_pVfDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that starts a timer.  Used with Trigger.OnTimeUs.
//!
class PmAction_WaitProcEvent : public PmAction
{
public:
    PmAction_WaitProcEvent(const PmVfTestDesc * pVfDesc,
            const string & message);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmVfTestDesc * m_pVfDesc;
    const string m_Message;
};

//--------------------------------------------------------------------
//! PmAction subclass that starts a timer.  Used with Trigger.OnTimeUs.
//!
class PmAction_SendProcEvent : public PmAction
{
public:
    PmAction_SendProcEvent(const PmVfTestDesc * pVfDesc,
            const string & message);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmVfTestDesc * m_pVfDesc;
    const string m_Message;
};

//--------------------------------------------------------------------
//! PmAction subclass that waits runlist idle interrupt.
//!
class PmAction_WaitErrorLoggerInterrupt : public PmAction
{
public:
    PmAction_WaitErrorLoggerInterrupt(const PmGpuDesc *pGpuDesc,
        const std::string &name, FLOAT64 TimeoutMs);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
    virtual ~PmAction_WaitErrorLoggerInterrupt();

private:
    static bool PollInterrupts(void *pvArgs);

    const PmGpuDesc *m_pGpuDesc = nullptr;     //!< Matching gpus
    const std::string m_Name;
    FLOAT64 m_TimeoutMs = Tasker::NO_TIMEOUT;
    vector<PmErrorLoggerInt::Event*> m_WaitEvents;
};

//--------------------------------------------------------------------
//! PmAction subclass that Trigger VF FLR.
//!
class PmAction_VirtFunctionLevelReset : public PmAction
{
public:
    PmAction_VirtFunctionLevelReset(const PmVfTestDesc * pVfDesc);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmVfTestDesc * m_pVfTestDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that Waiting FLR_PENDING bit clear if it's not cleared by user; wait VF Process exit.
//!
class PmAction_WaitFlrDone : public PmAction
{
public:
    PmAction_WaitFlrDone(const PmVfTestDesc * pVfDesc);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmVfTestDesc * m_pVfTestDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that Clear FLR_PENDING bit; Mods will set RM registry key to avoid RM clears the bit once this action is seen..
//!
class PmAction_ClearFlrPendingBit : public PmAction
{
public:
    PmAction_ClearFlrPendingBit(const PmVfTestDesc * pVfDesc);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const PmVfTestDesc * m_pVfTestDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that exit current vf process.
//!
class PmAction_ExitLwrrentVfProcess : public PmAction
{
public:
    PmAction_ExitLwrrentVfProcess();

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
};

//--------------------------------------------------------------------
//! PmAction subclass that Create a conditional action block that will execute if the specified engine existed.
//!
class PmAction_OnEngineIsExisting : public PmAction_Goto
{
public:
    PmAction_OnEngineIsExisting(const string &engineName,
                                const PmSmcEngineDesc * pSmcEngineDesc);

    virtual RC IsSupported(const PmTrigger *pTrigger) const;
protected:    
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent, bool *pGoto);
private:
    const string m_EngineName;
    const PmSmcEngineDesc * m_pSmcEngineDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that source channel dump channel RAM to a file.
//!
class PmAction_SaveCHRAM : public PmAction
{
public:
    PmAction_SaveCHRAM(const PmChannelDesc *pChannelDesc,
                       const string& filename);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    const PmChannelDesc *m_pChannelDesc;
    string m_Filename;
};

//--------------------------------------------------------------------
//! PmAction subclass that dst channel load channel RAM from a file.
//!
class PmAction_RestoreCHRAM : public PmAction
{
public:
    PmAction_RestoreCHRAM(const PmChannelDesc *pChannelDesc,
                       const string& filename);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    struct CHRAMParseData
    {
        string testName;
        string channelName;
        UINT32 subdevice;
        LwU32 data;

        CHRAMParseData(const string& testName, const string& chanName, UINT32 subdev, LwU32 data);
    };

    const PmChannelDesc *m_pChannelDesc;
    string m_Filename;
};

// The enclosed vGpu migration related object passing across
// base action and the derived sub actions.
struct MDiagVmObjs;

//--------------------------------------------------------------------
//! PmAction subclass as FB copy Save/restore actions base class.
//!
class PmAction_FbCopyBase : public PmAction
{
public:
    virtual ~PmAction_FbCopyBase() {}
    PmAction_FbCopyBase(const string& actionName,
        bool bRead,
        const string& fileName);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

protected:
    virtual RC Setup(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm) = 0;
    // Default Cleanup().
    virtual void Cleanup(MDiagVmObjs* pVm);

    // Alloc mem using MdiagSurface but not directly map FB mem on BAR1.
    void SetUseMdiagSurf() { m_bUseMdiagSurf = true; }
    bool IsRead() const { return m_bRead; }
    string& GetFileName()
    {
        return m_File;
    }
    void SetDataPattern(size_t blockSize, UINT32 val)
    {
        m_BlockSize = blockSize;
        m_Pattern = val;
    }

private:
    RC Read(MDiagVmObjs* pVm);
    RC Write(MDiagVmObjs* pVm);

    string m_File;
    size_t m_BlockSize = sizeof(UINT32);
    UINT32 m_Pattern = 0;
    const bool m_bRead;
    // Alloc mem using MdiagSurface or directly map FB mem on BAR1.
    // Directly map on BAR1 by default.
    bool m_bUseMdiagSurf = false;
};

//--------------------------------------------------------------------
//! PmAction subclass that dump VM or SMC partition
//  FB mem to or load FB mem from a file.
//!
class PmAction_FbCopy : public PmAction_FbCopyBase
{
public:
    PmAction_FbCopy(const string& actionName,
        bool bRead,
        const PmVfTestDesc* pVfDesc,
        const PmSmcEngineDesc* pSmcEngDesc,
        const string& fileName);

private:
    virtual RC Setup(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);

    RC SetupWholeFb(MDiagVmObjs* pVm);
    RC SetupSmc(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);
    RC SetupSriov(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);

    const PmVfTestDesc* m_pVfDesc;
    const PmSmcEngineDesc* m_pSmcEngDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that save/restore single FB mem segment to or from a file.
//!
class PmAction_FbCopySeg : public PmAction_FbCopyBase
{
public:
    PmAction_FbCopySeg(const string& actionName,
        bool bRead,
        const string& fileName,
        UINT64 paOff, UINT64 size);

private:
    virtual RC Setup(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);

    PHYSADDR m_PaOff;
    UINT64 m_Size;
};

//--------------------------------------------------------------------
//! PmAction subclass that save/restore single FB mem segment to or from a file.
//!
class PmAction_FbCopySegSurf : public PmAction_FbCopySeg
{
public:
    PmAction_FbCopySegSurf(const string& actionName,
        bool bRead,
        const string& fileName,
        UINT64 paOff, UINT64 size);
};

//--------------------------------------------------------------------
//! PmAction subclass that save a runlist FB buffer to local buffer.
//!
class PmAction_FbCopyRunlist : public PmAction_FbCopyBase
{
public:
    PmAction_FbCopyRunlist(const string& actionName,
        bool bRead,
        const PmChannelDesc* pDesc,
        const string& fileName);

protected:
    virtual RC Setup(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);
    // Need own Cleanup().
    virtual void Cleanup(MDiagVmObjs* pVm);

    const PmChannelDesc* m_pDesc;
};

//--------------------------------------------------------------------
//! PmAction subclass that CE copy FB mem from one location to another.
//!
class PmAction_DmaFbCopyBase : public PmAction
{
public:
    PmAction_DmaFbCopyBase(const string& actionName);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

protected:
    struct Config
    {
        bool m_b2StepsCopy = false;
        bool m_bRead = false;
        bool m_bSysmem = false;
    };
        
    virtual RC Setup(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm) = 0;
    // Default Cleanup().
    virtual void Cleanup(MDiagVmObjs* pVm);

    void SetConfig(const Config& cfg) { m_Cfg = cfg; }

private:
    Config m_Cfg;
};

//--------------------------------------------------------------------
//! PmAction subclass that CE copy FB mem from one VM to another.
//!
class PmAction_DmaCopyFb : public PmAction_DmaFbCopyBase
{
public:
    PmAction_DmaCopyFb(const PmVfTestDesc* pVfDescFrom,
        const PmVfTestDesc* pVfDescTo,
        const PmSmcEngineDesc* pSmcEngDescFrom,
        const PmSmcEngineDesc* pSmcEngDescTo
        );

private:
    virtual RC Setup(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);
    RC SetupWholeFb(MDiagVmObjs* pVm);
    RC SetupSmc(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);
    RC SetupSriov(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);

    const PmVfTestDesc* m_pVfDescFrom;
    const PmVfTestDesc* m_pVfDescTo;
    const PmSmcEngineDesc* m_pSmcEngDescFrom;
    const PmSmcEngineDesc* m_pSmcEngDescTo;
};

//--------------------------------------------------------------------
//! PmAction subclass that CE copy FB mem from one VM to vidmem.
//!
class PmAction_2StepsDmaCopyFb : public PmAction_DmaFbCopyBase
{
public:
    PmAction_2StepsDmaCopyFb(const string& actionName,
        const PmVfTestDesc* pVfDesc,
        bool bRead,
        bool bSysmem
        );

private:
    virtual RC Setup(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);

    const PmVfTestDesc* m_pVfDesc;
    PmAction_DmaFbCopyBase::Config m_Cfg;
};

//--------------------------------------------------------------------
//! PmAction subclass that CE copy FB mem seg to vidmem.
//!
class PmAction_DmaCopySeg : public PmAction_DmaFbCopyBase
{
public:
    PmAction_DmaCopySeg(const string& actionName,
        bool bRead,
        PHYSADDR paOff,
        UINT64 size 
        );

private:
    virtual RC Setup(PmTrigger *pTrigger,
        const PmEvent *pEvent,
        MDiagVmObjs* pVm);

    PmAction_DmaFbCopyBase::Config m_Cfg;
    const PHYSADDR m_PaOff;
    const UINT64 m_Size;
};

//--------------------------------------------------------------------
//! PmAction subclass that dump FB mem info of SMC partitions to a file.
//!
class PmAction_SaveSmcPartFbInfo : public PmAction
{
public:
    PmAction_SaveSmcPartFbInfo(const string& fileName);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const string m_File;
};

//--------------------------------------------------------------------
//! PmAction subclass that add the mutex between UTL and Policymanager.
//!
class PmAction_TryMutex : public PmAction_Goto
{
public:
    PmAction_TryMutex(const RegExp& mutexName);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC DoExelwte(PmTrigger *pTrigger, const PmEvent *pEvent, bool *pGoto);

private:
    const RegExp m_MutexName;
};

//--------------------------------------------------------------------
//! PmAction subclass that add the mutex between UTL and Policymanager.
//!
class PmAction_OnMutex : public PmAction
{
public:
    PmAction_OnMutex(const RegExp& mutexName);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);

private:
    const RegExp m_MutexName;
};

//--------------------------------------------------------------------
//! PmAction subclass that clear current actionBlock status.
//!
class PmAction_ReleaseMutex : public PmAction
{
public:
    PmAction_ReleaseMutex(RegExp mutex);
    virtual RC IsSupported(const PmTrigger *pTrigger) const;
    virtual RC Execute(PmTrigger *pTrigger, const PmEvent *pEvent);
private:
    const RegExp m_MutexName;
};

#endif // INCLUDED_PMACTION_H 

