/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/types.h"

#include <set>

class LwLinkIobist
{
public:
    virtual ~LwLinkIobist() { }

    enum class IobistType : UINT08
    {
        Off,
        PreTrain,
        PostTrain,
    };

    enum class IobistTime : UINT08
    {
        Iobt20us,
        Iobt800us,
        Iobt1s,
        Iobt10s,
        IobtDefault
    };

    enum class ErrorFlagType : UINT08
    {
        AlignDone,
        ScramLock,
        AlignLock,
        None
    };


    struct ErrorFlag
    {
        UINT32        linkId  = 0;
        ErrorFlagType errFlag = ErrorFlagType::None;
        bool operator < (const ErrorFlag & rhs) const
        {
            if (linkId == rhs.linkId)
                return static_cast<UINT08>(errFlag) < static_cast<UINT08>(rhs.errFlag);
            return linkId < rhs.linkId;
        }
    };

    void GetErrorFlags(UINT64 linkMask, set<ErrorFlag> * pErrorFlags) const
        { DoGetIobistErrorFlags(linkMask, pErrorFlags); }
    void SetInitiator(UINT64 linkMask, bool bInitiator)
        { DoSetIobistInitiator(linkMask, bInitiator); }
    RC SetTime(UINT64 linkMask, IobistTime iobistTime)
        { return DoSetIobistTime(linkMask, iobistTime); }
    void SetType(UINT64 linkMask, IobistType iobistType)
        { DoSetIobistType(linkMask, iobistType); }

    static const char * ErrorFlagTypeToString(ErrorFlagType errFlag);
    static const char * IobbistTimeToString(IobistTime iobistTime);
    static const char * IobbistTypeToString(IobistType iobistType);

private:
    virtual void DoGetIobistErrorFlags(UINT64 linkMask, set<ErrorFlag> * pErrorFlags) const = 0;
    virtual void DoSetIobistInitiator(UINT64 linkMask, bool bInitiator) = 0;
    virtual RC DoSetIobistTime(UINT64 linkMask, IobistTime iobistTime) = 0;
    virtual void DoSetIobistType(UINT64 linkMask, IobistType iobistType) = 0;
};
