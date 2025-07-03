/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _SURFACE_DUMPER_H_
#define _SURFACE_DUMPER_H_

#include "SurfaceReader.h"
#include "surfasm.h"
#include "mdiag/utils/buf.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"

class SurfaceDumper
{
public:
    SurfaceDumper(
        SurfaceReader* surfaceReader,
        IGpuSurfaceMgr *surfMgr,
        const ArgReader* params);

    virtual ~SurfaceDumper();

    virtual void SetBuf(Buf* buf) { m_pBuf = buf; }

    // The following replaces DumpActiveSurface, DumpColorOutput,
    // and DumpZetaOutput.
    // if surface_base == 0, then the surface will be read.
    // profileKey is handed to SaveBufTGA call.
    RC DumpSurface(const char *filename, MdiagSurf *surface,
        UINT08* surface_base, const SurfaceAssembler::Rectangle &rect, const char* profileKey,
        UINT32 gpuSubdevice, bool rawCrc);

    // will read the buffer if bufferData == 0
    // assumes that the "fullFilename" is passed in for filename.
    // assumes ".gz" has been added to the filename if use_gz == TRUE
    RC DumpDmaBuf(const char *filename, CheckDmaBuffer *pCheck,
        UINT08 *bufferData, bool use_gz, UINT32 gpuSubdevice = 0,
        UINT32 chunkSize = 0);

private:
    SurfaceReader*   m_pSurfaceReader;
    IGpuSurfaceMgr*  m_surfMgr;
    Buf*             m_pBuf;
    const ArgReader* m_params;
};

#endif
