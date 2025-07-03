/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/device.h"
#include "core/include/utility.h"
#include <map>
#include <vector>

//--------------------------------------------------------------------
//! \brief Pure virtual interface describing C2C interfaces
//!
class C2C
{
public:
    INTERFACE_HEADER(C2C);

    enum class ConnectionType : UINT08
    {
        NONE
       ,GPU
       ,CPU
    };

    enum class PerfMonDataType : UINT08
    {
        RxDataAllVCs
       ,TxDataAllVCs
       ,None
    };

    enum class LinkStatus : UINT08
    {
        Active
       ,Off
       ,Fail
       ,Unknown
    };

    struct PerfMonCount
    {
        PerfMonDataType pmDataType = PerfMonDataType::None;
        UINT64          pmCount    = 0;
    };

    struct FomData
    {
        UINT16 passingWindow = 0;
        UINT16 numWindows    = 0;
    };

    enum DumpReason
    {
        DR_TEST,
        DR_TEST_ERROR,
        DR_ERROR,
        DR_POST_TRAINING
    };

    class ErrorCounts
    {
    public:
        //! Enum describing the error counter indexes
        enum ErrId
        {
             RX_CRC_ID
            ,TX_REPLAY_ID
            ,TX_REPLAY_B2B_FID_ID

            // Add new counter defines above this line
            ,NUM_ERR_COUNTERS
        };

        ErrorCounts();

        UINT32 GetCount(const ErrId errId) const;
        bool IsValid(const ErrId errId) const;
        RC SetCount(const ErrId errId, const UINT32 val);

        static string GetName(const ErrId errId);
        static string GetJsonTag(const ErrId errId);
    private:
        struct C2CErrorCount
        {
            UINT32 countValue;    //!< Value of the counter
            bool   bValid;        //!< True if the count is valid
            C2CErrorCount() : countValue(0), bValid(false) { }
        };

        vector<C2CErrorCount> m_ErrorCounts;
    };

    RC ConfigurePerfMon(UINT32 pmBit, PerfMonDataType pmDataType)
        { return DoConfigurePerfMon(pmBit, pmDataType); }
    RC DumpRegs() const
        { return DoDumpRegs(); }
    ConnectionType GetConnectionType(UINT32 partitionId) const
        { return DoGetConnectionType(partitionId); }
    UINT32 GetActivePartitionMask() const
        { return DoGetActivePartitionMask(); }
    RC GetErrorCounts(map<UINT32, ErrorCounts> * pErrorCounts) const
        { return DoGetErrorCounts(pErrorCounts); }
    RC GetFomData(UINT32 linkId, vector<FomData> *pFomData)
        { return DoGetFomData(linkId, pFomData); }
    RC GetLineRateMbps(UINT32 partitionId, UINT32 *pLineRateMbps) const
        { return DoGetLineRateMbps(partitionId, pLineRateMbps); }
    UINT32 GetPartitionBandwidthKiBps(UINT32 partitionId) const
        { return DoGetPartitionBandwidthKiBps(partitionId); }
    UINT32 GetLinksPerPartition() const
        { return DoGetLinksPerPartition(); }
    RC GetLinkStatus(UINT32 linkId, LinkStatus *pStatus) const
        { return DoGetLinkStatus(linkId, pStatus); }
    UINT32 GetMaxFomValue()
        { return DoGetMaxFomValue(); }
    UINT32 GetMaxFomWindows()
        { return DoGetMaxFomWindows(); }
    UINT32 GetMaxLinks() const
        { return GetMaxPartitions() * GetLinksPerPartition(); }
    UINT32 GetMaxPartitions() const
        { return DoGetMaxPartitions(); }
    UINT32 GetPerfMonBitCount() const
        { return DoGetPerfMonBitCount(); }
    string GetRemoteDeviceString(UINT32 partitionId) const
        { return DoGetRemoteDeviceString(partitionId); }
    Device::Id GetRemoteId(UINT32 partitionId) const
        { return DoGetRemoteId(partitionId); }
    RC Initialize()
        { return DoInitialize(); }
    bool IsPartitionActive(UINT32 partitionId) const
        { return DoIsPartitionActive(partitionId); }
    bool IsInitialized() const
        { return DoIsInitialized(); }
    bool IsSupported() const
        { return DoIsSupported(); }
    void PrintTopology(Tee::Priority pri)
        { DoPrintTopology(pri); }
    RC StartPerfMon()
        { return DoStartPerfMon(); }
    RC StopPerfMon(map<UINT32, vector<PerfMonCount>> * pPmCounts)
        { return DoStopPerfMon(pPmCounts); }

    static const char * LinkStatusString(LinkStatus status);

    static constexpr UINT32 UNKNOWN_LINE_RATE = ~0U;
protected:
    virtual RC DoConfigurePerfMon(UINT32 pmBit, PerfMonDataType pmDataType) = 0;
    virtual RC DoDumpRegs() const = 0;
    virtual ConnectionType DoGetConnectionType(UINT32 partitionId) const = 0;
    virtual UINT32 DoGetActivePartitionMask() const = 0;
    virtual RC DoGetErrorCounts(map<UINT32, ErrorCounts> * pErrorCounts) const = 0;
    virtual RC DoGetFomData(UINT32 linkId, vector<FomData> *pFomData) = 0;
    virtual RC DoGetLineRateMbps(UINT32 partitionId, UINT32 *pLineRateMbps) const = 0;
    virtual UINT32 DoGetPartitionBandwidthKiBps(UINT32 partitionId) const = 0;
    virtual UINT32 DoGetLinksPerPartition() const = 0;
    virtual RC DoGetLinkStatus(UINT32 linkId, LinkStatus *pStatus) const = 0;
    virtual UINT32 DoGetMaxFomValue() const = 0;
    virtual UINT32 DoGetMaxFomWindows() const = 0;
    virtual UINT32 DoGetMaxPartitions() const = 0;
    virtual UINT32 DoGetPerfMonBitCount() const = 0;
    virtual string DoGetRemoteDeviceString(UINT32 partitionId) const = 0;
    virtual Device::Id DoGetRemoteId(UINT32 partitionId) const = 0;
    virtual RC DoInitialize() = 0;
    virtual bool DoIsPartitionActive(UINT32 partitionId) const = 0;
    virtual bool DoIsInitialized() const = 0;
    virtual bool DoIsSupported() const = 0;
    virtual void DoPrintTopology(Tee::Priority pri) = 0;
    virtual RC DoStartPerfMon() = 0;
    virtual RC DoStopPerfMon(map<UINT32, vector<PerfMonCount>> * pPmCounts) = 0;
};

