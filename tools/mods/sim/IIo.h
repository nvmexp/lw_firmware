/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IIO_H_
#define _IIO_H_

#include "ITypes.h"

class IIo : public IIfaceObject {
public:
    virtual LwU008 IoRd08(LwU016 address) = 0;
    virtual LwU016 IoRd16(LwU016 address) = 0;
    virtual LwU032 IoRd32(LwU016 address) = 0;
    virtual void IoWr08(LwU016 address, LwU008 data) = 0;
    virtual void IoWr16(LwU016 address, LwU016 data) = 0;
    virtual void IoWr32(LwU016 address, LwU032 data) = 0;

    virtual LwU008 CfgRd08(LwU032 address) = 0;
    virtual LwU016 CfgRd16(LwU032 address) = 0;
    virtual LwU032 CfgRd32(LwU032 address) = 0;
    virtual void CfgWr08(LwU032 address, LwU008 data) = 0;
    virtual void CfgWr16(LwU032 address, LwU016 data) = 0;
    virtual void CfgWr32(LwU032 address, LwU032 data) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
