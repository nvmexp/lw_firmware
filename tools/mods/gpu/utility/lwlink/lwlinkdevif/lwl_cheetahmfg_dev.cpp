/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_tegramfg_dev.h"

#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwlregs.h"
#include "lwl_devif.h"
#include "lwl_topology_mgr.h"

//------------------------------------------------------------------------------
//! \brief Constructor (real device)
//!
LwLinkDevIf::LwTegraMfgDev::LwTegraMfgDev
(
    LibInterface::LibDeviceHandle handle
)
 :  TestDevice(Id(0, 0xffff, 0xffff, 0xffff))
   ,m_LibHandle(handle)
{
}

//-----------------------------------------------------------------------------
UINT32 LwLinkDevIf::LwTegraMfgDev::BusType()
{
    return 0;
}

//------------------------------------------------------------------------------
/* virtual */ string LwLinkDevIf::LwTegraMfgDev::DeviceIdString() const
{
    return "T194MFGLWL";
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetAteDvddIddq(UINT32 *pIddq)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetAteIddq(UINT32 *pIddq, UINT32 *pVersion)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetAteRev(UINT32 *pAteRev)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetAteSpeedos
(
    vector<UINT32> *pValues
   ,UINT32 * pVersion
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetChipSkuModifier(string *pStr)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetChipSkuNumber(string *pStr)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetChipPrivateRevision(UINT32 *pPrivRev)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetChipRevision(UINT32 *pRev)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetChipTemps(vector<FLOAT32> *pTempsC)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetClockMHz(UINT32 *pClkMhz)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LwTegraMfgDev::GetDeviceErrorList(vector<DeviceError>* pErrorList)
{
    MASSERT(pErrorList);
    pErrorList->clear();
    return OK;
}

//-----------------------------------------------------------------------------
Device::LwDeviceId LwLinkDevIf::LwTegraMfgDev::GetDeviceId() const
{
    return T194MFGLWL;
}

//------------------------------------------------------------------------------
//! \brief Get the ECID of the gpu device
//!
//! \return string representing the ECID
//!
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetEcidString(string *pStr)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetFoundry(ChipFoundry *pFoundry)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
/* virtual */ LwLinkDevIf::LibIfPtr LwLinkDevIf::LwTegraMfgDev::GetLibIf() const
{
    return LwLinkDevIf::GetTegraLibIf();
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetRawEcid(vector<UINT32> *pEcids, bool bReverseFields)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::GetVoltageMv(UINT32 *pMv)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::LwTegraMfgDev::HotReset(FundamentalResetOptions funResetOption)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//! \brief Initialize the device
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::Initialize()
{
    RC rc;

    if (m_Initialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : LwLink device %s already initialized, skipping\n",
               __FUNCTION__, GetName().c_str());
        return OK;
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Initializing LwLink device %s\n",
           __FUNCTION__, GetName().c_str());

    CHECK_RC(TestDevice::Initialize());

    CHECK_RC(Pcie::Initialize(m_LibHandle));

    CHECK_RC(LwLink::Initialize(m_LibHandle));

    m_Initialized = true;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Print the lwlink device identifier information
/* virtual */ void LwLinkDevIf::LwTegraMfgDev::Print
(
    Tee::Priority pri,
    UINT32 code,
    string prefix
) const
{
    Printf(pri, code, "%s%s\n", prefix.c_str(), GetName().c_str());
}


//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::LwTegraMfgDev::RegRd32(UINT32 offset) const
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
    return 0;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::LwTegraMfgDev::RegRd32
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset
)
{
    auto pLwLinkRegs = TestDevice::GetInterface<LwLinkRegs>();
    UINT32 regVal = 0;
    pLwLinkRegs->RegRd(domIdx, domain, offset, &regVal);
    return regVal;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWr32(UINT32 offset, UINT32 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWr32
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT32        data
)
{
    auto pLwLinkRegs = TestDevice::GetInterface<LwLinkRegs>();
    pLwLinkRegs->RegWr(domIdx, domain, offset, data);
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWrBroadcast32
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT32        data
)
{
    auto pLwLinkRegs = TestDevice::GetInterface<LwLinkRegs>();
    pLwLinkRegs->RegWrBroadcast(domIdx, domain, offset, data);
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWrSync32(UINT32 offset, UINT32 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWrSync32
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT32        data
)
{
    MASSERT(!"RegWrSync32 not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
UINT64 LwLinkDevIf::LwTegraMfgDev::RegRd64(UINT32 offset) const
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
    return 0;
}

//------------------------------------------------------------------------------
UINT64 LwLinkDevIf::LwTegraMfgDev::RegRd64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset
)
{
    MASSERT(!"RegRd64 not supported in TegraMfgDev test devices\n");
    return 0;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWr64(UINT32 offset, UINT64 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWr64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWr64 not supported in TegraMfgDev test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWrBroadcast64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWrBroadcast64 not supported in TegraMfgDev test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWrSync64(UINT32 offset, UINT64 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwTegraMfgDev::RegWrSync64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWrSync64 not supported in TegraMfgDev test devices\n");
}

//------------------------------------------------------------------------------
//! \brief Shutdown the device
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::Shutdown()
{
    if (!m_Initialized)
        return OK;

    StickyRC rc;
    rc = LwLink::Shutdown();

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Shutdown LwLink device %s\n", __FUNCTION__, GetName().c_str());
    m_Initialized = false;

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ string LwLinkDevIf::LwTegraMfgDev::GetName() const
{
    string devTypeInstStr = "";
    if (GetDeviceTypeInstance() != ~0U)
        devTypeInstStr = Utility::StrPrintf("%u ", GetDeviceTypeInstance());

    string topologyStr = "";
    auto pLwLink = TestDevice::GetInterface<LwLink>();
    if (pLwLink && pLwLink->GetTopologyId() != ~0U)
        topologyStr = Utility::StrPrintf("(%u) ", pLwLink->GetTopologyId());

    string domain = "";
    return Utility::StrPrintf("LwTegraMfg %s%s[%s]",
                              devTypeInstStr.c_str(),
                              topologyStr.c_str(),
                              GetId().GetString().c_str());
}

//------------------------------------------------------------------------------
//! \brief Pass-thru mechanism to get access to the JTag chains.
//!
//! \param chiplet     : The chiplet this chain is located on
//! \param instruction : Instruction id used to access this chain via JTag
//! \param chainLength : Total number of bits in this chain
//! \param pResult     : Read result
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::ReadHost2Jtag
(
    UINT32 chiplet,
    UINT32 instruction,
    UINT32 chainLength,
    vector<UINT32> *pResult
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
//! \brief Pass-thru mechanism to get access to the JTag chains.
//!
//! \param chiplet     : The chiplet this chain is located on
//! \param instruction : Instruction id used to access this chain via JTag
//! \param chainLength : Total number of bits in this chain
//! \param inputData   : Data to write
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwTegraMfgDev::WriteHost2Jtag
(
    UINT32 chiplet,
    UINT32 instruction,
    UINT32 chainLength,
    const vector<UINT32> &inputData
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
