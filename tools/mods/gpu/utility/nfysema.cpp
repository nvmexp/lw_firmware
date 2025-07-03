/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2011,2013,2015-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/nfysema.h"
#include "class/cl85b5.h"
#include "class/cl90b5.h"

#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "lwos.h"
#include "gpu/tests/gputestc.h"
#include "surf2d.h"
#include "core/include/tasker.h"
#include "gpu/include/gpudev.h"

const UINT32 MaxNumSurfaces = 16;   //! Arbitrary, increase if we ever hit this.

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

NotifySemaphore::NotifySemaphore()
:
    m_NumSurfaces(0),
    m_Location(Memory::Optimal)
{
    m_SemaphoreSize = ONE_WORD;
    m_TriggerPayload.clear();
    m_pSurface2Ds.clear();
    m_pChannel.clear();
}

 //------------------------------------------------------------------------------

void NotifySemaphore::Free()
{
    if (m_pSurface2Ds.size() > 0)
    {
        for (UINT32 i = 0; i < m_NumSurfaces; i++)
            m_pSurface2Ds[i].Free();

        m_pSurface2Ds.clear();
        m_TriggerPayload.clear();
        m_pChannel.clear();
    }
    m_NumSurfaces = 0;
    m_SemaphoreSize = ONE_WORD;
}

//------------------------------------------------------------------------------

UINT64 NotifySemaphore::GetTimestamp(UINT32 surfNum)
{
    MASSERT(surfNum < m_NumSurfaces);
    if (m_SemaphoreSize == FOUR_WORDS)
    {

        return MEM_RD64((volatile UINT08 *)(m_pSurface2Ds[surfNum].GetAddress())+8);
    }
    return 0;
}

//------------------------------------------------------------------------------

void NotifySemaphore::Setup
(
    SemaphoreSize Size,
    Memory::Location Location,
    UINT32 NumSurfaces
)
{
    MASSERT(NumSurfaces <= MaxNumSurfaces);

    m_SemaphoreSize = Size;
    m_Location = Location;
    m_NumSurfaces = NumSurfaces;
    m_pSurface2Ds.resize(m_NumSurfaces);
    m_TriggerPayload.resize(m_NumSurfaces);
    m_pChannel.resize(m_NumSurfaces);
}

//------------------------------------------------------------------------------

RC NotifySemaphore::Allocate
(
    Channel * pChannel,
    UINT32 surfNum
)
{
    if ((pChannel->GetGpuDevice()->GetNumSubdevices() > 1) &&
        (m_Location == Memory::Fb))
    {
        // A Surface2D in FB on an SLI device actually exists at the same
        // offset in *each* subdevice's FB.
        // Since we have to map *one* of these to the CPU's virtual address
        // space, you must choose one by using the 3-argument Allocate.
        MASSERT(!"Unknown SLI FB mapping");
        return RC::SOFTWARE_ERROR;
    }
    return Allocate(pChannel, surfNum, 0);
}

RC NotifySemaphore::Allocate
(
    Channel * pChannel,
    UINT32 surfNum,
    UINT32 subdevInst
)
{
    RC rc;
    GpuDevice * pDev = pChannel->GetGpuDevice();

    MASSERT(surfNum < m_NumSurfaces);

    if (surfNum >= MaxNumSurfaces)
        return RC::SOFTWARE_ERROR;

    m_pChannel[surfNum] = pChannel;

    Memory::AddressModel model = Memory::Paging;

    m_pSurface2Ds[surfNum].SetAddressModel(model);
    m_pSurface2Ds[surfNum].SetLocation(m_Location);
    m_pSurface2Ds[surfNum].SetLayout(Surface2D::Pitch);
    m_pSurface2Ds[surfNum].SetPitch(sizeof(UINT32)*m_SemaphoreSize);
    m_pSurface2Ds[surfNum].SetColorFormat(ColorUtils::VOID32);
    m_pSurface2Ds[surfNum].SetWidth(sizeof(UINT32)*m_SemaphoreSize);
    m_pSurface2Ds[surfNum].SetHeight(1);
    m_pSurface2Ds[surfNum].SetDepth(1);
    m_pSurface2Ds[surfNum].SetPageSize(4);
    m_pSurface2Ds[surfNum].SetKernelMapping(true);
    m_pSurface2Ds[surfNum].SetVASpace(Surface2D::GPUVASpace);
    m_pSurface2Ds[surfNum].SetGpuVASpace(pChannel->GetVASpace());
    CHECK_RC(m_pSurface2Ds[surfNum].Alloc(pDev,
                                       m_pChannel[surfNum]->GetRmClient()));
    CHECK_RC(m_pSurface2Ds[surfNum].Map(subdevInst));
    CHECK_RC(m_pSurface2Ds[surfNum].BindGpuChannel(
                 m_pChannel[surfNum]->GetHandle(),
                 m_pChannel[surfNum]->GetRmClient()));

    Printf(Tee::PriDebug,
           "Notifier physical address is %llx\n",
           m_pSurface2Ds[surfNum].GetPhysAddress());

    return rc;
}

//------------------------------------------------------------------------------

LwRm::Handle NotifySemaphore::GetCtxDmaHandleGpu(UINT32 surfNum)
{
    MASSERT(surfNum < m_NumSurfaces);
    return m_pSurface2Ds[surfNum].GetCtxDmaHandleGpu();
}

//------------------------------------------------------------------------------

UINT64 NotifySemaphore::GetCtxDmaOffsetGpu(UINT32 surfNum)
{
    MASSERT(surfNum < m_NumSurfaces);
    return m_pSurface2Ds[surfNum].GetCtxDmaOffsetGpu();
}

//------------------------------------------------------------------------------

const Surface2D& NotifySemaphore::GetSurface(UINT32 surfNum) const
{
    MASSERT(surfNum < m_NumSurfaces);
    return m_pSurface2Ds[surfNum];
}

//------------------------------------------------------------------------------

struct Notifier_PollArgs
{
    Channel *                   pChannel[MaxNumSurfaces];
    volatile UINT08 *           Addresses[MaxNumSurfaces];
    UINT32                      NumSurfs;
    UINT32                      Payload[MaxNumSurfaces];
    NotifySemaphore::CondFunc   Cond;
};

bool NotifySemaphore::CondEquals
(
    UINT32 LwrrentSurfaceWord,
    UINT32 MostRecentTriggerPayload
)
{
    return LwrrentSurfaceWord == MostRecentTriggerPayload;
}

bool NotifySemaphore::CondGreaterEq
(
    UINT32 LwrrentSurfaceWord,
    UINT32 MostRecentTriggerPayload
)
{
    return LwrrentSurfaceWord >= MostRecentTriggerPayload;
}

bool NotifySemaphore::Poll( void * pArgs )
{
   Notifier_PollArgs Args = *(Notifier_PollArgs*)pArgs;

   // Return true if GPU has written its notifier memory.
   for (UINT32 i = 0; i < Args.NumSurfs; i++)
   {
       if (OK != Args.pChannel[i]->CheckError())
           return true;
       if (!Args.Cond(MEM_RD32(Args.Addresses[i]), Args.Payload[i]))
           return false;
   }
   return true;
}

//------------------------------------------------------------------------------
RC NotifySemaphore::IsCopyComplete(bool *pComplete)
{
    StickyRC rc;
    *pComplete = true;
    for (UINT32 i = 0; i < m_NumSurfaces; i++)
    {
        rc = m_pChannel[i]->CheckError();
        volatile UINT08 * pMem = (volatile UINT08 *) m_pSurface2Ds[i].GetAddress();
        MASSERT(pMem);
        if (!CondGreaterEq(MEM_RD32(pMem), m_TriggerPayload[i]))
        {
            *pComplete = false;
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC NotifySemaphore::Wait(FLOAT64 TimeoutMs, CondFunc Cond)
{
    Notifier_PollArgs Args;
    Args.NumSurfs = m_NumSurfaces;
    Args.Cond = Cond;

    bool InFb = false;

    for (UINT32 i = 0; i < m_NumSurfaces; i++)
    {
        volatile UINT08 * pMem = (volatile UINT08 *)
            m_pSurface2Ds[i].GetAddress();
        MASSERT(pMem);
        Args.Addresses[i] = pMem;
        Args.Payload[i] = m_TriggerPayload[i];
        Args.pChannel[i] = m_pChannel[i];
        if (m_pSurface2Ds[i].GetLocation() == Memory::Fb)
            InFb = true;
    }

    RC rc;
    if (InFb)
        rc = POLLWRAP_HW( &Poll, &Args, TimeoutMs );
    else
        rc = POLLWRAP( &Poll, &Args, TimeoutMs );

    if (RC::TIMEOUT_ERROR == rc)
    {
        rc = RC::NOTIFIER_TIMEOUT;

        Printf(Tee::PriLow, "Notifier::Wait timeout");
        for (UINT32 i = 0; i < m_NumSurfaces; i++)
        {
            UINT32 x = MEM_RD32(Args.Addresses[i]);
            if (x == m_TriggerPayload[i])
            {
                Printf(Tee::PriLow, " surfNum %d", i);
            }
        }
        Printf(Tee::PriLow, "\n");
    }

    return rc;
}

//------------------------------------------------------------------------------

RC NotifySemaphore::WaitOnOne (UINT32 surfNum, FLOAT64 TimeoutMs, CondFunc Cond)
{
    MASSERT(surfNum < m_NumSurfaces);
    Notifier_PollArgs Args;
    Args.pChannel[0] = m_pChannel[surfNum];
    Args.NumSurfs = 1;
    Args.Payload[0] = m_TriggerPayload[surfNum];
    Args.Cond = Cond;

    volatile UINT08 * pMem = (volatile UINT08 *) m_pSurface2Ds[surfNum].GetAddress();
    MASSERT(pMem);
    Args.Addresses[0] = pMem;

    RC rc;
    if (m_pSurface2Ds[surfNum].GetLocation() == Memory::Fb)
        rc = POLLWRAP_HW( &Poll, &Args, TimeoutMs );
    else
        rc = POLLWRAP( &Poll, &Args, TimeoutMs );

    if (RC::TIMEOUT_ERROR == rc)
    {
        rc = RC::NOTIFIER_TIMEOUT;

        Printf(Tee::PriLow, "Notifier::Wait timeout");

        UINT32 x = MEM_RD32(Args.Addresses[surfNum]);
        if (x == m_TriggerPayload[surfNum])
        {
            Printf(Tee::PriLow, " surfNum %d", surfNum);
        }
        Printf(Tee::PriLow, "\n");
    }
    else if (OK == rc)
    {
        rc = m_pChannel[surfNum]->CheckError();
    }

    return rc;
}

//------------------------------------------------------------------------------

void NotifySemaphore::SetOnePayload(UINT32 surfNum, UINT32 value)
{
    MASSERT(surfNum < m_NumSurfaces);
    char * pMem = (char *) m_pSurface2Ds[surfNum].GetAddress();
    MASSERT(pMem);

    MEM_WR32(pMem, value);
}

//------------------------------------------------------------------------------

void NotifySemaphore::SetPayload(UINT32 value)
{

    for (UINT32 surfNum = 0; surfNum < m_NumSurfaces; surfNum++)
    {
        char * pMem = (char *) m_pSurface2Ds[surfNum].GetAddress();
        MASSERT(pMem);

        MEM_WR32(pMem, value);
    }
}

//------------------------------------------------------------------------------

UINT32 NotifySemaphore::GetPayload(UINT32 surfNum)
{
    MASSERT(surfNum < m_NumSurfaces);
    char * pMem = (char *) m_pSurface2Ds[surfNum].GetAddress();
    MASSERT(pMem);

    return MEM_RD32(pMem);
}

//------------------------------------------------------------------------------

void NotifySemaphore::SetOneTriggerPayload(UINT32 surfNum, UINT32 value)
{
    MASSERT(surfNum < m_NumSurfaces);
    m_TriggerPayload[surfNum] = value;
}

//------------------------------------------------------------------------------

void NotifySemaphore::SetTriggerPayload(UINT32 value)
{
    for (UINT32 surfNum = 0; surfNum < m_NumSurfaces; surfNum++)
        m_TriggerPayload[surfNum] = value;
}

//------------------------------------------------------------------------------

UINT32 NotifySemaphore::GetTriggerPayload(UINT32 surfNum)
{
    MASSERT(surfNum < m_NumSurfaces);
    return m_TriggerPayload[surfNum];
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
