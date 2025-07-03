/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file lwlink.h -- LwLink interface defintion

#pragma once

#include "core/include/utility.h"
#include "core/include/device.h"

#include <map>
#include <string>
#include <vector>
//--------------------------------------------------------------------
//! \brief Pure virtual interface describing LwLink interfaces specific to a GPU
//!
class LwLink
{
public:
    INTERFACE_HEADER(LwLink);

    enum CeTarget
    {
        CET_SYSMEM
       ,CET_PEERMEM
    };
    enum CeWidth
    {
        CEW_128_BYTE
       ,CEW_256_BYTE
    };
    enum DataType
    {
        DT_REQUEST  = (1 << 0)
       ,DT_RESPONSE = (1 << 1)
       ,DT_ALL      = (DT_REQUEST | DT_RESPONSE)
    };
    enum FomMode
    {
        EYEO_X,
        EYEO_XL_O,
        EYEO_XL_E,
        EYEO_XH_O,
        EYEO_XH_E,
        EYEO_Y,
        EYEO_Y_U,
        EYEO_Y_M,
        EYEO_Y_L,
        EYEO_YL_O,
        EYEO_YL_E,
        EYEO_YH_O,
        EYEO_YH_E
    };
    enum HwType
    {
        HT_CE
       ,HT_TWOD
       ,HT_SM
       ,HT_QUAL_ENG
       ,HT_TREX
       ,HT_NONE
    };
    enum LinkState
    {
        LS_INIT
       ,LS_HWCFG
       ,LS_SWCFG
       ,LS_ACTIVE
       ,LS_FAULT
       ,LS_SLEEP
       ,LS_RECOVERY
       ,LS_ILWALID
    };
    enum SubLinkDir
    {
        TX
       ,RX
    };
    enum SubLinkState
    {
        SLS_OFF
       ,SLS_SAFE_MODE
       ,SLS_TRAINING
       ,SLS_SINGLE_LANE
       ,SLS_HIGH_SPEED
       ,SLS_ILWALID
    };
    enum TransferType
    {
        TT_UNIDIR_READ
       ,TT_UNIDIR_WRITE
       ,TT_BIDIR_READ
       ,TT_BIDIR_WRITE
       ,TT_READ_WRITE
    };

    enum class SystemParameter : UINT08
    {
        RESTORE_PHY_PARAMS
       ,SL_ENABLE
       ,L2_ENABLE
       ,LINE_RATE
       ,LINE_CODE_MODE
       ,LINK_DISABLE
       ,TXTRAIN_OPT_ALGO
       ,BLOCK_CODE_MODE
       ,REF_CLOCK_MODE
       ,LINK_INIT_DISABLE
       ,ALI_ENABLE
       ,TXTRAIN_MIN_TRAIN_TIME_MANTISSA
       ,TXTRAIN_MIN_TRAIN_TIME_EXPONENT
       ,L1_MIN_RECAL_TIME_MANTISSA
       ,L1_MIN_RECAL_TIME_EXPONENT
       ,L1_MAX_RECAL_PERIOD_MANTISSA
       ,L1_MAX_RECAL_PERIOD_EXPONENT
       ,DISABLE_UPHY_UCODE_LOAD
       ,CHANNEL_TYPE
       ,L1_ENABLE
    };

    enum class SystemLineRate : UINT08
    {
        GBPS_50_0
       ,GBPS_16_0
       ,GBPS_20_0
       ,GBPS_25_0
       ,GBPS_25_7
       ,GBPS_32_0
       ,GBPS_40_0
       ,GBPS_53_1
       ,GBPS_28_1
       ,GBPS_100_0
       ,GBPS_106_2
       ,GBPS_UNKNOWN
    };

    enum class SystemLineCode : UINT08
    {
        NRZ
       ,NRZ_128B130
       ,PAM4 = 3
    };

    enum class SystemTxTrainAlg : UINT08
    {
        A0_SINGLE_PRESET
       ,A1_PRESET_ARRAY
       ,A2_FINE_GRAINED_EXHAUSTIVE
       ,A4_FOM_CENTROID = 4
    };

    enum class SystemBlockCodeMode : UINT08
    {
        OFF
       ,ECC96
       ,ECC88
       ,ECC89
    };

    enum class SystemRefClockMode : UINT08
    {
        COMMON,
        RESERVED,
        NON_COMMON_NO_SS,
        NON_COMMON_SS
    };

    enum class SystemChannelType : UINT08
    {
        GENERIC         = 0
       ,ACTIVE_0        = 4
       ,ACTIVE_1        = 5
       ,ACTIVE_CABLE_0  = 8
       ,ACTIVE_CABLE_1  = 9
       ,OPTICAL_CABLE_0 = 12
       ,OPTICAL_CABLE_1 = 13
       ,DIRECT_0        = 14
    };

    union SystemParamValue
    {
        bool                bEnable;
        SystemLineRate      lineRate;
        SystemLineCode      lineCode;
        SystemTxTrainAlg    txTrainAlg;
        SystemBlockCodeMode blockCodeMode;
        SystemRefClockMode  refClockMode;
        SystemChannelType   channelType;
        UINT08              rangeValue;
    };

    //! Miscellaneous enumerations
    enum
    {
        ILWALID_LINK_ID   = ~0U      //!< Value if a link ID is invalid
       ,UNKNOWN_RATE      = ~0U      //!< Value if clock/line/bw is unknown
       ,ILWALID_LINK_MASK = ~0U      //!< Value if link mask is unknown
       ,ALL_LINKS         = ~0U
       ,ILWALID_LANE_MASK = ~0U
    };

    //! Structure for storing link endpoint identifier information
    struct EndpointInfo
    {
        Device::Id   id;          //!< Id of the remote LwLink device
        Device::Type devType;     //!< Type of remote link
        UINT32 pciDomain;         //!< Pci domain of the endpoint
        UINT32 pciBus;            //!< Pci bus of the endpoint
        UINT32 pciDevice;         //!< Pci device of the endpoint
        UINT32 pciFunction;       //!< Pci function of the endpoint
        UINT32 linkNumber;        //!< Link number on the remote endpoint
        EndpointInfo() : devType(Device::TYPE_UNKNOWN), pciDomain(~0), pciBus(~0),
                         pciDevice(~0), pciFunction(~0), linkNumber(~0) { }
    };

    //! Structure for storing the link status of each link
    struct LinkInfo
    {
        bool         bValid               = false;             //!< True if the link is valid
        bool         bActive              = false;             //!< True if the link is active
        UINT32       version              = 0;                 //!< LwLink version that the link negotiated at
        UINT32       lineRateMbps         = UNKNOWN_RATE;      //!< Rate that the bits toggle on the link
        UINT32       refClkSpeedMhz       = UNKNOWN_RATE;      //!< Reference clock rate of the link
        UINT32       maxLinkDataRateKiBps = UNKNOWN_RATE;      //!< Effective rate available for transactions
                                                               //!< after subtracting overhead, as seen at Data
                                                               //!< Layer in kibibytes (1024 bytes) per second.
        UINT32       linkClockMHz         = UNKNOWN_RATE;      //!< Link Clock in MHz
        UINT32       sublinkWidth         = 0;                 //!< Number of lanes in this link
        bool         bLanesReversed       = false;             //!< Number of lanes in this link
        UINT32       rxdetLaneMask        = ILWALID_LANE_MASK; //!< Mask of lanes that RXDET passed on (Ampere+ only)
        LinkInfo() = default;
        LinkInfo(bool v, bool a, UINT32 ver, UINT32 r, UINT32 rc, UINT32 ldr, UINT32 sw, bool lr) :
            bValid(v), bActive(a), version(ver),
            lineRateMbps(r), refClkSpeedMhz(rc), maxLinkDataRateKiBps(ldr),
            sublinkWidth(sw), bLanesReversed(lr) { };
    };

    struct LinkStatus
    {
        LinkState    linkState;
        SubLinkState rxSubLinkState;
        SubLinkState txSubLinkState;
    };

    struct EomSettings
    {
        UINT32 numBlocks = 0;
        UINT32 numErrors = 0;
        EomSettings(UINT32 nb, UINT32 ne) : numBlocks(nb), numErrors(ne) { }
    };

    enum DumpReason
    {
        DR_TEST,
        DR_TEST_ERROR,
        DR_ERROR,
        DR_POST_DISCOVERY,
        DR_PRE_TRAINING,
        DR_POST_TRAINING,
        DR_POST_SHUTDOWN
    };

    enum class NearEndLoopMode : UINT08
    {
        None,
        NEA,
        NED
    };

    //------------------------------------------------------------------------------
    //! \brief Abstraction for a set of error counts associated with a single
    //! endpoint of an LwLink
    //!
    class ErrorCounts
    {
    public:
        //! Enum describing the error counter indexes
        enum ErrId
        {
             LWL_RX_CRC_FLIT_ID
            ,LWL_PRE_FEC_ID
            ,LWL_TX_REPLAY_ID
            ,LWL_RX_REPLAY_ID
            ,LWL_TX_RECOVERY_ID
            ,LWL_SW_RECOVERY_ID
            ,LWL_UPHY_REFRESH_FAIL_ID
            ,LWL_ECC_DEC_FAILED_ID
            ,LWL_RX_CRC_LANE0_ID
            ,LWL_RX_CRC_LANE1_ID
            ,LWL_RX_CRC_LANE2_ID
            ,LWL_RX_CRC_LANE3_ID
            ,LWL_RX_CRC_LANE4_ID
            ,LWL_RX_CRC_LANE5_ID
            ,LWL_RX_CRC_LANE6_ID
            ,LWL_RX_CRC_LANE7_ID
            ,LWL_RX_CRC_MASKED_ID
            ,LWL_FEC_CORRECTIONS_LANE0_ID
            ,LWL_FEC_CORRECTIONS_LANE1_ID
            ,LWL_FEC_CORRECTIONS_LANE2_ID
            ,LWL_FEC_CORRECTIONS_LANE3_ID
            ,LWL_UPHY_REFRESH_PASS_ID

            // Add new counter defines above this line
            ,LWL_NUM_ERR_COUNTERS
        };

        ErrorCounts();

        UINT64 GetCount(const ErrId errId) const;
        bool IsValid(const ErrId errId) const;
        bool DidCountOverflow(const ErrId errId) const;
        RC SetCount(const ErrId errId, const UINT32 val, bool bOverflowed);
        bool operator!=(const LwLink::ErrorCounts &rhs) const;
        LwLink::ErrorCounts & operator+=(const LwLink::ErrorCounts &rhs);

        static string GetJsonTag(const ErrId errId);
        static UINT32 GetLane(const ErrId errId);
        static string GetName(const ErrId errId);
        static bool IsInternalOnly(const ErrId errId);
        static bool IsCountOnly(const ErrId errId);
        static bool IsThreshold(const ErrId errId);

        static const UINT32 LWLINK_COUNTER_MASK;
        static const UINT32 LWLINK_PER_LANE_COUNTER_MASK;
        static const UINT32 LWLINK_ILWALID_LANE;
    private:
        //! Structure describing a single error count
        struct LwlErrorCount
        {
            UINT64 countValue;    //!< Value of the counter
            bool   bValid;        //!< True if the count is valid
            bool   bOverflowed;   //!< True if the counter overflowed
            LwlErrorCount() : countValue(0), bValid(false), bOverflowed(false) { }
        };
        vector<LwlErrorCount> m_ErrorCounts;  //!< Vector of all counts

        //! Structure describing a count and how it is used
        struct ErrIdDescriptor
        {
            string name;            //!< Name of the count
            string jsonTag;         //!< JSON tag of the count
            bool   bThreshold;      //!< true if the count has a threshold applied
            bool   bCountOnly;      //!< true if the threshold should always use counts and not rates
            bool   bInternalOnly;   //!< true if the count is internal only
            UINT32 lane;            //!< LwLink lane associated with the error ID
                                    //!< ILWALID_LANE if not associated with a lane
            ErrIdDescriptor() :
                name(""), jsonTag(""), bThreshold(false), bCountOnly(false), bInternalOnly(true),
                lane(LWLINK_ILWALID_LANE) { }
            ErrIdDescriptor(string n, string j, bool bThresh, bool bCountOnly, bool bInt, UINT32 l) :
                name(n), jsonTag(j), bThreshold(bThresh), bCountOnly(bCountOnly), bInternalOnly(bInt),
                lane(l) { }
        };
        //! Static map describing each count type
        static map<ErrId, ErrIdDescriptor> s_ErrorDescriptors;
    };

    struct ErrorFlags
    {
         map<UINT32, vector<string>> linkErrorFlags;
         map<UINT32, vector<string>> linkGroupErrorFlags;
    };

    using FabricAperture = pair<UINT64, UINT64>;

    using PerLaneFomData = vector<UINT16>;

    using LibDevHandles_t = vector<Device::LibDeviceHandle>;
    using LibDevHandles = const LibDevHandles_t &;
    static const LibDevHandles_t NO_HANDLES; // Same as {}

    virtual ~LwLink() {};

    static const char* GetHwTypeName(HwType hwType)
        { return (hwType == HT_CE)        ? "CE"   :
                 (hwType == HT_TWOD)      ? "TwoD" :
                 (hwType == HT_SM)        ? "SM"   :
                 (hwType == HT_QUAL_ENG)  ? "QUAL ENG"  :
                                            "None"; }
    bool AreLanesReversed(UINT32 linkId) const
        { return DoAreLanesReversed(linkId); }
    RC AssertBufferReady(UINT32 linkId, bool ready)
        { return DoAssertBufferReady(linkId, ready); }
    RC ClearErrorCounts(UINT32 linkId)
        { return DoClearErrorCounts(linkId); }
    RC ConfigurePorts(UINT32 topologyId)
        { return DoConfigurePorts(topologyId); }
    RC DetectTopology()
        { return DoDetectTopology(); }
    RC DumpRegs(UINT32 linkId) const
        { return DoDumpRegs(linkId); }
    RC DumpAllRegs()
        {
            RC rc;
            for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
                CHECK_RC(DumpAllRegs(linkId));
            return rc;
        }
    RC DumpAllRegs(UINT32 linkId)
        { return DoDumpAllRegs(linkId); }
    RC EnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
        { return DoEnablePerLaneErrorCounts(linkId, bEnable); }
    RC EnablePerLaneErrorCounts(bool bEnable)
        {
            RC rc;
            for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
                if (IsLinkActive(linkId))
                    CHECK_RC(EnablePerLaneErrorCounts(linkId, bEnable));
            return rc;
        }
    CeWidth GetCeCopyWidth(CeTarget target) const
        { return DoGetCeCopyWidth(target); }
    FLOAT64 GetDefaultErrorThreshold
    (
        LwLink::ErrorCounts::ErrId errId,
        bool bRateErrors
    ) const
        { return DoGetDefaultErrorThreshold(errId, bRateErrors); }
    RC GetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo)
        { return DoGetDetectedEndpointInfo(pEndpointInfo); }
    RC GetDiscoveredLinkMask(UINT64 *pLinkMask)
        { return DoGetDiscoveredLinkMask(pLinkMask); }
    void GetEomDefaultSettings(UINT32 link, EomSettings* pSettings) const
        { DoGetEomDefaultSettings(link, pSettings); }
    RC GetEomStatus
    (
        FomMode mode
       ,UINT32 link
       ,UINT32 numErrors
       ,UINT32 numBlocks
       ,FLOAT64 timeoutMs
       ,vector<INT32>* status
    ) { return DoGetEomStatus(mode, link, numErrors, numBlocks, timeoutMs, status); }
    RC GetFomStatus(vector<PerLaneFomData> * pFomData)
        { return DoGetFomStatus(pFomData); }
    RC GetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
    {
        RC rc;
        CHECK_RC(DoGetErrorCounts(linkId, pErrorCounts));
        if (DoSupportsErrorCaching())
        {
            CHECK_RC(DoClearHwErrorCounts(linkId));
        }
        return rc;
    }
    RC GetErrorFlags(ErrorFlags * pErrorFlags)
        { return DoGetErrorFlags(pErrorFlags); }
    UINT32 GetFabricRegionBits() const
        { return DoGetFabricRegionBits(); }
    RC GetFabricApertures(vector<FabricAperture> *pFas)
        { return DoGetFabricApertures(pFas); }
    void GetFlitInfo
    (
        UINT32 transferWidth
       ,HwType hwType
       ,bool bSysmem
       ,FLOAT64 *pReadDataFlits
       ,FLOAT64 *pWriteDataFlits
       ,UINT32 *pTotalReadFlits
       ,UINT32 *pTotalWriteFlits
    ) const { DoGetFlitInfo(transferWidth, hwType, bSysmem,
                            pReadDataFlits, pWriteDataFlits, pTotalReadFlits, pTotalWriteFlits); }
    UINT32 GetLineRateMbps(UINT32 linkId) const
        { return DoGetLineRateMbps(linkId); }
    UINT32 GetLinkClockRateMhz(UINT32 linkId) const
        { return DoGetLinkClockRateMhz(linkId); }
    UINT32 GetLinkDataRateKiBps(UINT32 linkId) const
        { return DoGetLinkDataRateKiBps(linkId); }
    string GetLinkGroupName() const
        { return DoGetLinkGroupName(); }
    RC GetLinkId(const EndpointInfo &endpointInfo, UINT32 *pLinkId) const
        { return DoGetLinkId(endpointInfo, pLinkId); }
    RC GetLinkInfo(vector<LinkInfo> *pLinkInfo)
        { return DoGetLinkInfo(pLinkInfo); }
    UINT32 GetLinksPerGroup() const
        { return DoGetLinksPerGroup(); }
    RC GetLinkStatus(vector<LinkStatus> *pLinkStatus)
        { return DoGetLinkStatus(pLinkStatus); }
    UINT32 GetLinkVersion(UINT32 linkId) const
        { return DoGetLinkVersion(linkId); }
    UINT32 GetMaxLinkGroups() const
        { return DoGetMaxLinkGroups(); }
    UINT32 GetMaxLinkDataRateKiBps(UINT32 linkId) const
        { return DoGetMaxLinkDataRateKiBps(linkId); }
    UINT32 GetMaxLinks() const
        { return DoGetMaxLinks(); }
    UINT64 GetMaxLinkMask() const
        { return DoGetMaxLinkMask(); }
    FLOAT64 GetMaxObservableBwPct(LwLink::TransferType tt)
        { return DoGetMaxObservableBwPct(tt); }
    UINT32 GetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const
        { return DoGetMaxPerLinkPerDirBwKiBps(lineRateMbps); }
    UINT32 GetMaxTotalBwKiBps(UINT32 lineRateMbps) const
        { return DoGetMaxTotalBwKiBps(lineRateMbps); }
    FLOAT64 GetMaxTransferEfficiency() const
        { return DoGetMaxTransferEfficiency(); }
    UINT64 GetNeaLoopbackLinkMask()
        { return DoGetNeaLoopbackLinkMask(); }
    UINT64 GetNedLoopbackLinkMask()
        { return DoGetNedLoopbackLinkMask(); }
    RC GetOutputLinks(UINT32 inputLink, UINT64 fabricBase, DataType dt, set<UINT32>* pPorts) const
        { return DoGetOutputLinks(inputLink, fabricBase, dt, pPorts); }
    virtual RC GetPciInfo
    (
        UINT32  linkId,
        UINT32 *pDomain,
        UINT32 *pBus,
        UINT32 *pDev,
        UINT32 *pFunc
    ) { return DoGetPciInfo(linkId, pDomain, pBus, pDev, pFunc); }
    UINT32 GetRefClkSpeedMhz(UINT32 linkId) const
        { return DoGetRefClkSpeedMhz(linkId); }
    UINT32 GetRxdetLaneMask(UINT32 linkId) const
        { return DoGetRxdetLaneMask(linkId); }
    RC GetRemoteEndpoint(UINT32 locLinkId, EndpointInfo *pRemoteEndpoint) const
        { return DoGetRemoteEndpoint(locLinkId, pRemoteEndpoint); }
    UINT32 GetSublinkWidth(UINT32 linkId) const
        { return DoGetSublinkWidth(linkId); }
    SystemLineRate GetSystemLineRate(UINT32 linkId) const
        { return DoGetSystemLineRate(linkId); }
    UINT32 GetTopologyId() const
        { return DoGetTopologyId(); }
    RC GetXbarBandwidth
    (
        bool bWriting,
        bool bReading,
        bool bWrittenTo,
        bool bReadFrom,
        UINT32 *pXbarBwKiBps
    ) { return DoGetXbarBandwidth(bWriting, bReading, bWrittenTo, bReadFrom, pXbarBwKiBps); }
    bool HasCollapsedResponses() const
        { return DoHasCollapsedResponses(); }
    bool HasNonPostedWrites() const
        { return DoHasNonPostedWrites(); }
    RC Initialize(LibDevHandles handles)
        { return DoInitialize(handles); }
    RC Initialize(Device::LibDeviceHandle handle)
        { return DoInitialize(LibDevHandles{handle}); }
    RC Initialize()
        { return DoInitialize(NO_HANDLES); }
    RC InitializePostDiscovery()
        { return DoInitializePostDiscovery(); }
    bool IsInitialized() const
        { return DoIsInitialized(); }
    bool IsLinkAcCoupled(UINT32 linkId) const
        { return DoIsLinkAcCoupled(linkId); }
    bool IsLinkActive(UINT32 linkId) const
        { return DoIsLinkActive(linkId); }
    bool IsLinkPam4(UINT32 linkId) const
        { return DoIsLinkPam4(linkId); }
    bool IsLinkValid(UINT32 linkId) const
        { return DoIsLinkValid(linkId); }
    RC IsPerLaneEccEnabled(UINT32 linkId, bool* pbPerLaneEccEnabled)
        { return DoIsPerLaneEccEnabled(linkId, pbPerLaneEccEnabled); }
    bool IsPostDiscovery() const
        { return DoIsPostDiscovery(); }
    bool IsSupported(LibDevHandles handles) const
        { return DoIsSupported(handles); }
    bool IsSupported(Device::LibDeviceHandle handle) const
        { return DoIsSupported(LibDevHandles_t{handle}); }
    bool IsSupported() const
        { return DoIsSupported(NO_HANDLES); }
    RC PerLaneErrorCountsEnabled(UINT32 linkId, bool* pbPerLaneEnabled)
        { return DoPerLaneErrorCountsEnabled(linkId, pbPerLaneEnabled); }
    RC ReleaseCounters()
        { return DoReleaseCounters(); }
    RC ReserveCounters()
        { return DoReserveCounters(); }
    RC ResetTopology()
        { return DoResetTopology(); }
    void SetCeCopyWidth(CeTarget target, CeWidth width)
        { DoSetCeCopyWidth(target, width); }
    RC SetCollapsedResponses(UINT32 mask)
        { return DoSetCollapsedResponses(mask); }
    RC SetCompressedResponses(bool bEnable)
        { return DoSetCompressedResponses(bEnable); }
    RC SetDbiEnable(bool bEnable)
        { return DoSetDbiEnable(bEnable); }
    void SetEndpointInfo(const vector<EndpointInfo>& endpointInfo)
        { DoSetEndpointInfo(endpointInfo); }
    RC SetFabricApertures(const vector<FabricAperture> &fas)
        { return DoSetFabricApertures(fas); }
    RC SetLinkState(SubLinkState state)
        { return DoSetLinkState(GetMaxLinkMask(), state); }
    RC SetLinkState(UINT32 linkId, SubLinkState state)
        { return DoSetLinkState(1ULL << linkId, state); }
    void SetNeaLoopbackLinkMask(UINT64 linkMask)
        { DoSetNeaLoopbackLinkMask(linkMask); }
    RC SetNearEndLoopbackMode(UINT64 linkMask, NearEndLoopMode loopMode)
        { return DoSetNearEndLoopbackMode(linkMask, loopMode); }
    void SetNedLoopbackLinkMask(UINT64 linkMask)
        { DoSetNedLoopbackLinkMask(linkMask); }
    RC SetSystemParameter(UINT64 linkMask, SystemParameter parm, SystemParamValue val)
        { return DoSetSystemParameter(linkMask, parm, val); }
    RC SetupTopology(const vector<EndpointInfo> &endpointInfo, UINT32 topologyId)
        { return DoSetupTopology(endpointInfo, topologyId); }
    RC Shutdown()
        { return DoShutdown(); }
    bool SupportsFomMode(FomMode mode) const
        { return DoSupportsFomMode(mode); }
    bool SupportsErrorCaching() const
        { return DoSupportsErrorCaching(); }
    RC WaitForLinkInit()
        { return DoWaitForLinkInit(); }
    RC WaitForIdle(FLOAT64 timeoutMs)
        { return DoWaitForIdle(timeoutMs); }
    RC GetLineCode(UINT32 linkId, SystemLineCode *pLineCode)
        { return DoGetLineCode(linkId, pLineCode); }
    RC GetBlockCodeMode(UINT32 linkId, SystemBlockCodeMode *pBlockCodeMode)
        { return DoGetBlockCodeMode(linkId, pBlockCodeMode); }

    static LinkState RmLinkStateToLinkState(UINT32 rmState);
    static SubLinkState RmSubLinkStateToLinkState(UINT32 rmState);
    static string LinkStateToString(LinkState state);
    static string SubLinkStateToString(SubLinkState state);
    static string TransferTypeString(TransferType t);
    static string SystemParameterString(SystemParameter sysParam);
    static string BlockCodeModeToString(SystemBlockCodeMode blockMode);
    static string LineCodeToString(SystemLineCode lineCode);
    static const char *  NearEndLoopModeToString(NearEndLoopMode nelMode);

protected:
    virtual bool DoAreLanesReversed(UINT32 linkId) const = 0;
    virtual RC DoAssertBufferReady(UINT32 linkId, bool ready) = 0;
    virtual RC DoClearErrorCounts(UINT32 linkId) = 0;
    virtual RC DoClearHwErrorCounts(UINT32 linkId) = 0;
    virtual RC DoConfigurePorts(UINT32 topologyId) = 0;
    virtual RC DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable) = 0;
    virtual RC DoDetectTopology() = 0;
    virtual RC DoDumpRegs(UINT32 linkId) const = 0;
    virtual RC DoDumpAllRegs(UINT32 linkId) = 0;
    virtual CeWidth DoGetCeCopyWidth(CeTarget target) const = 0;
    virtual FLOAT64 DoGetDefaultErrorThreshold
    (
        LwLink::ErrorCounts::ErrId errId,
        bool bRateErrors
    ) const = 0;
    virtual RC DoGetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo) = 0;
    virtual RC DoGetDiscoveredLinkMask(UINT64 *pLinkMask) = 0;
    virtual void DoGetEomDefaultSettings
    (
        UINT32 link,
        EomSettings* pSettings
    ) const = 0;
    virtual RC DoGetEomStatus
    (
        FomMode mode
       ,UINT32 link
       ,UINT32 numErrors
       ,UINT32 numBlocks
       ,FLOAT64 timeoutMs
       ,vector<INT32>* status
    ) = 0;
    virtual RC DoGetFomStatus(vector<PerLaneFomData> * pFomData) = 0;
    virtual RC DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts) = 0;
    virtual RC DoGetErrorFlags(ErrorFlags * pErrorFlags) = 0;
    virtual UINT32 DoGetFabricRegionBits() const = 0;
    virtual RC DoGetFabricApertures(vector<FabricAperture> *pFas) = 0;
    virtual void DoGetFlitInfo
    (
        UINT32 transferWidth
       ,HwType hwType
       ,bool bSysmem
       ,FLOAT64 *pReadDataFlits
       ,FLOAT64 *pWriteDataFlits
       ,UINT32 *pTotalReadFlits
       ,UINT32 *pTotalWriteFlits
    ) const = 0;
    virtual UINT32 DoGetLineRateMbps(UINT32 linkId) const = 0;
    virtual UINT32 DoGetLinkClockRateMhz(UINT32 linkId) const = 0;
    virtual UINT32 DoGetLinkDataRateKiBps(UINT32 linkId) const = 0;
    virtual string DoGetLinkGroupName() const = 0;
    virtual RC DoGetLinkId(const EndpointInfo &endpointInfo, UINT32 *pLinkId) const = 0;
    virtual RC DoGetLinkInfo(vector<LinkInfo> *pLinkInfo) = 0;
    virtual UINT32 DoGetLinksPerGroup() const = 0;
    virtual RC DoGetLinkStatus(vector<LinkStatus> *pLinkStatus) = 0;
    virtual UINT32 DoGetLinkVersion(UINT32 linkId) const = 0;
    virtual UINT32 DoGetMaxLinkDataRateKiBps(UINT32 linkId) const = 0;
    virtual UINT32 DoGetMaxLinkGroups() const = 0;
    virtual UINT32 DoGetMaxLinks() const = 0;
    virtual UINT64 DoGetMaxLinkMask() const = 0;
    virtual FLOAT64 DoGetMaxObservableBwPct(LwLink::TransferType tt) = 0;
    virtual UINT32 DoGetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const = 0;
    virtual UINT32 DoGetMaxTotalBwKiBps(UINT32 lineRateMbps) const = 0;
    virtual FLOAT64 DoGetMaxTransferEfficiency() const = 0;
    virtual UINT64 DoGetNeaLoopbackLinkMask() = 0;
    virtual UINT64 DoGetNedLoopbackLinkMask() = 0;
    virtual RC DoGetOutputLinks(UINT32 inputLink, UINT64 fabricBase, DataType dt, set<UINT32>* pPorts) const = 0;
    virtual RC DoGetPciInfo
    (
        UINT32  linkId,
        UINT32 *pDomain,
        UINT32 *pBus,
        UINT32 *pDev,
        UINT32 *pFunc
    ) = 0;
    virtual UINT32 DoGetRefClkSpeedMhz(UINT32 linkId) const = 0;
    virtual RC DoGetRemoteEndpoint(UINT32 locLinkId, EndpointInfo *pRemoteEndpoint) const = 0;
    virtual UINT32 DoGetRxdetLaneMask(UINT32 linkId) const = 0;
    virtual UINT32 DoGetSublinkWidth(UINT32 linkId) const = 0;
    virtual SystemLineRate DoGetSystemLineRate(UINT32 linkId) const = 0;
    virtual UINT32 DoGetTopologyId() const = 0;
    virtual RC DoGetXbarBandwidth
    (
        bool bWriting,
        bool bReading,
        bool bWrittenTo,
        bool bReadFrom,
        UINT32 *pXbarBwKiBps
    ) = 0;
    virtual bool DoHasCollapsedResponses() const = 0;
    virtual bool DoHasNonPostedWrites() const = 0;
    virtual RC DoInitialize(LibDevHandles handles) = 0;
    virtual RC DoInitializePostDiscovery() = 0;
    virtual bool DoIsInitialized() const = 0;
    virtual bool DoIsLinkAcCoupled(UINT32 linkId) const = 0;
    virtual bool DoIsLinkActive(UINT32 linkId) const = 0;
    virtual bool DoIsLinkPam4(UINT32 linkId) const = 0;
    virtual bool DoIsLinkValid(UINT32 linkId) const = 0;
    virtual RC   DoIsPerLaneEccEnabled(UINT32 linkId, bool* pbPerLaneEccEnabled) const = 0;
    virtual bool DoIsPostDiscovery() const = 0;
    virtual bool DoIsSupported(LibDevHandles handles) const = 0;
    virtual RC DoPerLaneErrorCountsEnabled(UINT32 linkId, bool* pbPerLaneEnabled) = 0;
    virtual RC DoReleaseCounters() = 0;
    virtual RC DoReserveCounters() = 0;
    virtual RC DoResetTopology() = 0;
    virtual void DoSetCeCopyWidth(CeTarget target, CeWidth width) = 0;
    virtual RC DoSetCollapsedResponses(UINT32 mask) = 0;
    virtual RC DoSetCompressedResponses(bool bEnable) = 0;
    virtual RC DoSetDbiEnable(bool bEnable) = 0;
    virtual void DoSetEndpointInfo(const vector<EndpointInfo>& endpointInfo) = 0;
    virtual RC DoSetFabricApertures(const vector<FabricAperture> &fas) = 0;
    virtual RC DoSetLinkState(UINT64 linkMask, SubLinkState state) = 0;
    virtual void DoSetNeaLoopbackLinkMask(UINT64 linkMask) = 0;
    virtual RC DoSetNearEndLoopbackMode(UINT64 linkMask, NearEndLoopMode loopMode) = 0;
    virtual void DoSetNedLoopbackLinkMask(UINT64 linkMask) = 0;
    virtual RC DoSetSystemParameter(UINT64 linkMask, SystemParameter parm, SystemParamValue val) = 0;
    virtual RC DoSetupTopology(const vector<EndpointInfo> &endpointInfo, UINT32 topologyId) = 0;
    virtual RC DoShutdown() = 0;
    virtual bool DoSupportsFomMode(FomMode mode) const = 0;
    virtual bool DoSupportsErrorCaching() const = 0;
    virtual RC DoWaitForLinkInit() = 0;
    virtual RC DoWaitForIdle(FLOAT64 timeoutMs) = 0;
    virtual RC DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode) = 0;
    virtual RC DoGetBlockCodeMode(UINT32 linkId, SystemBlockCodeMode *pBlockCodeMode) = 0;
};
