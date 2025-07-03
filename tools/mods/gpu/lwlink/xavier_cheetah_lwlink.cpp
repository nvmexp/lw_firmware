/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xavier_tegra_lwlink.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"

#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

//------------------------------------------------------------------------------
bool XavierTegraLwLink::DoIsSupported(LibDevHandles handles) const
{
    // CheetAh is slightly different than a dGPU, if lwlink is possible but does not
    // exist (because the /dev node is missing for instance) then it is not even
    // possible to get the capabilities or even initialize the Lwlink implementation
    // enough to query support.  Therefore check for the presense of an lwlink
    // device in the GPU first, which will at least indicate the potential for
    // lwlink connectivity exists and prevents querying for a LwLink library
    // interface when one cannot exist
    if (GetLibHandle() == Device::ILWALID_LIB_HANDLE && handles.size() == 0)
        return false;

    return XavierLwLink::DoIsSupported(handles);
}

//------------------------------------------------------------------------------
LwLinkDevIf::LibIfPtr XavierTegraLwLink::GetTegraLwLinkLibIf()
{
    // XavierTegraLwLink is special because while its associated TestDevice is a GpuSubdevice,
    // GpuSubdevice can only access registers that are within the GPU partition on CheetAh.  In
    // order to access actual lwlink registers, it is necessary to get the TestDevice that is
    // within the LwLinkDev tree hierarchy
    return LwLinkDevIf::GetTegraLibIf();
}

//------------------------------------------------------------------------------
RC XavierTegraLwLink::GetRegPtr(void **ppRegs)
{
    if (CheetAh::IsInitialized())
    {
        return CheetAh::SocPtr()->GetAperture("LWLINK_CFG", ppRegs);
    }
    *ppRegs = nullptr;
    return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
}
