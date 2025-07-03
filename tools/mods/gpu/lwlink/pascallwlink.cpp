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

#include "pascallwlink.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "core/include/framebuf.h"
#include "core/include/gpu.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghaltable.h"
#include "lwgputypes.h"
#include "class/cl90cc.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl90cc.h"

namespace
{
    map<UINT32, UINT32> s_LwlFlagToTLBit;

    struct ErrorFlagRegs
    {
        ModsGpuRegAddress errCtrlReg;
        ModsGpuRegAddress errInjReg;
    };
    struct ErrorInjectRegs
    {
        ModsGpuRegField   errInjEnableBit;
        ModsGpuRegAddress errInj2Reg;
        ModsGpuRegAddress errInj3Reg;
        ModsGpuRegAddress errInj4Reg;
        ModsGpuRegAddress errInj5Reg;
        ModsGpuRegAddress errInj6Reg;
        ModsGpuRegAddress replayThresh;
    };
    struct DomainBaseRegs
    {
        ModsGpuRegAddress lwlBaseReg;
        ModsGpuRegAddress lwltlBaseReg;
    };

    struct EyeDiagramRegs
    {
        ModsGpuRegAddress lwlPadCtl6;
        ModsGpuRegField   lwlPadCtl6WrData;
        ModsGpuRegField   lwlPadCtl6Addr;
        ModsGpuRegField   lwlPadCtl6Wds;
        ModsGpuRegField   lwlRxEomEn;
        ModsGpuRegField   lwlRxEomOvrd;
        ModsGpuRegField   lwlRxEomDone;
        ModsGpuRegField   lwlRxEomStatus;
    };

    const EyeDiagramRegs s_EyeDiagramRegs[] =
    {
        {
             MODS_PLWL0_BR0_PAD_CTL_6
            ,MODS_PLWL0_BR0_PAD_CTL_6_CFG_WDATA
            ,MODS_PLWL0_BR0_PAD_CTL_6_CFG_ADDR
            ,MODS_PLWL0_BR0_PAD_CTL_6_CFG_WDS
            ,MODS_PLWL0_BR0_PAD_CTL_4_RX_EOM_EN
            ,MODS_PLWL0_BR0_PAD_CTL_4_RX_EOM_OVRD
            ,MODS_PLWL0_BR0_PAD_CTL_4_RX_EOM_DONE
            ,MODS_PLWL0_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
             MODS_PLWL1_BR0_PAD_CTL_6
            ,MODS_PLWL1_BR0_PAD_CTL_6_CFG_WDATA
            ,MODS_PLWL1_BR0_PAD_CTL_6_CFG_ADDR
            ,MODS_PLWL1_BR0_PAD_CTL_6_CFG_WDS
            ,MODS_PLWL1_BR0_PAD_CTL_4_RX_EOM_EN
            ,MODS_PLWL1_BR0_PAD_CTL_4_RX_EOM_OVRD
            ,MODS_PLWL1_BR0_PAD_CTL_4_RX_EOM_DONE
            ,MODS_PLWL1_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
             MODS_PLWL2_BR0_PAD_CTL_6
            ,MODS_PLWL2_BR0_PAD_CTL_6_CFG_WDATA
            ,MODS_PLWL2_BR0_PAD_CTL_6_CFG_ADDR
            ,MODS_PLWL2_BR0_PAD_CTL_6_CFG_WDS
            ,MODS_PLWL2_BR0_PAD_CTL_4_RX_EOM_EN
            ,MODS_PLWL2_BR0_PAD_CTL_4_RX_EOM_OVRD
            ,MODS_PLWL2_BR0_PAD_CTL_4_RX_EOM_DONE
            ,MODS_PLWL2_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
             MODS_PLWL3_BR0_PAD_CTL_6
            ,MODS_PLWL3_BR0_PAD_CTL_6_CFG_WDATA
            ,MODS_PLWL3_BR0_PAD_CTL_6_CFG_ADDR
            ,MODS_PLWL3_BR0_PAD_CTL_6_CFG_WDS
            ,MODS_PLWL3_BR0_PAD_CTL_4_RX_EOM_EN
            ,MODS_PLWL3_BR0_PAD_CTL_4_RX_EOM_OVRD
            ,MODS_PLWL3_BR0_PAD_CTL_4_RX_EOM_DONE
            ,MODS_PLWL3_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
             MODS_PLWL4_BR0_PAD_CTL_6
            ,MODS_PLWL4_BR0_PAD_CTL_6_CFG_WDATA
            ,MODS_PLWL4_BR0_PAD_CTL_6_CFG_ADDR
            ,MODS_PLWL4_BR0_PAD_CTL_6_CFG_WDS
            ,MODS_PLWL4_BR0_PAD_CTL_4_RX_EOM_EN
            ,MODS_PLWL4_BR0_PAD_CTL_4_RX_EOM_OVRD
            ,MODS_PLWL4_BR0_PAD_CTL_4_RX_EOM_DONE
            ,MODS_PLWL4_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
             MODS_PLWL5_BR0_PAD_CTL_6
            ,MODS_PLWL5_BR0_PAD_CTL_6_CFG_WDATA
            ,MODS_PLWL5_BR0_PAD_CTL_6_CFG_ADDR
            ,MODS_PLWL5_BR0_PAD_CTL_6_CFG_WDS
            ,MODS_PLWL5_BR0_PAD_CTL_4_RX_EOM_EN
            ,MODS_PLWL5_BR0_PAD_CTL_4_RX_EOM_OVRD
            ,MODS_PLWL5_BR0_PAD_CTL_4_RX_EOM_DONE
            ,MODS_PLWL5_BR0_PAD_CTL_4_RX_EOM_STATUS
        }
    };

    const ModsGpuRegField s_TxLaneCrcRegs[] =
    {
         MODS_PLWL0_SL0_TXLANECRC_ENABLE
        ,MODS_PLWL1_SL0_TXLANECRC_ENABLE
        ,MODS_PLWL2_SL0_TXLANECRC_ENABLE
        ,MODS_PLWL3_SL0_TXLANECRC_ENABLE
        ,MODS_PLWL4_SL0_TXLANECRC_ENABLE
        ,MODS_PLWL5_SL0_TXLANECRC_ENABLE
    };

    const ModsGpuRegField s_BlkLenRegs[] =
    {
         MODS_PLWL0_LINK_CONFIG_BLKLEN
        ,MODS_PLWL1_LINK_CONFIG_BLKLEN
        ,MODS_PLWL2_LINK_CONFIG_BLKLEN
        ,MODS_PLWL3_LINK_CONFIG_BLKLEN
        ,MODS_PLWL4_LINK_CONFIG_BLKLEN
        ,MODS_PLWL5_LINK_CONFIG_BLKLEN
    };

    const FLOAT64 BW_PCT_ALL_FBS     = 0.99;
    const FLOAT64 BW_PCT_6FBS        = 0.96;
    const FLOAT64 BW_PCT_ILWALID     = 1.0;

    const UINT32 BLOCK_SIZE  = 16;
    const UINT32 FOOTER_BITS = 2;

    const UINT32 MAX_LWLINK_PACKET_BYTES_2D_SM = 128;
    const UINT32 BYTES_PER_FLIT                = 16;

    bool SortByFabricBase
    (
        const LwLink::FabricAperture ap1,
        const LwLink::FabricAperture ap2
    )
    {
        return ap1.first < ap2.first;
    }
}

//------------------------------------------------------------------------------
//! \brief Clear the error counts for the specified link id
//!
//! \param linkId  : Link id to clear counts on
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoClearHwErrorCounts(UINT32 linkId)
{
    RC rc;
    LW2080_CTRL_LWLINK_CLEAR_COUNTERS_PARAMS clearParams;
    clearParams.linkMask = (1 << linkId);

    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));
    clearParams.counterMask = GetErrorCounterControlMask(bPerLaneEnabled);

    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_CLEAR_COUNTERS,
                                           &clearParams,
                                           sizeof(clearParams)));

    // SW Recoveries are self clearing on read
    LW2080_CTRL_CMD_LWLINK_GET_ERROR_RECOVERIES_PARAMS recoveryParams = {};
    recoveryParams.linkMask = 1 << linkId;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_ERROR_RECOVERIES,
                                           &recoveryParams,
                                           sizeof(recoveryParams)));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Detect topology for the gpu device
//!
//! \return OK if successful, not OK otherwise
//!
RC PascalLwLink::DoDetectTopology()
{
    RC rc;

    CHECK_RC(InitializePostDiscovery());

    vector<EndpointInfo> endpointInfo;
    CHECK_RC(GetDetectedEndpointInfo(&endpointInfo));
    SetEndpointInfo(endpointInfo);

    CHECK_RC(SavePerLaneCrcSetting());

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Enable per lane error counts
//!
//! \param bEnable : True to enable per-lane counts, false to disable
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
{
    if (linkId >= NUMELEMS(s_TxLaneCrcRegs))
    {
        Printf(Tee::PriError, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    Regs().Write32(s_TxLaneCrcRegs[linkId], bEnable ? 1 : 0);
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get Copy Engine copy width through lwlink
//!
//! \param target : Copy engine target to get the width for
//!
//! \return Copy Engine copy width through lwlink
/* virtual */ LwLinkImpl::CeWidth PascalLwLink::DoGetCeCopyWidth(CeTarget target) const
{
    CeWidth width = CEW_128_BYTE;
    if (target == CET_SYSMEM)
    {
        if (Regs().Test32(MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_SYSMEM_EN_ON))
            width = CEW_256_BYTE;
    }
    else if (target == CET_PEERMEM)
    {
        if (Regs().Test32(MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_PEERMEM_EN_ON))
            width = CEW_256_BYTE;
    }
    return width;
}

//------------------------------------------------------------------------------
RC PascalLwLink::DoGetDiscoveredLinkMask(UINT64 *pLinkMask)
{
    MASSERT(pLinkMask);

    *pLinkMask = 0ULL;

    RC rc;
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS capParams = { };
    LwRmPtr pLwRm;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
                                           &capParams,
                                           sizeof(capParams)));
    *pLinkMask = capParams.discoveredLinkMask;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the error counts for the specified link id
//!
//! \param linkId        : Link id to get counts on
//! \param pErrorCounts  : Pointer to returned error counts
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoGetErrorCounts
(
    UINT32 linkId,
    LwLink::ErrorCounts * pErrorCounts
)
{
    // Counter registers are not implemented on simulation platforms
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return OK;

    RC rc;
    LW2080_CTRL_LWLINK_GET_COUNTERS_PARAMS countParams = {};

    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));

    countParams.linkMask |= 1ULL<<(linkId);
    countParams.counterMask = GetErrorCounterControlMask(bPerLaneEnabled);
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_COUNTERS,
                                           &countParams,
                                           sizeof(countParams)));

    INT32 lwrCounterIdx = Utility::BitScanForward(countParams.counterMask, 0);
    while (lwrCounterIdx != -1)
    {
        LwLink::ErrorCounts::ErrId errId = ErrorCounterBitToId(1U << lwrCounterIdx);
        if (errId == LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS)
        {
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(pErrorCounts->SetCount(errId,
                                  static_cast<UINT32>(countParams.counters[linkId].value[lwrCounterIdx]),
                                  false));
        lwrCounterIdx = Utility::BitScanForward(countParams.counterMask,
                                                lwrCounterIdx + 1);
    }

    LW2080_CTRL_CMD_LWLINK_GET_ERROR_RECOVERIES_PARAMS recoveryParams = {};
    recoveryParams.linkMask = 1 << linkId;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_ERROR_RECOVERIES,
                                           &recoveryParams,
                                           sizeof(recoveryParams)));
    CHECK_RC(pErrorCounts->SetCount(LwLink::ErrorCounts::LWL_SW_RECOVERY_ID,
                                    static_cast<UINT32>(recoveryParams.numRecoveries[linkId]),
                                    false));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the current HW LwLink error flags
//!
//! \param pErrorFlags : Pointer to returned error flags
//!
//! \return OK if successful, not OK otherwise
RC PascalLwLink::DoGetErrorFlags(LwLink::ErrorFlags * pErrorFlags)
{
    MASSERT(pErrorFlags);
    pErrorFlags->linkErrorFlags.clear();
    pErrorFlags->linkGroupErrorFlags.clear();

    // Flag registers are not implemented on simulation platforms
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return OK;

    RC rc;
    LW2080_CTRL_LWLINK_GET_ERR_INFO_PARAMS flagParams = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_ERR_INFO,
                                           &flagParams,
                                           sizeof(flagParams)));

    for (INT32 lwrLink = Utility::BitScanForward(flagParams.linkMask, 0);
         lwrLink != -1;
         lwrLink = Utility::BitScanForward(flagParams.linkMask, lwrLink + 1))
    {
        set<string> newFlags;
        for (UINT32 errBitIdx = 0; errBitIdx < LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_MAX; errBitIdx++)
        {
            if (flagParams.linkErrInfo[lwrLink].TLErrlog & (1 << errBitIdx))
            {
                LwLinkErrorFlags::LwLinkFlagBit errFlagBit =
                    LwLinkErrorFlags::GetErrorFlagBit(errBitIdx);

                if (errFlagBit == LwLinkErrorFlags::LWL_NUM_ERR_FLAGS)
                {
                    return RC::SOFTWARE_ERROR;
                }

                string errFlagStr = LwLinkErrorFlags::GetFlagString(errFlagBit);
                newFlags.insert(errFlagStr);
            }
        }
        if (flagParams.linkErrInfo[lwrLink].bExcessErrorDL)
        {
            LwLinkErrorFlags::LwLinkFlagBit flagBit = LwLinkErrorFlags::LWL_ERR_FLAG_EXCESS_DL;
            string flagStr = LwLinkErrorFlags::GetFlagString(flagBit);
            newFlags.insert(flagStr);
        }
        pErrorFlags->linkErrorFlags[lwrLink] = { newFlags.begin(), newFlags.end() };
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get Link ID associated with the specified endpoint
//!
//! \param endpointInfo : Endpoint info to get link ID for
//! \param pLinkId      : Pointer to returned Link ID
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoGetLinkId
(
    const EndpointInfo &endpointInfo,
    UINT32                        *pLinkId
) const
{
    if (GetDevice()->GetId() != endpointInfo.id)
    {
        Printf(Tee::PriError,
               "%s : Endpoint not present on %s\n", __FUNCTION__,
               GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    *pLinkId = endpointInfo.linkNumber;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get information about the LwLinks on the associated subdevice
//!
//! \param pLinkInfo  : Pointer to returned link info
//!
//! \return OK if successful, not OK otherwise
RC PascalLwLink::DoGetLinkInfo(vector<LinkInfo> *pLinkInfo)
{
    RC rc;
    LwRmPtr pLwRm;

    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
    CHECK_RC(pLwRm->ControlBySubdevice(GetGpuSubdevice(),
                                       LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                                       &statusParams,
                                       sizeof(statusParams)));
    pLinkInfo->clear();

    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        LinkInfo lwrInfo;

        if (!(statusParams.enabledLinkMask & (1 << lwrLink)))
        {
            pLinkInfo->push_back(lwrInfo);
            continue;
        }

        // There can be holes in the link mask from RM and RM will not
        // virtualize the linkIds (i.e. bit x always corresponds
        // to index x and always refers to physical lwlink x)
        SetLinkInfo(lwrLink, statusParams.linkInfo[lwrLink], &lwrInfo);
        pLinkInfo->push_back(lwrInfo);
    }

    return rc;
}

//------------------------------------------------------------------------------
UINT32 PascalLwLink::DoGetLinksPerGroup() const
{
    // Prior to Ampere ther was a single link group with all links in it and there is
    // also no way to determine the actual physical number of links (values returned from
    // RM are filtered by links that are disabled in the VBIOS which is what is returned
    // from GetMaxLinks()).  This width of this field is the same as the number of
    // physical links (this is the method RM uses to determine the maximum physical number). 
    return Regs().LookupMaskSize(MODS_IOCTRL_RESET_LINKRESET);
}

//------------------------------------------------------------------------------
//! \brief Get link status for all links
//!
//! \param pLinkStatus  : Pointer to returned per-link status
//!
//! \return OK if successful, not OK otherwise
RC PascalLwLink::DoGetLinkStatus(vector<LinkStatus> *pLinkStatus)
{
    StickyRC rc;
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                                           &statusParams,
                                           sizeof(statusParams)));
    pLinkStatus->clear();

    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        LinkStatus lwrState =
        {
            LS_ILWALID,
            SLS_ILWALID,
            SLS_ILWALID
        };

        if (!(statusParams.enabledLinkMask & (1 << lwrLink)))
        {
            pLinkStatus->push_back(lwrState);
            continue;
        }

        LW2080_CTRL_LWLINK_LINK_STATUS_INFO *pLinkStat = &statusParams.linkInfo[lwrLink];
        lwrState.linkState      = RmLinkStateToLinkState(pLinkStat->linkState);
        lwrState.rxSubLinkState = RmSubLinkStateToLinkState(pLinkStat->rxSublinkStatus);
        lwrState.txSubLinkState = RmSubLinkStateToLinkState(pLinkStat->txSublinkStatus);
        pLinkStatus->push_back(lwrState);
    }

    return rc;
}

//------------------------------------------------------------------------------
void PascalLwLink::DoGetEomDefaultSettings
(
    UINT32 link,
    EomSettings* pSettings
) const
{
    MASSERT(pSettings);
    pSettings->numErrors = 0x7;
    pSettings->numBlocks = 0xe;
}

namespace
{
    struct PollEomDoneArgs
    {
        LwLinkRegs *    pLwLinkRegs;
        ModsGpuRegField regField;
        UINT32          numLanes;
        UINT32          doneStatus;
    };
    bool PollEomDone(void* args)
    {
        PollEomDoneArgs* pollArgs = static_cast<PollEomDoneArgs*>(args);
        RegHal& regs = pollArgs->pLwLinkRegs->Regs();
        for (UINT32 lane = 0; lane < pollArgs->numLanes; lane++)
        {
            if (regs.Read32(pollArgs->regField, lane) != pollArgs->doneStatus)
                return false;
        }
        return true;
    }
}

RC PascalLwLink::DoGetEomStatus(FomMode mode,
                                UINT32 link,
                                UINT32 numErrors,
                                UINT32 numBlocks,
                                FLOAT64 timeoutMs,
                                vector<INT32>* status)
{
    MASSERT(status);
    RC rc;

    RegHal& regs = Regs();

    if (link > NUMELEMS(s_EyeDiagramRegs))
    {
        Printf(Tee::PriError,
               "%s : Invalid link = %d. Maybe eye diagram registers need to be updated.\n",
               __FUNCTION__, link);
        return RC::BAD_PARAMETER;
    }

    if (!SupportsFomMode(mode))
    {
        Printf(Tee::PriError, "Invalid Mode = %u\n", mode);
        return RC::BAD_PARAMETER;
    }

    const UINT32 numLanes = static_cast<UINT32>(status->size());
    MASSERT(numLanes > 0);

    UINT32 ber = (numBlocks << 4) | numErrors;
    switch (mode)
    {
        case EYEO_X:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x3, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x5 << 8) | ber, link, numLanes));
            break;
        case EYEO_XL_O:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x2, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x0 << 8 )| ber, link, numLanes));
            break;
        case EYEO_XL_E:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x1, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x0 << 8 )| ber, link, numLanes));
            break;
        case EYEO_XH_O:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x2, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x1 << 8) | ber, link, numLanes));
            break;
        case EYEO_XH_E:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x1, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x1 << 8) | ber, link, numLanes));
            break;
        case EYEO_Y:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x3, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0xb << 8) | ber, link, numLanes));
            break;
        case EYEO_YL_O:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x2, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x6 << 8) | ber, link, numLanes));
            break;
        case EYEO_YL_E:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x1, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x6 << 8) | ber, link, numLanes));

            break;
        case EYEO_YH_O:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x2, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x7 << 8) | ber, link, numLanes));

            break;
        case EYEO_YH_E:
            CHECK_RC(WriteLwLinkCfgData(0x66, 0x1, link, numLanes));
            CHECK_RC(WriteLwLinkCfgData(0x67, (0x7 << 8) | ber, link, numLanes));
            break;
        default:
            Printf(Tee::PriError, "Invalid Mode = %u\n", mode);
            return RC::SOFTWARE_ERROR;
    }

    // Set EOM disabled
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomEn, numLanes, 0x0);

    // Check that EOM_DONE is reset
    PollEomDoneArgs pollArgs =
    {
        GetInterface<LwLinkRegs>(), s_EyeDiagramRegs[link].lwlRxEomDone, numLanes, 0x0
    };
    CHECK_RC(POLLWRAP_HW(&PollEomDone, &pollArgs, timeoutMs));

    // Set EOM programmable
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomOvrd, numLanes, 0x1);

    // Set EOM enabled
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomEn, numLanes, 0x1);

    // Check if EOM is done
    pollArgs.doneStatus = 0x1;
    CHECK_RC(POLLWRAP_HW(&PollEomDone, &pollArgs, timeoutMs));

    // Set EOM disabled
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomEn, numLanes, 0x0);

    // Set EOM not programmable
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomOvrd, numLanes, 0x0);

    // Get EOM status
    for (UINT32 i = 0; i < numLanes; i++)
    {
        status->at(i) = static_cast<INT16>(regs.Read32(s_EyeDiagramRegs[link].lwlRxEomStatus, i));
    }

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get the fabric aperture for this device
//!
//! \param pFas : Pointer to returned fabric apertures
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoGetFabricApertures(vector<FabricAperture> *pFas)
{
    *pFas = m_FabricApertures;
    return (m_FabricApertures.empty()) ? RC::SOFTWARE_ERROR : OK;
}

//------------------------------------------------------------------------------
void PascalLwLink::DoGetFlitInfo
(
    UINT32 transferWidth
   ,HwType hwType
   ,bool bSysmem
   ,FLOAT64 *pReadDataFlits
   ,FLOAT64 *pWriteDataFlits
   ,UINT32 *pTotalReadFlits
   ,UINT32 *pTotalWriteFlits
) const
{
    UINT32 packetDataBytes;
    if (hwType == HT_CE)
    {
        packetDataBytes = (GetCeCopyWidth(bSysmem ? CET_SYSMEM : CET_PEERMEM) == CEW_128_BYTE) ?
            128 :256;
    }
    else
    {
        packetDataBytes = MAX_LWLINK_PACKET_BYTES_2D_SM;
    }

    if (transferWidth < packetDataBytes)
    {
        // Arg...special case...if only all packets were required to be powers of 2
        if ((transferWidth > 64) && (transferWidth <= 96))
        {
            packetDataBytes = 96;
        }
        else
        {
            packetDataBytes = (1 << Utility::BitScanReverse(transferWidth));
            if (transferWidth & (packetDataBytes - 1))
                packetDataBytes <<= 1;
        }
    }

    // Sub-flit size data transfer efficiences will be accurate
    if ((transferWidth > BYTES_PER_FLIT) && (transferWidth % packetDataBytes))
    {
        Printf(Tee::PriWarn,
               "%s : Transfer width (%u) not a multiple of packet data bytes (%u), efficiency"
               " will not be accurate \n",
               __FUNCTION__, transferWidth, packetDataBytes);
    }

    // Read and write data flits are always the same
    *pReadDataFlits = static_cast<FLOAT64>(packetDataBytes) / BYTES_PER_FLIT;
    *pWriteDataFlits = *pReadDataFlits;

    // Minimum of 2 data flits on read
    *pTotalReadFlits = max(packetDataBytes / BYTES_PER_FLIT, 2U);
    *pTotalWriteFlits = max(packetDataBytes / BYTES_PER_FLIT, 1U);

    // When writing and the data amount is smaller than a flit, then a byte
    // enable flit is generated
    if (packetDataBytes < BYTES_PER_FLIT)
    {
        *pTotalWriteFlits = (*pTotalWriteFlits + 1);
    }
}

RC PascalLwLink::WriteLwLinkCfgData(UINT32 addr, UINT32 data, UINT32 link, UINT32 lane)
{
    RegHal &regs = Regs();

    UINT32 plwl0ctl6 = regs.Read32(s_EyeDiagramRegs[link].lwlPadCtl6, lane);

    regs.SetField(&plwl0ctl6, s_EyeDiagramRegs[link].lwlPadCtl6Addr, addr);

    regs.SetField(&plwl0ctl6, s_EyeDiagramRegs[link].lwlPadCtl6WrData, data);

    regs.SetField(&plwl0ctl6, s_EyeDiagramRegs[link].lwlPadCtl6Wds, 0x1);

    regs.Write32(s_EyeDiagramRegs[link].lwlPadCtl6, lane, plwl0ctl6);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get the maximum number of links on the GpuSubdevice parent
//!
//! \return Maximum number of links
UINT32 PascalLwLink::DoGetMaxLinks() const
{
    MASSERT(m_bLwCapsLoaded);
    return m_MaxLinks;
}

//------------------------------------------------------------------------------
//! \brief Get the maximum observable bandwidth percentage
//!
//! \return Maximum percent of SOL bandwidth that can be observed
/* virtual */ FLOAT64 PascalLwLink::DoGetMaxObservableBwPct(LwLink::TransferType tt)
{
    FrameBuffer * pFb = GetGpuSubdevice()->GetFB();

    if (pFb == nullptr)
        return BW_PCT_ILWALID;

    // 6FBPS is a valid FB configuration and it can affect the potential lwlink
    // bandwidth due to creating an imbalance in the ports from HSHUB to XBAR
    if (pFb->GetFbpCount() == 6)
        return BW_PCT_6FBS;

    if (pFb->GetFbpCount() != GetGpuSubdevice()->GetMaxFbpCount())
    {
        if (!m_bFbpBwWarningPrinted)
        {
            Printf(Tee::PriNormal,
                   "%s : Unsupported FB configuration (%u partitions).\n"
                   "%s   Unable to determine observable bandwidth percent!\n",
                   GetGpuSubdevice()->GpuIdentStr().c_str(),
                   pFb->GetFbpCount(),
                   string(GetGpuSubdevice()->GpuIdentStr().size(), ' ').c_str());
            m_bFbpBwWarningPrinted = true;
            return BW_PCT_ILWALID;
        }
    }
    return BW_PCT_ALL_FBS;
}

//------------------------------------------------------------------------------
//! \brief Get maximum possible transfer efficiency on LwLink
//!
//! \return Maximum percent of raw bandwidth achievable
//!
/* virtual */ FLOAT64 PascalLwLink::DoGetMaxTransferEfficiency() const
{
    return 0.94;
}

//------------------------------------------------------------------------------
//! \brief Get the detected endpoint information for all links
//!
//! \param pEndpointInfo  : Pointer to returned endpoint info
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoGetDetectedEndpointInfo
(
    vector<EndpointInfo> *pEndpointInfo
)
{
    RC rc;
    MASSERT(IsPostDiscovery());

    if (m_CachedDetectedEndpointInfo.size() > 0)
    {
        *pEndpointInfo = m_CachedDetectedEndpointInfo;
        return OK;
    }

    LwRmPtr pLwRm;

    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
    CHECK_RC(pLwRm->ControlBySubdevice(GetGpuSubdevice(),
                                       LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                                       &statusParams,
                                       sizeof(statusParams)));
    pEndpointInfo->clear();

    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        pEndpointInfo->push_back(EndpointInfo());

        if (!(statusParams.enabledLinkMask & (1 << lwrLink)))
            continue;

        // There can be holes in the link mask from RM and RM will not
        // virtualize the linkIds (i.e. bit x always corresponds
        // to index x and always refers to physical lwlink x)
        LW2080_CTRL_LWLINK_LINK_STATUS_INFO *pLinkStat = &statusParams.linkInfo[lwrLink];

        if (LW2080_CTRL_LWLINK_GET_CAP(((LwU8*)(&pLinkStat->capsTbl)),
                                        LW2080_CTRL_LWLINK_CAPS_VALID) == 0)
        {
            continue;
        }

        if (!pLinkStat->connected)
            continue;

        LW2080_CTRL_LWLINK_DEVICE_INFO *pRemoteInfo = &pLinkStat->remoteDeviceInfo;

        if (pRemoteInfo->deviceType == LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE)
        {
            const Tee::Priority pri =
                (Platform::GetSimulationMode() != Platform::Hardware) ? Tee::PriWarn :
                                                                        Tee::PriError;
            Printf(pri,
                   "%s : RM reported connected link with no remote device type at link %u!\n",
                   __FUNCTION__, lwrLink);

            // SIM platforms do not always lwrrently return valid data for status calls
            // and failing will regress arch tests.  Simply flag the link as unconnected
            // and continue when this happens
            if (Platform::GetSimulationMode() != Platform::Hardware)
            {
                pLinkStat->connected = false;
                continue;
            }
            return RC::SOFTWARE_ERROR;
        }

        if (!DRF_VAL(2080_CTRL_LWLINK,
                     _DEVICE_INFO_DEVICE_ID,
                     _FLAGS,
                     pRemoteInfo->deviceIdFlags) &
                LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_ID_FLAGS_PCI)
        {
            Printf(Tee::PriError, "%s : PCI info missing endpoint at link %u!\n",
                   __FUNCTION__, lwrLink);
            return RC::SOFTWARE_ERROR;
        }

        EndpointInfo lwrInfo;
        switch (pRemoteInfo->deviceType)
        {
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_GPU:
                lwrInfo.devType = TestDevice::TYPE_LWIDIA_GPU;
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain,
                                      pRemoteInfo->bus,
                                      pRemoteInfo->device,
                                      pRemoteInfo->function);
                break;
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_EBRIDGE:
                lwrInfo.devType = TestDevice::TYPE_EBRIDGE;
                // Ebridge devices are only differentiated by their Domain/Bus/Device
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain,
                                    pRemoteInfo->bus,
                                    pRemoteInfo->device);
                break;
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NPU:
                lwrInfo.devType = TestDevice::TYPE_IBM_NPU;
                // IBMNPU devices are only differentiated by their Domain
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain);
                break;
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_SWITCH:
                lwrInfo.devType = TestDevice::TYPE_LWIDIA_LWSWITCH;
                // Switch devices are not differentiated by function
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain,
                                      pRemoteInfo->bus,
                                      pRemoteInfo->device);
                break;
            case LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_TEGRA:
                // If a DGPU is connected to a cheetah (i.e. RM is available) then the
                // cheetah representation is a TEGRA_MFG and not a CHEETAH
                lwrInfo.devType = TestDevice::TYPE_TEGRA_MFG;
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain,
                                      0xffff,
                                      0xffff,
                                      0xffff);
                break;
            default:
                Printf(Tee::PriError, "%s : Unknown remote device type %u\n",
                       __FUNCTION__, static_cast<UINT32>(pRemoteInfo->deviceType));
                return RC::SOFTWARE_ERROR;
        }
        lwrInfo.linkNumber = pLinkStat->remoteDeviceLinkNumber;
        lwrInfo.pciDomain   = pRemoteInfo->domain;
        lwrInfo.pciBus      = pRemoteInfo->bus;
        lwrInfo.pciDevice   = pRemoteInfo->device;
        lwrInfo.pciFunction = pRemoteInfo->function;
        pEndpointInfo->at(lwrLink) = lwrInfo;
    }

    m_CachedDetectedEndpointInfo = *pEndpointInfo;

    return rc;
}

//--------------------------------------------------------------------------
/* virtual */ void PascalLwLink::SetLinkInfo
(
    UINT32                                      linkId,
    const LW2080_CTRL_LWLINK_LINK_STATUS_INFO & rmLinkInfo,
    LinkInfo *                                  pLinkInfo
) const
{
    pLinkInfo->bValid =
        (LW2080_CTRL_LWLINK_GET_CAP(((const LwU8*)(&rmLinkInfo.capsTbl)),
                                    LW2080_CTRL_LWLINK_CAPS_VALID) != 0);

    pLinkInfo->version           = rmLinkInfo.lwlinkVersion;

    if (!pLinkInfo->bValid)
        return;

    const LW2080_CTRL_LWLINK_DEVICE_INFO & remoteInfo = rmLinkInfo.remoteDeviceInfo;
    pLinkInfo->bActive = (remoteInfo.deviceType !=
                       LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE);

    pLinkInfo->refClkSpeedMhz    = rmLinkInfo.lwlinkRefClkSpeedMhz;
    pLinkInfo->sublinkWidth      = rmLinkInfo.subLinkWidth;
    pLinkInfo->bLanesReversed    = (rmLinkInfo.bLaneReversal == LW_TRUE);
    pLinkInfo->linkClockMHz      = rmLinkInfo.lwlinkCommonClockSpeedMhz;

    if (!pLinkInfo->bActive)
        return;

    pLinkInfo->lineRateMbps      = rmLinkInfo.lwlinkLineRateMbps;
    // TODO : lwlinkCommonClockSpeedMhz is deprecated and lwlinkLinkClockMhz should be used instead
    // however RM is returning lwlinkLineRateMbps in lwlinkLinkClockMhz instead of the value that
    // lwlinkCommonClockSpeedMhz used to return
    // pLinkInfo->linkClockMHz      = rmLinkInfo.lwlinkLinkClockMhz;

    FLOAT64 efficiency = 1.0;
    const UINT32 blklen = Regs().Read32(s_BlkLenRegs[linkId]);
    if (!Regs().TestField(blklen, MODS_PLWL0_LINK_CONFIG_BLKLEN_OFF))
    {
        const FLOAT64 frameLength = (BLOCK_SIZE * blklen) + FOOTER_BITS;
        efficiency = (frameLength - FOOTER_BITS)  /  frameLength;
    }        
    pLinkInfo->maxLinkDataRateKiBps = efficiency *
                                      CallwlateLinkBwKiBps(pLinkInfo->lineRateMbps,
                                                           pLinkInfo->sublinkWidth);
}

//------------------------------------------------------------------------------
//! \brief Get whether per lane counts are enabled
//!
//! \param linkId : Link ID to get per-lane error count enable state
//! \param pbPerLaneEnabled : Return status of per lane enabled
//!
//! \return true if per lane counts are enabled, false otherwise
/* virtual */ RC PascalLwLink::DoPerLaneErrorCountsEnabled
(
    UINT32 linkId,
    bool *pbPerLaneEnabled
)
{
    MASSERT(pbPerLaneEnabled);
    *pbPerLaneEnabled = false;

    if (linkId >= NUMELEMS(s_TxLaneCrcRegs))
    {
        Printf(Tee::PriError, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;;
    }

    *pbPerLaneEnabled = Regs().Read32(s_TxLaneCrcRegs[linkId]) ? true : false;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Register read interface used by lwlink device
//!
//! \param linkId  : Link ID to read the register on
//! \param domain  : Domain to read
//! \param offset  : Offset from the link/domain to read
//! \param pData   : Pointer to returned data
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoRegRd
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64*      pData
)
{
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid link ID %u!\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    UINT32 regBase = GetDomainBase(domain, linkId);
    if (regBase == ILWALID_DOMAIN_BASE)
        return RC::BAD_PARAMETER;

    *pData = static_cast<UINT64>(GetGpuSubdevice()->RegRd32(regBase + offset));
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Register write interface used by lwlink device
//!
//! \param linkId  : Link ID to write the register on
//! \param domain  : Domain to write
//! \param offset  : Offset from the link/domain to write
//! \param data    : Data to write
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoRegWr
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64       data
)
{
    MASSERT(data <= _UINT32_MAX);

    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid link ID %u!\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    UINT32 regBase = GetDomainBase(domain, linkId);
    if (regBase == ILWALID_DOMAIN_BASE)
        return RC::BAD_PARAMETER;

    GetGpuSubdevice()->RegWr32(regBase + offset, static_cast<UINT32>(data));
    return OK;
}

//------------------------------------------------------------------------------
RC PascalLwLink::DoResetTopology()
{
    RC rc;
    GpuDevMgr * pGpuDevMgr = (GpuDevMgr *)DevMgrMgr::d_GraphDevMgr;
    pGpuDevMgr->RemoveAllPeerMappings(GetGpuSubdevice());

    m_TopologyId = ~0U;

    CHECK_RC(LwLinkImpl::DoResetTopology());

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Set Copy Engine copy width through lwlink
//!
//! \param target : Copy engine target to get the width for
//! \param width  : Copy engine width to set
/* virtual */ void PascalLwLink::DoSetCeCopyWidth(CeTarget target, CeWidth width)
{
    if (target == CET_SYSMEM)
    {
        if (width == CEW_256_BYTE)
        {
            Regs().Write32(MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_SYSMEM_EN_ON);
        }
        else
        {
            Regs().Write32(MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_SYSMEM_EN_OFF);
        }
    }
    else if (target == CET_PEERMEM)
    {
        if (width == CEW_256_BYTE)
        {
            Regs().Write32(MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_PEERMEM_EN_ON);
        }
        else
        {
            Regs().Write32(MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_PEERMEM_EN_OFF);
        }
    }
}

//------------------------------------------------------------------------------
//! \brief Setup the fabric aperture for this device
//!
//! \param fa : Fabric aperture to set
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoSetFabricApertures(const vector<FabricAperture> &fa)
{
    m_FabricApertures = fa;

    // RM only supports a single contiguous 32GB aligned fabric base address for a GPU.
    // Ensure that the provided fabric apertures meet this requirement
    sort(m_FabricApertures.begin(), m_FabricApertures.end(), SortByFabricBase);

    static const UINT64 FABRIC_ADDR_ALIGN = (1ULL << 35) - 1;
    if (m_FabricApertures[0].first & FABRIC_ADDR_ALIGN)
    {
        Printf(Tee::PriError, "%s : Fabric addresses must be 32GB aligned!\n", __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    for (UINT32 ii = 1; ii < m_FabricApertures.size(); ii++)
    {
        const UINT64 expectedFa = m_FabricApertures[ii - 1].first +
                                  m_FabricApertures[ii - 1].second;
        if (m_FabricApertures[ii].first != expectedFa)
        {
            Printf(Tee::PriError, "%s : Fabric addresses are not contiguous!\n", __FUNCTION__);
            return RC::BAD_PARAMETER;
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Setup topology related functionality on the device
//!
//! \param endpointInfo : Per link endpoint info for the device
//! \param topologyId   : Id in the topology file for the device
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoSetupTopology
(
    const vector<EndpointInfo> &endpointInfo
   ,UINT32 topologyId
)
{
    RC rc;
    Printf(Tee::PriLow, LwLinkDevIf::GetTeeModule()->GetCode(),
           "%s : Setting up topology on %s\n",
           __FUNCTION__, GetDevice()->GetName().c_str());

    SetEndpointInfo(endpointInfo);

    m_TopologyId = topologyId;

    CHECK_RC(SavePerLaneCrcSetting());

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the lwlink device
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::DoShutdown()
{
    StickyRC rc;

    if (m_SavedPerLaneEnable != ~0U)
        rc = EnablePerLaneErrorCounts(m_SavedPerLaneEnable ? true : false);

    if (m_hProfiler)
    {
        LwRmPtr()->Free(m_hProfiler);
        m_hProfiler = 0;
    }

    if (m_CountersReservedCount)
    {
        Printf(Tee::PriWarn, "%s : LwLink counters still reserved on shutdown\n",
               GetDevice()->GetName().c_str());
    }
    m_pCounterMutex = Tasker::NoMutex();

    rc = LwLinkImpl::DoShutdown();

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ bool PascalLwLink::DoSupportsFomMode(FomMode mode) const
{
    switch (mode)
    {
        case EYEO_X:
        case EYEO_XL_O:
        case EYEO_XL_E:
        case EYEO_XH_O:
        case EYEO_XH_E:
        case EYEO_Y:
        case EYEO_YL_O:
        case EYEO_YL_E:
        case EYEO_YH_O:
        case EYEO_YH_E:
            return true;
        default:
            return false;
    }
}

//-----------------------------------------------------------------------------
/* virtual */ RC PascalLwLink::DoInitialize(LibDevHandles handles)
{
    LoadLwLinkCaps();
    if (OK != m_LoadLwLinkCapsRc)
    {
        Printf(Tee::PriError, "%s : Unable to get lwlink caps!\n", __FUNCTION__);
        return m_LoadLwLinkCapsRc;
    }

    RC rc;
    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(GetGpuSubdevice()),
                                                    &m_hProfiler,
                                                    GF100_PROFILER,
                                                    nullptr));

    m_pCounterMutex = Tasker::CreateMutex("Per Gpu LwLink Counter Mutex", Tasker::mtxUnchecked);
    CHECK_RC(LwLinkImpl::DoInitialize(handles));
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC PascalLwLink::DoInitializePostDiscovery()
{
    RC rc;

    CHECK_RC(LwLinkImpl::DoInitializePostDiscovery());

    GetGpuSubdevice()->SetupFeatures();
    GetGpuSubdevice()->SetupBugs();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Check if the GpuSubdevice parent suports lwlink
//!
//! \return true if supported, false otherwise
bool PascalLwLink::DoIsSupported(LibDevHandles handles) const
{
    MASSERT(handles.size() == 0);
    const_cast<PascalLwLink*>(this)->LoadLwLinkCaps();

    // If TREX is specified, then the GPU (if present) should be ignored
    // TREX does not support traffic to GPU or vice versa
    // GPU links also can't be trained properly while TREX is enabled
    const bool trex = !!(LwLinkDevIf::TopologyManager::GetTopologyFlags() & LwLinkDevIf::TF_TREX);

    return m_bSupported && !trex;
}

//------------------------------------------------------------------------------
//! \brief Get the base for the register domain and linkId
//!
//! \param domain : Domain to get the base for
//! \param linkId : Link Id to get the base for
//!
//! \return Base for the register domain / link if one exists,
//!         ILWALID_DOMAIN_BASE if not
UINT32 PascalLwLink::GetDomainBase(RegHalDomain domain, UINT32 linkId)
{
    static const DomainBaseRegs s_LinkBaseRegs[] =
    {
        {
             MODS_PLWL0_LINK_STATE
            ,MODS_PLWLTL0_TL_TXCTRL_REG
        },
        {
             MODS_PLWL1_LINK_STATE
            ,MODS_PLWLTL1_TL_TXCTRL_REG
        },
        {
             MODS_PLWL2_LINK_STATE
            ,MODS_PLWLTL2_TL_TXCTRL_REG
        },
        {
             MODS_PLWL3_LINK_STATE
            ,MODS_PLWLTL3_TL_TXCTRL_REG
        }
    };

    if (linkId >= NUMELEMS(s_LinkBaseRegs))
    {
        Printf(Tee::PriError, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return ILWALID_DOMAIN_BASE;
    }

    switch (domain)
    {
        case RegHalDomain::DLPL:
            return Regs().LookupAddress(s_LinkBaseRegs[linkId].lwlBaseReg);
        case RegHalDomain::NTL:
            return Regs().LookupAddress(s_LinkBaseRegs[linkId].lwltlBaseReg);
        case RegHalDomain::RAW:
            return 0x0;
        default:
            Printf(Tee::PriError,
                   "%s : Invalid domain %u\n",
                   __FUNCTION__,
                   static_cast<UINT32>(domain));
            break;
    }
    return ILWALID_DOMAIN_BASE;
}

//------------------------------------------------------------------------------
//! \brief Load LwLink capabilities
void PascalLwLink::LoadLwLinkCaps()
{
    if (m_bLwCapsLoaded)
        return;

    // This particular function cannot simply be moved to an Initialize function and fail
    // if the RM control call fails because in order to call GetInterface<LwLink> to get
    // a pointer to this class, the GetInterface call needs to call IsSupported which in
    // turn needs to call LoadLwLinkCaps
    if (!GetGpuSubdevice()->IsInitialized())
    {
        m_MaxLinks = 0;
        m_bSupported = false;
        return;
    }

    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS capParams = { };
    LwRmPtr pLwRm;
    m_LoadLwLinkCapsRc = pLwRm->ControlBySubdevice(GetGpuSubdevice(),
                                        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
                                        &capParams,
                                        sizeof(capParams));

    if (OK == m_LoadLwLinkCapsRc)
    {
        const UINT32 supportedMask = (0 ? LW2080_CTRL_LWLINK_CAPS_SUPPORTED) <<
                                     (8 * (1 ? LW2080_CTRL_LWLINK_CAPS_SUPPORTED));
        m_bSupported = !!(capParams.capsTbl & supportedMask);
        if (m_bSupported)
        {
            INT32 maxLinks = Utility::BitScanReverse(capParams.discoveredLinkMask) + 1;
            SetMaxLinks(maxLinks);
        }
    }
    else
    {
        // If loading the caps actually fails report that supported is true but
        // no links are present. the RC code will be checked in initialize and
        // cause MODS to fail
        m_MaxLinks = 0;
        m_bSupported = true;
    }
    m_bLwCapsLoaded = true;
}

//------------------------------------------------------------------------------
//! \brief Save the current per-lane CRC setting
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC PascalLwLink::SavePerLaneCrcSetting()
{
    RC rc;
    // Need to read the actual state on an active link.
    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (IsLinkValid(linkId))
        {
            bool perlaneEnabled = false;
            CHECK_RC(PerLaneErrorCountsEnabled(linkId, &perlaneEnabled));
            m_SavedPerLaneEnable = perlaneEnabled ? 1 : 0;
            break;
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
GpuSubdevice* PascalLwLink::GetGpuSubdevice()
{
    return dynamic_cast<GpuSubdevice*>(GetDevice());
}

//-----------------------------------------------------------------------------
const GpuSubdevice* PascalLwLink::GetGpuSubdevice() const
{
    return dynamic_cast<const GpuSubdevice*>(GetDevice());
}

//-----------------------------------------------------------------------------
RC PascalLwLink::DoReserveCounters()
{
    RC rc;
    Tasker::MutexHolder mh(m_pCounterMutex);

    if (m_CountersReservedCount == 0)
    {
        RC pollRc;
        CHECK_RC(Tasker::Poll(Tasker::GetDefaultTimeoutMs(), [&]()->bool
        {
            pollRc = LwRmPtr()->Control(m_hProfiler,
                                        LW90CC_CTRL_CMD_LWLINK_RESERVE_COUNTERS,
                                        nullptr,
                                        0);
            const bool bDonePolling = (RC::LWRM_IN_USE != pollRc);
            if (!bDonePolling)
                pollRc.Clear();
            return bDonePolling;
        }, __FUNCTION__));
        CHECK_RC(pollRc);
    }
    m_CountersReservedCount++;

    return rc;
}

//-----------------------------------------------------------------------------
RC PascalLwLink::DoReleaseCounters()
{
    Tasker::MutexHolder mh(m_pCounterMutex);
    MASSERT(m_CountersReservedCount);

    RC rc;
    if (--m_CountersReservedCount == 0)
    {
        CHECK_RC(LwRmPtr()->Control(m_hProfiler,
                                    LW90CC_CTRL_CMD_LWLINK_RELEASE_COUNTERS,
                                    nullptr,
                                    0));
    }
    return rc;
}


//-----------------------------------------------------------------------------
RC PascalLwLink::ReserveLwLinkCounters::Release()
{
    RC rc;
    if (m_bCountersReserved)
    {
        CHECK_RC(m_pLwLink->ReleaseCounters());
        m_bCountersReserved = false;
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PascalLwLink::ReserveLwLinkCounters::Reserve()
{
    RC rc;
    if (!m_bCountersReserved)
    {
        CHECK_RC(m_pLwLink->ReserveCounters());
        m_bCountersReserved = true;
    }
    return rc;
}
