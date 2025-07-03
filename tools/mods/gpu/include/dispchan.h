/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DisplayChannel interface.

#ifndef INCLUDED_DISPCHAN_H
#define INCLUDED_DISPCHAN_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif
#ifndef INCLUDED_GPU_H
#include "core/include/gpu.h"
#endif

//------------------------------------------------------------------------------
// DisplayChannel interface.
//
class DisplayChannel
{
public:
    enum class DispChannelType : UINT08
    {
        CORE,
        BASE,
        OVERLAY,
        WINDOW,
        WINDOW_IMM,
        NUM_TYPES
    };

    DisplayChannel::DispChannelType ChannelNameToType(string name);

    static const UINT32 AllSubdevicesMask = 0xfff;

    virtual ~DisplayChannel() { }

    //! Set options for automatic flushing of the channel
    virtual RC SetAutoFlush
    (
        bool AutoFlushEnable,
        UINT32 AutoFlushThreshold
    ) = 0;

    virtual RC GetAutoFlush
    (
        bool *pAutoFlushEnable,
        UINT32 *pAutoFlushThreshold
    ) const = 0;

    //! Write a single method into the channel.
    virtual RC Write
    (
        UINT32 Method,
        UINT32 Data
    ) = 0;

    //! Write multiple methods to the channel.
    virtual RC Write
    (
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  DataPtr
    ) = 0;
    virtual RC WriteNonInc
    (
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  DataPtr
    ) = 0;

    //! Write various control methods into the channel.
    virtual RC WriteNop()                          = 0;
    virtual RC WriteJump(UINT32 Offset)            = 0;
    virtual RC WriteSetSubdevice(UINT32 Subdevice) = 0;

    virtual RC WriteOpcode
    (
        UINT32          Opcode,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  DataPtr
    ) = 0;

    //! Flush the method(s) and data.
    virtual RC Flush() = 0;

    //! Wait for the hardware to finish pulling all flushed methods.
    virtual RC WaitForDmaPush
    (
        FLOAT64 TimeoutMs
    ) = 0;

    //! Wait for the hardware to finish exelwting all flushed methods.
    RC WaitIdle
    (
        FLOAT64 TimeoutMs
    );

    //! Get and set the default timeout used for functions where no timeout is
    //! explicitly passed in
    void SetDefaultTimeoutMs(FLOAT64 TimeoutMs);
    FLOAT64 GetDefaultTimeoutMs() const;

    //! Return a MODS RC if a channel error has been written to the
    //! error notifier memory.
    RC CheckError() const;

    virtual RC SetSkipFreeCount(bool skip) {return OK;}

    virtual UINT32 GetRoomPercent() {return 100;}

    //! Set information on pushbuffer memory space
    void SetPushbufMemSpace(Memory::Location PushbufMemSpace);

    //! Enable very verbose channel debug spew (all methods and data, all flushes).
    void EnableLogging(bool b);

protected:
    DisplayChannel
    (
        void *       ControlRegs[LW2080_MAX_SUBDEVICES],
        GpuDevice *  pGpuDev
    );

    //! Pointer(s) (to BAR0 GPU register space) to channel control structure(s).
    //! Single-GPU devices will populate only entry [0] in m_ControlRegs.
    void *       m_ControlRegs[LW2080_MAX_SUBDEVICES];
    void *       m_pErrorNotifierMemory;
    LwRm::Handle m_hChannel;
    Memory::Location m_PushbufMemSpace;
    FLOAT64      m_TimeoutMs;
    GpuDevice *  m_pGpuDevice;
    UINT32       m_NumSubdevices;

    //! Enable very verbose channel info dumping.
    bool         m_EnableLogging;

    void LogData(UINT32 n, UINT32 * pData) const;
};

//------------------------------------------------------------------------------
// PIO display channel implementation.
//
class PioDisplayChannel : public DisplayChannel
{
public:
    PioDisplayChannel
    (
        void *       ControlRegs[LW2080_MAX_SUBDEVICES],
        void *       pErrorNotifierMemory,
        LwRm::Handle hChannel,
        GpuDevice *  pGpuDev
    );
    ~PioDisplayChannel();

    RC SetAutoFlush
    (
        bool AutoFlushEnable,
        UINT32 AutoFlushThreshold
    ) override;

    RC GetAutoFlush
    (
        bool *pAutoFlushEnable,
        UINT32 *pAutoFlushThreshold
    ) const override;

    RC Write
    (
        UINT32 Method,
        UINT32 Data
    ) override;
    RC Write
    (
        UINT32         Method,
        UINT32         Count,
        const UINT32 * Data
    ) override;
    RC WriteNonInc
    (
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  DataPtr
    ) override;

    RC WriteNop() override;
    RC WriteJump(UINT32 Offset) override;
    RC WriteSetSubdevice(UINT32 Subdevice) override;

    RC WriteOpcode
    (
        UINT32          Opcode,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  DataPtr
    ) override;

    RC Flush() override;

    RC WaitForDmaPush
    (
        FLOAT64 TimeoutMs
    ) override;

    RC SetSkipFreeCount(bool skip) override;

private:
    virtual RC WaitFreeCount
    (
        UINT32 Words,
        FLOAT64 TimeoutMs
    );

    bool m_SkipFreeCount;

    // The cached free count is maintained in number of words, rather than
    // in number of bytes.
    UINT32 m_CachedFreeCount[LW2080_MAX_SUBDEVICES];
};

//------------------------------------------------------------------------------
// DMA display channel implementation.
//
class DmaDisplayChannel : public DisplayChannel
{
public:
    DmaDisplayChannel
    (
        void *       ControlRegs[LW2080_MAX_SUBDEVICES],
        void *       pPushBufferBase,
        UINT32       PushBufferSize,
        void *       pErrorNotifierMemory,
        LwRm::Handle hChannel,
        UINT32       Offset,
        GpuDevice *  pGpuDevice
    );
    ~DmaDisplayChannel();

    RC SetAutoFlush
    (
        bool AutoFlushEnable,
        UINT32 AutoFlushThreshold
    ) override;

    RC GetAutoFlush
    (
        bool *pAutoFlushEnable,
        UINT32 *pAutoFlushThreshold
    ) const override;

    RC Write
    (
        UINT32 Method,
        UINT32 Data
    ) override;
    RC Write
    (
        UINT32         Method,
        UINT32         Count,
        const UINT32 * Data
    ) override;
    RC WriteNonInc
    (
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  DataPtr
    ) override;

    RC WriteNop() override;
    RC WriteJump(UINT32 Offset) override;
    RC WriteSetSubdevice(UINT32 Subdevice) override;

    RC WriteOpcode
    (
        UINT32          Opcode,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  DataPtr
    ) override;

    RC Flush() override;

    RC WaitForDmaPush
    (
        FLOAT64 TimeoutMs
    ) override;

    UINT32 GetRoomPercent() override;

    UINT32 GetCachedPut() const;

    virtual UINT32 ScanlineRead(UINT32 head);

    virtual UINT32 GetPut(string ChannelName);

    virtual void SetPut(string ChannelName, UINT32 newPut);

    virtual UINT32 GetGet(string ChannelName);

    virtual void SetGet(string ChannelName, UINT32 newGet);

    virtual UINT32 GetPut(DispChannelType ch);

    virtual void SetPut(DispChannelType ch, UINT32 newPut);

    virtual UINT32 GetGet(DispChannelType ch);

    virtual void SetGet(DispChannelType ch, UINT32 newGet);

    virtual UINT32 ComposeMethod
    (
        UINT32 OpCode,
        UINT32 Count,
        UINT32 Offest
     );

private:
    struct PollArgs
    {
        DmaDisplayChannel *pThis;
        volatile UINT32 * pGet[LW2080_MAX_SUBDEVICES];
        UINT32 *pCachedGets;
        UINT32 NumSubdevices;
        UINT32 Put;
        UINT32 Count;
        void * pErrNotifier;
    };

    static bool PollGetEqPut (void * pVoidPollArgs);
    static bool PollGetNotEqPut (void * pVoidPollArgs);
    static bool PollWrap (void * pVoidPollArgs);
    static bool PollNoWrap (void * pVoidPollArgs);

    // Options for automatic flushing
    bool     m_AutoFlushEnable;
    UINT32   m_AutoFlushThreshold;

    // Pointer to the base of the pushbuffer
    UINT08 * m_pPushBufferBase;

    // The size of the pushbuffer context DMA
    UINT32   m_PushBufferSize;

    // Pointer to where we are lwrrently writing in the pushbuffer
    UINT32 * m_pLwrrent;

    // Pointer to where the next automatic flush should occur.
    UINT32 * m_pNextAutoFlush;

    // Cached Put pointer: this is always the last value we wrote into the
    // channel's Put register.
    UINT32   m_CachedPut;

    // Cached Get pointers: these are the last value we read from the
    // channel's Get register (for each subdevice).
    UINT32   m_CachedGet[LW2080_MAX_SUBDEVICES];

    // The last point where we flushed the CPU caches, etc. for the pushbuffer.
    // This is usually, but not always, the same as m_CachedPut.
    UINT32   m_LastFlushAt;

    UINT32   m_HighWaterMark;
    PollArgs m_PollArgs;

    RC MakeRoom(UINT32 Count);

    void UpdateCachedGets();

    // Alignment requirement for the Put pointer
    UINT32 m_RqdPutAlignment;
};

class DmaDisplayChannelC3 : public DmaDisplayChannel
{
public:
    DmaDisplayChannelC3
    (
        void *       ControlRegs[LW2080_MAX_SUBDEVICES],
        void *       pPushBufferBase,
        UINT32       PushBufferSize,
        void *       pErrorNotifierMemory,
        LwRm::Handle hChannel,
        UINT32       Offset,
        GpuDevice *  pGpuDevice
    );
    ~DmaDisplayChannelC3();

    UINT32 ScanlineRead(UINT32 head) override;
    UINT32 GetPut(DispChannelType ch) override;
    void   SetPut(DispChannelType ch, UINT32 newPut) override;
    UINT32 GetGet(DispChannelType ch) override;
    void   SetGet(DispChannelType ch, UINT32 newGet) override;
    UINT32 GetPut(string ChannelName) override;
    void   SetPut(string ChannelName, UINT32 newPut) override;
    UINT32 GetGet(string ChannelName) override;
    void   SetGet(string ChannelName, UINT32 newGet) override;
    UINT32 ComposeMethod
    (
        UINT32 OpCode,
        UINT32 Count,
        UINT32 Offest
    ) override;
};

class DebugPortDisplayChannelC3 : public DisplayChannel
{
private:
    UINT32  m_ChannelNum;

public:
    DebugPortDisplayChannelC3
    (
        UINT32       channelNum,
        GpuDevice *  pGpuDevice
    );
    ~DebugPortDisplayChannelC3();

    RC Write
    (
        UINT32 method,
        UINT32 data
    ) override;

    RC SetAutoFlush
    (
        bool    bAutoFlushEnable,
        UINT32  autoFlushThreshold
    ) override;

    RC GetAutoFlush
    (
        bool *pAutoFlushEnable,
        UINT32 *pAutoFlushThreshold
    ) const override;

    RC Write
    (
        UINT32          method,
        UINT32          count,
        const UINT32 *  dataPtr
    ) override;

    RC WriteNonInc
    (
        UINT32          method,
        UINT32          count,
        const UINT32 *  dataPtr
    ) override;

    RC WriteNop() override;
    RC WriteJump(UINT32 offset) override;
    RC WriteSetSubdevice(UINT32 subdevice) override;

    RC WriteOpcode
    (
        UINT32          opcode,
        UINT32          method,
        UINT32          count,
        const UINT32 *  dataPtr
    ) override;

    RC Flush() override;

    RC WaitForDmaPush
    (
        FLOAT64 timeoutMs
    ) override;

    UINT32  GetChannelNum() const;
};
#endif // ! INCLUDED_DISPCHAN_H
