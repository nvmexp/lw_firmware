/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009,2011-2019, 2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SEMAWRAP_H
#define INCLUDED_SEMAWRAP_H

#ifndef INCLUDED_CHANWRAP_H
#include "core/include/chanwrap.h"
#endif

#ifndef INCLUDED_GRALLOC_H
#include "gpu/include/gralloc.h"
#endif

#ifndef INCLUDED_STL_STACK
#include <stack>
#define INCLUDED_STL_STACK
#endif

//--------------------------------------------------------------------
//! \brief Class that enables insertion of semaphore commands into the
//! pushbuffer.
//!
//! This class has several responsibilities:
//! - It parses the methods prior to writing them, separating out
//!   semaphore methods and tracking the current semaphore state of the
//!   channel.
//! - It implements Push/Pop capabilities for the semaphore state
//! - It implements releasing back-end semaphores.
//! - It tracks which class/engine the channel is lwrrently controlling,
//!   in order to support back-end semaphores.
//!
class SemaphoreChannelWrapper : public ChannelWrapper
{
public:
    SemaphoreChannelWrapper(Channel *pChannel);
    static bool IsSupported(Channel *pChannel);

    bool   SupportsBackend()      const { return m_SupportsBackend; }
    UINT32 GetLwrrentSubchannel() const;
    const vector<UINT32> &GetSubchHistory() const { return m_SubchHistory; }
    UINT32 GetEngine(UINT32 subch) const { return m_SubchEngine[subch]; }
    UINT32 GetClassOnSubch(UINT32 subch) const { return m_SubchClass[subch]; }
    RC ReleaseBackendSemaphore(UINT64 gpuAddr, UINT64 payload,
                               bool allowHostSemaphore, UINT32 *pEngine);
    RC ReleaseBackendSemaphoreWithTrap(UINT64 gpuAddr, UINT64 payload,
                                       bool allowHostSemaphore,
                                       UINT32 *pEngine);
    RC AcquireBackendSemaphore(UINT64 gpuAddr, UINT64 payload,
                               bool allowHostSemaphore, UINT32 *pEngine);

    // Methods that override base-class Channel methods
    //
    virtual RC Write(UINT32 Subchannel, UINT32 Method, UINT32 Data);
    virtual RC Write(UINT32 Subchannel, UINT32 Method,
                     UINT32 Count, const UINT32 *pData);
    virtual RC WriteNonInc(UINT32 Subchannel, UINT32 Method,
                           UINT32 Count, const UINT32 *pData);
    virtual RC WriteHeader(UINT32 Subch, UINT32 Method, UINT32 Count);
    virtual RC WriteNonIncHeader(UINT32 Subch, UINT32 Method, UINT32 Count);
    virtual RC WriteIncOnce(UINT32 Subchannel, UINT32 Method,
                            UINT32 Count, const UINT32 *pData);
    virtual RC WriteImmediate(UINT32 Subchannel, UINT32 Method, UINT32 Data);
    virtual RC CallSubroutine(UINT64 Offset, UINT32 Size);
    virtual RC InsertSubroutine(const UINT32 *pBuffer, UINT32 count);
    virtual RC WriteSetSubdevice(UINT32 Mask);

    virtual RC SetObject(UINT32 Subchannel, LwRm::Handle Handle);
    virtual RC UnsetObject(LwRm::Handle Subchannel);
    virtual RC SetSemaphoreOffset(UINT64 Offset);
    virtual void SetSemaphoreReleaseFlags(UINT32 flags);
    virtual RC SetSemaphorePayloadSize(SemaphorePayloadSize size);

    virtual RC BeginInsertedMethods();
    virtual RC EndInsertedMethods();
    virtual void CancelInsertedMethods();
    virtual void ClearPushbuffer();
    virtual SemaphoreChannelWrapper * GetSemaphoreChannelWrapper();
    void SetLwrrentSubchannel(UINT32 Subchannel);

private:
    RC UpdateSemaphoreState(UINT32 Subch, UINT32 Method, UINT32 Data);
    RC CleanupDirtySemaphoreState(UINT32 SubchMask);
    void PrintSemaphoreState();
    RC FindSupportedClass(const GrClassAllocator **ppSupportedGrallocs,
                          UINT32 numSupportedGrallocs,
                          bool allowHostSemaphore,
                          UINT32 *pClass, UINT32 *pSubch);
    RC Check32BitPayload(UINT64 Payload);
    RC Write9097Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Write90b5Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 LaunchDma);
    RC Write90b7Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC WriteA0b0Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Write95a1Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Write90c0Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Writec4d1Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Writec6faSemaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Writec7c0Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Writec797Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Writec7b5Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Writec7b7Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Writec7b0Semaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);
    RC Writec7faSemaphore(UINT32 Flags, UINT32 Subch,
                          UINT64 GpuAddr, UINT64 Payload, UINT32 SemaphoreD);

    //! Structure that records the data for one semaphore method, so
    //! that we can restore the previous semaphore data if it gets
    //! changed between BeginInsertedMethods() and
    //! EndInsertedMethods().
    //!
    struct SemData
    {
        SemData() {}
        SemData(UINT32 subch, UINT32 method) : Subch(subch), Method(method) {}
        UINT32 Subch     = 0; //!< Subchannel the method was sent on,
                              //!< or 0 for host methods
        UINT32 Method    = 0; //!< The method number
        UINT32 DirtyMask = 0; //!< Mask of subdevices that have "dirty" data due to
                              //!< inserted methods
        map<UINT32, UINT32> Mask2Data; //!< The method data, per subdevice.
                                       //!< The key is a the one-bit mask
                                       //!< (1 << subdev).
    };

    //! Struct that records the SemData for all semaphore methods sent
    //! to the channel, as well as the bits set by WriteSetSubdevice.
    //!
    struct SemState
    {
    public:
        SemState();

        void   SetSubdevMask(UINT32 SubdevMask);
        UINT32 GetSubdevMask() const { return m_SubdevMask; }

        void   SetSemOffset(UINT64 offset);
        UINT64 GetSemOffset() { return m_SemOffset; }

        void   SetSemReleaseFlags(UINT32 flags);
        UINT32 GetSemReleaseFlags() { return m_SemReleaseFlags; }

        void           SetSemaphorePayloadSize(SemaphorePayloadSize size);
        SemaphorePayloadSize GetSemaphorePayloadSize() { return m_SemaphorePayloadSize; }

        void AddSemData(UINT32 Subch, UINT32 Method, UINT32 Data);
        void DelSemData(UINT32 Subch, UINT32 Method);
        void GetAllSemData(vector<SemData> *pSemDataArray) const;

        void SetDirty(const SemState *pInsertedSemState);
        bool IsSubdevMaskDirty()      const { return m_SubdevMaskDirty; }
        bool IsSemOffsetDirty()       const { return m_SemOffsetDirty; }
        bool IsSemReleaseFlagsDirty() const { return m_SemReleaseFlagsDirty; }
        bool IsSemaphorePayloadSizeDirty()  const { return m_SemaphorePayloadSizeDirty; }
        bool IsDirty(UINT32 SubchvMask) const;

    private:
        static UINT64 GetKey(UINT32 Subch, UINT32 Method);
        UINT32 m_SubdevMask;            //!< Set by SetSubdevMask()
        UINT64 m_SemOffset;             //!< Set by SetSemOffset()
        UINT32 m_SemReleaseFlags;       //!< Set by SetSemReleaseFlags()
        SemaphorePayloadSize m_SemaphorePayloadSize;//!< Set by SetSemaphorePayloadSize()
        map<UINT64, SemData> m_SemData; //!< Set by AddSemData()
        bool m_HostDirty;            //!< Tells if host data is dirty due to
                                     //!< inserted methods.  Set by SetDirty().
        UINT32 m_DirtySubchannels;   //!< Mask of subchannels with dirty
                                     //!< data due to inserted methods.
                                     //!< Set by SetDirty().
        bool m_SubdevMaskDirty;      //!< Tells if m_SubdevMask is dirty
        bool m_SemOffsetDirty;       //!< Tells if m_SemOffset is dirty
        bool m_SemReleaseFlagsDirty; //!< Tells if m_SemReleaseFlages is dirty
        bool m_SemaphorePayloadSizeDirty;  //!< Tells if m_SemaphorePayloadSize is dirty
    };

    enum ChannelType { GPFIFO_CHANNEL, DMA_CHANNEL, PIO_CHANNEL };
    ChannelType m_ChannelType;           //!< What type of channel this is
    bool m_SupportsHost;                 //!< True if channel supports host
                                         //!< semaphores.
    bool m_SupportsBackend;              //!< True if channel supports backend
                                         //!< semaphores.  Only true on fermi+.

    stack<SemState> m_SemStateStack;     //!< Stack for storing semaphore
                                         //!< state
    UINT32 m_AllSubdevMask;              //!< Mask of all subdevices

    LwRm::Handle m_SubchObject[NumSubchannels];
                                         //!< Object the subchannels are
                                         //!< controlling.
    UINT32 m_SubchClass[NumSubchannels]; //!< Class the subchannels are
                                         //!< controlling.
    UINT32 m_SubchEngine[NumSubchannels];//!< Engine the subchannels are
                                         //!< controlling.
    vector<UINT32> m_SubchHistory;       //!< History of the subchannels used.
                                         //!< m_SubchHistory[0] was used most
                                         //!< recently.  m_SubchHistory.back()
                                         //!< was used longest ago.  Each
                                         //!< subch appears at most once.
    bool m_CleaningDirtySemaphoreState;  //!< Prevents relwrsive calls of
                                         //!< CleanupDirtySemaphoreState()
    UINT32 m_CleaningDirtySubchMask;     //!< Mask of subchannels being cleaned
                                         //!< by CleanupDirtySemaphoreState()
};

#endif // INCLUDED_SEMAWRAP_H
