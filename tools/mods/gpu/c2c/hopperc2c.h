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

#include "core/include/rc.h"
#include "device/interface/c2c.h"
#include "device/interface/c2c/c2lwphy.h"
#include "device/c2c/gh100c2cdiagdriver.h"

#include <memory>

class TestDevice;
class GpuSubdevice;
class Gh100C2cDiagDriver;

class HopperC2C : public C2C, public C2LWphy
{
public:
    HopperC2C() = default;
    virtual ~HopperC2C() {}

private:
    RC DoConfigurePerfMon(UINT32 pmBit, PerfMonDataType pmDataType) override;
    RC DoDumpRegs() const override;
    UINT32 DoGetActivePartitionMask() const override;
    ConnectionType DoGetConnectionType(UINT32 partitionId) const override;
    RC DoGetErrorCounts(map<UINT32, ErrorCounts> * pErrorCounts) const override;
    RC DoGetFomData(UINT32 linkId, vector<FomData> *pFomData) override;
    RC DoGetLineRateMbps(UINT32 partitionId, UINT32 *pLineRateMbps) const override;
    UINT32 DoGetLinksPerPartition() const override;
    RC DoGetLinkStatus(UINT32 linkId, LinkStatus *pStatus) const override;
    UINT32 DoGetMaxFomValue() const override;
    UINT32 DoGetMaxFomWindows() const override;
    UINT32 DoGetMaxPartitions() const override;
    UINT32 DoGetPartitionBandwidthKiBps(UINT32 partitionId) const override;
    UINT32 DoGetPerfMonBitCount() const override;
    string DoGetRemoteDeviceString(UINT32 partitionId) const override;
    Device::Id DoGetRemoteId(UINT32 partitionId) const override;
    RC DoInitialize() override;
    bool DoIsPartitionActive(UINT32 partitionId) const override;
    bool DoIsInitialized() const override { return m_bInitialized; }
    bool DoIsSupported() const override { return m_bSupported; }
    void DoPrintTopology(Tee::Priority pri) override;
    RC DoStartPerfMon() override;
    RC DoStopPerfMon(map<UINT32, vector<PerfMonCount>> * pPmCounts) override;

    // C2LWphy
    UINT32 DoGetActiveLaneMask(RegBlock regBlock) const override;
    UINT32 DoGetMaxRegBlocks(RegBlock regBlock) const override;
    UINT32 DoGetMaxLanes(RegBlock regBlock) const override;
    bool DoGetRegLogEnabled() const override { return m_bUphyRegLogEnabled; }
    UINT64 DoGetRegLogRegBlockMask(RegBlock regBlock) override;
    Version DoGetVersion() const override { return Version::V10; }
    RC DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive) override;
    bool DoIsUphySupported() const override { return true; }
    RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) override;
    RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) override;
    RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) override
        { return RC::UNSUPPORTED_FUNCTION; }
    void DoSetRegLogEnabled(bool bEnabled) override  { m_bUphyRegLogEnabled = bEnabled; }
    RC DoSetRegLogLinkMask(UINT64 linkMask) override;

    bool           m_bInitialized        = false;
    bool           m_bSupported          = true;
    UINT32         m_ActivePartitionMask = 0;
    bool           m_bLink0Only          = false;
    UINT32         m_PartitionBwKiBps    = 0;
    ConnectionType m_ConnectionType      = ConnectionType::NONE;
    UINT64         m_UphyRegsLinkMask    = 0ULL;
    bool           m_bUphyRegLogEnabled  = false;
    Device::Id     m_RemoteId;
    map<UINT32, PerfMonDataType>   m_PmConfigs;
    unique_ptr<Gh100C2cDiagDriver> m_C2CDiagDriver;

protected:
    GpuSubdevice * GetGpuSubdevice();
    const GpuSubdevice * GetGpuSubdevice() const;

    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;
};
