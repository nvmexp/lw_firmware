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

#include "lwliobist.h"

const char * LwLinkIobist::ErrorFlagTypeToString(ErrorFlagType errFlag)
{
    switch (errFlag)
    {
        case ErrorFlagType::AlignDone: return "Align Done";
        case ErrorFlagType::ScramLock: return "Scram Lock";
        case ErrorFlagType::AlignLock: return "Align Lock";
        default:
        case ErrorFlagType::None:
            break;
    }

    return "None";
}

const char * LwLinkIobist::IobbistTimeToString(IobistTime iobistTime)
{
    switch (iobistTime)
    {
        case IobistTime::Iobt20us:  return "20us";
        case IobistTime::Iobt800us: return "800us";
        case IobistTime::Iobt1s:    return "1s";
        case IobistTime::Iobt10s:   return "10s";
        default:
        case IobistTime::IobtDefault:
            break;
    }

    return "Default";
}

const char * LwLinkIobist::IobbistTypeToString(IobistType iobistType)
{
    switch (iobistType)
    {
        case IobistType::Off:       return "Off";
        case IobistType::PreTrain:  return "PreTrain";
        case IobistType::PostTrain: return "PostTrain";
        default:
            break;
    }

    return "Unknown";
}
