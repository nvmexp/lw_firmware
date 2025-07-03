/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_lwswitch_dev.h"
#include "core/include/gpu.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "ctrl/ctrl2080.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "ctrl_dev_lwswitch.h"
#include "device/interface/lwlink/lwlregs.h"
#include "export_lwswitch.h"
#include "g_lwconfig.h"
#include "gpu/include/pcicfg.h"
#include "gpu/ism/lwswitchism.h"
#include "gpu/lwlink/lwswitchlwlink.h"
#include "gpu/utility/pcie/pexdev.h"
#include "lwl_devif.h"
#include "lwl_topology_mgr.h"
#include "lwl_topology_mgr_impl.h"
#include "lwmisc.h"
#include "rm.h"
#include <map>
#include <boost/range/algorithm/find_if.hpp>

namespace
{
    const UINT32 ATE_IDDQ_MULT = 50;
};

//--------------------------------------------------------------------------
//! \brief Constructor
LwLinkDevIf::LwSwitchDev::LwSwitchDev
(
    LibIfPtr pLwSwitchLibif,
    Id i,
    LibInterface::LibDeviceHandle handle,
    Device::LwDeviceId deviceId
)
  : TestDevice(i, deviceId)
  , m_pLibIf(pLwSwitchLibif)
  , m_LibHandle(handle)
  , m_OverTempCounter(this)
{
}

//-----------------------------------------------------------------------------
LwLinkDevIf::WillowDev::WillowDev
(
    LibIfPtr pLwSwitchLibif,
    Id i,
    LibInterface::LibDeviceHandle handle
) : LwSwitchDev(pLwSwitchLibif, i, handle, Device::SVNP01)
{
    m_pIsm.reset(new LwSwitchIsm(this, LwSwitchIsm::GetTable(Device::SVNP01)));
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::WillowDev::Shutdown()
{
    m_pIsm.reset();
    return LwSwitchDev::Shutdown();
}

//------------------------------------------------------------------------------
/* virtual */ string LwLinkDevIf::LwSwitchDev::GetName() const
{
    string devTypeInstStr = "";
    if (GetDeviceTypeInstance() != ~0U)
        devTypeInstStr = Utility::StrPrintf("%u ", GetDeviceTypeInstance());

    string topologyStr = "";
    auto pLwLink = TestDevice::GetInterface<LwLink>();
    if (pLwLink && pLwLink->GetTopologyId() != ~0U)
        topologyStr = Utility::StrPrintf("(%u) ", pLwLink->GetTopologyId());

    string physicalIdStr = "";
    if (m_PhysicalId != ILWALID_PHYSICAL_ID)
        physicalIdStr = Utility::StrPrintf("[%u] ", m_PhysicalId);

    return Utility::StrPrintf("LwSwitch %s%s%s[%s]",
                              devTypeInstStr.c_str(),
                              topologyStr.c_str(),
                              physicalIdStr.c_str(),
                              GetId().GetString().c_str());
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetAteDvddIddq(UINT32 *pIddq)
{
    MASSERT(pIddq);
    *pIddq = 0;

    RC rc;

    LWSWITCH_GET_INFO params = {};
    params.count = 1;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_IDDQ_DVDD;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    *pIddq = params.info[0];

    if (*pIddq == 0)
        return RC::UNSUPPORTED_FUNCTION;
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetAteIddq(UINT32 *pIddq, UINT32 *pVersion)
{
    MASSERT(pIddq && pVersion);

    *pIddq = 0;
    *pVersion = 0;

    RC rc;

    LWSWITCH_GET_INFO params = {};
    params.count = 2;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_IDDQ;
    params.index[1] = LWSWITCH_GET_INFO_INDEX_IDDQ_REV;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    *pIddq = params.info[0];
    *pVersion = params.info[1];

    if ((*pIddq == 0) && (*pVersion == 0))
        return RC::UNSUPPORTED_FUNCTION;
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetAteRev(UINT32 *pAteRev)
{
    *pAteRev = 0;

    RC rc;

    LWSWITCH_GET_INFO params = {};
    params.count = 1;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_ATE_REV;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    *pAteRev = params.info[0];

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetAteSpeedos
(
    vector<UINT32> *pValues
   ,UINT32 * pVersion
)
{
    MASSERT(pValues && pVersion);
    *pVersion = 0;

    RC rc;
    bool bSpeedoValid = false;

    LWSWITCH_GET_INFO params = {};
    params.count = 3;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_SPEEDO0;
    params.index[1] = LWSWITCH_GET_INFO_INDEX_SPEEDO1;
    params.index[2] = LWSWITCH_GET_INFO_INDEX_SPEEDO_REV;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    UINT32 temp = params.info[0];
    bSpeedoValid = bSpeedoValid || (temp != 0);
    pValues->push_back(temp);

    temp = params.info[1];
    bSpeedoValid = bSpeedoValid || (temp != 0);
    pValues->push_back(temp);

    *pVersion = params.info[2];

    if (!bSpeedoValid)
        return RC::UNSUPPORTED_FUNCTION;
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetChipPrivateRevision(UINT32 *pPrivRev)
{
    MASSERT(pPrivRev);
    RC rc;
    RegHal& regs = GetInterface<LwLinkRegs>()->Regs();

    *pPrivRev = 0;

    LWSWITCH_GET_INFO params = {};
    params.count = 3;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_REVISION_MAJOR;
    params.index[1] = LWSWITCH_GET_INFO_INDEX_REVISION_MINOR;
    params.index[2] = LWSWITCH_GET_INFO_INDEX_REVISION_MINOR_EXT;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    *pPrivRev = (regs.GetField(params.info[0], MODS_PSMC_BOOT_0_MAJOR_REVISION) << 16) |
                (regs.GetField(params.info[1], MODS_PSMC_BOOT_0_MINOR_REVISION) << 8) |
                 regs.GetField(params.info[2], MODS_PSMC_BOOT_2_MINOR_EXTENDED_REVISION);

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetChipRevision(UINT32 *pRev)
{
    MASSERT(pRev);
    RC rc;
    RegHal& regs = GetInterface<LwLinkRegs>()->Regs();

    *pRev = 0;

    LWSWITCH_GET_INFO params = {};
    params.count = 2;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_REVISION_MAJOR;
    params.index[1] = LWSWITCH_GET_INFO_INDEX_REVISION_MINOR;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    *pRev = (regs.GetField(params.info[0], MODS_PSMC_BOOT_0_MAJOR_REVISION) << 4) |
             regs.GetField(params.info[1], MODS_PSMC_BOOT_0_MINOR_REVISION);

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetChipSkuModifier(string *pStr)
{
    // TODO : SKU lookup
    *pStr = "0";
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetChipSkuNumber(string *pStr)
{
    // TODO : SKU lookup
    *pStr = "SVNP01";
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetChipTemps(vector<FLOAT32> *pTempsC)
{
    MASSERT(pTempsC);
    RC rc;

    LWSWITCH_CTRL_GET_TEMPERATURE_PARAMS params = {};
    params.channelMask = BIT(LWSWITCH_THERM_LOCATION_SV10_CENTER);
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_TEMPERATURE,
                                 &params,
                                 sizeof(params)));

    LwTemp temp = params.temperature[LWSWITCH_THERM_LOCATION_SV10_CENTER];
    pTempsC->push_back(Utility::FromLwTemp(temp));

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetClockMHz(UINT32 *pClkMhz)
{
    MASSERT(pClkMhz);
    RC rc;

    *pClkMhz = 0;

    LWSWITCH_GET_INFO params = {};
    params.count = 1;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_FREQ_KHZ;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    *pClkMhz = params.info[0] / 1000;

    return rc;
}

namespace
{
    static_assert(LWSWITCH_ERR_HW_HOST_LAST              == 10008, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_NPORT_INGRESS_LAST     == 11025, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_NPORT_EGRESS_LAST      == 12037, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_NPORT_FSTATE_LAST      == 13009, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_NPORT_TSTATE_LAST      == 14019, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_NPORT_ROUTE_LAST       == 15014, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_NPORT_LAST             == 16004, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_LWLCTRL_LAST           == 17009, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_LWLIPT_LAST            == 18021, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_LWLTLC_LAST            == 19085, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_DLPL_LAST              == 20039, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_AFS_LAST               == 21011, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_MINION_LAST            == 22014, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_NXBAR_LAST             == 23018, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_NPORT_SOURCETRACK_LAST == 24008, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_LWLIPT_LNK_LAST        == 25010, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_SOE_LAST               == 26009, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_HW_CCI_LAST               == 27005, "Need to add support for new lwswitch error codes"); //$
    static_assert(LWSWITCH_ERR_LAST                      == 27006, "Need to add support for new lwswitch error codes"); //$

    map<UINT32, pair<string,string>> s_ErrorStrings =
    {
        // HOST
        { LWSWITCH_ERR_HW_HOST, { ENCJSENT("Host"), "host" } }, //$
        { LWSWITCH_ERR_HW_HOST_PRIV_ERROR, { ENCJSENT("HostPrivError"), "host_priv_error" } }, //$
        { LWSWITCH_ERR_HW_HOST_PRIV_TIMEOUT, { ENCJSENT("HostPrivTimeout"), "host_priv_timeout" } }, //$
        { LWSWITCH_ERR_HW_HOST_UNHANDLED_INTERRUPT, { ENCJSENT("HostUnhandledInterrupt"), "host_unhandled_interrupt" } }, //$
        { LWSWITCH_ERR_HW_HOST_THERMAL_EVENT_START, { ENCJSENT("HostThermalEventStart"), "host_thermal_event_start" } }, //$
        { LWSWITCH_ERR_HW_HOST_THERMAL_EVENT_END, { ENCJSENT("HostThermalEventEnd"), "host_thermal_event_end" } }, //$
        { LWSWITCH_ERR_HW_HOST_THERMAL_SHUTDOWN,{ ENCJSENT("HostThermalShutdown"), "host_thermal_shutdown" } }, //$
        { LWSWITCH_ERR_HW_HOST_IO_FAILURE,{ ENCJSENT("HostIOFailure"), "host_io_failure" } }, //$
        // NPORT: Ingress errors
        { LWSWITCH_ERR_HW_NPORT_INGRESS, { ENCJSENT("NportIngress"), "nport_ingress" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_CMDDECODEERR, { ENCJSENT("NportIngressCmddecodeerr"), "nport_ingress_cmddecodeerr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_BDFMISMATCHERR, { ENCJSENT("NportIngressBdfmismatcherr"), "nport_ingress_bdfmismatcherr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_BUBBLEDETECT, { ENCJSENT("NportIngressBubbledetect"), "nport_ingress_bubbledetect" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_ACLFAIL, { ENCJSENT("NportIngressAclfail"), "nport_ingress_aclfail" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_PKTPOISONSET, { ENCJSENT("NportIngressPktpoisonset"), "nport_ingress_pktpoisonset" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_ECCSOFTLIMITERR, { ENCJSENT("NportIngressEccsoftlimiterr"), "nport_ingress_eccsoftlimiterr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_ECCHDRDOUBLEBITERR, { ENCJSENT("NportIngressEcchdrdoublebiterr"), "nport_ingress_ecchdrdoublebiterr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_ILWALIDCMD, { ENCJSENT("NportIngressIlwalidcmd"), "nport_ingress_ilwalidcmd" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_ILWALIDVCSET, { ENCJSENT("NportIngressIlwalidvcset"), "nport_ingress_ilwalidvcset" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_ERRORINFO, { ENCJSENT("NportIngressErrorinfo"), "nport_ingress_errorinfo" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_REQCONTEXTMISMATCHERR, { ENCJSENT("NportIngressReqcontextmismatcherr"), "nport_ingress_reqcontextmismatcherr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_NCISOC_HDR_ECC_LIMIT_ERR, { ENCJSENT("NportIngressNcisocHdrEccLimitErr"), "nport_ingress_ncisoc_hdr_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_NCISOC_HDR_ECC_DBE_ERR, { ENCJSENT("NportIngressNcisocHdrEccDbeErr"), "nport_ingress_ncisoc_hdr_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_ADDRBOUNDSERR, { ENCJSENT("NportIngressAddrboundserr"), "nport_ingress_addrboundserr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_RIDTABCFGERR, { ENCJSENT("NportIngressRidtabcfgerr"), "nport_ingress_ridtabcfgerr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_RLANTABCFGERR, { ENCJSENT("NportIngressRlantabcfgerr"), "nport_ingress_rlantabcfgerr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_REMAPTAB_ECC_DBE_ERR, { ENCJSENT("NportIngressRemaptabEccDbeErr"), "nport_ingress_remaptab_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_RIDTAB_ECC_DBE_ERR, { ENCJSENT("NportIngressRidtabEccDbeErr"), "nport_ingress_ridtab_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_RLANTAB_ECC_DBE_ERR, { ENCJSENT("NportIngressRlantabEccDbeErr"), "nport_ingress_rlantab_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_NCISOC_PARITY_ERR, { ENCJSENT("NportIngressNcisocParityErr"), "nport_ingress_ncisoc_parity_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_REMAPTAB_ECC_LIMIT_ERR, { ENCJSENT("NportIngressRemaptabEccLimitErr"), "nport_ingress_remaptab_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_RIDTAB_ECC_LIMIT_ERR, { ENCJSENT("NportIngressRidtabEccLimitErr"), "nport_ingress_ridtab_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_RLANTAB_ECC_LIMIT_ERR, { ENCJSENT("NportIngressRlantabEccLimitErr"), "nport_ingress_rlantab_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_INGRESS_ADDRTYPEERR, { ENCJSENT("NportIngressAddrtypeerr"), "nport_ingress_addrtypeerr" } }, //$
        // NPORT: Egress errors
        { LWSWITCH_ERR_HW_NPORT_EGRESS, { ENCJSENT("NportEgress"), "nport_egress" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_EGRESSBUFERR, { ENCJSENT("NportEgressEgressbuferr"), "nport_egress_egressbuferr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_PKTROUTEERR, { ENCJSENT("NportEgressPktrouteerr"), "nport_egress_pktrouteerr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_ECCSINGLEBITLIMITERR0, { ENCJSENT("NportEgressEccsinglebitlimiterr0"), "nport_egress_eccsinglebitlimiterr0" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_ECCHDRDOUBLEBITERR0, { ENCJSENT("NportEgressEcchdrdoublebiterr0"), "nport_egress_ecchdrdoublebiterr0" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_ECCDATADOUBLEBITERR0, { ENCJSENT("NportEgressEccdatadoublebiterr0"), "nport_egress_eccdatadoublebiterr0" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_ECCSINGLEBITLIMITERR1, { ENCJSENT("NportEgressEccsinglebitlimiterr1"), "nport_egress_eccsinglebitlimiterr1" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_ECCHDRDOUBLEBITERR1, { ENCJSENT("NportEgressEcchdrdoublebiterr1"), "nport_egress_ecchdrdoublebiterr1" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_ECCDATADOUBLEBITERR1, { ENCJSENT("NportEgressEccdatadoublebiterr1"), "nport_egress_eccdatadoublebiterr1" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NCISOCHDRCREDITOVFL, { ENCJSENT("NportEgressNcisochdrcreditovfl"), "nport_egress_ncisochdrcreditovfl" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NCISOCDATACREDITOVFL, { ENCJSENT("NportEgressNcisocdatacreditovfl"), "nport_egress_ncisocdatacreditovfl" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_ADDRMATCHERR, { ENCJSENT("NportEgressAddrmatcherr"), "nport_egress_addrmatcherr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_TAGCOUNTERR, { ENCJSENT("NportEgressTagcounterr"), "nport_egress_tagcounterr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_FLUSHRSPERR, { ENCJSENT("NportEgressFlushrsperr"), "nport_egress_flushrsperr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_DROPNPURRSPERR, { ENCJSENT("NportEgressDropnpurrsperr"), "nport_egress_dropnpurrsperr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_POISONERR, { ENCJSENT("NportEgressPoisonerr"), "nport_egress_poisonerr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_PACKET_HEADER, { ENCJSENT("NportEgressPacketHeader"), "nport_egress_packet_header" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_BUFFER_DATA, { ENCJSENT("NportEgressBufferData"), "nport_egress_buffer_data" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NCISOC_CREDITS, { ENCJSENT("NportEgressNcisocCredits"), "nport_egress_ncisoc_credits" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_TAG_DATA, { ENCJSENT("NportEgressTagData"), "nport_egress_tag_data" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_SEQIDERR, { ENCJSENT("NportEgressSeqiderr"), "nport_egress_seqiderr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NXBAR_HDR_ECC_LIMIT_ERR, { ENCJSENT("NportEgressNxbarHdrEccLimitErr"), "nport_egress_nxbar_hdr_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NXBAR_HDR_ECC_DBE_ERR, { ENCJSENT("NportEgressNxbarHdrEccDbeErr"), "nport_egress_nxbar_hdr_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_RAM_OUT_HDR_ECC_LIMIT_ERR, { ENCJSENT("NportEgressRamOutHdrEccLimitErr"), "nport_egress_ram_out_hdr_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_RAM_OUT_HDR_ECC_DBE_ERR, { ENCJSENT("NportEgressRamOutHdrEccDbeErr"), "nport_egress_ram_out_hdr_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NCISOCCREDITOVFL, { ENCJSENT("NportEgressNcisoccreditovfl"), "nport_egress_ncisoccreditovfl" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_REQTGTIDMISMATCHERR, { ENCJSENT("NportEgressReqtgtidmismatcherr"), "nport_egress_reqtgtidmismatcherr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_RSPREQIDMISMATCHERR, { ENCJSENT("NportEgressRspreqidmismatcherr"), "nport_egress_rspreqidmismatcherr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_PRIVRSPERR, { ENCJSENT("NportEgressPrivrsperr"), "nport_egress_privrsperr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_HWRSPERR, { ENCJSENT("NportEgressHwrsperr"), "nport_egress_hwrsperr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NXBAR_HDR_PARITY_ERR, { ENCJSENT("NportEgressNxbarHdrParityErr"), "nport_egress_nxbar_hdr_parity_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NCISOC_CREDIT_PARITY_ERR, { ENCJSENT("NportEgressNcisocCreditParityErr"), "nport_egress_ncisoc_credit_parity_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_NXBAR_FLITTYPE_MISMATCH_ERR, { ENCJSENT("NportEgressNxbarFlittypeMismatchErr"), "nport_egress_nxbar_flittype_mismatch_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_CREDIT_TIME_OUT_ERR, { ENCJSENT("NportEgressCreditTimeOutErr"), "nport_egress_credit_time_out_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_TIMESTAMP_LOG, { ENCJSENT("NportEgressTimestampLog"), "nport_egress_timestamp_log" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_MISC_LOG, { ENCJSENT("NportEgressMiscLog"), "nport_egress_misc_log" } }, //$
        { LWSWITCH_ERR_HW_NPORT_EGRESS_HEADER_LOG, { ENCJSENT("NportEgressHeaderLog"), "nport_egress_header_log" } }, //$
        // NPORT: Fstate errors
        { LWSWITCH_ERR_HW_NPORT_FSTATE, { ENCJSENT("NportFstate"), "nport_fstate" } }, //$
        { LWSWITCH_ERR_HW_NPORT_FSTATE_TAGPOOLBUFERR, { ENCJSENT("NportFstateTagpoolbuferr"), "nport_fstate_tagpoolbuferr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_FSTATE_CRUMBSTOREBUFERR, { ENCJSENT("NportFstateCrumbstorebuferr"), "nport_fstate_crumbstorebuferr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_FSTATE_SINGLEBITECCLIMITERR_CRUMBSTORE, { ENCJSENT("NportFstateSinglebitecclimiterrCrumbstore"), "nport_fstate_singlebitecclimiterr_crumbstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_FSTATE_UNCORRECTABLEECCERR_CRUMBSTORE, { ENCJSENT("NportFstateUncorrectableeccerrCrumbstore"), "nport_fstate_uncorrectableeccerr_crumbstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_FSTATE_SINGLEBITECCLIMITERR_TAGSTORE, { ENCJSENT("NportFstateSinglebitecclimiterrTagstore"), "nport_fstate_singlebitecclimiterr_tagstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_FSTATE_UNCORRECTABLEECCERR_TAGSTORE, { ENCJSENT("NportFstateUncorrectableeccerrTagstore"), "nport_fstate_uncorrectableeccerr_tagstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_FSTATE_SINGLEBITECCLIMITERR_FLUSHREQSTORE, { ENCJSENT("NportFstateSinglebitecclimiterrFlushreqstore"), "nport_fstate_singlebitecclimiterr_flushreqstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_FSTATE_UNCORRECTABLEECCERR_FLUSHREQSTORE, { ENCJSENT("NportFstateUncorrectableeccerrFlushreqstore"), "nport_fstate_uncorrectableeccerr_flushreqstore" } }, //$
        // NPORT: Tstate errors
        { LWSWITCH_ERR_HW_NPORT_TSTATE, { ENCJSENT("NportTstate"), "nport_tstate" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_TAGPOOLBUFERR, { ENCJSENT("NportTstateTagpoolbuferr"), "nport_tstate_tagpoolbuferr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_CRUMBSTOREBUFERR, { ENCJSENT("NportTstateCrumbstorebuferr"), "nport_tstate_crumbstorebuferr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_SINGLEBITECCLIMITERR_CRUMBSTORE, { ENCJSENT("NportTstateSinglebitecclimiterrCrumbstore"), "nport_tstate_singlebitecclimiterr_crumbstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_UNCORRECTABLEECCERR_CRUMBSTORE, { ENCJSENT("NportTstateUncorrectableeccerrCrumbstore"), "nport_tstate_uncorrectableeccerr_crumbstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_SINGLEBITECCLIMITERR_TAGSTORE, { ENCJSENT("NportTstateSinglebitecclimiterrTagstore"), "nport_tstate_singlebitecclimiterr_tagstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_UNCORRECTABLEECCERR_TAGSTORE, { ENCJSENT("NportTstateUncorrectableeccerrTagstore"), "nport_tstate_uncorrectableeccerr_tagstore" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_TAGPOOL_ECC_LIMIT_ERR, { ENCJSENT("NportTstateTagpoolEccLimitErr"), "nport_tstate_tagpool_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_TAGPOOL_ECC_DBE_ERR, { ENCJSENT("NportTstateTagpoolEccDbeErr"), "nport_tstate_tagpool_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_CRUMBSTORE_ECC_LIMIT_ERR, { ENCJSENT("NportTstateCrumbstoreEccLimitErr"), "nport_tstate_crumbstore_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_CRUMBSTORE_ECC_DBE_ERR, { ENCJSENT("NportTstateCrumbstoreEccDbeErr"), "nport_tstate_crumbstore_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_COL_CRUMBSTOREBUFERR, { ENCJSENT("NportTstateColCrumbstorebuferr"), "nport_tstate_col_crumbstorebuferr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_COL_CRUMBSTORE_ECC_LIMIT_ERR, { ENCJSENT("NportTstateColCrumbstoreEccLimitErr"), "nport_tstate_col_crumbstore_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_COL_CRUMBSTORE_ECC_DBE_ERR, { ENCJSENT("NportTstateColCrumbstoreEccDbeErr"), "nport_tstate_col_crumbstore_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_TD_TID_RAMBUFERR, { ENCJSENT("NportTstateTdTidRambuferr"), "nport_tstate_td_tid_rambuferr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_TD_TID_RAM_ECC_LIMIT_ERR, { ENCJSENT("NportTstateTdTidRamEccLimitErr"), "nport_tstate_td_tid_ram_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_TD_TID_RAM_ECC_DBE_ERR, { ENCJSENT("NportTstateTdTidRamEccDbeErr"), "nport_tstate_td_tid_ram_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_ATO_ERR, { ENCJSENT("NportTstateAgingTimeOutErr"), "nport_tstate_ato_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_TSTATE_CAMRSP_ERR, { ENCJSENT("NportTstateCamrspErr"), "nport_tstate_camrsp_err" } }, //$
        // NPORT: Route errors
        { LWSWITCH_ERR_HW_NPORT_ROUTE, { ENCJSENT("NportRoute"), "nport_route" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_ROUTEBUFERR, { ENCJSENT("NportRouteRoutebuferr"), "nport_route_routebuferr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_NOPORTDEFINEDERR, { ENCJSENT("NportRouteNoportdefinederr"), "nport_route_noportdefinederr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_ILWALIDROUTEPOLICYERR, { ENCJSENT("NportRouteIlwalidroutepolicyerr"), "nport_route_ilwalidroutepolicyerr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_ECCLIMITERR, { ENCJSENT("NportRouteEcclimiterr"), "nport_route_ecclimiterr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_UNCORRECTABLEECCERR, { ENCJSENT("NportRouteUncorrectableeccerr"), "nport_route_uncorrectableeccerr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_TRANSDONERESVERR, { ENCJSENT("NportRouteTransdoneresverr"), "nport_route_transdoneresverr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_PACKET_HEADER, { ENCJSENT("NportRoutePacketHeader"), "nport_route_packet_header" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_GLT_ECC_LIMIT_ERR, { ENCJSENT("NportRouteGltEccLimitErr"), "nport_route_glt_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_GLT_ECC_DBE_ERR, { ENCJSENT("NportRouteGltEccDbeErr"), "nport_route_glt_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_PDCTRLPARERR, { ENCJSENT("NportRoutePdctrlparerr"), "nport_route_pdctrlparerr" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_LWS_ECC_LIMIT_ERR, { ENCJSENT("NportRouteLwsEccLimitErr"), "nport_route_lws_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_LWS_ECC_DBE_ERR, { ENCJSENT("NportRouteLwsEccDbeErr"), "nport_route_lws_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_NPORT_ROUTE_CDTPARERR, { ENCJSENT("NportRouteCdtparerr"), "nport_route_cdtparerr" } }, //$
        // NPORT: Nport errors
        { LWSWITCH_ERR_HW_NPORT, { ENCJSENT("Nport"), "nport" } }, //$
        { LWSWITCH_ERR_HW_NPORT_DATAPOISONED, { ENCJSENT("NportDatapoisoned"), "nport_datapoisoned" } }, //$
        { LWSWITCH_ERR_HW_NPORT_UCINTERNAL, { ENCJSENT("NportUcinternal"), "nport_ucinternal" } }, //$
        { LWSWITCH_ERR_HW_NPORT_CINTERNAL, { ENCJSENT("NportCinternal"), "nport_cinternal" } }, //$
        // LWLCTRL
        { LWSWITCH_ERR_HW_LWLCTRL, { ENCJSENT("Lwlctrl"), "lwlctrl" } }, //$
        { LWSWITCH_ERR_HW_LWLCTRL_INGRESSECCSOFTLIMITERR, { ENCJSENT("LwlctrlIngresseccsoftlimiterr"), "lwlctrl_ingresseccsoftlimiterr" } }, //$
        { LWSWITCH_ERR_HW_LWLCTRL_INGRESSECCHDRDOUBLEBITERR, { ENCJSENT("LwlctrlIngressecchdrdoublebiterr"), "lwlctrl_ingressecchdrdoublebiterr" } }, //$
        { LWSWITCH_ERR_HW_LWLCTRL_INGRESSECCDATADOUBLEBITERR, { ENCJSENT("LwlctrlIngresseccdatadoublebiterr"), "lwlctrl_ingresseccdatadoublebiterr" } }, //$
        { LWSWITCH_ERR_HW_LWLCTRL_INGRESSBUFFERERR, { ENCJSENT("LwlctrlIngressbuffererr"), "lwlctrl_ingressbuffererr" } }, //$
        { LWSWITCH_ERR_HW_LWLCTRL_EGRESSECCSOFTLIMITERR, { ENCJSENT("LwlctrlEgresseccsoftlimiterr"), "lwlctrl_egresseccsoftlimiterr" } }, //$
        { LWSWITCH_ERR_HW_LWLCTRL_EGRESSECCHDRDOUBLEBITERR, { ENCJSENT("LwlctrlEgressecchdrdoublebiterr"), "lwlctrl_egressecchdrdoublebiterr" } }, //$
        { LWSWITCH_ERR_HW_LWLCTRL_EGRESSECCDATADOUBLEBITERR, { ENCJSENT("LwlctrlEgresseccdatadoublebiterr"), "lwlctrl_egresseccdatadoublebiterr" } }, //$
        { LWSWITCH_ERR_HW_LWLCTRL_EGRESSBUFFERERR, { ENCJSENT("LwlctrlEgressbuffererr"), "lwlctrl_egressbuffererr" } }, //$
        // Nport: Lwlipt errors
        { LWSWITCH_ERR_HW_LWLIPT, { ENCJSENT("Lwlipt"), "lwlipt" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_DLPROTOCOL, { ENCJSENT("LwliptDlprotocol"), "lwlipt_dlprotocol" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_DATAPOISONED, { ENCJSENT("LwliptDatapoisoned"), "lwlipt_datapoisoned" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_FLOWCONTROL, { ENCJSENT("LwliptFlowcontrol"), "lwlipt_flowcontrol" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_RESPONSETIMEOUT, { ENCJSENT("LwliptResponsetimeout"), "lwlipt_responsetimeout" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_TARGETERROR, { ENCJSENT("LwliptTargeterror"), "lwlipt_targeterror" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_UNEXPECTEDRESPONSE, { ENCJSENT("LwliptUnexpectedresponse"), "lwlipt_unexpectedresponse" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_RECEIVEROVERFLOW, { ENCJSENT("LwliptReceiveroverflow"), "lwlipt_receiveroverflow" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_MALFORMEDPACKET, { ENCJSENT("LwliptMalformedpacket"), "lwlipt_malformedpacket" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_STOMPEDPACKETRECEIVED, { ENCJSENT("LwliptStompedpacketreceived"), "lwlipt_stompedpacketreceived" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_UNSUPPORTEDREQUEST, { ENCJSENT("LwliptUnsupportedrequest"), "lwlipt_unsupportedrequest" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_UCINTERNAL, { ENCJSENT("LwliptUcinternal"), "lwlipt_ucinternal" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_PHYRECEIVER, { ENCJSENT("LwliptPhyreceiver"), "lwlipt_phyreceiver" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_BADAN0PKT, { ENCJSENT("LwliptBadan0pkt"), "lwlipt_badan0pkt" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_REPLAYTIMEOUT, { ENCJSENT("LwliptReplaytimeout"), "lwlipt_replaytimeout" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_ADVISORYERROR, { ENCJSENT("LwliptAdvisoryerror"), "lwlipt_advisoryerror" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_CINTERNAL, { ENCJSENT("LwliptCinternal"), "lwlipt_cinternal" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_HEADEROVERFLOW, { ENCJSENT("LwliptHeaderoverflow"), "lwlipt_headeroverflow" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_RSTSEQ_PHYARB_TIMEOUT,{ ENCJSENT("ResetSequencePhyarbTimeout"), "reset_sequence_phyarb_timeout" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_RSTSEQ_PLL_TIMEOUT,{ ENCJSENT("ResetSequencePllTimeout"), "reset_sequence_pll_timeout" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_CLKCTL_ILLEGAL_REQUEST,{ ENCJSENT("ClkctlIllegalRequest"), "clkctl_illegal_request" } }, //$

        // Nport: Lwltlc TX errors
        { LWSWITCH_ERR_HW_LWLTLC, { ENCJSENT("Lwltlc"), "lwltlc" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXHDRCREDITOVFERR, { ENCJSENT("LwltlcTxhdrcreditovferr"), "lwltlc_txhdrcreditovferr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXDATACREDITOVFERR, { ENCJSENT("LwltlcTxdatacreditovferr"), "lwltlc_txdatacreditovferr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXDLCREDITOVFERR, { ENCJSENT("LwltlcTxdlcreditovferr"), "lwltlc_txdlcreditovferr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXDLCREDITPARITYERR, { ENCJSENT("LwltlcTxdlcreditparityerr"), "lwltlc_txdlcreditparityerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXRAMHDRPARITYERR, { ENCJSENT("LwltlcTxramhdrparityerr"), "lwltlc_txramhdrparityerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXRAMDATAPARITYERR, { ENCJSENT("LwltlcTxramdataparityerr"), "lwltlc_txramdataparityerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXUNSUPVCOVFERR, { ENCJSENT("LwltlcTxunsupvcovferr"), "lwltlc_txunsupvcovferr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXSTOMPDET, { ENCJSENT("LwltlcTxstompdet"), "lwltlc_txstompdet" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TXPOISONDET, { ENCJSENT("LwltlcTxpoisondet"), "lwltlc_txpoisondet" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TARGETERR, { ENCJSENT("LwltlcTargeterr"), "lwltlc_targeterr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_PACKET_HEADER, { ENCJSENT("LwltlcTxPacketHeader"), "lwltlc_tx_packet_header" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_UNSUPPORTEDREQUESTERR, { ENCJSENT("Lwltllwnsupportedrequesterr"), "lwltlc_unsupportedrequesterr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXDLHDRPARITYERR, { ENCJSENT("LwltlcRxdlhdrparityerr"), "lwltlc_rxdlhdrparityerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXDLDATAPARITYERR, { ENCJSENT("LwltlcRxdldataparityerr"), "lwltlc_rxdldataparityerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXDLCTRLPARITYERR, { ENCJSENT("LwltlcRxdlctrlparityerr"), "lwltlc_rxdlctrlparityerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXRAMDATAPARITYERR, { ENCJSENT("LwltlcRxramdataparityerr"), "lwltlc_rxramdataparityerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXRAMHDRPARITYERR, { ENCJSENT("LwltlcRxramhdrparityerr"), "lwltlc_rxramhdrparityerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXILWALIDAEERR, { ENCJSENT("LwltlcRxilwalidaeerr"), "lwltlc_rxilwalidaeerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXILWALIDBEERR, { ENCJSENT("LwltlcRxilwalidbeerr"), "lwltlc_rxilwalidbeerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXILWALIDADDRALIGNERR, { ENCJSENT("LwltlcRxilwalidaddralignerr"), "lwltlc_rxilwalidaddralignerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXPKTLENERR, { ENCJSENT("LwltlcRxpktlenerr"), "lwltlc_rxpktlenerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RSVCMDENCERR, { ENCJSENT("LwltlcRsvcmdencerr"), "lwltlc_rsvcmdencerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RSVDATLENENCERR, { ENCJSENT("LwltlcRsvdatlenencerr"), "lwltlc_rsvdatlenencerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RSVADDRTYPEERR, { ENCJSENT("LwltlcRsvaddrtypeerr"), "lwltlc_rsvaddrtypeerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RSVRSPSTATUSERR, { ENCJSENT("LwltlcRsvrspstatuserr"), "lwltlc_rsvrspstatuserr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RSVPKTSTATUSERR, { ENCJSENT("LwltlcRsvpktstatuserr"), "lwltlc_rsvpktstatuserr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RSVCACHEATTRPROBEREQERR, { ENCJSENT("LwltlcRsvcacheattrprobereqerr"), "lwltlc_rsvcacheattrprobereqerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RSVCACHEATTRPROBERSPERR, { ENCJSENT("LwltlcRsvcacheattrprobersperr"), "lwltlc_rsvcacheattrprobersperr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_DATLENGTATOMICREQMAXERR, { ENCJSENT("LwltlcDatlengtatomicreqmaxerr"), "lwltlc_datlengtatomicreqmaxerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_DATLENGTRMWREQMAXERR, { ENCJSENT("LwltlcDatlengtrmwreqmaxerr"), "lwltlc_datlengtrmwreqmaxerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_DATLENLTATRRSPMINERR, { ENCJSENT("LwltlcDatlenltatrrspminerr"), "lwltlc_datlenltatrrspminerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_ILWALIDCACHEATTRPOERR, { ENCJSENT("LwltlcIlwalidcacheattrpoerr"), "lwltlc_ilwalidcacheattrpoerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_ILWALIDCRERR, { ENCJSENT("LwltlcIlwalidcrerr"), "lwltlc_ilwalidcrerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXRESPSTATUSTARGETERR, { ENCJSENT("LwltlcRxrespstatustargeterr"), "lwltlc_rxrespstatustargeterr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXRESPSTATUSUNSUPPORTEDREQUESTERR, { ENCJSENT("LwltlcRxrespstatusunsupportedrequesterr"), "lwltlc_rxrespstatusunsupportedrequesterr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXHDROVFERR, { ENCJSENT("LwltlcRxhdrovferr"), "lwltlc_rxhdrovferr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXDATAOVFERR, { ENCJSENT("LwltlcRxdataovferr"), "lwltlc_rxdataovferr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_STOMPDETERR, { ENCJSENT("LwltlcStompdeterr"), "lwltlc_stompdeterr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXPOISONERR, { ENCJSENT("LwltlcRxpoisonerr"), "lwltlc_rxpoisonerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_CORRECTABLEINTERNALERR, { ENCJSENT("LwltlcCorrectableinternalerr"), "lwltlc_correctableinternalerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXUNSUPVCOVFERR, { ENCJSENT("LwltlcRxunsupvcovferr"), "lwltlc_rxunsupvcovferr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXUNSUPLWLINKCREDITRELERR, { ENCJSENT("LwltlcRxunsuplwlinkcreditrelerr"), "lwltlc_rxunsuplwlinkcreditrelerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RXUNSUPNCISOCCREDITRELERR, { ENCJSENT("LwltlcRxunsupncisoccreditrelerr"), "lwltlc_rxunsupncisoccreditrelerr" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_PACKET_HEADER, { ENCJSENT("LwltlcRxPacketHeader"), "lwltlc_rx_packet_header" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_ERR_HEADER, { ENCJSENT("LwltlcRxErrHeader"), "lwltlc_rx_err_header" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_SYS_NCISOC_PARITY_ERR, { ENCJSENT("LwltlcTxSysNcisocParityErr"), "lwltlc_tx_sys_ncisoc_parity_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_SYS_NCISOC_HDR_ECC_DBE_ERR, { ENCJSENT("LwltlcTxSysNcisocHdrEccDbeErr"), "lwltlc_tx_sys_ncisoc_hdr_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_SYS_NCISOC_DAT_ECC_DBE_ERR, { ENCJSENT("LwltlcTxSysNcisocDatEccDbeErr"), "lwltlc_tx_sys_ncisoc_dat_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_SYS_NCISOC_ECC_LIMIT_ERR, { ENCJSENT("LwltlcTxSysNcisocEccLimitErr"), "lwltlc_tx_sys_ncisoc_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_SYS_TXRSPSTATUS_HW_ERR, { ENCJSENT("LwltlcTxSysTxrspstatusHwErr"), "lwltlc_tx_sys_txrspstatus_hw_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_SYS_TXRSPSTATUS_UR_ERR, { ENCJSENT("LwltlcTxsysTxrspstatusUrErr"), "lwltlc_tx_sys_txrspstatus_ur_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_SYS_TXRSPSTATUS_PRIV_ERR, { ENCJSENT("LwltlcTxSysTxrspstatusPrivErr"), "lwltlc_tx_sys_txrspstatus_priv_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_SYS_NCISOC_PARITY_ERR, { ENCJSENT("LwltlcRxSysNcisocParityErr"), "lwltlc_rx_sys_ncisoc_parity_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_SYS_HDR_RAM_ECC_DBE_ERR, { ENCJSENT("LwltlcRxSysHdrRamEccDbeErr"), "lwltlc_rx_sys_hdr_ram_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_SYS_HDR_RAM_ECC_LIMIT_ERR, { ENCJSENT("LwltlcRxSysHdrRamEccLimitErr"), "lwltlc_rx_sys_hdr_ram_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_SYS_DAT0_RAM_ECC_DBE_ERR, { ENCJSENT("LwltlcRxSysDat0RamEccDbeErr"), "lwltlc_rx_sys_dat0_ram_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_SYS_DAT0_RAM_ECC_LIMIT_ERR, { ENCJSENT("LwltlcRxSysDat0RamEccLimitErr"), "lwltlc_rx_sys_dat0_ram_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_SYS_DAT1_RAM_ECC_DBE_ERR, { ENCJSENT("LwltlcRxSysDat1RamEccDbeErr"), "lwltlc_rx_sys_dat1_ram_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_SYS_DAT1_RAM_ECC_LIMIT_ERR, { ENCJSENT("LwltlcRxSysDat1RamEccLimitErr"), "lwltlc_rx_sys_dat1_ram_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_CREQ_RAM_HDR_ECC_DBE_ERR, { ENCJSENT("LwltlcTxLnkCreqRamHdrEccDbeErr"), "lwltlc_tx_lnk_creq_ram_hdr_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_CREQ_RAM_DAT_ECC_DBE_ERR, { ENCJSENT("LwltlcTxLnkCreqRamDatEccDbeErr"), "lwltlc_tx_lnk_creq_ram_dat_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_CREQ_RAM_ECC_LIMIT_ERR, { ENCJSENT("LwltlcTxLnkCreqRamEccLimitErr"), "lwltlc_tx_lnk_creq_ram_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_RSP_RAM_HDR_ECC_DBE_ERR, { ENCJSENT("LwltlcTxLnkRspRamHdrEccDbeErr"), "lwltlc_tx_lnk_rsp_ram_hdr_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_RSP_RAM_DAT_ECC_DBE_ERR, { ENCJSENT("LwltlcTxLnkRspRamDatEccDbeErr"), "lwltlc_tx_lnk_rsp_ram_dat_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_RSP_RAM_ECC_LIMIT_ERR, { ENCJSENT("LwltlctxLnkRspRamEccLimitErr"), "lwltlc_tx_lnk_rsp_ram_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_COM_RAM_HDR_ECC_DBE_ERR, { ENCJSENT("LwltlcTxLnkComRamHdrEccDbeErr"), "lwltlc_tx_lnk_com_ram_hdr_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_COM_RAM_DAT_ECC_DBE_ERR, { ENCJSENT("LwltlcTxLnkComRamDatEccDbeErr"), "lwltlc_tx_lnk_com_ram_dat_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_COM_RAM_ECC_LIMIT_ERR, { ENCJSENT("LwltlcTxLnkComRamEccLimitErr"), "lwltlc_tx_lnk_com_ram_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_RSP1_RAM_HDR_ECC_DBE_ERR, { ENCJSENT("LwltlcTxLnkRsp1RamHdrEccDbeErr"), "lwltlc_tx_lnk_rsp1_ram_hdr_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_RSP1_RAM_DAT_ECC_DBE_ERR, { ENCJSENT("LwltlcTxLnkRsp1RamDatEccDbeErr"), "lwltlc_tx_lnk_rsp1_ram_dat_ecc_dbe_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_RSP1_RAM_ECC_LIMIT_ERR, { ENCJSENT("LwltlcTxLnkRsp1RamEccLimitErr"), "lwltlc_tx_lnk_rsp1_ram_ecc_limit_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_AN1_TIMEOUT_VC0, { ENCJSENT("LwltlcTxLnkAn1TimeoutVc0"), "lwltlc_tx_lnk_an1_timeout_vc0" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_AN1_TIMEOUT_VC1, { ENCJSENT("LwltlcTxLnkAn1TimeoutVc1"), "lwltlc_tx_lnk_an1_timeout_vc1" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_AN1_TIMEOUT_VC2, { ENCJSENT("LwltlcTxLnkAn1TimeoutVc2"), "lwltlc_tx_lnk_an1_timeout_vc2" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_AN1_TIMEOUT_VC3, { ENCJSENT("LwltlcTxLnkAn1TimeoutVc3"), "lwltlc_tx_lnk_an1_timeout_vc3" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_AN1_TIMEOUT_VC4, { ENCJSENT("LwltlcTxLnkAn1TimeoutVc4"), "lwltlc_tx_lnk_an1_timeout_vc4" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_AN1_TIMEOUT_VC5, { ENCJSENT("LwltlcTxLnkAn1TimeoutVc5"), "lwltlc_tx_lnk_an1_timeout_vc5" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_AN1_TIMEOUT_VC6, { ENCJSENT("LwltlcTxLnkAn1TimeoutVc6"), "lwltlc_tx_lnk_an1_timeout_vc6" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_TX_LNK_AN1_TIMEOUT_VC7, { ENCJSENT("LwltlcTxLnkAn1TimeoutVc7"), "lwltlc_tx_lnk_an1_timeout_vc7" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_LNK_RXRSPSTATUS_HW_ERR, { ENCJSENT("LwltlcRxLnkRxrspstatusHwErr"), "lwltlc_rx_lnk_rxrspstatus_hw_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_LNK_RXRSPSTATUS_UR_ERR, { ENCJSENT("LwltlcRxLnkRxrspstatusUrErr"), "lwltlc_rx_lnk_rxrspstatus_ur_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_LNK_RXRSPSTATUS_PRIV_ERR, { ENCJSENT("LwltlcRxLnkRxrspstatusPrivErr"), "lwltlc_rx_lnk_rxrspstatus_priv_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_LNK_ILWALID_COLLAPSED_RESPONSE_ERR, { ENCJSENT("LwltlcRxLnkIlwalidCollapsedResponseErr"), "lwltlc_rx_lnk_ilwalid_collapsed_response_err" } }, //$
        { LWSWITCH_ERR_HW_LWLTLC_RX_LNK_AN1_HEARTBEAT_TIMEOUT_ERR, { ENCJSENT("LwltlcRxLnkAn1HeartbeatTimeoutErr"), "lwltlc_rx_lnk_an1_heartbeat_timeout_err" } }, //$
        // DLPL: errors
        { LWSWITCH_ERR_HW_DLPL, { ENCJSENT("Dlpl"), "dlpl" } }, //$
        { LWSWITCH_ERR_HW_DLPL_TX_REPLAY, { ENCJSENT("DlplTxReplay"), "dlpl_tx_replay" } }, //$
        { LWSWITCH_ERR_HW_DLPL_TX_RECOVERY_SHORT, { ENCJSENT("DlplTxRecoveryShort"), "dlpl_tx_recovery_short" } }, //$
        { LWSWITCH_ERR_HW_DLPL_TX_RECOVERY_LONG, { ENCJSENT("DlplTxRecoveryLong"), "dlpl_tx_recovery_long" } }, //$
        { LWSWITCH_ERR_HW_DLPL_TX_FAULT_RAM, { ENCJSENT("DlplTxFaultRam"), "dlpl_tx_fault_ram" } }, //$
        { LWSWITCH_ERR_HW_DLPL_TX_FAULT_INTERFACE, { ENCJSENT("DlplTxFaultInterface"), "dlpl_tx_fault_interface" } }, //$
        { LWSWITCH_ERR_HW_DLPL_TX_FAULT_SUBLINK_CHANGE, { ENCJSENT("DlplTxFaultSublinkChange"), "dlpl_tx_fault_sublink_change" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RX_FAULT_SUBLINK_CHANGE, { ENCJSENT("DlplRxFaultSublinkChange"), "dlpl_rx_fault_sublink_change" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RX_FAULT_DL_PROTOCOL, { ENCJSENT("DlplRxFaultDlProtocol"), "dlpl_rx_fault_dl_protocol" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RX_SHORT_ERROR_RATE, { ENCJSENT("DlplRxShortErrorRate"), "dlpl_rx_short_error_rate" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RX_LONG_ERROR_RATE, { ENCJSENT("DlplRxLongErrorRate"), "dlpl_rx_long_error_rate" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RX_ILA_TRIGGER, { ENCJSENT("DlplRxIlaTrigger"), "dlpl_rx_ila_trigger" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RX_CRC_COUNTER, { ENCJSENT("DlplRxCrcCounter"), "dlpl_rx_crc_counter" } }, //$
        { LWSWITCH_ERR_HW_DLPL_LTSSM_FAULT, { ENCJSENT("DlplLtssmFault"), "dlpl_ltssm_fault" } }, //$
        { LWSWITCH_ERR_HW_DLPL_LTSSM_PROTOCOL, { ENCJSENT("DlplLtssmProtocol"), "dlpl_ltssm_protocol" } }, //$
        { LWSWITCH_ERR_HW_DLPL_MINION_REQUEST, { ENCJSENT("DlplMinionRequest"), "dlpl_minion_request" } }, //$
        { LWSWITCH_ERR_HW_DLPL_FIFO_DRAIN_ERR, { ENCJSENT("DlplFifoDrainErr"), "dlpl_fifo_drain_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_CONST_DET_ERR, { ENCJSENT("DlplConstDetErr"), "dlpl_const_det_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_OFF2SAFE_LINK_DET_ERR, { ENCJSENT("DlplOff2safeLinkDetErr"), "dlpl_off2safe_link_det_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_SAFE2NO_LINK_DET_ERR, { ENCJSENT("DlplSafe2noLinkDetErr"), "dlpl_safe2no_link_det_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_SCRAM_LOCK_ERR, { ENCJSENT("DlplScramLockErr"), "dlpl_scram_lock_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_SYM_LOCK_ERR, { ENCJSENT("DlplSymLockErr"), "dlpl_sym_lock_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_SYM_ALIGN_END_ERR, { ENCJSENT("DlplSymAlignEndErr"), "dlpl_sym_align_end_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_FIFO_SKEW_ERR, { ENCJSENT("DlplFifoSkewErr"), "dlpl_fifo_skew_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_TRAIN2SAFE_LINK_DET_ERR, { ENCJSENT("DlplTrain2safeLinkDetErr"), "dlpl_train2safe_link_det_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_HS2SAFE_LINK_DET_ERR, { ENCJSENT("DlplHs2safeLinkDetErr"), "dlpl_hs2safe_link_det_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_FENCE_ERR, { ENCJSENT("DlplFenceErr"), "dlpl_fence_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_SAFE_NO_LD_ERR, { ENCJSENT("DlplSafeNoLdErr"), "dlpl_safe_no_ld_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_E2SAFE_LD_ERR, { ENCJSENT("DlplE2safeLdErr"), "dlpl_e2safe_ld_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RC_RXPWR_ERR, { ENCJSENT("DlplRcRxpwrErr"), "dlpl_rc_rxpwr_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RC_TXPWR_ERR, { ENCJSENT("DlplRcTxpwrErr"), "dlpl_rc_txpwr_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RC_DEADLINE_ERR, { ENCJSENT("DlplRcDeadlineErr"), "dlpl_rc_deadline_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_TX_HS2LP_ERR, { ENCJSENT("DlplTxHs2lpErr"), "dlpl_tx_hs2lp_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_RX_HS2LP_ERR, { ENCJSENT("DlplRxHs2lpErr"), "dlpl_rx_hs2lp_err" } }, //$
        { LWSWITCH_ERR_HW_DLPL_LTSSM_FAULT_DOWN, { ENCJSENT("DlplLtssmFaultDown"), "dlpl_ltssm_fault_down" } }, //$
        { LWSWITCH_ERR_HW_DLPL_LTSSM_FAULT_UP, { ENCJSENT("DlplLtssmFaultUp"), "dlpl_ltssm_fault_up" } }, //$

        // AFS: errors
        { LWSWITCH_ERR_HW_AFS, { ENCJSENT("Afs"), "afs" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_INGRESS_CREDIT_OVERFLOW, { ENCJSENT("AfsUcIngressCreditOverflow"), "afs_uc_ingress_credit_overflow" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_INGRESS_CREDIT_UNDERFLOW, { ENCJSENT("AfsUcIngressCreditUnderflow"), "afs_uc_ingress_credit_underflow" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_EGRESS_CREDIT_OVERFLOW, { ENCJSENT("AfsUcEgressCreditOverflow"), "afs_uc_egress_credit_overflow" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_EGRESS_CREDIT_UNDERFLOW, { ENCJSENT("AfsUcEgressCreditUnderflow"), "afs_uc_egress_credit_underflow" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_INGRESS_NON_BURSTY_PKT_DETECTED, { ENCJSENT("AfsUcIngressNonBurstyPktDetected"), "afs_uc_ingress_non_bursty_pkt_detected" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_INGRESS_NON_STICKY_PKT_DETECTED, { ENCJSENT("AfsUcIngressNonStickyPktDetected"), "afs_uc_ingress_non_sticky_pkt_detected" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_INGRESS_BURST_GT_17_DATA_VC_DETECTED, { ENCJSENT("AfsUcIngressBurstGt17DataVcDetected"), "afs_uc_ingress_burst_gt_17_data_vc_detected" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_INGRESS_BURST_GT_1_NONDATA_VC_DETECTED, { ENCJSENT("AfsUcIngressBurstGt1NondataVcDetected"), "afs_uc_ingress_burst_gt_1_nondata_vc_detected" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_ILWALID_DST, { ENCJSENT("AfsUcIlwalidDst"), "afs_uc_ilwalid_dst" } }, //$
        { LWSWITCH_ERR_HW_AFS_UC_PKT_MISROUTE, { ENCJSENT("AfsUcPktMisroute"), "afs_uc_pkt_misroute" } }, //$
        // MINION: errors
        { LWSWITCH_ERR_HW_MINION, { ENCJSENT("Minion"), "minion" } }, //$
        { LWSWITCH_ERR_HW_MINION_UCODE_IMEM, { ENCJSENT("MinionUcodeImem"), "minion_ucode_imem" } }, //$
        { LWSWITCH_ERR_HW_MINION_UCODE_DMEM, { ENCJSENT("MinionUcodeDmem"), "minion_ucode_dmem" } }, //$
        { LWSWITCH_ERR_HW_MINION_HALT, { ENCJSENT("MinionHalt"), "minion_halt" } }, //$
        { LWSWITCH_ERR_HW_MINION_BOOT_ERROR, { ENCJSENT("MinionBootError"), "minion_boot_error" } }, //$
        { LWSWITCH_ERR_HW_MINION_TIMEOUT, { ENCJSENT("MinionTimeout"), "minion_timeout" } }, //$
        { LWSWITCH_ERR_HW_MINION_DLCMD_FAULT, { ENCJSENT("MinionDlcmdFault"), "minion_dlcmd_fault" } }, //$
        { LWSWITCH_ERR_HW_MINION_DLCMD_TIMEOUT, { ENCJSENT("MinionDlcmdTimeout"), "minion_dlcmd_timeout" } }, //$
        { LWSWITCH_ERR_HW_MINION_DLCMD_FAIL, { ENCJSENT("MinionDlcmdFail"), "minion_dlcmd_fail" } }, //$
        { LWSWITCH_ERR_HW_MINION_FATAL_INTR, { ENCJSENT("MinionFatalIntr"), "minion_fatal_intr" } }, //$
        { LWSWITCH_ERR_HW_MINION_WATCHDOG, { ENCJSENT("MinionWatchdog"), "minion_watchdog" } }, //$
        { LWSWITCH_ERR_HW_MINION_EXTERR, { ENCJSENT("MinionExterr"), "minion_exterr" } }, //$
        { LWSWITCH_ERR_HW_MINION_FATAL_LINK_INTR, { ENCJSENT("MinionFatalLinkIntr"), "minion_fatal_link_intr" } }, //$
        { LWSWITCH_ERR_HW_MINION_NONFATAL, { ENCJSENT("MinionNonfatal"), "minion_nonfatal" } }, //$
        // NXBAR: errors
        { LWSWITCH_ERR_HW_NXBAR,{ ENCJSENT("Nxbar"), "nxbar" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_INGRESS_BUFFER_OVERFLOW, { ENCJSENT("IngressBufferOverflow"), "ingress_buffer_overflow" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_INGRESS_BUFFER_UNDERFLOW, { ENCJSENT("IngressBufferUnderflow"), "ingress_buffer_underflow" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_EGRESS_CREDIT_OVERFLOW, { ENCJSENT("EgressCreditOverflow"), "egress_credit_overflow" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_EGRESS_CREDIT_UNDERFLOW, { ENCJSENT("EgressCreditUnderflow"), "egress_credit_underflow" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_INGRESS_NON_BURSTY_PKT, { ENCJSENT("IngressNonBurstyPkt"), "ingress_non_bursty_pkt" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_INGRESS_NON_STICKY_PKT, { ENCJSENT("IngressNonStickyPkt"), "ingress_non_sticky_pkt" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_INGRESS_BURST_GT_9_DATA_VC, { ENCJSENT("IngressBurstGt9DataVc"), "ingress_burst_gt_9_data_vc" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_INGRESS_PKT_ILWALID_DST, { ENCJSENT("IngressPktIlwalidDst"), "ingress_pkt_ilwalid_dst" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILE_INGRESS_PKT_PARITY_ERROR, { ENCJSENT("IngressPktParityError"), "ingress_pkt_parity_error" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILEOUT_INGRESS_BUFFER_OVERFLOW, { ENCJSENT("IngressBufferOverflow"), "ingress_buffer_overflow" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILEOUT_INGRESS_BUFFER_UNDERFLOW, { ENCJSENT("IngressBufferUnderflow"), "ingress_buffer_underflow" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILEOUT_EGRESS_CREDIT_OVERFLOW, { ENCJSENT("EgressCreditOverflow"), "egress_credit_overflow" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILEOUT_EGRESS_CREDIT_UNDERFLOW, { ENCJSENT("EgressCreditUnderflow"), "egress_credit_underflow" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILEOUT_INGRESS_NON_BURSTY_PKT, { ENCJSENT("IngressNonBurstyPkt"), "ingress_non_bursty_pkt" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILEOUT_INGRESS_NON_STICKY_PKT, { ENCJSENT("IngressNonStickyPkt"), "ingress_non_sticky_pkt" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILEOUT_INGRESS_BURST_GT_9_DATA_VC, { ENCJSENT("IngressBurstGt9DataVc"), "ingress_burst_gt_9_data_vc" } }, //$
        { LWSWITCH_ERR_HW_NXBAR_TILEOUT_EGRESS_CDT_PARITY_ERROR, { ENCJSENT("EgressCdtParityError"), "egress_cdt_parity_error" } }, //$
        // NPORT: Sourcetrack errors
        { LWSWITCH_ERR_HW_NPORT_SOURCETRACK,{ ENCJSENT("Sourcetrack"), "sourcetrack" } }, //$
        { LWSWITCH_ERR_HW_NPORT_SOURCETRACK_CREQ_TCEN0_CRUMBSTORE_ECC_LIMIT_ERR,{ ENCJSENT("SourcetrackTcen0CrubmstoreEccLimitError"), "sourcetrack_tcen0_crubmstore_ecc_limit_error" } }, //$
        { LWSWITCH_ERR_HW_NPORT_SOURCETRACK_CREQ_TCEN0_TD_CRUMBSTORE_ECC_LIMIT_ERR,{ ENCJSENT("SourcetrackTcen0TdCrubmstoreEccLimitError"), "sourcetrack_tcen0_td_crubmstore_ecc_limit_error" } }, //$
        { LWSWITCH_ERR_HW_NPORT_SOURCETRACK_CREQ_TCEN1_CRUMBSTORE_ECC_LIMIT_ERR,{ ENCJSENT("SourcetrackTcen1CrubmstoreEccLimitError"), "sourcetrack_tcen1_crubmstore_ecc_limit_error" } }, //$
        { LWSWITCH_ERR_HW_NPORT_SOURCETRACK_CREQ_TCEN0_CRUMBSTORE_ECC_DBE_ERR,{ ENCJSENT("SourcetrackTcen0CrubmstoreEccDbeError"), "sourcetrack_tcen0_crubmstore_ecc_dbe_error" } }, //$
        { LWSWITCH_ERR_HW_NPORT_SOURCETRACK_CREQ_TCEN0_TD_CRUMBSTORE_ECC_DBE_ERR,{ ENCJSENT("SourcetrackTcen0TdCrubmstoreEccDbeError"), "sourcetrack_tcen0_td_crubmstore_ecc_dbe_error" } }, //$
        { LWSWITCH_ERR_HW_NPORT_SOURCETRACK_CREQ_TCEN1_CRUMBSTORE_ECC_DBE_ERR,{ ENCJSENT("SourcetrackTcen1CrubmstoreEccDbeError"), "sourcetrack_tcen1_crubmstore_ecc_dbe_error" } }, //$
        { LWSWITCH_ERR_HW_NPORT_SOURCETRACK_SOURCETRACK_TIME_OUT_ERR,{ ENCJSENT("SourcetrackTimeoutError"), "sourcetrack_timeout_error" } }, //$
        // LWLIPT: LNK errors */
        { LWSWITCH_ERR_HW_LWLIPT_LNK,{ ENCJSENT("LwliptLnk"), "lwlipt_lnk" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_ILLEGALLINKSTATEREQUEST,{ ENCJSENT("LwliptLnkIllegalLinkStateRequest"), "lwlipt_lnk_illegal_link_state_request" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_FAILEDMINIONREQUEST,{ ENCJSENT("LwliptLnkFailedMinionRequest"), "lwlipt_lnk_failed_minion_request" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_RESERVEDREQUESTVALUE,{ ENCJSENT("LwliptLnkReservedRequestValue"), "lwlipt_lnk_reserved_request_value" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_LINKSTATEWRITEWHILEBUSY,{ ENCJSENT("LwliptLnkLinkStateWriteWhileBusy"), "lwlipt_lnk_link_state_write_while_busy" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_LINK_STATE_REQUEST_TIMEOUT,{ ENCJSENT("LwliptLnkLinkStateRequestTimeout"), "lwlipt_lnk_link_state_request_timeout" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_WRITE_TO_LOCKED_SYSTEM_REG_ERR,{ ENCJSENT("LwliptLnkWriteToLockedSystemRegErr"), "lwlipt_lnk_write_to_locked_system_reg_err" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_SLEEPWHILEACTIVELINK,{ ENCJSENT("LwliptLnkSleepWhileActiveLink"), "lwlipt_lnk_sleep_while_active_link" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_RSTSEQ_PHYCTL_TIMEOUT,{ ENCJSENT("LwliptLnkRstseqPhyctlTimeout"), "lwlipt_lnk_rstseq_phyctl_timeout" } }, //$
        { LWSWITCH_ERR_HW_LWLIPT_LNK_RSTSEQ_CLKCTL_TIMEOUT,{ ENCJSENT("LwliptLnkRstseqClkctlTimeout"), "lwlipt_lnk_rstseq_clkctl_timeout" } }, //$

        // SOE: errors                                                                                                                   // LWLIPT: LNK errors */
        { LWSWITCH_ERR_HW_SOE,{ ENCJSENT("Soe"), "soe" } }, //$
        { LWSWITCH_ERR_HW_SOE_RESET, { ENCJSENT("SoeReset"), "soe_reset" } }, //$
        { LWSWITCH_ERR_HW_SOE_BOOTSTRAP, { ENCJSENT("SoeBootstrap"), "soe_bootstrap" } }, //$
        { LWSWITCH_ERR_HW_SOE_COMMAND_QUEUE, { ENCJSENT("SoeCommandQueue"), "soe_command_queue" } }, //$
        { LWSWITCH_ERR_HW_SOE_TIMEOUT, { ENCJSENT("SoeTimeout"), "soe_timeout" } }, //$
        { LWSWITCH_ERR_HW_SOE_SHUTDOWN, { ENCJSENT("SoeShutdown"), "soe_shutdown" } }, //$
        { LWSWITCH_ERR_HW_SOE_HALT, { ENCJSENT("SoeHalt"), "soe_halt" } }, //$
        { LWSWITCH_ERR_HW_SOE_EXTERR, { ENCJSENT("SoeExterr"), "soe_exterr" } }, //$
        { LWSWITCH_ERR_HW_SOE_WATCHDOG, { ENCJSENT("SoeWatchdog"), "soe_watchdog" } }, //$

        // CCI: errors                                                                                                                   // CCI errors */
        { LWSWITCH_ERR_HW_CCI,{ ENCJSENT("Cci"), "cci" } }, //$
        { LWSWITCH_ERR_HW_CCI_RESET, { ENCJSENT("CciReset"), "cci_reset" } }, //$
        { LWSWITCH_ERR_HW_CCI_TIMEOUT, { ENCJSENT("CciTimeout"), "cci_timeout" } }, //$
        { LWSWITCH_ERR_HW_CCI_SHUTDOWN, { ENCJSENT("CciShutdown"), "cci_shutdown" } }, //$
    };
}

RC LwLinkDevIf::LwSwitchDev::GetDeviceErrorList(vector<DeviceError>* pErrorList)
{
    MASSERT(pErrorList);
    RC rc;

    pErrorList->clear();
    LWSWITCH_GET_ERRORS_PARAMS params = {};
    params.errorType = LWSWITCH_ERROR_SEVERITY_FATAL;

    do
    {
        params.errorCount = 0;
        params.errorIndex = m_FatalErrorIndex;
        CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                     LibInterface::CONTROL_LWSWITCH_GET_ERRORS,
                                     &params,
                                     sizeof(params)));
        if (params.errorCount > 0)
        {
            m_FatalErrorIndex = params.errorIndex;
        }

        for (UINT32 i = 0; i < params.errorCount; i++)
        {
            LWSWITCH_ERROR& error = params.error[i];
            const auto errorStrings = s_ErrorStrings.find(error.error_value);
            UINT32 errorType = static_cast<UINT32>(error.error_value);
            if (errorStrings == s_ErrorStrings.end())
            {
                Printf(Tee::PriError, "Invalid LwSwitch Error Type = %d\n", errorType);
                return RC::SOFTWARE_ERROR;
            }
            pErrorList->emplace_back(errorType,
                                     error.instance,
                                     error.subinstance,
                                     (errorType == LWSWITCH_ERR_HW_DLPL_TX_RECOVERY_LONG),
                                     errorStrings->second.first,
                                     errorStrings->second.second);
        }
    } while (params.errorCount != 0);

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */  RC LwLinkDevIf::LwSwitchDev::GetEcidString(string *pStr)
{
    MASSERT(pStr);
    RC rc;

    const Platform::SimulationMode Mode = Platform::GetSimulationMode();
    if ((Mode != Platform::Hardware) && (Mode != Platform::RTL))
    {
        *pStr = GetName();
        return OK;
    }

    LWSWITCH_GET_INFO params = {};
    params.count = 5;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_FAB;
    params.index[1] = LWSWITCH_GET_INFO_INDEX_LOT_CODE_0;
    params.index[2] = LWSWITCH_GET_INFO_INDEX_WAFER;
    params.index[3] = LWSWITCH_GET_INFO_INDEX_XCOORD;
    params.index[4] = LWSWITCH_GET_INFO_INDEX_YCOORD;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    const UINT32 FabCode     = params.info[0];
    const UINT32 LotCode0    = params.info[1];
    const UINT32 WaferId     = params.info[2];
    const UINT32 XCoordinate = params.info[3];
    const UINT32 YCoordinate = params.info[4];

    char LotCodeCooked[16] = "*****";
    const char LotCode2Char[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "????????????????????????????????????";

    for (UINT32 CharIdx = 0; CharIdx < 5; CharIdx++)
        LotCodeCooked[4-CharIdx] =
            LotCode2Char[
                (LotCode0 >> 6*CharIdx) & 0x3F];
    LotCodeCooked[5] = '\0';

    *pStr = Utility::StrPrintf("%c%s-%02u_x%02d_y%02u",
            LotCode2Char[FabCode & 0x3F],
            LotCodeCooked,
            WaferId,
            static_cast<INT32>(XCoordinate),
            YCoordinate);

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetFoundry(ChipFoundry *pFoundry)
{
    MASSERT(pFoundry);
    RC rc;
    RegHal& regs = GetInterface<LwLinkRegs>()->Regs();

    *pFoundry = CF_UNKNOWN;

    LWSWITCH_GET_INFO params = {};
    params.count = 1;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_FOUNDRY;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    const UINT32 tsmc = regs.LookupValue(MODS_PSMC_BOOT_0_FOUNDRY_TSMC);
    const UINT32 umc = regs.LookupValue(MODS_PSMC_BOOT_0_FOUNDRY_UMC);
    const UINT32 ibm = regs.LookupValue(MODS_PSMC_BOOT_0_FOUNDRY_IBM);
    const UINT32 smic = regs.LookupValue(MODS_PSMC_BOOT_0_FOUNDRY_SMIC);
    const UINT32 chartered = regs.LookupValue(MODS_PSMC_BOOT_0_FOUNDRY_CHARTERED);
    const UINT32 toshiba = regs.LookupValue(MODS_PSMC_BOOT_0_FOUNDRY_TOSHIBA);
    const UINT32 sony = regs.LookupValue(MODS_PSMC_BOOT_0_FOUNDRY_SONY);

         if (params.info[0] == tsmc)      *pFoundry = CF_TSMC;
    else if (params.info[0] == umc)       *pFoundry = CF_UMC;
    else if (params.info[0] == ibm)       *pFoundry = CF_IBM;
    else if (params.info[0] == smic)      *pFoundry = CF_SMIC;
    else if (params.info[0] == chartered) *pFoundry = CF_CHARTERED;
    else if (params.info[0] == toshiba)   *pFoundry = CF_TOSHIBA;
    else if (params.info[0] == sony)      *pFoundry = CF_SONY;
    else                                  *pFoundry = CF_UNKNOWN;

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ Fuse* LwLinkDevIf::LwSwitchDev::GetFuse()
{
    return m_pFuse.get();
}

//------------------------------------------------------------------------------
RC LwLinkDevIf::LwSwitchDev::GetPhysicalId(UINT32* pId)
{
    RC rc;

    *pId = ILWALID_PHYSICAL_ID;

    LWSWITCH_GET_INFO params = {};
    params.count = 1;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_PHYSICAL_ID;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    *pId = params.info[0];

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetRawEcid(vector<UINT32> *pEcids, bool bReverseFields)
{
    MASSERT(pEcids);
    RC rc;
    const Platform::SimulationMode Mode = Platform::GetSimulationMode();
    if ((Mode != Platform::Hardware) && (Mode != Platform::RTL))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RegHal& regs = GetInterface<LwLinkRegs>()->Regs();

    static const UINT32 size = (regs.LookupMaskSize(MODS_FUSE_OPT_VENDOR_CODE_DATA)
                              + regs.LookupMaskSize(MODS_FUSE_OPT_FAB_CODE_DATA)
                              + regs.LookupMaskSize(MODS_FUSE_OPT_LOT_CODE_0_DATA)
                              + regs.LookupMaskSize(MODS_FUSE_OPT_LOT_CODE_1_DATA)
                              + regs.LookupMaskSize(MODS_FUSE_OPT_WAFER_ID_DATA)
                              + regs.LookupMaskSize(MODS_FUSE_OPT_X_COORDINATE_DATA)
                              + regs.LookupMaskSize(MODS_FUSE_OPT_Y_COORDINATE_DATA)
                              + regs.LookupMaskSize(MODS_FUSE_OPT_OPS_RESERVED_DATA)
                              + 31) / 32;
    pEcids->clear();
    pEcids->resize(size, 0);

    LWSWITCH_GET_INFO params = {};
    params.count = 8;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_VENDOR_CODE;
    params.index[1] = LWSWITCH_GET_INFO_INDEX_FAB;
    params.index[2] = LWSWITCH_GET_INFO_INDEX_LOT_CODE_0;
    params.index[3] = LWSWITCH_GET_INFO_INDEX_LOT_CODE_1;
    params.index[4] = LWSWITCH_GET_INFO_INDEX_WAFER;
    params.index[5] = LWSWITCH_GET_INFO_INDEX_XCOORD;
    params.index[6] = LWSWITCH_GET_INFO_INDEX_YCOORD;
    params.index[7] = LWSWITCH_GET_INFO_INDEX_OPS_RESERVED;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    vector<UINT32> srcArray;
    srcArray.push_back(params.info[0]); //VendorCode
    srcArray.push_back(params.info[1]); //FabCode
    srcArray.push_back(params.info[2]); //LotCode0
    srcArray.push_back(params.info[3]); //LotCode1
    srcArray.push_back(params.info[4]); //WaferId
    srcArray.push_back(params.info[5]); //XCoordinate
    srcArray.push_back(params.info[6]); //YCoordinate
    srcArray.push_back(params.info[7]); //Rsvd

    vector<UINT32> numBits;
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_VENDOR_CODE_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_FAB_CODE_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_LOT_CODE_0_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_LOT_CODE_1_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_WAFER_ID_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_X_COORDINATE_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_Y_COORDINATE_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_OPS_RESERVED_DATA));

    UINT32 dstOffset = 0;
    for (unsigned i=0; i < srcArray.size(); i++)
    {
        Utility::CopyBits(srcArray, *pEcids, i*32, dstOffset, numBits[i]);
        dstOffset += numBits[i];
    }

    // swap the indices so ascii printing shows MSB on left and LSB on right.
    std::reverse(pEcids->begin(), pEcids->end());

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetRomVersion(string* pVersion)
{
    MASSERT(pVersion);
    RC rc;

    *pVersion = "";

    LWSWITCH_GET_BIOS_INFO_PARAMS params = {};
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_BIOS_INFO,
                                 &params,
                                 sizeof(params)));

    *pVersion = Utility::StrPrintf("%x.%02x.%02x.%02x.%02x",
                                   static_cast<UINT32>((params.version >> 32) & 0xFF),
                                   static_cast<UINT32>((params.version >> 24) & 0xFF),
                                   static_cast<UINT32>((params.version >> 16) & 0xFF),
                                   static_cast<UINT32>((params.version >>  8) & 0xFF),
                                   static_cast<UINT32>((params.version >>  0) & 0xFF));

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::GetVoltageMv(UINT32 *pMv)
{
    MASSERT(pMv);
    RC rc;

    *pMv = 0;

    LWSWITCH_GET_INFO params = {};
    params.count = 1;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_VOLTAGE_MVOLT;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));

    *pMv = params.info[0];

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::HotReset(FundamentalResetOptions funResetOption)
{
    StickyRC rc;
    UINT32 startingDevId = GetInterface<Pcie>()->DeviceId();
    RegHal& regs = Regs();

    unique_ptr<PciCfgSpace> pGpuCfgSpace(new PciCfgSpace());

    UINT16 domain;
    UINT16 bus;
    UINT16 dev;
    UINT16 func;
    GetId().GetPciDBDF(&domain, &bus, &dev, &func);

    // determine whether fundamental and hot reset are coupled
    bool fundamentalResetCoupled = false;
    if (regs.IsSupported(MODS_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET))
    {
        UINT32 value = 0;
        CHECK_RC(Platform::PciRead32(domain, bus, dev, func,
                                     regs.LookupAddress(MODS_XVE_RESET_CTRL), &value));

        fundamentalResetCoupled = regs.TestField(value,
                                MODS_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_ENABLED);
    }

    // determine whether we want to trigger a fundamental reset
    bool fundamentalReset = false;
    switch (funResetOption)
    {
        case FundamentalResetOptions::Default:
            fundamentalReset = fundamentalResetCoupled;
            break;
        case FundamentalResetOptions::Enable:
            fundamentalReset = true;
            break;
        case FundamentalResetOptions::Disable:
            fundamentalReset = false;
            break;
        default:
            Printf(Tee::PriError, "Unknown fundamental reset option: %d\n",
                   static_cast<UINT32>(funResetOption));
            return RC::ILWALID_ARGUMENT;
    }
    string resetString = fundamentalReset ? "fundamental" : "hot";

    Printf(Tee::PriNormal, "Preparing to %s reset %04x:%02x:%02x.%02x\n",
           resetString.c_str(), domain, bus, dev, func);

    CHECK_RC(pGpuCfgSpace->Initialize(domain, bus, dev, func));
    Printf(Tee::PriNormal, "Saving config space...\n");
    CHECK_RC(pGpuCfgSpace->Save());

    // optionally override whether fundamental reset gets triggered. doing this after pci cfg space
    // is saved so that the original value is restored.
    if (fundamentalReset != fundamentalResetCoupled)
    {
        if (!regs.IsSupported(MODS_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET))
        {
            Printf(Tee::PriError, "Unable to trigger %s reset.\n", resetString.c_str());
            return RC::UNSUPPORTED_FUNCTION;
        }

        if (regs.IsSupported(MODS_XVE_RESET_CTRL_PRIV_LEVEL_MASK))
        {
            UINT32 plmValue = 0;
            CHECK_RC(Platform::PciRead32(domain, bus, dev, func,
                                         regs.LookupAddress(MODS_XVE_RESET_CTRL_PRIV_LEVEL_MASK), &plmValue));
            if (!regs.TestField(plmValue,
                        MODS_XVE_RESET_CTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED))
            {
                Printf(Tee::PriError, "Unable to trigger %s reset due to priv mask.\n",
                       resetString.c_str());
                return RC::UNSUPPORTED_FUNCTION;
            }
        }

        UINT32 value = 0;
        CHECK_RC(Platform::PciRead32(domain, bus, dev, func,
                                     regs.LookupAddress(MODS_XVE_RESET_CTRL), &value));

        regs.SetField(&value,
            fundamentalReset ?
                MODS_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_ENABLED:
                MODS_XVE_RESET_CTRL_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_DISABLED);

        CHECK_RC(Platform::PciWrite32(domain, bus, dev, func,
                                      regs.LookupAddress(MODS_XVE_RESET_CTRL), value));
    }

    PexDevice* bridge;
    UINT32 parentPort;
    CHECK_RC(GetInterface<Pcie>()->GetUpStreamInfo(&bridge, &parentPort));

    // disable pcie hotplug interrupts. otherwise the OS could get notified and interfere
    bool hotPlugEnabled = false;
    CHECK_RC(bridge->GetDownstreamPortHotplugEnabled(parentPort, &hotPlugEnabled));
    if (hotPlugEnabled)
    {
        CHECK_RC(bridge->SetDownstreamPortHotplugEnabled(parentPort, false));
    }

    DEFER
    {
        if (hotPlugEnabled)
        {
            bridge->SetDownstreamPortHotplugEnabled(parentPort, true);
        }
    };

    bridge->ResetDownstreamPort(parentPort);

    // Wait 200ms for SOE boot to run after issuing hot reset
    Tasker::Sleep(200);

    rc = Tasker::PollHw(1000, [this, startingDevId]()->bool
        {
            // Read the device ID and see if it matches the expected one.
            UINT32 deviceId = GetInterface<Pcie>()->DeviceId();

            return startingDevId == deviceId;
        },
        __FUNCTION__);

    if (rc != OK)
    {
        // We shouldn't return here. We should at least *try* to
        // restore the PCIE config space(s) so we don't force the poor
        // user to reboot their system.
        rc.Clear();
        rc = RC::PCI_HOT_RESET_FAIL;
    }

    CHECK_RC(pGpuCfgSpace->Restore());

    if (rc == OK)
    {
        Printf(Tee::PriNormal, "%s reset was successful\n", resetString.c_str());
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize the device
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwSwitchDev::Initialize()
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

    CHECK_RC(SetupBugs());

    m_pFuse = CreateFuse();

    CHECK_RC(TestDevice::Initialize());

    CHECK_RC(SetupFeatures());

    CHECK_RC(GetInterface<Pcie>()->Initialize(m_LibHandle));

    CHECK_RC(GetInterface<LwLink>()->Initialize(m_LibHandle));

    CHECK_RC(GetInterface<I2c>()->Initialize(m_LibHandle));

    CHECK_RC(GetPhysicalId(&m_PhysicalId));

    CHECK_RC(m_OverTempCounter.Initialize());

    LWSWITCH_GET_INFO params = {};
    params.count = 1;
    params.index[0] = LWSWITCH_GET_INFO_INDEX_PLATFORM;
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_GET_INFO,
                                 &params,
                                 sizeof(params)));
    m_IsEmulation = (params.info[0] == LWSWITCH_GET_INFO_INDEX_PLATFORM_EMULATION);

    m_Initialized = true;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Print the lwlink device identifier information
/* virtual */ void LwLinkDevIf::LwSwitchDev::Print
(
    Tee::Priority pri,
    UINT32 code,
    string prefix
) const
{
    Printf(pri, code, "%s%s\n", prefix.c_str(), GetName().c_str());
    for (size_t ii = 0; ii < GetInterface<LwLink>()->GetMaxLinks(); ii++)
    {

        UINT32 pciDomain, pciBus, pciDev, pciFunc;
        if (OK == m_pLibIf->GetPciInfo(m_LibHandle,
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
                   static_cast<UINT32>(ii));
        }
    }
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::LwSwitchDev::RegRd32(UINT32 offset) const
{
    LWSWITCH_REGISTER_READ readParams = {};
    readParams.engine   = REGISTER_RW_ENGINE_RAW;
    readParams.instance = 0;
    readParams.offset   = offset;
    readParams.val      = 0x0;

    RC rc = m_pLibIf->Control(GetLibHandle(),
                              LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_REGISTER_READ,
                              &readParams,
                              sizeof(readParams));
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Failed to read offset 0x%0x on %s\n", offset, GetName().c_str());
    }

    return static_cast<UINT32>(readParams.val);
}

//------------------------------------------------------------------------------
UINT32 LwLinkDevIf::LwSwitchDev::RegRd32
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
void LwLinkDevIf::LwSwitchDev::RegWr32(UINT32 offset, UINT32 data)
{
    LWSWITCH_REGISTER_READ writeParams = {};
    writeParams.engine   = REGISTER_RW_ENGINE_RAW;
    writeParams.instance = 0;
    writeParams.offset   = offset;
    writeParams.val      = data;

    RC rc = m_pLibIf->Control(GetLibHandle(),
                              LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_REGISTER_WRITE,
                              &writeParams,
                              sizeof(writeParams));

    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Failed to write offset 0x%0x=0x%0x on %s\n",
               offset, data, GetName().c_str());
    }
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwSwitchDev::RegWr32
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
void LwLinkDevIf::LwSwitchDev::RegWrBroadcast32
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
void LwLinkDevIf::LwSwitchDev::RegWrSync32(UINT32 offset, UINT32 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwSwitchDev::RegWrSync32
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
UINT64 LwLinkDevIf::LwSwitchDev::RegRd64(UINT32 offset) const
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
    return 0;
}

//------------------------------------------------------------------------------
UINT64 LwLinkDevIf::LwSwitchDev::RegRd64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset
)
{
    MASSERT(!"RegRd64 not supported in LwSwitch test devices\n");
    return 0;
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwSwitchDev::RegWr64(UINT32 offset, UINT64 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwSwitchDev::RegWr64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWr64 not supported in LwSwitch test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwSwitchDev::RegWrBroadcast64
(
    UINT32        domIdx,
    RegHalDomain  domain,
    LwRm        * pLwRm,
    UINT32        offset,
    UINT64        data
)
{
    MASSERT(!"RegWrBroadcast64 not supported in LwSwitch test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwSwitchDev::RegWrSync64(UINT32 offset, UINT64 data)
{
    MASSERT(!"Non-domain register access not supported in LwLink test devices\n");
}

//------------------------------------------------------------------------------
void LwLinkDevIf::LwSwitchDev::RegWrSync64
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

//-----------------------------------------------------------------------------
/* virtual */ RC LwLinkDevIf::LwSwitchDev::SetOverTempRange
(
    OverTempCounter::TempType overTempType,
    INT32 min,
    INT32 max
)
{
    return m_OverTempCounter.SetTempRange(overTempType, min, max);
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LwSwitchDev::GetOverTempCounter(UINT32 *pCount)
{
    MASSERT(pCount);
    *pCount = 0;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LwSwitchDev::GetExpectedOverTempCounter(UINT32 *pCount)
{
    MASSERT(pCount);
    *pCount = 0;
    return RC::OK;
}

//--------------------------------------------------------------------------
//! \brief Shutdown the lwswitch device
//!
//! \return OK if successful not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwSwitchDev::Shutdown()
{
    StickyRC rc;

    rc = m_OverTempCounter.ShutDown(true);

    m_pFuse.reset();

    rc = GetInterface<LwLink>()->Shutdown();
    rc = GetInterface<Gpio>()->Shutdown();

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Shutdown LwLink device %s\n", __FUNCTION__, GetName().c_str());
    m_Initialized = false;

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::WillowDev::DoStartThermalThrottling(UINT32 onCount, UINT32 offCount)
{
    RC rc;
    RegHal& regs = TestDevice::GetInterface<LwLinkRegs>()->Regs();

    regs.Write32(0, MODS_LWLSAW_OVERTEMPONCNTR_COUNT,  onCount);
    regs.Write32(0, MODS_LWLSAW_OVERTEMPOFFCNTR_COUNT, offCount);
    regs.Write32(0, MODS_LWLSAW_THERMAL_DEBUG_CTRL_FAKE_TEMP_OVER_ALERT_SET);

    return OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::WillowDev::DoStopThermalThrottling()
{
    RC rc;
    RegHal& regs = TestDevice::GetInterface<LwLinkRegs>()->Regs();

    regs.Write32(0, MODS_LWLSAW_THERMAL_DEBUG_CTRL_FAKE_TEMP_OVER_ALERT_CLEAR);

    return OK;
}

//! \brief Pass-thru mechanism to get access to the JTag chains using the LwSwitch.
//!
//! \param chiplet     : The chiplet this chain is located on
//! \param instruction : Instruction id used to access this chain via JTag
//! \param chainLength : Total number of bits in this chain
//! \param pResult     : Read result
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwSwitchDev::ReadHost2Jtag
(
    UINT32 chiplet,
    UINT32 instruction,
    UINT32 chainLength,
    vector<UINT32> *pResult
)
{
    RC rc;
    MASSERT(pResult);
    pResult->resize(chainLength / 32 + 1);

    LWSWITCH_JTAG_CHAIN_PARAMS params = { 0 };
    params.chainLen = chainLength;
    params.chipletSel = chiplet;
    params.instrId = instruction;
    params.dataArrayLen = static_cast<LwU32>(pResult->size());
    params.data = (LwU32*)(&((*pResult)[0]));
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_READ_JTAG_CHAIN,
                                 &params,
                                 sizeof(params)));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Pass-thru mechanism to get access to the JTag chains using the LwSwitch.
//!
//! \param chiplet     : The chiplet this chain is located on
//! \param instruction : Instruction id used to access this chain via JTa
//! \param chainLength : Total number of bits in this chain
//! \param inputData   : Data to write
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwSwitchDev::WriteHost2Jtag
(
    UINT32 chiplet,
    UINT32 instruction,
    UINT32 chainLength,
    const vector<UINT32> &inputData
)
{
    RC rc;
    if (inputData.size() < (chainLength / 32))
    {
        Printf(Tee::PriNormal,
            "%s: Input vector and chain length mismatch\n",
            __FUNCTION__);
        return RC::BAD_PARAMETER;
    }

    LWSWITCH_JTAG_CHAIN_PARAMS params = { 0 };
    params.chainLen = chainLength;
    params.chipletSel = chiplet;
    params.instrId = instruction;
    params.dataArrayLen = static_cast<LwU32>(inputData.size());
    params.data = (LwU32*)(LwUPtr)(&inputData[0]);
    CHECK_RC(GetLibIf()->Control(m_LibHandle,
                                 LibInterface::CONTROL_LWSWITCH_WRITE_JTAG_CHAIN,
                                 &params,
                                 sizeof(params)));
    return rc;
}
