/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLCHANWRAP_H
#define INCLUDED_UTLCHANWRAP_H

#include "core/include/chanwrap.h"

class Channel;
class Utl;
class UtlChannel;

// This class wraps UTL channels and is used to trigger channel related
// events (for example, method writes).
//
class UtlChannelWrapper : public ChannelWrapper
{
public:
    UtlChannelWrapper(Channel* channel);

    static bool IsSupported(Channel* channel);

    UtlChannel* GetUtlChannel() const { return m_UtlChannel; }

    // The remaining public methods all override base-class Channel methods.

    // Write a single method into the channel.
    virtual RC Write
    (
        UINT32 subchannel,
        UINT32 method,
        UINT32 data
    );

    // Write multiple methods to the channel, passing the data as an array.
    virtual RC Write
    (
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32* dataArray
    );

    // Write multiple non-incrementing methods to the channel,
    // passing the data as an array.
    virtual RC WriteNonInc
    (
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32* dataArray
    );

    // Write an increment-once method, passing the data as an array.
    virtual RC WriteIncOnce
    (
        UINT32 subchannel,
        UINT32 method,
        UINT32 count,
        const UINT32* dataArray
    );

    // Write an immediate-data method.
    virtual RC WriteImmediate
    (
        UINT32 subchannel,
        UINT32 method,
        UINT32 data
    );

    virtual RC BeginInsertedMethods();
    virtual RC EndInsertedMethods();
    virtual void CancelInsertedMethods();

    virtual RC WriteHeader
    (
        UINT32 subchannel,
        UINT32 method,
        UINT32 count
    );

    virtual RC WriteNonIncHeader
    (
        UINT32 subchannel,
        UINT32 method,
        UINT32 count
    );

    virtual RC WriteNop();
    virtual RC WriteSetSubdevice(UINT32 mask);

    virtual RC SetCachedPut(UINT32);
    virtual RC SetPut(UINT32);

    virtual UtlChannelWrapper* GetUtlChannelWrapper();

private:
    void SetUtlChannel(UtlChannel* utlChannel) { m_UtlChannel = utlChannel; }

    // This is so the UtlChannel constructor can call SetUtlChannel.
    friend class UtlChannel;

    // The UTL channel that this object is wrapping.
    UtlChannel* m_UtlChannel = nullptr;

    // The total number of methods written to the channel so far
    UINT32 m_TotalMethodsWritten = 0;

    // The total number of test methods written to the channel so far
    UINT32 m_TotalTestMethodsWritten = 0;

    // Used to detect TriggerEvent being called relwrsively
    bool m_Relwrsive = false;

    // Used to keep track of of nested BeginInsertedMethods calls
    UINT32 m_NestedInsertedMethods = 0;

    RC TriggerEvent
    (
        UINT32 subchannel,
        UINT32 method,
        UINT32 data,
        bool afterWrite,
        UINT32 groupIndex,
        UINT32 groupCount,
        bool* skipWrite
     );
};

#endif
