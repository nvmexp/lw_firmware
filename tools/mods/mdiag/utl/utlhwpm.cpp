/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utlhwpm.h"
#include "utl.h"
#include "utlgpu.h"
#include "utlutil.h"
#include "ctrl/ctrlb0cc.h"
#include "class/clb2cc.h"
#include "lwRmReg.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "core/include/registry.h"

map<UtlGpu*, vector<unique_ptr<UtlPmaChannel>>> UtlPmaChannel::s_PmaChannels;

void UtlPmaChannel::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();

    kwargs->RegisterKwarg("PmaChannel.Create", "gpu", false, 
        "GPU to which this SelwreProfiler belongs");
    kwargs->RegisterKwarg("PmaChannel.Create", "profiler", true, 
        "the SelwreProfiler object to associate the PMA channel with.");
    kwargs->RegisterKwarg("PmaChannel.UpdateGetPtr", "wait", true,  
        "boolean to reprensent if RM will block waiting for the value to be changed.");

    py::class_<UtlPmaChannel> pmaChannel(module, "PmaChannel");
    pmaChannel.def_static("Create", &UtlPmaChannel::CreatePy,
        UTL_DOCSTRING("PmaChannel.Create", "This function creates a PmaChannel object."),
        py::return_value_policy::reference);
    pmaChannel.def("Allocate", &UtlPmaChannel::AllocatePy,
        UTL_DOCSTRING("PmaChannel.Allocate", 
        "This function allocates a PMA channel via B0CC call ALLOC_PMA_STREAM."));
    pmaChannel.def("UpdateGetPtr", &UtlPmaChannel::UpdateGetPtrPy,
        UTL_DOCSTRING("PmaChannel.UpdateGetPtr", 
        "This function updates GET pointer via B0CC call PMA_STREAM_UPDATE_GET_PUT."));
    pmaChannel.def("GetPutPtr", &UtlPmaChannel::GetPutPtrPy,
        UTL_DOCSTRING("PmaChannel.GetPutPtr", 
        "This function returns PUT pointer via B0CC call PMA_STREAM_UPDATE_GET_PUT."));

    /****** PMA Channel property ******/
    pmaChannel.def_property_readonly("channelIndex",
        &UtlPmaChannel::GetChannelIndex, "this property returns the allocated PMA channel index.");
    pmaChannel.def_property("recordBuffer",
        &UtlPmaChannel::GetRecordBuffer, &UtlPmaChannel::SetRecordBuffer,
        "the record buffer associated to this PMA channel.");
    pmaChannel.def_property("memBytesBuffer",
        &UtlPmaChannel::GetMemBytesBuffer, &UtlPmaChannel::SetMemBytesBuffer,
        "the memBytes buffer associated to this PMA channel.");
}


// This constructor should only be called from UtlPmaChannel::CreatePy.
//
UtlPmaChannel::UtlPmaChannel(LwRm::Handle profiler) 
    : m_pRecordBuffer(nullptr),
      m_pMemBytesBuffer(nullptr),
      m_ChannelIndex(0xFFFFFFFF)
{ 
    m_ProfilerObject = profiler;
}

UtlPmaChannel::~UtlPmaChannel()
{
    m_ProfilerObject = 0;
    m_pRecordBuffer = nullptr;
    m_pMemBytesBuffer = nullptr;
    m_ChannelIndex = 0xFFFFFFFF;
}

// This function can be called from a UTL script to create a PMA channel.
//
UtlPmaChannel* UtlPmaChannel::CreatePy(py::kwargs kwargs)
{
    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu", "PmaChannel.Create"))
    {
        // If the user doesn't specify a GPU, use GPU 0 by default.
        gpu = UtlGpu::GetGpus()[0];
    }
        
    if ((s_PmaChannels.count(gpu) > 0) &&
        (s_PmaChannels[gpu].size() == 
        gpu->GetGpuSubdevice()->Regs().LookupAddress(MODS_PERF_PMASYS_CHANNEL_CONTROL_SELWRE__SIZE_1)))
    {
        UtlUtility::PyError("PmaChannel.Create: PMA channel size reaches maximum value %d for device 0x%x subdevice 0x%x.", 
            s_PmaChannels[gpu].size(), gpu->GetGpudevice(), gpu->GetGpuSubdevice());
    }
    
    UtlSelwreProfiler* utlSelwreProfiler = UtlUtility::GetRequiredKwarg<UtlSelwreProfiler*>(kwargs,
        "profiler", "PmaChannel.Create");
    if (!utlSelwreProfiler)
    {
        UtlUtility::PyError("PmaChannel.Create: invalid profiler object.");
    }

    unique_ptr<UtlPmaChannel> utlPmaChannel(new UtlPmaChannel(utlSelwreProfiler->GetHandle()));
    UtlPmaChannel* returnPtr = utlPmaChannel.get();
    s_PmaChannels[gpu].push_back(move(utlPmaChannel));
    return returnPtr;
}

void UtlPmaChannel::FreePmaChannels()
{
    s_PmaChannels.clear(); 
}

void UtlPmaChannel::AllocatePy()
{
    RC rc;

    if (!m_pRecordBuffer || !m_pMemBytesBuffer)
    {
        UtlUtility::PyError("PmaChannel.Allocate: recordBuffer or memBytesBuffer is not set.");
    }

    MASSERT(m_ProfilerObject);

    UtlGil::Release gil;

    LwRmPtr pLwRm;
    LWB0CC_CTRL_ALLOC_PMA_STREAM_PARAMS pmaStreamParams = {};
    pmaStreamParams.hMemPmaBuffer = m_pRecordBuffer->GetMemHandle();
    pmaStreamParams.pmaBufferOffset = 0;
    pmaStreamParams.pmaBufferSize = m_pRecordBuffer->GetSize();
    pmaStreamParams.hMemPmaBytesAvailable = m_pMemBytesBuffer->GetMemHandle();
    pmaStreamParams.pmaBytesAvailableOffset = 0;
    pmaStreamParams.ctxsw = false;
    rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_ALLOC_PMA_STREAM, 
        &pmaStreamParams, sizeof(pmaStreamParams));
    UtlUtility::PyErrorRC(rc, "PmaChannel.Allocate: failed to allocate PMA stream.");
    m_ChannelIndex = pmaStreamParams.pmaChannelIdx;
}

UINT32 UtlPmaChannel::GetChannelIndex() const
{
    return m_ChannelIndex;
}

UINT64 UtlPmaChannel::UpdateGetPtrPy(py::kwargs kwargs)
{
    RC rc;
    LwRmPtr pLwRm;

    MASSERT(m_ProfilerObject);

    bool wait = UtlUtility::GetRequiredKwarg<bool>(kwargs,
        "wait", "PmaChannel.UpdateGetPtr");

    UtlGil::Release gil;

    LWB0CC_CTRL_PMA_STREAM_UPDATE_GET_PUT_PARAMS updateGetPutParams = {};
    updateGetPutParams.bUpdateAvailableBytes = true;
    updateGetPutParams.bWait = wait;
    updateGetPutParams.pmaChannelIdx = m_ChannelIndex;

    rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_PMA_STREAM_UPDATE_GET_PUT, 
        &updateGetPutParams, sizeof(updateGetPutParams));
    UtlUtility::PyErrorRC(rc, "PmaChannel.UpdateGetPtr: failed to update GET pointer.");

    return wait ? updateGetPutParams.bytesAvailable : 0; 
}

UINT64 UtlPmaChannel::GetPutPtrPy()
{
    RC rc;
    LwRmPtr pLwRm;

    MASSERT(m_ProfilerObject);

    UtlGil::Release gil;

    LWB0CC_CTRL_PMA_STREAM_UPDATE_GET_PUT_PARAMS updateGetPutParams = {};
    updateGetPutParams.bUpdateAvailableBytes = false;
    updateGetPutParams.bWait = false;
    updateGetPutParams.bReturnPut = true;
    updateGetPutParams.pmaChannelIdx = m_ChannelIndex;

    rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_PMA_STREAM_UPDATE_GET_PUT, 
        &updateGetPutParams, sizeof(updateGetPutParams));
    UtlUtility::PyErrorRC(rc, "PmaChannel.GetPutPtr: failed to get PUT pointer.");

    return updateGetPutParams.putPtr; 
}

void UtlPmaChannel::SetRecordBuffer(UtlSurface* pBuffer)
{
    m_pRecordBuffer = pBuffer;
}

UtlSurface* UtlPmaChannel::GetRecordBuffer() const
{
    return m_pRecordBuffer;
}

void UtlPmaChannel::SetMemBytesBuffer(UtlSurface* pBuffer)
{
    m_pMemBytesBuffer = pBuffer;
}

UtlSurface* UtlPmaChannel::GetMemBytesBuffer() const
{
    return m_pMemBytesBuffer;
}

map<UtlGpu*, unique_ptr<UtlSelwreProfiler>> UtlSelwreProfiler::s_SelwreProfilers;

void UtlSelwreProfiler::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();

    kwargs->RegisterKwarg("SelwreProfiler.Create", "gpu", false, 
        "GPU to which this SelwreProfiler belongs");
    kwargs->RegisterKwarg("SelwreProfiler.Reserve", "type", true, 
        "An enum that represents the HWPM resource type to be reserved");
    kwargs->RegisterKwarg("SelwreProfiler.Release", "type", true, 
        "An enum that represents the HWPM resource type to be released");

    py::class_<UtlSelwreProfiler> selwreProfiler(module, "SelwreProfiler");
    selwreProfiler.def_static("Create", &UtlSelwreProfiler::CreatePy,
        UTL_DOCSTRING("SelwreProfiler.Create", 
        "This function creates the global SelwreProfiler object."),
        py::return_value_policy::reference);
    selwreProfiler.def("Reserve", &UtlSelwreProfiler::ReservePy,
        UTL_DOCSTRING("SelwreProfiler.Reserve", 
        "This function reserves HWPM resource before PMA channel allocation via B0CC."));
    selwreProfiler.def("Release", &UtlSelwreProfiler::ReleasePy,
        UTL_DOCSTRING("SelwreProfiler.Release", 
        "This function releases HWPM resource before PMA channel allocation via B0CC."));
    selwreProfiler.def("Bind", &UtlSelwreProfiler::BindPy,
        UTL_DOCSTRING("SelwreProfiler.Bind", 
        "This function binds resources for all PMA channels via B0CC call BIND_PM_RESOURCES."));
    selwreProfiler.def("Unbind", &UtlSelwreProfiler::UnbindPy,
        UTL_DOCSTRING("SelwreProfiler.Unbind", 
        "This function unbinds resources for all PMA channels via B0CC call UNBIND_PM_RESOURCES."));

    py::enum_<ReservationType>(selwreProfiler, "Reservation")
        .value("HWPM_LEGACY", HWPM_LEGACY)
        .value("PM_AREA_SMPC", PM_AREA_SMPC);
}

// This constructor should only be called from UtlSelwreProfiler::CreatePy.
//
UtlSelwreProfiler::UtlSelwreProfiler(GpuSubdevice* pSubdev)
    : m_ProfilerObject(0)
{
    LwRmPtr pLwRm;
    // allocate profiler object
    LWB2CC_ALLOC_PARAMETERS allocParams = {};
    RC rc = pLwRm->Alloc(pLwRm->GetSubdeviceHandle(pSubdev), 
        &m_ProfilerObject, MAXWELL_PROFILER_DEVICE, &allocParams);
    UtlUtility::PyErrorRC(rc, "SelwreProfiler.Create: failed to allocate profiler object.");
}

UtlSelwreProfiler::~UtlSelwreProfiler()
{
    MASSERT(m_ProfilerObject);
    LwRmPtr pLwRm;
    pLwRm->Free(m_ProfilerObject);
}

// This function can be called from a UTL script to create a secure profiler.
// Lwrrently there is only one global profiler object per GPU.
//
UtlSelwreProfiler* UtlSelwreProfiler::CreatePy(py::kwargs kwargs)
{
    RC rc;

    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu", "SelwreProfiler.Create"))
    {
        // If the user doesn't specify a GPU, use GPU 0 by default.
        gpu = UtlGpu::GetGpus()[0];
    }
    
    if (s_SelwreProfilers.count(gpu) == 0)
    {
        UtlGil::Release gil;

        rc = Registry::Write("ResourceManager", "RmProfilerFeature",
            DRF_DEF(_REG, _STR_RM, _PROFILING_CHECK_BYPASS_ALLOWLIST, _TRUE) | 
            DRF_DEF(_REG, _STR_RM, _PROFILING_CHECK_BYPASS_RESERVATION, _TRUE));
        UtlUtility::PyErrorRC(rc, "SelwreProfiler.Create: failed to write RM regkey RMProfilerFeature.");

        unique_ptr<UtlSelwreProfiler> utlSelwreProfiler(new UtlSelwreProfiler(gpu->GetGpuSubdevice()));
        s_SelwreProfilers[gpu] = move(utlSelwreProfiler);
    }

    return s_SelwreProfilers[gpu].get();
}

void UtlSelwreProfiler::FreeProfilers()
{
    s_SelwreProfilers.clear();
}

LwRm::Handle UtlSelwreProfiler::GetHandle()
{
    return m_ProfilerObject;
}

void UtlSelwreProfiler::ReservePy(py::kwargs kwargs)
{
    RC rc;
    LwRmPtr pLwRm;

    MASSERT(m_ProfilerObject);
    
    ReservationType type = UtlUtility::GetRequiredKwarg<ReservationType>(kwargs,
        "type", "SelwreProfiler.Reserve");

    if (m_ReserveState[type])
    {
        UtlUtility::PyError("SelwreProfiler.Reserve: the requested resource %d is already reserved, call Release first.", type);
    }
    else
    {
        UtlGil::Release gil;

        if (type == HWPM_LEGACY)
        {
            LWB0CC_CTRL_RESERVE_HWPM_LEGACY_PARAMS reserveParams = {};
            rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_RESERVE_HWPM_LEGACY, 
                &reserveParams, sizeof(reserveParams));
            UtlUtility::PyErrorRC(rc, "SelwreProfiler.Reserve: failed to reserve HWPM legacy.");
        }
        else if (type == PM_AREA_SMPC)
        {
            LWB0CC_CTRL_RESERVE_PM_AREA_SMPC_PARAMS reserveParams = {};
            rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_RESERVE_PM_AREA_SMPC, 
                &reserveParams, sizeof(reserveParams));
            UtlUtility::PyErrorRC(rc, "SelwreProfiler.Reserve: failed to reserve SMPC.");
        }
        if (rc == OK)
        {
            m_ReserveState[type] = true;
        }
    }
}

void UtlSelwreProfiler::ReleasePy(py::kwargs kwargs)
{
    RC rc;
    LwRmPtr pLwRm;

    MASSERT(m_ProfilerObject);
    
    ReservationType type = UtlUtility::GetRequiredKwarg<ReservationType>(kwargs,
        "type", "SelwreProfiler.Release");

    if (!m_ReserveState[type])
    {
        UtlUtility::PyError("SelwreProfiler.Release: the requested resource %d to release is not reserved yet.", type);
    }
    else
    {
        UtlGil::Release gil;

        if (type == HWPM_LEGACY)
        {
            rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_RELEASE_HWPM_LEGACY, nullptr, 0);
            UtlUtility::PyErrorRC(rc, "SelwreProfiler.Release: failed to release HWPM legacy.");
        }
        else if (type == PM_AREA_SMPC)
        {
            rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_RELEASE_PM_AREA_SMPC, nullptr, 0);
            UtlUtility::PyErrorRC(rc, "SelwreProfiler.Release: failed to release SMPC.");
        }
        if (rc == OK)
        {
            m_ReserveState[type] = false;
        }
    }
}

void UtlSelwreProfiler::BindPy()
{
    RC rc;
    LwRmPtr pLwRm;

    MASSERT(m_ProfilerObject);

    if (m_BindState)
    {
        UtlUtility::PyError("SelwreProfiler.Bind: the profiler is already bound, call Unbind first.");
    }
    else
    {
        UtlGil::Release gil;

        rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_BIND_PM_RESOURCES, nullptr, 0);
        if (rc == OK)
        {
            m_BindState = true;
        }
        else
        {
            UtlUtility::PyErrorRC(rc, "SelwreProfiler.Bind: failed to bind PM resources.");
        }
    }
}

void UtlSelwreProfiler::UnbindPy()
{
    RC rc;
    LwRmPtr pLwRm;

    MASSERT(m_ProfilerObject);

    if (!m_BindState)
    {
        UtlUtility::PyError("SelwreProfiler.Unbind: the profiler is not bound.");
    }
    else
    {
        UtlGil::Release gil;

        rc = pLwRm->Control(m_ProfilerObject, LWB0CC_CTRL_CMD_UNBIND_PM_RESOURCES, nullptr, 0);
        if (rc == OK)
        {
            m_BindState = false;
        }
        else
        {
            UtlUtility::PyErrorRC(rc, "SelwreProfiler.Unbind: failed to unbind PM resources.");
        }
    }
}

