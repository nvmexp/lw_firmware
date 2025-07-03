/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "nofb.h"
#include "core/include/rc.h"
#include "gpu/include/gpusbdev.h"
#define FBUNSUPPORTEDMESSAGE Printf(m_PrintPri, "%s: %s\n", __FUNCTION__, m_WarningMessage.c_str())

/*virtual*/ RC NoFrameBuffer :: GetFbRanges(FbRanges *pRanges) const
{
    FBUNSUPPORTEDMESSAGE;
    FbRange fbr;
    //TBD: verify memerror
    fbr.start = 0;
    fbr.size = 0;
    pRanges->push_back(fbr);
    return OK;
}

/*virtual*/ UINT64 NoFrameBuffer :: GetGraphicsRamAmount() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
}

/*virtual*/ bool NoFrameBuffer :: IsSOC() const
{
    return GpuSub()->IsSOC();
}

/*virtual*/ UINT32 NoFrameBuffer :: GetSubpartitions() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
}

/*virtual*/ UINT32 NoFrameBuffer :: GetBusWidth() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
};

/* virtual */ UINT32 NoFrameBuffer :: GetPseudoChannelsPerChannel() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
}

/* virtual */ bool NoFrameBuffer :: IsEccOn() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
}

/* virtual */ bool NoFrameBuffer :: IsRowRemappingOn() const
{
    FBUNSUPPORTEDMESSAGE;
    return false;
}

/*virtual*/ UINT32 NoFrameBuffer :: GetRankCount() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
};

/*virtual*/ UINT32 NoFrameBuffer :: GetBankCount() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
};

/*virtual*/ UINT32 NoFrameBuffer :: GetRowBits() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
};

/*virtual*/ UINT32 NoFrameBuffer :: GetRowSize() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
};

/*virtual*/ UINT32 NoFrameBuffer :: GetBurstSize() const
{
    FBUNSUPPORTEDMESSAGE;
    return 0;
}

/*virtual*/ RC NoFrameBuffer :: Initialize()
{
    return InitFbpInfo();
}

/*virtual*/ UINT32 NoFrameBuffer :: GetDataRate() const
{
    FBUNSUPPORTEDMESSAGE;
    return 1;
}

/*virtual*/ RC NoFrameBuffer :: DecodeAddress
(
    FbDecode *pDecode,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB
) const
{
    FBUNSUPPORTEDMESSAGE;
    memset(pDecode, 0, sizeof(*pDecode));
    return OK;
}

RC NoFrameBuffer::EccAddrToPhysAddr
(
    const EccAddrParams & eccAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    PHYSADDR *pPhysAddr
)
{
    FBUNSUPPORTEDMESSAGE;
    *pPhysAddr = 0;
    return OK;
}
