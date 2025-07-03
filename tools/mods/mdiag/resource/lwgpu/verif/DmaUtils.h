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

#ifndef INCLUDED_DMAUTILS_H
#define INCLUDED_DMAUTILS_H

#include "core/include/gpu.h"
#include "mdiag/utils/buf.h"
#include "core/include/argread.h"
#include "mdiag/resource/lwgpu/dmard.h"
#include "mdiag/resource/lwgpu/gpucrccalc.h"
#include "mdiag/resource/lwgpu/SurfaceReader.h"
#include "mdiag/resource/lwgpu/SurfaceDumper.h"

class GpuVerif;

class DmaUtils
{
public:
    DmaUtils(GpuVerif* verifier);
    ~DmaUtils();

    void Setup(BufferConfig* config);

    RC SetupDmaCheck(UINT32 gpuSubdevice);
    bool SetupDmaBuffer(UINT32 sz = 0, UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    bool SetupSurfaceCopier(UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    bool UseDmaTransfer() const { return m_DmaCheck; }
    SurfaceReader* GetSurfaceReader() { return m_SurfaceReader; }
    SurfaceDumper* GetSurfaceDumper() { return m_SurfaceDumper; }
    DmaReader* GetDmaReader() { return m_SurfaceReader->GetDmaReader(); }
    GpuCrcCallwlator* GetGpuCrcCallwlator() { return m_GpuCrcCallwlator; }
    Buf* GetBuf() { return m_pBuf; }

private:
    bool CreateGpuCrcCallwlator(UINT32 gpuSubdevice);

private:
    GpuVerif*         m_verifier;
    const ArgReader*   m_params;

    bool               m_DmaCheck;
    SurfaceReader*     m_SurfaceReader;
    SurfaceDumper*     m_SurfaceDumper;

    GpuCrcCallwlator*  m_GpuCrcCallwlator;

    Buf*               m_pBuf;
};

#endif /* INCLUDED_DMAUTILS_H */
