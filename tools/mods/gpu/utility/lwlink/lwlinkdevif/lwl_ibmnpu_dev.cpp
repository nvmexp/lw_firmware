/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_ibmnpu_dev.h"
#include "core/include/platform.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/lwlink/ibmnpulwlink.h"
#include "mods_reg_hal.h"
#include "lwl_devif.h"
#include "lwmisc.h"
#include "rm.h"

#define LW_PCI_PROCEDURE_STATUS                    0x84
#define LW_PCI_PROCEDURE_STATUS_RETURN             3:0
#define LW_PCI_PROCEDURE_STATUS_RETURN_SUCCESS     0x0
#define LW_PCI_PROCEDURE_STATUS_RETURN_TRANS_ERR   0x1
#define LW_PCI_PROCEDURE_STATUS_RETURN_PERM_ERR    0x2
#define LW_PCI_PROCEDURE_STATUS_RETURN_ABORTED     0x3
#define LW_PCI_PROCEDURE_STATUS_RETURN_UNSUPPORTED 0x4
#define LW_PCI_PROCEDURE_STATUS_PROGRESS           31:28
#define LW_PCI_PROCEDURE_STATUS_PROGRESS_DONE      0x4
#define LW_PCI_PROCEDURE_STATUS_PROGRESS_RUNNING   0x8
#define LW_PCI_PROCEDURE_CTRL                      0x88
#define LW_PCI_PROCEDURE_CTRL_CMD                  3:0
#define LW_PCI_PROCEDURE_CTRL_CMD_RESET            0xA

#define PCI_PROCEDURE_RETRIES 10

//--------------------------------------------------------------------------
//! \brief Constructor
LwLinkDevIf::IbmNpuDev::IbmNpuDev
(
    LibIfPtr pIbmNpuLibif,
    Id i,
    const vector<LibInterface::LibDeviceHandle> &handles
)
  : TestDevice(i)
  , m_pLibIf(pIbmNpuLibif)
{
    for (size_t ii = 0; ii < handles.size(); ii++)
        m_LibLinkInfo.emplace_back(handles[ii], LwLink::ILWALID_LINK_ID);
}

LwLinkDevIf::LibInterface::LibDeviceHandle LwLinkDevIf::IbmNpuDev::GetLibHandle
(
    UINT32 linkId
) const
{
    MASSERT(linkId < m_LibLinkInfo.size());
    if (linkId >= m_LibLinkInfo.size())
        return Device::ILWALID_LIB_HANDLE;

    return m_LibLinkInfo.at(linkId).first;
}

UINT32 LwLinkDevIf::IbmNpuDev::GetLibLinkId(UINT32 linkId) const
{
    MASSERT(linkId < m_LibLinkInfo.size());
    if (linkId >= m_LibLinkInfo.size())
        return LwLink::ILWALID_LINK_ID;

    return m_LibLinkInfo.at(linkId).second;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::IbmNpuDev::GetDeviceErrorList(vector<DeviceError>* pErrorList)
{
    MASSERT(pErrorList);
    pErrorList->clear();
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ string LwLinkDevIf::IbmNpuDev::GetName() const
{
    string devTypeInstStr = "";
    if (GetDeviceTypeInstance() != ~0U)
        devTypeInstStr = Utility::StrPrintf("%u ", GetDeviceTypeInstance());

    string topologyStr = "";
    auto pLwLink = TestDevice::GetInterface<LwLink>();
    if (pLwLink && pLwLink->GetTopologyId() != ~0U)
        topologyStr = Utility::StrPrintf("(%u) ", pLwLink->GetTopologyId());

    return Utility::StrPrintf("IbmNpu %s%s[%s]",
                              devTypeInstStr.c_str(),
                              topologyStr.c_str(),
                              GetId().GetString().c_str());
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::IbmNpuDev::HotReset(FundamentalResetOptions funResetOption)
{
    RC rc;

    for (UINT32 linkId = 0; linkId < m_LibLinkInfo.size(); linkId++)
    {
        CHECK_RC(RunPciProcedure(linkId, PP_Reset));
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize the device
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::IbmNpuDev::Initialize()
{
    RC rc;

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Initializing LwLink device %s\n",
           __FUNCTION__, GetName().c_str());

    CHECK_RC(TestDevice::Initialize());

    vector<LwLinkDevIf::LibInterface::LibDeviceHandle> handles;
    for (auto info : m_LibLinkInfo)
        handles.push_back(info.first);

    vector< pair<LibInterface::LibDeviceHandle, UINT32> >::iterator pLwrInfo;
    for (pLwrInfo = m_LibLinkInfo.begin(); pLwrInfo != m_LibLinkInfo.end();)
    {
        UINT64 linkMask = 0x0ULL;
        CHECK_RC(GetLibIf()->GetLinkMask(pLwrInfo->first, &linkMask));
        for (INT32 ii = 1; ii < Utility::CountBits(linkMask); ii++)
        {
            pair<LibInterface::LibDeviceHandle, UINT32> dupData = make_pair(pLwrInfo->first, 0);
            pLwrInfo = m_LibLinkInfo.insert(pLwrInfo, dupData);
        }

        INT32 lwrLinkNum = Utility::BitScanForward(linkMask, 0);
        while ((lwrLinkNum != -1) && (pLwrInfo != m_LibLinkInfo.end()))
        {
            pLwrInfo->second = lwrLinkNum;
            pLwrInfo++;
            lwrLinkNum = Utility::BitScanForward(linkMask, lwrLinkNum + 1);
        }
        if (lwrLinkNum != -1)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Unable to assign all link numbers on LwLink device %s!\n",
                   __FUNCTION__, GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    for (auto handle : handles)
    {
        CHECK_RC(GetLibIf()->InitializeDevice(handle));
    }

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Print the lwlink device identifier information
/* virtual */ void LwLinkDevIf::IbmNpuDev::Print
(
    Tee::Priority pri,
    UINT32 code,
    string prefix
) const
{
    Printf(pri, code, "%s%s\n", prefix.c_str(), GetName().c_str());
    for (size_t ii = 0; ii < GetMaxLinks(); ii++)
    {

        UINT32 pciDomain, pciBus, pciDev, pciFunc;
        if (OK == m_pLibIf->GetPciInfo(GetLibHandle(ii),
                                       &pciDomain,
                                       &pciBus,
                                       &pciDev,
                                       &pciFunc))
        {

            Printf(pri, code, "%s  %u : %04x:%02x:%02x.%x, %u\n", prefix.c_str(),
                   static_cast<UINT32>(ii),
                   pciDomain,
                   pciBus,
                   pciDev,
                   pciFunc,
                   GetLibLinkId(ii));
        }
    }
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::IbmNpuDev::RegRd32(UINT32 offset) const
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
    return 0;
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::IbmNpuDev::RegRd32
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
void LwLinkDevIf::IbmNpuDev::RegWr32(UINT32 offset, UINT32 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::IbmNpuDev::RegWr32
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
void LwLinkDevIf::IbmNpuDev::RegWrBroadcast32
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
void LwLinkDevIf::IbmNpuDev::RegWrSync32(UINT32 offset, UINT32 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::IbmNpuDev::RegWrSync32
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
UINT64 LwLinkDevIf::IbmNpuDev::RegRd64(UINT32 offset) const
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
    return 0;
}

//------------------------------------------------------------------------------
UINT64 LwLinkDevIf::IbmNpuDev::RegRd64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset
)
{
    auto pLwLinkRegs = TestDevice::GetInterface<LwLinkRegs>();
    UINT64 regVal = 0;
    pLwLinkRegs->RegRd(domIdx, domain, offset, &regVal);
    return regVal;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::IbmNpuDev::RegWr64(UINT32 offset, UINT64 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::IbmNpuDev::RegWr64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    auto pLwLinkRegs = TestDevice::GetInterface<LwLinkRegs>();
    pLwLinkRegs->RegWr(domIdx, domain, offset, data);
}

//------------------------------------------------------------------------------
void LwLinkDevIf::IbmNpuDev::RegWrBroadcast64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    auto pLwLinkRegs = TestDevice::GetInterface<LwLinkRegs>();
    pLwLinkRegs->RegWrBroadcast(domIdx, domain, offset, data);
}

//------------------------------------------------------------------------------
void LwLinkDevIf::IbmNpuDev::RegWrSync64(UINT32 offset, UINT64 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::IbmNpuDev::RegWrSync64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWrSync64 not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::IbmNpuDev::RunPciProcedure(UINT32 linkId, PciProcedure procedure)
{
    RC rc;

    UINT32 domain, bus, device, function;
    CHECK_RC(GetLibIf()->GetPciInfo(GetLibHandle(linkId), &domain, &bus, &device, &function));

    UINT32 status = 0;
    UINT32 retries = 0;
    do
    {
        if (retries++ == PCI_PROCEDURE_RETRIES)
        {
            Printf(Tee::PriError, "IBMNPU PCI Procedure Timed Out!\n");
            return RC::TIMEOUT_ERROR;
        }
        CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                     LW_PCI_PROCEDURE_STATUS, &status));
    } while (FLD_TEST_DRF(_PCI_PROCEDURE, _STATUS, _PROGRESS, _RUNNING, status));

    UINT32 command = 0;
    switch (procedure)
    {
        case PP_Reset:
            Printf(Tee::PriNormal, "Running PCI reset procedure on IBMNPU %04x:%02x:%02x:%02x\n",
                   domain, bus, device, function);
            command = DRF_DEF(_PCI_PROCEDURE, _CTRL, _CMD, _RESET);
            break;
        default:
            Printf(Tee::PriError, "Invalid IBMNPU PCI Procedure = %d!\n",
                   static_cast<UINT32>(procedure));
            return RC::ILWALID_ARGUMENT;
    }

    UINT32 transientRetries = 0;
    do
    {
        CHECK_RC(Platform::PciWrite32(domain, bus, device, function,
                                      LW_PCI_PROCEDURE_CTRL, command));

        retries = 0;
        do
        {
            if (retries++ == PCI_PROCEDURE_RETRIES)
            {
                Printf(Tee::PriError, "IBMNPU PCI Procedure Timed Out!\n");
                return RC::TIMEOUT_ERROR;
            }
            Platform::PciRead32(domain, bus, device, function,
                                LW_PCI_PROCEDURE_STATUS, &status);
        } while (FLD_TEST_DRF(_PCI_PROCEDURE, _STATUS, _PROGRESS, _RUNNING, status));

        switch (DRF_VAL(_PCI_PROCEDURE, _STATUS, _RETURN, status))
        {
            case LW_PCI_PROCEDURE_STATUS_RETURN_SUCCESS:
                Printf(Tee::PriNormal, "IBMNPU PCI Procedure successful\n");
                return OK;
            case LW_PCI_PROCEDURE_STATUS_RETURN_TRANS_ERR:
                Printf(Tee::PriWarn, "IBMNPU PCI Procedure = %u, Transient Failure\n", command);
                continue;
            case LW_PCI_PROCEDURE_STATUS_RETURN_PERM_ERR:
                Printf(Tee::PriError, "IBMNPU PciProcedure = %u, Permement Failure\n", command);
                return RC::HW_STATUS_ERROR;
            case LW_PCI_PROCEDURE_STATUS_RETURN_ABORTED:
                Printf(Tee::PriError, "IBMNPU PciProcedure = %u, Aborted\n", command);
                return RC::SOFTWARE_ERROR;
            case LW_PCI_PROCEDURE_STATUS_RETURN_UNSUPPORTED:
                Printf(Tee::PriError, "IBMNPU PciProcedure = %u, Unsupported procedure\n", command);
                return RC::UNSUPPORTED_FUNCTION;
            default:
                Printf(Tee::PriError, "IBMNPU PciProcedure = %u, Unknown error\n", command);
                return RC::SOFTWARE_ERROR;
        }
    } while (transientRetries++ < PCI_PROCEDURE_RETRIES);

    Printf(Tee::PriError, "IBMNPU PCI Procedure Timed Out!\n");
    return RC::TIMEOUT_ERROR;
}
