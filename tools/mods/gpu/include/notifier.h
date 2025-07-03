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
 * @file    notifier.h
 * @brief   Declare class Notifier.
 */

#ifndef INCLUDED_NOTIFIER_H
#define INCLUDED_NOTIFIER_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_MEMTYPES_H
#include "core/include/memtypes.h"
#endif
#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#if defined(PAGESIZE)
#undef PAGESIZE
#endif

class Channel;
class GpuTestConfiguration;
class Surface2D;
class ModsEvent;
class GpuDevice;

/**
 * @class(Notifier)  Graphics notification memory handler.
 *
 * A notifier is just a block of 16 bytes of host memory that the
 * GPU (or resman, for a software method) will write to asynchronously.
 *
 * We use these to allow the GPU to tell the client app when a blit
 * has completed, or when an overlay scanout has completed, etc.
 *
 * In theory, the GPU could return any data that fits into 16 bytes,
 * but in practice the only data MODS is ever interested in is just the
 * fact that the last byte (byte 15) changes from 0xff to something else,
 * telling us that the GPU has finished whatever we asked it to do.
 *
 * Some classes have more than one thing they need to communicate, and
 * therefor need more than one 16-byte notifier.  In the class header
 * file, LWxxx_NOTIFIERS_MAXCOUNT will define this number.
 *
 * No actual memory or dma context will be allocated until the Allocate()
 * method is called.
 *
 * On lw25+ multichip boards, we allocate a separate dma context for each
 * Gpu so they won't overwrite each other's data.  This is handled here
 * inside the Notifier class.
 */
class Notifier
{
public:
   Notifier();
   ~Notifier();

   /** Allocate the host memory and dma context for the notifiers.
    *  This version explicitly specifies sysmem vs. FB mem for the notifier.
    */
   RC Allocate
   (
      Channel *             pChan,
      UINT32                BlockCount,
      Memory::DmaProtocol   Protocol,
      Memory::Location      Location,
      GpuDevice *           pGpuDev,
      Memory::Protect       Protect = Memory::ReadWrite
   );

   /** Allocate the host memory and dma context for the notifiers.
    *  This version passes a TestConfiguration object that will supply
    *  the desired location for the memory.
    */
   RC Allocate
   (
      Channel *             pChan,
      UINT32                BlockCount,
      const GpuTestConfiguration * pTstCfg
   );

   /** Allocate a mods and RM events and tie them to awaken interrupts from
    *  the specified object.  This allows using the WaitAwaken call later.
    *  Not supported on all platforms.
    *
    *  The notifier index must be the Index of the notifier to
    *  allocate the event on (i.e. one of the LWxxx_NOTIFIERS_xxx
    *  defines for the particular class).  The default value of 0
    *  maps to LWxxxx_NOTIFIERS_NOTIFY.
    */
   RC AllocEvent
   (
       UINT32 hObjectThatWillAwaken,
       UINT32 NotifierIndex = 0
   );

   /** Free the host memory and dma context for the notifiers, if any. */
   void Free();

    /** Send the LWxxx_SET_CONTEXT_DMA_NOTIFIES method to each GPU, with the
    *  appropriate DMA context handle(s).
    *  Pre-Fermi graphics classes seem to use 0x180 as the method number.
    */
    //! \brief Wrapper - determines how to send NOTIFY Address to subchannel
    //! and then calls more speific version of Instantiate.
    //!
    //! Use the private channel member to determine method(s) to write
    //! Most code calls this routine which calls one of two that follow
    RC Instantiate (UINT32 SubChannel);

    //! \brief Use 32-bit Context DMA version of notifier
    //!
    //!  pre-Fermi classes.
    RC Instantiate (UINT32 SubChannel, UINT32 Method);

    //! \brief Use 64-bit gpu address version of notifier
    //!
    //! Fermi classes.
    RC Instantiate
    (
        UINT32 SubChannel,
        UINT32 gpuAddrHighMethod,
        UINT32 gpuAddrLowMethod
    );

    //! \brief Write the Notifier to the pushbuffer
    //!
    //! Call the Write routine for the channel associated with
    //! notifier using the WRITE_NOTIFY method appropriate to the class
    //! of said channel and passing the InteruptMode to the write call.
    //! InterruptMode == 0 will be mapped to a WRITE_ONLY value.
    RC Write(UINT32 SubChannel, UINT32 InterruptMode = 0x0);

   /** Set the last byte of a block to 0xff, so we can tell when the GPU
    *  writes it.
    */
   void Clear(UINT32 WhichBlock);

   /** Artificially set some notifier flags, for use in cases where we want
    *  to wait for only a subset of the GPUs in an SLI device.
    */
   void Set(UINT32 WhichBlock, UINT32 SubdevMask);

   /** Wait for the notifier to be written by the Gpu(s).
    *  This version polls the notifier memory.
    */
   RC   Wait(UINT32 WhichBlock, FLOAT64 TimeoutMs);

   /** Wait for the object to issue an Awaken interrupt.
    *  This version puts the thread to sleep, no polling.
    */
   RC   WaitAwaken(FLOAT64 TimeoutMs);

   /** On single-chip boards, this is just like Wait.
    *  On multi-chip boards, wait for either GPU to notify, then delay
    *  until a couple more vblanks have oclwrred.
    *  This is a workaround until the resman allows setting different notify
    *  dma contexts for the different GPUs of a multichip board, for software
    *  objects.  (Hardware objects work correctly).
    */
   RC WaitWithMcSwWorkaround (UINT32 WhichBlock, FLOAT64 TimeoutMs);

   /** Return true if the notifier has been written by the GPU. */
   bool IsSet(UINT32 WhichBlock);

   /** Get the contents of the notifier block (16 bytes).
    *  On multi-chip, if Subdevice is the broadcast device, the
    *  notifier memory for the first GPU will be read.
    */
   void ReadBlock (UINT32 WhichBlock, UINT32 Subdevice, UINT08 * pBuffer);

   /** Return the dma-context handle for the given subdevice (0-based) */
   UINT32 GetHandle (UINT32 subdeviceIndex = 0xffffffff) const;

   /** Icky replacement for the old Graphics::ClearNotifier function.
    *  Sets byte 15 of the memory pointed to to the "unset" state.
    *  For code that doesn't use a proper Notifier object.
    */
   static void ClearExternal (void * Address);

   /** Icky replacement for the old Graphics::WaitNotifier function.
    *  Waits for the notifier to be set.
    */
   static RC WaitExternal (Channel * pCh, void * Address, FLOAT64 TimeoutMs);

   /** Icky function to check whether a notifier is set.
    *  For code that doesn't use a proper Notifier object.
    */
   static bool IsSetExternal (void * Address);

   //! \brief Allow a nonclass function to get this via a Notifier parameter
   UINT64 GetCtxDmaOffsetGpu(UINT32 gpuNum);

   //! \brief Allow a nonclass function to get this via a Notifier parameter
   UINT32 GetNumGpus();

   //! \brief Allow a nonclass function to get this via a Notifier parameter
   Channel * GetChannel();

private:
   enum
   {
       BLOCKSIZE = 16            // bytes per notifier block
      ,PAGESIZE  = 4096          // bytes per resman memory page
   };

   // Notifier class is multi client compliant and should never use the
   // LwRmPtr constructor without specifying which client
   DISALLOW_LWRMPTR_IN_CLASS(Notifier);

   Channel * m_pChannel;         //!< Channel object
   UINT32    m_BlockCount;       //!< num 16-byte blocks
   UINT32    m_BytesPerGpu;      //!< blocks*16, rounded up to pagesize
   UINT32    m_NumGpus;          //!< Gpu::NumSubdevices() at Allocate time.
   Surface2D *  m_pSurface2Ds;    //!< Surfaces to hold notifiers.
   ModsEvent*   m_pModsEvent;
   UINT32       m_hRmEvent;

   static bool Poll(void*);      //!< Used by Wait.
   static bool PollWithMcSwWorkaround(void*);
                                 //!< Used by WaitWithMcSwWorkaround.

    //! \brief Return the Method needed to write notifier flags for the
    //! channel associated with the notifier
    static UINT32 GetNotifyWriteMethod(Channel * pChannel);

}; // class Notifier

#endif // INCLUDED_NOTIFIER_H

