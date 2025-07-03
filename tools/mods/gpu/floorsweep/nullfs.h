/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * This is the class interface for the Floorsweep implementor class on Kepler+
 * style of floorsweeping.
 * Floorsweeping is typically done during bringup, on fmodel, and during lots
 * of arch tests.
 * Floorsweeping is not done on production test systems without good cause.
 * To floorsweep something there would be a command line argument as follows:
 * -fermi_fs <feature-string> where
 * feature-string = <name>:<value>[:<value>...]
 */
#ifndef INCLUDED_NULLFS_H
#define INCLUDED_NULLFS_H

#include "gpu/include/floorsweepimpl.h"

class NullFs : public FloorsweepImpl
{
public:

    // Constructor
    NullFs( GpuSubdevice *pSubdev);
    virtual ~NullFs() {}

protected:
    // Gpu specific APIs must be implemented by derived class.
    virtual RC      ApplyFloorsweepingChanges(bool InPreVBIOSSetup) { return RC::OK; }

    // Fermi+ masks
    virtual UINT32  FbioMaskImpl() const {return 0;}
    virtual UINT32  FbMaskImpl() const {return 0;}
    virtual UINT32  FbpMaskImpl() const {return 0;}
    virtual UINT32  FbpaMaskImpl() const {return 0;}
    virtual UINT32  FscontrolMask() const {return 0;}
    virtual RC      GetFloorsweepingMasks(FloorsweepingMasks* pFsMasks) const {return OK;}
    virtual UINT32  GpcMask() const {return 0;}
    virtual UINT32  LwencMask() const {return 0;}
    virtual UINT32  LwdecMask() const {return 0;}

    // Tesla masks
    virtual UINT32  GetTpcEnableMask() const {return 0;}

};
#endif

