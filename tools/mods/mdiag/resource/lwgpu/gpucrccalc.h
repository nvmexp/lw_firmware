/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2018, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _DMA_GPU_CRC_CALLWLATOR_H_
#define _DMA_GPU_CRC_CALLWLATOR_H_

#include "dmard.h"

class GpuCrcCallwlator : public DmaReader
{
public:

    /// d'tor
    virtual ~GpuCrcCallwlator();

    //! gets the dma reader class id
    virtual UINT32 GetClassId() const { return m_Class; }

    // callwlates CRC using FECS 
    virtual RC ReadLine
    (
        UINT32 hSrcCtxDma, UINT64 SrcOffset, UINT32 SrcPitch,
        UINT32 LineCount, UINT32 LineLength,
        UINT32 gpuSubdevice, const Surface2D* srcSurf,
        MdiagSurf* dstSurf = 0, UINT64 DstOffset = 0
    ) = 0;

    //! Factory function to create appropriate DMAReader objects
    static GpuCrcCallwlator* CreateGpuCrcCallwlator
    (
        WfiType wfiType,
        LWGpuResource* lwgpu,
        LWGpuChannel* channel,
        UINT32 size,
        const ArgReader* params,
        LwRm *pLwRm,
        SmcEngine *pSmcEngine,
        UINT32 subdev
    );

protected:
    /// c'tor
    GpuCrcCallwlator(WfiType wfiType, LWGpuChannel* ch, UINT32 size, EngineType engineType);

    UINT32 m_Class;
private:
    // Prevent copying
    GpuCrcCallwlator(const GpuCrcCallwlator&);
    GpuCrcCallwlator& operator=(const GpuCrcCallwlator&);
};

class FECSCrcCallwlator : public GpuCrcCallwlator
{

    struct PollMailboxArgs
    {
        LWGpuResource* pGpuRes;
        UINT32 gpuSubdevIdx;
        LWGpuChannel* channel;
        UINT32 flag;
        bool checkIfSet;
    };

    //! Factory function to create appropriate DMAReader objects
    friend GpuCrcCallwlator* GpuCrcCallwlator::CreateGpuCrcCallwlator
    (
        WfiType wfiType,
        LWGpuResource* lwgpu,
        LWGpuChannel* channel,
        UINT32 size,
        const ArgReader* params,
        LwRm *pLwRm,
        SmcEngine *pSmcEngine,
        UINT32 subdev
    );

protected:
    /// c'tor
    FECSCrcCallwlator(WfiType wfiType, LWGpuChannel* ch, UINT32 size);

public:
    /// d'tor
    virtual ~FECSCrcCallwlator();

    // dma line from vidmem to sysmem
    virtual RC ReadLine
    (
        UINT32 hSrcCtxDma, UINT64 SrcOffset, UINT32 SrcPitch,
        UINT32 LineCount, UINT32 LineLength,
        UINT32 gpuSubdevice, const Surface2D* srcSurf,
        MdiagSurf* dstSurf = 0, UINT64 DstOffset = 0
    );

private:

    static bool PollMailbox(void* pollargs);
    static bool PollMailboxForNonzero(void* pollargs);

    // Prevent copying
    FECSCrcCallwlator(const FECSCrcCallwlator&);
    FECSCrcCallwlator& operator=(const FECSCrcCallwlator&);
};

#endif
