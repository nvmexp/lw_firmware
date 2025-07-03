/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2010,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  chansub.cpp
 * @brief Lwston push buffer to be used with GpFifoChannel::CallSubroutine
 */

#include "chansub.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwmisc.h"         // for REF_NUM
#include "core/include/massert.h"
#include "gpu/tests/gputestc.h"
#include "gpu/include/gpudev.h"
#include "lwos.h"
#include "core/include/pool.h"
#include "core/include/utility.h"
#include <cstdarg>

ChannelSubroutine::ChannelSubroutine(Channel *pCh, GpuTestConfiguration *pGTC)
{
    MASSERT(pCh);

    m_pCh  = pCh;
    m_pGTC = pGTC;
    m_Size = 0;

    m_BufSize = 32;
    m_Buffer.resize(m_BufSize);
    m_BufPut = 0;
}

ChannelSubroutine::~ChannelSubroutine()
{
}

UINT64 ChannelSubroutine::Offset() const
{
    MASSERT(m_Size > 0);  // FinishSubroutine must be called first
    return m_LwstomPb.GetCtxDmaOffsetGpu();
}

UINT32 ChannelSubroutine::Size() const
{
    MASSERT(m_Size > 0);  // FinishSubroutine must be called first
    return m_Size;
}

RC ChannelSubroutine::Call(Channel * pCh) const
{
    if (0 == m_Size)
    {
        // FinishSubroutine hasn't been called yet.
        return RC::SOFTWARE_ERROR;
    }
    return pCh->CallSubroutine(Offset(), Size());
}

RC ChannelSubroutine::FinishSubroutine()
{
    RC rc = OK;

    // Has FinishSubroutine been called before this call?
    if (m_Size != 0)
        return RC::SOFTWARE_ERROR;

    // Callwlate Size of Push Buffer
    m_Size = m_BufPut * sizeof(UINT32);

    // Allocate the surface of appropriate size and type
    m_LwstomPb.SetHeight(1);
    m_LwstomPb.SetWidth(m_BufPut);
    m_LwstomPb.SetColorFormat(ColorUtils::VOID32);
    m_LwstomPb.SetType(LWOS32_TYPE_IMAGE);
    if (m_pGTC)
        m_LwstomPb.SetLocation(m_pGTC->PushBufferLocation());
    else
        m_LwstomPb.SetLocation(Memory::NonCoherent);
    m_LwstomPb.SetProtect(Memory::ReadWrite);
    m_LwstomPb.SetTiled(false);
    m_LwstomPb.SetAddressModel(Memory::Paging);
    m_LwstomPb.SetLayout(Surface2D::Pitch);
    m_LwstomPb.SetDisplayable(false);

    CHECK_RC( m_LwstomPb.Alloc(m_pCh->GetGpuDevice()) );
    CHECK_RC( m_LwstomPb.Map((m_pCh->GetGpuDevice())->
                                GetDisplaySubdevice()->GetSubdeviceInst()) );
    CHECK_RC( m_LwstomPb.BindGpuChannel(m_pCh->GetHandle()) );

    // Fill Surface
    Platform::MemCopy(m_LwstomPb.GetAddress(), &m_Buffer[0], m_BufPut*sizeof(UINT32));

    // In case the surface is in write-combined FB or sysmem, flush WC buffer.
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    return rc;
}

RC ChannelSubroutine::FinishSubroutine(UINT64 *pOffset, UINT32 *pSize)
{
    RC rc = OK;

    MASSERT(pOffset && pSize);

    CHECK_RC( FinishSubroutine() );
    *pOffset = Offset();
    *pSize = Size();

    return rc;
}

RC ChannelSubroutine::Write(UINT32 Subchannel, UINT32 Method, UINT32 Data)
{
    RC rc = OK;

    // Has FinishSubroutine been called?
    if (m_Size != 0)
        return RC::SOFTWARE_ERROR;

    MakeRoomInBuffer(2);
    UINT32 *pBuffer = &m_Buffer[m_BufPut];
    CHECK_RC(m_pCh->WriteExternal(&pBuffer, Subchannel, Method, 1, &Data));
    m_BufPut = UNSIGNED_CAST(UINT32, pBuffer - &m_Buffer[0]);
    return rc;
}

RC ChannelSubroutine::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    UINT32 data,
    ...  //Rest of Data
)
{
    RC rc = OK;

    // Has FinishSubroutine been called?
    if (m_Size != 0)
        return RC::SOFTWARE_ERROR;

    vector<UINT32> dataVector(count);
    va_list dataVa;
    va_start(dataVa, data);
    dataVector[0] = data;
    for (UINT32 ii = 1; ii < count; ++ii)
        dataVector[ii] = va_arg(dataVa, UINT32);
    va_end(dataVa);

    MakeRoomInBuffer(count + 1); // There are (count) Data and 1 Method Header
    UINT32 *pBuffer = &m_Buffer[m_BufPut];
    CHECK_RC(m_pCh->WriteExternal(&pBuffer, subchannel,
                                  method, count, &dataVector[0]));
    m_BufPut = UNSIGNED_CAST(UINT32, pBuffer - &m_Buffer[0]);
    return rc;
}

RC ChannelSubroutine::Write
(
    UINT32        Subchannel,
    UINT32        Method,
    UINT32        Count,
    const UINT32 *pData
)
{
    RC rc = OK;

    // Has FinishSubroutine been called?
    if (m_Size != 0)
        return RC::SOFTWARE_ERROR;

    MakeRoomInBuffer(Count + 1);
    UINT32 *pBuffer = &m_Buffer[m_BufPut];
    CHECK_RC(m_pCh->WriteExternal(&pBuffer, Subchannel, Method, Count, pData));
    m_BufPut = UNSIGNED_CAST(UINT32, pBuffer - &m_Buffer[0]);
    return rc;
}

RC ChannelSubroutine::WriteRaw(const UINT32 *pData, UINT32 Count)
{
    RC rc = OK;

    MASSERT(Count != 0);

    // Has FinishSubroutine been called?
    if (m_Size != 0) return RC::SOFTWARE_ERROR;

    MakeRoomInBuffer(Count);

    UINT32 i;
    for (i = 0; i < Count; ++i)
    {
        m_Buffer[m_BufPut++] = pData[i];
    }

    return rc;
}

void ChannelSubroutine::MakeRoomInBuffer(UINT32 size)
{
    if (m_BufSize < (size + m_BufPut) )
    {
        while (m_BufSize < (size + m_BufPut))
        {
            m_BufSize *= 2;
        }

        m_Buffer.resize(m_BufSize);
    }
}
