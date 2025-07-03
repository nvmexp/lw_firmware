/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  utility/pexdev.h
 * @brief Manage BR04/Chipset devices.
 *
 *
 */

#ifndef INCLUDED_PEXDEV_H
#define INCLUDED_PEXDEV_H

#include "core/include/devmgr.h"
#include <list>
#include <map>
#include <vector>
#include <memory>
#include <deque>
#include "ctrl/ctrl2080/ctrl2080bus.h"
#include "core/include/pci.h"
#include "jsapi.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"

class GpuDevice;
class GpuSubdevice;
class PlxErrorCounters;
class PexDevMgr;
class TestDevice;
typedef shared_ptr<TestDevice> TestDevicePtr;

//--------------------------------------------------------------------
//! \brief Provides an interface aclwmulating and verifying PEX errors
//!
//! This class can be used for either aclwmulating actual counts or
//! storing count thresholds.  Due to operator overloading, aclwmulating
//! and checking thresholds is easy
//!
class PexErrorCounts
{
public:

    //! Enum describing the error counter indexes
    enum PexErrCountIdx
    {
        CORR_ID
        ,NON_FATAL_ID
        ,FATAL_ID
        ,UNSUP_REQ_ID

        // Internal use only IDs below this point
        ,DETAILED_LINEERROR_ID
        ,DETAILED_CRC_ID
        ,DETAILED_NAKS_R_ID
        ,DETAILED_FAILEDL0SEXITS_ID
        ,DETAILED_NAKS_S_ID

        // Add new counters that have thresholds above this line.  Counters that
        // are informational in nature below this line
        ,REPLAY_ID
        ,RECEIVER_ID
        ,LANE_ID
        ,BAD_DLLP_ID
        ,REPLAY_ROLLOVER_ID
        ,E_8B10B_ID
        ,SYNC_HEADER_ID
        ,BAD_TLP_ID

        // Add new non-threshold counter defines above this line
        ,NUM_ERR_COUNTERS
        ,FIRST_INTERNAL_ONLY_ID = DETAILED_LINEERROR_ID
        ,FIRST_NON_THRESHOLD_COUNTER = REPLAY_ID
    };

    PexErrorCounts() { Reset(); }

    PexErrorCounts & operator+=(const PexErrorCounts &rhs);
    bool operator > (const PexErrorCounts &rhs) const;
    UINT32 GetCount(const UINT32 CountIdx) const;
    static string GetString(const UINT32 CountIdx);
    static bool IsInternalOnly(const UINT32 CountIdx);
    RC SetCount
    (
        const UINT32 CountIdx,
        const bool   bHwCount,
        const UINT32 Count
    );
    RC SetPerLaneCounts(UINT08 *perLaneCounts);
    UINT08 GetSingleLaneErrorCount(UINT32 laneIdx);
    bool IsValid(const UINT32 CountIdx) const;
    bool IsThreshold(const UINT32 CountIdx) const;
    UINT32 GetValidMask() const { return m_IsCounterValidMask; }
    UINT32 GetHwCountMask() const { return m_IsHwCounterMask; }
    UINT32 GetThresholdMask() const { return m_IsThresholdMask; }
    void Reset();
    static RC TranslatePexCounterTypeToPexErrCountIdx(UINT32 lw2080PexCounterType,
                                                      PexErrCountIdx *outIdx);

private:
    static map<UINT32, PexErrCountIdx> CreateLw2080PexCounterTypeToPexErrCountIdxMap();

    //! If the corresponding bit is set for a counter then the counter
    //! is valid
    UINT32   m_IsCounterValidMask;

    //! If the corresponding bit is set for a counter then the counter
    //! is derived from a HW counter, if not set then it is derived from
    //! a hardware flag
    UINT32   m_IsHwCounterMask;

    //! If the corresponding bit is set for a counter then the counter
    //! is used as a threshold in PEX testing.  If not set then the count is
    //! used solely for informational purposes only
    UINT32   m_IsThresholdMask;

    //! Actual error counts
    UINT32 m_ErrCounts[NUM_ERR_COUNTERS];

    UINT08 m_PerLaneErrCounts[LW2080_CTRL_PEX_MAX_LANES];

    static map<UINT32, PexErrCountIdx> lw2080PexCounterTypeToPexErrCountIdxMap;
};

class PexDevice : public Device
{
public:

    enum PexDevType
    {
        CHIPSET,
        BR03,
        BR04,
        PLX,
        CHIPSET_INTELZ75,
        QEMU_BRIDGE,
        UNKNOWN_PEXDEV,
    };

    enum OnAerLogFull
    {
        AER_LOG_IGNORE_NEW
       ,AER_LOG_DELETE_OLD
    };

    struct AERLogEntry
    {
        UINT32         errMask;
        vector<UINT32> aerData;
    };

    typedef INT32 DevErrors[PexErrorCounts::UNSUP_REQ_ID+1];

    // on a Bridge, there is 1 Upstream port and 4 downstream ports
    // Each port has its own Bus and Device number in PCI config space
    struct PciDevice
    {
        UINT16 Domain;              // pci domain
        UINT08 Bus;                 // primary bus number
        UINT08 Dev;
        UINT08 Func;                // maybe not necessary...
        UINT08 SecBus;              // Secondary bus number
        UINT08 SubBus;              // Suboridante bus number
        UINT32 VendorDeviceId;      // Vendor/DeviceId
        bool Located;               // has been parsed
        bool Root;                  // it's a downstream port of a rootport device
        Device* pDevice;            // connection to the device (up or downstream)
        bool IsPexDevice;           // the device Up/Down stream is a bridge
        PexDevType Type;            // type of Bridge: ALL siblings must have same the same TYPE!!!
        UINT08 PcieCapPtr;          // this is for PciDevice ctrl on the chipset
        UINT32 PcieL1ExtCapPtr;     // this is for PciDevice L1 substate control on the chipset
        UINT32 PcieDpcExtCapPtr;    // this is for PciDevice DPC control on the chipset
        UINT16 PcieAerExtCapSize;   // This is the size of the AER block (0 if not present)
        UINT16 PcieAerExtCapPtr;    // This is the offset of the AER block, only valid
                                    // if PcieAerSize !=0
        void *pBar0;                // BAR0 address for the port
        UINT08 PortNum;             // Port Number (from link cap register)
        bool UniqueLinkPolling;     // if true use unique link polling algorithm.
        void* Mutex;                // guard for the error counts
        DevErrors ErrCount;         // aclwmulated error counts of each kind
        deque<AERLogEntry> AerLog;  // Log of all AER errors captured when a device error
                                    // is detected
        PlxErrorCounters *pErrorCounters = nullptr;
    };

    //! Enum describing the types of pex error counters to operate on
    //!
    //! There are 2 types of PEX error counters, error counters that have an
    //! actual underlying hardware counter (and therefore dont need to be polled
    //! or only need polling infrequently) and counters that are derived from
    //! reading a single hardware flag (and therefore would need to be polled
    //! very frequently to get an accurate count).
    //!
    enum PexCounterType
    {
        PEX_COUNTER_HW,
        PEX_COUNTER_FLAG,
        PEX_COUNTER_ALL
    };

    PexDevice(bool IsRoot,
              PciDevice* UpStreamPort,
              vector<PciDevice*> DownStreamPorts);
    ~PexDevice(){};
    RC Initialize();
    RC Shutdown();

    PexDevice* GetUpStreamDev();

    // This gets the downstream port device ptr by the Index (0-3)
    // pIsPexDevice will indicate if the device is a bridge or
    // a GPU
    bool IsDownStreamPexDevice(UINT32 Index);
    // this would return NULL if DownStream is empty or a Gpu
    PexDevice* GetDownStreamPexDevice(UINT32 Index);
    // this would return NULL if downstream is empty or a Bridge
    GpuSubdevice* GetDownStreamGpu(UINT32 Index);
    void ResetDownstreamPort(UINT32 port);

    PciDevice* GetUpStreamPort() const { return m_UpStreamPort; }
    UINT32 GetUpStreamIndex() const { return m_UpStreamIndex; }
    PciDevice* GetDownStreamPort(UINT32 Index);
    bool IsRoot() const { return m_IsRoot; }
    PexDevType GetType() const { return m_Type; }
    UINT32 GetNumDPorts() const { return static_cast<UINT32>(m_DownStreamPort.size()); }
    UINT32 GetNumActiveDPorts();
    UINT32 GetDepth() const { return m_Depth; }
    RC SetDepth(UINT32 Depth);

    //! Get the Upstream device's downstream port to which the current bridge is connected
    UINT32 GetParentDPortIdx() const { return m_UpStreamIndex; }
    RC SetUpStreamDevice(PexDevice* pUpStream, UINT32 Index);

    // These are the functions that might be needed for Canoas,
    // Gen2Link, GpuTest LinkStatus check

    //! Link speeds in Mbps
    RC ChangeUpStreamSpeed(Pci::PcieLinkSpeed NewSpeed);
    Pci::PcieLinkSpeed GetUpStreamSpeed();
    Pci::PcieLinkSpeed GetDownStreamSpeed(UINT32 port);
    RC SetDownStreamSpeed(UINT32 port, Pci::PcieLinkSpeed speed);

    // The Error functions: These functions look at the errors at the connections
    // of each link (upstream of local, and downstream registers of upstream
    // device).

    //! This looks at the local upstream errors and the PCIE registers of the device
    //! above's errors (the device above is either a bridge or chipset) at the
    //! connecting downstream port
    RC GetUpStreamErrors(Pci::PcieErrorsMask *pLocErrors, Pci::PcieErrorsMask* pHostErrors);
    RC GetUpStreamErrorCounts
    (
        PexErrorCounts *pLocErrors,
        PexErrorCounts* pHostErrors,
        PexCounterType  CountType
    );

    //! This is called from Gpu if Gpu is not connected to Chipset
    RC GetDownStreamErrors(Pci::PcieErrorsMask* pDownErrors, UINT32 Port);

    //! This is called from Gpu if Gpu is not connected to Chipset
    RC GetDownStreamErrorCounts
    (
        PexErrorCounts* pDownErrors,
        UINT32          Port,
        PexCounterType  CountType
    );

    RC GetDownStreamAerLog(UINT32 Port, vector<AERLogEntry> *pLogData);
    RC GetUpStreamAerLog(vector<AERLogEntry> *pLogData);
    RC ClearDownStreamAerLog(UINT32 Port);
    RC ClearUpStreamAerLog();

    //! This resets all errors on the specified downstream port
    //! as well as the upstream port
    RC ResetErrors(UINT32 DownPort);

    //! This looks at the local and host's upstream link widths by PCIE read/write
    RC GetUpStreamLinkWidths(UINT32 *pLocWidth, UINT32 *pHostWidth);

    //! This is called from Gpu if Gpu is not connected to Chipset
    RC GetDownStreamLinkWidth(UINT32* pWidth, UINT32 Port);

    //!see what kind of ASPM state is supported at the downstream port
    UINT32 GetAspmCap();
    bool   HasAspmL1Substates() { return m_HasAspmL1Substates; }
    UINT32 GetAspmL1SSCap();

    //! Get downstream port containment capability
    UINT32 GetDpcCap() { return m_DpcCap; }

    //! Get speeds in Mbps
    Pci::PcieLinkSpeed GetUpStreamLinkSpeedCap();
    Pci::PcieLinkSpeed GetDownStreamLinkSpeedCap(UINT32 Port);
    
    UINT32 GetUpStreamLinkWidthCap();
    UINT32 GetDownStreamLinkWidthCap(UINT32 Port);

    //! Gets the current state of ASPM (disabled, L0s, L1, L0s/L1)
    RC GetDownStreamAspmState(UINT32 *pState, UINT32 Port);
    RC GetUpStreamAspmState(UINT32 *pState);

    //! Set the current ASPM state
    RC SetDownStreamAspmState(UINT32 State, UINT32 Port, bool bForce);
    RC SetUpStreamAspmState(UINT32 State, bool bForce);

    //! Gets the current L1 substate of ASPM (disabled, L11, L12, L11/L12)
    RC GetDownStreamAspmL1SubState(UINT32 *pState, UINT32 Port);
    RC GetUpStreamAspmL1SubState(UINT32 *pState);

    // NOTE : DO NOT ADD "SET" FUNCTIONS FOR ASPM L1 SUBSTATES
    // They cannot be arbitrarily changed like ASPM states.
    // See PexDevMgr::SetAspmL1SS for details

    //! Get the current DPC state
    RC GetDownStreamDpcState(Pci::DpcState *pState, UINT32 Port);
    RC SetDownStreamDpcState(Pci::DpcState state, UINT32 Port);

    // hopefully we can remove this in future
    static UINT32 GetPexDevHandle();

    void PrintInfo(Tee::Priority pri);
    RC   PrintDownStreamExtCapIdBlock(Tee::Priority Pri, Pci::ExtendedCapID CapId, UINT32 Port);
    RC   PrintUpStreamExtCapIdBlock(Tee::Priority Pri, Pci::ExtendedCapID CapId);

    RC GetRom(void *pInBuffer, UINT32 BufferSize);

    void SetUsePollPcieStatus(bool UsePoll) { m_UsePollPcieStatus = UsePoll; }
    bool UsePollPcieStatus() { return m_UsePollPcieStatus; }
    RC GetDownstreamPortLTR(UINT32 port, bool *pEnable);
    RC SetDownstreamPortLTR(UINT32 port, bool  enable);
    RC DisableDownstreamPort(UINT32 port);
    RC EnableDownstreamPort(UINT32 port);
    RC IsDLLAReportingSupported(UINT32 port, bool *pSupported);
    RC GetP2PForwardingEnabled(UINT32 port, bool *pbEnabled);

    enum LinkPowerState
    {
        L0, // Full power state(same as Link_Enable)
        L2, // Low power link state for D3 state
        LINK_ENABLE,
        LINK_DISABLE,
    };
    const char *GetLinkStateString(LinkPowerState state);
    RC SetLinkPowerState(UINT32 port, LinkPowerState state);

    enum class PlatformType
    {
        CFL,
        TGL
    };
    void InitializeRootPortPowerOffsets(PlatformType type);

    RC PollDownstreamPortLinkStatus
    (
        UINT32 port,        // downstream port to check
        UINT32 timeoutMs,   // how long to poll
        LinkPowerState linkState
    );

    RC EnableUpstreamBusMastering();
    RC EnableDownstreamBusMastering(UINT32 port);

    RC GetDownstreamPortHotplugEnabled(UINT32 port, bool *pEnable);
    RC SetDownstreamPortHotplugEnabled(UINT32 port, bool enable);

    RC GetDownstreamAtomicRoutingCapable(UINT32 port, bool *pCapable);
    RC GetUpstreamAtomicRoutingCapable(bool *pCapable);
    RC GetUpstreamAtomicEgressBlocked(bool *pEnable);
    RC GetDownstreamAtomicsEnabledMask(UINT32 port, UINT32 *pAtomicMask);

    static const UINT32 DEPTH_UNINITIALIZED = 0xFFFFFFFF;
    static const UINT32 BR04_ROMSIZE = 0x1000;
    static const UINT32 PLX_RCVR_ERR_COUNT_OFFSET = 0xBF0;
    static const UINT32 PLX_BAD_TLP_COUNT_OFFSET  = 0xFAC;
    static const UINT32 PLX_BAD_DLLP_COUNT_OFFSET = 0xFB0;

    static RC ReadPcieErrors(PexDevice::PciDevice* pPciDev);

    static RC ClearAerErrorStatus(PciDevice* pPciDev, bool printErrors);
    static RC GetOnAerLogFull(UINT32 *pOnFull);
    static RC SetOnAerLogFull(UINT32 onFull);

    static RC GetAerLogMaxEntries(UINT32 *pMaxEntries);
    static RC SetAerLogMaxEntries(UINT32 maxEntries);

    static RC GetAerUEStatusMaxClearCount(UINT32 *pMaxCount);
    static RC SetAerUEStatusMaxClearCount(UINT32 maxCount);

    static RC GetRootPortDevice(PexDevice **ppPexDev, UINT32 *pPort);

    static RC GetSkipHwCounterInit(bool *pSkipInit);
    static RC SetSkipHwCounterInit(bool skipInit);
private:
    struct PollArgs
    {
        PciDevice*  pPciDev;
        LinkPowerState linkState;
    };
    static bool PollLinkStatus(void * pArgs); // uses PollArgs*
    static bool PollL0orL2Status(void * pArgs);
    static RC UpdatePLPMOffsetForL2(PciDevice *pPciDev, LinkPowerState linkState);

    static UINT32 GetPcieStatusOffset(PciDevice* pPciDev);
    static RC GetPcieErrors(PciDevice* pPciDev,
                            DevErrors* pErrors);
    static RC GetPcieErrorCounts
    (
        PexErrorCounts *pErrors,
        PciDevice      *pPciDev,
        bool            UsePoll,
        PexCounterType  CountType
    );
    static RC ResetPcieErrors(PciDevice      *pPciDev,
                              PexCounterType  CountType);

    static RC GetLinkWidth(UINT32 *pWidth, PciDevice* pPciDev);
    static RC Read(PciDevice* pPciDev, UINT32 Offset, UINT32 *pValue);
    static RC Write(PciDevice* pPciDev, UINT32 Offset, UINT32 Value);
    static void PrintExtCapIdBlock
    (
        PciDevice* pPciDev,
        Tee::Priority Pri,
        Pci::ExtendedCapID CapId,
        const vector<UINT32> & capData
    );
    static RC GetExtCapIdBlock
    (
        PciDevice* pPciDev,
        Pci::ExtendedCapID CapId,
        vector<UINT32> *pExtCapData
    );

    RC SetupCapabilities();

    RC GetAspmStateInt(PciDevice *pPciDev, UINT32 Offset, UINT32 *pState);
    RC GetAspmL1SubStateInt(PciDevice *pPciDev, UINT32 *pState);
    RC EnableBusMasterInt(PciDevice *pPciDev);

    RC ValidateAspmState(bool bForce, UINT32 port, UINT32 *pState);
    RC ReadSpeedAndWidthCap
    (
        PciDevice *pPciDev,
        Pci::PcieLinkSpeed *pLinkSpeed,
        UINT32 *pLinkWidth
    );

    RC SetL2MagicBit(UINT32 port, LinkPowerState linkState);

    PciDevice *                 m_UpStreamPort;
    vector <PciDevice*>         m_DownStreamPort;
    // m_LinkSpeedCap and m_LinkWidthCap are the capabilities of the
    // downstream ports
    vector <Pci::PcieLinkSpeed> m_LinkSpeedCap;
    vector <UINT32>             m_LinkWidthCap;

    bool m_IsRoot;
    //! this is the downstream port index on the Upstream device
    UINT32 m_UpStreamIndex;
    UINT32 m_Depth;         //! depth from root (chipset = 0)
    PexDevType m_Type;
    bool m_AspmL0sAllowed;
    bool m_AspmL1Allowed;
    bool m_HasAspmL1Substates;
    bool m_AspmL11Allowed;
    bool m_AspmL12Allowed;
    UINT32 m_DpcCap;
    bool m_Initialized;
    bool m_UsePollPcieStatus;
    PlatformType m_PlatformType = PlatformType::CFL;

    unique_ptr<PlxErrorCounters> m_pErrorCounters;

    static OnAerLogFull s_OnAerLogFull;
    static size_t       s_AerLogMaxEntries;
    static size_t       s_AerUEStatusMaxClearCount;

    static bool s_SkipHwCounterInit;

    // Magic offsets of root port device to perform L0<-->L2 transition
    // based on CFL/TGL platform
    static UINT32 RP_LINK_POWER;
    static UINT32 RP_LINK_POWER_L2_BIT;
    static UINT32 RP_LINK_POWER_L0_BIT;
    static UINT32 RP_LINK_POWER_MAGIC_BIT;
    static const UINT32 RP_P0RM = 0xC38; //offset
    static const UINT32 RP_P0RM_SET_BIT = 0x8; // Bit 3
    static const UINT32 RP_P0AP = 0xC20; //offset
    static const UINT32 RP_P0AP_SET_BIT = 0x30; // Bit [5:4]
};

class PlxErrorCounters
{
public:
    PlxErrorCounters() = default;
    ~PlxErrorCounters() { Shutdown(); };

    RC Initialize(PexDevice* pPexDevice);
    void Shutdown();

    RC GetPcieErrorCounts(PexErrorCounts *pErrors, PexDevice::PciDevice *pPciDev);
    RC ResetPcieErrors(PexDevice::PciDevice *pPciDev);

private:
    void GetStationPort(UINT08 port, UINT08* stationPort, UINT08* stationPortZero);
    RC ReadPort32(UINT08 port, UINT32 offset, UINT32 *pValue);
    RC WritePort32(UINT08 port, UINT32 offset, UINT32 value);
    RC ReadPort08(UINT08 port, UINT32 offset, UINT08 *pValue);
    RC WritePort08(UINT08 port, UINT32 offset, UINT08 value);

    bool   m_initialized        = false;
    void*  m_pBar0              = nullptr;
    UINT32 m_RcvrErrOffset      = 0;
    UINT64 m_PortRegisterOffset = 0;
    UINT64 m_PortRegisterStride = 0;
    UINT32 m_PortsPerStation    = 0;
    UINT32 m_StationCount       = 0;
    bool   m_HasPortSelect      = false;
    bool   m_HasInternalUpstreamPorts = false;
};

class PexDevMgr : public DevMgr
{
public:
    PexDevMgr();
    ~PexDevMgr();

    RC InitializeAll() override;
    RC ShutdownAll() override;
    void OnExit() override { }

    UINT32 NumDevices() override;

    UINT32 ChipsetASPMState();
    bool   ChipsetHasASPML1Substates();
    UINT32 ChipsetASPML1SubState();

    // FindDevices would:
    // 1) Identify all the PciDevices in the system
    // 2) Group PciDevices into Bridge device: This relies on the fact that
    //    GpuDevices have been identified ...
    // 3) Construct the way Gpus and Bridges are connected
    RC FindDevices() override;
    void FreeDevices() override { }

    RC GetDevice(UINT32 Index, Device **device) override;

    // This would be required if we want to traverse from top to bottom...
    // there is no use case for these yet
    UINT32 GetNumRoots() const { return static_cast<UINT32>(m_pRoots.size()); }
    PexDevice* GetRoot(UINT32 Index);
    
    // Add pex device to the topology that isn't necessarily a test device
    RC RegisterLeafDevice(UINT32 domain, UINT32 secBus, UINT32 subBus);

    // Get the PexDevice above a Known GpuSubdevice
    // hlwu - todo - evalute these... i'm leaning towards removing them
    static bool IsPexDeviceAbove(GpuSubdevice* pGpuSubdev, PexDevice* pBridgeAbove);
    static bool IsPexDeviceAbove(PexDevice* pBridgeBelow, PexDevice* pBridgeAbove);

    void PrintPexDeviceInfo();
    static RC DumpPexInfo(TestDevice* pTestDevice, UINT32 pexVerbose,
                          bool printOnceRegs);

    UINT32 GetVerboseFlags() const;
    RC   SetVerboseFlags(UINT32 verboseFlags);

    UINT32 GetErrorPollingPeriodMs() const { return m_ErrorPollingPeriodMs; }
    RC SetErrorPollingPeriodMs(UINT32 value) { m_ErrorPollingPeriodMs = value; return OK; }

    enum RestoreParams
    {
        REST_SPEED        = (1 << 0),
        REST_LINK_WIDTH   = (1 << 1),
        REST_ASPM         = (1 << 2),
        REST_ASPM_L1SS    = (1 << 3),
        REST_DPC          = (1 << 4),
        REST_ALL          = REST_SPEED|REST_LINK_WIDTH|REST_ASPM|REST_ASPM_L1SS|REST_DPC,
    };
    //! This takes a snap shot of the PEX settings
    RC SavePexSetting(UINT32 *pRet, UINT32 WhatToSave);

    bool DoesSessionIdExist(UINT32 SessionId);

    //! restores a set of PEX parameters:
    // SessionId : session ID returned in SavePexSetting
    // RemoveSession : if true, forget the session
    // GpuDevice : if only want to restore the PEX settings on all paths for a device
    // GpuSubdevice : if only want to restore the PEX settings on one path
    RC RestorePexSetting(UINT32 SessionId,
                         UINT32 WhatToRestore,
                         bool RemoveSession,
                         GpuDevice *pGpuDevice = 0,
                         GpuSubdevice *pSubdev = 0);
    RC RestorePexSetting(UINT32 sessionId,
                         UINT32 whatToRestore,
                         bool removeSession,
                         TestDevicePtr pTestDevice);

    RC SetAspm(TestDevicePtr pTestDevice, // depth is relative to the device
               UINT32 Depth,   // depth = 0 is link directly above the device
               UINT32 LocAspm,
               UINT32 HostAspm,
               bool   ForceValue); // if false - value = suggested & Cap; if true just force it
    RC SetAspmL1SS(UINT32 testDevInst, // depth is relative to the test device
                   UINT32 Depth,       // depth = 0 is link directly above the test device
                   UINT32 AspmL1SS);

    enum PexVerboseBit
    {
        PEX_VERBOSE_ENABLE          = 0,
        PEX_DUMP_ON_TEST            = 1,
        PEX_DUMP_ON_TEST_ERROR      = 2,
        PEX_DUMP_ON_ERROR_CB        = 3,
        PEX_PRE_CHECK_CB_ENABLE     = 4,
        PEX_VERBOSE_COUNTS_ONLY     = 5,
        PEX_OFFICIAL_MODS_VERBOSITY = 6,
        PEX_LOG_AER_DATA            = 7,
        PEX_VERBOSE_JSON_ONLY       = 8,
    };

    class PauseErrorCollectorThread
    {
        public:
            explicit PauseErrorCollectorThread(PexDevMgr* pPexDevMgr);
            ~PauseErrorCollectorThread();

            PauseErrorCollectorThread(const PauseErrorCollectorThread&)            = delete;
            PauseErrorCollectorThread& operator=(const PauseErrorCollectorThread&) = delete;

        private:
            PexDevMgr* m_pPexDevMgr;
    };

private:
    struct PexParam
    {
        Pci::PcieLinkSpeed Speed;
        UINT32 LocAspm;
        UINT32 HostAspm;
        UINT32 AzaAspm;
        UINT32 XusbAspm;
        UINT32 PpcAspm;
        UINT32 Width;
        UINT32 LocAspmL1SS;
        UINT32 HostAspmL1SS;
        Pci::DpcState DpcState;
    };
    struct PexSettings
    {
        UINT32           Inst;    // the GpuInst/DevInst that owns this list of settings
        vector<PexParam> List;    // list of pex settings - for each device up the branch
                                  // So the lepth of this list = how far device is away from root
    };
    struct SnapShot
    {
        UINT32                   SessionId;           // current session ID
        map<UINT32, PexSettings> GpuSettingList;      // key is GpuInst
        map<UINT32, PexSettings> LwSwitchSettingList; // key is DevInst
    };
    struct LeafDevice
    {
        UINT32 domain;
        UINT32 secBus;
        UINT32 subBus;
    };

    map<UINT32, SnapShot> m_PexSnapshots;  // master list of all saved settigns

    // a map of PciDvices, use SecBus as the key for the search
    map<UINT32, PexDevice::PciDevice> m_BridgePorts;
    list<PexDevice>                   m_Bridges;

    Tee::Priority m_MsgLevel         = Tee::PriLow;
    UINT32        m_PexSnapshotCount = 0U;                  // this always increments

    // Consider removing these if GetRoot is never used...
    vector <PexDevice*> m_pRoots;
    vector<LeafDevice> m_LeafDevices;
    bool m_Initialized = false;
    bool m_Found       = false;

    Tasker::ThreadID m_ErrorCollectorThread = Tasker::NULL_THREAD;
    ModsEvent*       m_ErrorCollectorEvent  = nullptr;
    void*            m_ErrorCollectorMutex  = nullptr;
    volatile INT32   m_ErrorCollectorEnd    = 0;
    volatile INT32   m_ErrorCollectorRC     = OK;
    volatile INT32   m_ErrorCollectorPause  = 0;
    UINT32           m_ErrorPollingPeriodMs = 10U;

    RC StartErrorCollector();
    RC StopErrorCollector();
    void ErrorCollectorThread();

    //  Look through the bridge devices in the Domain, and look for the downstream port
    //  that has the SecBus & SubBus. Once the downstream port, look for sibling
    //  ports. Once all sibiling is found, we can look for the upstream port.
    //  One upstream port + 4 downstream port = 1 Bridge !
    //
    //  Note: This is a relwsive function. It will continually find the Bridge card
    //  above itself until we reach root
    bool FindPexDevice(UINT16 Domain, UINT08 SecBus, UINT08 SubBus);

    //  Return true if this downstream port requires unique link polling
    //  algorithm, false otherwise.
    bool RequiresUniqueLinkPolling(UINT32 vendorDeviceId);

    // Using the list of Bridges created, connect the PexDevice and GpuSubdevices.
    RC ConstructTopology();
    RC ConnectPorts(PexDevice* pBridgeDev);
    RC EnableBusMastering();

    RC IntRestorePexSetting(UINT32 SessionId,
                            UINT32 WhatToRestore,
                            GpuSubdevice *pSubdev);
    RC IntRestorePexSetting(UINT32 SessionId,
                            UINT32 WhatToRestore,
                            TestDevicePtr pTestDevice);

    static RC SetAspmL1SubStateInt
    (
        PexDevice::PciDevice *pDownPciDev,
        PexDevice::PciDevice *pUpPciDev,
        UINT32 downState,
        UINT32 upState
    );
    static RC SetAspmL1SubStateInt
    (
        PexDevice::PciDevice *pPciDev,
        bool bDownStream,
        UINT32 state
    );
    static RC GetPciPmL1SubStateInt
    (
        PexDevice::PciDevice *pPciDev,
        UINT32 *pState
    );
    static RC SetPciPmL1SubStateInt
    (
        PexDevice::PciDevice *pPciDev,
        bool bDownStream,
        UINT32 state
    );
};

// -----------------------------------------------------------------------------
class JsPexDev
{
public:
    JsPexDev();

    RC              CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *      GetJSObject() const { return m_pJsPexDevObj; }
    void            SetPexDev(PexDevice * pPexDev);
    PexDevice *  GetPexDev();

    bool GetIsRoot();
    UINT32 GetUpStreamDomain();
    UINT32 GetUpStreamBus();
    UINT32 GetUpStreamDev();
    UINT32 GetUpStreamFunc();
    UINT32 GetDownStreamDomain(UINT32 Index);
    UINT32 GetDownStreamBus(UINT32 Index);
    UINT32 GetDownStreamDev(UINT32 Index);
    UINT32 GetDownStreamFunc(UINT32 Index);
    UINT32 GetUpStreamSpeed();
    UINT32 GetUpStreamAspmState();
    UINT32 GetNumDPorts();
    UINT32 GetNumActiveDPorts();
    UINT32 GetDepth();
    UINT32 GetUpStreamIndex();
    UINT32 GetAspmCap();
    bool GetHasAspmL1Substates();
    Pci::PcieLinkSpeed GetDownStreamSpeed(UINT32 port); // Mbps
    RC SetDownStreamSpeed(UINT32 port, Pci::PcieLinkSpeed speed);
    static RC PexErrorCountsToJs(JSObject       *pJsErrorCounts,
                                 PexErrorCounts *pErrors);
    static RC AerLogToJs
    (
        JSContext *pContext
       ,jsval *pRetVal
       ,const vector<PexDevice::AERLogEntry> &logData
    );
    static RC GetOnAerLogFull(UINT32 *pOnFull);
    static RC SetOnAerLogFull(UINT32 onFull);

    static RC GetAerLogMaxEntries(UINT32 *pMaxEntries);
    static RC SetAerLogMaxEntries(UINT32 maxEntries);
    
    static RC GetAerUEStatusMaxClearCount(UINT32 *pMaxCount);
    static RC SetAerUEStatusMaxClearCount(UINT32 maxCount);

    static RC GetSkipHwCounterInit(bool *pSkipInit);
    static RC SetSkipHwCounterInit(bool skipInit);
private:
    PexDevice * m_pPexDev;
    JSObject *      m_pJsPexDevObj;
};
#endif
