/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cheetah/include/cpuutil.h"

namespace CpuUtilMgr
{
    CpuUtil cpuUtil;
};

CpuUtil* CpuUtilMgr::GetCpuUtil()
{
    return &cpuUtil;
}

CpuUtil::CpuUtil()
{
    m_ClockHandle = 0;
}

CpuUtil::~CpuUtil() { }
RC CpuUtil::GetNumCores(UINT32 *pNumCores) { *pNumCores = 1; return OK; }
RC CpuUtil::SetCpuMode(const string& PowerMode, const UINT32 CoreId)
{
    return OK;
}
RC CpuUtil::GetCpuMode(string& PowerMode) { PowerMode = "G"; return OK; }
RC CpuUtil::GetPhysCoreIds(vector<UINT32> *pPhysCoreIds) { return OK; }
RC CpuUtil::GetCpuType(UINT32 CoreId, string* pCpuType) { return OK; }
RC CpuUtil::EnableCpuCore(const UINT32 CoreId, const bool IsOnline)
{
    return OK;
}
RC CpuUtil::IsClDvfsOn(bool& State) { State = true; return OK; }
RC CpuUtil::SetMaxFreq(const LwU64 MaxFreq, const bool IsClDvfsCapable)
{
    return OK;
}
RC CpuUtil::SetCoreMaxFreq(const UINT32 CoreId) { return OK; }
RC CpuUtil::GetFreq(LwU64* Freq) { *Freq = 0; return OK; }
RC CpuUtil::GetCoreFreq(const UINT32 CoreId, LwU64* Freq) { return OK; }
RC CpuUtil::GetMonitoredFreq(UINT64* freq) { return OK; }
RC CpuUtil::SetFreq(const LwU64 Freq, const UINT32 CoreId, const UINT32 SamplesCount, const UINT32 StabilizeTime)
{
    return OK;
}
RC CpuUtil::SetUseDfll(UINT32 Val) { return OK; }
RC CpuUtil::GetUseDfll(UINT32* Val) { *Val = 0; return OK; }
RC CpuUtil::SetMaxVolt(const UINT32 Volt, string Component)
{
    return OK;
}
RC CpuUtil::SetMilwolt(const UINT32 Volt, string Component)
{
    return OK;
}

RC CpuUtil::GetVolt(UINT32* Volt) { *Volt = 0; return OK; }
RC CpuUtil::GetComponentVolt(string Component, UINT32* Volt)
{
    *Volt = 0;
    return OK;
}
RC CpuUtil::SetVolt(const UINT32 Volt, string Component) { return OK; }
RC CpuUtil::EnableCpuDvfs(bool IsEnable) { return OK; }
RC CpuUtil::EnableOpenLoop(bool IsEnable) { return OK; }
RC CpuUtil::EnableCores(const vector<bool> &CoresUsed, bool Enable)
{
    return OK;
}
RC CpuUtil::GetFreqGovernor(const UINT32 CoreId, string *pLwrrGov)
{
    *pLwrrGov = "userspace";
    return OK;
}
RC CpuUtil::DisableCpuFreqScaling(bool IsDisable, string OrigGov)
{
    return OK;
}
RC CpuUtil::IsDfllOpenLoop(bool &State) { State = false; return OK; }
RC CpuUtil::IsBigClusterOn(bool& IsOn) { IsOn = false; return OK; }
RC CpuUtil::IsLittleClusterOn(bool& IsOn) { IsOn = false; return OK; }
RC CpuUtil::IsCoreOnline(UINT32 CoreId, bool *pIsOnline)
{
    *pIsOnline = true;
    return OK;
}
RC CpuUtil::DisablePowerStates(bool IsDisable) { return OK; }
RC CpuUtil::SetClusterNode(const string& Node, const string& Value)
{
    return OK;
}
RC CpuUtil::SwitchLPMode() { return OK; }
RC CpuUtil::SwitchGMode() { return OK; }
RC CpuUtil::SaveOrigCPUVolt() {return OK;}
RC CpuUtil::GetOrigCPUVolt(UINT32 *pVolt)
{
    *pVolt = 0;
    return OK;
}
