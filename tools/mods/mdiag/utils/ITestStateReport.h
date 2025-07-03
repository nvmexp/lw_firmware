/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _ITESTSTATEREPORT_H_
#define _ITESTSTATEREPORT_H_

#include "sim/IIface.h"
#include "sim/ITypes.h"
#include "ICrcProfile.h"

class ITestStateReport : public IIfaceObject
{
public:
// IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;

// IGpuVerif
    // this violates encapsulation a bit, but
    virtual void init(const char *testname) = 0;                          // DNR

    // during setup
    virtual void setupFailed(const char *report) = 0;                     //      +ERR

    // during a run, or potentially during cleanup
    virtual void runFailed(const char *report = 0) = 0;                   // NOO
    virtual void runInterrupted() = 0;                                    // NOO  +DNC
    virtual void runSucceeded(bool override=false) = 0;                   // YES
    virtual void crcCheckPassed() = 0;                                    // YES
    virtual void crcCheckPassedGold() = 0;                                // GOLD
    virtual void crlwsingOldVersion() = 0;                                //      +MIS
    virtual void crcNoFileFound() = 0;                                    // MIS  +CRC
    virtual void goldCrcMissing() = 0;                                    //      +MSG (if also CRC)
    virtual void crcCheckFailed(ICrcProfile *crc) = 0;              // NOO  +CRC
    virtual void crcCheckFailed(const char *report = 0) = 0;              // NOO  +CRC
    virtual void error(const char *report = 0) = 0;                       // NOO  +ERR

    virtual bool isStatusGold(void) = 0;

    virtual void disable() = 0;
    virtual void enable() = 0;
};

#endif // _ITESTSTATEREPORT_H_
