/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file    notifier.cpp
 * @brief   Implement class Notifier.
 */

#include "gpu/include/notifier.h"
#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "lwos.h"
#include "gpu/tests/gputestc.h"
#include "surf2d.h"
#include "core/include/tasker.h"
#include "gpu/include/gpudev.h"

// Added .h files to get channel class definitions
#include "class/cl006b.h"    // LW03_CHANNEL_DMA
#include "class/cl006c.h"    // LW04_CHANNEL_DMA
#include "class/cl006e.h"    // LW10_CHANNEL_DMA
#include "class/cl206e.h"    // LW20_CHANNEL_DMA
#include "class/cl366e.h"    // LW36_CHANNEL_DMA
#include "class/cl406e.h"    // LW40_CHANNEL_DMA
#include "class/cl446e.h"    // LW44_CHANNEL_DMA
#include "class/cl506f.h"    // LW50_CHANNEL_GPFIFO
#include "class/cl826f.h"    // G82_CHANNEL_GPFIFO
#include "class/cl902d.h"    // FERMI_TWOD_A

//------------------------------------------------------------------------------
Notifier::Notifier()
:
   m_pChannel(0),
   m_BlockCount(0),
   m_BytesPerGpu(0),
   m_NumGpus(0),
   m_pSurface2Ds(NULL),
   m_pModsEvent(NULL),
   m_hRmEvent(0)
{
}

//------------------------------------------------------------------------------
Notifier::~Notifier()
{
   if (m_pSurface2Ds)
      Free();
}

//------------------------------------------------------------------------------
RC Notifier::Allocate
(
    Channel * pChan,
    UINT32 BlockCount,
    const GpuTestConfiguration * pTstCfg
)
{
   return Allocate(pChan,
                   BlockCount,
                   pTstCfg->DmaProtocol(),
                   pTstCfg->NotifierLocation(),
                   pTstCfg->GetGpuDevice());
}

//------------------------------------------------------------------------------
RC Notifier::Allocate
(
    Channel * pChan,
    UINT32 BlockCount,
    Memory::DmaProtocol Protocol,
    Memory::Location Location,
    GpuDevice *pGpuDev,
    Memory::Protect Protect
)
{
   MASSERT(pGpuDev);

   if (m_pSurface2Ds)
   {
      MASSERT(pChan == m_pChannel);
      return OK;
   }
   m_pChannel = pChan;

   RC rc;

   m_BlockCount      = BlockCount;
   m_NumGpus         = pGpuDev->GetNumSubdevices();

   // Callwlate memory needed per-GPU
   m_BytesPerGpu = m_BlockCount * BLOCKSIZE;

   // Force cached host memory for SLI.
   if (m_NumGpus > 1 && Location == Memory::Fb)
      Location = Memory::Coherent;

   // Allocate a memory buffer per GPU in the device.
   Printf(Tee::PriDebug, "Notifier::Allocate in %s.\n",
        Location == Memory::Fb ? "fb" :
        Location == Memory::Coherent ? "coh" : "nco");

   m_pSurface2Ds = new Surface2D [m_NumGpus];

   UINT32 i;
   for (i = 0; i < m_NumGpus; i++)
   {
      m_pSurface2Ds[i].SetLocation(Location);
      m_pSurface2Ds[i].SetDmaProtocol(Protocol);
      m_pSurface2Ds[i].SetLayout(Surface2D::Pitch);
      m_pSurface2Ds[i].SetPitch(m_BytesPerGpu);
      m_pSurface2Ds[i].SetColorFormat(ColorUtils::I8);
      m_pSurface2Ds[i].SetWidth(m_BytesPerGpu);
      m_pSurface2Ds[i].SetHeight(1);
      m_pSurface2Ds[i].SetKernelMapping(true);
      m_pSurface2Ds[i].SetProtect(Protect);
      CHECK_RC(m_pSurface2Ds[i].Alloc(pGpuDev, pChan->GetRmClient()));
      CHECK_RC(m_pSurface2Ds[i].Map(i));
      CHECK_RC(m_pSurface2Ds[i].BindGpuChannel(pChan->GetHandle(),
                                               pChan->GetRmClient()));

      Printf(Tee::PriDebug, "Notifier physical address is %llx\n",
             m_pSurface2Ds[i].GetPhysAddress());
   }

   return rc;
}

//------------------------------------------------------------------------------
RC Notifier::AllocEvent (UINT32 hObjectThatWillAwaken, UINT32 NotifierIndex)
{
    if (m_pModsEvent)
        return OK;      // already called AllocEvent

    if (!m_pSurface2Ds)
        return RC::SOFTWARE_ERROR; // must Alloc first

    LwRm* pLwRm = m_pChannel->GetRmClient();

    m_pModsEvent = Tasker::AllocEvent("awaken notifier", true/*ManualReset*/);
    MASSERT(m_NumGpus == 1);
    GpuDevice* const pGpuDev = m_pSurface2Ds[0].GetGpuDev();
    void* const pOsEvent = Tasker::GetOsEvent(
            m_pModsEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(pGpuDev));

    if (! pOsEvent)
    {
        // This platform hasn't got any way to hook a GPU awaken interrupt to
        // a mods event, too bad.
        Tasker::FreeEvent(m_pModsEvent);
        m_pModsEvent = 0;
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc = pLwRm->AllocEvent(
            hObjectThatWillAwaken,
            &m_hRmEvent,
            LW01_EVENT_OS_EVENT,
            NotifierIndex,
            pOsEvent);
    return rc;
}

//------------------------------------------------------------------------------
void Notifier::Free()
{
   if (m_pSurface2Ds)
   {
      UINT32 i;
      for (i = 0; i < m_NumGpus; i++)
         m_pSurface2Ds[i].Free();

      delete [] m_pSurface2Ds;
      m_pSurface2Ds = NULL;
   }
   if (m_hRmEvent)
   {
      m_pChannel->GetRmClient()->Free(m_hRmEvent);
      m_hRmEvent = 0;
   }
   if (m_pModsEvent)
   {
      Tasker::FreeEvent(m_pModsEvent);
      m_pModsEvent = NULL;
   }

   m_BlockCount    = 0;
   m_BytesPerGpu   = 0;
   m_NumGpus       = 0;
   m_pChannel      = 0;
}

//------------------------------------------------------------------------------
//! \brief Write 64-bit address value to pushbuffer
//!
//! We always have a fermi or newer channel class, any hw object will
//!   support the new "64-bit paging model semaphore" style interface.
//! The 64-bit semaphore interface is standardized for Fermi classes.
//!   So, we can use the CL902D_SET_NOTIFY_A, _B, etc. for any class.
RC Notifier::Instantiate (UINT32 SubChannel)
{
    RC rc;
    // Fermi+, need to send 64-bit GPU Address.
    // LW902D_SET_... get typical methods needed for Fermi
    rc = Instantiate(SubChannel,
                         LW902D_SET_NOTIFY_A,
                         LW902D_SET_NOTIFY_B);
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Write 32-bit Context DMA value to pushbuffer using supplied Method
RC Notifier::Instantiate
(
   UINT32 SubChannel,
   UINT32 Method
)
{
    RC rc;
    MASSERT(m_pChannel);

    UINT32 i;
    if (m_NumGpus == 1)
    {
        CHECK_RC(m_pChannel->Write(SubChannel, Method, m_pSurface2Ds[0].GetCtxDmaHandleGpu()));
    }
    else
    {
        for (i = 0; i < m_NumGpus; i++)
        {
            CHECK_RC(m_pChannel->WriteSetSubdevice(1<<i));
            CHECK_RC(m_pChannel->Write(SubChannel, Method, m_pSurface2Ds[i].GetCtxDmaHandleGpu()));
        }

        CHECK_RC(m_pChannel->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }

    CHECK_RC(m_pChannel->Flush());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Write 64-bit Gpu Address to pushbuffer using High and Low methods
RC Notifier::Instantiate
(
    UINT32 SubChannel,
    UINT32 gpuAddrHighMethod,
    UINT32 gpuAddrLowMethod
)
{
    RC rc;
    MASSERT(m_pChannel);

    UINT64 gpuAddress;
    UINT32 gpuAddressHigh;
    UINT32 gpuAddressLow;

    UINT32 i;
    for (i = 0; i < m_NumGpus; i++)
    {
        if (m_NumGpus != 1)
        {
            CHECK_RC(m_pChannel->WriteSetSubdevice(1<<i));
        }

        gpuAddress = m_pSurface2Ds[i].GetCtxDmaOffsetGpu();
        gpuAddressHigh = (UINT32) (gpuAddress >> 32);
        gpuAddressLow = (UINT32) gpuAddress;

        CHECK_RC(m_pChannel->Write(SubChannel,
                                   gpuAddrHighMethod,
                                   gpuAddressHigh));
        CHECK_RC(m_pChannel->Write(SubChannel,
                                   gpuAddrLowMethod,
                                   gpuAddressLow));
    }

    if (m_NumGpus != 1)
    {
        CHECK_RC(m_pChannel->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }

    CHECK_RC(m_pChannel->Flush());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Provide Method number needed for Notify. Changed for Fermi
//!
//! Fermi Class 90XX( xx== 2d,39,97 use method: (0x0000010c) LW902D_NOTIFY for example.
UINT32 Notifier::GetNotifyWriteMethod(Channel * pChannel)
{
    return LW902D_NOTIFY;
}

//------------------------------------------------------------------------------
//! \brief Send InteruptMode to Pushbuffer using method appropriate for channel
//!
//! Default value == 0 matches the actual value of WRITE_ONLY for all classes
//! such as: NO02D_NOTIFY_WRITE_ONLY
RC Notifier::Write(UINT32 SubChannel, UINT32 InterruptMode /* default == 0 */)
{
    RC rc;

    rc = m_pChannel->Write(SubChannel,
                        GetNotifyWriteMethod(m_pChannel),
                        InterruptMode);
    return rc;
}

//------------------------------------------------------------------------------
void Notifier::Clear(UINT32 WhichBlock)
{
   MASSERT(WhichBlock < m_BlockCount);
   UINT32 offset = WhichBlock * BLOCKSIZE + 15;
   UINT32 gpu;
   for (gpu = 0; gpu < m_NumGpus; gpu++)
   {
      char * pMem = (char *) m_pSurface2Ds[gpu].GetAddress();
      MASSERT(pMem);

      MEM_WR08(pMem + offset, 0xff);
   }
   if (m_pModsEvent)
      Tasker::ResetEvent(m_pModsEvent);
}

//------------------------------------------------------------------------------
void Notifier::Set(UINT32 WhichBlock, UINT32 SubdevMask)
{
   MASSERT(WhichBlock < m_BlockCount);
   UINT32 offset = WhichBlock * BLOCKSIZE + 15;
   UINT32 gpu;
   for (gpu = 0; gpu < m_NumGpus; gpu++)
   {
      if (SubdevMask & (1<<gpu))
      {
          char * pMem = (char *) m_pSurface2Ds[gpu].GetAddress();
          MASSERT(pMem);

          MEM_WR08(pMem + offset, 0x0);
      }
   }
   MASSERT(!m_pModsEvent); // This interface not working for SLI mode.
}

//------------------------------------------------------------------------------
struct Notifier_PollArgs
{
   Channel *          pChannel;
   volatile UINT08 *  Addresses[Gpu::MaxNumSubdevices];
   UINT32             NumGpus;
};

bool Notifier::Poll( void * pArgs )
{
   Notifier_PollArgs Args = *(Notifier_PollArgs*)pArgs;

   if (OK != Args.pChannel->CheckError())
      return true;

   // Return true if all GPUs have written their notifier memory.
   UINT32 i;
   for (i = 0; i < Args.NumGpus; i++)
   {
      if (0xff == MEM_RD08(Args.Addresses[i]))
         return false;
   }

   return true;
}

bool Notifier::PollWithMcSwWorkaround( void * pArgs )
{
   Notifier_PollArgs Args = *(Notifier_PollArgs*)pArgs;

   if (OK != Args.pChannel->CheckError())
      return true;

   // Return true if either GPU has notified.

   // This is for SW classes that do not keep separate state per-GPU.
   // For example class 0x357C lw35 vid-lut-dac-cursor writes the notifier
   // only once per-device, where a HW class would write per gpu.

   UINT32 i;
   for (i = 0; i < Args.NumGpus; i++)
   {
      if (0xff != MEM_RD08(Args.Addresses[i]))
         return true;
   }
   return false;
}

//------------------------------------------------------------------------------
RC Notifier::Wait (UINT32 WhichBlock, FLOAT64 TimeoutMs)
{
    MASSERT(WhichBlock < m_BlockCount);

    Notifier_PollArgs Args;
    Args.pChannel = m_pChannel;
    Args.NumGpus = m_NumGpus;

    bool InFb = false;

    UINT32 i;
    for (i = 0; i < m_NumGpus; i++)
    {
        volatile UINT08 * pMem = (volatile UINT08 *) m_pSurface2Ds[i].GetAddress();
        MASSERT(pMem);
        Args.Addresses[i] = pMem + WhichBlock * BLOCKSIZE + 15;

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
        for (i = 0; i < m_NumGpus; i++)
        {
            UINT08 x = MEM_RD08(Args.Addresses[i]);
            if (x == 0xff)
            {
                Printf(Tee::PriLow, " subdev %d", i + Gpu::MasterSubdevice);
                if (i)
                    rc = RC::NOTIFIER_TIMEOUT_SD2;
            }
        }
        Printf(Tee::PriLow, "\n");
    }
    else if (OK == rc)
    {
        rc = m_pChannel->CheckError();
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Notifier::WaitExternal(Channel * pCh, void * Address, FLOAT64 TimeoutMs)
{
   MASSERT(pCh);
   MASSERT(Address);

   Notifier_PollArgs Args;

   Args.pChannel = pCh;
   Args.Addresses[0] = ((UINT08*)Address) + 15;
   Args.NumGpus = 1;

   RC rc = POLLWRAP_HW( &Poll, &Args, TimeoutMs );

   if (RC::TIMEOUT_ERROR == rc)
      rc = RC::NOTIFIER_TIMEOUT;
   else if (OK == rc)
      rc = pCh->CheckError();
   return rc;
}

//------------------------------------------------------------------------------
/* static */ bool Notifier::IsSetExternal (void * Address)
{
    MASSERT(Address);
    UINT08 Value = MEM_RD08((char*)Address + 15);
    return (Value != 0xff);
}

//------------------------------------------------------------------------------
RC Notifier::WaitWithMcSwWorkaround (UINT32 WhichBlock, FLOAT64 TimeoutMs)
{
    MASSERT(WhichBlock < m_BlockCount);

    Notifier_PollArgs Args;
    Args.pChannel = m_pChannel;
    Args.NumGpus = m_NumGpus;

    bool InFb = false;

    UINT32 i;
    for (i = 0; i < m_NumGpus; i++)
    {
        volatile UINT08 * pMem = (volatile UINT08 *) m_pSurface2Ds[i].GetAddress();
        MASSERT(pMem);
        Args.Addresses[i] = pMem + WhichBlock * BLOCKSIZE + 15;
        if (m_pSurface2Ds[i].GetLocation() == Memory::Fb)
            InFb = true;
    }

    // Wait for either notifier to get written.
    // Due to the resman not tracking SW object state separately for the two
    // GPUs, both GPUs will write the same notifier.
    RC rc;

    if (InFb)
        rc = POLLWRAP_HW( &PollWithMcSwWorkaround, &Args, TimeoutMs);
    else
        rc = POLLWRAP( &PollWithMcSwWorkaround, &Args, TimeoutMs);

    if (OK == rc)
        rc = m_pChannel->CheckError();

    if (RC::TIMEOUT_ERROR == rc)
    {
        return RC::NOTIFIER_TIMEOUT;
    }
    else
    {
        // Sleep long enough for about 2 vblanks to occur.
        // That ought to be long enough for the other GPU to catch up to whichever
        // one was first to write *the* notifier.
        Tasker::Sleep(2 * 1000 / 60.0);
        return rc;
    }
}

//------------------------------------------------------------------------------
RC Notifier::WaitAwaken(FLOAT64 TimeoutMs)
{
    // Unfortunately, there's no way to wait per-gpu in an SLI device.
    // The GPU interrupt-to-ModsEvent mapping is per-device, not per-subdevice.
    // So it isn't possible to have a separate event for each GPU.

    MASSERT(m_pModsEvent);

    return Tasker::WaitOnEvent(m_pModsEvent, Tasker::FixTimeout(TimeoutMs));
}

//------------------------------------------------------------------------------
bool Notifier::IsSet (UINT32 WhichBlock)
{
   MASSERT(WhichBlock < m_BlockCount);

    UINT32 gpu;
    for (gpu = 0; gpu < m_NumGpus; gpu++)
    {
        volatile UINT08 * pMem = (volatile UINT08 *) m_pSurface2Ds[gpu].GetAddress();
        MASSERT(pMem);
        if (MEM_RD08(pMem + WhichBlock * BLOCKSIZE + 15) == 0xff)
            return false;
    }

   // All Gpus have written their messages.
   return true;
}

//------------------------------------------------------------------------------
UINT32 Notifier::GetHandle(UINT32 subdeviceIndex /* = 0xffffffff */) const
{
    if (subdeviceIndex > m_NumGpus)
    {
        if (m_NumGpus > 1)
        {
            Printf(Tee::PriHigh, "Notifier::GetHandle() called w/o subdev arg in SLI mode!\n");
            return 0;
        }
        subdeviceIndex = 0;
    }

    return m_pSurface2Ds[subdeviceIndex].GetCtxDmaHandleGpu();
}

//------------------------------------------------------------------------------
void Notifier::ReadBlock (UINT32 WhichBlock, UINT32 Subdevice, UINT08 * pBuffer)
{
   MASSERT(WhichBlock < m_BlockCount);
   MASSERT(pBuffer);

   if (Subdevice)
      Subdevice--;

   if (Subdevice > m_NumGpus)
      Subdevice = 0;

   volatile UINT08 * pMem = (volatile UINT08 *) m_pSurface2Ds[Subdevice].GetAddress();
   MASSERT(pMem);
   volatile UINT08 * addr = pMem + WhichBlock * BLOCKSIZE;
   for (int i = 0; i < BLOCKSIZE; i++)
   {
      pBuffer[i] = MEM_RD08(addr + i);
   }
}

void Notifier::ClearExternal(void * Address)
{
   MEM_WR08((char*)Address + 15, 0xff);
}

//-----------------------------------------------------------------------------
//! \brief Allow a nonclass function to get this via a Notifier parameter
UINT64 Notifier::GetCtxDmaOffsetGpu(UINT32 gpuNum)
{
    // Make sure that Allocate has already been done.
    MASSERT(m_NumGpus != 0);

    return m_pSurface2Ds[gpuNum].GetCtxDmaOffsetGpu();
}

//-----------------------------------------------------------------------------
//! \brief Allow a nonclass function to get this via a Notifier parameter
UINT32 Notifier::GetNumGpus()
{
    // Make sure that Allocate has already been done.
    MASSERT(m_NumGpus != 0);

    return m_NumGpus;
}

//-----------------------------------------------------------------------------
//! \brief Allow a nonclass function to get this via a Notifier parameter
Channel * Notifier::GetChannel()
{
    // Make sure that Allocate has already been done.
    MASSERT(m_pChannel != 0);

    return m_pChannel;
}
