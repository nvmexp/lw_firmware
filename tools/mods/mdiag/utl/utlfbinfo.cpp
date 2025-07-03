/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utlfbinfo.h"
#include "utlutil.h"
#include "utlgpupartition.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/lwgpu.h"

void UtlFbInfo::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("FbInfo.GetPhysicalFbpaMask", "fbpIndex", true, "FB Partition index");
    kwargs->RegisterKwarg("FbInfo.GetPhysicalLtcMask", "fbpIndex", true, "FB Partition index");
    kwargs->RegisterKwarg("FbInfo.GetPhysicalLtsMask", "fbpIndex", true, "FB Partition index");

    py::class_<UtlFbInfo> fbInfo(module, "FbInfo",
        "Interface for retrieving FB info.");
    fbInfo.def_property_readonly("startAddress", 
        [](UtlFbInfo& info) { return info.GetData(FbInfoType::StartAddress); });
    fbInfo.def_property_readonly("size", 
        [](UtlFbInfo& info) { return info.GetData(FbInfoType::Size); });
    fbInfo.def("GetPhysicalFbpMask", &UtlFbInfo::GetPhysicalFbpMask,
        UTL_DOCSTRING("FbInfo.GetPhysicalFbpMask", "This function returns the physical FBP mask of the GPU or the memory partition."));
    fbInfo.def("GetPhysicalFbpaMask", &UtlFbInfo::GetPhysicalFbpaMask,
        UTL_DOCSTRING("FbInfo.GetPhysicalFbpaMask", "This function returns the physical FBPA mask of the GPU or the memory partition for the specified FBP index."));
    fbInfo.def("GetPhysicalLtcMask", &UtlFbInfo::GetPhysicalLtcMask,
        UTL_DOCSTRING("FbInfo.GetPhysicalLtcMask", "This function returns the physical LTC mask of the GPU or the memory partition for the specified FBP index."));
    fbInfo.def("GetPhysicalLtsMask", &UtlFbInfo::GetPhysicalLtsMask,
        UTL_DOCSTRING("FbInfo.GetPhysicalLtsMask", "This function returns the physical LTS mask of the GPU or the memory partition for the specified FBP index."));
}

UtlFbInfo::UtlFbInfo
(
    GpuSubdevice* pGpuSubdev, 
    LwRm* pLwRm, 
    UtlGpuPartition* pUtlGpuPartition
) :
    m_pGpuSubdev(pGpuSubdev),
    m_pLwRm(pLwRm),
    m_pUtlGpuPartition(pUtlGpuPartition)
{
}

UINT64 UtlFbInfo::GetData(FbInfoType fbInfoType)
{
    UtlGil::Release gil;

    LW2080_CTRL_FB_INFO info = {0};
    info.index = static_cast<UINT32>(fbInfoType);
    LW2080_CTRL_FB_GET_INFO_PARAMS params = { 1, LW_PTR_TO_LwP64(&info) };
    RC rc = m_pLwRm->ControlBySubdevice(m_pGpuSubdev, LW2080_CTRL_CMD_FB_GET_INFO,
            &params, sizeof(params));

    UtlUtility::PyErrorRC(rc, "FbInfo GetData failed.");

    return (static_cast<UINT64>(info.data) << 10);
}

RC UtlFbInfo::GetPhysicalFbMasksRMCtrlCall(LW2080_CTRL_FB_GET_FS_INFO_PARAMS* params)
{
    LwRm* pLwRm = m_pLwRm;
    if (m_pGpuSubdev->IsSMCEnabled())
    {
        auto pLWGpuResource = LWGpuResource::GetGpuByDeviceInstance(m_pGpuSubdev->GetParentDevice()->GetDeviceInst());
        MASSERT(pLWGpuResource);
        auto pSmcResourceController = pLWGpuResource->GetSmcResourceController();
        MASSERT(pSmcResourceController);
        pLwRm = pSmcResourceController->GetProfilingLwRm();
    }

    return pLwRm->ControlBySubdevice(m_pGpuSubdev,
        LW2080_CTRL_CMD_FB_GET_FS_INFO,
        params, sizeof(*params));
}

UINT64 UtlFbInfo::GetPhysicalFbpMask()
{
    UtlGil::Release gil;

    LW2080_CTRL_FB_GET_FS_INFO_PARAMS params = {0};
    params.queries[0].queryType = LW2080_CTRL_FB_FS_INFO_FBP_MASK;
    if (m_pUtlGpuPartition)
    {
        params.queries[0].queryParams.fbp.swizzId = m_pUtlGpuPartition->GetSwizzId();
    }
    params.numQueries = 1;

    RC rc = GetPhysicalFbMasksRMCtrlCall(&params);
    UtlUtility::PyErrorRC(rc, "FbInfo.GetPhysicalFbpMask() failed.");

    return params.queries[0].queryParams.fbp.fbpEnMask;
}

UINT32 UtlFbInfo::GetPhysicalFbpaMask(py::kwargs kwargs)
{
    UINT32 fbpIndex = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs, "fbpIndex", "FbInfo.GetPhysicalFbpaMask");

    UtlGil::Release gil;
   
    LW2080_CTRL_FB_GET_FS_INFO_PARAMS params = {0};
    
    if (m_pUtlGpuPartition)
    {
        params.queries[0].queryType = LW2080_CTRL_FB_FS_INFO_PROFILER_MON_FBPA_MASK;
        params.queries[0].queryParams.dmFbpa.swizzId = m_pUtlGpuPartition->GetSwizzId();
        params.queries[0].queryParams.dmFbpa.fbpIndex = fbpIndex;
    }
    else
    {
        params.queries[0].queryType = LW2080_CTRL_FB_FS_INFO_FBPA_MASK;
        params.queries[0].queryParams.fbpa.fbpIndex = fbpIndex;
    }
    params.numQueries = 1;

    RC rc = GetPhysicalFbMasksRMCtrlCall(&params);
    UtlUtility::PyErrorRC(rc, 
        "FbInfo.GetPhysicalFbpaMask() failed for fbpIndex = %u.", fbpIndex);

    if (m_pUtlGpuPartition)
    {
        return params.queries[0].queryParams.dmFbpa.fbpaEnMask;
    }
    else
    {
        return params.queries[0].queryParams.fbpa.fbpaEnMask;
    }
}
    
UINT32 UtlFbInfo::GetPhysicalLtcMask(py::kwargs kwargs)
{
    UINT32 fbpIndex = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs, "fbpIndex", "FbInfo.GetPhysicalLtcMask");

    UtlGil::Release gil;

    LW2080_CTRL_FB_GET_FS_INFO_PARAMS params = {0};

    if (m_pUtlGpuPartition)
    {
        params.queries[0].queryType = LW2080_CTRL_FB_FS_INFO_PROFILER_MON_LTC_MASK;
        params.queries[0].queryParams.dmLtc.swizzId = m_pUtlGpuPartition->GetSwizzId();
        params.queries[0].queryParams.dmLtc.fbpIndex = fbpIndex;
    }
    else
    {
        params.queries[0].queryType = LW2080_CTRL_FB_FS_INFO_LTC_MASK;
        params.queries[0].queryParams.ltc.fbpIndex = fbpIndex;
    }
    params.numQueries = 1;

    RC rc = GetPhysicalFbMasksRMCtrlCall(&params);
    UtlUtility::PyErrorRC(rc, 
        "FbInfo.GetPhysicalLtcMask() failed for fbpIndex = %u.", fbpIndex);

    if (m_pUtlGpuPartition)
    {
        return params.queries[0].queryParams.dmLtc.ltcEnMask;
    }
    else
    {
        return params.queries[0].queryParams.ltc.ltcEnMask;
    }
}
    
UINT32 UtlFbInfo::GetPhysicalLtsMask(py::kwargs kwargs)
{
    UINT32 fbpIndex = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs, "fbpIndex", "FbInfo.GetPhysicalLtsMask");

    UtlGil::Release gil;

    LW2080_CTRL_FB_GET_FS_INFO_PARAMS params = {0};

    if (m_pUtlGpuPartition)
    {
        params.queries[0].queryType = LW2080_CTRL_FB_FS_INFO_PROFILER_MON_LTS_MASK;
        params.queries[0].queryParams.dmLts.swizzId = m_pUtlGpuPartition->GetSwizzId();
        params.queries[0].queryParams.dmLts.fbpIndex = fbpIndex;
    }
    else
    {
        params.queries[0].queryType = LW2080_CTRL_FB_FS_INFO_LTS_MASK;
        params.queries[0].queryParams.lts.fbpIndex = fbpIndex;
    }
    params.numQueries = 1;

    RC rc = GetPhysicalFbMasksRMCtrlCall(&params);
    UtlUtility::PyErrorRC(rc, 
        "FbInfo.GetPhysicalLtsMask() failed for fbpIndex = %u.", fbpIndex);

    if (m_pUtlGpuPartition)
    {
        return params.queries[0].queryParams.dmLts.ltsEnMask;
    }
    else
    {
        return params.queries[0].queryParams.lts.ltsEnMask;
    }
}
