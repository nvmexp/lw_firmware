/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014,2016-2018 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "gpu/utility/smbec.h"

SmbEC::SmbEC(){}
RC SmbEC::EnterGC6Power(UINT32 * pInOut) {return RC::SOFTWARE_ERROR;}
RC SmbEC::GetPowerStatus(UINT32 * pInOut){return RC::SOFTWARE_ERROR;}
RC SmbEC::InitializeSmbEC()              {return RC::SOFTWARE_ERROR;}
RC SmbEC::InitializeSmbusDevice(GpuSubdevice *pSubdev){return RC::SOFTWARE_ERROR;}
RC SmbEC::ExitGC6Power(UINT32 * pInOut)  {return RC::SOFTWARE_ERROR;}
RC SmbEC::EnterRTD3Power() {return RC::SOFTWARE_ERROR;}
RC SmbEC::ExitRTD3Power()  {return RC::SOFTWARE_ERROR;}
RC SmbEC::GetCycleStats(Xp::JTMgr::CycleStats * pStats){return RC::SOFTWARE_ERROR;}
RC SmbEC::SetDebugOptions(Xp::JTMgr::DebugOpts * pOpts){return RC::SOFTWARE_ERROR;}
RC SmbEC::GetDebugOptions(Xp::JTMgr::DebugOpts * pOpts){return RC::SOFTWARE_ERROR;} //$
UINT32 SmbEC::GetCapabilities(){return 0;}
RC SmbEC::SetWakeupEvent(UINT32 eventId){return RC::SOFTWARE_ERROR;}
RC SmbEC::GetLwrrentFsmMode(UINT08* pMode){return RC::SOFTWARE_ERROR;}
RC SmbEC::SetLwrrentFsmMode(UINT08  mode) {return RC::SOFTWARE_ERROR;}
void SmbEC::Cleanup(){return;}
void SmbEC::PrintFirmwareInfo(Tee::Priority pri){return;}
RC SmbEC::DumpStateMachine(Tee::Priority pri){return RC::SOFTWARE_ERROR;}
void SmbEC::FatalEnterGc6Msg(fatalMsg * pMsg){return;}
void SmbEC::FatalExitGc6Msg(fatalMsg * pMsg){return;}
RC SmbEC::ReadReg08(const char * pMsg, UINT32 devAddr, UINT08 reg, UINT08 *pValue,Tee::Priority pri){return RC::SOFTWARE_ERROR;}
RC SmbEC::ReadReg08(const char * pMsg, UINT32 devAddr, UINT08 reg, UINT08 *pValue){return RC::SOFTWARE_ERROR;}
RC SmbEC::WriteReg08(const char * pMsg,UINT32 devAddr, UINT08 reg, UINT08 value){return RC::SOFTWARE_ERROR;}
RC SmbEC::WriteReg16(const char * pMsg,UINT32 devAddr, UINT08 reg, UINT16 value){return RC::SOFTWARE_ERROR;}
const char * SmbEC::GetRegName(UINT08 reg){return "";}
RC SmbEC::PollLinkStatus(PexDevice::LinkPowerState state){return RC::SOFTWARE_ERROR;}
RC SmbEC::EnableRootPort(PexDevice::LinkPowerState state) {return RC::SOFTWARE_ERROR;}
RC SmbEC::DisableRootPort(PexDevice::LinkPowerState state) {return RC::SOFTWARE_ERROR;}
const char * SmbEC::GetCapString(){return "";}
RC SmbEC::SendWakeupEvent(){return RC::SOFTWARE_ERROR;}
RC SmbEC::AssertHpd(bool bAssert){return RC::SOFTWARE_ERROR;}
