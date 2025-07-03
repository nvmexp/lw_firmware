/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_UtlGpuVerif_H
#define INCLUDED_UtlGpuVerif_H

#include <vector>
#include <map>
#include <memory>
#include "core/include/types.h"
#include "core/include/lwrm.h"
#include "mdiag/utl/utl.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
using namespace std;

class SurfaceInfo;
class SurfaceReader;
class Test;
class MdiagSurf;
class IGpuSurfaceMgr;
class Trace3DTest;

// This class represents a unified GpuVerif which can work for different
// type of trace. 
// UtlGpuVerif only work for the UTL since UTL control mdiag test not matter 
// which type of test it is.
// For the UTL request, it will need CallwlateCrc, GetCrcKey, DumpImageToFile, etc.
// For the different type of trace, it will have various way to implement it.
// For the trace_3d test, it leverage GpuVerif to implement it. 
// For v2d test, it can have its own way.
// For UtlUser test, it can have its own way. 
class UtlGpuVerif
{
public:
    static UtlGpuVerif * GetGpuVerif(Test * pTest, LwRm::Handle hVaSpace, LWGpuChannel * pChannel);
    virtual UINT32 CallwlateCrc(MdiagSurf * pSurf, UINT32 preCrcValue) = 0; 
    virtual string GetCrcKey(MdiagSurf * pSurf) = 0;
    virtual RC DumpImageToFile(MdiagSurf * pSurf, const string & fileName) = 0;
    // Read back the pitch surface data
    virtual void ReadSurface(void * data, MdiagSurf * pSurface) = 0;
    virtual UINT32 GetPreCrcValue() const = 0;
private:
    typedef std::tuple<Test *, LwRm::Handle, LWGpuChannel *> ResourceKey;
    static map<ResourceKey, unique_ptr<UtlGpuVerif> > s_GpuVerifs;
};

// This class is work for UTL to callwlate the crc, get crcKey, etc.
// You can extend other type trace here to handle related work.
class UtlT3dGpuVerif : public UtlGpuVerif
{
public:
    UtlT3dGpuVerif(Trace3DTest * pTest, LwRm::Handle hVaSpace, LWGpuChannel * pChannel);
    virtual UINT32 CallwlateCrc(MdiagSurf * pSurf, UINT32 preCrcValue) override; 
    virtual string GetCrcKey(MdiagSurf * pSurf) override;
    virtual RC DumpImageToFile(MdiagSurf * pSurf, const string & fileName) override;
    // Read back the pitch surface data
    virtual void ReadSurface(void * data, MdiagSurf * pSurface) override; 
    virtual UINT32 GetPreCrcValue() const override { return m_pGpuVerif->GetPreCrcValue(); }    
private:
    SurfaceInfo * GetSurfInfo(const string & name);
    void AddSurfaceInfo(MdiagSurf * pSurf);
    GpuDevice * GetGpuDevice();
    GpuSubdevice * GetSubDevice();
    UINT32 GetSubDevIndex();
    GpuVerif * m_pGpuVerif = nullptr;
    Trace3DTest * m_pTest;
};

#endif
