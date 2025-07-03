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

#pragma once

#include <map>
#include <vector>

#include "core/include/lwrm.h"
#include "core/include/rc.h"
#include "gpu/include/gpudev.h"

//! Handles created on default RMClient
struct GpuDeviceHandles
{
    map<LwRm*, LwRm::Handle> hFermiDeviceVASpace; //!< LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE
    map<LwRm::Handle, LwRm::Handle> hFermiHostVASpaces; //!< LW_VASPACE_ALLOCATION_INDEX_GPU_HOST
};

struct VASpaceInfo
{
    LwRm::Handle hVASpace;
    LwRm::Handle hSubDevice;

    //!< Save them here to avoid unnecessary RM query
    const struct GMMU_FMT *pGmmuFmt;
    UINT64 PdbPhysAddr;
    UINT32 Aperture;
};


class VaSpaceUtils
{
public:

	static GpuDeviceHandles s_GpuDevHandles;
	static vector<VASpaceInfo> s_VASpaceInfo;

	static RC GetFermiVASpaceHandle
    (
        UINT32 index,
        LwRm::Handle hGpu,
        LwRm::Handle *handle,
        LwRm* pLwRm
    );

    static RC SetVASpaceInfo(LwRm::Handle hVASpace, LwRm::Handle hSubDevice,
                      UINT64 physAddr, UINT32 aperture,
                      const struct GMMU_FMT *pGmmuFmt);

    static RC GetVASpacePdbAddress(LwRm::Handle hVASpace, LwRm::Handle hSubDevice,
                            UINT64 *pPhysAddr, UINT32 *pAperture, LwRm* pLwRm, GpuDevice* pGpuDevice);

    static RC GetVASpaceGmmuFmt(LwRm::Handle hVASpace, LwRm::Handle hSubDevice,
                         const struct GMMU_FMT **ppGmmuFmt, LwRm* pLwRm);
};
