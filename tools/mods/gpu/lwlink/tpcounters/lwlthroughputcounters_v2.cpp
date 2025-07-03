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

#include "lwlthroughputcounters_v2.h"
#include "core/include/utility.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/include/testdevice.h"
#include "gpu/reghal/reghal.h"
#include "mods_reg_hal.h"

//------------------------------------------------------------------------------
namespace
{
    struct ThroughputCounterRegs
    {
        ModsGpuRegField   countLo;
        ModsGpuRegField   countHi;
        ModsGpuRegField   countOverflow;
        ModsGpuRegAddress control0;
        ModsGpuRegField   requestFilter;
        ModsGpuRegField   responseFilter;
        ModsGpuRegValue   pktRspFilter;
        ModsGpuRegField   reqlinkIdFilter;
        ModsGpuRegField   reqlinkIdMask;
        ModsGpuRegAddress miscFilter;
        ModsGpuRegField   addrLoFilter;
        ModsGpuRegField   addrHiFilter;
        ModsGpuRegField   addrLoMask;
        ModsGpuRegField   addrHiMask;
    };
    const ThroughputCounterRegs s_TpCounterRegs[] =
    {
        {
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CAPTURE_LO_COUNTER,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CAPTURE_HI_COUNTER,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_HI_ROLLOVER,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_RSP_FILTER_RESPONSEFILTERMODE,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_1_PKTFILTERRSPMODE_BOTHBASEANDCOMPRESSEDRESPONSES,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_1_REQUESTERLINKIDFILTER,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_1_REQUESTERLINKIDMASK,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_MISC_FILTER,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_ADDR_LO_FILTER_ADDRESSFILTER,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_ADDR_HI_FILTER_ADDRESSFILTER,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_ADDR_LO_MASK_ADDRESSMASK,
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_ADDR_HI_MASK_ADDRESSMASK,
        },
        {
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_CAPTURE_LO_COUNTER,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_CAPTURE_HI_COUNTER,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_HI_ROLLOVER,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_CTRL_0,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_RSP_FILTER_RESPONSEFILTERMODE,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_CTRL_1_PKTFILTERRSPMODE_BOTHBASEANDCOMPRESSEDRESPONSES,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_CTRL_1_REQUESTERLINKIDFILTER,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_CTRL_1_REQUESTERLINKIDMASK,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_MISC_FILTER,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_ADDR_LO_FILTER_ADDRESSFILTER,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_ADDR_HI_FILTER_ADDRESSFILTER,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_ADDR_LO_MASK_ADDRESSMASK,
            MODS_PLWLTLC_RXx_DEBUG_TP_CNTRy_ADDR_HI_MASK_ADDRESSMASK,
        }
    };
    RC GetTpRegs(INT32 counter, const ThroughputCounterRegs **ppRegs, UINT32 *pRegValIdx)
    {
        MASSERT(pRegValIdx);
        *pRegValIdx = 0;
        switch (counter)
        {
            case LwLinkThroughputCount::CI_TX_COUNT_0:
                *ppRegs = &s_TpCounterRegs[0];
                *pRegValIdx = 0;
                break;
            case LwLinkThroughputCount::CI_TX_COUNT_1:
                *ppRegs = &s_TpCounterRegs[0];
                *pRegValIdx = 1;
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_0:
                *ppRegs = &s_TpCounterRegs[1];
                *pRegValIdx = 0;
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_1:
                *ppRegs = &s_TpCounterRegs[1];
                *pRegValIdx = 1;
                break;
             default:
                 Printf(Tee::PriError,
                        "LwLink Version 2 Throughput Counters : Unknown throughput counter %d "
                        "when getting regs\n", counter);
                 return RC::SOFTWARE_ERROR;
        }
        return RC::OK;
    }

    struct CounterRegVals
    {
        UINT32 control0;
        UINT32 reqFilter;
        UINT32 respFilter;
    };

    RC GetCounterRegVals
    (
        const RegHal& regs,
        const LwLinkThroughputCount::Config &cfg,
        CounterRegVals *pRegs
    )
    {
        MASSERT(pRegs);

        RC rc;

        memset(pRegs, 0, sizeof(CounterRegVals));
        switch (cfg.m_Ut)
        {
            case LwLinkThroughputCount::UT_CYCLES:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT_CYCLES);
                break;
            case LwLinkThroughputCount::UT_PACKETS:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT_PACKETS);
                break;
            case LwLinkThroughputCount::UT_FLITS:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT_FLITS);
                break;
            case LwLinkThroughputCount::UT_BYTES:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT_BYTES);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 2 Throughput Counters : Unknown TL Counter Unit Type %d\n",
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
                    lwrMask |=
                        regs.SetField(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_HEAD);
                    break;
                case LwLinkThroughputCount::FF_AE:
                    lwrMask |= regs.SetField(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_AE);
                    break;
                case LwLinkThroughputCount::FF_BE:
                    lwrMask |= regs.SetField(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_BE);
                    break;
                case LwLinkThroughputCount::FF_DATA:
                    lwrMask |=
                        regs.SetField(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_DATA);
                    break;
                default:
                    Printf(Tee::PriError,
                           "LwLink Version 2 Throughput Counters : Unknown TL Counter Flit Filter "
                           "Type %d\n",
                           1 << lwrBit);
                    return RC::SOFTWARE_ERROR;
            }
            lwrBit = Utility::BitScanForward(cfg.m_Ff, lwrBit + 1);
        }
        pRegs->control0 |= lwrMask;

        switch (cfg.m_PmSize)
        {
            case LwLinkThroughputCount::PS_1:
                regs.SetField(&pRegs->control0, MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_ONE);
                break;
            case LwLinkThroughputCount::PS_2:
                regs.SetField(&pRegs->control0, MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_TWO);
                break;
            case LwLinkThroughputCount::PS_4:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_FOUR);
                break;
            case LwLinkThroughputCount::PS_8:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_EIGHT);
                break;
            case LwLinkThroughputCount::PS_16:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_SIXTEEN);
                break;
            case LwLinkThroughputCount::PS_32:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_THIRTYTWO);
                break;
            case LwLinkThroughputCount::PS_64:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_SIXTYFOUR);
                break;
            case LwLinkThroughputCount::PS_128:
                regs.SetField(&pRegs->control0,
                              MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_ONETWENTYEIGHT);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 2 Throughput Counters : Unknown TL Counter PM Size %d\n",
                       cfg.m_PmSize);
                return RC::SOFTWARE_ERROR;
        }

        pRegs->control0 |=
            regs.SetField(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_VCSETFILTERMODE_VCSET0);
        pRegs->control0 |=
            regs.SetField(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_VCSETFILTERMODE_VCSET1);

        const ModsGpuRegValue reqNopValue =
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_NOP;
        const ModsGpuRegField reqField =
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE;
        const ModsGpuRegField respField =
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_RSP_FILTER_RESPONSEFILTERMODE;

        switch (cfg.m_Pf)
        {
            case LwLinkThroughputCount::PF_ONLY_NOP:
                pRegs->reqFilter  = regs.LookupValue(reqNopValue);
                pRegs->respFilter = 0;
                break;
            case LwLinkThroughputCount::PF_DATA_ONLY:
                pRegs->reqFilter  =
                    regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_WRITE_NONPOSTED_NCNP) |
                    regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_WRITE_NONPOSTED_NCP) |
                    regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_WRITE_POSTED_NCNP) |
                    regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_WRITE_POSTED_NCP);
                pRegs->respFilter =
                    regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_RSP_FILTER_RESPONSEFILTERMODE_REQRSP_D);
                break;
            case LwLinkThroughputCount::PF_ALL_EXCEPT_NOP:
                pRegs->reqFilter  = regs.LookupMask(reqField) & ~regs.SetField(reqNopValue);
                pRegs->respFilter = regs.LookupMask(respField);
                break;
            case LwLinkThroughputCount::PF_ALL:
                pRegs->reqFilter  = regs.LookupMask(reqField);
                pRegs->respFilter = regs.LookupMask(respField);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 2 Throughput Counters : Unknown TL Counter packet "
                       "filter %d\n",
                       cfg.m_Pf);
                return RC::SOFTWARE_ERROR;
        }

        return rc;
    }

    RC ParseCounterRegVals
    (
        const RegHal& regs,
        const CounterRegVals &cntrRegs,
        LwLinkThroughputCount::Config * pCfg
    )
    {
        MASSERT(pCfg);

        RC rc;

        *pCfg = LwLinkThroughputCount::Config();

        if (regs.TestField(cntrRegs.control0,
                           MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT_CYCLES))
        {
            pCfg->m_Ut = LwLinkThroughputCount::UT_CYCLES;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT_PACKETS))
        {
            pCfg->m_Ut = LwLinkThroughputCount::UT_PACKETS;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT_FLITS))
        {
            pCfg->m_Ut = LwLinkThroughputCount::UT_FLITS;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT_BYTES))
        {
            pCfg->m_Ut = LwLinkThroughputCount::UT_BYTES;
        }
        else
        {
            Printf(Tee::PriError,
                   "LwLink Version 2 Throughput Counters : Unknown Unit Type %u parsing "
                   "counter regs\n",
                   regs.GetField(cntrRegs.control0, MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_UNIT));
            return RC::SOFTWARE_ERROR;
        }

        pCfg->m_Ff = 0;

        UINT32 lwrVal =
            regs.GetField(cntrRegs.control0, MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER);
        if (lwrVal & regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_HEAD))
            pCfg->m_Ff |= LwLinkThroughputCount::FF_HEAD;
        if (lwrVal & regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_AE))
            pCfg->m_Ff |= LwLinkThroughputCount::FF_AE;
        if (lwrVal & regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_BE))
            pCfg->m_Ff |= LwLinkThroughputCount::FF_BE;
        if (lwrVal & regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_DATA))
            pCfg->m_Ff |= LwLinkThroughputCount::FF_DATA;

        lwrVal &= ~(regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_HEAD) |
                    regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_AE)   |
                    regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_BE)   |
                    regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_FLITFILTER_DATA));
        if (lwrVal)
        {
            Printf(Tee::PriError,
                   "LwLink Version 2 Throughput Counters : Unknown Flit Filter value(s) 0x%x "
                   "parsing counter regs\n",
                   lwrVal);
            return RC::SOFTWARE_ERROR;
        }

        if (regs.TestField(cntrRegs.control0,
                           MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_ONE))
        {
            pCfg->m_PmSize = LwLinkThroughputCount::PS_1;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_TWO))
        {
            pCfg->m_PmSize = LwLinkThroughputCount::PS_2;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_FOUR))
        {
            pCfg->m_PmSize = LwLinkThroughputCount::PS_4;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_EIGHT))
        {
            pCfg->m_PmSize = LwLinkThroughputCount::PS_8;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_SIXTEEN))
        {
            pCfg->m_PmSize = LwLinkThroughputCount::PS_16;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_THIRTYTWO))
        {
            pCfg->m_PmSize = LwLinkThroughputCount::PS_32;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_SIXTYFOUR))
        {
            pCfg->m_PmSize = LwLinkThroughputCount::PS_64;
        }
        else if (regs.TestField(cntrRegs.control0,
                                MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE_ONETWENTYEIGHT))
        {
            pCfg->m_PmSize = LwLinkThroughputCount::PS_128;
        }
        else
        {
            Printf(Tee::PriError,
                   "LwLink Version 2 Throughput Counters : Unknown PM Size %u parsing "
                   "counter regs\n",
                   regs.GetField(cntrRegs.control0,
                                 MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_CTRL_0_PMSIZE));
            return RC::SOFTWARE_ERROR;
        }

        const ModsGpuRegValue reqNopValue =
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_NOP;
        const ModsGpuRegField reqField =
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE;
        const ModsGpuRegField respField =
            MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_RSP_FILTER_RESPONSEFILTERMODE;
        const UINT32 reqDataOnly  =
            regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_WRITE_NONPOSTED_NCNP) |
            regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_WRITE_NONPOSTED_NCP) |
            regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_WRITE_POSTED_NCNP) |
            regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_REQ_FILTER_REQUESTFILTERMODE_WRITE_POSTED_NCP);
        const UINT32 respDataOnly  =
            regs.LookupValue(MODS_PLWLTLC_TXx_DEBUG_TP_CNTRy_RSP_FILTER_RESPONSEFILTERMODE_REQRSP_D);

        if ((cntrRegs.reqFilter == regs.LookupValue(reqNopValue)) && (cntrRegs.respFilter == 0))
        {
            pCfg->m_Pf = LwLinkThroughputCount::PF_ONLY_NOP;
        }
        else if ((cntrRegs.reqFilter == regs.LookupMask(reqField)) &&
                 (cntrRegs.respFilter == regs.LookupMask(respField)))
        {
            pCfg->m_Pf = LwLinkThroughputCount::PF_ALL;
        }
        else if ((cntrRegs.reqFilter == reqDataOnly) &&
                 (cntrRegs.respFilter == respDataOnly))
        {
            pCfg->m_Pf = LwLinkThroughputCount::PF_DATA_ONLY;
        }
        else
        {
            const UINT32 allExceptNop = regs.LookupMask(reqField) & ~regs.SetField(reqNopValue);
            if ((cntrRegs.reqFilter == allExceptNop) &&
                (cntrRegs.respFilter == regs.LookupMask(respField)))
            {
                pCfg->m_Pf = LwLinkThroughputCount::PF_ALL_EXCEPT_NOP;
            }
            else
            {
                Printf(Tee::PriError,
                       "LwLink Version 2 Throughput Counters : Unknown Packet Filter value(s), "
                       "request filter 0x%x, response filter 0x%x parsing counter regs\n",
                       cntrRegs.reqFilter, cntrRegs.respFilter);
                return RC::SOFTWARE_ERROR;
            }
        }
        return rc;
    }
}

//------------------------------------------------------------------------------
/* virtual */ bool LwLinkThroughputCountersV2::DoIsCounterSupported
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
/* virtual */ RC LwLinkThroughputCountersV2::ReadThroughputCounts
(
    UINT32 linkId
   ,UINT32 counterMask
   ,vector<LwLinkThroughputCount> *pCounts
)
{
    RC rc;
    RegHal & regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    INT32 lwrCounter = Utility::BitScanForward(counterMask, 0);
    while (lwrCounter != -1)
    {
        LwLinkThroughputCount lwrCount = { };
        const ThroughputCounterRegs *pRegs;
        UINT32 regValIdx = 0;
        CHECK_RC(GetTpRegs(lwrCounter, &pRegs, &regValIdx));

        switch (lwrCounter)
        {
            case LwLinkThroughputCount::CI_TX_COUNT_0:
                regs.Write32(MODS_PLWLTLC_TXx_DEBUG_TP_CNTR_CTRL_CAPTURE0, linkId, 1);
                break;
            case LwLinkThroughputCount::CI_TX_COUNT_1:
                regs.Write32(MODS_PLWLTLC_TXx_DEBUG_TP_CNTR_CTRL_CAPTURE1, linkId, 1);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_0:
                regs.Write32(MODS_PLWLTLC_RXx_DEBUG_TP_CNTR_CTRL_CAPTURE0, linkId, 1);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_1:
                regs.Write32(MODS_PLWLTLC_RXx_DEBUG_TP_CNTR_CTRL_CAPTURE1, linkId, 1);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 2 Throughput Counters : Unknown throughput counter %d when "
                       "reading counts\n", lwrCounter);
                return RC::SOFTWARE_ERROR;
        }

        const UINT64 countHi = regs.Read32(pRegs->countHi, { linkId, regValIdx });
        const UINT64 countLo = regs.Read32(pRegs->countLo, { linkId, regValIdx });
        lwrCount.countValue = (countHi << 32) | countLo;
        lwrCount.bValid     = true;
        lwrCount.bOverflow  = regs.Test32(pRegs->countOverflow, 1);

        CounterRegVals lwrRegVals = { };
        lwrRegVals.control0   = regs.Read32(pRegs->control0, { linkId, regValIdx });
        lwrRegVals.reqFilter  = regs.Read32(pRegs->requestFilter, { linkId, regValIdx });
        lwrRegVals.respFilter = regs.Read32(pRegs->responseFilter, { linkId, regValIdx });
        CHECK_RC(ParseCounterRegVals(regs, lwrRegVals, &lwrCount.config));

        lwrCount.config.m_Cid = static_cast<LwLinkThroughputCount::CounterId>(lwrCounter);
        pCounts->push_back(lwrCount);
        lwrCounter = Utility::BitScanForward(counterMask, lwrCounter + 1);
    }
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkThroughputCountersV2::WriteThroughputClearRegs
(
    UINT32 linkId,
    UINT32 counterMask
)
{
    RegHal &regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    INT32 lwrCounter = Utility::BitScanForward(counterMask, 0);
    while (lwrCounter != -1)
    {
        switch (lwrCounter)
        {
            case LwLinkThroughputCount::CI_TX_COUNT_0:
                regs.Write32(MODS_PLWLTLC_TXx_DEBUG_TP_CNTR_CTRL_RESETTX0, linkId, 1);
                break;
            case LwLinkThroughputCount::CI_TX_COUNT_1:
                regs.Write32(MODS_PLWLTLC_TXx_DEBUG_TP_CNTR_CTRL_RESETTX1, linkId, 1);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_0:
                regs.Write32(MODS_PLWLTLC_RXx_DEBUG_TP_CNTR_CTRL_RESETRX0, linkId, 1);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_1:
                regs.Write32(MODS_PLWLTLC_RXx_DEBUG_TP_CNTR_CTRL_RESETRX1, linkId, 1);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 2 Throughput Counters : Unknown throughput counter %d when "
                       "clearing counts\n", lwrCounter);
                return RC::SOFTWARE_ERROR;
        }
        lwrCounter = Utility::BitScanForward(counterMask, lwrCounter + 1);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkThroughputCountersV2::WriteThroughputStartRegs
(
    UINT32 linkId,
    UINT32 counterMask,
    bool bStart
)
{
    RegHal &regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    INT32 lwrCounter = Utility::BitScanForward(counterMask, 0);
    while (lwrCounter != -1)
    {
        switch (lwrCounter)
        {
            case LwLinkThroughputCount::CI_TX_COUNT_0:
                regs.Write32(MODS_PLWLTLC_TXx_DEBUG_TP_CNTR_CTRL_ENTX0, linkId, bStart ? 1 : 0);
                break;
            case LwLinkThroughputCount::CI_TX_COUNT_1:
                regs.Write32(MODS_PLWLTLC_TXx_DEBUG_TP_CNTR_CTRL_ENTX1, linkId, bStart ? 1 : 0);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_0:
                regs.Write32(MODS_PLWLTLC_RXx_DEBUG_TP_CNTR_CTRL_ENRX0, linkId, bStart ? 1 : 0);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_1:
                regs.Write32(MODS_PLWLTLC_RXx_DEBUG_TP_CNTR_CTRL_ENRX1, linkId, bStart ? 1 : 0);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 2 Throughput Counters : Unknown throughput counter %d when "
                       "starting counters\n", lwrCounter);
                return RC::SOFTWARE_ERROR;
        }
        lwrCounter = Utility::BitScanForward(counterMask, lwrCounter + 1);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkThroughputCountersV2::WriteThroughputSetupRegs
(
    UINT32 linkId,
    const vector<LwLinkThroughputCount::Config> &configs
)
{
    RC rc;
    RegHal & regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    for (const auto & lwrConfig : configs)
    {
        CounterRegVals lwrRegVals = { };
        const ThroughputCounterRegs *pRegs;
        UINT32 regValIdx = 0;
        CHECK_RC(GetTpRegs(lwrConfig.m_Cid, &pRegs, &regValIdx));
        CHECK_RC(GetCounterRegVals(regs, lwrConfig, &lwrRegVals));
        regs.Write32(pRegs->control0,        { linkId, regValIdx }, lwrRegVals.control0);
        regs.Write32(pRegs->requestFilter,   { linkId, regValIdx }, lwrRegVals.reqFilter);
        regs.Write32(pRegs->responseFilter,  { linkId, regValIdx }, lwrRegVals.respFilter);
        regs.Write32(pRegs->pktRspFilter,    { linkId, regValIdx });
        regs.Write32(pRegs->reqlinkIdFilter, { linkId, regValIdx }, 0);
        regs.Write32(pRegs->reqlinkIdMask,   { linkId, regValIdx }, 0);
        regs.Write32(pRegs->miscFilter,      { linkId, regValIdx }, 0);
        regs.Write32(pRegs->addrLoFilter,    { linkId, regValIdx }, 0);
        regs.Write32(pRegs->addrHiFilter,    { linkId, regValIdx }, 0);
        regs.Write32(pRegs->addrLoMask,      { linkId, regValIdx }, 0);
        regs.Write32(pRegs->addrHiMask,      { linkId, regValIdx }, 0);
    }
    return rc;
}
