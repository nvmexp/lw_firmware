/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlthroughputcounters_v1.h"
#include "core/include/utility.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/include/testdevice.h"
#include "gpu/reghal/reghal.h"
#include "mods_reg_hal.h"

//------------------------------------------------------------------------------
namespace
{
    struct ThroughputCounterFields
    {
        ModsGpuRegField countLo;
        ModsGpuRegField countHi;
        ModsGpuRegField countOverflow;
    };
    const ThroughputCounterFields s_TpCounterFields[] =
    {
        {
            MODS_PLWLTL0_TL_TPCNTLO_REG_COUNT,
            MODS_PLWLTL0_TL_TPCNTHI_REG_COUNT,
            MODS_PLWLTL0_TL_TPCNTHI_REG_ROLLOVER
        },
        {
            MODS_PLWLTL1_TL_TPCNTLO_REG_COUNT,
            MODS_PLWLTL1_TL_TPCNTHI_REG_COUNT,
            MODS_PLWLTL1_TL_TPCNTHI_REG_ROLLOVER
        },
        {
            MODS_PLWLTL2_TL_TPCNTLO_REG_COUNT,
            MODS_PLWLTL2_TL_TPCNTHI_REG_COUNT,
            MODS_PLWLTL2_TL_TPCNTHI_REG_ROLLOVER
        },
        {
            MODS_PLWLTL3_TL_TPCNTLO_REG_COUNT,
            MODS_PLWLTL3_TL_TPCNTHI_REG_COUNT,
            MODS_PLWLTL3_TL_TPCNTHI_REG_ROLLOVER
        }
    };

    const ModsGpuRegAddress s_TpCountStartRegs[] =
    {
        MODS_PLWLTL0_TL_COUNT_START
       ,MODS_PLWLTL1_TL_COUNT_START
       ,MODS_PLWLTL2_TL_COUNT_START
       ,MODS_PLWLTL3_TL_COUNT_START
    };

    const ModsGpuRegAddress s_TpCountConfigRegs[] =
    {
        MODS_PLWLTL0_TL_TPCNTCTL_REG
       ,MODS_PLWLTL1_TL_TPCNTCTL_REG
       ,MODS_PLWLTL2_TL_TPCNTCTL_REG
       ,MODS_PLWLTL3_TL_TPCNTCTL_REG
    };

    RC CounterToRegValIdx(INT32 counter, UINT32 *pRegValIdx)
    {
        MASSERT(pRegValIdx);
        *pRegValIdx = 0;
        switch (counter)
        {
            case LwLinkThroughputCount::CI_TX_COUNT_0:
                *pRegValIdx = 0;
                break;
            case LwLinkThroughputCount::CI_TX_COUNT_1:
                *pRegValIdx = 1;
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_0:
                *pRegValIdx = 2;
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_1:
                *pRegValIdx = 3;
                break;
             default:
                 Printf(Tee::PriError,
                        "LwLink Version 1 Throughput Counters : Unknown throughput counter %d "
                        "querying reg index\n", counter);
                 return RC::SOFTWARE_ERROR;
        }
        return RC::OK;
    }

    RC GetCounterRegVal
    (
        const RegHal& regs,
        const LwLinkThroughputCount::Config &cfg,
        UINT32 *pControl0
    )
    {
        MASSERT(pControl0);

        RC rc;

        *pControl0 = 0;
        switch (cfg.m_Ut)
        {
            case LwLinkThroughputCount::UT_CYCLES:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT_CYCLES);
                break;
            case LwLinkThroughputCount::UT_PACKETS:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT_PACKETS);
                break;
            case LwLinkThroughputCount::UT_FLITS:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT_FLITS);
                break;
            case LwLinkThroughputCount::UT_BYTES:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT_BYTES);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 1 Throughput Counters : Unknown TL Counter Unit Type %d\n",
                       cfg.m_Ut);
                return RC::SOFTWARE_ERROR;
        }

        UINT32 lwrMask = 0;
        INT32 lwrBit = Utility::BitScanForward(cfg.m_Ff);
        while (lwrBit != -1)
        {
            switch (static_cast<LwLinkThroughputCount::FlitFilter>(1 << lwrBit))
            {
                case LwLinkThroughputCount::FF_HEAD:
                    lwrMask |= regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_HEAD);
                    break;
                case LwLinkThroughputCount::FF_AE:
                    lwrMask |= regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_AE);
                    break;
                case LwLinkThroughputCount::FF_BE:
                    lwrMask |= regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_BE);
                    break;
                case LwLinkThroughputCount::FF_DATA:
                    lwrMask |= regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_DATA);
                    break;
                default:
                    Printf(Tee::PriError,
                           "LwLink Version 1 Throughput Counters : Unknown TL Counter Flit Filter "
                           "Type %d\n",
                           1 << lwrBit);
                    return RC::SOFTWARE_ERROR;
            }
            lwrBit = Utility::BitScanForward(cfg.m_Ff, lwrBit + 1);
        }
        *pControl0 |= lwrMask;

        switch (cfg.m_PmSize)
        {
            case LwLinkThroughputCount::PS_1:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_ONE);
                break;
            case LwLinkThroughputCount::PS_2:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_TWO);
                break;
            case LwLinkThroughputCount::PS_4:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_FOUR);
                break;
            case LwLinkThroughputCount::PS_8:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_EIGHT);
                break;
            case LwLinkThroughputCount::PS_16:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_SIXTEEN);
                break;
            case LwLinkThroughputCount::PS_32:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_THIRTYTWO);
                break;
            case LwLinkThroughputCount::PS_64:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_SIXTYFOUR);
                break;
            case LwLinkThroughputCount::PS_128:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_ONETWENTYEIGHT);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 1 Throughput Counters : Unknown TL Counter PM Size %d\n",
                       cfg.m_PmSize);
                return RC::SOFTWARE_ERROR;
        }

        switch (cfg.m_Pf)
        {
            case LwLinkThroughputCount::PF_ONLY_NOP:
                regs.SetField(pControl0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER_NOP);
                break;
            case LwLinkThroughputCount::PF_ALL_EXCEPT_NOP:
                *pControl0 |= regs.LookupMask(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER);
                *pControl0 &= ~regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER_NOP);
                break;
            case LwLinkThroughputCount::PF_DATA_ONLY:
                *pControl0 |= regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER_RESPD);
                *pControl0 |= regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER_WRITE);
                break;
            case LwLinkThroughputCount::PF_ALL:
                *pControl0 |= regs.LookupMask(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 1 Throughput Counters : Unknown TL Counter packet "
                       "filter %d\n",
                       cfg.m_PmSize);
                return RC::SOFTWARE_ERROR;
        }
        *pControl0 |= regs.LookupMask(MODS_PLWLTL0_TL_TPCNTCTL_REG_ATTRFILTER);

        return rc;
    }
    RC ParseCounterRegVal
    (
        const RegHal& regs,
        UINT32 control0,
        LwLinkThroughputCount::Config * pCfg
    )
    {
        MASSERT(pCfg);
        RC rc;

        *pCfg = LwLinkThroughputCount::Config();

        if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT_CYCLES))
            pCfg->m_Ut = LwLinkThroughputCount::UT_CYCLES;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT_PACKETS))
            pCfg->m_Ut = LwLinkThroughputCount::UT_PACKETS;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT_FLITS))
            pCfg->m_Ut = LwLinkThroughputCount::UT_FLITS;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT_BYTES))
            pCfg->m_Ut = LwLinkThroughputCount::UT_BYTES;
        else
        {
            Printf(Tee::PriError,
                   "LwLink Version 1 Throughput Counters : Unknown Unit Type %u parsing "
                   "counter regs\n",
                   regs.GetField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_UNIT));
            return RC::SOFTWARE_ERROR;
        }

        pCfg->m_Ff = 0;

        UINT32 lwrVal = regs.GetField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER);
        if (lwrVal & regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_HEAD))
            pCfg->m_Ff |= LwLinkThroughputCount::FF_HEAD;
        if (lwrVal & regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_AE))
            pCfg->m_Ff |= LwLinkThroughputCount::FF_AE;
        if (lwrVal & regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_BE))
            pCfg->m_Ff |= LwLinkThroughputCount::FF_BE;
        if (lwrVal & regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_DATA))
            pCfg->m_Ff |= LwLinkThroughputCount::FF_DATA;

        lwrVal &= ~(regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_HEAD) |
                    regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_AE)   |
                    regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_BE)   |
                    regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_FLITFILTER_DATA));
        if (lwrVal)
        {
            Printf(Tee::PriError,
                   "LwLink Version 1 Throughput Counters : Unknown Flit Filter value(s) 0x%x "
                   "parsing counter regs\n",
                   lwrVal);
            return RC::SOFTWARE_ERROR;
        }

        if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_ONE))
            pCfg->m_PmSize = LwLinkThroughputCount::PS_1;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_TWO))
            pCfg->m_PmSize = LwLinkThroughputCount::PS_2;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_FOUR))
            pCfg->m_PmSize = LwLinkThroughputCount::PS_4;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_EIGHT))
            pCfg->m_PmSize = LwLinkThroughputCount::PS_8;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_SIXTEEN))
            pCfg->m_PmSize = LwLinkThroughputCount::PS_16;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_THIRTYTWO))
            pCfg->m_PmSize = LwLinkThroughputCount::PS_32;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_SIXTYFOUR))
            pCfg->m_PmSize = LwLinkThroughputCount::PS_64;
        else if (regs.TestField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE_ONETWENTYEIGHT))
            pCfg->m_PmSize = LwLinkThroughputCount::PS_128;
        else
        {
            Printf(Tee::PriError,
                   "LwLink Version 1 Throughput Counters : Unknown PM Size %u parsing "
                   "counter regs\n",
                   regs.GetField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PMSIZE));
            return RC::SOFTWARE_ERROR;
        }

        const UINT32 pfOnlyData =
            regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER_RESPD) |
            regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER_WRITE);

        UINT32 pfVal = regs.GetField(control0, MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER);
        if (pfVal == regs.LookupValue(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER_NOP))
        {
            pCfg->m_Pf = LwLinkThroughputCount::PF_ONLY_NOP;
        }
        else if (pfVal == regs.LookupMask(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER))
        {
            pCfg->m_Pf = LwLinkThroughputCount::PF_ALL;
        }
        else if (pfVal == pfOnlyData)
        {
            pCfg->m_Pf = LwLinkThroughputCount::PF_DATA_ONLY;
        }
        else
        {
            const UINT32 allExceptNop = regs.LookupMask(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER) &
                                       ~regs.SetField(MODS_PLWLTL0_TL_TPCNTCTL_REG_PKTFILTER_NOP);
            if (pfVal == allExceptNop)
            {
                pCfg->m_Pf = LwLinkThroughputCount::PF_ALL_EXCEPT_NOP;
            }
            else
            {
                Printf(Tee::PriError,
                       "LwLink Version 1 Throughput Counters : Unknown Packet Filter value(s) "
                       "0x%x parsing counter regs\n",
                       pfVal);
                return RC::SOFTWARE_ERROR;
            }
        }
        return rc;
    }
}

//------------------------------------------------------------------------------
/* virtual */ bool LwLinkThroughputCountersV1::DoIsCounterSupported
(
    LwLinkThroughputCount::CounterId cid
) const
{
    return ((cid == LwLinkThroughputCount::CI_TX_COUNT_0) ||
            (cid == LwLinkThroughputCount::CI_TX_COUNT_1) ||
            (cid == LwLinkThroughputCount::CI_RX_COUNT_0) ||
            (cid == LwLinkThroughputCount::CI_RX_COUNT_1));
}

//------------------------------------------------------------------------------
/* virtual */  RC LwLinkThroughputCountersV1::ReadThroughputCounts
(
    UINT32 linkId
   ,UINT32 counterMask
   ,vector<LwLinkThroughputCount> *pCounts
)
{
    MASSERT(pCounts);
    MASSERT(linkId < NUMELEMS(s_TpCounterFields));
    MASSERT(linkId < NUMELEMS(s_TpCountConfigRegs));

    RC rc;
    RegHal & regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    INT32 lwrCounter = Utility::BitScanForward(counterMask, 0);
    while (lwrCounter != -1)
    {
        LwLinkThroughputCount lwrCount = { };
        UINT32 regValIdx = 0;
        CHECK_RC(CounterToRegValIdx(lwrCounter, &regValIdx));

        UINT64 countLo;
        UINT64 countHi1;
        UINT64 countHi2;
        do
        {
            countHi1 = regs.Read32(s_TpCounterFields[linkId].countHi, regValIdx);
            countLo  = regs.Read32(s_TpCounterFields[linkId].countLo, regValIdx);
            countHi2 = regs.Read32(s_TpCounterFields[linkId].countHi, regValIdx);
        } while (countHi1 != countHi2);

        lwrCount.countValue = (countHi1 << 32) | countLo;
        lwrCount.bValid     = true;
        lwrCount.bOverflow  = regs.Test32(s_TpCounterFields[linkId].countOverflow, 1);

        const UINT32 control0 = regs.Read32(s_TpCountConfigRegs[linkId], regValIdx);
        CHECK_RC(ParseCounterRegVal(regs, control0, &lwrCount.config));

        lwrCount.config.m_Cid = static_cast<LwLinkThroughputCount::CounterId>(lwrCounter);
        pCounts->push_back(lwrCount);
        lwrCounter = Utility::BitScanForward(counterMask, lwrCounter + 1);
    }
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkThroughputCountersV1::WriteThroughputClearRegs
(
    UINT32 linkId,
    UINT32 counterMask
)
{
    MASSERT(linkId < NUMELEMS(s_TpCountStartRegs));
    RegHal &regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    INT32 lwrCounter = Utility::BitScanForward(counterMask, 0);
    UINT32 clearVal = regs.Read32(s_TpCountStartRegs[linkId]);
    while (lwrCounter != -1)
    {
        switch (lwrCounter)
        {
           case LwLinkThroughputCount::CI_TX_COUNT_0:
               regs.SetField(&clearVal, MODS_PLWLTL0_TL_COUNT_START_RESETTX0, 1);
               break;
           case LwLinkThroughputCount::CI_TX_COUNT_1:
               regs.SetField(&clearVal, MODS_PLWLTL0_TL_COUNT_START_RESETTX1, 1);
               break;
           case LwLinkThroughputCount::CI_RX_COUNT_0:
               regs.SetField(&clearVal, MODS_PLWLTL0_TL_COUNT_START_RESETRX0, 1);
               break;
           case LwLinkThroughputCount::CI_RX_COUNT_1:
               regs.SetField(&clearVal, MODS_PLWLTL0_TL_COUNT_START_RESETRX1, 1);
               break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 1 Throughput Counters : Unknown throughput counter %d when "
                       "clearing counts\n", lwrCounter);
                return RC::SOFTWARE_ERROR;
        }
        lwrCounter = Utility::BitScanForward(counterMask, lwrCounter + 1);
    }

    regs.Write32(s_TpCountStartRegs[linkId], clearVal);
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkThroughputCountersV1::WriteThroughputStartRegs
(
    UINT32 linkId,
    UINT32 counterMask,
    bool bStart
)
{
    MASSERT(linkId < NUMELEMS(s_TpCountStartRegs));
    RegHal &regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    INT32 lwrCounter = Utility::BitScanForward(counterMask, 0);
    UINT32 clearVal = regs.Read32(s_TpCountStartRegs[linkId]);
    while (lwrCounter != -1)
    {
        switch (lwrCounter)
        {
            case LwLinkThroughputCount::CI_TX_COUNT_0:
                regs.SetField(&clearVal, MODS_PLWLTL0_TL_COUNT_START_ENTX0, bStart ? 1 : 0);
                break;
            case LwLinkThroughputCount::CI_TX_COUNT_1:
                regs.SetField(&clearVal, MODS_PLWLTL0_TL_COUNT_START_ENTX1, bStart ? 1 : 0);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_0:
                regs.SetField(&clearVal, MODS_PLWLTL0_TL_COUNT_START_ENRX0, bStart ? 1 : 0);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_1:
                regs.SetField(&clearVal, MODS_PLWLTL0_TL_COUNT_START_ENRX1, bStart ? 1 : 0);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 1 Throughput Counters : Unknown throughput counter %d when "
                       "starting counters\n", lwrCounter);
                return RC::SOFTWARE_ERROR;
        }
        lwrCounter = Utility::BitScanForward(counterMask, lwrCounter + 1);
    }

    regs.Write32(s_TpCountStartRegs[linkId], clearVal);
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkThroughputCountersV1::WriteThroughputSetupRegs
(
    UINT32 linkId,
    const vector<LwLinkThroughputCount::Config> &configs
)
{
    MASSERT(linkId < NUMELEMS(s_TpCountConfigRegs));

    RC rc;
    RegHal & regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    for (const auto & lwrConfig : configs)
    {
        UINT32 regValIdx = 0;
        CHECK_RC(CounterToRegValIdx(lwrConfig.m_Cid, &regValIdx));
        UINT32 control0;
        CHECK_RC(GetCounterRegVal(regs, lwrConfig, &control0));
        regs.Write32(s_TpCountConfigRegs[linkId], regValIdx, control0);
    }
    return rc;
}

