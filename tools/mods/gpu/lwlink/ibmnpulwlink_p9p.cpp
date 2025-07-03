/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "core/include/platform.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "ibmnpulwlink_p9p.h"
#include "ctrl/ctrl2080.h"
#include <boost/range/algorithm/find_if.hpp>

namespace
{
    #define LW_NPU_LOW_POWER                      0xE0
    #define LW_NPU_LOW_POWER_LP_MODE_ENABLE       0:0
    #define LW_NPU_LOW_POWER_LP_MODE_ENABLE_FALSE 0x0
    #define LW_NPU_LOW_POWER_LP_MODE_ENABLE_TRUE  0x1
    #define LW_NPU_LOW_POWER_LP_ONLY_MODE         1:1
    #define LW_NPU_LOW_POWER_LP_ONLY_MODE_FALSE   0x0
    #define LW_NPU_LOW_POWER_LP_ONLY_MODE_TRUE    0x1
}

using namespace LwLinkDevIf;

//-----------------------------------------------------------------------------
/* virtual */ RC IbmP9PLwLink::DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
{
    if (!IsLinkActive(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    Regs().Write32(linkId, MODS_LWLDL_TX_TXLANECRC_ENABLE, bEnable ? 1 : 0);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get whether per lane counts are enabled
//!
//! \param linkId           : Link ID to get per-lane error count enable state
//! \param pbPerLaneEnabled : Return status of per lane enabled
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmP9PLwLink::DoPerLaneErrorCountsEnabled
(
    UINT32 linkId,
    bool *pbPerLaneEnabled
)
{
    RC rc;
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    *pbPerLaneEnabled = Regs().Test32(linkId, MODS_LWLDL_TX_TXLANECRC_ENABLE, 1);
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmP9PLwLink::DoClearLPCounts(UINT32 linkId)
{
    Regs().Write32(linkId, MODS_LWLDL_TOP_STATS_CTRL_CLEAR_ALL_CLEAR);
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmP9PLwLink::DoGetLPEntryOrExitCount
(
    UINT32 linkId,
    bool entry,
    UINT32 *pCount,
    bool *pbOverflow
)
{
    MASSERT(nullptr != pCount);

    if (entry)
    {
        *pCount = Regs().Read32(linkId, MODS_LWLDL_STATS_D_NUM_TX_LP_ENTER);
    }
    else
    {
        *pCount = Regs().Read32(linkId, MODS_LWLDL_STATS_D_NUM_TX_LP_EXIT);
    }

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmP9PLwLink::DoGetLinkPowerStateStatus
(
    UINT32 linkId,
    LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
) const
{
    MASSERT(nullptr != pLinkPowerStatus);

    RC rc;

    // TODO confirm LOW_POWER address/fields in NTL on P9'

    UINT32 val = 0;
    CHECK_RC(const_cast<IbmP9PLwLink*>(this)->RegRd(linkId, RegHalDomain::NTL, LW_NPU_LOW_POWER, &val));

    pLinkPowerStatus->rxHwControlledPowerState =
        FLD_TEST_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _TRUE, val) &&
        FLD_TEST_DRF(_NPU, _LOW_POWER, _LP_ONLY_MODE, _FALSE, val);
    pLinkPowerStatus->rxSubLinkLwrrentPowerState =
        FLD_TEST_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _FALSE, val)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->rxSubLinkConfigPowerState =
        FLD_TEST_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _FALSE, val)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;

    pLinkPowerStatus->txHwControlledPowerState = pLinkPowerStatus->rxHwControlledPowerState;
    pLinkPowerStatus->txSubLinkLwrrentPowerState = pLinkPowerStatus->rxSubLinkLwrrentPowerState;
    pLinkPowerStatus->txSubLinkConfigPowerState = pLinkPowerStatus->rxSubLinkConfigPowerState;

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmP9PLwLink::DoRequestPowerState
(
    UINT32 linkId
  , LwLinkPowerState::SubLinkPowerState state
  , bool bHwControlled
)
{
    RC rc;
    CHECK_RC(CheckPowerStateAvailable(state, bHwControlled));

    // TODO confirm LOW_POWER address/fields in NTL on P9'
    UINT32 val = 0;
    CHECK_RC(RegRd(linkId, RegHalDomain::NTL, LW_NPU_LOW_POWER, &val));

    if (bHwControlled)
    {
        val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _TRUE, val);
        val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_ONLY_MODE, _FALSE, val);
    }
    else
    {
        switch (state)
        {
        case LwLinkPowerState::SLS_PWRM_LP:
            val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _TRUE, val);
            val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_ONLY_MODE, _TRUE, val);
            break;
        case LwLinkPowerState::SLS_PWRM_FB:
            val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _FALSE, val);
            val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_ONLY_MODE, _FALSE, val);
            break;
        default:
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Invalid SubLinkPowerState = %d\n",
                   __FUNCTION__, static_cast<UINT32>(state));
            return RC::BAD_PARAMETER;
        }
    }

    CHECK_RC(RegWr(linkId, RegHalDomain::NTL, LW_NPU_LOW_POWER, val));

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Save the current per-lane CRC setting
//!
//! \return OK if successful, not OK otherwise
RC IbmP9PLwLink::SavePerLaneCrcSetting()
{
    RC rc;
    // Need to read the actual state on an active link.
    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (IsLinkActive(linkId))
        {
            m_SavedPerLaneEnable = Regs().Read32(0, MODS_LWLDL_TX_TXLANECRC_ENABLE);
            break;
        }
    }

    return rc;
}

//--------------------------------------------------------------------------
//! \brief Return whether the domain requires a VBIOS
//!
//! \param domain  : one of the known hw domains (DLPL/NTL/GLUE/PHY)
//!
//! \return true if the domain requires a byte swap, false otherwise
//!
bool IbmP9PLwLink::DomainRequiresByteSwap(RegHalDomain domain) const
{
    return (domain == RegHalDomain::LWLDL);
}
