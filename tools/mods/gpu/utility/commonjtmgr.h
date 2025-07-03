/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "smbec.h"

//------------------------------------------------------------------------------
// Wrapper class for Jefferson Technology protocol for GC6
class CommonJTMgr : public Xp::JTMgr
{
public:
    CommonJTMgr(UINT32 GpuInstance);
    virtual ~CommonJTMgr();
    // Generic ACPI Device State Method Evaluator
    RC DSM_Eval
    (
        UINT16 domain,
        UINT16 bus,
        UINT08 dev,
        UINT08 func,
        UINT32 acpiFunc,
        UINT32 acpiSubFunc,
        UINT32 *pInOut,
        UINT16 *pSize
    ) override;

    virtual void Initialize();
    RC GC6PowerControl(UINT32 *pInOut) override;
    void GetCycleStats(Xp::JTMgr::CycleStats * pStats) override;
    RC GetDebugOptions(Xp::JTMgr::DebugOpts * pOpts) override;
    void SetDebugOptions(Xp::JTMgr::DebugOpts * pOpts) override;
    UINT32 GetCapabilities() override;
    RC SetWakeupEvent(UINT32 eventId) override;
    RC AssertHpd(bool bAssert) override;
    RC GetLwrrentFsmMode(UINT08 *pFsmMode) override;
    RC SetLwrrentFsmMode(UINT08 fsmMode) override;
    bool IsJTACPISupported() override { return m_JTACPISupport; }
    bool IsNativeACPISupported() override { return m_NativeACPISupport; }
    bool IsSMBusSupported() override { return m_SMBusSupport; }
    RC RTD3PowerCycle(Xp::PowerState state) override;
protected:
    virtual RC DiscoverACPI();
    virtual RC DiscoverSMBus();
    virtual RC DiscoverNativeACPI();
    virtual RC ACPIGC6PowerControl(UINT32* pInOut);
    virtual RC SMBusGC6PowerControl(UINT32* pInOut);
    virtual RC DSM_Eval_ACPI
    (
        UINT16 domain,
        UINT16 bus,
        UINT08 dev,
        UINT08 func,
        UINT32 acpiFunc,
        UINT32 acpiSubFunc,
        UINT32 *pInOut,
        UINT16 *pSize
    );

    bool            m_JTACPISupport = false;
    bool            m_NativeACPISupport = false;
    bool            m_SMBusSupport = false;
    UINT32          m_GpuInstance;
    UINT32          m_Caps = 0;     //GC6 capabilities
    GpuSubdevice *  m_pSubdev = nullptr;
    SmbEC           m_SmbEc;
    UINT32          m_Domain = ~0;
    UINT32          m_Bus = ~0;
    UINT32          m_Device = ~0;
    UINT32          m_Function = ~0;
};

//--------------------------------------------------------------------------
// Wrapper class to manage one JTMgr instance per GpuInstance.
class CommonJTMgrs
{
protected:
    map<UINT32,CommonJTMgr *> m_JTMgrs;
public:
    CommonJTMgrs(){}
    virtual ~CommonJTMgrs()
    {
        Cleanup();
    }

    void Cleanup()
    {
        map<UINT32,CommonJTMgr*>::iterator it;
        for (it = m_JTMgrs.begin(); it != m_JTMgrs.end(); it++)
        {
            delete it->second;
        }
        m_JTMgrs.clear();
    }

    virtual Xp::JTMgr* Get(UINT32 GpuInstance) = 0;
};
