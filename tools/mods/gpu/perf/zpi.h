/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file zpi.h
 * @brief Implement algorithms to perform Zero Power Idle
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/setget.h"
#include "device/utility/genericipmi.h"

class GpuDevice;
class GpuSubdevice;
class PexDevice;
class SmbManager;
class SmbPort;

class ZeroPowerIdle
{
public:
    ZeroPowerIdle();
    bool IsSupported(const GpuSubdevice *pSubdev);
    RC Setup(GpuSubdevice *pSubdev);
    RC Setup(UINT32 domain, UINT32 bus,
             UINT32 device, UINT32 function,
             UINT08 smbAddress, PexDevice* pPexDev, UINT32 port);
    RC Cleanup();
    RC EnterZPI(UINT32 enterDelayUsec);
    RC ExitZPI();
    RC ReInitializePciInterface(GpuSubdevice *pSubdev);
    RC IsGpuOnTheBus(GpuSubdevice *pSubdev, bool *pGpuOnTheBus);
    SETGET_PROP(DelayBeforeOOBEntryMs, UINT32);
    SETGET_PROP(DelayAfterOOBExitMs, UINT32);
    SETGET_PROP(DelayBeforeRescanMs, UINT32);
    SETGET_PROP(RescanRetryDelayMs, UINT32);
    SETGET_PROP(IstMode, bool);
    SETGET_PROP(DelayAfterPexRstAssertMs, UINT32);
    SETGET_PROP(DelayBeforePexDeassertMs, UINT32);
    SETGET_PROP(LinkPollTimeoutMs, UINT32);
    SETGET_PROP(DelayAfterExitZpiMs, UINT32);
    SETGET_PROP(IsSeleneSystem, bool);
    SETGET_PROP(DelayAfterPollingMs, UINT32);
    SETGET_PROP(DelayAfterBmcCmdMs, UINT32);
    RC GetGPUTemperatureThroughBMC(UINT08 *temp);

private:
    RC FindAndInitMLWDevice(bool *bFound);
    RC InitializePciInterface(GpuSubdevice *pSubdev);
    RC ShutdownMLWDevice();
    RC WriteCommand(std::vector<UINT08> *command);
    RC ReadStatus(UINT08 *status);
    RC ReadData(UINT32 *data);
    RC DisablePowerSupply();
    RC EnablePowerSupply();
    RC RemoveGpu();
    RC DiscoverGPU();
    void SaveRootPortLTRState();
    void RestoreRootPortLTRState();
    RC SaveTCVCMapping();
    RC RestoreTCVCMapping();
    RC DisableBMCAndFPGAPolling();
    RC EnableBMCAndFPGAPolling();
    RC GetGPUCapabilities
    (
        UINT08 opcode,
        UINT08 arg1,
        UINT08 arg2,
        UINT32 *capData
    );

    enum RecvDataType
    {
        CMD_STATUS,
        SIGNAL_STATE,
        TEMPERATURE
    };

    RC FindAndInitBMCDevice(bool *bFound);
    RC WriteBMCCommand(UINT08 cmdCode, vector<UINT08> &command);
    RC ReadBMCStatus(vector<UINT08> &command, UINT08 *status);
    RC ReadBMCData(vector<UINT08> &command, UINT08 *data, RecvDataType type);
    RC GetBMCGPUsPowerStatus(vector<UINT08> &command, bool &poweron);
    RC GetSXMI2CAddress(GpuSubdevice *pSubdev);

    GpuDevice *m_pGpuDev = nullptr;
    SmbManager  *m_pSmbMgr = nullptr;
    SmbPort *m_SmbPort = nullptr;
    UINT08 m_SmbAddress = 0;
    PexDevice *m_pPexDev = nullptr;
    UINT32 m_Port = 0;
    UINT32 m_LinkPollTimeoutMs = 500;
    bool m_RootPortLtrState = false;
    UINT32 m_DelayBeforeOOBEntryMs = 0;
    UINT32 m_DelayAfterOOBExitMs = 0;
    UINT32 m_DelayBeforeRescanMs = 0;
    UINT32 m_RescanRetryDelayMs = 100;
    bool m_IstMode = false;
    UINT32 m_DelayAfterPexRstAssertMs = 10;
    UINT32 m_DelayBeforePexDeassertMs = 100;
    UINT32 m_DelayAfterExitZpiMs = 200;
    UINT32 m_DelayAfterPollingMs = 1000;
    UINT32 m_DelayAfterBmcCmdMs = 10;

    bool m_PollingDisabled = false;
    bool m_IsSeleneSystem = false;
    UINT32 m_TCVCMapping;
    UINT08 m_SXMI2CAddress;
    unique_ptr<IpmiDevice> m_pIpmiDevice;
    GpuSubdevice *m_pSubdev = nullptr;

    enum Command
    {
        POWER_ENABLE  = 0,
        POWER_DISABLE = 1,
        PCIE_RESET_ASSERT = 2,
        PCIE_RESET_DEASSERT = 3,
        NUM_COMMANDS = 4
    };

    struct CommandPack
    {
        UINT08 setOpCode;
        UINT08 getOpCode;
        UINT08 action;
        const char *msg;
    };

    RC RunCommand(Command command);
    RC RunBMCCommand(Command command);

    static constexpr UINT08 COMMAND_REGISTER = 0x5C;
    static constexpr UINT08 DATA_REGISTER    = 0x5D;

    static constexpr UINT08 FPGA_ADDRESS    = 0xC0;
    static constexpr UINT08 BMC_NETFN       = 0x30;
    static constexpr UINT08 BMC_CMD         = 0x81;
    static constexpr UINT08 POLL_CMD        = 0x11;
    static constexpr UINT08 IC1_BUS         = 0x0A;
    static constexpr UINT08 TEMP_OPCODE     = 0x02;
    static constexpr UINT08 I2C1_ENTITY     = 0x09;
    static constexpr UINT08 I2C2_ENTITY     = 0x18;
    static constexpr UINT08 I2C1_INSTANCE   = 0xA0;
    static constexpr UINT08 I2C2_INSTANCE   = 0x01;
    static constexpr UINT08 FPGA_BUS        = 0x0B;
    static constexpr UINT08 FPGA_POLL_OPCODE          = 0xBF;

    static constexpr UINT32 LW_XVE_VCCAP_CTRL0_OFFSET = 0x114;

    // Reference: SMBPBI Notebook Software Design Guide DG-06034-002_v03.11.pdf
    static constexpr UINT08 OPCODE_SET_POWER_SUPPLY = 0xF0;
    static constexpr UINT08 OPCODE_GET_POWER_SUPPLY = 0xF1;
    static constexpr UINT08 POWER_SUPPLY_DISABLE    = 0;
    static constexpr UINT08 POWER_SUPPLY_ENABLE     = 1;

    static constexpr UINT08 OPCODE_SET_PCIE_FUNDAMENTAL_RESET = 0xF2;
    static constexpr UINT08 OPCODE_GET_PCIE_FUNDAMENTAL_RESET = 0xF3;
    static constexpr UINT08 PCIE_FUNDAMENTAL_RESET_DEASSERT   = 0;
    static constexpr UINT08 PCIE_FUNDAMENTAL_RESET_ASSERT     = 1;

    static constexpr UINT08 OPCODE_GET_GPU_CAPABILITY = 0x01;

    static constexpr UINT08 STATUS_SUCCESS = 0x1F;
    static constexpr UINT08 NO_OP = 0;
    static constexpr UINT08 STATUS_MASK = 0x1F; // with respect to bits [28:24]

    static constexpr CommandPack m_Commands[NUM_COMMANDS] = {
        { OPCODE_SET_POWER_SUPPLY, OPCODE_GET_POWER_SUPPLY,
            POWER_SUPPLY_ENABLE, "Power supply enable"},
        { OPCODE_SET_POWER_SUPPLY, OPCODE_GET_POWER_SUPPLY,
            POWER_SUPPLY_DISABLE, "Power supply disable"},
        { OPCODE_SET_PCIE_FUNDAMENTAL_RESET, OPCODE_GET_PCIE_FUNDAMENTAL_RESET,
            PCIE_FUNDAMENTAL_RESET_ASSERT, "Pcie reset assert"},
        { OPCODE_SET_PCIE_FUNDAMENTAL_RESET, OPCODE_GET_PCIE_FUNDAMENTAL_RESET,
            PCIE_FUNDAMENTAL_RESET_DEASSERT, "Pcie reset deassert"}
    };

    struct busInfo
    {
        INT32 domain = 0;
        INT32 bus = 0;
        INT32 device = 0;
        INT32 func = 0;
    } m_BusInfo, m_BridgeInfo;
};
