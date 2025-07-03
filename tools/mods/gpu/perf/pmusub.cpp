/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "class/cl85b6.h"  // GT212_SUBDEVICE_PMU
#include "core/include/fileholder.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "ctrl/ctrl85b6.h" // LW85B6_CTRL_PMU_*
#include "device/interface/pcie.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/pmusub.h"
#include "gpu/perf/pwrwait.h"
#include "gpu/utility/pcie/pexdev.h"
#include "lwmisc.h"
#include "lwos.h"
#include "lwRmReg.h"       // for LW_REG_STR_RM_PMU_BOOTSTRAP_MODE_xxx
#include "lwtypes.h"
#include "rmflcncmdif.h"
#include "rmpmucmdif.h"

#define ROUND32(a) (((a) + 3) & ~0x03U)

#define EVENT_QUEUE_DEPTH 32
#define EVENT_QUEUE_SIZE  (ROUND32(sizeof(RM_FLCN_MSG_PMU)) * EVENT_QUEUE_DEPTH)
#define EVENT_QUEUE_DATA_SIZE  (m_EventQueueBytes - 2 * sizeof(UINT32))

#define EVENT_QUEUE_VALID(h, t, s) (((h) >= (t)) ? ((h) - (t)) : \
                                                   ((h) + (s) - (t)))

#define BOOTSTRAP_TIMEOUT  10000   //!< Maximum time to boot the PMU
#define CMD_TIMEOUT        100000  //!< Maximum time to wait for all commands
                                   //!< when shutting down
#define DEEP_IDLE_STATS_TIMEOUT 1000  //!< Maximum time to wait for deep idle
                                      //!< statistics to be acquired

/* static */ UINT32 PMU::s_RegisterUnitIds[] =
{
    RM_PMU_UNIT_REWIND,
    RM_PMU_UNIT_LPWR,
    RM_PMU_UNIT_INIT,
    RM_PMU_UNIT_GCX
};

/* static */ UINT32 PMU::s_RegisteredUnitCount =
        sizeof(s_RegisterUnitIds) / sizeof(UINT32);

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
PMU::PMU(GpuSubdevice *pParent)
:
    m_Parent(pParent),
    m_IsPaused(0),
    m_PauseCount(0),
    m_Initialized(false),
    m_Handle((LwRm::Handle)0),
    m_PmuElpgEnabled(false),
    m_SupportedElpg(0),
    m_SupportedElpgQueried(false),
    m_ElpgIsOwned(false),
    m_EventQueueBytes(0),
    m_SurfaceLoc(Memory::Coherent)
{
    m_EventTask.hRmEvent = 0;
    m_EventTask.pEventThread = NULL;
    m_EventTask.pPmu = this;
    m_CommandTask.hRmEvent = 0;
    m_CommandTask.pEventThread = NULL;
    m_CommandTask.pPmu = this;
    m_PMUMutex = NULL;

    // Register for all PMU events
    m_EventQueues.resize(s_RegisteredUnitCount);
    for (UINT32 unitIdIdx = 0; unitIdIdx < s_RegisteredUnitCount; unitIdIdx++)
    {
        m_EventQueues[unitIdIdx].hEvent = ~0;
        m_EventQueues[unitIdIdx].pHead = NULL;
        m_EventQueues[unitIdIdx].pTail = NULL;
    }

    // Per engine column headers used during printing, Each header should be 10 characters
    // wide and right justified
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD] =         "Idle Thrsh";
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD] = "PPU Thresh";
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_DELAY_INTERVAL] =         "Delay Itvl";
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_INITIALIZED] =            "    Init'd";
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT] =            "Gate Count";
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_DENYCOUNT]   =            "Deny Count";
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_PSORDER]     =            "   PSOrder";
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_ZONELUT]     =            "  Zone LUT";

    // Non engine based parameter strings used during printing
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES] = "Number Power Gatable Engines";
    m_PGParamString[LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_IDLE_THRESHOLD] = "Power Rail Idle Thresh";
}

//-----------------------------------------------------------------------------
//! \brief Destructor
//!
PMU::~PMU()
{
    Shutdown(false);
}

//-----------------------------------------------------------------------------
//! \brief Initialize the PMU object
//!
//!  - Allocate space for all event buffers in a surface
//!  - Allocate OS events for PMU Events and PMU Commands
//!  - Register to receive PMU Events of all Unit IDs
//!
//! \return OK if initialization succeeds, not OK otherwise
//!
RC PMU::Initialize()
{
    RC      rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS   notifParams = {0};
    LW85B6_CTRL_PMU_EVENT_REGISTER_PARAMS pmuEventParams = {0};

    // Validate that this GPU has a PMU, if so, allocate and initialize it
    if (!pLwRm->IsClassSupported(GT212_SUBDEVICE_PMU,
                                 m_Parent->GetParentDevice()))
        return RC::UNSUPPORTED_FUNCTION;

    // Allocate a mutex for protecting the various lists
    m_PMUMutex = Tasker::AllocMutex("PMU::m_PMUMutex", Tasker::mtxUnchecked);
    if (m_PMUMutex == NULL)
    {
        MASSERT(!"Unable to create PMU mutex\n");
        CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
    }

    // Because MODS/RM are sharing surfaces allocated for the PMU and these
    // surfaces are being allocated in MODS, MODS needs to put these surfaces
    // into the correct location based on the forced PMU Instance location
    UINT32 instLoc;
    if (OK == Registry::Read("ResourceManager", LW_REG_STR_RM_INST_LOC, &instLoc))
    {
        switch (DRF_VAL(_REG_STR, _RM_INST_LOC, _PMUINST, instLoc))
        {
            case LW_REG_STR_RM_INST_LOC_PMUINST_COH:
                m_SurfaceLoc = Memory::Coherent;
                break;
            case LW_REG_STR_RM_INST_LOC_PMUINST_NCOH:
                m_SurfaceLoc = Memory::NonCoherent;
                break;
            case LW_REG_STR_RM_INST_LOC_PMUINST_VID:
                m_SurfaceLoc = Memory::Fb;
                break;
            case LW_REG_STR_RM_INST_LOC_PMUINST_DEFAULT:
                // Value is not forced, allow MODS to use its default for
                // shared RM/PMU surfaces
                break;
            default:
                MASSERT(!"Unknown PMU Instance Location");
                break;
        }
    }

    // Allocate Subdevice PMU handle
    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(m_Parent),
                          &m_Handle,
                          GT212_SUBDEVICE_PMU,
                          NULL));

    // Setup the surface for the Event queues
    m_EventBuffers.SetName("_PmuEventBuffer");
    m_EventQueueBytes = EVENT_QUEUE_SIZE + 2*sizeof(UINT32);
    m_EventBuffers.SetWidth(GetWidth(&m_EventQueueBytes));
    m_EventBuffers.SetPitch(m_EventQueueBytes);
    m_EventBuffers.SetHeight(s_RegisteredUnitCount);
    m_EventBuffers.SetColorFormat(ColorUtils::VOID32);
    m_EventBuffers.SetLocation(m_SurfaceLoc);
    m_EventBuffers.SetKernelMapping(true);
    m_EventBuffers.SetVaReverse(((GpuDevMgr*) DevMgrMgr::d_GraphDevMgr)->GetVasReverse());

    if (Platform::IsPhysFunMode() && (m_SurfaceLoc == Memory::Fb))
    {
        // Force m_EventBuffers to be allocated from TOP to save the reserved VMMU
        // segment. Comments above vfSetting.fbReserveSize in
        // VmiopMdiagElwManager::ParseVfConfigFile() have more details
        m_EventBuffers.SetPaReverse(true);
    }

    if (Memory::NonCoherent == Surface2D::GetActualLocation(
                m_SurfaceLoc, m_Parent))
    {
        m_EventBuffers.SetExtSysMemFlags(DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                                         DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
                                         0);
    }
    CHECK_RC(m_EventBuffers.Alloc(m_Parent->GetParentDevice()));
    CHECK_RC(m_EventBuffers.Map(m_Parent->GetSubdeviceInst()));

    // Allocate the necessary OS events so that PMU events can be received
    m_EventTask.pEventThread = new EventThread(Tasker::MIN_STACK_SIZE,
        Utility::StrPrintf("PmuEventHandler %d:%d",
                           m_Parent->GetParentDevice()->GetDeviceInst(),
                           m_Parent->GetSubdeviceInst()).c_str());
    if (m_EventTask.pEventThread == NULL)
    {
        MASSERT(!"Unable to allocate Event Thread for PMU Event Task\n");
        CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
    }
    void* pOsEvent = m_EventTask.pEventThread->GetOsEvent(
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(m_Parent->GetParentDevice()));
    CHECK_RC(m_EventTask.pEventThread->SetHandler(EventHandler, this));
    CHECK_RC(pLwRm->AllocEvent(pLwRm->GetSubdeviceHandle(m_Parent),
                       &m_EventTask.hRmEvent,
                       LW01_EVENT_OS_EVENT,
                       LW2080_NOTIFIERS_PMU_EVENT,
                       pOsEvent));

    m_CommandTask.pEventThread = new EventThread(Tasker::MIN_STACK_SIZE,
        Utility::StrPrintf("PmuCommandHandler %d:%d",
        m_Parent->GetParentDevice()->GetDeviceInst(),
        m_Parent->GetSubdeviceInst()).c_str());
    if (m_CommandTask.pEventThread == NULL)
    {
        MASSERT(!"Unable to allocate Event Thread for PMU Command Task\n");
        CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
    }
    pOsEvent = m_CommandTask.pEventThread->GetOsEvent(
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(m_Parent->GetParentDevice()));
    CHECK_RC(m_CommandTask.pEventThread->SetHandler(CommandHandler,
                                                    this));
    CHECK_RC(pLwRm->AllocEvent(pLwRm->GetSubdeviceHandle(m_Parent),
                       &m_CommandTask.hRmEvent,
                       LW01_EVENT_OS_EVENT,
                       LW2080_NOTIFIERS_PMU_COMMAND,
                       pOsEvent));

    // Register for all PMU events
    for (UINT32 unitIdIdx = 0; unitIdIdx < s_RegisteredUnitCount; unitIdIdx++)
    {
        m_EventQueues[unitIdIdx].pHead =
                (UINT32 *)((UINT08 *)m_EventBuffers.GetAddress() +
                                     m_EventQueueBytes * unitIdIdx +
                                     LW85B6_CTRL_PMU_EVENT_QUEUE_OFFSET_HEAD);
        m_EventQueues[unitIdIdx].pTail =
                (UINT32 *)((UINT08 *)m_EventBuffers.GetAddress() +
                                     m_EventQueueBytes * unitIdIdx +
                                     LW85B6_CTRL_PMU_EVENT_QUEUE_OFFSET_TAIL);
        m_EventQueues[unitIdIdx].pQueue =
               (UINT08 *)m_EventBuffers.GetAddress() +
                         m_EventQueueBytes * unitIdIdx +
                         LW85B6_CTRL_PMU_EVENT_QUEUE_OFFSET_DATA;
        MEM_WR32(m_EventQueues[unitIdIdx].pHead, 0);
        MEM_WR32(m_EventQueues[unitIdIdx].pTail, 0);

        pmuEventParams.unitId = s_RegisterUnitIds[unitIdIdx];
        pmuEventParams.evtQueue.hMemory = m_EventBuffers.GetMemHandle();
        pmuEventParams.evtQueue.offset = m_EventQueueBytes * unitIdIdx;
        pmuEventParams.evtQueue.size = m_EventQueueBytes;
        CHECK_RC(pLwRm->Control(m_Handle,
                        LW85B6_CTRL_CMD_PMU_EVENT_REGISTER,
                        &pmuEventParams, sizeof(pmuEventParams)));
        m_EventQueues[unitIdIdx].hEvent = pmuEventParams.evtDesc;
    }

    // Enable PMU Event and command notifiers
    notifParams.event  = LW2080_NOTIFIERS_PMU_EVENT;
    notifParams.action =
        LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(m_Parent),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &notifParams, sizeof(notifParams)));

    notifParams.event  = LW2080_NOTIFIERS_PMU_COMMAND;
    notifParams.action =
        LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(m_Parent),
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &notifParams, sizeof(notifParams)));

    vector<LW2080_CTRL_POWERGATING_PARAMETER> PgParam(1);
    PgParam[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_PMU_ELPG_ENABLED;
    if (OK == GetPowerGatingParameters(&PgParam))
    {
        m_PmuElpgEnabled = (PgParam[0].parameterValue != 0);
    }

    m_Initialized = true;
    PrintPowerGatingParams(Tee::PriLow, ELPG_ALL_ENGINES, true);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Shutdown the PMU
//!
//!  - Unregister for all PMU events
//!  - Disable Event and Command notification and shutdown threads
//!  - Free all event buffer data
//!  - Free up queues
//!
void PMU::Shutdown(bool bEos)
{
    if (m_Initialized)
    {
        PrintPowerGatingParams(Tee::PriLow, ELPG_ALL_ENGINES, true);
    }

    LwRmPtr pLwRm;
    LW85B6_CTRL_PMU_EVENT_REGISTER_PARAMS     pmuEventParams = {0};
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS notifParams = {0};
    list<EventListEntry *>::iterator      eventIter;
    map<ModsEvent *, ModsEventData>::iterator modsEventMapIter;

    // Unregister all events
    for (UINT32 unitIdIdx = 0; unitIdIdx < s_RegisteredUnitCount; unitIdIdx++)
    {
        if (m_EventQueues[unitIdIdx].hEvent != (UINT32)~0)
        {
            pmuEventParams.unitId = s_RegisterUnitIds[unitIdIdx];
            pmuEventParams.evtQueue.hMemory =
                m_EventBuffers.GetMemHandle();
            pmuEventParams.evtQueue.offset = m_EventQueueBytes * unitIdIdx;
            pmuEventParams.evtQueue.size = m_EventQueueBytes;
            pmuEventParams.evtDesc = m_EventQueues[unitIdIdx].hEvent;
            pLwRm->Control(m_Handle,
                           LW85B6_CTRL_CMD_PMU_EVENT_UNREGISTER,
                           &pmuEventParams, sizeof(pmuEventParams));
            m_EventQueues[unitIdIdx].hEvent = (UINT32)~0;
        }
    }

    // Disable all event and command notification and shutdown processing
    // threads
    if (m_Initialized)
    {
        notifParams.event  = LW2080_NOTIFIERS_PMU_EVENT;
        notifParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_DISABLE;
        pLwRm->Control(pLwRm->GetSubdeviceHandle(m_Parent),
                       LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                       &notifParams, sizeof(notifParams));

        if (m_EventTask.pEventThread)
            m_EventTask.pEventThread->SetHandler(NULL);

        eventIter = m_EventList.begin();
        while (eventIter != m_EventList.end())
        {
            delete *eventIter;
            eventIter = m_EventList.erase(eventIter);
        }

        Printf(Tee::PriLow,
               "PMU::Shutdown : Waiting for command responses before shutdown\n");
        notifParams.event  = LW2080_NOTIFIERS_PMU_COMMAND;
        notifParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_DISABLE;
        pLwRm->Control(pLwRm->GetSubdeviceHandle(m_Parent),
                       LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                       &notifParams, sizeof(notifParams));
    }
    if (m_CommandTask.pEventThread)
        m_CommandTask.pEventThread->SetHandler(NULL);

    for (modsEventMapIter = m_ModsEventData.begin();
         modsEventMapIter != m_ModsEventData.end(); modsEventMapIter++)
    {
        if (modsEventMapIter->second.bEventAlloc)
            Tasker::FreeEvent(modsEventMapIter->first);
    }

    m_ModsEventData.clear();
    m_EventList.clear();

    //
    // On RTL, ensure PMU is in the proper state for EOS if EOS handling is
    // imminent. Since this ilwolves interfacing with the RM, be sure to do
    // this before freeing RM handles.
    //
#ifdef SIM_BUILD
    if ((Platform::GetSimulationMode() == Platform::RTL) &&
        bEos &&
        !m_Parent->GetParentDevice()->GetResetInProgress())
    {
        Printf(Tee::PriLow, "Detaching PMU\n");
        Detach();
    }
#endif

    // Free up all RM handles
    if (m_CommandTask.hRmEvent)
    {
        pLwRm->Free(m_CommandTask.hRmEvent);
        m_CommandTask.hRmEvent = 0;
    }
    if (m_EventTask.hRmEvent)
    {
        pLwRm->Free(m_EventTask.hRmEvent);
        m_EventTask.hRmEvent = 0;
    }

    if (m_EventBuffers.GetAddress())
        m_EventBuffers.Unmap();
    if (m_EventBuffers.GetCtxDmaHandleGpu())
        m_EventBuffers.Free();

    if (m_Handle)
    {
        pLwRm->Free(m_Handle);
        m_Handle = 0;
    }

    if (m_CommandTask.pEventThread)
    {
        delete m_CommandTask.pEventThread;
        m_CommandTask.pEventThread = NULL;
    }
    if (m_EventTask.pEventThread)
    {
        delete m_EventTask.pEventThread;
        m_EventTask.pEventThread = NULL;
    }

    if (m_PMUMutex)
    {
        Tasker::FreeMutex(m_PMUMutex);
        m_PMUMutex = NULL;
    }

    m_Initialized = false;
}

//-----------------------------------------------------------------------------
//! \brief Fetches and saves the current PMU ucode state into pPmuUcodeState
//!
//! \param ucodeState : Pointer to the ucode state in which the current ucode
//!                     state will be saved.
//!
//! \return OK if the ucode state was fetched and saved, not OK otherwise
//!
RC PMU::GetUcodeState
(
    UINT32 *ucodeState
)
{
    RC           rc;
    LwRmPtr      pLwRm;
    LW85B6_CTRL_PMU_UCODE_STATE_PARAMS ucodeStateParams = {0};

    MASSERT(m_Initialized);

    if(!ucodeState)
    {
        return RC::LWRM_ILWALID_PARAMETER;
    }
    CHECK_RC(pLwRm->Control(m_Handle,
                            LW85B6_CTRL_CMD_PMU_UCODE_STATE,
                            &ucodeStateParams,
                            sizeof (LW85B6_CTRL_PMU_UCODE_STATE_PARAMS)));
    *ucodeState = ucodeStateParams.ucodeState;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Reset the PMU, and bootstrap if bReboot is true
//!
//! \param bReboot : When true, the PMU will be bootstrapped, using the default
//!                  ucode image.
//!
//! \return OK if the reset command succeeds, not OK otherwise.
//!
RC PMU::Reset
(
    bool bReboot
)
{
    RC           rc;
    LwRmPtr      pLwRm;
    LW85B6_CTRL_PMU_RESET_PARAMS resetParams = {0};

    MASSERT(m_Initialized);
    resetParams.bReboot = bReboot;
    CHECK_RC(pLwRm->Control(m_Handle,
                            LW85B6_CTRL_CMD_PMU_RESET,
                            &resetParams,
                            sizeof (resetParams)));
    if (bReboot)
        CHECK_RC(WaitPMUReady());

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Detach the PMU from the RM.
//!
//! In the "detached" state, the PMU will cease all DMA operations as well as
//! all communication with the RM, and perform only vital operations such as
//! GPU fan-control. Once detached, the only way to return to a fully functional
//! state is to reset and rebootstrap.
//!
//! \return OK if the detach command succeeds, not OK otherwise.
//!
RC PMU::Detach(void)
{
    RC      rc;
    LwRmPtr pLwRm;

    MASSERT(m_Initialized);
    CHECK_RC(pLwRm->Control(m_Handle, LW85B6_CTRL_CMD_PMU_DETACH, NULL, 0x00));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Allocate a MODS event and add it to the MODS event map with the
//!        specified filter information.  All MODS events allocated in this
//!        manner should be freed with FreeEvent
//!
//! \param filterType     : Filter type associated with the MODS event
//! \param bIlwertFilter  : True to ilwert the sense of the filter type.  I.e.
//!                         ilwerting EVENT_UNIT_ID results in notifications for
//!                         all unitIDs except the specified unit ID
//! \param filterData     : Filter data associated with the MODS event
//!
//! \return The pointer to the allocated event
//!
ModsEvent * PMU::AllocEvent
(
    PMU::MessageFilterType filterType,
    bool                   bIlwertFilter,
    UINT32                 filterData
)
{
    ModsEvent *pEvent;
    MASSERT(m_Initialized);
    MASSERT(!bIlwertFilter || (filterType != ALL));
    pEvent = Tasker::AllocEvent("");
    InternalAddEvent(pEvent, filterType, bIlwertFilter, filterData, true);
    return pEvent;
}

//-----------------------------------------------------------------------------
//! \brief Free a MODSEvent and remove it from the MODS event map
//!
//! \param pEvent  : Pointer to the MODS event to free and remove from the
//!                  MODS event map
//!
void PMU::FreeEvent(ModsEvent * pEvent)
{
    MASSERT(m_Initialized);
    if (pEvent == NULL)
        return;
    MASSERT(m_ModsEventData.find(pEvent) != m_ModsEventData.end());
    MASSERT(m_ModsEventData[pEvent].bEventAlloc);
    InternalDeleteEvent(pEvent);
    Tasker::FreeEvent(pEvent);
}

//-----------------------------------------------------------------------------
//! \brief Add an event to the MODS event map with the specified filter
//!        information
//!
//! \param pEvent         : Pointer to the MODS event to add
//! \param filterType     : Filter type associated with the MODS event
//! \param bIlwertFilter  : True to ilwert the sense of the filter type.  I.e.
//!                         ilwerting EVENT_UNIT_ID results in notifications for
//!                         all unitIDs except the specified unit ID
//! \param filterData     : Filter data associated with the MODS event
//!
//! \return OK
//!
RC PMU::AddEvent
(
    ModsEvent        *pEvent,
    MessageFilterType filterType,
    bool              bIlwertFilter,
    UINT32            filterData
)
{
    MASSERT(m_Initialized);
    MASSERT(!bIlwertFilter || (filterType != ALL));
    MASSERT(m_ModsEventData.find(pEvent) == m_ModsEventData.end());
    InternalAddEvent(pEvent, filterType, bIlwertFilter, filterData, false);
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Delete a MODS event from the MODS event map
//!
//! \param pEvent      : Pointer to the MODS event to delete
//!
void PMU::DeleteEvent(ModsEvent *pEvent)
{
    MASSERT(m_Initialized);
    MASSERT(m_ModsEventData.find(pEvent) != m_ModsEventData.end());
    MASSERT(!m_ModsEventData[pEvent].bEventAlloc);
    InternalDeleteEvent(pEvent);
}

//-----------------------------------------------------------------------------
//! \brief Wait for the next message (command response or event) that matches
//!        the specified filter
//!
//! \param filterType     : Filter type to wait for
//! \param filterData     : Filter data to wait for
//! \param timeoutMs      : Time to wait for the message for in ms
//!                         (Tasker::NO_TIMEOUT means no timeout)
//!
//! \return OK if the a matching message was received within the specified time
//!         not OK otherwise
//!
RC PMU::WaitMessage
(
    PMU::MessageFilterType filterType,
    UINT32                 filterData,
    FLOAT64                timeoutMs
)
{
    ModsEvent *pEvent;
    RC         rc;

    MASSERT(m_Initialized);

    pEvent = AllocEvent(filterType, false, filterData);
    rc = Tasker::WaitOnEvent(pEvent, timeoutMs);
    FreeEvent(pEvent);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Retreive any messages for the provided MODS event
//!
//! \param pEvent       : Pointer to the event to retrieve messages for
//! \param pMessages    : Pointer to an array of messages for returning
//!                       matching retrieved messages
//! \param messageCount : The number of messages that can be stored in the
//!                       provided array.  Upon exit indicates the number of
//!                       valid messages in the array
//!
//! \return OK
//!
RC PMU::GetMessages
(
    ModsEvent      *pEvent,
    vector<PMU::Message> *pMessages
)
{
    list<EventListEntry *>::iterator   eventIter;
    list<ModsEvent *>::iterator        modsEventIter;
    bool                               bFoundModsEvent = false;

    MASSERT(m_Initialized);

    Tasker::MutexHolder pmuMutex(m_PMUMutex);

    eventIter = m_EventList.begin();
    while (eventIter != m_EventList.end())
    {
        bFoundModsEvent = false;

        // For each event in the event list, look for the provided MODS event
        // in its MODS event list.  If the MODS event matches then copy the
        // message into the provided storage
        for (modsEventIter = (*eventIter)->modsEvents.begin();
             modsEventIter != (*eventIter)->modsEvents.end();
             modsEventIter++)
        {
            if (*modsEventIter == pEvent)
            {
                pMessages->push_back((*eventIter)->message);
                (*eventIter)->modsEvents.remove(pEvent);
                bFoundModsEvent = true;
                break;
            }
        }

        // If there are no more consumers for his command, free it
        if (bFoundModsEvent && (*eventIter)->modsEvents.empty())
        {
            delete *eventIter;
            eventIter = m_EventList.erase(eventIter);
        }
        else
        {
            eventIter++;
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Bind a surface onto a PMU aperture
//!
//! \param pSurf      : Pointer to the surface to bind
//! \param aperture   : The PMU aperture to bind the surface onto
//! \param pVaddr     : Pointer to return the PMU virtual address for the
//!                     mapped memory
//!
//! \return OK if binding succeeds, not OK otherwise
//!
RC PMU::BindSurface
(
    Surface2D *pSurf,
    UINT08     aperture,
    PHYSADDR  *pVaddr
)
{
    RC      rc;
    LwRmPtr pLwRm;
    bool    bBindAperture = false;

    MASSERT(m_Initialized);

    MASSERT(pSurf->GetAddressModel() == Memory::Paging);
    if (!pSurf->IsGpuObjectMapped(m_Handle))
    {
        CHECK_RC(pSurf->MapToGpuObject(m_Handle));
        bBindAperture = true;
    }
    *pVaddr = pSurf->GetCtxDmaOffsetGpuObject(m_Handle);

    if (bBindAperture)
    {
        LW85B6_CTRL_PMU_BIND_APERTURE_PARAMS pmuParams = { 0, };

        // Bind the memory to the PMU aperture
        pmuParams.hMemory = pSurf->GetMemHandle();
        pmuParams.apertureIndex = aperture;
        pmuParams.vAddr = *pVaddr;
        CHECK_RC(pLwRm->Control(m_Handle,
                                LW85B6_CTRL_CMD_PMU_BIND_APERTURE,
                                &pmuParams,
                                sizeof (pmuParams)));
    }
    else
    {
        Printf(Tee::PriNormal, "Surface %s already bound to PMU[%d:%d]\n",
               pSurf->GetName().c_str(),
               m_Parent->GetParentDevice()->GetDeviceInst(),
               m_Parent->GetSubdeviceInst());
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Set power gating parameters
//!
//! \param parameters : The parameters to set
//!
//! \return OK if setting parameters succeeds, not OK otherwise
//!
RC PMU::SetPowerGatingParameters
(
    vector<LW2080_CTRL_POWERGATING_PARAMETER> *pParameters,
    UINT32 Flags
)
{
    LW2080_CTRL_MC_SET_POWERGATING_PARAMETER_PARAMS params = { 0, };
    RC rc;
    vector<LW2080_CTRL_POWERGATING_PARAMETER> supportedParamsList;

    MASSERT(pParameters);
    MASSERT(pParameters->size() > 0);

    for (const auto &itr : *pParameters)
    {
        if ((m_Parent->HasFeature(Device::GPUSUB_HAS_ELPG)) ||
            (itr.parameterExtended == PMU::ELPG_MS_ENGINE))
        {
            supportedParamsList.push_back(itr);
        }
    }

    if (supportedParamsList.empty())
    {
#ifdef SIM_BUILD
        // Arch tests still use ELPG args on Pascal and Pascal Fmodel crashes
        // if we disable the code below.
        if (m_Parent->GetParentDevice()->GetFamily() != GpuDevice::Pascal)
#endif
        return RC::UNSUPPORTED_FUNCTION;
    }

    params.flags    = Flags;
    params.listSize = (UINT32)supportedParamsList.size();
    params.parameterList = LW_PTR_TO_LwP64(&(supportedParamsList[0]));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_Parent,
                  LW2080_CTRL_CMD_MC_SET_POWERGATING_PARAMETER,
                  &params, sizeof(params)));

    for (auto paramItr = pParameters->begin(); paramItr != pParameters->end(); ++paramItr)
    {
        for (const auto &supportedParamItr : supportedParamsList)
        {
            if ((paramItr->parameterExtended == supportedParamItr.parameterExtended) &&
                (paramItr->parameter == supportedParamItr.parameter))
            {
                paramItr->parameterValue = supportedParamItr.parameterValue;
                break;
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the power gating parameters
//!
//! \param pParameters        : Pointer to returned parameters
//!
//! \return OK if successful, not OK otherwise
RC PMU::GetPowerGatingParameters
(
    vector<LW2080_CTRL_POWERGATING_PARAMETER> *pParameters
)
{
    LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS params = { 0 };
    RC rc;
    vector<LW2080_CTRL_POWERGATING_PARAMETER> supportedParamsList;

    // At least one threshold must be queried otherwise there is no point
    MASSERT(pParameters);
    MASSERT(pParameters->size());

    for (const auto &itr : *pParameters)
    {
        if ((m_Parent->HasFeature(Device::GPUSUB_HAS_ELPG)) ||
            (itr.parameterExtended == PMU::ELPG_MS_ENGINE))
        {
            supportedParamsList.push_back(itr);
        }
    }

    if (supportedParamsList.empty())
    {
#ifdef SIM_BUILD
        // Arch tests still use ELPG args on Pascal and Pascal Fmodel crashes
        // if we disable the code below.
        if (m_Parent->GetParentDevice()->GetFamily() != GpuDevice::Pascal)
#endif
        return RC::UNSUPPORTED_FUNCTION;
    }

    params.listSize = (UINT32)supportedParamsList.size();
    params.parameterList = LW_PTR_TO_LwP64(&(supportedParamsList[0]));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_Parent,
                                LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                &params, sizeof(params)));

    for (auto paramItr = pParameters->begin(); paramItr != pParameters->end(); ++paramItr)
    {
        for (const auto &supportedParamItr : supportedParamsList)
        {
            if ((paramItr->parameterExtended == supportedParamItr.parameterExtended) &&
                (paramItr->parameter == supportedParamItr.parameter))
            {
                paramItr->parameterValue = supportedParamItr.parameterValue;
                break;
            }
        }
    }

    return rc;
}
//-----------------------------------------------------------------------------
RC PMU::GetPowerGateSupportEngineMask(UINT32 *pEngineMask)
{
    MASSERT(pEngineMask);
    RC rc;
    if (!m_SupportedElpgQueried)
    {
        vector<LW2080_CTRL_POWERGATING_PARAMETER> Param(4);
        Param[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_SUPPORTED;
        Param[0].parameterExtended = PMU::ELPG_GRAPHICS_ENGINE;
        Param[1].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_SUPPORTED;
        Param[1].parameterExtended = PMU::ELPG_VIDEO_ENGINE;
        Param[2].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_SUPPORTED;
        Param[2].parameterExtended = PMU::ELPG_VIC_ENGINE;
        Param[3].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_SUPPORTED;
        Param[3].parameterExtended = PMU::ELPG_MS_ENGINE;

        CHECK_RC(GetPowerGatingParameters(&Param));
        for (UINT32 i = 0; i < Param.size(); i++)
        {
            if (0 != Param[i].parameterValue)
            {
                m_SupportedElpg |= 1<<Param[i].parameterExtended;
            }
        }
        m_SupportedElpgQueried = true;
    }

    *pEngineMask = m_SupportedElpg;
    return rc;
}
//-----------------------------------------------------------------------------
RC PMU::GetPowerGateEnableEngineMask(UINT32 *pEngineMask)
{
    MASSERT(pEngineMask);
    RC rc;
    UINT32 SupportedMask;
    CHECK_RC(GetPowerGateSupportEngineMask(&SupportedMask));

    // Initialize pEngineMask
    *pEngineMask = 0;

    vector<LW2080_CTRL_POWERGATING_PARAMETER> Param;
    if (SupportedMask & (1<<PMU::ELPG_GRAPHICS_ENGINE))
    {
        LW2080_CTRL_POWERGATING_PARAMETER NewParam;
        NewParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        NewParam.parameterExtended = ELPG_GRAPHICS_ENGINE;
        Param.push_back(NewParam);
    }
    if (SupportedMask & (1<<PMU::ELPG_VIDEO_ENGINE))
    {
        LW2080_CTRL_POWERGATING_PARAMETER NewParam;
        NewParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        NewParam.parameterExtended = ELPG_VIDEO_ENGINE;
        Param.push_back(NewParam);
    }
    if (SupportedMask & (1<<PMU::ELPG_VIC_ENGINE))
    {
        LW2080_CTRL_POWERGATING_PARAMETER NewParam;
        NewParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        NewParam.parameterExtended = ELPG_VIC_ENGINE;
        Param.push_back(NewParam);
    }
    if (SupportedMask & (1<<PMU::ELPG_MS_ENGINE))
    {
        LW2080_CTRL_POWERGATING_PARAMETER NewParam;
        NewParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        NewParam.parameterExtended = ELPG_MS_ENGINE;
        Param.push_back(NewParam);
    }

    if (0 != Param.size())
    {
        CHECK_RC(GetPowerGatingParameters(&Param));
        for (UINT32 i = 0; i < Param.size(); i++)
        {
            if (1 == Param[i].parameterValue)
            {
                *pEngineMask |= 1<<Param[i].parameterExtended;
            }
        }
    }

    return rc;
}

RC PMU::SetPowerGateEnableEngineMask(UINT32 EnableMask)
{
    RC rc;
    UINT32 SupportedMask;
    CHECK_RC(GetPowerGateSupportEngineMask(&SupportedMask));
    vector<LW2080_CTRL_POWERGATING_PARAMETER> Param;
    if (SupportedMask & (1<<PMU::ELPG_GRAPHICS_ENGINE))
    {
        LW2080_CTRL_POWERGATING_PARAMETER NewParam;
        NewParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        NewParam.parameterExtended = ELPG_GRAPHICS_ENGINE;
        NewParam.parameterValue = (EnableMask & (1<<PMU::ELPG_GRAPHICS_ENGINE))!= 0;
        Param.push_back(NewParam);
    }
    if (SupportedMask & (1<<PMU::ELPG_VIDEO_ENGINE))
    {
        LW2080_CTRL_POWERGATING_PARAMETER NewParam;
        NewParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        NewParam.parameterExtended = ELPG_VIDEO_ENGINE;
        NewParam.parameterValue = (EnableMask & (1<<PMU::ELPG_VIDEO_ENGINE))!= 0;
        Param.push_back(NewParam);
    }
    if (SupportedMask & (1<<PMU::ELPG_VIC_ENGINE))
    {
        LW2080_CTRL_POWERGATING_PARAMETER NewParam;
        NewParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        NewParam.parameterExtended = ELPG_VIC_ENGINE;
        NewParam.parameterValue = (EnableMask & (1<<PMU::ELPG_VIC_ENGINE))!= 0;
        Param.push_back(NewParam);
    }
    if (SupportedMask & (1<<PMU::ELPG_MS_ENGINE))
    {
        LW2080_CTRL_POWERGATING_PARAMETER NewParam;
        NewParam.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        NewParam.parameterExtended = ELPG_MS_ENGINE;
        NewParam.parameterValue = (EnableMask & (1<<PMU::ELPG_MS_ENGINE))!= 0;
        Param.push_back(NewParam);
    }

    if (0 == Param.size())
    {
        return OK;
    }

    CHECK_RC(SetPowerGatingParameters(&Param, 0));
    return rc;
}
//-----------------------------------------------------------------------------
//! \brief Print out the current power gating parameters
//!
//! \param printPri        : Priority to perform the prints at
//! \param engine          : The engine to print the the thresholds for
//!                          (graphics/video/all)
//! \param bShowThresholds : true if the thresholds should be printed along
//!                          with the engine gating/deny counts
void PMU::PrintPowerGatingParams(Tee::Priority printPri,
                                 PgTypes       type,
                                 bool          bShowThresholds)
{
    // No point in doing all the RM calls if nothing will end up being printed
    if (!Tee::WillPrint(printPri) ||
        !m_Parent->HasFeature(Device::GPUSUB_HAS_ELPG))
        return;

    if (type > PMU::ELPG_ALL_ENGINES)
        return;

    UINT32  startEngine = type;
    UINT32  lwrEngine   = type;
    UINT32  endEngine   = type + 1;
    UINT32  lwrParam;
    LwRmPtr pLwRm;
    LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS params = { 0 };
    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams;

    if (type == ELPG_ALL_ENGINES)
    {
        pgParams.resize(1);
        pgParams[0].parameter =
            LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES;
        pgParams[0].parameterExtended = 0;
        params.listSize = 1;
        params.parameterList = LW_PTR_TO_LwP64(&pgParams[0]);
        if (OK != pLwRm->ControlBySubdevice(m_Parent,
                                LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                &params, sizeof(params)))
        {
            Printf(printPri,
                   "PrintPowerGatingParams : Unable to get parameters\n");
            return;
        }
        startEngine = 0;
        endEngine = pgParams[0].parameterValue;
    }

    pgParams.clear();
    vector<LW2080_CTRL_POWERGATING_PARAMETER> lwrQueryParam;
    if (bShowThresholds)
    {
        for (lwrParam = LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD;
             lwrParam <= LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES;
             lwrParam++)
        {
            if (!IsPerEngineParameter(lwrParam) &&
                (lwrParam !=
                 LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES))
            {
                LW2080_CTRL_POWERGATING_PARAMETER param = {0};
                param.parameter = lwrParam;
                param.parameterExtended = 0;
                param.parameterValue = 0;
                lwrQueryParam.clear();
                lwrQueryParam.push_back(param);
                if (OK == GetPowerGatingParameters(&lwrQueryParam))
                {
                    pgParams.push_back(lwrQueryParam[0]);
                }
            }
        }
    }

    // The RM calls can generate output, so collect all the data first and
    // then print it out in a nice tabular format
    for (lwrEngine = startEngine ; lwrEngine < endEngine; lwrEngine++)
    {
        if (!IsPowerGatingSupported((PMU::PgTypes)lwrEngine))
            continue;

        for (lwrParam = LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD;
              lwrParam <= LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES;
              lwrParam++)
        {
            if (IsPerEngineParameter(lwrParam) &&
                (bShowThresholds ||
                 !IsThresholdParameter(lwrParam)))
            {

                LW2080_CTRL_POWERGATING_PARAMETER param = {0};
                param.parameter = lwrParam;
                param.parameterExtended = lwrEngine;
                param.parameterValue = 0;
                lwrQueryParam.clear();
                lwrQueryParam.push_back(param);
                if (OK == GetPowerGatingParameters(&lwrQueryParam))
                {
                    pgParams.push_back(lwrQueryParam[0]);
                }
            }
        }
    }

    if (pgParams.size() == 0)
    {
        Printf(printPri,
               "PrintPowerGatingParams : No valid parameters found\n");
        return;
    }

    Printf(printPri,
           "*************************   Power Gating Parameters   "
           "*************************\n");
    lwrParam = 0;
    while ((lwrParam < pgParams.size()) &&
           !IsPerEngineParameter(pgParams[lwrParam].parameter))
    {
        if (lwrParam == 0)
        {
            Printf(printPri, "Non-Engine parameters\n");
        }
        if (m_PGParamString.count(pgParams[lwrParam].parameter))
        {
            Printf(printPri, "   %s : %d\n",
                   m_PGParamString[pgParams[lwrParam].parameter].c_str(),
                   (UINT32)pgParams[lwrParam].parameterValue);
        }
        else
        {
            Printf(printPri, "   Unknown Parameter : %d\n",
                   (UINT32)pgParams[lwrParam].parameterValue);
        }
        lwrParam++;
    }

    if (lwrParam == pgParams.size())
        return;

    UINT32 headerParam = lwrParam;

    Printf(printPri, "Per-Engine parameters\n");
    Printf(printPri, "   Eng");
    lwrEngine = pgParams[headerParam].parameterExtended;
    while ((headerParam < pgParams.size()) &&
           (pgParams[headerParam].parameterExtended == lwrEngine))
    {
        if (m_PGParamString.count(pgParams[headerParam].parameter))
        {
            Printf(printPri, "  %s",
                   m_PGParamString[pgParams[headerParam].parameter].c_str());
        }
        else
        {
            Printf(printPri, "     Unknown");
        }
        headerParam++;
    }
    Printf(printPri, "\n");

    while (lwrParam < pgParams.size())
    {
        lwrEngine = pgParams[lwrParam].parameterExtended;
        Printf(printPri, "     %d", lwrEngine);
        do
        {
            Printf(printPri, "  %10d", (UINT32)pgParams[lwrParam].parameterValue);
            lwrParam++;
            if (lwrParam >= pgParams.size())
                break;
        }
        while (lwrEngine == pgParams[lwrParam].parameterExtended);
        Printf(printPri, "\n");
    }
}

//-----------------------------------------------------------------------------
//! \brief Force an engine to issue PG_ON through RM
//!
//! \param Engine     : Engine to wait for power gating on
//! \param TimeoutMs  : Timeout to wait for
//!
//! \return OK if the engine was successfully power gated, not OK otherwise
RC PMU::PowerGateOn(PgTypes Type, FLOAT64 TimeoutMs)
{
    StickyRC firstRc;
    ElpgWait PowerGater(m_Parent, Type);

    firstRc = PowerGater.Initialize();
    if (OK == firstRc)
    {
        firstRc = PowerGater.Wait(PwrWait::PG_ON, TimeoutMs);
        firstRc = PowerGater.Teardown();
    }
    return firstRc;
}
//-----------------------------------------------------------------------------
//! \brief Ask RM if the power gating 'Type' is already ON.
//!
//! \param Type     : Type of power-gating to check for
bool PMU::IsPowerGated(PgTypes Type)
{
    vector<LW2080_CTRL_POWERGATING_PARAMETER> Param(1);

    switch (Type)
    {
        case ELPG_GRAPHICS_ENGINE:
        case ELPG_VIDEO_ENGINE:
        case ELPG_VIC_ENGINE:
            Param[0].parameterExtended = Type;
            Param[0].parameter  = LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE;

            if (OK != GetPowerGatingParameters(&Param))
                return false;

            return (Param[0].parameterValue ==
                LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF);

        case PWR_RAIL:
            Param[0].parameter  = LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_PWR_RAIL;

            if (OK != GetPowerGatingParameters(&Param))
                return false;

            return (Param[0].parameterValue ==
                LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_PWR_RAIL_PWR_OFF);

        default:
            break;
    }

    MASSERT(!"Unsupported Power gating type.");
    return false;
}
//-----------------------------------------------------------------------------
//! \brief Ask RM if the power gating 'Type' is Supported
//!
//! \param Type     : PG Engine to check for
bool PMU::IsPowerGatingSupported(PgTypes Type)
{
    vector<LW2080_CTRL_POWERGATING_PARAMETER> Param(1);

    switch (Type)
    {
        case ELPG_GRAPHICS_ENGINE:
        case ELPG_VIDEO_ENGINE:
        case ELPG_VIC_ENGINE:
        case ELPG_MS_ENGINE:
            Param[0].parameterExtended = Type;
            Param[0].parameter  = LW2080_CTRL_MC_POWERGATING_PARAMETER_SUPPORTED;

            if (OK != GetPowerGatingParameters(&Param))
                return false;

            return (Param[0].parameterValue == 1);

        case PWR_RAIL:
            Param[0].parameter  = LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_SUPPORTED;

            if (OK != GetPowerGatingParameters(&Param))
                return false;

            return (Param[0].parameterValue == 1);

        default:
            break;
    }

    return false;
}

//-----------------------------------------------------------------------------
//! \brief Check whether deep idle is supported
//!
//! \return true if deep idle is supported, false otherwise
//!
bool PMU::IsDeepIdleSupported()
{
    UINT32 opsb = 0;
    PexDevice *pPexDev;
    UINT32        upStreamIdx;
    UINT32        aspmState = Pci::ASPM_L1;
    Perf *pPerf = m_Parent->GetPerf();
    bool bRequiredPStateExit = true;

    // Note : to prevent test escapes for tests that use this call as part of
    // their IsSupported call, if any of the individual queries fail, ensure
    // that that portion of the check returns true.

    if (pPerf == NULL)
    {
        Printf(Tee::PriNormal, "Perf object not available\n");
        return false;
    }

    vector <UINT32> DeepIdlePStates;
    if (OK != pPerf->GetDIPStates(&DeepIdlePStates))
    {
        Printf(Tee::PriWarn,
               "PMU::IsDeepIdleSupported : Error getting DI Pstates\n");
        bRequiredPStateExit = true;
    }
    for (UINT32 ps = 0; ps < DeepIdlePStates.size(); ps++)
    {
        if (OK != pPerf->DoesPStateExist(DeepIdlePStates[ps], &bRequiredPStateExit))
        {
            Printf(Tee::PriWarn,
                   "PMU::IsDeepIdleSupported : Error checking for PState %d\n",
                   DeepIdlePStates[ps]);
            bRequiredPStateExit = true;
        }
        else if (bRequiredPStateExit)
        {
            break; //Atleast there is one pstate which supports DI
        }
    }

    if ((OK != m_Parent->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &upStreamIdx)) ||
        (pPexDev == NULL) ||
        (OK != pPexDev->GetDownStreamAspmState(&aspmState, upStreamIdx)))
    {
        #if (defined(LW_MACINTOSH)) // For Mac builds make this PriLow print
            Printf(Tee::PriLow,
                   "PMU::IsDeepIdleSupported : Error getting ASPM state\n");
        #else
            Printf(Tee::PriWarn,
                   "PMU::IsDeepIdleSupported : Error getting ASPM state\n");
        #endif
        aspmState = Pci::ASPM_L1;
    }

    // TODO : RM Support for deep idle query is pending, once that is available
    // replace this registry read
    if (OK != Registry::Read("ResourceManager", LW_REG_STR_RM_OPSB, &opsb))
    {
        Printf(Tee::PriWarn,
               "PMU::IsDeepIdleSupported : Error reading OPSB registry key\n");
        opsb = DRF_DEF(_REG_STR, _RM_OPSB, _CLKREQ, _ENABLE) |
               DRF_DEF(_REG_STR, _RM_OPSB, _DEEP_L1, _ENABLE) |
               DRF_DEF(_REG_STR, _RM_OPSB, _DEEP_IDLE, _ENABLE);
    }

    return (((aspmState == Pci::ASPM_L1) || (aspmState == Pci::ASPM_L0S_L1)) &&
            bRequiredPStateExit &&
            FLD_TEST_DRF(_REG_STR, _RM_OPSB, _CLKREQ, _ENABLE, opsb) &&
            FLD_TEST_DRF(_REG_STR, _RM_OPSB, _DEEP_L1, _ENABLE, opsb) &&
            FLD_TEST_DRF(_REG_STR, _RM_OPSB, _DEEP_IDLE, _ENABLE, opsb));
}

//-----------------------------------------------------------------------------
//! \brief Get the current Power gate counts
//! Under the control of PMU now
RC PMU::GetLwrrentPgCount(UINT32 engineId, PgCount *pNewCnt)
{
    RC rc;

    MASSERT(pNewCnt);
    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams(2);
    pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGCOUNT;
    pgParams[0].parameterExtended = engineId;
    pgParams[1].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_UNGATINGCOUNT;
    pgParams[1].parameterExtended = engineId;

    CHECK_RC(GetPowerGatingParameters(&pgParams));

    pNewCnt->pwrOn  = pgParams[0].parameterValue;
    pNewCnt->pwrOff = pgParams[1].parameterValue;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the current Power gate timers
RC PMU::GetLwrrentPgTimeUs(UINT32 engineId, UINT32* pInGateTime, UINT32* pUnGateTime)
{
    RC rc;

    MASSERT(pInGateTime);
    MASSERT(pUnGateTime);
    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams(2);
    pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGTIME_US;
    pgParams[0].parameterExtended = engineId;
    pgParams[1].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_UNGATINGTIME_US;
    pgParams[1].parameterExtended = engineId;

    CHECK_RC(GetPowerGatingParameters(&pgParams));

    *pInGateTime = pgParams[0].parameterValue;
    *pUnGateTime = pgParams[1].parameterValue;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the current values for deep idle statistics
//!
//! \param Type      : The type of deep idle statistics to acquire
//! \param pStats    : Pointer to returned statistics
//!
//! \return OK if the statistics were successfully acquired, not OK otherwise
//!
RC PMU::GetDeepIdleStatistics
(
    DeepIdleStatType Type,
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStats
)
{
    PollDeepIdleStruct pollStruct;
    RC                 rc;
    LW2080_CTRL_GPU_INITIATE_DEEP_IDLE_STATISTICS_READ_PARAMS initiateParams = { 0 };
    LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_PARAMS modeParams = { 0 };
    LwRmPtr            pLwRm;

    modeParams.deepIdleMode = Type;
    modeParams.reset = LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_RESET_NO;
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(m_Parent),
                            LW2080_CTRL_CMD_GPU_SET_DEEP_IDLE_STATISTICS_MODE,
                            &modeParams, sizeof(modeParams)));

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(m_Parent),
                            LW2080_CTRL_CMD_GPU_INITIATE_DEEP_IDLE_STATISTICS_READ,
                            &initiateParams, sizeof(initiateParams)));

    pollStruct.pPmu    = this;
    pollStruct.pParams = pStats;
    CHECK_RC(POLLWRAP(PollGetDeepIdleStats,
                      &pollStruct,
                      DEEP_IDLE_STATS_TIMEOUT));
    return pollStruct.pollRc;
}

//-----------------------------------------------------------------------------
//! \brief Reset the deep idle statistics to zero
//!
//! \param Type      : The type of deep idle statistics to reset
//!
//! \return OK if the statistics were successfully reset, not OK otherwise
//!
RC PMU::ResetDeepIdleStatistics(DeepIdleStatType Type)
{
    LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_PARAMS params = { 0 };
    params.deepIdleMode = Type;
    params.reset = LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_RESET_YES;
    return LwRmPtr()->Control(LwRmPtr()->GetSubdeviceHandle(m_Parent),
                              LW2080_CTRL_CMD_GPU_SET_DEEP_IDLE_STATISTICS_MODE,
                              &params, sizeof(params));
}

bool PMU::IsMscgSupported()
{
    return IsPowerGatingSupported(ELPG_MS_ENGINE);
}

//-----------------------------------------------------------------------------
//! \brief Check whether a LPWR feature is supported at the specified PState
RC PMU::PStateSupportsLpwrFeature(UINT32 PStateNum, LpwrFeature feature, bool* pSupported)
{
    MASSERT(pSupported);
    *pSupported = false;

    RC rc;

    LW2080_CTRL_LPWR_FEATURE_PARAMETER_GET_PARAMS params = {};
    params.listSize = 1;
    params.list[0].param.id      = LW2080_CTRL_LPWR_PARAMETER_ID_PG_CTRL_PSTATE_SUPPORT_MASK;

    switch (feature)
    {
        case LpwrFeature::MSCG:
            params.list[0].feature.id    = LW2080_CTRL_LPWR_FEATURE_ID_PG;
            params.list[0].feature.subId = LW2080_CTRL_LPWR_SUB_FEATURE_ID_MS;
            break;
        case LpwrFeature::TPCPG:
            params.list[0].feature.id    = LW2080_CTRL_LPWR_FEATURE_ID_PG;
            params.list[0].feature.subId = LW2080_CTRL_LPWR_SUB_FEATURE_ID_GRAPHICS;
            break;
        default:
            MASSERT(!"Unknown lpwr feature");
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(LwRmPtr()->ControlBySubdevice
    (
        m_Parent,
        LW2080_CTRL_CMD_LPWR_FEATURE_PARAMETER_GET,
        &params,
        sizeof(params)
    ));

    CHECK_RC(PMU::CheckForLwrmLpwrError(params, 1, __FUNCTION__));

    *pSupported = (params.list[0].param.val & (1u << PStateNum)) != 0;

    return rc;
}

RC PMU::GetMscgEnabled(bool* pEnabled)
{
    RC rc;
    MASSERT(pEnabled);

    UINT32 engineMask = 0;
    CHECK_RC(GetPowerGateEnableEngineMask(&engineMask));
    *pEnabled = (engineMask & (1u << ELPG_MS_ENGINE)) != 0;

    return OK;
}

RC PMU::SetMscgEnabled(bool enable)
{
    RC rc;

    vector<LW2080_CTRL_POWERGATING_PARAMETER> params(1);
    params[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
    params[0].parameterExtended = ELPG_MS_ENGINE;
    params[0].parameterValue = enable;

    CHECK_RC(SetPowerGatingParameters(&params, 0u));

    bool nowEnabled;
    CHECK_RC(GetMscgEnabled(&nowEnabled));
    if (nowEnabled != enable)
    {
        Printf(Tee::PriError, "SetMscgEnabled Could not set!\n");
        return RC::LWRM_ERROR;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the number of times MSCG has been entered since it was enabled
RC PMU::GetMscgEntryCount(UINT32* pCount)
{
    RC rc;

    PgCount pgCount = {};
    CHECK_RC(PMU::GetLwrrentPgCount(ELPG_MS_ENGINE, &pgCount));
    *pCount = pgCount.pwrOff;

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Static event handler for handling PMU Event Notifier OS events
//!
void PMU::EventHandler(void *pData)
{
    PMU * pPmu = (PMU*)pData;
    while (pPmu->m_PauseCount > 0)
    {
        pPmu->m_IsPaused |= EVENT_HANDLER_PAUSED;
        Tasker::Yield();
    }
    pPmu->m_IsPaused &= ~EVENT_HANDLER_PAUSED;
    pPmu->ProcessEvents();
}

//-----------------------------------------------------------------------------
//! \brief Static event handler for handling PMU Command Notifier OS events
//!
void PMU::CommandHandler(void *pData)
{
    PMU * pPmu = (PMU*)pData;
    while (pPmu->m_PauseCount > 0)
    {
        pPmu->m_IsPaused |= COMMAND_HANDLER_PAUSED;
        Tasker::Yield();
    }
    pPmu->m_IsPaused &= ~COMMAND_HANDLER_PAUSED;
}

//-----------------------------------------------------------------------------
//! \brief Pause the CommandHandler & EventHandler processing. This is necessary
//! to suppport GC6 Entry/Exit processing.
//!
void PMU::Pause( bool bPause)
{
    if(bPause)
    {
        m_PauseCount++;
        if(m_PauseCount == 1)
        {
            // force the cmd & event handlers to run so they can set their
            // IsPaused bit.
            Tasker::SetEvent(m_EventTask.pEventThread->GetEvent());
            Tasker::SetEvent(m_CommandTask.pEventThread->GetEvent());
        }
    }
    else if(m_PauseCount > 0)
    {
        m_PauseCount--;
    }
}

//-----------------------------------------------------------------------------
//! \brief Return true if the CommandHandler & EventHandler have been paused.
bool PMU::IsPaused()
{
    return (m_IsPaused == ALL_PAUSED);
}

//-----------------------------------------------------------------------------
//! \brief Non-static event handler for PMU Event Notifier OS events
//!
void PMU::ProcessEvents()
{
    map<ModsEvent *, ModsEventData>::iterator modsEventMapIter;
    set<ModsEvent *>                          sendEvents;
    EventListEntry                           *pNewEvent;

    UINT32  head, tail, bytesRead;
    bool    bEventsToProcess;
    UINT08 *pQueue;

    // Need to lock the mutex for the entire duration of processing events to
    // make sure that events do not get deleted after they are added to the
    // sendEvents set
    Tasker::MutexHolder pmuMutex(m_PMUMutex);

    for (UINT32 unitIdIdx = 0; unitIdIdx < s_RegisteredUnitCount; unitIdIdx++)
    {
        head = MEM_RD32(m_EventQueues[unitIdIdx].pHead);
        tail = MEM_RD32(m_EventQueues[unitIdIdx].pTail);
        pQueue = m_EventQueues[unitIdIdx].pQueue;

        bEventsToProcess = (head != tail);
        while (bEventsToProcess)
        {
            pNewEvent = new EventListEntry;

            // If retrieving the message fails, there was not enough data to
            // constitute a full message.  Stop processing events from this
            // queue at that point
            bytesRead = EventQueueGetBytes((UINT08 *)&pNewEvent->message.format,
                             pQueue, head, tail, sizeof(LwU32));
            if (bytesRead == sizeof(LwU32))
            {
                tail += ROUND32(bytesRead);
                if (tail >= EVENT_QUEUE_DATA_SIZE)
                    tail -= EVENT_QUEUE_DATA_SIZE;
                switch (pNewEvent->message.format)
                {
                    case PMU_MESSAGE:
                        bytesRead = EventQueueGetBytes((UINT08 *)&pNewEvent->message.data.msg.hdr,
                                     pQueue, head, tail, RM_FLCN_QUEUE_HDR_SIZE);
                        if (bytesRead == RM_FLCN_QUEUE_HDR_SIZE)
                        {
                            bytesRead = EventQueueGetBytes(
                                         (UINT08 *)&pNewEvent->message.data.msg,
                                         pQueue, head, tail,
                                         pNewEvent->message.data.msg.hdr.size);
                        }
                        break;
                    case PG_LOG_ENTRY:
                        bytesRead = EventQueueGetBytes((UINT08 *)&pNewEvent->message.data.pgLogEntry,
                                     pQueue, head, tail, sizeof(RM_PMU_PG_LOG_ENTRY));

                        break;
                    default:

                        Printf(Tee::PriError,
                               "Error : Unknown PMU event format %u on unit ID %d\n"
                               "!!! EVENT LOGGING ON THIS UNIT ID IS CORRUPT !!!\n",
                               (unsigned int) pNewEvent->message.format,
                               s_RegisterUnitIds[unitIdIdx]);
                        MASSERT(!"Unknown PMU event format");
                        bytesRead = 0;
                        break;
                }
            }

            if (bytesRead == 0)
            {
                delete pNewEvent;
                bEventsToProcess = false;
                continue;
            }

            tail += ROUND32(bytesRead);
            if (tail >= EVENT_QUEUE_DATA_SIZE)
                tail -= EVENT_QUEUE_DATA_SIZE;

            m_EventList.push_back(pNewEvent);

            if (pNewEvent->message.format == PMU_MESSAGE)
            {
                DumpHeader(&pNewEvent->message.data.msg.hdr, EVENT_HDR);
            }
            if (s_RegisterUnitIds[unitIdIdx] == RM_PMU_UNIT_LPWR)
            {
                PrintPowerGatingParams(Tee::PriLow, ELPG_ALL_ENGINES, false);
            }

            for (modsEventMapIter = m_ModsEventData.begin();
                 modsEventMapIter != m_ModsEventData.end();
                 modsEventMapIter++)
            {
                if (FilterMatches((pNewEvent->message.format == PMU_MESSAGE) ?
                                      pNewEvent->message.data.msg.hdr.unitId : RM_PMU_UNIT_LPWR,
                                  0,
                                  true,
                                  &modsEventMapIter->second))
                {
                    if (!modsEventMapIter->second.bNotifyOnly)
                    {
                        pNewEvent->modsEvents.push_back(
                            modsEventMapIter->first);
                    }
                    sendEvents.insert(modsEventMapIter->first);
                }
            }

            // If a new event was created and the MODS event list is
            // empty, then either this event either has no matching
            // filters or all matching filters are notify only so delete
            // the new event from the event list
            if ((pNewEvent != NULL) && pNewEvent->modsEvents.empty())
            {
                delete pNewEvent;
                m_EventList.remove(pNewEvent);
            }

            // As long as there is data left in the queue, process events
            bEventsToProcess = (head != tail);
        }

        // Update the tail offset.  If the Head offset has changed while the
        // events were being processed, the event thread will cause us to be
        // called again (one notification per block of events, and the event
        // thread counts the notifications to be sure the handler is called
        // the appropriate number of times).
        MEM_WR32(m_EventQueues[unitIdIdx].pTail, tail);
    }

    // Notify all MODS events that are interested in the received events
    if (!sendEvents.empty())
    {
        set<ModsEvent *>::iterator setIter;

        for (setIter = sendEvents.begin();
             setIter != sendEvents.end(); setIter++)
        {
            Tasker::SetEvent(*setIter);
        }
        sendEvents.clear();
    }
}

//-----------------------------------------------------------------------------
//! \brief Check a particular command response or event for a match against
//!        a MODS event
//!
//! \param unitId         : Unit Id for the command or message
//! \param cmdSeq         : Sequence number for the command response associated
//!                         with the message.  Ignored if bEvent is false
//! \param bEvent         : true if the message is an event, false if a command
//!                         response
//! \param pModsEventData : Pointer to the MODS event data to match the message
//!                         against
//!
//! \return true if the command matches the specified MODS event, false
//!         otherwise
//!
bool PMU::FilterMatches(UINT08 unitId, UINT32 cmdSeq, bool bEvent,
                        ModsEventData *pModsEventData)
{
    switch (pModsEventData->filterType)
    {
        case COMMAND_SEQ:
            return (!bEvent &&
                    ((!pModsEventData->bIlwertFilter &&
                      (cmdSeq == pModsEventData->filterData)) ||
                     (pModsEventData->bIlwertFilter &&
                      (cmdSeq != pModsEventData->filterData))));
        case COMMAND_UNIT_ID:
            return (!bEvent &&
                    ((!pModsEventData->bIlwertFilter &&
                      (unitId == pModsEventData->filterData)) ||
                     (pModsEventData->bIlwertFilter &&
                      (unitId != pModsEventData->filterData))));
        case COMMAND_ALL:
            return (pModsEventData->bIlwertFilter == bEvent);
        case EVENT_UNIT_ID:
            return (bEvent &&
                    ((!pModsEventData->bIlwertFilter &&
                      (unitId == pModsEventData->filterData)) ||
                     (pModsEventData->bIlwertFilter &&
                      (unitId != pModsEventData->filterData))));
        case EVENT_ALL:
            return (pModsEventData->bIlwertFilter == !bEvent);
        case ALL_UNIT_ID:
            return ((!pModsEventData->bIlwertFilter &&
                     (unitId == pModsEventData->filterData)) ||
                    (pModsEventData->bIlwertFilter &&
                     (unitId != pModsEventData->filterData)));
        case ALL:
            return true;
        default:
            return false;
    }
}

//-----------------------------------------------------------------------------
//! \brief Add a MODSEvent and its associated filter to the event map
//!
//! \param pEvent         : MODS event to add
//! \param filterType     : Filter type for the MODS event
//! \param bIlwertFilter  : True to ilwert the sense of the filter type.  I.e.
//!                         ilwerting EVENT_UNIT_ID results in notifications for
//!                         all unitIDs except the specified unit ID
//! \param filterData     : Filter data
//! \param bEventAlloc    : true if the associated MODSEvent was allocated
//!                         through AllocEvent
//!
void PMU::InternalAddEvent(ModsEvent             *pEvent,
                           PMU::MessageFilterType filterType,
                           bool                   bIlwertFilter,
                           UINT32                 filterData,
                           bool                   bEventAlloc)
{
    ModsEventData modsEventData;

    modsEventData.bNotifyOnly    = false;
    modsEventData.filterType     = filterType;
    modsEventData.bIlwertFilter  = bIlwertFilter;
    modsEventData.filterData     = filterData;
    modsEventData.bEventAlloc    = bEventAlloc;

    Tasker::MutexHolder pmuMutex(m_PMUMutex);
    m_ModsEventData[pEvent] = modsEventData;
}

//-----------------------------------------------------------------------------
//! \brief Delete a MODSEvent and its associated filter from the event map
//!
//! \param pEvent       : MODS event to delete
//!
void PMU::InternalDeleteEvent(ModsEvent *pEvent)
{
    list<EventListEntry *>::iterator eventIter;

    Tasker::MutexHolder pmuMutex(m_PMUMutex);

    eventIter = m_EventList.begin();
    while (eventIter != m_EventList.end())
    {
        // Remove the ModsEvent from the event list entry
        (*eventIter)->modsEvents.remove(pEvent);

        // Delete the event from the event list if this ModsEvent was the last
        // consumer of this event
        if ((*eventIter)->modsEvents.empty())
        {
            delete *eventIter;
            eventIter = m_EventList.erase(eventIter);
        }
        else
        {
            eventIter++;
        }
    }
    m_ModsEventData.erase(pEvent);
}

//-----------------------------------------------------------------------------
//! \brief Retrieve data from the event queue
//!
//! \param pDst       : Destination for the data
//! \param pQueue     : Pointer to the start of the event queue to retrieve
//!                     data from (source pointer)
//! \param Head       : Head offset into the event queue (endpoint for valid
//!                     data)
//! \param Tail       : Tail offset into the event queue (start of valid data)
//! \param Size       : Number of bytes to read from the queue
//!
//! \return The number of bytes read
UINT32 PMU::EventQueueGetBytes(UINT08 *pDst, UINT08 *pQueue, UINT32 Head,
                               UINT32 Tail, UINT32 Size)
{
    UINT32 remainTillRollover = (EVENT_QUEUE_DATA_SIZE - Tail);

    if (EVENT_QUEUE_VALID(Head, Tail, EVENT_QUEUE_DATA_SIZE) < Size)
        return 0;

    if ((Tail < Head) || (remainTillRollover >= Size))
    {
        CopyFromSurface(pDst, pQueue + Tail, Size);
    }
    else
    {
        CopyFromSurface(pDst, pQueue + Tail, remainTillRollover);
        CopyFromSurface(pDst + remainTillRollover,
                        pQueue,
                        Size - remainTillRollover);
    }

    return Size;
}

//-----------------------------------------------------------------------------
//! \brief Allocate and map a surface for use with the PMU
//!
//! \param pSurface : Pointer to the surface to allocate
//! \param Size     : Size in bytes for the surface
//!
//! \return OK if successful
RC PMU::CreateSurface(Surface2D *pSurface, UINT32 Size)
{
    RC rc;

    Size = ROUND32(Size);
    pSurface->SetWidth(GetWidth(&Size));
    pSurface->SetPitch(Size);
    pSurface->SetHeight(1);
    pSurface->SetColorFormat(ColorUtils::VOID32);
    pSurface->SetLocation(m_SurfaceLoc);
    pSurface->SetKernelMapping(true);
    CHECK_RC(pSurface->Alloc(m_Parent->GetParentDevice()));
    Utility::CleanupOnReturn<Surface2D,void>
        FreeSurface(pSurface,&Surface2D::Free);
    CHECK_RC(pSurface->Fill(0, m_Parent->GetSubdeviceInst()));
    CHECK_RC(pSurface->Map(m_Parent->GetSubdeviceInst()));
    FreeSurface.Cancel();
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the width for a PMU surface based on the desired pitch.  If the
//!        pitch requires alignment to meet bytes per pixel requirement, then
//!        the pitch will also be updated
//!
//! \param pPitch     : Pointer to the desired pitch (altered it it does not
//!                     meet the bytes-per-pixel alignment requirement)
//!
//! \return Width of the surface in pixels
UINT32 PMU::GetWidth(UINT32 *pPitch)
{
    UINT32 bytesPerPixel = ColorUtils::PixelBytes(ColorUtils::VOID32);
    UINT32 width;
    width  = (*pPitch + bytesPerPixel - 1) / bytesPerPixel;
    *pPitch = width * bytesPerPixel;

    return width;
}

//-----------------------------------------------------------------------------
//! \brief Copy that is aware of RTL issue on tesla2 where copying non-dword
//!        aligned / non dword size on a system memory surface does not function
//!        correctly
//!
//! \param pDst           : Destination for data
//! \param pSrc           : Source of data
//! \param Size           : Size of data to copy in bytes
//!
void PMU::CopyFromSurface(void *pDst, void *pSrc, UINT32 Size)
{
    if (m_Parent->HasBug(396471) &&
        (Platform::GetSimulationMode() == Platform::RTL) &&
        (((LwUPtr)pSrc & 0x03) || (Size & 0x03)))
    {
        UINT08 *pCopyFrom = (UINT08 *)pSrc - ((LwUPtr)pSrc & 0x03);
        UINT32  copySize = ROUND32(Size + ((UINT32)(LwUPtr)pSrc & 0x03));

        UINT08 *pData = new UINT08[copySize];

        Platform::MemCopy(pData, pCopyFrom, copySize);

        // Note unaligned pData offset OK with bug 396471, it is not RTL
        // simulator memory.
        Platform::MemCopy(pDst, &pData[((LwUPtr)pSrc & 0x03)], Size);

        delete[] pData;
    }
    else
    {
        Platform::MemCopy(pDst, pSrc, Size);
    }
}

//-----------------------------------------------------------------------------
//! \brief Copy that is aware of RTL issue on tesla2 where copying non-dword
//!        aligned / non dword size on a system memory surface does not function
//!        correctly
//!
//! \param pDst           : Destination for data
//! \param pSrc           : Source of data
//! \param Size           : Size of data to copy in bytes
//! \param bFlushWCBuffer : Whether the CPU WC buffers should be flushed after
//!                         the data copy has been done
//!
void PMU::CopyToSurface(void *pDst,
                        void *pSrc,
                        UINT32 Size,
                        bool bFlushWCBuffer)
{
    if (m_Parent->HasBug(396471) &&
        (Platform::GetSimulationMode() == Platform::RTL) &&
        (((LwUPtr)pDst & 0x03) || (Size & 0x03)))
    {
        UINT08 *pCopyTo = (UINT08 *)pDst - ((LwUPtr)pDst & 0x03);
        UINT32  copySize = ROUND32(Size + ((UINT32)(LwUPtr)pDst & 0x03));

        UINT08 *pData = new UINT08[copySize];
        UINT32 *pData32 = (UINT32 *)pData;;

        memset(pData, 0, copySize);

        *pData32 = MEM_RD32(pCopyTo);
        if (copySize > 4)
        {
            pData32 = (UINT32 *)&pData[copySize - 4];
            *pData32 = MEM_RD32(pCopyTo + copySize - 4);
        }

        // Note unaligned pData offset OK with bug 396471, it is not RTL
        // simulator memory.
        Platform::MemCopy(&pData[((LwUPtr)pDst & 0x03)], pSrc, Size);
        Platform::MemCopy(pCopyTo, pData, copySize);

        delete[] pData;
    }
    else
    {
        Platform::MemCopy(pDst, pSrc, Size);
    }
    if (bFlushWCBuffer)
    {
        // WAR for BUG 848502: Flush the WC buffers so
        // writes to FB surfaces are reflected immediately.
        Platform::FlushCpuWriteCombineBuffer();
    }
}

//-----------------------------------------------------------------------------
//! \brief Wait for the PMU to be ready to receive commands
//!
//! \return OK if the PMU is ready, not OK otherwise
//!
RC PMU::WaitPMUReady()
{
    RC rc;
    LwRmPtr                              pLwRm;
    LW85B6_CTRL_PMU_UCODE_STATE_PARAMS   pmuStateParam = {0};

    // There are race conditions that exist when attempting to verify that the
    // PMU is ready.  It is not possible to simply wait for the INIT message
    // since that may never come.  To avoid the race conditions do the following:
    //
    // 1.  Query the current state, if the PMU is ready return OK
    // 2.  Wait for the INIT event.  Note that there is a small window of time
    //     between checking the current PMU state and waiting for the init event
    //     where the PMU could receive the actual INIT event and set its status
    //     to ready in which case the wait on INIT event will timeout
    // 3.  If waiting for the INIT event times out, then query the PMU state
    //     again to verify that we did not hit the race condition mentioned in
    //     step 2.
    //
    // Note that we cannot simply poll on the RM control to check the uCode
    // state due to bug 535299 where polling on this RM control can actually
    // prevent the INIT message from being processed and therefore the PMU state
    // from going ready

    CHECK_RC(pLwRm->Control(m_Handle,
                            LW85B6_CTRL_CMD_PMU_UCODE_STATE,
                            &pmuStateParam,
                            sizeof (pmuStateParam)));
    if (pmuStateParam.ucodeState == LW85B6_CTRL_PMU_UCODE_STATE_READY)
    {
        return OK;
    }

    rc = WaitMessage(EVENT_UNIT_ID, RM_PMU_UNIT_INIT,
                     Tasker::FixTimeout(BOOTSTRAP_TIMEOUT));

    if (rc == RC::TIMEOUT_ERROR)
    {
        rc.Clear();
        pmuStateParam.ucodeState = 0;
        CHECK_RC(pLwRm->Control(m_Handle,
                                LW85B6_CTRL_CMD_PMU_UCODE_STATE,
                                &pmuStateParam,
                                sizeof (pmuStateParam)));
        if (pmuStateParam.ucodeState == LW85B6_CTRL_PMU_UCODE_STATE_READY)
        {
            return OK;
        }
        else
        {
            rc = RC::TIMEOUT_ERROR;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Dump the header from the PMU command, event or message
//!
//! \param pHdr    : Pointer to the header to dump
//! \param hdrType : Type of header to dump
//!
void PMU::DumpHeader(void *pHdr, PmuHeaderType hdrType)
{
    if (!Tee::WillPrint(Tee::PriDebug))
        return;

    if (hdrType == COMMAND_HDR)
    {
        RM_FLCN_QUEUE_HDR hdr;
        CopyFromSurface(&hdr, pHdr, RM_FLCN_QUEUE_HDR_SIZE);

        Printf(Tee::PriDebug,
               "PMU Command : unitId=0x%02x, size=0x%02x, "
               "ctrlFlags=0x%02x, seqNumId=0x%02x\n",
               hdr.unitId, hdr.size, hdr.ctrlFlags, hdr.seqNumId);
    }
    else
    {
        RM_FLCN_QUEUE_HDR hdr;
        CopyFromSurface(&hdr, pHdr, RM_FLCN_QUEUE_HDR_SIZE);
        Printf(Tee::PriDebug,
               "PMU %s : unitId=0x%02x, size=0x%02x, "
               "ctrlFlags=0x%02x, seqNumId=0x%02x\n",
               (hdrType == EVENT_HDR) ? "Event" : "Message",
               hdr.unitId, hdr.size, hdr.ctrlFlags, hdr.seqNumId);
    }
}

//-----------------------------------------------------------------------------
//! \brief Check if the powergating parameter is per-engine
//!
//! \param parameter : Parameter to check
//!
//! \return true if the parameter is per-engine, false otherwise
bool PMU::IsPerEngineParameter(UINT32 parameter)
{
    return ((parameter !=
             LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES) &&
            (parameter !=
             LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_IDLE_THRESHOLD));
}

//-----------------------------------------------------------------------------
//! \brief Check if the powergating parameter is a threshold parameter
//!
//! \param parameter : Parameter to check
//!
//! \return true if the parameter is a threshold parameter, false otherwise
bool PMU::IsThresholdParameter(UINT32 parameter)
{
    return ((parameter ==
                LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD) ||
            (parameter ==
                LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD) ||
            (parameter ==
                LW2080_CTRL_MC_POWERGATING_PARAMETER_DELAY_INTERVAL) ||
            (parameter ==
                LW2080_CTRL_MC_POWERGATING_PARAMETER_PSORDER) ||
            (parameter ==
                LW2080_CTRL_MC_POWERGATING_PARAMETER_ZONELUT) ||
            (parameter ==
                LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_IDLE_THRESHOLD));
}

//-----------------------------------------------------------------------------
//! \brief Static function to poll for deep idle statistics
//!
//! \param pvArgs : Pointer to deep idle statistics polling structure
//!
//! \return true if polling should terminate, false otherwise
//!
/* static */ bool PMU::PollGetDeepIdleStats(void *pvArgs)
{
    PollDeepIdleStruct *pPollStruct = (PollDeepIdleStruct *)pvArgs;
    LwRmPtr pLwRm;
    pPollStruct->pollRc.Clear();
    pPollStruct->pollRc =
        LwRmPtr()->Control(pLwRm->GetSubdeviceHandle(pPollStruct->pPmu->m_Parent),
                      LW2080_CTRL_CMD_GPU_READ_DEEP_IDLE_STATISTICS,
                      pPollStruct->pParams,
                      sizeof(LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS));
    return (pPollStruct->pollRc == RC::LWRM_STATUS_ERROR_NOT_READY) ? false :
                                                                      true;

}

//-----------------------------------------------------------------------------
//! \brief Free a command list entry
//!
PMU::CommandListEntry::~CommandListEntry()
{
    if (Command.GetMemHandle())
        Command.Unmap();
    if (Command.GetAddress())
        Command.Free();

    if (Message.GetMemHandle())
        Message.Unmap();
    if (Message.GetAddress())
        Message.Free();
}

template<typename RMCTRL_PARAM>
RC PMU::CheckForLwrmLpwrError(const RMCTRL_PARAM& param, UINT32 listSize, const char* func)
{
    MASSERT(func);

    for (UINT32 ii = 0; ii < listSize; ii++)
    {
        if (!FLD_TEST_DRF(2080, _CTRL_LPWR_FEATURE_PARAMETER_FLAG, _SUCCEED, _YES,
                          param.list[0].param.flag))
        {
            // TODO: Change this to PriERROR Tracking: MODSAMPERE-1895
            Printf(Tee::PriLow, "Error: LWRM LPWR RMCTRL failure in %s\n", func);
            return RC::LWRM_ERROR;
        }
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
ElpgOwner::ElpgOwner()
{
    m_pPmu = nullptr;
    m_ElpgOwnerRefCnt = 0;
}

ElpgOwner::~ElpgOwner()
{
    if (m_pPmu)
    {
        // Releasing a lock here if PMU is owned by us
        m_pPmu->SetElpgOwned(false);
        m_pPmu = nullptr;
        m_ElpgOwnerRefCnt = 0;
    }
}

RC ElpgOwner::ClaimElpg(PMU *pPmu)
{
    // lock already owned by someone else
    if (pPmu->IsElpgOwned())
    {
        if (m_ElpgOwnerRefCnt == 0)
            return RC::RESOURCE_IN_USE;
        else
        {
            m_ElpgOwnerRefCnt++;
            return OK;
        }
    }

    // claim pmu obj now
    m_pPmu = pPmu;
    m_pPmu->SetElpgOwned(true);
    m_ElpgOwnerRefCnt = 1;
    return OK;
}

RC ElpgOwner::ReleaseElpg()
{
    // lock is not acquired and attempt made to release it
    if (!m_ElpgOwnerRefCnt)
    {
        return RC::SOFTWARE_ERROR;
    }
    if (m_ElpgOwnerRefCnt > 1)
    {
        m_ElpgOwnerRefCnt--;
        return OK;
    }

    // release pmu obj
    m_pPmu->SetElpgOwned(false);
    m_pPmu = nullptr;
    m_ElpgOwnerRefCnt = 0;
    return OK;
}

//------------------------------------------------------------------------------
// Standard interface to load sections of code/data into the PMU without RM
RC PMU::LoadPmuUcodeSection
(
    UINT32 PmuPort,
    PmuUcodeSectionType SectionType,
    const vector<UINT32> &UcodeSection,
    const vector<PMU::PmuUcodeInfo> &UcodeSectionInfo
)
{
    RC rc;

    MASSERT(UcodeSection.size());
    MASSERT(UcodeSectionInfo.size() >= 2); // atleast OS and 1 app section need to be present

    UINT32 LwrrBlk = 0;

    // Iterate through the ucode sections and load each into the PMU DMEM/IMEM
    for (UINT32 idx = 0; idx < UcodeSectionInfo.size(); idx++)
    {
        switch (SectionType)
        {
            case PMU::PMU_SECTION_TYPE_CODE:
                CHECK_RC(LoadPmuImem(PmuPort,
                                             LwrrBlk,
                                             UcodeSection,
                                             UcodeSectionInfo[idx]));
                break;
            case PMU::PMU_SECTION_TYPE_DATA:
                CHECK_RC(LoadPmuDmem(PmuPort,
                                             LwrrBlk,
                                             UcodeSection,
                                             UcodeSectionInfo[idx]));
                break;
            default:
                Printf(Tee::PriError, "Invalid PMU section type %d\n", SectionType);
                return RC::BAD_PARAMETER;
                break;
        }

        // Move to the next PMU app code/data section, if present
        LwrrBlk += (UcodeSectionInfo[idx].SizeInBytes/256) + ((UcodeSectionInfo[idx].SizeInBytes%256)?1:0);
    }
    return rc;
}

//------------------------------------------------------------------------------
typedef PMU * (PMUFactory)(GpuSubdevice *);

extern PMUFactory GM10xPMUFactory;

static PMUFactory* FactoryArray[] =
{
    GM10xPMUFactory
};

/* static */
PMU * PMU::Factory(GpuSubdevice * pGpuSub)
{
    // Try to create a PMU object by trying each possible type
    // in order of newest to oldest until one matches.

    PMU * pObj = 0;

    for (UINT32 i = 0; i < sizeof(FactoryArray)/sizeof(FactoryArray[0]); i++)
    {
        pObj = (*FactoryArray[i])(pGpuSub);
        if (pObj)
            return pObj;
    }

    // For pre-Maxwell chips use the standard PMU base class
    return new PMU(pGpuSub);
}
