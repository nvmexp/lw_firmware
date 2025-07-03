/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2012,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GF100ECID_H
#define INCLUDED_GF100ECID_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

class GpuSubdevice;

//! Temporarily enables fuse registers on GF100 and similar.
class EnableGF100FuseRegisters
{
public:
    explicit EnableGF100FuseRegisters(GpuSubdevice* pGpuSub);
    ~EnableGF100FuseRegisters();

private:
    GpuSubdevice* m_pGpuSub;
    UINT32 m_DebugSave;
};

//! Reads raw ECID directly using registers.
//! Good for GF100
RC GetGF100RawEcid(GpuSubdevice* pSubdev, vector<UINT32>* pEcid, bool bReverseFields);

//! Reads human-readable ECID directly using registers.
//! Good for GF100
RC GetGF100CookedEcid(GpuSubdevice* pSubdev, string* pEcid);

#endif // INCLUDED_GF100ECID_H
