/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "dmavoltace.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"

#include "class/clc3b5.h" // VOLTA_DMA_COPY_A

// --------------------------------------------------------------------------
//  c'tor
// --------------------------------------------------------------------------
DmaReaderVoltaCE::DmaReaderVoltaCE(WfiType wfiType, LWGpuChannel* ch, UINT32 size)
    : DmaReaderGF100CE(wfiType, ch, size)
{
}

// --------------------------------------------------------------------------
//  d'tor
// --------------------------------------------------------------------------
DmaReaderVoltaCE::~DmaReaderVoltaCE()
{
}

//  read line from dma to sysmem
// --------------------------------------------------------------------------
RC DmaReaderVoltaCE::SendBasicKindMethod(const Surface2D* srcSurf, MdiagSurf* dstSurf)
{
    MASSERT(srcSurf);

    RC rc = OK;

    DebugPrintf("DmaReaderVoltaCE::SendBasicKindMethod start\n");

    UINT32 srcPhysMode = 0;
    Memory::Location srcLocation =  srcSurf->GetLocation();
    switch(srcLocation)
    {
        case Memory::Fb:
            srcPhysMode = DRF_DEF(C3B5, _SET_SRC_PHYS_MODE, _TARGET, _LOCAL_FB);
            break;

        case Memory::Coherent:
            srcPhysMode = DRF_DEF(C3B5, _SET_SRC_PHYS_MODE, _TARGET, _COHERENT_SYSMEM);
            break;

        case Memory::NonCoherent:
            srcPhysMode = DRF_DEF(C3B5, _SET_SRC_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM);
            break;

        default:
            MASSERT(!"Unsupported location!\n");
    }

    UINT32 dstPhysMode = 0;
    Memory::Location dstLocation =  dstSurf? dstSurf->GetLocation():m_DstLocation;
    switch(dstLocation)
    {
        case Memory::Fb:
            dstPhysMode = DRF_DEF(C3B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB);
            break;

        case Memory::Coherent:
            dstPhysMode = DRF_DEF(C3B5, _SET_DST_PHYS_MODE, _TARGET, _COHERENT_SYSMEM);
            break;

        case Memory::NonCoherent:
            dstPhysMode = DRF_DEF(C3B5, _SET_DST_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM);
            break;

        default:
            MASSERT(!"Unsupported location!\n");
    }

    LWGpuChannel* ch = m_Channel;
    ch->CtxSwEnable(false);

    UINT32 subChannel = 0;
    CHECK_RC(ch->GetSubChannelNumClass(m_DmaObjHandle, &subChannel, 0));
    CHECK_RC(ch->SetObjectRC(subChannel, m_DmaObjHandle));

    CHECK_RC(ch->MethodWriteRC(subChannel, LWC3B5_SET_SRC_PHYS_MODE, srcPhysMode));
    CHECK_RC(ch->MethodWriteRC(subChannel, LWC3B5_SET_DST_PHYS_MODE, dstPhysMode));

    DebugPrintf("DmaReaderVoltaCE::SendBasicKindMethod complete\n");

    return OK;
}
