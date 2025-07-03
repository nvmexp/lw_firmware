/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// This class implements access to a any uController that has been
// programmed to behave as an Embedded Power Controller. Access to the controller
// is done through the SM bus using a predefined set of registers.
#ifndef INCLUDED_SMBEC_H
#define INCLUDED_SMBEC_H

#if !defined(LW_WINDOWS) && !defined(LW_MACINTOSH) || defined(WIN_MFG)
#include "device/smb/smbdev.h"
#include "device/smb/smbport.h"
#include "device/smb/smbmgr.h"
#else
#include "stub/smb_stub.h"
#endif

#include "gpu/include/gpusbdev.h"
#include "core/include/types.h"
#include "core/include/rc.h"
#include <vector>
//TODO: Get the checked in version of this file prior to SW checkin
#include "gpu/gc6/ec_smbus.h"
#include "core/include/xp.h"

class SmbPort;
class SmbManager;
//------------------------------------------------------------------------------
// The SMB External Controller has a 4KB register space. To access it write the
// offset value (0-0xfff) to register EC_INDEX_REG, then read x number of bytes
// from the EX_DATA_REG.
#define EC_SMBUS_ADDR   LW_EC_SMBUS_SLAVE_ADDR_VALUE_INIT
#define EC_INDEX_REG    LW_EC_SMBUS_REG_INDEX
#define EC_DATA_REG     LW_EC_SMBUS_REG_DATA

class SmbEC {

public:
    enum {
        EC_WAKEUP_GPUEVENT          = (1 << 0),
        EC_WAKEUP_I2C               = (1 << 1),
        EC_WAKEUP_TIMER             = (1 << 2),
        EC_WAKEUP_HOTPLUG           = (1 << 3),
        EC_WAKEUP_HOTPLUGIRQ        = (1 << 4),
        EC_WAKEUP_HOTUNPLUG         = (1 << 5),
        EC_WAKEUP_I2CBYPASS         = (1 << 6),
        EC_WAKEUP_RTD3_CPU          = (1 << 7),
        EC_WAKEUP_RTD3_GPU_TIMER    = (1 << 8)        
    };
    SmbEC();
    RC InitializeSmbusDevice(GpuSubdevice *pSubdev);
    RC GetPowerStatus(UINT32 * pInOut);
    RC EnterGC6Power(UINT32 * pInOut);
    RC ExitGC6Power(UINT32 * pInOut);
    RC EnterRTD3Power();
    RC ExitRTD3Power();
    RC GetCycleStats(Xp::JTMgr::CycleStats * pStats);
    RC GetDebugOptions(Xp::JTMgr::DebugOpts * pOpts);
    RC SetDebugOptions(Xp::JTMgr::DebugOpts * pOpts);
    UINT32 GetCapabilities();
    void Cleanup();
    RC SetWakeupEvent(UINT32 eventId);
    RC SendWakeupEvent(UINT32 eventId, INT32 pollTimeMs);
    RC GetLwrrentFsmMode(UINT08* pMode);
    RC SetLwrrentFsmMode(UINT08 mode);
    RC AssertHpd(bool bAssert);
private:
    struct fatalMsg {
        UINT64 * pStart;
        RC * pRC;
    };
    RC InitializeSmbEC();
    void PrintFirmwareInfo(Tee::Priority pri);
    RC DumpStateMachine(Tee::Priority pri);
    void FatalEnterGc6Msg(fatalMsg * pMsg);
    void FatalExitGc6Msg(fatalMsg * pMsg);
    RC ReadReg08(const char * pMsg, UINT32 devAddr, UINT08 reg, UINT08 *pValue,Tee::Priority pri);
    RC ReadReg08(const char * pMsg, UINT32 devAddr, UINT08 reg, UINT08 *pValue);
    RC WriteReg08(const char * pMsg,UINT32 devAddr, UINT08 reg, UINT08 value);
    RC WriteReg16(const char * pMsg,UINT32 devAddr, UINT08 reg, UINT16 value);
    const char * GetRegName(UINT08 reg);
    RC PollLinkStatus(PexDevice::LinkPowerState state);
    RC SaveRootPortLTRState();
    RC RestoreRootPortLTRState();
    RC EnableRootPort(PexDevice::LinkPowerState state);
    RC DisableRootPort(PexDevice::LinkPowerState state);
    RC SetWakeConfigForTu10x();
    RC PollGpuWakeOnRTD3();
    RC PollGpuPciRead();
    const char * GetCapString();
    const char * GetFsmModeString();
    RC SendWakeupEvent();
    UINT32 m_SmbAddr;
    bool m_SmbusInit;
    SmbPort *m_pSmbPort;
    SmbManager *m_pSmbMgr;
    GpuSubdevice *m_pSubdev;
    bool m_IsBridgeDevicePresent = false;
    static constexpr const char* BridgesConfigSave = "./gc6_save.sh";
    static constexpr const char* BridgesConfigRestore = "./gc6_restore.sh";

    //see ec_info.h & jt.h, we munge both definitions together using this var.
    UINT32 m_Caps;
    bool m_RootPortLtrState;         //LTR capability of chipset (not Gpu)
    UINT08 m_ECFsmMode;
    UINT32 m_ECWakeupEvent;
    UINT32 m_ThermAddr;    // SMBus addr of the internal therm sensor.
    vector<UINT08> m_Regs;
    Xp::JTMgr::CycleStats m_Stats;
    Xp::JTMgr::DebugOpts m_Opts;
};

#endif

