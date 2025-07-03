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

#include "zpi.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "device/interface/pcie.h"
#include "device/smb/smbdev.h"
#include "device/smb/smbmgr.h"
#include "device/smb/smbport.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"

#ifdef INCLUDE_AZALIA
#include "device/azalia/azactrl.h"
#endif
using ResetControls = GpuSubdevice::ResetControls;
namespace
{
    //----------------------------------------------------------------------------
    std::vector<UINT08> MakeCommand(UINT08 opcode, UINT08 arg1, UINT08 arg2)
    {
        return { opcode, arg1, arg2, 0x80 }; // 0x80 = Set Execute bit
    }
}


//----------------------------------------------------------------------------
ZeroPowerIdle::ZeroPowerIdle()
{
}

//! \brief Find and initialize BMC(Baseboard Management Controller) device 
//! This function applied to luna systems. It pings the BMC and checks that all
//! 8 GPUs are present and powered on
//! Luna IPMI command format are described 
//! https://confluence.lwpu.com/display/GPUP/Luna+Useful+IPMI+commands+for+GPU+baseboard+devices+access
// TODO: The BMC init, GPU presence and  power status check is lwrrently done by the DGX
// diag package. This function is lwrrenlty not called. It's left for future alternative for
// doing the check from within MODS (design alternative)
RC ZeroPowerIdle::FindAndInitBMCDevice(bool *bFound)
{
    RC rc; 
    UINT08 status = 0;
    bool gpuPowered = false;

    vector<UINT08> cmdInitBMC = {IC1_BUS, FPGA_ADDRESS, 0, COMMAND_REGISTER, 4, 0xBB, 0, 0, 0x80};
    vector<UINT08> cmdQueryInitBMC = {IC1_BUS, FPGA_ADDRESS, 5, COMMAND_REGISTER};

    m_pIpmiDevice = make_unique<IpmiDevice>();
    CHECK_RC(m_pIpmiDevice->Initialize());
    //Send command to BMC
    CHECK_RC(WriteBMCCommand(BMC_CMD, cmdInitBMC));

    // Query the command status to check the command was successful
    CHECK_RC(ReadBMCData(cmdQueryInitBMC, &status, RecvDataType::CMD_STATUS));
    if (status != STATUS_SUCCESS)
    {
        Printf(Tee::PriError, "Power Status Command to BMC was not successful %x\n", status);
        return RC::UNEXPECTED_RESULT;
    }

    // Get the GPUs power status
    vector<UINT08> cmdGetPowerStatus = {IC1_BUS, FPGA_ADDRESS, 5, DATA_REGISTER};
    GetBMCGPUsPowerStatus(cmdGetPowerStatus, gpuPowered);
    if (!gpuPowered)
    {
        Printf(Tee::PriError, "All 8 GPUs are either not online or not powered on\n");
        return RC::HW_STATUS_ERROR;
    }
    *bFound = true;

    return rc;
}

//! \brief Find and initialize MLW device
//! This function will initialize the Smbus Post Box Interface(SmbPBI) and ping
//! Micro Controller Unit(MLW) required by ZPI to control the power supply.
//!
//! \return OK if successful
//!
//----------------------------------------------------------------------------
RC ZeroPowerIdle::FindAndInitMLWDevice(bool *bFound)
{
    RC rc;

    m_pSmbMgr = SmbMgr::Mgr();
    CHECK_RC(m_pSmbMgr->Find());
    CHECK_RC(m_pSmbMgr->InitializeAll());
    UINT32 numDev = 0;
    CHECK_RC(m_pSmbMgr->GetNumDevices(&numDev));

    for (UINT32 i = 0; i < numDev; ++i)
    {
        SmbDevice *smbDevice = nullptr;
        CHECK_RC(m_pSmbMgr->GetByIndex(i, reinterpret_cast<McpDev**>(&smbDevice)));
        UINT32 numSubDev = 0;
        CHECK_RC(smbDevice->GetNumSubDev(&numSubDev));
        for (UINT32 j = 0; j < numSubDev; ++j)
        {
            CHECK_RC(smbDevice->GetSubDev(j, reinterpret_cast<SmbPort**>(&m_SmbPort)));
            UINT08 status = 0;
            rc = m_SmbPort->RdByteCmd(m_SmbAddress, COMMAND_REGISTER, &status);
            if (rc == RC::OK)
            {
                *bFound = true;
                return rc;
            }
            rc.Clear();
        }
    }
    return rc;
}

//! \brief Get the GPU capabilities through the SMBus as specified in the SMBPBI doc.
//! This function can be used to verify that communication between MLW and GPU 
//! is wired properly. As this time, we do not use it yet.
//! 
RC ZeroPowerIdle::GetGPUCapabilities
(
    UINT08 opcode,
    UINT08 arg1,
    UINT08 arg2,
    UINT32 *capData
)
{
    RC rc;
    constexpr UINT32 numRetries = 15;
    auto capCmd = MakeCommand(opcode, arg1, arg2);
    UINT08 status = 0;
    UINT32 data = 0;
    constexpr UINT32 capabilityDelayMs = 1000;

    for (UINT32 retry = 0; retry < numRetries; retry++)
    {
         CHECK_RC(WriteCommand(&capCmd));
         status = 0;
         CHECK_RC(ReadStatus(&status));
         if (status != STATUS_SUCCESS)
         {
             Printf(Tee::PriDebug, "Capabilities Opcode 0x%x Arg1 0x%x failed at try %u, "
                     "Status 0x%x\n",
                     opcode, arg1, retry, status);
             // The SMBus can be flaky at times, so we wait 10ms before trying again.
             Platform::Delay(capabilityDelayMs * 10);
             continue;
         }

         data = 0;
         CHECK_RC(ReadData(&data));
         if (data != 0)
         {
             // A succesfully read, break
             Printf(Tee::PriDebug, "Capabilities Opcode 0x%x Arg1 0x%x passed, Data 0x%x\n",
                     opcode, arg1, data);
             *capData = data;
             break;
         }
         Platform::Delay(capabilityDelayMs * 1000);
         Printf(Tee::PriDebug, "Capabilities Opcode 0x%x Arg1 0x%x failed at try %u, "
                 "Data 0x%x\n", opcode, arg1, retry, data);
    }
    if (status != STATUS_SUCCESS || data == 0)
    {
        Printf(Tee::PriDebug, "Capabilities Opcode 0x%x Arg1 0x%x failed\n",
                opcode, arg1);
        return RC::HW_STATUS_ERROR;
    }

    return rc;
}

//----------------------------------------------------------------------------
bool ZeroPowerIdle::IsSupported(const GpuSubdevice *pSubdev)
{
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    if (pGpuPcie == nullptr)
    {
        Printf(Tee::PriLow, "PCIe interface not found\n");
        return false;
    }

    StickyRC rc;
    bool found = false;

    m_SmbAddress = pSubdev->GetSmbusAddress();
    
    rc = FindAndInitMLWDevice(&found);

    // To ensure we don't leave it initialized if MLW device is not found
    rc = ShutdownMLWDevice();
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Failed to communicate with SMBus device\n");
        return true; // To avoid silent test skip and fail in setup/cleanup
    }

    return found;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::Setup(GpuSubdevice *pSubdev)
{
    RC rc;
    bool found = false;
    
    m_SmbAddress = pSubdev->GetSmbusAddress();

    if (m_IsSeleneSystem)
    {
        Printf(Tee::PriDebug, "ZPI on Selene systems\n");
        m_pIpmiDevice = make_unique<IpmiDevice>();
        CHECK_RC(m_pIpmiDevice->Initialize());
        CHECK_RC(GetSXMI2CAddress(pSubdev));
        found = true;
    }
    else
    {
        Printf(Tee::PriDebug, "ZPI on Regular systems\n");
        CHECK_RC(FindAndInitMLWDevice(&found));
    }

    if (!found)
    {
        Printf(Tee::PriError, "Smbus device(MLW) not found\n");
        return RC::DEVICE_NOT_FOUND;
    }

    m_pGpuDev = pSubdev->GetParentDevice();
    CHECK_RC(InitializePciInterface(pSubdev));

    return rc;
}

//----------------------------------------------------------------------
RC ZeroPowerIdle::Setup
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT08 smbAddress,
    PexDevice* pPexDev,
    UINT32 port
)
{
    RC rc;
    
    m_SmbAddress = smbAddress;
    
    m_pPexDev = pPexDev;
    m_Port = port;
    
    m_BusInfo.domain = domain;
    m_BusInfo.bus = bus;
    m_BusInfo.device = device;
    m_BusInfo.func = function;

    m_BridgeInfo.domain = m_pPexDev->GetDownStreamPort(m_Port)->Domain;
    m_BridgeInfo.bus = m_pPexDev->GetDownStreamPort(m_Port)->Bus;
    m_BridgeInfo.device = m_pPexDev->GetDownStreamPort(m_Port)->Dev;
    m_BridgeInfo.func = m_pPexDev->GetDownStreamPort(m_Port)->Func;
    
    bool found = false;
    CHECK_RC(FindAndInitMLWDevice(&found));
    
    if (!found)
    {
        Printf(Tee::PriError, "Smbus device(MLW) not found\n");
        return RC::DEVICE_NOT_FOUND;
    }
    
    return rc;
}

//----------------------------------------------------------------------
RC ZeroPowerIdle::GetSXMI2CAddress(GpuSubdevice *pSubdev)
{
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    if (pGpuPcie == nullptr)
    {   
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    switch (pGpuPcie->BusNumber())
    {
        case 0x47:
        case 0x27:
            m_SXMI2CAddress = 0x98; // SXM1
            break;
        case 0xBD:
        case 0xAD:
            m_SXMI2CAddress = 0x9A; // SXM2
            break;
        case 0xB7:
        case 0xA7:
            m_SXMI2CAddress = 0x9C; // SXM3
            break;
        case 0x4E:
        case 0x2E:
            m_SXMI2CAddress = 0x9E; // SXM4
            break;
        case 0x07:
            m_SXMI2CAddress = 0x88; // SXM5
            break;
        case 0x90:
            m_SXMI2CAddress = 0x8A; // SXM6
            break;
        case 0x87:
            m_SXMI2CAddress = 0x8C; // SXM7
            break;
        case 0x0F:
            m_SXMI2CAddress = 0x8E; // SXM8
            break;
        default:
            Printf(Tee::PriError, "Could not associate GPU bus %x with an IPMI address\n",
                    m_BusInfo.bus);
            return RC::HW_STATUS_ERROR;
    }

    Printf(Tee::PriDebug, "I2C Address of GPU on Bus %02x is %02x\n", pGpuPcie->BusNumber(),
                            m_SXMI2CAddress);

    return RC::OK;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::InitializePciInterface(GpuSubdevice *pSubdev)
{
    RC rc;
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    if (pGpuPcie == nullptr)
    {
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    m_pSubdev = pSubdev;
    CHECK_RC(pGpuPcie->GetUpStreamInfo(&m_pPexDev, &m_Port));
    MASSERT(m_pPexDev);

    m_BusInfo.domain = pGpuPcie->DomainNumber();
    m_BusInfo.bus = pGpuPcie->BusNumber();
    m_BusInfo.device = pGpuPcie->DeviceNumber();
    m_BusInfo.func = pGpuPcie->FunctionNumber();

    m_BridgeInfo.domain = m_pPexDev->GetDownStreamPort(m_Port)->Domain;
    m_BridgeInfo.bus = m_pPexDev->GetDownStreamPort(m_Port)->Bus;
    m_BridgeInfo.device = m_pPexDev->GetDownStreamPort(m_Port)->Dev;
    m_BridgeInfo.func = m_pPexDev->GetDownStreamPort(m_Port)->Func;

    return rc;
}

//----------------------------------------------------------------------------
//! \brief This function reinitializes pex device object which might have
//! changed in a scenario like IST which re-initializes GpuSubdevice
//!
RC ZeroPowerIdle::ReInitializePciInterface(GpuSubdevice *pSubdev)
{
    return InitializePciInterface(pSubdev);
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::Cleanup()
{
    RC rc;
    CHECK_RC(ShutdownMLWDevice());
    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::ShutdownMLWDevice()
{
    StickyRC rc;

    if (m_pSmbMgr)
    {
        rc = m_pSmbMgr->Uninitialize();
        rc = m_pSmbMgr->Forget();
        m_pSmbMgr = nullptr;
    }
    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::EnterZPI(UINT32 enterDelayUsec)
{
    RC rc;

    auto *pGpuDevMgr = DevMgrMgr::d_GraphDevMgr;
    bool enterZpiSucceded = false;

    if (m_IsSeleneSystem)
    {
        CHECK_RC(DisableBMCAndFPGAPolling());
    }

    DEFER
    {
        if (!enterZpiSucceded && m_IsSeleneSystem)
        {   
            EnableBMCAndFPGAPolling();
        }
    };

    if (!pGpuDevMgr || !m_pGpuDev)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (!m_IstMode)
    {
        // Acquire the mutex of OneHz thread so as to pause it (and stop its interaction
        // with GPU).
        Tasker::AcquireMutex(GpuPtr()->GetOneHzMutex());
        pGpuDevMgr->ShutdownDrivers();

        // It lwrrently works on a single-GPU setup
        // Inband actions will probably need to be refactored further for
        // multi-GPU case because below interrupts handling API's work on all GPU's.
        // TODO: Unload the driver and interrupt handling for a given subdevice
        CHECK_RC(m_pGpuDev->RecoverFromResetShutdown(ResetControls::DoZpiReset));
        pGpuDevMgr->UnhookInterrupts();
    }

    CHECK_RC(RemoveGpu());
    Platform::Delay(m_DelayBeforeOOBEntryMs * 1000);
    CHECK_RC(DisablePowerSupply());
    Platform::Delay(enterDelayUsec);
    // EnterZPI went ok, prevent the re-enable of polling by DEFER on Selene system
    enterZpiSucceded = true;

    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::ExitZPI()
{
    RC rc;

    DEFER
    {   
        if (m_IsSeleneSystem)
        {
            EnableBMCAndFPGAPolling();
        }
    };

    CHECK_RC(EnablePowerSupply());
    Platform::Delay(m_DelayAfterOOBExitMs * 1000);
    CHECK_RC(DiscoverGPU());

    auto *pGpuDevMgr = DevMgrMgr::d_GraphDevMgr;

    // There may be additional functions required for other device initializations
    CHECK_RC(pGpuDevMgr->FindDevicesClientSideResman(GpuDevMgr::SubdevDiscoveryMode::Reassign));

    if (!m_IstMode && m_pGpuDev)
    {
        CHECK_RC(GpuPtr()->ValidateAndHookIsr());
        CHECK_RC(m_pGpuDev->RecoverFromResetInit(ResetControls::DoZpiReset));

        // Release the mutex of OneHz thread so as to resume it
        Tasker::ReleaseMutex(GpuPtr()->GetOneHzMutex());
    }

    Platform::Delay(m_DelayAfterExitZpiMs * 1000);

    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::ReadStatus(UINT08 *status)
{
    RC rc;
    UINT32 retry = 0;
    *status = 0;
    std::vector<UINT08> data(4, 0);

    do
    {
        rc.Clear();
        rc = m_SmbPort->RdBlkCmd(m_SmbAddress, COMMAND_REGISTER, &data);

        if (rc != RC::OK)
        {
            Printf(Tee::PriDebug, "Failed read command, retry = %u\n", retry);
            retry++;
            Tasker::Sleep(1);
        }
    } while (rc != RC::OK && retry < 10);

    if (rc == RC::OK)
    {
        // As per SmbPBI reference bit 28:24 refers to status in
        // COMMAND_REGISTER which fall in data[3] of our vector
        *status = data[3] & STATUS_MASK;
    }

    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::WriteCommand(std::vector<UINT08> *command)
{
    RC rc;
    UINT32 retry = 0;

    do
    {
        rc.Clear();
        rc = m_SmbPort->WrBlkCmd(m_SmbAddress, COMMAND_REGISTER, command);
        if (rc != RC::OK)
        {
            Printf(Tee::PriDebug, "Failed write command, retry = %u\n", retry);
            retry++;
            Tasker::Sleep(1);
        }
    } while (rc != RC::OK && retry < 10);

    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::ReadData(UINT32 *data)
{
    RC rc;
    UINT32 retry = 0;
    std::vector<UINT08> cmd(4, 0);

    do
    {
        rc.Clear();
        rc = m_SmbPort->RdBlkCmd(m_SmbAddress, DATA_REGISTER, &cmd);
        if (rc != RC::OK)
        {
            Printf(Tee::PriDebug, "Failed read data command, retry = %u\n", retry);
            retry++;
            Tasker::Sleep(1);
        }
    } while (rc != RC::OK && retry < 10);

    if (rc == RC::OK)
    {
        *data |= cmd[0];
        *data |= (cmd[1] << 8);
        *data |= (cmd[2] << 16);
        *data |= (cmd[3] << 24);
    }

    return rc;
}

constexpr ZeroPowerIdle::CommandPack ZeroPowerIdle::m_Commands[];

//---------------------------------------------------------------------------
RC ZeroPowerIdle::WriteBMCCommand(UINT08 cmdCode, vector<UINT08> &command)
{
    RC rc; 
    UINT32 retry = 0;
    const UINT32  maxRetries = 20;
    vector<UINT08> recvData;

    do
    {
        rc = m_pIpmiDevice->RawAccess(BMC_NETFN, cmdCode, command, recvData);
        if (rc != RC::OK)
        {
            // The command failed. Let's try again, no need to query command 
            Printf(Tee::PriDebug, "IPMI write command failed, retry = %u\n", retry);
            retry++;
        }
        Tasker::Sleep(m_DelayAfterBmcCmdMs); // IPMI can be unreliable, delay between commands
    } while (rc != RC::OK && retry < maxRetries);

    return rc;
}

//--------------------------------------------------------------------------
RC ZeroPowerIdle::ReadBMCData(vector<UINT08> &command, UINT08 *data, RecvDataType type)
{
    RC rc; 
    UINT32 retry = 0;
    const UINT32 maxRetries = 20;
    vector<UINT08> recvData;

    do
    {
        rc = m_pIpmiDevice->RawAccess(BMC_NETFN, BMC_CMD, command, recvData);
        if (rc != RC::OK || recvData.size() != 6)
        {
            Printf(Tee::PriDebug, "IPMI Data read failed, retry = %u\n", retry);
            retry++;
        }
        Tasker::Sleep(m_DelayAfterBmcCmdMs); // IPMI can be unreliable, delay between commands
    } while ((rc != RC::OK || recvData.size() != 6) && retry < maxRetries);

    if (rc == RC::OK)
    {   
        switch (type)
        {
            case RecvDataType::CMD_STATUS:
                *data = recvData[5] & STATUS_MASK;
                break;
            case RecvDataType::SIGNAL_STATE:
                *data = recvData[2];
                break;
            case RecvDataType::TEMPERATURE:
                *data = recvData[3];
                break;
            default:
                Printf(Tee::PriError, "Inavlid BMC Data type specified\n");
                return RC::BAD_PARAMETER;
        }
    }

    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::GetBMCGPUsPowerStatus(vector<UINT08> &command, bool &poweron)
{
    RC rc; 
    UINT32 retry = 0;
    const UINT32 maxRetries = 20;
    vector<UINT08> recvData;

    do
    {
        rc = m_pIpmiDevice->RawAccess(BMC_NETFN, BMC_CMD, command, recvData);
        if (rc != RC::OK)
        {
            Printf(Tee::PriDebug, "IPMI power query status failed, retry = %u\n", retry);
            retry++;
        }
        Tasker::Sleep(m_DelayAfterBmcCmdMs); // IPMI can be unreliable, delay between commands
    } while (rc != RC::OK && retry < maxRetries);

    if (rc == RC::OK)
    {
        poweron = (recvData[2] == 0xff && recvData[3] == 0xff);
    }

    return rc;
}

//---------------------------------------------------------------------------
RC ZeroPowerIdle::DisableBMCAndFPGAPolling()
{
    RC rc;
    vector<UINT08> cmdDisablePolling = {I2C1_ENTITY, I2C1_INSTANCE, 0x00};
    CHECK_RC(WriteBMCCommand(POLL_CMD, cmdDisablePolling));

    cmdDisablePolling = {I2C2_ENTITY, I2C2_INSTANCE, 0x00};
    CHECK_RC(WriteBMCCommand(POLL_CMD, cmdDisablePolling));

    cmdDisablePolling = {FPGA_BUS, FPGA_ADDRESS, 0, COMMAND_REGISTER, 4, FPGA_POLL_OPCODE,
                        0x0, NO_OP, 0x80};
    CHECK_RC(WriteBMCCommand(BMC_CMD, cmdDisablePolling));
    m_PollingDisabled = true;

    // Disabling FPGA and BMC polling takes a bit of time before being effective.
    // So we wait for 2s before proceeding with sending commands to the BMC.
    Tasker::Sleep(m_DelayAfterPollingMs);

    return rc;
}

//------------------------------------------------------------------
RC ZeroPowerIdle::EnableBMCAndFPGAPolling()
{
    RC rc;

    if (m_PollingDisabled)
    {
        vector<UINT08> cmdEnablePolling = {I2C1_ENTITY, I2C1_INSTANCE, 0x01};
        CHECK_RC(WriteBMCCommand(POLL_CMD, cmdEnablePolling));

        cmdEnablePolling = {I2C2_ENTITY, I2C2_INSTANCE, 0x01};
        CHECK_RC(WriteBMCCommand(POLL_CMD, cmdEnablePolling));

        cmdEnablePolling = {FPGA_BUS, FPGA_ADDRESS, 0, COMMAND_REGISTER, 4, FPGA_POLL_OPCODE,
                                0x1, NO_OP, 0x80};
        CHECK_RC(WriteBMCCommand(BMC_CMD, cmdEnablePolling));
        m_PollingDisabled = false;
        Tasker::Sleep(m_DelayAfterPollingMs); // Wait for the polling command to take  effect
    }

    return rc;
}

//-----------------------------------------------------------------
RC  ZeroPowerIdle::GetGPUTemperatureThroughBMC(UINT08 *temperature)
{
    RC rc;
    const UINT32 maxRetries = 20;
    UINT08 status = 0;
    vector<UINT08> cmdGetTemperature = {IC1_BUS, m_SXMI2CAddress, 0, COMMAND_REGISTER, 4,
                                        TEMP_OPCODE, NO_OP, NO_OP, 0x80};
    vector<UINT08> cmdQueryTemperature = {IC1_BUS, m_SXMI2CAddress, 5, COMMAND_REGISTER};
    Printf(Tee::PriDebug, "In GetGPUTemperatureThroughBMC\n");

    CHECK_RC(DisableBMCAndFPGAPolling());
    DEFER{EnableBMCAndFPGAPolling();};

    for (UINT32 retry = 0; retry < maxRetries; retry++)
    {
        CHECK_RC(WriteBMCCommand(BMC_CMD, cmdGetTemperature));

        status = 0;
        CHECK_RC(ReadBMCData(cmdQueryTemperature, &status, RecvDataType::CMD_STATUS));
        if (status == STATUS_SUCCESS)
        {   
            break;
        }
    }

    vector<UINT08> cmdReadTemperature = {IC1_BUS, m_SXMI2CAddress, 5, DATA_REGISTER};
    CHECK_RC(ReadBMCData(cmdReadTemperature, temperature, RecvDataType::TEMPERATURE));
    if (status != STATUS_SUCCESS)
    {
        return RC::HW_STATUS_ERROR;
    }

    return rc;
}

//---------------------------------------------------------------
RC ZeroPowerIdle::RunBMCCommand(Command command)
{
    RC rc;
    const UINT32 maxRetries = 20;
    UINT08 status = 0;
    UINT08 state = 0;
    const CommandPack &commandPack = m_Commands[command];

    Printf(Tee::PriDebug, "In RunBMCCommand cmd %d\n", command);

    vector<UINT08> sendCmd = {IC1_BUS, m_SXMI2CAddress, 0, COMMAND_REGISTER, 4, 0, 0, NO_OP, 0x80};
    vector<UINT08> sendQuery = {IC1_BUS, m_SXMI2CAddress, 5, 0x0};

    for (UINT32 retry = 0; retry < maxRetries; retry++)
    {
        sendCmd[5] = commandPack.setOpCode;
        sendCmd[6] = commandPack.action;
        CHECK_RC(WriteBMCCommand(BMC_CMD, sendCmd));

        status = 0;
        sendQuery[3] = COMMAND_REGISTER;
        CHECK_RC(ReadBMCData(sendQuery, &status, RecvDataType::CMD_STATUS));
        if (status != STATUS_SUCCESS)
        {
            // The command failed, next try
            Printf(Tee::PriDebug, "%s cmd status failed\n", commandPack.msg);
            continue;
        }

        sendCmd[5] = commandPack.getOpCode;
        sendCmd[6] = NO_OP;
        CHECK_RC(WriteBMCCommand(BMC_CMD, sendCmd));

        status = 0;
        CHECK_RC(ReadBMCData(sendQuery, &status, RecvDataType::CMD_STATUS));
        if (status != STATUS_SUCCESS)
        {
            // The command failed let's try again
            Printf(Tee::PriDebug, "%s query status failed\n", commandPack.msg);
            continue;
        }

        state = 0;
        sendQuery[3] = DATA_REGISTER;
        CHECK_RC(ReadBMCData(sendQuery, &state, RecvDataType::SIGNAL_STATE));
        if (state == commandPack.action)
        {
            Printf(Tee::PriDebug, "%s succeeded\n", commandPack.msg);
            break;
        }
    }

    if (status != STATUS_SUCCESS || state != commandPack.action)
    {
        return RC::HW_STATUS_ERROR;
    }

    Printf(Tee::PriDebug, "Out RunBMCCommand\n");
    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::RunCommand(Command command)
{
    RC rc;
    constexpr UINT32 numRetries = 10;
    const CommandPack &commandPack = m_Commands[command];
    UINT08 status = 0;
    UINT32 data = 0;

    for (UINT32 retry = 0; retry < numRetries; ++retry)
    {
        auto setCmd = MakeCommand(commandPack.setOpCode, commandPack.action, NO_OP);
        CHECK_RC(WriteCommand(&setCmd));
        status = 0;
        CHECK_RC(ReadStatus(&status));
        if (status != STATUS_SUCCESS)
        {
            // Command failed, next try
            Printf(Tee::PriDebug, "%s Opcode 0x%x failed at try %u, Status 0x%x\n",
                    commandPack.msg, commandPack.setOpCode, retry, status);
            continue;
        }

        auto getCmd = MakeCommand(commandPack.getOpCode, NO_OP, NO_OP);
        CHECK_RC(WriteCommand(&getCmd));
        status = 0;
        CHECK_RC(ReadStatus(&status));
        if (status != STATUS_SUCCESS)
        {
            Printf(Tee::PriDebug, "%s Opcode 0x%x failed at try %u, Status 0x%x\n",
                    commandPack.msg, commandPack.getOpCode, retry, status);
            continue;
        }

        data = 0;
        CHECK_RC(ReadData(&data));
        if (data == commandPack.action)
        {
            Printf(Tee::PriLow, "%s successful\n", commandPack.msg);
            break;
        }
        Printf(Tee::PriDebug, "%s Opcode 0x%x failed at try %u, Data 0x%x\n",
                commandPack.msg, commandPack.getOpCode, retry, data);
    }
    if (status != STATUS_SUCCESS || data != commandPack.action)
    {
        Printf(Tee::PriError, "%s failed\n", commandPack.msg);
        return RC::HW_STATUS_ERROR;
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Perform Out Of Band(OOB) entry action
//! This function cut the power to GPU and perform PCIE reset assert
//!
//! \return OK if successful
//!
RC ZeroPowerIdle::DisablePowerSupply()
{
    RC rc;
    Printf(Tee::PriLow, "OOB entry start\n");
    if (m_IsSeleneSystem)
    {
        CHECK_RC(RunBMCCommand(Command::PCIE_RESET_ASSERT));
        Platform::Delay(m_DelayAfterPexRstAssertMs * 1000);
        CHECK_RC(RunBMCCommand(Command::POWER_DISABLE));
    }
    else
    {
        CHECK_RC(RunCommand(Command::PCIE_RESET_ASSERT));
        // Wait for PEX_RST to be completed in order to proceed for power rails shutdown
        Platform::Delay(m_DelayAfterPexRstAssertMs * 1000);
        CHECK_RC(RunCommand(Command::POWER_DISABLE));
    }
    Printf(Tee::PriLow, "OOB entry finished\n");

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Perform Out Of Band(OOB) exit action
//! This function bring back the power to GPU and perform PCIE reset de-assert
//!
//! \return OK if successful
//!
RC ZeroPowerIdle::EnablePowerSupply()
{
    RC rc;
    Printf(Tee::PriLow, "OOB exit start\n");
    if (m_IsSeleneSystem)
    {
        CHECK_RC(RunBMCCommand(Command::POWER_ENABLE));
        Platform::Delay(m_DelayBeforePexDeassertMs * 1000);
        CHECK_RC(RunBMCCommand(Command::PCIE_RESET_DEASSERT));
    }
    else
    {
        CHECK_RC(RunCommand(Command::POWER_ENABLE));
        // Wait for all power rails to be up
        Platform::Delay(m_DelayBeforePexDeassertMs * 1000);
        CHECK_RC(RunCommand(Command::PCIE_RESET_DEASSERT));
    }
    Printf(Tee::PriLow, "OOB exit finished\n");

    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::RemoveGpu()
{
    RC rc;

    CHECK_RC(SaveTCVCMapping());

    // The OS tends to turn the bridge to lower power mode (D3Hot)
    // making the config space registers inaccessible. This lead to a failure
    // when the bus rescan fails. We force the bridge to be always on (D0)
    string powerControlPath = Utility::StrPrintf(
            "/sys/bus/pci/devices/%04x:%02x:%02x.%1u/power/control", 
            m_BridgeInfo.domain, m_BridgeInfo.bus, m_BridgeInfo.device, m_BridgeInfo.func);
    if (Xp::DoesFileExist(powerControlPath) && 
            Xp::CheckWritePerm(powerControlPath.c_str()) == RC::OK)
    {
        rc = Xp::InteractiveFileWrite(powerControlPath.c_str(), "on");
        if (rc != RC::OK)
        {
            // Not the end of the world, print a warning msg and move on
            Printf(Tee::PriWarn, "Write to Bus power control failed\n");
            rc.Clear();
        }
    }

#ifdef INCLUDE_AZALIA
    AzaliaController* pAzalia = m_pSubdev->GetAzaliaController();
    // TODO: It will not work with -skip_azalia_init, find a better way
    if (pAzalia)
    {
        SysUtil::PciInfo azaPciInfo = {};
        CHECK_RC(pAzalia->GetPciInfo(&azaPciInfo));
        CHECK_RC(Platform::PciRemove(azaPciInfo.DomainNumber, azaPciInfo.BusNumber,
                    azaPciInfo.DeviceNumber, azaPciInfo.FunctionNumber));
    }
#endif

    CHECK_RC(Platform::PciRemove(m_BusInfo.domain,
        m_BusInfo.bus, m_BusInfo.device, m_BusInfo.func));

    SaveRootPortLTRState();
    CHECK_RC(m_pPexDev->DisableDownstreamPort(m_Port));
    CHECK_RC(m_pPexDev->PollDownstreamPortLinkStatus(
        m_Port, m_LinkPollTimeoutMs, PexDevice::LINK_DISABLE));

    return rc;
}

//----------------------------------------------------------------------------
RC ZeroPowerIdle::DiscoverGPU()
{
    RC rc;

    CHECK_RC(m_pPexDev->EnableDownstreamPort(m_Port));
    CHECK_RC(m_pPexDev->PollDownstreamPortLinkStatus(
        m_Port, m_LinkPollTimeoutMs, PexDevice::LINK_ENABLE));
    RestoreRootPortLTRState();

    Platform::Delay(m_DelayBeforeRescanMs * 1000);

    // Rescan PCI bus
    {
        StickyRC rescanRC;
        constexpr UINT32 numRetries = 9;
        for (UINT32 retry = 0; retry < numRetries; ++retry)
        {
            rescanRC = Platform::RescanPciBus(m_BridgeInfo.domain, m_BridgeInfo.bus);
            if (rescanRC == RC::PCI_FUNCTION_NOT_SUPPORTED)
            {
                rescanRC.Clear();
                rescanRC = Xp::InteractiveFileWrite(Utility::StrPrintf(
                    "/sys/bus/pci/devices/%04x:%02x:%02x.%1u/rescan",
                    m_BridgeInfo.domain, m_BridgeInfo.bus, m_BridgeInfo.device,
                    m_BridgeInfo.func).c_str(), 1);
            }

            // Read vendorId to confirm if GPU is back on PCI bus
            UINT16 vendorId = 0;
            rescanRC = Platform::PciRead16(m_BusInfo.domain, m_BusInfo.bus,
                m_BusInfo.device, m_BusInfo.func, 0, &vendorId);

            if (rescanRC != RC::PCI_DEVICE_NOT_FOUND)
                break;

            if (retry != numRetries - 1)
            {
                Printf(Tee::PriWarn, "PCI rescan %u failed, retrying in %u ms\n",
                    retry + 1, m_RescanRetryDelayMs);
                Platform::Delay(m_RescanRetryDelayMs * 1000);
                rescanRC.Clear();
            }
        }

        if (rescanRC == RC::PCI_DEVICE_NOT_FOUND)
        {
            Printf(Tee::PriError, "PCI rescan failed unable to detect GPU device\n");
        }
        CHECK_RC(rescanRC);
    }
    CHECK_RC(RestoreTCVCMapping());

    return rc;
}

//----------------------------------------------------------------------------
void ZeroPowerIdle::SaveRootPortLTRState()
{
    RC rc;
    rc = m_pPexDev->GetDownstreamPortLTR(m_Port, &m_RootPortLtrState);
    if (rc != RC::OK && rc != RC::UNSUPPORTED_HARDWARE_FEATURE)
    {
        Printf(Tee::PriWarn,
            "Get downstream port LTR mechanism failed! Continuing anyway.\n");
    }
}

//----------------------------------------------------------------------------
void ZeroPowerIdle::RestoreRootPortLTRState()
{
    RC rc;
    rc = m_pPexDev->SetDownstreamPortLTR(m_Port, m_RootPortLtrState);
    if (rc != RC::OK && rc != RC::UNSUPPORTED_HARDWARE_FEATURE)
    {
        Printf(Tee::PriWarn,
            "Set downstream port LTR mechanism failed! Continuing anyway.\n");
    }
}

RC ZeroPowerIdle::SaveTCVCMapping()
{
    RC rc;
    CHECK_RC(Platform::PciRead32(m_BusInfo.domain, m_BusInfo.bus, m_BusInfo.device,
                m_BusInfo.func, LW_XVE_VCCAP_CTRL0_OFFSET, &m_TCVCMapping));

    return rc;
}

RC  ZeroPowerIdle::RestoreTCVCMapping()
{
    RC rc;
    CHECK_RC(Platform::PciWrite32(m_BusInfo.domain, m_BusInfo.bus,  m_BusInfo.device,
                m_BusInfo.func, LW_XVE_VCCAP_CTRL0_OFFSET, m_TCVCMapping));

    return rc;
}
//----------------------------------------------------------------------------
RC ZeroPowerIdle::IsGpuOnTheBus(GpuSubdevice *pSubdev, bool *pGpuOnTheBus)
{
    RC rc;
    MASSERT(pSubdev && pGpuOnTheBus);
    *pGpuOnTheBus = false;
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();
    if (pGpuPcie == nullptr)
    {
        return rc;
    }
    PexDevice* pPexDev = nullptr;
    UINT32 port;
    CHECK_RC(pGpuPcie->GetUpStreamInfo(&pPexDev, &port));
    if (!pPexDev)
    {
        Printf(Tee::PriDebug, "GPU not exists on the PCI Bus anymore.\n");
        return rc;
    }
    *pGpuOnTheBus = true;
    return rc;
}
