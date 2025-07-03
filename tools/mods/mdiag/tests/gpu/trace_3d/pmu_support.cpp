/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "class/cl85b6.h"
#include "ctrl/ctrl85b6.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/perf/pmusub.h"
#include "gputrace.h"
#include "traceop.h"

//-----------------------------------------------------------------------------
//! Bind the DMA buffer for the module onto the PMUs specified in the subdevice
//! mask.  If necessary also bind/map the DMA buffer into the PMU address space.
//!
RC GenericTraceModule::BindPMUSurface()
{
    GpuDevice    * pGpuDev = m_Test->GetBoundGpuDevice();
    GpuSubdevice * pSubdev;
    PMU          * pPmu;
    PHYSADDR       pmuVA = (UINT64)0;
    bool           bPmuSupported = false;
    RC             rc;

    InfoPrintf("%s : Binding %s\n",
               m_ModuleName.c_str(),
               GpuTrace::GetFileTypeData(m_FileType).Description);

    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); subdev++)
    {
        pSubdev = pGpuDev->GetSubdevice(subdev);

        // Validate that this GPU has a PMU setup
        if (pSubdev->GetPmu(&pPmu) != OK)
        {
            InfoPrintf("%s : PMU not supported on subdevice %d\n",
                       m_ModuleName.c_str(),
                       subdev);
            continue;
        }

        bPmuSupported = true;

        CHECK_RC(pPmu->BindSurface(m_pDmaBuf->GetSurf2D(),
                                   m_FileType - GpuTrace::FT_PMU_0,
                                   &pmuVA));

        pmuVA += m_BufOffset;
        InfoPrintf("%s : %s bound to PMU[%d:%d] at PMU virtual "
                   "address 0x%llx\n",
                   m_ModuleName.c_str(),
                   GpuTrace::GetFileTypeData(m_FileType).Description,
                   pGpuDev->GetDeviceInst(),
                   subdev,
                   pmuVA);
    }

    if (!bPmuSupported)
        ErrPrintf("%s : PMU not supported on any subdevice\n",
                   m_ModuleName.c_str());

    return bPmuSupported ? OK : RC::UNSUPPORTED_FUNCTION;
}

bool GenericTraceModule::IsPMUSupported()
{
    return ((m_FileType >= GpuTrace::FT_PMU_0) && (m_FileType <= GpuTrace::FT_PMU_7));
}

RC GenericTraceModule::GetOffsetPmu(UINT32 subdev, UINT64 *pOffset)
{
    PMU          * pPmu;
    GpuSubdevice * pSubdev = m_Test->GetBoundGpuDevice()->GetSubdevice(subdev);

    MASSERT(pSubdev);
    MASSERT(pOffset);

    *pOffset = (UINT64)0;

    if (!IsPMUSupported() ||
        (pSubdev->GetPmu(&pPmu) != OK))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    *pOffset = m_pDmaBuf->GetCtxDmaOffsetGpuObject(pPmu->GetHandle()) +
               m_BufOffset;

    return OK;
}

TraceOpPmu *GpuTrace::FindPmuOp(UINT32 cmdId)
{

    vector<TraceOpPmu *>::iterator ppPmuOp = m_PmuCmdTraceOps.begin();
    while (ppPmuOp != m_PmuCmdTraceOps.end())
    {
        if ((*ppPmuOp)->GetCmdId() == cmdId)
        {
            return (*ppPmuOp);
        }
        ppPmuOp++;
    }

    return 0;
}
