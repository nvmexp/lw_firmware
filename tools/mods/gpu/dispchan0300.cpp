/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Channel interface.

#include "lwtypes.h"
#include "core/include/channel.h"
#include "gpu/include/dispchan.h"
#include "core/include/massert.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/utility.h"

#include "gpu/include/gpudev.h"

// LwDisplay specific stuff
#include "disp/v03_00/dev_disp.h"
#include "class/clc37b.h" // LWC37B_WINDOW_IMM_CHANNEL_DMA
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "class/clc37e.h" // LWC37E_WINDOW_CHANNEL_DMA

DmaDisplayChannelC3::DmaDisplayChannelC3
(
    void *       ControlRegs[LW2080_MAX_SUBDEVICES],
    void *       pPushBufferBase,
    UINT32       PushBufferSize,
    void *       pErrorNotifierMemory,
    LwRm::Handle hChannel,
    UINT32       Offset,
    GpuDevice *  pGpuDev
) :
    DmaDisplayChannel(ControlRegs, pPushBufferBase, PushBufferSize, pErrorNotifierMemory,
                      hChannel, Offset, pGpuDev)
{

}

DmaDisplayChannelC3::~DmaDisplayChannelC3()
{

}

UINT32 DmaDisplayChannelC3::ScanlineRead(UINT32 head)
{
    //This is not implemented in the hw yet:
    //return MEM_RD32(&((LWC37DDispControlDma*)m_ControlRegs[0])->GetRgScanLine[head]);

    RegHal &regs = m_pGpuDevice->GetSubdevice(0)->Regs();

    UINT32 scanLineValue = DRF_NUM(C37D, _GET_RG_SCAN_LINE, _LINE,
        regs.Read32(MODS_PDISP_RG_DPCA_LINE_CNT, head));

    if (regs.Test32(MODS_PDISP_RG_STATUS_VBLNK_ACTIVE, head))
    {
        FLD_SET_DRF(C37D, _GET_RG_SCAN_LINE, _VBLANK, _TRUE, scanLineValue);
    }
    else
    {
        FLD_SET_DRF(C37D, _GET_RG_SCAN_LINE, _VBLANK, _FALSE, scanLineValue);
    }

    return scanLineValue;
}

/* virtual */void DmaDisplayChannelC3::SetPut(DispChannelType ch, UINT32 newPut)
{
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_NumSubdevices; SubdeviceIdx++)
    {
        switch (ch)
        {
            case DispChannelType::CORE:
                MEM_WR32(&(static_cast<LWC37DDispControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Put, newPut);
                break;
            case DispChannelType::WINDOW:
                MEM_WR32(&(static_cast<LWC37EDispControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Put, newPut);
                break;
            case DispChannelType::WINDOW_IMM:
                MEM_WR32(&(static_cast<LWC37BDispControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Put, newPut);
                break;
            default:
                MASSERT(!"Channel unknown");
                break;
        }
    }
}

/* virtual */UINT32 DmaDisplayChannelC3::GetPut(DispChannelType ch)
{
    UINT32 put;
    switch (ch)
    {
        case DispChannelType::CORE:
            put = MEM_RD32(&(static_cast<LWC37DDispControlDma *>(m_ControlRegs[0]))->Put);
            break;
        case DispChannelType::WINDOW:
            put = MEM_RD32(&(static_cast<LWC37EDispControlDma *>(m_ControlRegs[0]))->Put);
            break;
        case DispChannelType::WINDOW_IMM:
            put = MEM_RD32(&(static_cast<LWC37BDispControlDma *>(m_ControlRegs[0]))->Put);
            break;
        default:
            MASSERT(!"Channel unknown");
            put = 0;
            break;
    }
    return put;
}

/* virtual */void DmaDisplayChannelC3::SetGet(DispChannelType ch, UINT32 newGet)
{
    for (UINT32 SubdeviceIdx = 0; SubdeviceIdx < m_NumSubdevices; SubdeviceIdx++)
    {
        switch (ch)
        {
            case DispChannelType::CORE:
                MEM_WR32(&(static_cast<LWC37DDispControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Get, newGet);
                break;
            case DispChannelType::WINDOW:
                MEM_WR32(&(static_cast<LWC37EDispControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Get, newGet);
                break;
            case DispChannelType::WINDOW_IMM:
                MEM_WR32(&(static_cast<LWC37BDispControlDma *>(
                    m_ControlRegs[SubdeviceIdx]))->Get, newGet);
                break;
            default:
                MASSERT(!"Channel unknown");
                break;
        }
    }
}

/* virtual */UINT32 DmaDisplayChannelC3::GetGet(DispChannelType ch)
{
    UINT32 get;
    switch (ch)
    {
        case DispChannelType::CORE:
            get = MEM_RD32(&(static_cast<LWC37DDispControlDma *>(m_ControlRegs[0]))->Get);
            break;
        case DispChannelType::WINDOW:
            get = MEM_RD32(&(static_cast<LWC37EDispControlDma *>(m_ControlRegs[0]))->Get);
            break;
        case DispChannelType::WINDOW_IMM:
            get = MEM_RD32(&(static_cast<LWC37BDispControlDma *>(m_ControlRegs[0]))->Get);
            break;
        default:
            MASSERT(!"Channel unknown");
            get = 0;
            break;
    }
    return get;
}

UINT32 DmaDisplayChannelC3::GetPut(string ChannelName)
{
    return DmaDisplayChannel::GetPut(ChannelName);
}

void DmaDisplayChannelC3::SetPut(string ChannelName, UINT32 newPut)
{
    return DmaDisplayChannel::SetPut(ChannelName, newPut);
}

UINT32 DmaDisplayChannelC3::GetGet(string ChannelName)
{
    return DmaDisplayChannel::GetGet(ChannelName);
}

void DmaDisplayChannelC3::SetGet(string ChannelName, UINT32 newGet)
{
    return DmaDisplayChannel::SetGet(ChannelName, newGet);
}

/* virtual */ UINT32 DmaDisplayChannelC3::ComposeMethod
(
    UINT32 OpCode,
    UINT32 Count,
    UINT32 Offest
 )
{
    return DRF_NUM(_UDISP, _DMA, _OPCODE, OpCode) |
           DRF_NUM(_UDISP, _DMA, _METHOD_COUNT,  Count) |
           DRF_NUM(_UDISP, _DMA, _METHOD_OFFSET,  Offest >> 2);
}

DebugPortDisplayChannelC3::DebugPortDisplayChannelC3
(
    UINT32       channelNum,
    GpuDevice *  pGpuDev
) :
    DisplayChannel(nullptr, pGpuDev),
    m_ChannelNum (channelNum)
{

}

DebugPortDisplayChannelC3::~DebugPortDisplayChannelC3()
{

}

RC DebugPortDisplayChannelC3::Write
(
    UINT32 Method,
    UINT32 Data
)
{
    RC rc;
    UINT32 dbgCtrl = 0;
    GpuSubdevice *pSubdev = m_pGpuDevice->GetDisplaySubdevice();

    pSubdev->RegWr32(LW_PDISP_FE_DEBUG_DAT(m_ChannelNum), Data);

    dbgCtrl = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL, _MODE, _ENABLE, dbgCtrl);
    dbgCtrl = FLD_SET_DRF_NUM(_PDISP, _FE_DEBUG_CTL, _METHOD_OFS, Method>>2, dbgCtrl);
    dbgCtrl = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL,  _CTXDMA, _NORMAL,  dbgCtrl);
    dbgCtrl = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL, _NEW_METHOD, _TRIGGER,  dbgCtrl);
    pSubdev->RegWr32(LW_PDISP_FE_DEBUG_CTL(m_ChannelNum), dbgCtrl);

    while (TRUE)
    {
        if (FLD_TEST_DRF(_PDISP, _FE_DEBUG_CTL, _NEW_METHOD, _DONE,
            pSubdev->RegRd32(LW_PDISP_FE_DEBUG_CTL(m_ChannelNum))))
        {
            break;
        }
    }

    return rc;
}

UINT32 DebugPortDisplayChannelC3::GetChannelNum() const
{
    return m_ChannelNum;
}

RC DebugPortDisplayChannelC3::SetAutoFlush
(
    bool AutoFlushEnable,
    UINT32 AutoFlushThreshold
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DebugPortDisplayChannelC3::GetAutoFlush
(
    bool *pAutoFlushEnable,
    UINT32 *pAutoFlushThreshold
) const
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DebugPortDisplayChannelC3::Write
(
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  DataPtr
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DebugPortDisplayChannelC3::WriteNonInc
(
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  DataPtr
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DebugPortDisplayChannelC3::WriteNop()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DebugPortDisplayChannelC3::WriteJump
(
    UINT32 Offset
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DebugPortDisplayChannelC3::WriteSetSubdevice
(
    UINT32 Subdevice
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DebugPortDisplayChannelC3::WriteOpcode
(
    UINT32          Opcode,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  DataPtr
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC DebugPortDisplayChannelC3::Flush()
{
    // Since Flush is not needed in debug port, just return OK.
    return OK;
}

RC DebugPortDisplayChannelC3::WaitForDmaPush
(
    FLOAT64 TimeoutMs
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
