/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_ibmnpu_libif_user.h"
#include "lwl_devif.h"
#include "lwlink.h"
#include "ibmnpu.h"
#include "ibmnpu_export.h"
#include "core/include/tee.h"
#include "lwgputypes.h"
#include "core/include/jscript.h"
#include <cstring>

namespace LwLinkDevIf
{
    const UINT32 IBMNPU_HANDLE       = 0x1b000000;
    const UINT32 ALL_NPU_DOMAINS     = ~0U;
    const UINT32 ALL_NPU_LINKS       = ~0U;

    RC GetIbmNpuDeviceHandle(LibInterface::LibDeviceHandle handle, lwlink_device_handle *pDev)
    {
        UINT32 index = handle & ~LibInterface::HANDLE_TYPE_MASK;
        IBMNPU_CTRL_PARAMS params;
        CTRL_LWLINK_IBMNPU_GET_DEVICE_HANDLE_PARAMS getHandleParams;

        getHandleParams.index = index;
        params.cmd            = CTRL_LWLINK_IBMNPU_GET_DEVICE_HANDLE;
        params.handle         = pDev;
        params.params         = &getHandleParams;
        params.params_size    = sizeof(getHandleParams);
        return LibInterface::LwLinkLibRetvalToModsRc(ibmnpu_lib_ctrl(&params));
    }
};

//------------------------------------------------------------------------------
//! \brief Create an the instance of the ibm npu library interface
//!
//! \return Pointer to the ibm npu library interface
//!
/* static */ LwLinkDevIf::LibIfPtr LwLinkDevIf::LibInterface::CreateIbmNpuLibIf()
{
    return make_shared<LwLinkDevIf::IbmNpuLibIfUser>();
}

//------------------------------------------------------------------------------
//! \brief Destructor, shutdown the library if initialized
//!
LwLinkDevIf::IbmNpuLibIfUser::~IbmNpuLibIfUser()
{
    if (m_bInitialized)
    {
        Shutdown();
    }
}

//------------------------------------------------------------------------------
//! \brief Call the NPU control interface
//!
//! \param handle    : Device handle to apply the control to
//! \param controlId : Control call to make
//! \param pParams   : Control parameters
//! \param paramSize : Size of the control parameters
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwLinkDevIf::IbmNpuLibIfUser::Control
(
    LibDeviceHandle  handle,
    LibControlId     controlId,
    void *           pParams,
    UINT32           paramSize
)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    UINT32 libControlId;

    switch (controlId)
    {
        case CONTROL_GET_LINK_CAPS:
            libControlId = CTRL_LWLINK_IBMNPU_GET_LWLINK_CAPS;
            break;
        case CONTROL_GET_LINK_STATUS:
            libControlId = CTRL_LWLINK_IBMNPU_GET_LWLINK_STATUS;
            break;
        case CONTROL_CLEAR_COUNTERS:
            libControlId = CTRL_LWLINK_IBMNPU_CLEAR_COUNTERS;
            break;
        case CONTROL_GET_COUNTERS:
            libControlId = CTRL_LWLINK_IBMNPU_GET_COUNTERS;
            break;
        case CONTROL_GET_ERR_INFO:
            libControlId = CTRL_LWLINK_IBMNPU_GET_ERR_INFO;
            break;
        case CONTROL_INJECT_ERR:
            libControlId = CTRL_LWLINK_IBMNPU_INJECT_ERR;
            break;
        default:
            Printf(Tee::PriHigh, "%s : Unknown control Id %d\n", __FUNCTION__, controlId);
            return RC::SOFTWARE_ERROR;
    }

    RC rc;
    lwlink_device_handle dev;
    CHECK_RC(GetIbmNpuDeviceHandle(handle, &dev));

    IBMNPU_CTRL_PARAMS params;
    params.cmd            = libControlId;
    params.handle         = &dev;
    params.params         = pParams;
    params.params_size    = paramSize;

    LwlStatus status = ibmnpu_lib_ctrl(&params);
    rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
    Printf(Tee::PriDebug, GetTeeModule()->GetCode(),
            "%s : IbmNpu library control %u returns %d!!\n",
           __FUNCTION__, controlId, rc.Get());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Find all NPU devices
//!
//! \param pDevHandles :  Pointer to list of device handles of the devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::IbmNpuLibIfUser::FindDevices(vector<LibDeviceHandle> *pDevHandles)
{
    if (!m_bInitialized)
    {
        Printf(Tee::PriHigh, GetTeeModule()->GetCode(),
                "%s : IbmNpu library not initialized!!\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(), "Find ibmnpu ports\n");

    pDevHandles->clear();

    UINT32 index = 0;
    lwlink_device_handle dev;
    RC rc = GetIbmNpuDeviceHandle(index, &dev);
    while (rc == OK)
    {
        LibDeviceHandle newHandle = IBMNPU_HANDLE | index;
        pDevHandles->push_back(newHandle);
        rc = GetIbmNpuDeviceHandle(++index, &dev);
    }

    if (rc != RC::LWRM_NOT_FOUND)
    {
        Printf(Tee::PriHigh, GetTeeModule()->GetCode(),
               "%s : IbmNpu device discovery failed!!\n", __FUNCTION__);
        return rc;
    }
    else
    {
        rc.Clear();
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
            "-------Finish ibmnpu detection-----\n");
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the BAR info for the specified device
//!
//! \param handle     : Device handle to get BAR info for
//! \param barNum     : The BAR number to get the BAR info for
//! \param pBarAddr   : Pointer to returned bar physical address
//! \param pBarSize   : Pointer to returned bar size
//! \param ppBar0     : Pointer to returned bar CPU virtural pointer
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::IbmNpuLibIfUser::GetBarInfo
(
    LibDeviceHandle handle,
    UINT32          barNum,
    UINT64         *pBarAddr,
    UINT64         *pBarSize,
    void          **ppBar0
)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    if (barNum >= IBMNPU_MAX_BARS)
    {
        Printf(Tee::PriHigh, GetTeeModule()->GetCode(),
               "%s : Invalid BAR number %u\n",
               __FUNCTION__, barNum);
        return RC::BAD_PARAMETER;
    }

    RC rc;
    lwlink_device_handle dev;
    CHECK_RC(GetIbmNpuDeviceHandle(handle, &dev));

    if (pBarAddr != nullptr)
        *pBarAddr = dev.pciInfo.bars[barNum].baseAddr;
    if (pBarSize != nullptr)
        *pBarSize = dev.pciInfo.bars[barNum].barSize;
    if (ppBar0 != nullptr)
        *ppBar0 = dev.pciInfo.bars[barNum].pBar;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the link mask for the specified device
//!
//! \param handle     : Device handle to get link mask for
//! \param pLinkMask  : Pointer to returned link mask
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::IbmNpuLibIfUser::GetLinkMask(LibDeviceHandle handle, UINT64 *pLinkMask)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    RC rc;
    lwlink_device_handle dev;
    CHECK_RC(GetIbmNpuDeviceHandle(handle, &dev));
    *pLinkMask = dev.linkMask;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the PCI info for the specified device
//!
//! \param handle     : Device handle to get PCI info for
//! \param pDomain    : Pointer to returned PCI domain
//! \param pBus       : Pointer to returned PCI bus
//! \param pDev       : Pointer to returned PCI device
//! \param pFunc      : Pointer to returned PCI function
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::IbmNpuLibIfUser::GetPciInfo
(
    LibDeviceHandle handle,
    UINT32         *pDomain,
    UINT32         *pBus,
    UINT32         *pDev,
    UINT32         *pFunc
)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    RC rc;
    lwlink_device_handle dev;
    CHECK_RC(GetIbmNpuDeviceHandle(handle, &dev));

    if (pDomain != nullptr)
        *pDomain = dev.pciInfo.domain;
    if (pBus != nullptr)
        *pBus    = dev.pciInfo.bus;
    if (pDev != nullptr)
        *pDev    = dev.pciInfo.device;
    if (pFunc != nullptr)
        *pFunc   = dev.pciInfo.function;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize the library interface
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::IbmNpuLibIfUser::Initialize()
{
    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "Ibmnpu library already initialized\n");
        return OK;
    }

    JavaScriptPtr pJs;
    JSObject * pGlobObj = JavaScriptPtr()->GetGlobalObject();
    UINT32 npuDomain = ALL_NPU_DOMAINS;
    UINT32 npuLinkMask = ALL_NPU_LINKS;
    if (OK != pJs->GetProperty(pGlobObj, "g_NpuDomain", &npuDomain))
    {
        Printf(Tee::PriLow,
               "%s : Failed to get g_NpuDomain, finding NPU devices in all domains\n",
               __FUNCTION__);
        npuDomain = ALL_NPU_DOMAINS;
    }
    if (OK != pJs->GetProperty(pGlobObj, "g_NpuLinkMask", &npuLinkMask))
    {
        Printf(Tee::PriLow,
               "%s : Failed to get g_NpuLinkMask, finding all links\n",
               __FUNCTION__);
        npuLinkMask = ALL_NPU_LINKS;
    }

    LwlStatus status = ibmnpu_lib_load(npuDomain, npuLinkMask);
    const RC rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
    m_bInitialized = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the library interface
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::IbmNpuLibIfUser::Shutdown()
{
    StickyRC rc;

    if (!m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "Ibmnpu library not initialized, skipping shutdown\n");
        return OK;
    }

    LwlStatus status = ibmnpu_lib_unload();
    rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
    m_bInitialized = false;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Initialize a particular NPU device
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::IbmNpuLibIfUser::InitializeDevice(LibDeviceHandle handle)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    RC rc;

    if (!m_bInitialized)
    {
        Printf(Tee::PriHigh, GetTeeModule()->GetCode(),
                "%s : Ibmnpu library not initialized\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    lwlink_device_handle dev;
    CHECK_RC(GetIbmNpuDeviceHandle(handle, &dev));

    lwlink_device* lwlinkDev = ibmnpu_lib_get_device_by_handle(&dev);
    MASSERT(lwlinkDev);
    DEV_INFO* devInfo = static_cast<DEV_INFO*>(lwlinkDev->pDevInfo);
    MASSERT(devInfo);

    LwlStatus status = ibmnpu_lib_initialize_device(devInfo->handle);
    CHECK_RC(LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status));
    return rc;
}

