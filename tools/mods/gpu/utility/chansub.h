/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  chansub.h
 * @brief Custom push buffer to be used with GpFifoChannel::CallSubroutine
 */

#ifndef INCLUDED_CHANSUB_H
#define INCLUDED_CHANSUB_H

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_SURF2D_H
#include "surf2d.h"
#endif

//! \class ChannelSubroutine
//! \brief Allows for a fake pushbuffer that can be written to like a channel
//!
//! A ChannelSubroutine simply utilizes Surface2D and the channel class and
//! makes using the CallSubroutine of GpFifoChannels more natural to use.  It
//! emulates the GpFifoChannel::Write function, so methods can be written to
//! this custom push buffer just as easily as they can be written to a
//! GpFifoChannel itself.

class GpuTestConfiguration;
class Channel;

class ChannelSubroutine
{
public:
    ChannelSubroutine(Channel *pCh, GpuTestConfiguration *pGTC = NULL);
    ~ChannelSubroutine();

//-----------------------  Getters (Accessors)  ------------------------------//

    //! Get GPU vaddr for use with Channel::CallSubroutine.
    //! Asserts if FinishSubroutine hasn't been called.
    UINT64 Offset() const;

    //! Get size for use with Channel::CallSubroutine.
    //! Asserts if FinishSubroutine hasn't been called.
    UINT32 Size() const;

    //! Send this subroutine to a Channel.
    //! Returns error if FinishSubroutine hasn't been called.
    RC Call(Channel * pCh) const;

//-----------------------  Setters (Mutators)   ------------------------------//

    RC FinishSubroutine();
    RC FinishSubroutine(UINT64 *Offset, UINT32 *Size);

    // Emulates the Write functions of the channel class
    // No write commands may be issued after FinishSubroutine,
    //      or RC::SOFTWARE_ERROR will be returned
    RC  Write(UINT32 Subchannel, UINT32 Method, UINT32 Data);
    RC  Write(
            UINT32 Subchannel,
            UINT32 Method,
            UINT32 Count,
            UINT32 Data,
            ...  //Rest of Data
        );
    RC  Write(
            UINT32         Subchannel,
            UINT32         Method,
            UINT32         Count,
            const UINT32 * pData
        );

    // Will write RawData directly into push buffer vector
    RC WriteRaw(const UINT32 *pData, UINT32 Count);
    RC WriteRaw(UINT32 Data) { return WriteRaw(&Data, 1); }

private:
    void MakeRoomInBuffer(UINT32 size); // Checks to see if buffer has room
                                        // for (size) elements.  If not, make
                                        // more room

    GpuTestConfiguration *m_pGTC;
    Channel        *m_pCh;
    UINT32          m_Size;     // Size (in bytes) of the push buffer data
    vector<UINT32>  m_Buffer;   // Storage for Channel::WriteExternal
    UINT32          m_BufPut;   // Put index of the above buffer
    UINT32          m_BufSize;  // Size of m_Buffer in UINT32s
    Surface2D       m_LwstomPb; // For final storage of push buffer data
};

#endif
