/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utlperfmonbuffer.h"
#include "utl.h"
#include "utlgpu.h"
#include "utlutil.h"
#include "core/include/massert.h"

void UtlPerfmonBuffer::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("PerfmonBuffer.Create", "gpu", false, "GPU to which this PerfmonBuffer belongs");
    kwargs->RegisterKwarg("PerfmonBuffer.Dump", "filename", true, "the file name to store the data from perfmon buffer");

    py::class_<UtlPerfmonBuffer, UtlSurface> buffer(module, "PerfmonBuffer");
    buffer.def_static("Create", &UtlPerfmonBuffer::Create,
        UTL_DOCSTRING("PerfmonBuffer.Create",
            "This function creates a PerfmonBuffer object."),
        py::return_value_policy::reference);
    buffer.def("Bind", &UtlPerfmonBuffer::Bind,
        UTL_DOCSTRING("PerfmonBuffer.Bind",
            "This function initializes the buffer and configures PMA."));
    buffer.def("Dump", &UtlPerfmonBuffer::Dump,
        UTL_DOCSTRING("PerfmonBuffer.Dump",
            "This function dumps the buffer into a file."));
}

// This constructor should only be called from UtlPerfmonBuffer::Create.
//
UtlPerfmonBuffer::UtlPerfmonBuffer(unique_ptr<PerfmonBuffer> pPerfmonBuffer, MdiagSurf* pMdiagSurf)
    : UtlSurface(pMdiagSurf)
{
    m_pPerfmonBuffer = move(pPerfmonBuffer);
}

UtlPerfmonBuffer::~UtlPerfmonBuffer()
{
    m_pPerfmonBuffer.reset(nullptr);
}

// This function can be called from a UTL script to create a perfmon buffer. 
//
UtlPerfmonBuffer* UtlPerfmonBuffer::Create(py::kwargs kwargs)
{
    UtlUtility::PyError("utl.PerfmonBuffer is deprecated, stop using it!");
    MASSERT(0);

    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu", "PerfmonBuffer.Create"))
    {
        // If the user doesn't specify a GPU, use GPU 0 by default.
        gpu = UtlGpu::GetGpus()[0];
    }

    if (kwargs.contains("keepCpuMapping"))
    {
        UtlUtility::PyError("kwarg keepCpuMapping is not supported for PerfmonBuffer\n");
    }

    // Set keepCpuMapping to always true since PerfmonBuffer does an explicit 
    // Map()/Unmap() call during init and free respectively
    kwargs["keepCpuMapping"] = true;

    return static_cast<UtlPerfmonBuffer*>(
        UtlSurface::DoCreate(kwargs, 
            [&](MdiagSurf* pMdiagSurf)
            {
                pMdiagSurf->SetLocation(Memory::Coherent);
                unique_ptr<PerfmonBuffer> perfmonBuffer = make_unique<PerfmonBuffer>( 
                    gpu->GetGpuResource(), pMdiagSurf, false);
                return unique_ptr<UtlSurface>(
                    new UtlPerfmonBuffer(move(perfmonBuffer), pMdiagSurf));
            }
        )
    );
}


// This function can be called from a UTL script to bind a perfmon buffer, 
// Which will first allocate and init the buffer, then config PMA registers. 
//
void UtlPerfmonBuffer::Bind()
{
    MASSERT(m_pPerfmonBuffer);

    m_pPerfmonBuffer->Setup(GetLwRm());
}

// This function can be called from a UTL script to dump a perfmon buffer. 
// For MODE C, text file will be dumped. For MODE E, binary file will be dumped. 
//
void UtlPerfmonBuffer::Dump(py::kwargs kwargs)
{
    MASSERT(m_pPerfmonBuffer);
    if (!IsAllocated())
    {
        UtlUtility::PyError("can't dump a perfmon buffer before it has been binded.");
    }

    string filename = UtlUtility::GetRequiredKwarg<string>(kwargs, "filename", "PerfmonBuffer.Dump");

    UtlGil::Release gil;

    if (!m_pPerfmonBuffer->OpenFile(filename))
    {
        MASSERT(!"UtlPerfmonBuffer::Dump: failed to open file!");
    }
    m_pPerfmonBuffer->DumpToFile();
    m_pPerfmonBuffer->CloseFile();
}
