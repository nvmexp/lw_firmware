/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "DmaUtils.h"

#include "mdiag/sysspec.h"
#include "mdiag/utils/surfutil.h"
#include "mdiag/tests/gpu/trace_3d/tracechan.h"

#include "GpuVerif.h"

DmaUtils::DmaUtils(GpuVerif* verifier) : m_verifier(verifier)
{
    m_params = m_verifier->Params();
    m_DmaCheck = m_verifier->NeedDmaCheck();

    m_SurfaceReader = new SurfaceReader(
            m_verifier->GetLwRmPtr(), m_verifier->GetSmcEngine(),
            m_verifier->LWGpu(), m_verifier->SurfaceManager(),
            m_params, m_verifier->TraceArchClass(), m_verifier->Channel(), m_DmaCheck);

    m_SurfaceDumper = new SurfaceDumper(
            m_SurfaceReader, m_verifier->SurfaceManager(), m_params);

    m_GpuCrcCallwlator = NULL;

    m_pBuf = new Buf;
}

void DmaUtils::Setup(BufferConfig* config)
{
    m_pBuf->SetConfigBuffer(config);
    m_SurfaceDumper->SetBuf(m_pBuf);
}

DmaUtils::~DmaUtils()
{
    if (m_pBuf)
    {
        delete m_pBuf;
        m_pBuf = 0;
    }

    if (m_GpuCrcCallwlator)
    {
        delete m_GpuCrcCallwlator;
        m_GpuCrcCallwlator = 0;
    }

    if (m_SurfaceDumper)
    {
        delete m_SurfaceDumper;
        m_SurfaceDumper = 0;
    }

    if (m_SurfaceReader)
    {
        delete m_SurfaceReader;
        m_SurfaceReader = 0;
    }
}

RC DmaUtils::SetupDmaCheck(UINT32 gpuSubdevice)
{
    if (m_DmaCheck)
    {
        if (!SetupDmaBuffer(0, gpuSubdevice))
        {
            ErrPrintf("Failed to setup dma buffer for dma crc check\n");
            return RC::SOFTWARE_ERROR;
        }

        if (m_params->ParamUnsigned("-dma_check") == 6)
        {
            if (!CreateGpuCrcCallwlator(gpuSubdevice))
            {
                ErrPrintf("Failed to setup gpu calculator for gpu crc check\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    return OK;
}

bool DmaUtils::SetupDmaBuffer( UINT32 sz, UINT32 subdev )
{
    if (m_SurfaceReader->GetDmaReader())            // DmaBuffer already setup
    {
        return true;
    }

    if (!m_SurfaceReader->SetupDmaBuffer(sz, subdev))
    {
        return false;
    }

    return true;
}

bool DmaUtils::SetupSurfaceCopier( UINT32 subdev )
{
    if (m_SurfaceReader->GetSurfaceCopier())            // DmaBuffer already setup
    {
        return true;
    }

    m_SurfaceReader->SetupSurfaceCopier(subdev);

    return true;
}

bool DmaUtils::CreateGpuCrcCallwlator(UINT32 gpuSubdevice)
{
    if (m_GpuCrcCallwlator)
    {
        return true; // it has been created
    }

    // create dmaReader
    m_GpuCrcCallwlator = GpuCrcCallwlator::CreateGpuCrcCallwlator(
        DmaReader::GetWfiType(m_params), m_verifier->LWGpu(),
        m_verifier->Channel() ? m_verifier->Channel()->GetCh() : 0,
        (UINT32)4, m_params, m_verifier->GetLwRmPtr(), m_verifier->GetSmcEngine(),
        gpuSubdevice);

    // allocate notifier
    if (!m_GpuCrcCallwlator->AllocateNotifier(GetPageLayoutFromParams(m_params, NULL)))
    {
        ErrPrintf("Error oclwred while allocating DmaReader notifier!\n");
        return false;
    }

    // allocate dst dma buffer
    if (m_GpuCrcCallwlator->AllocateDstBuffer(GetPageLayoutFromParams(m_params, NULL), (UINT32)16, _DMA_TARGET_VIDEO) == false)
    {
        ErrPrintf("Error oclwred while allocating DmaReader dst buffer!\n");
        return false;
    }

    // allocate dma reader handles
    if (m_GpuCrcCallwlator->AllocateObject() == false)
    {
        ErrPrintf("Error oclwred while allocating DmaReader handles!\n");
        return false;
    }

    return true;
}
