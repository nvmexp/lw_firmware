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

#include "mdiag/utils/vaspaceutils.h"

#include "class/cl90f1.h"       // FERMI_VASPACE_A
#include "ctrl/ctrl90f1.h"      // LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS

GpuDeviceHandles VaSpaceUtils::s_GpuDevHandles;
vector<VASpaceInfo> VaSpaceUtils::s_VASpaceInfo;

//--------------------------------------------------------------------
//! \brief Get GPU vaspace handler
//!
//! \param index : LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE
//!                     Gpu default vaspace created by RM
//!                LW_VASPACE_ALLOCATION_INDEX_GPU_HOST
//!                     Gpu Bar1 vaspace created by RM
//! \param hGpu: Device handle need for GPU_DEVICE or Subdevice handle neeeded for GPU_HOST
//! \param handle: Returned value
RC VaSpaceUtils::GetFermiVASpaceHandle
(
    UINT32 index,
    LwRm::Handle hGpu,
    LwRm::Handle *handle,
    LwRm* pLwRm
)
{
    RC rc;

    if (index == LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE)
    {
        LwRm::Handle hDevice = hGpu;
        if (s_GpuDevHandles.hFermiDeviceVASpace.find(pLwRm) ==
                s_GpuDevHandles.hFermiDeviceVASpace.end())
        {
            LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
            vaParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE;
            LwRm::Handle hFermiDeviceVASpace = 0;

            CHECK_RC(pLwRm->Alloc(hDevice,
                &hFermiDeviceVASpace,
                FERMI_VASPACE_A,
                &vaParams));
            s_GpuDevHandles.hFermiDeviceVASpace[pLwRm] = hFermiDeviceVASpace;
        }

        *handle = s_GpuDevHandles.hFermiDeviceVASpace[pLwRm];
    }
    else if (index == LW_VASPACE_ALLOCATION_INDEX_GPU_HOST)
    {
        LwRm::Handle hSubDevice = hGpu;
        if (s_GpuDevHandles.hFermiHostVASpaces.find(hSubDevice) ==
                s_GpuDevHandles.hFermiHostVASpaces.end())
        {
            LwRmPtr pLwRm;
            LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
            vaParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_HOST;
            LwRm::Handle hFermiHostVASpace = 0;
            CHECK_RC(pLwRm->Alloc(hSubDevice,
                &hFermiHostVASpace,
                FERMI_VASPACE_A,
                &vaParams));
            s_GpuDevHandles.hFermiHostVASpaces[hSubDevice] = hFermiHostVASpace;
        }
        *handle = s_GpuDevHandles.hFermiHostVASpaces[hSubDevice];
    }
    else
    {
        *handle = 0;
        rc = RC::ILWALID_ARGUMENT;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set PA and Aperture of PDB for given hVASpace and hSubDevice
//!        Save vaspace infomation to avoid duplicated RM query
RC VaSpaceUtils::SetVASpaceInfo
(
    LwRm::Handle hVASpace,
    LwRm::Handle hSubDevice,
    UINT64 physAddr,
    UINT32 aperture,
    const struct GMMU_FMT *pGmmuFmt
)
{
    RC rc;
    vector<VASpaceInfo>::iterator it = s_VASpaceInfo.begin();
    for(; it != s_VASpaceInfo.end(); ++it)
    {
        if ((it->hVASpace == hVASpace) && (it->hSubDevice == hSubDevice))
        {
            MASSERT(it->PdbPhysAddr == physAddr);
            MASSERT(it->Aperture == aperture);
            MASSERT(it->pGmmuFmt == pGmmuFmt);

            return rc;
        }
    }

    VASpaceInfo vaspaceInfo;
    memset(&vaspaceInfo, 0, sizeof(vaspaceInfo));

    vaspaceInfo.hVASpace = hVASpace;
    vaspaceInfo.hSubDevice = hSubDevice;
    vaspaceInfo.PdbPhysAddr = physAddr;
    vaspaceInfo.Aperture = aperture;
    vaspaceInfo.pGmmuFmt = pGmmuFmt;

    s_VASpaceInfo.push_back(vaspaceInfo);

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get PA and Aperture of PDB for a given hVASpace and hSubDevice
//!  Assumption: PDB is unchanged for a given vaspace;
//!  Get GmmuFmt from save database to avoid unnecessary dumplicated RM query
RC VaSpaceUtils::GetVASpacePdbAddress
(
    LwRm::Handle hVASpace,
    LwRm::Handle hSubDevice,
    UINT64 *pPhysAddr,
    UINT32 *pAperture,
    LwRm* pLwRm,
    GpuDevice* pGpuDevice
)
{
    RC rc;
    MASSERT(pPhysAddr);
    MASSERT(pAperture);
    MASSERT(pLwRm);

    vector<VASpaceInfo>::iterator it = s_VASpaceInfo.begin();
    for(; it != s_VASpaceInfo.end(); ++it)
    {
        if ((it->hVASpace == hVASpace) && (it->hSubDevice == hSubDevice))
        {
            *pPhysAddr = it->PdbPhysAddr;
            *pAperture = it->Aperture;
            return rc;
        }
    }

    // Not found
    // Call RM API to get the it; Save will not happen
    // until client calls SetVASpaceInfo()
    UINT64 pageSize = 0;
    if (pGpuDevice == nullptr)
    {
        return RC::SOFTWARE_ERROR;
    }
    pageSize = pGpuDevice->GetBigPageSize();

    LW90F1_CTRL_VASPACE_GET_PAGE_LEVEL_INFO_PARAMS mmuLevelInfo = { 0 };
    mmuLevelInfo.hSubDevice = hSubDevice;
    mmuLevelInfo.pageSize = pageSize;
    CHECK_RC(pLwRm->Control(hVASpace,
        LW90F1_CTRL_CMD_VASPACE_GET_PAGE_LEVEL_INFO,
        &mmuLevelInfo,
        sizeof(mmuLevelInfo)));

    MASSERT(mmuLevelInfo.numLevels >= 0);
    *pPhysAddr =  mmuLevelInfo.levels[0].physAddress;
    *pAperture = mmuLevelInfo.levels[0].aperture;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get GmmuFmt for given hVASpace and hSubDevice
//!  Assumption: GmmuFmt is unchanged
//!  Get GmmuFmt from save database to avoid unnecessary dumplicated RM query
RC VaSpaceUtils::GetVASpaceGmmuFmt
(
    LwRm::Handle hVASpace,
    LwRm::Handle hSubDevice,
    const struct GMMU_FMT **ppGmmuFmt,
    LwRm* pLwRm
)
{
    RC rc;

    MASSERT(ppGmmuFmt);
    MASSERT(pLwRm);

    vector<VASpaceInfo>::iterator it = s_VASpaceInfo.begin();
    for(; it != s_VASpaceInfo.end(); ++it)
    {
        if ((it->hVASpace == hVASpace) && (it->hSubDevice == hSubDevice))
        {
            *ppGmmuFmt = it->pGmmuFmt;
            return rc;
        }
    }

    // Not found
    // Call RM API to get the it; Save will not happen
    // until client calls SetVASpaceInfo()
    LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS gmmuFmtParams = {0};
    gmmuFmtParams.hSubDevice = hSubDevice;
    CHECK_RC(pLwRm->Control(hVASpace,
             LW90F1_CTRL_CMD_VASPACE_GET_GMMU_FORMAT,
             &gmmuFmtParams,
             sizeof(gmmuFmtParams)));

    *ppGmmuFmt = (const GMMU_FMT *) LwP64_VALUE(gmmuFmtParams.pFmt);

    return rc;
}
