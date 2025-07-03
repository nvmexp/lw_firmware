/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2008, 2014-2017, 2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _MASSAGE_H_
#define _MASSAGE_H_

#include "mdiag/utils/types.h"
#include <vector>

class TraceModule;
class PushBufferMessage;

//! Base class for handlers which get called for every message.
//!
//! A message massager can do with a message whatever it wants, e.g.:
//!  - overwrite/update payload (Get/SetPayload)
//!  - insert a new method/message (SetRawData)
//!  - delete message (SetRawData)
class Massager
{
public:
    virtual ~Massager() { }
    //! This method shall be overriden to provide actual massaging.
    virtual void Massage(PushBufferMessage& message) = 0;
};

//! Provides means for accessing a massaged message.
//!
//! Derived classes support message addressing nuisances,
//! such as pre-Fermi or Fermi pushbuffer format.
class PushBufferMessage
{
public:
    virtual ~PushBufferMessage();
    //! Surface reader for the massaged message.
    class SurfaceReader
    {
    public:
        virtual ~SurfaceReader() { }
        // !Attempts to read several dwords from the surface.
        virtual vector<UINT32> Read(UINT32 numberOfDwords) = 0;
    };
    //! Initializes massaged message.
    void Init(
        UINT32 pushBufferOffset,    //!< Message offset in the original pushbuffer.
        UINT32 newPushBufferOffset, //!< Message offset in the modified pushbuffer.
        SurfaceReader& reader);     //!< Reads subsequent bytes from the original surface/pushbuffer.
    //! Returns message's offset in the original, unmodified pushbuffer.
    UINT32 GetPushBufferOffset() const { return m_PushBufferOffset; }
    //! Returns message's offset in the new, modified pushbuffer.
    UINT32 GetNewPushBufferOffset() const { return m_NewPushBufferOffset; }
    //! Returns method code (LW_FIFO_DMA_METHOD_ADDRESS).
    UINT32 GetMethod() const { return m_Method; }
    //! Returns the subchannel number (LW_FIFO_DMA_METHOD_SUBCHANNEL).
    UINT32 GetSubChannelNum() const { return m_SubChannel; }
    //! Returns message's header size, in dwords.
    UINT32 GetHeaderSize() const { return m_HeaderSize; }
    //! Returns message's payload size, in dwords.
    UINT32 GetPayloadSize() const { return m_PayloadSize; }
    //! Returns original size of the message, including header and payload (can overlap).
    UINT32 GetMessageSize() const { return m_WholeSize; }
    //! Returns offset for method's payload at the specified index.
    UINT32 GetMethodOffset(UINT32 index) const;
    //! Returns original pushbuffer offset of payload at the specified index.
    UINT32 GetPayloadOffset(UINT32 index) const;
    //! Returns payload at the specified index.
    UINT32 GetPayload(UINT32 index) const;
    //! Indicates whether the message has been modified since last Init().
    bool IsModified() const { return m_Modified; }
    //! Overwrites message's payload at the specified index with a new value.
    bool SetPayload(UINT32 index, UINT32 data);
    //! Disables modification at a particular index (for SetValue).
    //!
    //! SetValue and in turn SetPayload is affected by this. ReplaceMessage
    //! ignores frozen values.
    void FreezeValue(UINT32 index);
    //! Determines whether modification at a particular offset was frozen
    bool IsOffsetFrozen(UINT32 ofs);
    //! Returns a vector containing the whole message, including header and payload.
    const vector<UINT32>& GetMessageChunk() const { return m_MessageChunk; }
    //! Restores original message if it was modified.
    void RestoreOriginal();
    //! Overwrites message, both header and payload.
    //!
    //! Further calls to GetMethodOffset/GetPayloadOffset/GetPayload/SetPayload become imposible.
    //! This function also bypasses frozen values.
    void ReplaceMessage(const vector<UINT32>& buf) { SetMessageChunk(buf, false); m_Replaced = true; }
    //! Causes removal of the whole message from pushbuffer.
    //!
    //! \sa ReplaceMessage
    void ClearMessage() { const vector<UINT32> empty; ReplaceMessage(empty); }
    //! Adds a message for particular subdevice.
    //!
    //! Takes care of adding proper subdevice masks.
    //! Typical usage:
    //! \code
    //!     vector< vector<UINT32> > newMsgs;
    //!     for (every subdev) {
    //!         message.SetPayload(someIndex, someData);
    //!         newMsgs.push_back(message.GetMessageChunk());
    //!     }
    //!     message.ClearMessage();
    //!     for (every subdev) {
    //!         message.AddSubdevMessage(subdev, newMsgs[subdev]);
    //!     }
    //! \endcode
    //!
    //! \param subdevIndex Index of subdevice for which message will be added.
    //! \param buf New message contents for the chosen subdevice.
    //!
    //! \sa ReplaceMessage, SetSubdevMessages
    void AddSubdevMessage(UINT32 subdevIndex, const vector<UINT32>& buf);
    //! Sets messages for multiple subdevices.
    //!
    //! Original message is replaced. Uses AddSubdevMessage to add messages.
    void SetSubdevMessages(const vector< vector<UINT32> >& messages);

protected:
    PushBufferMessage();
    UINT32 GetValue(UINT32 index) const { return m_MessageChunk[index]; }
    bool SetValue(UINT32 index, UINT32 value);
    void SetMessageInfo(UINT32 method, UINT32 subch, UINT32 headerSize, UINT32 payloadSize, UINT32 wholeSize);
    bool SetMessageChunk(const vector<UINT32>& buf, bool original);

private:
    virtual void InitImpl(SurfaceReader& reader) = 0;
    virtual UINT32 GetMethodOffsetImpl(UINT32 index) const = 0;
    virtual UINT32 GetPayloadOffsetImpl(UINT32 index) const = 0;
    virtual UINT32 GetPayloadImpl(UINT32 index) const = 0;
    virtual void SetPayloadImpl(UINT32 index, UINT32 data) = 0;
    //! Returns a pushbuffer method which selects a subdevice.
    virtual UINT32 SelectSubdev(UINT32 subdevIndex) const = 0;
    //! Returns a pushbuffer method which selects all subdevices.
    virtual UINT32 SelectAllSubdevs() const = 0;

    UINT32 m_PushBufferOffset;      //!< Offset of the message in original pushbuffer.
    UINT32 m_NewPushBufferOffset;   //!< Offset of the message in new pushbuffer.
    UINT32 m_Method;                //!< Method code/offset from message header.
    UINT32 m_SubChannel;            //!< Subchannel number from message header.
    UINT32 m_HeaderSize;            //!< Size of message header.
    UINT32 m_PayloadSize;           //!< Size of message payload.
    UINT32 m_WholeSize;             //!< Size of entire message.
    vector<UINT32> m_MessageChunk;  //!< Buffer containing the entire message.
    vector<UINT32> m_OriginalChunk; //!< Buffer containing original message.
    vector<bool> m_Frozen;          //!< Indicates frozen payload entries, inaccessible to SetPayload.
    bool m_Modified;                //!< Indicates whether the message has been modified.
    bool m_Replaced;                //!< Indicates whether the message has been replaced.
};

//! Supports Fermi pushbuffer format.
class FermiMessage: public PushBufferMessage
{
public:
    FermiMessage();
    virtual ~FermiMessage();

private:
    virtual void InitImpl(SurfaceReader& reader);
    virtual UINT32 GetMethodOffsetImpl(UINT32 index) const;
    virtual UINT32 GetPayloadOffsetImpl(UINT32 index) const;
    virtual UINT32 GetPayloadImpl(UINT32 index) const;
    virtual void SetPayloadImpl(UINT32 index, UINT32 data);
    //! Returns a pushbuffer method which selects a subdevice.
    virtual UINT32 SelectSubdev(UINT32 subdevIndex) const;
    //! Returns a pushbuffer method which selects all subdevices.
    virtual UINT32 SelectAllSubdevs() const;

    // Usage example:
    // If input data to an IMMD_DATA_METHOD PushBuffer > max Immediate Data, we
    // colwert the PB to a NON_INCR_METHOD one.
    void ColwertMethodToNonIncr();

    bool m_ImmediateData;       //!< Indicates whether the method has immediate (embedded) data.
    enum IncrementType
    {
        noIncrement,
        incrementOnce,
        incrementAlways
    };
    IncrementType m_Increment;  //!< Indicates increment type of method in the message.
};

//! Creates massaged message
PushBufferMessage* CreateMassagedMessage();

#endif // _MASSAGE_H_
