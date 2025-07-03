/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_DUMPUTILS_H
#define INCLUDED_DUMPUTILS_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#include "mdiag/utils/buf.h"
#include "core/include/argread.h"
#include "gpu/utility/surf2d.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/resource/lwgpu/dmard.h"
#include "mdiag/resource/lwgpu/gpucrccalc.h"
#include "mdiag/resource/lwgpu/SurfaceReader.h"
#include "mdiag/resource/lwgpu/SurfaceDumper.h"

#include "DmaUtils.h"
#include "SurfBufInfo.h"
#include "GpuVerif.h"

class MdiagSurf;

class DumpUtils
{
public:
    DumpUtils(GpuVerif* verifier);
    ~DumpUtils();

public:
    // Buffer & Surface
    RC DumpDmaBuffer(const char *filename, CheckDmaBuffer *pCheck, UINT08 *Data, UINT32 gpuSubdevice);
    RC ReadAndDumpDmaBuffer(MdiagSurf *buffer, UINT32 size, string &name, UINT32 gpuSubdevice = 0);

    // CRC
    RC DumpCRCInfos() const;
    void DumpCRCsPostTest();
    bool DumpUpdateCrcs (TestEnums::TEST_STATUS& status, ITestStateReport* report, const string& tgaProfileKey, UINT32 gpuSubdevice);

    // Surface and Images
    bool DumpImages(const string& tgaProfileKey, const UINT32 gpuSubdevice);
    bool DumpActiveSurface(MdiagSurf* surface, const char* fname , UINT32 gpuSubdevice);
    RC DumpRawImages(UINT08* buffer, UINT32 size, UINT32 subdev, const string& output_filename);
    RC DumpSurfOrBuf(MdiagSurf* dma, string &fname, UINT32 gpuSubdevice);

    // Interrupt
    bool DumpUpdateIntrs(ITestStateReport* report);

private:
    DmaReader* GetDmaReader();
    const string GetIntrFileName() const;

    bool DumpUpdateAllocSurfImages(SurfInfo2 *info, const string &tgaProfileKey, UINT32 gpuSubdevice);
    void optionalCreateDirectoryContainingFile(const char *filename_in);

private:
    GpuVerif*       m_verifier;
    DmaUtils*        m_DmaUtils;

    const ArgReader* m_params;
};

#endif /* INCLUDED_DUMPUTILS_H */
