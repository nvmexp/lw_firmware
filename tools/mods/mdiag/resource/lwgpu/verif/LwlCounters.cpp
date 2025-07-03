/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/utils/cpumodel.h" // CPUModel()

#include "ctrl/ctrl90cc.h" ///ctrl90cclwlink.h" // LW90CC_CTRL_CMD_LWLINK_RESERVE_COUNTERS
#include "class/cl90cc.h" // GF100_PROFILER
#include "LwlCounters.h"

LwlCounters::LwlCounters()
    : m_pLwgpu(nullptr),
    m_SubDevObj(0),
    m_LinkMask(0),
    m_CheckedLinkMask(0),
    m_bEnabled(false)
{
    m_pLwRm = LwRmPtr().Get();
}

void LwlCounters::Setup(GpuVerif* verifier)
{
    m_pLwgpu = verifier->LWGpu();
    m_pLwRm = verifier->GetLwRmPtr();
    Probe(verifier->Params());
    Init();
}

RC LwlCounters::Probe(const ArgReader *params)
{
    RC rc;
    m_bEnabled = false;

    // 1. Check API & Links

    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS lwlCaps = {0};

    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pLwgpu->GetGpuSubdevice(),
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
        &lwlCaps, sizeof(lwlCaps)));

    if (!LW2080_CTRL_LWLINK_GET_CAP(((UINT08*)&lwlCaps.capsTbl), LW2080_CTRL_LWLINK_CAPS_SUPPORTED))
    {
        DebugPrintf("LwlCounters Probe: LWLink is not supported.\n");
        return rc;
    }
    m_LinkMask = lwlCaps.enabledLinkMask;
    DebugPrintf("LwlCounters Probe: detected linkmask 0x%x\n",
        m_LinkMask);

    if (params->ParamPresent("-lwlink_counters_linkmask"))
    {
        m_CheckedLinkMask = params->ParamNUnsigned("-lwlink_counters_linkmask", 0);
        DebugPrintf("LwlCounters Probe: user specified linkmask 0x%x.\n", m_CheckedLinkMask);
        UINT32 link;
        FOR_EACH_INDEX_IN_MASK(32, link, m_CheckedLinkMask)
        {
            if (0 == ((1 << link) & m_LinkMask))
            {
                ErrPrintf("LwlCounters Probe: user specified link#%u isn't present in detected linkmask 0x%x.\n",
                    link, m_LinkMask);
                return RC::ILWALID_INPUT;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    if (0 == m_CheckedLinkMask)
    {
        DebugPrintf("LwlCounters Probe: No LWLink link# need to be checked.\n");
        return rc;
    }

    // 2. RM counter only supports Pascal & Later.

    if (m_pLwgpu->GetGpuDevice()->GetFamily() < GpuDevice::Pascal)
    {
        DebugPrintf("LwlCounters Probe: only support Pascal & later classes, LWLink Counters will not be enabled.\n");
        return rc;
    }

    // 3. Emulation and fmod only lwrrently.

    LW2080_CTRL_GPU_GET_SIMULATION_INFO_PARAMS simInfo = {0};
    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pLwgpu->GetGpuSubdevice(),
        LW2080_CTRL_CMD_GPU_GET_SIMULATION_INFO,
        &simInfo, sizeof(simInfo)));
    if (!(LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_FMODEL == simInfo.type ||
        LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_EMU == simInfo.type ||
        LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_EMU_LOW_POWER == simInfo.type))
    {
        DebugPrintf("LwlCounters Probe: only support Fmodel and Emulation, LWLink Counters will not be enabled.\n");
        return rc;
    }

    // 4. CPUModel enabled.

    if (!CPUModel::Enabled())
    {
        DebugPrintf("LwlCounters Probe: CPUModel not enabled, LWLink Counters will not be enabled.\n");
        return rc;
    }

    m_bEnabled = true;

    return rc;
}

RC LwlCounters::Config()
{
    UINT32 cfgMask = 0;

    // Count packets
    cfgMask = FLD_SET_DRF(90CC, _CTRL_LWLINK_COUNTER_TL_CFG, _UNIT, _PACKETS, cfgMask);
    // The following fields are outdated for volta but needed for pascal functionality
    cfgMask |= DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_CFG_PKTFILTER);
    cfgMask |= DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_CFG_ATTRFILTER);
    cfgMask = FLD_SET_DRF(90CC, _CTRL_LWLINK_COUNTER_TL_CFG, _PMSIZE, _1, cfgMask);

    RC rc;
    // Set up cfgMask for the SET_TL_COUNTER_CFG call coming up so we get
    // the desired counter behavior
    LW90CC_CTRL_LWLINK_SET_TL_COUNTER_CFG_PARAMS configs = {0};
    // Apply the cfgMask to relevant links in the parameter struct
    configs.linkMask = m_LinkMask;
    UINT32 link;
    FOR_EACH_INDEX_IN_MASK(32, link, m_LinkMask)
    {
        configs.linkCfg[link].tx0Cfg = cfgMask;
        configs.linkCfg[link].rx0Cfg = cfgMask;
        configs.linkCfg[link].tx1Cfg = cfgMask;
        configs.linkCfg[link].rx1Cfg = cfgMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //RM call to apply our specified counter configuration to the first GPU
    CHECK_RC(m_pLwRm->Control(m_SubDevObj,
        LW90CC_CTRL_CMD_LWLINK_SET_TL_COUNTER_CFG,
        &configs,
        sizeof(configs)));

    //unfreeze counters
    LW90CC_CTRL_LWLINK_SET_COUNTERS_FROZEN_PARAMS freezeParams = {0};
    freezeParams.linkMask = m_LinkMask;
    FOR_EACH_INDEX_IN_MASK(32, link, m_LinkMask)
    {
        freezeParams.counterMask[link] = 0xFFFFFFFF;
    }
    FOR_EACH_INDEX_IN_MASK_END;
    freezeParams.bFrozen = LW_FALSE;
    CHECK_RC(m_pLwRm->Control(m_SubDevObj,
        LW90CC_CTRL_CMD_LWLINK_SET_COUNTERS_FROZEN,
        &freezeParams,
        sizeof(freezeParams)));

    return rc;
}

namespace
{
    inline UINT32 LwlCountersGetTxMask()
    {
        return DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_TX0) |
            DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_TX1);
    }

    inline UINT32 LwlCountersGetRxMask()
    {
        return DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_RX0) |
            DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_RX1);
    }

    inline UINT32 LwlCountersGetMask()
    {
        return LwlCountersGetTxMask() | LwlCountersGetRxMask();
    }

    struct LwlCounterVal
    {
        UINT64 m_TxVal;
        UINT64 m_RxVal;
        UINT64 m_Overflow;

        LwlCounterVal()
            : m_TxVal(0),
            m_RxVal(0),
            m_Overflow(0)
        {}

        void Decode(const LW90CC_CTRL_LWLINK_GET_COUNTERS_PARAMS& parm,
            UINT32 link)
        {
            MASSERT(0 == m_TxVal && 0 == m_RxVal && 0 == m_Overflow);
            UINT32 type;
            UINT32 counterMask = LwlCountersGetMask();
            FOR_EACH_INDEX_IN_MASK(32, type, counterMask)
            {
                UINT64 counterValue = parm.counterData[link].counters[type];
                if ((1 << type) & LwlCountersGetTxMask())
                {
                    m_TxVal += counterValue;
                }
                if ((1 << type) & LwlCountersGetRxMask())
                {
                    m_RxVal += counterValue;
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;
            m_Overflow = parm.counterData[link].overflowMask;
        }
    };

} // namespace

RC LwlCounters::Reserve()
{
    RC rc;

    CHECK_RC(m_pLwRm->Alloc(
        m_pLwRm->GetSubdeviceHandle(m_pLwgpu->GetGpuSubdevice()),
        &m_SubDevObj, GF100_PROFILER, NULL));

    DebugPrintf("LwlCounters Reserve: calling LW90CC_CTRL_CMD_LWLINK_RESERVE_COUNTERS.\n");
    CHECK_RC(m_pLwRm->Control(m_SubDevObj,
        LW90CC_CTRL_CMD_LWLINK_RESERVE_COUNTERS,
        NULL,
        0));

    return rc;
}

RC LwlCounters::Init()
{
    if (!m_bEnabled)
    {
        return OK;
    }
    Cleanup();

    RC rc;

    MASSERT(0 == m_SubDevObj);
    CHECK_RC(Reserve());
    CHECK_RC(Config());

    // Get counter value for debugging purpose.
    GetCounters(false);

    return rc;
}

RC LwlCounters::GetCounters(bool bCheck) const
{
    if (!m_bEnabled)
    {
        return OK;
    }
    RC rc;

    MASSERT(m_SubDevObj != 0);

    LW90CC_CTRL_LWLINK_GET_COUNTERS_PARAMS counters = {0};
    counters.linkMask = m_LinkMask;

    UINT32 counterMask = LwlCountersGetMask();
    UINT32 link;
    FOR_EACH_INDEX_IN_MASK(32, link, m_LinkMask)
    {
        counters.counterData[link].counterMask = counterMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;
    DebugPrintf("LwlCounters GetCounters: calling LW90CC_CTRL_CMD_LWLINK_GET_COUNTERS.\n");
    CHECK_RC(m_pLwRm->Control(m_SubDevObj,
        LW90CC_CTRL_CMD_LWLINK_GET_COUNTERS,
        &counters,
        sizeof(counters)));

    int errors = 0;
    FOR_EACH_INDEX_IN_MASK(32, link, m_LinkMask)
    {
        LwlCounterVal values;
        values.Decode(counters, link);
        DebugPrintf("LwlCounters GetCounters: Tx/Rx counter value of link %02u: 0x%llx/%llx.\n",
            link, values.m_TxVal, values.m_RxVal);
        DebugPrintf("LwlCounters GetCounters: overflowMask of link %02u: 0x%llx.\n",
            link, values.m_Overflow);

        if (bCheck && ((1 << link) & m_CheckedLinkMask))
        {
            DebugPrintf("LwlCounters GetCounters: check of link %02u requred.\n", link);
            if (0 == values.m_TxVal || 0 == values.m_RxVal)
            {
                ErrPrintf("LwlCounters: Counter values Tx/Rx 0x%llx/0x%llx, "
                    "either Tx or Rx counter value of link# %u is 0.\n",
                    values.m_TxVal, values.m_RxVal, link);
                errors ++;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    if (errors > 0)
    {
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

TestEnums::TEST_STATUS LwlCounters::Check(ICheckInfo* info)
{
    TestEnums::TEST_STATUS status = TestEnums::TEST_SUCCEEDED;
    if (m_bEnabled)
    {
        if (GetCounters(true) != OK)
        {
            ErrPrintf("LwlCounters GetCounters failed! see bug 1824408 & 1811911\n");
            status = TestEnums::TEST_FAILED;
        }
        else
        {
            InfoPrintf("LwlCounters GetCounters passed! see bug 1824408 & 1811911\n");
        }
    }
    Cleanup();
    return status;
}

void LwlCounters::Cleanup()
{
    if (!m_bEnabled)
    {
        return;
    }

    if (0 == m_SubDevObj)
    {
        Reserve();
    }

    LW90CC_CTRL_LWLINK_CLEAR_COUNTERS_PARAMS clearParms = {0};
    clearParms.linkMask = m_LinkMask;
    UINT32 counterMask = LwlCountersGetMask();
    UINT32 link;
    FOR_EACH_INDEX_IN_MASK(32, link, m_LinkMask)
    {
        clearParms.counterMask[link] = counterMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;
    DebugPrintf("LwlCounters Cleaup: calling LW90CC_CTRL_CMD_LWLINK_CLEAR_COUNTERS.\n");
    m_pLwRm->Control(m_SubDevObj,
        LW90CC_CTRL_CMD_LWLINK_CLEAR_COUNTERS,
        &clearParms,
        sizeof(clearParms));
    DebugPrintf("LwlCounters Cleanup: calling LW90CC_CTRL_CMD_LWLINK_RELEASE_COUNTERS.\n");
    m_pLwRm->Control(m_SubDevObj,
        LW90CC_CTRL_CMD_LWLINK_RELEASE_COUNTERS,
        NULL,
        0);
    m_pLwRm->Free(m_SubDevObj);
    m_SubDevObj = 0;
}

