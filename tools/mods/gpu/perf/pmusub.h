/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_PMU_H
#define INCLUDED_PMU_H

#ifndef LWTYPES_INCLUDED
#include "lwtypes.h"
#endif

#ifndef _RMPMUCMDIF_H
#include "rmpmucmdif.h"
#endif

#ifndef _RMPMUPGEVT_H
#include "rmpmupgevt.h"
#endif

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_SURF2D_H
#include "gpu/utility/surf2d.h"
#endif

#ifndef INCLUDED_EVNTTHRD_H
#include "core/include/evntthrd.h"
#endif

#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef _ctrl2080mc_h_
#include "ctrl/ctrl2080/ctrl2080mc.h"
#endif

class GpuSubdevice;
class ModsEvent;
class Surface2D;

//--------------------------------------------------------------------
//! \brief Provides an interface for communicating with the PMU
//!
//! Each GpuSubdevice that supports the PMU should contain a valid
//! pointer to this class.
//!
class PMU
{
public:
    //! This enumeration describes the various types of filters that may be
    //! applied to commands / events received from the PMU
    typedef enum
    {
        COMMAND_SEQ,        //!< Filter on Command sequence, will only match
                            //!< the single command response associated with
                            //!< the sequence number provided as the filter
                            //!< data
        COMMAND_UNIT_ID,    //!< Filter on Command unit, will match all
                            //!< command responses with unit ids that match
                            //!< the unit id provided as the filter data
        COMMAND_ALL,        //!< Filter on Command, matches all command
                            //!< responses
        EVENT_UNIT_ID,      //!< Filter on event unit id, will match all events
                            //!< with unit ids that match the unit id provided
                            //!< as the filter data
        EVENT_ALL,          //!< Filter on event, matches all events
        ALL_UNIT_ID,        //!< Filter on unit id, will match all events
                            //!< and command responses with unit ids that match
                            //!< the unit id provided as the filter data
        ALL                 //!< Matches all events and command IDs
    } MessageFilterType;

    //! This enumeration describes the engines that may be powergated by
    //! the PMU
    typedef enum
    {
        ELPG_GRAPHICS_ENGINE = RM_PMU_LPWR_CTRL_ID_GR_PG,
        ELPG_VIDEO_ENGINE    = RM_PMU_LPWR_CTRL_ID_GR_PASSIVE, // TODO-djamadar : Remove ELPG_VIDEO_ENGINE
        ELPG_VIC_ENGINE      = RM_PMU_LPWR_CTRL_ID_GR_RG,      // TODO-aranjan  : Remove ELPG_VIC_ENGINE
        ELPG_DI_ENGINE       = RM_PMU_LPWR_CTRL_ID_DI,
        ELPG_MS_ENGINE       = RM_PMU_LPWR_CTRL_ID_MS_MSCG,
        ELPG_ALL_ENGINES,
        PWR_RAIL,
    } PgTypes;

    //! This enumeration describes the different types of Deep Idle statistics
    //! that may be gathered
    typedef enum
    {
        DEEP_IDLE_FB_OFF         = LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_FO,
        DEEP_IDLE_NO_HEADS       = LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_NH,
        DEEP_IDLE_VIDEO_ENABLED  = LW2080_CTRL_GPU_SET_DEEP_IDLE_STATISTICS_MODE_VE
    } DeepIdleStatType;

    //! This enumeration describes the different types of Deep Idle statistics
    //! that may be gathered
    typedef enum
    {
        PMU_MESSAGE  = RM_PMU_EVENT_CALLBACK_DATA_FORMAT_MESSAGE,
        PG_LOG_ENTRY = RM_PMU_EVENT_CALLBACK_DATA_FORMAT_PG_LOG_ENTRY
    } MessageType;
    typedef RM_PMU_EVENT_CALLBACK_INFO Message;

    enum class LpwrFeature : UINT32
    {
        MSCG,
        TPCPG
    };

    PMU(GpuSubdevice *pParent);
    virtual ~PMU();

    RC           Initialize();
    void         Pause(bool bPause);
    bool         IsPaused();
    void         Shutdown(bool bEos);
    LwRm::Handle GetHandle() { return m_Handle; }
    RC           Reset(bool bReboot);
    RC           GetUcodeState(UINT32 *ucodeState);
    ModsEvent *  AllocEvent(PMU::MessageFilterType filterType,
                            bool                   bIlwertFilter,
                            UINT32                 filterData);
    void         FreeEvent(ModsEvent * pEvent);
    RC           AddEvent(ModsEvent             *pEvent,
                          PMU::MessageFilterType filterType,
                          bool                   bIlwertFilter,
                          UINT32                 filterData);
    void         DeleteEvent(ModsEvent             *pEvent);
    RC           WaitMessage(PMU::MessageFilterType filterType,
                             UINT32            filterData,
                             FLOAT64           timeoutMs = Tasker::NO_TIMEOUT);
    RC           GetMessages(ModsEvent      *pEvent,
                             vector<PMU::Message> *pMessages);
    RC           BindSurface(Surface2D *pSurf,
                             UINT08     aperture,
                             PHYSADDR  *pVaddr);
    RC           SetPowerGatingParameters(vector<LW2080_CTRL_POWERGATING_PARAMETER> *pParameters,
                                          UINT32 Flags);
    RC           GetPowerGatingParameters(vector<LW2080_CTRL_POWERGATING_PARAMETER> *pParameters);

    RC           GetPowerGateSupportEngineMask(UINT32 *pEngineMask);
    RC           GetPowerGateEnableEngineMask(UINT32 *pEngineMask);
    RC           SetPowerGateEnableEngineMask(UINT32 EnableMask);
    void         SetElpgOwned(bool IsOwned){ m_ElpgIsOwned = IsOwned;}
    bool         IsElpgOwned() const {return m_ElpgIsOwned;};

    void         PrintPowerGatingParams(Tee::Priority PrintPri,
                                        PgTypes   engine,
                                        bool          bShowThresholds);

    RC           PowerGateOn(PgTypes Engine, FLOAT64 TimeoutMs);

    bool         IsPowerGated(PgTypes Engine);
    bool         IsPowerGatingSupported(PgTypes Engine);
    bool         IsPmuElpgEnabled(){return m_PmuElpgEnabled;};

    bool         IsDeepIdleSupported();
    RC           GetDeepIdleStatistics(DeepIdleStatType Type,
                     LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStats);
    RC           ResetDeepIdleStatistics(DeepIdleStatType Type);

    bool         IsMscgSupported();
    RC           PStateSupportsLpwrFeature(UINT32 PStateNum,
                                           LpwrFeature feature,
                                           bool* pSupported);
    RC           GetMscgEnabled(bool* pEnabled);
    RC           SetMscgEnabled(bool enable);
    RC           GetMscgEntryCount(UINT32* pCount);

    struct PgCount
    {
        UINT32 pwrOff;
        UINT32 pwrOn;
    };
    RC          GetLwrrentPgCount(UINT32 engineId, PgCount *pNewCnt);
    RC          GetLwrrentPgTimeUs(UINT32 engineId, UINT32* pInGateTime, UINT32* pUnGateTime);


    // Structure to define a PMU code/data binary blob
    typedef struct PmuUcodeInfo
    {
        UINT32 OffsetInBytes; // offset from start of code/data section
        UINT32 SizeInBytes;   // size in bytes
        bool   bIsSelwre;      // should this section be marked secure ?
    } PmuUcodeInfo;

    enum PmuUcodeSectionType
    {
        PMU_SECTION_TYPE_CODE,
        PMU_SECTION_TYPE_DATA
    };

    // Standard interface to load sections of code/data into the PMU without RM
    RC LoadPmuUcodeSection
    (
        UINT32 PmuPort,
        PmuUcodeSectionType SectionType,
        const vector<UINT32> &UcodeSection,
        const vector<PMU::PmuUcodeInfo> &UcodeSectionInfo
    );

    // APIs to reset execute ucode in PMU *without* RM loaded
    // (can be reimplemented based on GPU
    virtual RC ResetPmuWithoutRm() {return RC::UNSUPPORTED_FUNCTION;}
    virtual RC ExelwtePmuWithoutRm() {return RC::UNSUPPORTED_FUNCTION;}

    // Factory method to create Gpu specific PMU object
    // Should be called by the owning GpuSubdevice object
    static PMU * Factory(GpuSubdevice * pGpuSub);

protected:
    GpuSubdevice *m_Parent;             //!< GpuSubdevice that owns this PMU

    // APIs to load data and code in PMU *without* RM loaded
    // (can be reimplemented based on GPU
    virtual RC LoadPmuDmem
    (
        UINT32 PmuPort, // DMEM access port
        UINT32 BlockNum, // Block index to be written to
        const  vector<UINT32> &PmuData,
        const  PMU::PmuUcodeInfo &DataSection
    )
    {return RC::UNSUPPORTED_FUNCTION;}

    virtual RC LoadPmuImem
    (
        UINT32 PmuPort, // IMEM access port
        UINT32 BlockNum, // Block index to be written to
        const  vector<UINT32> &PmuCode,
        const  PMU::PmuUcodeInfo &CodeSection
    )
    {return RC::UNSUPPORTED_FUNCTION;}

private:

    struct MessageTask
    {
        EventThread    *pEventThread;  //!< Manages the boilerplate "wait for
                                       //!< OS event & call handler" subtask
                                       //!< loop.
        PMU            *pPmu;          //!< Pointer to the PMU
        LwRm::Handle    hRmEvent;      //!< Returned by LwRm::AllocEvent;
                                       //!< needed for cleanup.
    };

    //! Structure for managing an event in the PMU software event list
    //! Lwrrently no events have payloads associated.  If events can have
    //! payloads then this structure will need to have storage for that payload
    struct EventListEntry
    {
        PMU::Message      message;    //!< Storage for the event data
        list<ModsEvent *> modsEvents; //!< List of MODS events that were
                                      //!< notified when the PMU event was
                                      //!< received, used to determine when
                                      //!< all registered users have consumed
                                      //!< the event
    };

    //! Structure for managing a command and its response in the PMU software
    //! Command list
    struct CommandListEntry
    {
        ~CommandListEntry();
        Surface2D       Command;        //!< Command surface
        Surface2D       Message;        //!< Message surface

        UINT32          cmdSequence;    //!< Sequence number for the command
        bool            bMsgReceived;   //!< true if the message has been
                                        //!< received
        list<ModsEvent *>   modsEvents; //!< List of MODS events to notify when
                                        //!< the message is received.  Also
                                        //!< used to determine when all
                                        //!< registered users have consumed
                                        //!< the event
    };

    //! Structure for managing registered events and the filters associated
    //! with each event
    struct ModsEventData
    {
        bool                   bNotifyOnly;  //!< true if the mods event only
                                             //!< wants notification and will
                                             //!< never call GetMessages to
                                             //!< extract the message data
        PMU::MessageFilterType filterType;   //!< Type of filter for the event
        bool                   bIlwertFilter;//!< true to ilwert the meaning of
                                             //!< the filter type
        UINT32                 filterData;   //!< Data for the event filter
        bool                   bEventAlloc;  //!< true if the associated
                                             //!< ModsEvent was allocated via a
                                             //!< call to AllocEvent
    };

    typedef enum
    {
        COMMAND_HANDLER_PAUSED = 0x01,
        EVENT_HANDLER_PAUSED = 0x02,
        ALL_PAUSED = 0x3
    } PauseType;

    UINT32        m_IsPaused;           //!< current pause status
    UINT32        m_PauseCount;         //!< number of requests to pause the PMU

    bool          m_Initialized;        //!< true if the PMU has been
                                        //!< initialized
    LwRm::Handle  m_Handle;             //!< Handle to the PMU object
    Surface2D     m_EventBuffers;       //!< Surface containing allocation for
                                        //!< event queues for all types of
                                        //!< events
    bool         m_PmuElpgEnabled;      //!< ELPG is offloaded to PMU or still on CPU
    UINT32       m_SupportedElpg;       //!< Masks of ELPG that are available
    UINT32       m_SupportedElpgQueried;//!< only need to fetch m_SupportedElpg from RM once
    bool         m_ElpgIsOwned;         //!< ELPG is "in used" - tell tests to not mess with settings

    //! Structure for managing the RM-MODS shared event queue (there is one
    //! queue per PMU unit ID
    struct EventQueueData
    {
        UINT32   hEvent;  //!< Handle for the event queue
        UINT08 * pQueue;  //!< CPU pointer to queue data - THIS IS A
                          //!< POINTER ONTO A SURFACE!!
        UINT32 * pHead;   //!< Head offset (RM writes data here and updates
                          //!< this offset) - MODS MUST TREAT THIS MEMBER AS
                          //!< READ ONLY!!!
        UINT32 * pTail;   //!< Tail offset (MODS reads data from here and
                          //!< updates this offset) - RM MUST TREAT THIS MEMBER
                          //!< AS READ ONLY!!!
    };
    vector<EventQueueData> m_EventQueues;
    UINT32                 m_EventQueueBytes;

    MessageTask  m_EventTask;   //!< Task for handling events from the PMU
    MessageTask  m_CommandTask; //!< Task for handling command responses from
                                //!< the PMU
    set<LwRm::Handle> m_BoundSurfaceHandles; //!< Set of surface handles that
                                             //!< have been bound to the PMU

    //! Mapping of all registered ModsEvents to their respective data.  Each
    //! ModsEvent will be set when a PMU event or command response is received
    //! that matches the filter data within the ModsEventData.
    map<ModsEvent *, ModsEventData> m_ModsEventData;

    //! Event and Command lists that store the PMU events and the command
    //! responses for all PMU events and commands
    list<EventListEntry *>          m_EventList;

    //! Map of power gating parameters to a string for printing
    map<UINT32, string>             m_PGParamString;

    void * m_PMUMutex;              //!<  Mutex for accessing the various lists

    Memory::Location m_SurfaceLoc;  //!< Location for RM/MODS shared surfaces

    static UINT32 s_RegisterUnitIds[];  //!< Unit IDs to register for
    static UINT32 s_RegisteredUnitCount;//!< Count of registered unit IDs

    static void EventHandler(void *pData);
    static void CommandHandler(void *pData);

    void ProcessEvents();

    bool FilterMatches(UINT08 unitId,
                       UINT32 cmdSeq,
                       bool bEvent,
                       ModsEventData *pModsEventData);
    void   InternalAddEvent(ModsEvent             *pEvent,
                            PMU::MessageFilterType filterType,
                            bool                   bIlwertFilter,
                            UINT32                 filterData,
                            bool                   bEventAlloc);
    void   InternalDeleteEvent(ModsEvent *pEvent);

    UINT32 EventQueueGetBytes(UINT08 *pDst, UINT08 *pQueue, UINT32 Head,
                              UINT32  Tail, UINT32 Size);

    RC     CreateSurface(Surface2D *pSurface, UINT32 Size);
    UINT32 GetWidth(UINT32 *pPitch);
    void   CopyFromSurface(void *pDst, void *pSrc, UINT32 Size);
    void   CopyToSurface(void *pDst, void *pSrc, UINT32 Size,
                         bool bFlushWCBuffer);

    RC     WaitPMUReady();

    typedef enum
    {
        COMMAND_HDR,
        MESSAGE_HDR,
        EVENT_HDR
    } PmuHeaderType;
    void DumpHeader(void *pHdr, PmuHeaderType hdrType);
    bool IsPerEngineParameter(UINT32 parameter);
    bool IsThresholdParameter(UINT32 parameter);

    //! This structure describes the necessary data for waiting for
    //! Deep Idle statistics
    struct PollDeepIdleStruct
    {
        PMU *pPmu;
        LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pParams;
        RC   pollRc;
    };
    static bool PollGetDeepIdleStats(void *pvArgs);
    RC     Detach(void);

    template<typename RMCTRL_PARAM>
    static RC CheckForLwrmLpwrError(const RMCTRL_PARAM& param, UINT32 listSize, const char* func);
};

class ElpgOwner
{
    public:
        ElpgOwner();
        ~ElpgOwner();
        // claim the lock or take relwrsive ownership
        RC ClaimElpg(PMU *);
        // release the lock or release relwrsive ownership
        RC ReleaseElpg();
    private:
        UINT32 m_ElpgOwnerRefCnt;
        PMU *m_pPmu;
};
#endif
