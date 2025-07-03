/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlthroughputcounters_v4.h"

#include "device/interface/lwlink/lwlregs.h"
#include "gpu/include/testdevice.h"
#include "gpu/reghal/reghal.h"
#include "mods_reg_hal.h"

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkThroughputCountersV4::WriteThroughputClearRegs
(
    UINT32 linkId,
    UINT32 counterMask
)
{
    RegHal &regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    INT32 lwrCounter = Utility::BitScanForward(counterMask, 0);
    while (lwrCounter != -1)
    {
        const LwLinkThroughputCount::CounterId counterId =
            static_cast<LwLinkThroughputCount::CounterId>(lwrCounter);
        const UINT32 counterIdx = LwLinkThroughputCount::CounterIdToIndex(counterId);

        switch (counterId)
        {
            case LwLinkThroughputCount::CI_TX_COUNT_0:
            case LwLinkThroughputCount::CI_TX_COUNT_1:
            case LwLinkThroughputCount::CI_TX_COUNT_2:
            case LwLinkThroughputCount::CI_TX_COUNT_3:
                regs.Write32(linkId, MODS_LWLTLC_TX_LNK_DEBUG_TP_CNTR_CTRL_0_RESET, counterIdx, 1);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_0:
            case LwLinkThroughputCount::CI_RX_COUNT_1:
            case LwLinkThroughputCount::CI_RX_COUNT_2:
            case LwLinkThroughputCount::CI_RX_COUNT_3:
                regs.Write32(linkId, MODS_LWLTLC_RX_LNK_DEBUG_TP_CNTR_CTRL_0_RESET, counterIdx, 1);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 4 Throughput Counters : Unknown throughput counter %d "
                       "when clearing counters\n", lwrCounter);
                return RC::SOFTWARE_ERROR;
        }
        lwrCounter = Utility::BitScanForward(counterMask, lwrCounter + 1);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkThroughputCountersV4::WriteThroughputStartRegs
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
        const LwLinkThroughputCount::CounterId counterId =
            static_cast<LwLinkThroughputCount::CounterId>(lwrCounter);
        const UINT32 counterIdx = LwLinkThroughputCount::CounterIdToIndex(counterId);

        switch (counterId)
        {
            case LwLinkThroughputCount::CI_TX_COUNT_0:
            case LwLinkThroughputCount::CI_TX_COUNT_1:
            case LwLinkThroughputCount::CI_TX_COUNT_2:
            case LwLinkThroughputCount::CI_TX_COUNT_3:
                regs.Write32(linkId,
                             MODS_LWLTLC_TX_LNK_DEBUG_TP_CNTR_CTRL_0_ENABLE,
                             counterIdx,
                             bStart ? 1 : 0);
                break;
            case LwLinkThroughputCount::CI_RX_COUNT_0:
            case LwLinkThroughputCount::CI_RX_COUNT_1:
            case LwLinkThroughputCount::CI_RX_COUNT_2:
            case LwLinkThroughputCount::CI_RX_COUNT_3:
                regs.Write32(linkId,
                             MODS_LWLTLC_RX_LNK_DEBUG_TP_CNTR_CTRL_0_ENABLE,
                             counterIdx,
                             bStart ? 1 : 0);
                break;
            default:
                Printf(Tee::PriError,
                       "LwLink Version 4 Throughput Counters : Unknown throughput counter %d "
                       "when starting counters\n", lwrCounter);
                return RC::SOFTWARE_ERROR;
        }
        lwrCounter = Utility::BitScanForward(counterMask, lwrCounter + 1);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkThroughputCountersV4::CaptureCounter
(
    LwLinkThroughputCount::CounterId counterId,
    UINT32 linkId
)
{
    RegHal &regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    const UINT32 counterIdx = LwLinkThroughputCount::CounterIdToIndex(counterId);
    switch (counterId)
    {
        case LwLinkThroughputCount::CI_TX_COUNT_0:
        case LwLinkThroughputCount::CI_TX_COUNT_1:
        case LwLinkThroughputCount::CI_TX_COUNT_2:
        case LwLinkThroughputCount::CI_TX_COUNT_3:
            regs.Write32(linkId, MODS_LWLTLC_TX_LNK_DEBUG_TP_CNTR_CTRL_0_CAPTURE, counterIdx, 1);
            break;
        case LwLinkThroughputCount::CI_RX_COUNT_0:
        case LwLinkThroughputCount::CI_RX_COUNT_1:
        case LwLinkThroughputCount::CI_RX_COUNT_2:
        case LwLinkThroughputCount::CI_RX_COUNT_3:
            regs.Write32(linkId, MODS_LWLTLC_RX_LNK_DEBUG_TP_CNTR_CTRL_0_CAPTURE, counterIdx, 1);
            break;
        default:
            Printf(Tee::PriError,
                   "LwLink Version 4 Throughput Counters : Unknown throughput counter %d "
                   "when capturing counters\n", counterId);
            return RC::SOFTWARE_ERROR;
    }
    return RC::OK;
}
