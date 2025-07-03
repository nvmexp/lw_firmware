/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "smbec.h"

#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "device/interface/pcie.h"
#include "gpu/gc6/ec_info.h"
#include "gpu/utility/pcie/pexdev.h"
#include "jt.h"
#include "maxwell/gm107/dev_i2cs.h"
#include "maxwell/gm107/dev_therm.h"
#include "lw_ref.h"
#include "lwmisc.h" // DRF_DEF()

SmbEC::SmbEC() :
    m_SmbAddr(EC_SMBUS_ADDR),
    m_SmbusInit(false),
    m_pSmbPort(nullptr),
    m_pSmbMgr(nullptr),
    m_pSubdev(nullptr),
    m_Caps(0),
    m_RootPortLtrState(false),
    m_ECFsmMode(0),
    m_ECWakeupEvent(EC_WAKEUP_GPUEVENT),
    m_ThermAddr(0)

{
    // The uController has 2KB register space, but we only need to access the
    // 1st 0x50 bytes for now.
    m_Regs.resize(0x50);
    m_Opts.enterCmdToStatDelayMs = 0x00;
    m_Opts.exitCmdToStatDelayMs = 0x00;
    // How long to delay after the link status is correct.
    m_Opts.delayAfterLinkStatusMs = 1;
    // How long to poll on the PEX link status for it to change.
    m_Opts.pollLinkStatusMs = 500;
    // Default verbose print priority
    m_Opts.verbosePrint = Tee::PriLow;
    m_Opts.delayAfterL2EntryMs = 10;
    m_Opts.delayAfterL2ExitMs = 10;
}

//------------------------------------------------------------------------------
// Initialize the 1st SmbEC device found on any of the SMBUS ports.
RC SmbEC::InitializeSmbusDevice(GpuSubdevice *pSubdev)
{
    RC rc = OK;
    SmbDevice   *pSmbDev;
    m_pSubdev = pSubdev;

    if (m_SmbusInit)
    {
        Printf(Tee::PriLow,"Smbus already initialized\n");
        return rc;
    }

    PexDevice *pPexDev = NULL;
    UINT32 port;
    CHECK_RC(m_pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &port));
    //We need the upstream device so we can poll on link status, etc.
    if (!pPexDev)
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    m_IsBridgeDevicePresent = ((pPexDev == nullptr) || pPexDev->IsRoot()) ? false : true;

    m_ThermAddr = DRF_NUM(_THERM, _I2CS_STATE, _VALUE_SLAVE_ADR,
                          m_pSubdev->RegRd32(LW_THERM_I2CS_STATE));

    m_pSmbMgr = SmbMgr::Mgr();
    CHECK_RC(m_pSmbMgr->Find());
    CHECK_RC(m_pSmbMgr->InitializeAll());
    CHECK_RC(m_pSmbMgr->GetLwrrent((McpDev**)&pSmbDev));
    m_SmbusInit = true;

    // Search for SMBus EC on each port
    UINT32 numSubdev = 0;
    pSmbDev->GetNumSubDev(&numSubdev);
    for (UINT32 portNumber = 0; portNumber < numSubdev; portNumber++)
    {
        //Calling GetSubDev with portNumber since the ports are stored in order
        CHECK_RC(pSmbDev->GetSubDev(portNumber, &m_pSmbPort));

        // Read the data register of the target address and check return code
        // for errors. If no errors are reported, then a device exists at the
        // target address.
        UINT08 tmp;
        rc = m_pSmbPort->RdByteCmd(m_SmbAddr, EC_DATA_REG, &tmp);
        if (OK == rc)
        {
            Printf(Tee::PriLow,"Found device on Port %i\n", portNumber);
            return(InitializeSmbEC());
        }
        else
        {
            rc.Clear();
        }
    }

    Printf(Tee::PriLow,"EC Device not found on Smbus\n");
    return RC::DEVICE_NOT_FOUND;
}

//------------------------------------------------------------------------------
// Decode and print the EC firmware header.
void SmbEC::PrintFirmwareInfo(Tee::Priority pri)
{
    Printf(pri,"Raw EC Firmware Info:\n");
    for(unsigned i = 0; i < m_Regs.size(); )
    {
        Printf(pri,"0x%02x: ",i);
        for( unsigned j = 0; j < 8 && i < m_Regs.size(); j++, i++)
        {
            Printf(pri,"0x%02x ",m_Regs[i]);
        }
        Printf(pri,"\n");
    }

    // TODO: We may want to change this back to use pri once all of the qual
    //       work is complete. For not its critical that everyone know what
    //       version of EC firmware they are testing.
    Printf(Tee::PriNormal,"EC HW Vendor   : %d\n",m_Regs[0]);
    Printf(Tee::PriNormal,"EC HW Family   : %d\n",m_Regs[1]);
    Printf(Tee::PriNormal,"EC HW Device   : 0x%x%x\n",m_Regs[3],m_Regs[2]);
    Printf(Tee::PriNormal,"EC FW Project  : %c%c%c%c\n",
        m_Regs[44],m_Regs[45],m_Regs[46],m_Regs[47]);
    Printf(Tee::PriNormal,"EC FW Vendor   : 0x%x%x\n",m_Regs[17],m_Regs[16]);
    Printf(Tee::PriNormal,"EC FW Version  : %d.%d\n",m_Regs[21],m_Regs[20]);
    Printf(Tee::PriNormal,"EC FW Date     : %11s %c%c%c%c%c%c%c%c\n",
        (char*)&m_Regs[24],m_Regs[36],m_Regs[37],m_Regs[38],m_Regs[39],
        m_Regs[40],m_Regs[41],m_Regs[42],m_Regs[43]);
    Printf(Tee::PriNormal,"EC Capabilities: %s \n", GetCapString());
    Printf(Tee::PriNormal,"EC FSM Mode    : %s \n",GetFsmModeString());
    return;
}

const char * SmbEC::GetCapString()
{
    static string caps;
    caps = "GC6=";
    caps += FLD_TEST_DRF(_EC_INFO,_CAP_0,_GC6,_TRUE,m_Caps) ? "true" : "false";
    caps += ",PWEN=";
    caps += FLD_TEST_DRF(_EC_INFO,_CAP_0,_GC6_PWEN,_TRUE,m_Caps) ? "true" : "false";
    caps += ",GC6M=";
    caps += FLD_TEST_DRF(_EC_INFO,_CAP_0,_GC6M,_TRUE,m_Caps) ? "true" : "false";
    caps += ",D3_COLD=";
    caps += FLD_TEST_DRF(_EC_INFO,_CAP_0,_D3_COLD,_TRUE,m_Caps) ? "true" : "false";
    caps += ",RTD3=";
    caps += FLD_TEST_DRF(_EC_INFO, _CAP_1, _RTD3_SUPPORT, _TRUE, m_Caps >> 8) ? "true" : "false";
    caps += ",T1 Support:";
    caps += FLD_TEST_DRF(_EC_INFO,_CAP_1,_PWRUP_T1_SUPPORT,_TRUE,m_Caps>>8) ? "true" : "false";
    caps += ",T2 Support:";
    caps += FLD_TEST_DRF(_EC_INFO,_CAP_1,_PWRUP_T2_SUPPORT,_TRUE,m_Caps>>8) ? "true" : "false";

    return caps.c_str();
}
//------------------------------------------------------------------------------
// Return the EC's GC6 capabilities.
UINT32 SmbEC::GetCapabilities()
{
    return m_Caps;
}

//------------------------------------------------------------------------------
// Return the EC's current FSM mode.
// Note the EC is capable of support both GC6K & GC6M but not both at the same
// time. There is a dip switch on the board to tell the EC which one to support.
// We can read the FW_CFG register to see how these switches are configured.
// Note: The caps reflects what the EC is capable if doing but may not how it is
//       lwrrently configured. This register reflects how it is configured.
RC SmbEC::GetLwrrentFsmMode(UINT08* pMode)
{
    RC rc;
    CHECK_RC(WriteReg16(__FUNCTION__,m_SmbAddr,EC_INDEX_REG,LW_EC_INFO_FW_CFG));
    CHECK_RC(ReadReg08(__FUNCTION__, m_SmbAddr,EC_DATA_REG,pMode));
    m_ECFsmMode = DRF_VAL(_EC_INFO,_FW_CFG,_FSM_MODE, *pMode);
    return OK;
}

RC SmbEC::SetLwrrentFsmMode(UINT08 mode)
{
    if (m_ECFsmMode == mode)
    {
        // EC FSM mode is already set to required mode
        return RC::OK;
    }

    RC rc;
    Printf(m_Opts.verbosePrint, "EC FSM Mode changed from %s to ", GetFsmModeString());
    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FW_CFG));
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG, mode |
            DRF_DEF(_EC_INFO, _FW_CFG, _UPDATE, _TRIGGER)));
    mode = 0;
    CHECK_RC(ReadReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG, &mode));
    m_ECFsmMode = DRF_VAL(_EC_INFO, _FW_CFG, _FSM_MODE, mode);
    Printf(m_Opts.verbosePrint, "%s\n", GetFsmModeString());
    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, 0));
    return rc;
}

const char * SmbEC::GetFsmModeString()
{
    switch (m_ECFsmMode)
    {
        case LW_EC_INFO_FW_CFG_FSM_MODE_BOOTLOADER: return "BootLoader";
        case LW_EC_INFO_FW_CFG_FSM_MODE_GC6K:       return "GC6K";
        case LW_EC_INFO_FW_CFG_FSM_MODE_GC6M:       return "GC6M";
        case LW_EC_INFO_FW_CFG_FSM_MODE_GC6M_SP:    return "GC6M_SP";
        case LW_EC_INFO_FW_CFG_FSM_MODE_RTD3:       return "RTD3";
        default:                                    return "Unknown";
    }
}
//------------------------------------------------------------------------------
// Dump the debug registers.
// The actual meaning of these registers will change from version to version but
// will still be useful for debugging.
RC SmbEC::DumpStateMachine(Tee::Priority pri)
{
    RC rc;
    const UINT16 dbgStart = LW_EC_INFO_DEBUG_0;
    const UINT08 major = m_Regs[LW_EC_INFO_FW_VERSION_MAJOR];
    const UINT08 minor = m_Regs[LW_EC_INFO_FW_VERSION_MINOR];

    if (major > 1 || (major == 0 && minor >= 9))
    {
        UINT16 value = DRF_DEF(_EC_SMBUS,_REG_INDEX,_AINCW,_ENABLE) |
                       DRF_DEF(_EC_SMBUS,_REG_INDEX,_AINCR,_ENABLE) |
                       dbgStart;
        CHECK_RC(WriteReg16(__FUNCTION__,m_SmbAddr,EC_INDEX_REG, value));

        // Read and print the current contents of the uController's internal
        // debug registers.
        Printf(pri,"EC_INFO_DEBUG_0_%d:\n", LW_EC_INFO_DEBUG__SIZE_1-1);
        const unsigned dbgEnd = dbgStart+LW_EC_INFO_DEBUG__SIZE_1;
        for(unsigned i = dbgStart; i < dbgEnd; )
        {
            Printf(pri,"0x%02x: ",i);
            for( unsigned j = 0; j < 8 && i < dbgEnd; j++, i++)
            {
                CHECK_RC(ReadReg08(__FUNCTION__,m_SmbAddr,EC_DATA_REG,&m_Regs[i]));
                Printf(pri,"0x%02x ",m_Regs[i]);
            }
            Printf(pri,"\n");
        }
        // reset the r/w auto-inc.
        CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, 0));
    }
    return rc;
}

//------------------------------------------------------------------------------
void SmbEC::Cleanup()
{
    if(m_pSmbMgr)
    {
        UINT32 numDevices = 0;
        m_pSmbMgr->GetNumDevices(&numDevices);
        if (numDevices)
        {
            m_pSmbMgr->Uninitialize();
            m_pSmbMgr->Forget();
        }
        m_pSmbMgr = NULL;
    }
}

//------------------------------------------------------------------------------
RC SmbEC::InitializeSmbEC()
{
    RC rc;
    UINT16 value = DRF_DEF(_EC_SMBUS,_REG_INDEX,_AINCW,_ENABLE) |
                   DRF_DEF(_EC_SMBUS,_REG_INDEX,_AINCR,_ENABLE) |
                   DRF_NUM(_EC_SMBUS,_REG_INDEX,_OFFSET, LW_EC_INFO_FW_ID_0);

    // Note: Don't use the looping WriteReg16() or ReadReg08() APIs until we are
    // sure that we are talking to the SmbEC because is causes unwanted prints
    // when non-SmbEc devices are connected.
    CHECK_RC(m_pSmbPort->WrWordCmd(m_SmbAddr,EC_INDEX_REG,value));

    UINT16 vendorId = 0;
    CHECK_RC(m_pSmbPort->RdWordCmd(m_SmbAddr,EC_DATA_REG,&vendorId));

    if (vendorId == (LW_EC_INFO_FW_ID_1_VENDOR_H_LWIDIA << 8 |
                     LW_EC_INFO_FW_ID_0_VENDOR_L_LWIDIA))
    {
        value = DRF_DEF(_EC_SMBUS,_REG_INDEX,_AINCW,_ENABLE) |
                DRF_DEF(_EC_SMBUS,_REG_INDEX,_AINCR,_ENABLE) |
                DRF_DEF(_EC_SMBUS,_REG_INDEX,_OFFSET,_INIT);

        CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr,EC_INDEX_REG,value));

        // Read the current contents of the uController's internal register space.
        for( unsigned offset=0; offset < m_Regs.size(); offset++)
        {
            CHECK_RC(ReadReg08(__FUNCTION__,m_SmbAddr,EC_DATA_REG,&m_Regs[offset]));
        }

        const int idx = LW_EC_INFO_CAP_0;
        m_Caps =static_cast<UINT32>(m_Regs[idx])         |   // caps 0
                static_cast<UINT32>(m_Regs[idx+1]) << 8  |   // caps 1
                static_cast<UINT32>(m_Regs[idx+2]) << 16 |   // caps 2 (not used)
                static_cast<UINT32>(m_Regs[idx+3]) << 24 ;   // caps 3 (not used)

        m_ECFsmMode = m_Regs[LW_EC_INFO_FW_CFG];
        m_ECFsmMode = DRF_VAL(_EC_INFO,_FW_CFG,_FSM_MODE, m_ECFsmMode);

        PrintFirmwareInfo(Tee::PriLow);
        const UINT08 major = m_Regs[LW_EC_INFO_FW_VERSION_MAJOR];
        const UINT08 minor = m_Regs[LW_EC_INFO_FW_VERSION_MINOR];

        if ( major == 0 && minor <= 8)
        {
            Printf(Tee::PriHigh,
                "\nERROR- EC Firmware is out of date and not supported.\n");
            return RC::INCORRECT_ROM_VERSION;
        }
        else
        {
            // overwrite bits:31:20 with the JT Caps Revision. YUCK we are going
            // to get into trouble with this in the future unless the EC FW
            // reserves these bits for all time.
            m_Caps = FLD_SET_DRF(_JT_FUNC, _CAPS_REVISION, _ID, _2_00, m_Caps);
        }

        // reset the r/w auto-inc.
        CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr,EC_INDEX_REG, 0));
        return OK;
    }

    return RC::DEVICE_NOT_FOUND;
}

//------------------------------------------------------------------------------
const char * SmbEC::GetRegName(UINT08 reg)
{
    switch(reg)
    {
        case EC_INDEX_REG: return "index";
        case EC_DATA_REG: return "data";
        default: return "unknown";
    }
}

//------------------------------------------------------------------------------
// Write one of the ECs 8bit registers using retries if necessary.
// Note: There appears to be an issue with the EC where read/write requests
// timeout. When this happens its usually the EC failing to acknowledge its
// address(SDA fails to go low while SCL is high on the ACK clock).
RC SmbEC::WriteReg08(const char * pMsg,UINT32 devAddr, UINT08 reg, UINT08 value)
{
    RC rc;
    int retry = 0;

    do{
        rc.Clear();
        rc = m_pSmbPort->WrByteCmd(devAddr,reg,value);
        if(OK != rc)
        {
            Printf(Tee::PriNormal,
                "%s failed to write %d to EC's %s register(0x%x) rc=%d retry:%d\n",
                pMsg, value, GetRegName(reg), reg, rc.Get(), retry);
            retry++;
            Tasker::Sleep(1);
        }
    }while(OK != rc && retry < 10);

    return rc;
}

//------------------------------------------------------------------------------
// Write one of the ECs 16 bit registers using retries if necessary.
// Note: There appears to be an issue with the EC where read/write requests
// timeout. When this happens its usually the EC failing to acknowledge its
// address(SDA fails to go low while SCL is high on the ACK clock).
RC SmbEC::WriteReg16(const char * pMsg,UINT32 devAddr, UINT08 reg, UINT16 value)
{
    RC rc;
    int retry = 0;

    do{
        rc.Clear();
        rc = m_pSmbPort->WrWordCmd(devAddr,reg,value);
        if(OK != rc)
        {
            Printf(Tee::PriNormal,
                "%s failed to write %d to EC's %s register(0x%x) rc=%d retry:%d\n",
                pMsg, value, GetRegName(reg), reg, rc.Get(), retry);
            retry++;
            Tasker::Sleep(1);
        }
    }while(OK != rc && retry < 10);

    return rc;
}
//------------------------------------------------------------------------------
// Read one of the ECs registers using retries if necessary.
// Note: There appears to be an issue with the EC where read/write requests
// timeout. When this happens its usually the EC failing to acknowledge its
// address(SDA fails to go low while SCL is high on the ACK clock).
RC SmbEC::ReadReg08(const char* pMsg, UINT32 devAddr,UINT08 reg, UINT08 *pValue)
{
    return ReadReg08(pMsg,devAddr,reg,pValue,Tee::PriNormal);
}

RC SmbEC::ReadReg08
(   const char* pMsg,
    UINT32 devAddr,
    UINT08 reg,
    UINT08 *pValue,
    Tee::Priority pri)
{
    RC rc;
    int retry = 0;

    do{
        rc.Clear();
        rc = m_pSmbPort->RdByteCmd(devAddr,reg,pValue);

        if(OK != rc)
        {
            Printf(pri,
                "%s failed to read EC's %s register(0x%x) rc=%d, retry=%d\n",
                pMsg, GetRegName(reg), reg, rc.Get(),retry);
            retry++;
            Tasker::Sleep(1);
        }
    }while(OK != rc && retry < 10);

    return rc;
}

//------------------------------------------------------------------------------
RC SmbEC::SaveRootPortLTRState()
{
    Printf(m_Opts.verbosePrint,"SmbEC::SaveRootPortLTRState()\n");
    RC rc;

    // Retrieve the root port
    PexDevice *pPexDev = NULL;
    UINT32 port;
    CHECK_RC(m_pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &port));
    if (!pPexDev)
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    CHECK_RC(PexDevice::GetRootPortDevice(&pPexDev, &port));

    // Save the root port LTR Capability if supported
    rc = pPexDev->GetDownstreamPortLTR(port, &m_RootPortLtrState);
    if (rc != OK && rc != RC::UNSUPPORTED_HARDWARE_FEATURE)
    {
        Printf(Tee::PriWarn,
               "Get downstream port LTR mechanism failed!"
               " Continuing anyway.\n");
    }
    return OK;
}

//------------------------------------------------------------------------------
RC SmbEC::RestoreRootPortLTRState()
{
    Printf(m_Opts.verbosePrint,"SmbEC::RestoreRootPortLTRState()\n");
    RC rc;

    // Retrieve the root port
    PexDevice *pPexDev = NULL;
    UINT32 port;
    CHECK_RC(m_pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &port));
    if (!pPexDev)
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    CHECK_RC(PexDevice::GetRootPortDevice(&pPexDev, &port));

    // Restore the root port LTR Capability if supported
    rc = pPexDev->SetDownstreamPortLTR(port, m_RootPortLtrState);
    if (rc != OK && rc != RC::UNSUPPORTED_HARDWARE_FEATURE)
    {
        Printf(Tee::PriWarn,
               "Set downstream port LTR mechanism failed!"
               " Continuing anyway.\n");
    }
    return OK;
}

//------------------------------------------------------------------------------
RC SmbEC::DisableRootPort(PexDevice::LinkPowerState state)
{
    Printf(m_Opts.verbosePrint,"SmbEC::DisableRootPort()\n");
    RC rc;

    // Retrieve the root port
    PexDevice *pPexDev = NULL;
    UINT32 port;
    CHECK_RC(m_pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &port));
    if (!pPexDev)
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    CHECK_RC(PexDevice::GetRootPortDevice(&pPexDev, &port));

    // Shut down the root port link
    if (state == PexDevice::LINK_DISABLE)
    {
        rc = pPexDev->DisableDownstreamPort(port);
    }
    else if (state == PexDevice::L2)
    {
        rc = pPexDev->SetLinkPowerState(port, state);
    }
    if (OK != rc)
    {
        Printf(Tee::PriError, "DisableDownstreamPort failed:%d\n",rc.Get());
    }
    return rc;
}

//------------------------------------------------------------------------------
RC SmbEC::EnableRootPort(PexDevice::LinkPowerState state)
{
    Printf(m_Opts.verbosePrint,"SmbEC::EnableRootPort()\n");
    RC rc;
    // Retrieve the root port
    PexDevice *pPexDev = NULL;
    UINT32 port;
    CHECK_RC(m_pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &port));
    if (!pPexDev)
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    CHECK_RC(PexDevice::GetRootPortDevice(&pPexDev, &port));

    // Bring up the root port link
    if (state == PexDevice::LINK_ENABLE)
    {
        rc = pPexDev->EnableDownstreamPort(port);
    }
    else if (state == PexDevice::L0)
    {
        rc = pPexDev->SetLinkPowerState(port, state);
    }

    if (OK != rc)
    {
        Printf(Tee::PriError, "EnableDownstreamPort failed:%d\n",rc.Get());
    }
    return rc;
}

//------------------------------------------------------------------------------
RC SmbEC::PollLinkStatus(PexDevice::LinkPowerState state)
{
    RC              rc;
    PexDevice       *pPexDev = NULL;
    UINT32          port;
    UINT64          start, end;
    const UINT64    freq = Xp::QueryPerformanceFrequency();
    const UINT32    delayMs = m_Opts.pollLinkStatusMs;
    CHECK_RC(m_pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &port));
    if (!pPexDev)
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    CHECK_RC(PexDevice::GetRootPortDevice(&pPexDev, &port));
    Printf(m_Opts.verbosePrint, "SmbEC::PollLinkStatus(%s)\n", pPexDev->GetLinkStateString(state));

    start = Xp::QueryPerformanceCounter();
    // Wait for the link to go up or down
    CHECK_RC(pPexDev->PollDownstreamPortLinkStatus(port, delayMs, state));
    end = Xp::QueryPerformanceCounter();

    if (state == PexDevice::LINK_ENABLE || state == PexDevice::L0)
    {
        m_Stats.exitLinkUpMs = ((end-start)*1000)/freq;
        Tasker::Sleep(m_Opts.delayAfterLinkStatusMs);
    }
    else
    {
        m_Stats.enterLinkDnMs = ((end-start)*1000)/freq;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC SmbEC::GetPowerStatus(UINT32 * pInOut)
{
    // debug counters for Maxwell GC6M debugging
    static int count = 1;
    static UINT32 s_pwrStatus = 0;
    string  pwrStatus("unknown");

    RC rc;
    UINT08 data = DRF_DEF(_EC_INFO, _FSM_STATUS_0, _PWR_STATUS,_GPU_UNKNOWN);

    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_STATUS_0));

    CHECK_RC(ReadReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG, &data));

    switch (DRF_VAL(_EC_INFO, _FSM_STATUS_0, _PWR_STATUS, data))
    {
        default:
        case LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_UNKNOWN:
            *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION);
            pwrStatus = "transition";
            break;
        case LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_ON:
            *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_ON) |
                      DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_PWOK);
            pwrStatus = "PWROK";
            break;
        case LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_OFF:
            *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_OFF) |
                      DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_OFF);
            pwrStatus = "OFF";
            break;
        case LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_GC6:
            *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_GC6) |
                      DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_OFF);
            pwrStatus = "GC6 | OFF";
            break;
    }
    Printf(m_Opts.verbosePrint,"SmbEC::GetPowerStatus() returning %s(0x%x)\n",
        pwrStatus.c_str(),static_cast<UINT32>(data));
    count = (s_pwrStatus == *pInOut) ? count+1: 1;
    s_pwrStatus = *pInOut;
    if (count % 25 == 0)
        DumpStateMachine(m_Opts.verbosePrint);

    return rc;
}

//------------------------------------------------------------------------------
// Tell the EC to prepare for power down request via the FB_CLAMP I/O line.
//
// This sequence represent the case where the GPU enters GC6 and the display
// enters Self-Refresh.
//                  E0                  E1               E2        E3
//                  :                   :                :_________:____________
// GC6 Active       :___________________:________________|         :
//                  :___________________:________________:_________:____
// GPU Power        :                   :                :         :    |_______
//                  :___________________:________________:_________:
// GPU_PWR_EN       :                   :                :         |____________
//                  :___________________:________________:_______________
// GPU_PWR_GOOD     :                   :                :               |______
//                  :___________________:                :______________________
// FB_CLMP_TGL_REQ# :                   |________________|
//                  :                             _______________________________
// FB_CLAMP         :____________________________|
//                  :__________________________________________
// PEX_RST#         :                                          |_________________
//                  ^                   ^                ^         ^
//                 DBGS                DBGC             DAGC      DAGP
// PCIe Root Power GC6 Enter
// DAGC - Link Disable After GC6 Entry complete and before GPU Power Down (E2) (default)
// DBGC - Link Disable Before GC6 Entry complete (E1)
// DAGP - Link Disable After GC6 Entry and GPU Power Down is complete (E3)
// DBGS - Link Disable Before GC6 Entry starts (E0)
//
// Figure 6 GPU GC6 Enter Sequence
// Refer to the Jefferson Technology System Design Guide for more info.
//
RC SmbEC::EnterGC6Power(UINT32 * pInOut)
{
    Printf(m_Opts.verbosePrint,"SmbEC::EnterGC6Power\n");
    RC rc;
    UINT64 start, end, freq = Xp::QueryPerformanceFrequency();
    UINT32 cmd = *pInOut;

    // set defaults
    m_Stats.entryTimeMs = 0;
    m_Stats.entryStatus = RC::TIMEOUT_ERROR;
    m_Stats.entryPwrStatus = 0;

    *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION);

    if (FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_PRGE, _DBGC, cmd) || // E1 of Fig6
        FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_PRGE, _DAGC, cmd))   // E2 of Fig6
    {
        Printf(Tee::PriNormal,"%s EC FW doesn't support op:0x%x.\n", __FUNCTION__, *pInOut);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    fatalMsg msg = {&start, &rc};
    Utility::CleanupOnReturnArg<SmbEC,void,fatalMsg*>
        fatalEnterGc6Msg(this,&SmbEC::FatalEnterGc6Msg,&msg);

    CHECK_RC(SaveRootPortLTRState());

    if (FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_PRGE, _DBGS, cmd)) // E0 of fig 6
    {
        CHECK_RC(DisableRootPort(PexDevice::LINK_DISABLE));
        CHECK_RC(PollLinkStatus(PexDevice::LINK_DISABLE));
    }
    // Read FSM_EVENT_CTRL register for TRIGGER_EVENT_DONE.
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_EVENT_CNTL));
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
        DRF_DEF(_EC_INFO,_FSM_EVENT_CNTL,_EVENT_ID, _GC6_PWRDN_PREP) |
        DRF_DEF(_EC_INFO,_FSM_EVENT_CNTL,_TRIGGER_EVENT, _TRIGGER)));

    start = Xp::QueryPerformanceCounter();

    // reset the r/w auto-inc.
    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, 0));
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_EVENT_CNTL));

    Tasker::Sleep(m_Opts.enterCmdToStatDelayMs);

    for (int retry = 0; retry < 500; retry++)
    {
        CHECK_RC(ReadReg08(__FUNCTION__, m_SmbAddr,EC_DATA_REG,
                reinterpret_cast<UINT08*>(&m_Stats.entryPwrStatus), Tee::PriLow));

        if (FLD_TEST_DRF(_EC_INFO,_FSM_EVENT_CNTL,_TRIGGER_EVENT,_DONE,
                m_Stats.entryPwrStatus))
        {
            // Update stats
            end = Xp::QueryPerformanceCounter();
            m_Stats.entryTimeMs = ((end-start)*1000)/freq;
            m_Stats.entryStatus = OK;
            break;
        }
        Tasker::Sleep(1);
    }

    if (m_Stats.entryStatus != OK )
        DumpStateMachine(Tee::PriNormal);

    if (OK == m_Stats.entryStatus &&
        FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_PRGE, _DAGP, *pInOut)) // E3 of fig 6
    {
        CHECK_RC(DisableRootPort(PexDevice::LINK_DISABLE));
        CHECK_RC(PollLinkStatus(PexDevice::LINK_DISABLE));
    }

    // Power is off and link is disabled so go ahead and updata final status.
    fatalEnterGc6Msg.Cancel();
    *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_GC6) |
             DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_OFF);

    return rc;
}

//------------------------------------------------------------------------------
void SmbEC::FatalEnterGc6Msg(fatalMsg * pMsg)
{
    // If we get here we are in big trouble and the GPU is not on the bus.
    DumpStateMachine(Tee::PriNormal);

    UINT64 end = Xp::QueryPerformanceCounter();
    m_Stats.entryTimeMs = ((end-*pMsg->pStart)*1000)/Xp::QueryPerformanceFrequency();
    m_Stats.entryStatus = RC::TIMEOUT_ERROR;
    Printf(Tee::PriHigh,"WARNING Fatal Error!\n");
    Printf(Tee::PriHigh,"EnterGC6Power failed with error:%d(%s)\n",
        pMsg->pRC->Get(),pMsg->pRC->Message());
    Printf(Tee::PriHigh,"GPU may be off the bus!\n");
    Printf(Tee::PriHigh,"EC PwrStatus = %d\n", m_Stats.entryPwrStatus);
    Tee::TeeFlushQueuedPrints(Tasker::NO_TIMEOUT);
    return;
}
//------------------------------------------------------------------------------
void SmbEC::FatalExitGc6Msg(fatalMsg * pMsg)
{
    // If we get here we are in big trouble and the GPU is not on the bus.
    DumpStateMachine(Tee::PriNormal);

    UINT64 end = Xp::QueryPerformanceCounter();
    m_Stats.exitTimeMs = ((end-*pMsg->pStart)*1000)/Xp::QueryPerformanceFrequency();
    m_Stats.exitStatus = RC::TIMEOUT_ERROR;
    Printf(Tee::PriHigh,"WARNING Fatal Error!\n");
    Printf(Tee::PriHigh,"ExitGC6Power failed with error:%d(%s)\n",
        pMsg->pRC->Get(),pMsg->pRC->Message());
    Printf(Tee::PriHigh,"GPU may be off the bus!\n");
    Printf(Tee::PriHigh,"EC PwrStatus = %d\n", m_Stats.exitPwrStatus);
    Tee::TeeFlushQueuedPrints(Tasker::NO_TIMEOUT);
    return;
}

//------------------------------------------------------------------------------
// Send a wakeup event then poll on EC for PWR_OK status
RC SmbEC::SendWakeupEvent(UINT32 eventId, INT32 pollTimeMs)
{
    m_ECWakeupEvent = eventId;
    SendWakeupEvent();
    UINT32 status = 0;
    while (pollTimeMs > 0)
    {
        GetPowerStatus(&status);
        if(FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_PWOK,status))
        {
            return OK;
        }
        Tasker::Sleep(2);
        pollTimeMs -= 2;
    }
    Printf(m_Opts.verbosePrint,
        "SmbEC::SetWakeupEvent(%d) failed to detect PWROK after generating"
        " wakeup event.\n",eventId);
    return RC::TIMEOUT_ERROR;
}

//------------------------------------------------------------------------------
// Send a wakeup event to the System Control Island (SCI).
// Note: For GC6K the only valid event is EC_WAKEUP_GPUEVENT.
//       For GC6M EC_WAKEUP_HOTPLUG & EC_WAKEUP_HOTPLUGIRQ require board rework
//       and by default will not be used. Modify the picker via js to add these
//       events.
RC SmbEC::SendWakeupEvent()
{
    Printf(m_Opts.verbosePrint, "SmbEC::SendWakeupEvent(0x%x)\n", m_ECWakeupEvent);
    RC rc;
    switch(m_ECWakeupEvent)
    {
        // We don't actually expect the GPU to wake up when fake therm sensor
        // is enabled, therefore we use a GPU event to wake up the GPU.
        // Hence falling to the EC_WAKE_GPUEVENT case is intentional.
        case EC_WAKEUP_I2CBYPASS:
            {
                UINT08 value = 0;
                m_Stats.exitWakeupEventStatus =
                    ReadReg08(__FUNCTION__, m_ThermAddr, 0, &value);
                if((OK == m_Stats.exitWakeupEventStatus) && value != 0)
                {
                    Printf(Tee::PriHigh,
                            "SmbEC::SendWakeupEvent() Fake Thermal sensor value"
                            " is:0x%x expected:0x0\n", value);
                    m_Stats.exitWakeupEventStatus = RC::SOFTWARE_ERROR;
                }
            }

        //default mode, backward compatible with GC6K
        case EC_WAKEUP_GPUEVENT:
            CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, LW_EC_INFO_FSM_EVENT_CNTL));
            CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
                DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT,_TRIGGER) |
                DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _EVENT_ID, _GC6_PWRUP_FULL)));
            break;

        // Note: Even if the I2C read fails the GPU will still wakeup, but the
        // replay logic may be faulty. Save the I2C status and let the GC6Test
        // declare failure. We don't want to return failure to RM and prevent the
        // Gc6 exit process from completing.
        case EC_WAKEUP_I2C:
            {
                UINT08 value = 0;
                const UINT08 reg = LW_LWI2CS_GPUID_VENDOR_ID_L;
                m_Stats.exitWakeupEventStatus =
                    ReadReg08(__FUNCTION__,m_ThermAddr,reg,&value);

                if((OK == m_Stats.exitWakeupEventStatus) &&
                   (value != LW_LWI2CS_GPUID_VENDOR_ID_L_VENDOR_ID_INIT))
                {
                   Printf(Tee::PriHigh,
                       "SmbEC::SendWakeupEvent() I2C data is:0x%x expected:0xDE\n",
                       value);
                   m_Stats.exitWakeupEventStatus = RC::HW_STATUS_ERROR;
                }
            }
            break;

        // The timer is programmed during GC6Entry, so nothing to do.
        case EC_WAKEUP_TIMER:
            break;

        case EC_WAKEUP_HOTUNPLUG:
            CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,LW_EC_INFO_FSM_EVENT_CNTL));
            CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
                DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT,_TRIGGER) |
                DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _EVENT_ID, _HPD_DEASSERT)));
            break;

        case EC_WAKEUP_HOTPLUG:
            CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,LW_EC_INFO_FSM_EVENT_CNTL));
            CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
                DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT,_TRIGGER) |
                DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _EVENT_ID, _GC6_PWRUP_T1)));
            break;

        case EC_WAKEUP_HOTPLUGIRQ:
            CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,LW_EC_INFO_FSM_EVENT_CNTL));
            CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
                DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT,_TRIGGER) |
                DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _EVENT_ID, _GC6_PWRUP_T2)));
            break;
        case EC_WAKEUP_RTD3_GPU_TIMER:
            // The timer is programmed during RTD3 entry
            break;
        case EC_WAKEUP_RTD3_CPU:
            // Don't need to do any WAKE# in RTD3 CPU event, expectation is
            // PEX_RST# will get de-asserted after some random idleness
            break;
    }
    return OK;
}

//------------------------------------------------------------------------------
// This sequence represent the case where the GPU exits GC6 and the display
// exits Self-Refresh.
//                  X0  X1          X1a  X2              X3
//                  :___:___________:____:_______________:
// GC6 Active       :   :           :    :               |______________________
//                  :   :    _______:____:_______________:______________________
// GPU Power        :___:___|       :    :               :
//                  :   :___________:____:_______________:______________________
// GPU_PWR_EN       :___|           :    :               :
//                  :            ___:____:_______________:______________________
// GPU_PWR_GOOD     :___________|   :    :               :
//                  :_______________:___ :               :______________________
// FB_CLMP_TGL_REQ# :               :   |:_______________|
//                  :_______________:____:________
// FB_CLAMP         :               :    :        |_____________________________
//                  :               :____:______________________________________
// PEX_RST#         :_______________|
//                  ^               ^    ^               ^
//                  ^               ^    ^               ^
//                EBPG            EAPG  EBPC           EAGP
// PCI Root Power GC6 Exit
// EBPG = Link enable before GPU power-on and GC6 exit begins (X0) default
// EBPC = Link enable at beginning of GPU GC6 exit (X2)
// EAGP = Link enable after end of GPU GC6 exit (X3)
// EAPG = Link enable after GPU power-on reset, but before GC6 exit begins
//
// Figure 7 GPU GC6 Exit & Exit Self-Refresh (XGXS) Sequence
//
RC SmbEC::ExitGC6Power(UINT32 * pInOut)
{
    Printf(m_Opts.verbosePrint,"SmbEC::ExitGC6Power\n");
    RC rc;
    UINT64 start=0, end=0, freq = Xp::QueryPerformanceFrequency();
    UINT32 cmd = *pInOut;

    // set defaults
    *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_TRANSITION);
    m_Stats.exitStatus = RC::TIMEOUT_ERROR;
    m_Stats.exitPwrStatus = 0;
    m_Stats.exitWakeupEvent = m_ECWakeupEvent;

    if (FLD_TEST_DRF(_JT_FUNC, _POWERCONTROL, _PRGX, _EBPC, cmd)||
        FLD_TEST_DRF(_JT_FUNC, _POWERCONTROL, _PRGX, _EAPG, cmd))
    {
        Printf(Tee::PriNormal,"%s EC FW doesn't support op:0x%x.\n", __FUNCTION__, cmd);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    fatalMsg msg = {&start, &rc};
    Utility::CleanupOnReturnArg<SmbEC,void,fatalMsg*>
        fatalExitGc6Msg(this,&SmbEC::FatalExitGc6Msg,&msg);

    //
    // Enable the PEX root port link (X0 in figure 7)
    //(but don't wait for link status until after power up).
    //
    if (FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_PRGX,_EBPG, cmd))
    {
        CHECK_RC(EnableRootPort(PexDevice::LINK_ENABLE));
    }

    CHECK_RC(SendWakeupEvent());

    start = Xp::QueryPerformanceCounter();

    // reset the r/w auto-inc.
    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, 0));

    // Wait for good status
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_STATUS_0));

    Tasker::Sleep(m_Opts.exitCmdToStatDelayMs);

    for (int retry = 0; retry < 500; retry++)
    {
        CHECK_RC(ReadReg08(__FUNCTION__, m_SmbAddr,EC_DATA_REG,
                reinterpret_cast<UINT08*>(&m_Stats.exitPwrStatus), Tee::PriLow));

        if (FLD_TEST_DRF(_EC_INFO,_FSM_STATUS_0,_PWR_STATUS,_GPU_ON,
                m_Stats.exitPwrStatus))
        {
            // Update stats
            end = Xp::QueryPerformanceCounter();
            m_Stats.exitTimeMs = ((end-start)*1000)/freq;
            m_Stats.exitStatus = OK;
            break;
        }
        Tasker::Sleep(2);
    }

    // Power should be up and PEX_RST deasserted. PCIE spec says to
    // wait 100ms before accessing the PCI config space. However we
    // are just going to wait for the link to be active and use the
    // m_Opts.delayAfterResetMs value to optimize this process.
    if (FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_PRGX,_EBPG, cmd))
    {
        CHECK_RC(PollLinkStatus(PexDevice::LINK_ENABLE));
    }

    if (FLD_TEST_DRF(_JT_FUNC,_POWERCONTROL,_PRGX,_EAGP, cmd))
    {
        CHECK_RC(EnableRootPort(PexDevice::LINK_ENABLE));
        CHECK_RC(PollLinkStatus(PexDevice::LINK_ENABLE));
    }

    CHECK_RC(RestoreRootPortLTRState());

    // Power is on and link is up so go ahead and update status.
    fatalExitGc6Msg.Cancel();
    *pInOut = DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_GC_STATE,_ON) |
              DRF_DEF(_JT_FUNC,_POWERCONTROL,_GPU_POWER_STATE,_PWOK);
    return rc;
}

//------------------------------------------------------------------------------
// Ask the EC to generate this wakeup event to the GPU on subsequent GC6 cycles
RC SmbEC::SetWakeupEvent(UINT32 eventId)
{
    RC rc;
    m_ECWakeupEvent = eventId;
    if ((eventId == EC_WAKEUP_HOTUNPLUG) || (eventId == EC_WAKEUP_HOTPLUGIRQ))
    {
        CHECK_RC(AssertHpd(true));
        Tasker::Sleep(2); // give the sci time to register that this signal has been asserted
    }
    return rc;
}

//------------------------------------------------------------------------------
// Assert true/false on the Hot plug detect signal in preparation for a hot plug
// or unplug event.
RC SmbEC::AssertHpd(bool bAssert)
{
    RC rc;
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,LW_EC_INFO_FSM_EVENT_CNTL));
    if (bAssert)
    {
        CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
            DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT,_TRIGGER) |
            DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _EVENT_ID, _HPD_ASSERT)));
    }
    else
    {
        CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
            DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT,_TRIGGER) |
            DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _EVENT_ID, _HPD_DEASSERT)));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC SmbEC::GetCycleStats(Xp::JTMgr::CycleStats * pStats)
{
    *pStats = m_Stats;
    return OK;
}

//------------------------------------------------------------------------------
RC SmbEC::SetDebugOptions(Xp::JTMgr::DebugOpts * pOpts)
{
    m_Opts = *pOpts;
    return OK;
}

//------------------------------------------------------------------------------
RC SmbEC::GetDebugOptions(Xp::JTMgr::DebugOpts * pOpts)
{
    *pOpts = m_Opts;
    return OK;
}

RC SmbEC::EnterRTD3Power()
{
    Printf(m_Opts.verbosePrint, "SmbEC::EnterRTD3Power\n");
    UINT64 start = 0, end = 0, freq = Xp::QueryPerformanceFrequency();
    m_Stats.entryStatus = RC::TIMEOUT_ERROR;
    m_Stats.entryPwrStatus = 0;
    RC rc;

    if (m_IsBridgeDevicePresent)
    {
        if (!Xp::DoesFileExist(BridgesConfigSave))
        {
            Printf(Tee::PriError, "%s does not exist which is required"
                "for interposer setup\n", BridgesConfigSave);
            return RC::FILE_DOES_NOT_EXIST;
        }

        const int status = system(BridgesConfigSave);

        if (status)
        {
            Printf(Tee::PriError, "Unable to execute %s to save config spaces\n",
                BridgesConfigSave);
            return RC::SCRIPT_FAILED_TO_EXELWTE;
        }
    }

    CHECK_RC(SaveRootPortLTRState());
    // Transtion to L2 state
    CHECK_RC(DisableRootPort(PexDevice::L2));
    CHECK_RC(PollLinkStatus(PexDevice::L2));

    Tasker::Sleep(m_Opts.delayAfterL2EntryMs);

    start = Xp::QueryPerformanceCounter();
    // --------------------------------
    // Assert PEX RST#
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_EVENT_CNTL));
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
        DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _EVENT_ID, _RTD3_PWRDN_PEXRESET_ASSERT) |
        DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT, _TRIGGER)));

    // reset the r/w auto-inc.
    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, 0));
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_EVENT_CNTL));
    for (int retry = 0; retry < 500; retry++)
    {
        CHECK_RC(ReadReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
                reinterpret_cast<UINT08*>(&m_Stats.entryPwrStatus), Tee::PriLow));

        if (FLD_TEST_DRF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT, _DONE, m_Stats.entryPwrStatus))
        {
            end = Xp::QueryPerformanceCounter();
            m_Stats.entryTimeMs = ((end-start)*1000)/freq;
            m_Stats.entryStatus = OK;
            break;
        }
        Tasker::Sleep(1);
    }
    return rc;
}

RC SmbEC::ExitRTD3Power()
{
    Printf(m_Opts.verbosePrint, "SmbEC::ExitRTD3Power\n");
    UINT64 start = 0, end = 0, freq = Xp::QueryPerformanceFrequency();
    m_Stats.exitWakeupEvent = m_ECWakeupEvent;
    RC rc;
    CHECK_RC(SendWakeupEvent());

    start = Xp::QueryPerformanceCounter();

    if (m_ECWakeupEvent & EC_WAKEUP_RTD3_GPU_TIMER)
    {
        CHECK_RC(PollGpuWakeOnRTD3());
    }

    // Deassert PEX RST#
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_EVENT_CNTL));
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
        DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _EVENT_ID, _RTD3_PWRUP_PEXRESET_DEASSERT) |
        DRF_DEF(_EC_INFO, _FSM_EVENT_CNTL, _TRIGGER_EVENT, _TRIGGER)));

    // reset the r/w auto-inc.
    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, 0));
    // Wait for good status
    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_STATUS_0));
    for (int retry = 0; retry < 500; retry++)
    {
        CHECK_RC(ReadReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
                reinterpret_cast<UINT08*>(&m_Stats.exitPwrStatus), Tee::PriLow));

        if (FLD_TEST_DRF(_EC_INFO, _FSM_STATUS_0, _PWR_STATUS, _GPU_ON, m_Stats.exitPwrStatus))
        {
            end = Xp::QueryPerformanceCounter();
            m_Stats.exitTimeMs = ((end-start)*1000)/freq;
            m_Stats.exitStatus = OK;
            break;
        }
        Tasker::Sleep(1);
    }
    // Transition back to L0 for power up
    CHECK_RC(EnableRootPort(PexDevice::L0));
    CHECK_RC(PollLinkStatus(PexDevice::L0));
    CHECK_RC(RestoreRootPortLTRState());

    const UINT64 startTime = Platform::GetTimeMS();
    if (m_IsBridgeDevicePresent)
    {
        if (!Xp::DoesFileExist(BridgesConfigRestore))
        {
            Printf(Tee::PriError, "%s does not exist which is required"
                "for interposer setup\n", BridgesConfigRestore);
            return RC::FILE_DOES_NOT_EXIST;
        }

        Tasker::Sleep(m_Opts.delayAfterL2ExitMs);

        const int status = system(BridgesConfigRestore);

        if (status)
        {
            Printf(Tee::PriError, "Unable to execute %s to restore config spaces\n",
                BridgesConfigRestore);
            return RC::SCRIPT_FAILED_TO_EXELWTE;
        }

        Tasker::Sleep(m_Opts.delayAfterL2ExitMs);
    }

    CHECK_RC(PollGpuPciRead());
    Printf(m_Opts.verbosePrint, "PCI config space restore took %llums\n",
            Platform::GetTimeMS() - startTime);

    return rc;
}

RC SmbEC::SetWakeConfigForTu10x()
{
    RC rc;

    Printf(m_Opts.verbosePrint, "Setting Wake Config for TU10X\n");

    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FW_CFG_WAKE_GPIO));

    if (m_pSubdev->DeviceId() == Gpu::TU102)
    {
        CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
            DRF_DEF(_EC_INFO, _FW_CFG_WAKE_GPIO, _ISTU102, _YES) |
            DRF_DEF(_EC_INFO, _FW_CFG_WAKE_GPIO, _UPDATE, _TRIGGER)));
    }
    else if (m_pSubdev->DeviceId() >= Gpu::TU102)
    {
        CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
            DRF_DEF(_EC_INFO, _FW_CFG_WAKE_GPIO, _ISTU102, _NO) |
            DRF_DEF(_EC_INFO, _FW_CFG_WAKE_GPIO, _UPDATE, _TRIGGER)));
    }

    // reset the r/w auto-inc.
    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, 0));

    return rc;
}

RC SmbEC::PollGpuWakeOnRTD3()
{
    RC rc;

    CHECK_RC(WriteReg08(__FUNCTION__, m_SmbAddr, EC_INDEX_REG,
        LW_EC_INFO_FSM_STATUS_3));

    for (int retry = 0; retry < 500; retry++)
    {
        CHECK_RC(ReadReg08(__FUNCTION__, m_SmbAddr, EC_DATA_REG,
                reinterpret_cast<UINT08*>(&m_Stats.exitGpuWakeStatus), Tee::PriLow));

        if (FLD_TEST_DRF(_EC_INFO, _FSM_STATUS_3,_RTD3_WAKE_TOGGLED, _YES,
                m_Stats.exitGpuWakeStatus))
        {
            break;
        }
        Tasker::Sleep(1);
    }

    // reset the r/w auto-inc.
    CHECK_RC(WriteReg16(__FUNCTION__, m_SmbAddr, EC_INDEX_REG, 0));

    return rc;
}

RC SmbEC::PollGpuPciRead()
{
    RC rc;

    auto pGpuPcie = m_pSubdev->GetInterface<Pcie>();
    if (pGpuPcie == nullptr)
    {
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    INT32 domain = pGpuPcie->DomainNumber();
    INT32 bus = pGpuPcie->BusNumber();
    INT32 device = pGpuPcie->DeviceNumber();
    INT32 func = pGpuPcie->FunctionNumber();

    rc = Tasker::Poll(m_Opts.pollLinkStatusMs, [&]() -> bool
    {
        UINT16 vendorId = 0;
        RC pciRC = Platform::PciRead16(domain, bus, device, func, 0, &vendorId);
        if (pciRC != RC::OK || vendorId != LW_CONFIG_PCI_LW_0_VENDOR_ID_LWIDIA)
        {
            Tasker::Sleep(1);
            return false;
        }
        return true;
    }, MODS_FUNCTION);

    if (rc == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError, "GPU PCI restore unsuccessful. Test is expected to fail\n");
    }

    return rc;
}
