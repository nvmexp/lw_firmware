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

#include "lwl_lwswitch_libif_kernel.h"
#include "lwl_devif_mgr.h"
#include "lwl_devif.h"
#include "lwlink.h"
#include "g_lwconfig.h"
#include "export_lwswitch.h"
#include "mods_lwswitch.h"
#include "ioctl_dev_lwswitch.h"
#include "ioctl_dev_internal_lwswitch.h"
#include "ctrl_dev_lwswitch.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "core/include/tee.h"
#include "lwgputypes.h"
#include "core/include/jscript.h"
#include "core/include/gpu.h"

#include <algorithm>

extern "C"
{
    #include "lwswitch_user_api.h"
}

extern RC RmApiStatusToModsRC(UINT32 status);

namespace LwLinkDevIf
{
    const UINT32 LWSWITCH_HANDLE = 0xaa000000;
};

//------------------------------------------------------------------------------
//! \brief Create an the instance of the lwswitch library interface
//!
//! \return Pointer to the lwswitch library interface
//!
/* static */ LwLinkDevIf::LibIfPtr LwLinkDevIf::LibInterface::CreateLwSwitchLibIf()
{
    return make_shared<LwLinkDevIf::LwSwitchLibIfKernel>();
}

//------------------------------------------------------------------------------
//! \brief Destructor, shutdown the library if initialized
//!
LwLinkDevIf::LwSwitchLibIfKernel::~LwSwitchLibIfKernel()
{
    if (m_bInitialized)
    {
        Shutdown();
    }
}

//------------------------------------------------------------------------------
//! \brief Call the LwSwitch control interface
//!
//! \param handle    : Device handle to apply the control to
//! \param controlId : Control call to make
//! \param pParams   : Control parameters
//! \param paramSize : Size of the control parameters
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwLinkDevIf::LwSwitchLibIfKernel::Control
(
    LibDeviceHandle  handle,
    LibControlId     controlId,
    void *           pParams,
    UINT32           paramSize
)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    RC rc;
    
    UINT32 libControlId = ~0U;
    switch (controlId)
    {
        case CONTROL_GET_LINK_STATUS:
            libControlId = IOCTL_LWSWITCH_GET_LWLINK_STATUS;
            break;
        case CONTROL_LWSWITCH_GET_INFO:
            libControlId = IOCTL_LWSWITCH_GET_INFO;
            break;
        case CONTROL_LWSWITCH_GET_BIOS_INFO:
            libControlId = IOCTL_LWSWITCH_GET_BIOS_INFO;
            break;
        case CONTROL_GET_LINK_CAPS:
        case CONTROL_CLEAR_COUNTERS:
        case CONTROL_GET_COUNTERS:
        case CONTROL_GET_ERR_INFO:
        case CONTROL_CONFIG_EOM:
            return RC::UNSUPPORTED_FUNCTION;

        case CONTROL_LWSWITCH_SET_SWITCH_PORT_CONFIG:
            libControlId = IOCTL_LWSWITCH_SET_SWITCH_PORT_CONFIG;
            break;
        case CONTROL_LWSWITCH_SET_INGRESS_REQUEST_TABLE:
            libControlId = IOCTL_LWSWITCH_SET_INGRESS_REQUEST_TABLE;
            break;
        case CONTROL_LWSWITCH_SET_INGRESS_REQUEST_VALID:
            libControlId = IOCTL_LWSWITCH_SET_INGRESS_REQUEST_VALID;
            break;
        case CONTROL_LWSWITCH_SET_INGRESS_RESPONSE_TABLE:
            libControlId = IOCTL_LWSWITCH_SET_INGRESS_RESPONSE_TABLE;
            break;
        case CONTROL_LWSWITCH_SET_REMAP_POLICY:
            libControlId = IOCTL_LWSWITCH_SET_REMAP_POLICY;
            break;
        case CONTROL_LWSWITCH_SET_ROUTING_ID:
            libControlId = IOCTL_LWSWITCH_SET_ROUTING_ID;
            break;
        case CONTROL_LWSWITCH_SET_ROUTING_LAN:
            libControlId = IOCTL_LWSWITCH_SET_ROUTING_LAN;
            break;
        case CONTROL_LWSWITCH_SET_PORT_TEST_MODE:
            return RC::UNSUPPORTED_FUNCTION;

        // LwSwitch Performance Metric Control and Collection
        case CONTROL_LWSWITCH_GET_INTERNAL_LATENCY:
            libControlId = IOCTL_LWSWITCH_GET_INTERNAL_LATENCY;
            break;
        case CONTROL_LWSWITCH_SET_LATENCY_BINS:
            libControlId = IOCTL_LWSWITCH_SET_LATENCY_BINS;
            break;
        case CONTROL_LWSWITCH_GET_LWLIPT_COUNTERS:
            libControlId = IOCTL_LWSWITCH_GET_LWLIPT_COUNTERS;
            break;
        case CONTROL_LWSWITCH_SET_LWLIPT_COUNTER_CONFIG:
            libControlId = IOCTL_LWSWITCH_SET_LWLIPT_COUNTER_CONFIG;
            break;
        case CONTROL_LWSWITCH_GET_LWLIPT_COUNTER_CONFIG:
            libControlId = IOCTL_LWSWITCH_GET_LWLIPT_COUNTER_CONFIG;
            break;

        // LwSwitch Error Data Control and Collection
        case CONTROL_LWSWITCH_GET_ERRORS:
            libControlId = IOCTL_LWSWITCH_GET_ERRORS;
            break;
        case CONTROL_LWSWITCH_INJECT_LINK_ERROR:
        case CONTROL_LWSWITCH_GET_LWLINK_ECC_ERRORS:
            return RC::UNSUPPORTED_FUNCTION;

        // LwSwitch Register Access
        case CONTROL_LWSWITCH_REGISTER_READ:
            libControlId = IOCTL_LWSWITCH_REGISTER_READ;
            break;
        case CONTROL_LWSWITCH_REGISTER_WRITE:
            libControlId = IOCTL_LWSWITCH_REGISTER_WRITE;
            break;

        // LwSwitch Host2Jtag interfaces
        case CONTROL_LWSWITCH_READ_JTAG_CHAIN:
        case CONTROL_LWSWITCH_WRITE_JTAG_CHAIN:
            return RC::UNSUPPORTED_FUNCTION;

        // LwSwitch Pex Counters
        case CONTROL_LWSWITCH_PEX_GET_COUNTERS:
        case CONTROL_LWSWITCH_PEX_CLEAR_COUNTERS:
        case CONTROL_LWSWITCH_PEX_GET_LANE_COUNTERS:
        case CONTROL_LWSWITCH_PEX_CONFIG_EOM:
        case CONTROL_LWSWITCH_PEX_GET_EOM_STATUS:
        case CONTROL_LWSWITCH_PEX_GET_UPHY_DLN_CFG_SPACE:
        case CONTROL_LWSWITCH_SET_PCIE_LINK_SPEED:
            return RC::UNSUPPORTED_FUNCTION;

        // LwSwitch I2C interfaces
        case CONTROL_LWSWITCH_I2C_GET_PORT_INFO:
        case CONTROL_LWSWITCH_I2C_TABLE_GET_DEV_INFO:
        case CONTROL_LWSWITCH_I2C_INDEXED:
            return RC::UNSUPPORTED_FUNCTION;

        // LwSwitch Temp/Voltage
        case CONTROL_LWSWITCH_GET_TEMPERATURE:
            libControlId = IOCTL_LWSWITCH_GET_TEMPERATURE;
            break;
        case CONTROL_LWSWITCH_GET_VOLTAGE:
            return RC::UNSUPPORTED_FUNCTION;

        case CONTROL_LWSWITCH_READ_UPHY_PAD_LANE_REG:
            return RC::UNSUPPORTED_FUNCTION;

        case CONTROL_LWSWITCH_GET_FOM_VALUES:
            return RC::UNSUPPORTED_FUNCTION;

        case CONTROL_LWSWITCH_SET_THERMAL_SLOWDOWN:
            return RC::UNSUPPORTED_FUNCTION;

        // CCI controls
        case CONTROL_LWSWITCH_CCI_CMIS_PRESENCE:
            libControlId = IOCTL_LWSWITCH_CCI_CMIS_PRESENCE;
            break;
        case CONTROL_LWSWITCH_CCI_CMIS_LWLINK_MAPPING:
            libControlId = IOCTL_LWSWITCH_CCI_CMIS_LWLINK_MAPPING;
            break;
        case CONTROL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ:
            libControlId = IOCTL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ;
            break;
        case CONTROL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_WRITE:
            libControlId = IOCTL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_WRITE;
            break;

        default:
            Printf(Tee::PriError, "%s : Unknown control Id %d\n", __FUNCTION__, controlId);
            return RC::SOFTWARE_ERROR;
    }

    lwswitch_device* pDev = nullptr;
    CHECK_RC(GetLwSwitchDeviceInfo(handle, &pDev));
    LwlStatus status = lwswitch_api_control(pDev, libControlId, pParams, paramSize);

    rc = RmApiStatusToModsRC(status);
    if (rc != RC::OK)
    {
        Printf(Tee::PriDebug, GetTeeModule()->GetCode(),
               "%s : Lwswitch library control %u returned %d!!\n",
               __FUNCTION__, controlId, rc.Get());
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Find all LwSwitch devices
//!
//! \param pDevHandles :  Pointer to list of device handles of the devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwSwitchLibIfKernel::FindDevices(vector<LibDeviceHandle> *pDevHandles)
{
    RC rc;
    
    if (!m_bInitialized)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                "%s : LwSwitch library not initialized!!\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(), "Find LwSwitch ports\n");

    pDevHandles->clear();
    
    for (UINT32 index = 0; index < m_Devices.size(); index++)
    {
        LibDeviceHandle newHandle = LWSWITCH_HANDLE | index;
        pDevHandles->push_back(newHandle);
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
            "-------Finish LwSwitch detection-----\n");
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
RC LwLinkDevIf::LwSwitchLibIfKernel::GetBarInfo
(
    LibDeviceHandle handle,
    UINT32          barNum,
    UINT64         *pBarAddr,
    UINT64         *pBarSize,
    void          **ppBar0
)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    if (barNum >= LWSWITCH_MAX_BARS)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid BAR number %u\n",
               __FUNCTION__, barNum);
        return RC::BAD_PARAMETER;
    }

    //TODO
    
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Get the link mask for the specified device
//!
//! \param handle     : Device handle to get link mask for
//! \param pLinkMask  : Pointer to returned link mask
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwSwitchLibIfKernel::GetLinkMask(LibDeviceHandle handle, UINT64 *pLinkMask)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    RC rc;
    
    LWSWITCH_GET_INFO infoParams;
    infoParams.index[0] = LWSWITCH_GET_INFO_INDEX_ENABLED_PORTS_MASK_31_0;
    infoParams.index[1] = LWSWITCH_GET_INFO_INDEX_ENABLED_PORTS_MASK_63_32;
    infoParams.count = 2;
    CHECK_RC(Control(handle,
                     LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_INFO,
                     &infoParams, sizeof(infoParams)));

    *pLinkMask = static_cast<UINT64>(infoParams.info[1]) << 32 | static_cast<UINT64>(infoParams.info[0]);
    
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
RC LwLinkDevIf::LwSwitchLibIfKernel::GetPciInfo
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
    
    LWSWITCH_GET_INFO infoParams;
    infoParams.index[0] = LWSWITCH_GET_INFO_INDEX_PCI_DOMAIN;
    infoParams.index[1] = LWSWITCH_GET_INFO_INDEX_PCI_BUS;
    infoParams.index[2] = LWSWITCH_GET_INFO_INDEX_PCI_DEVICE;
    infoParams.index[3] = LWSWITCH_GET_INFO_INDEX_PCI_FUNCTION;
    infoParams.count = 4;
    CHECK_RC(Control(handle,
                     LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_GET_INFO,
                     &infoParams, sizeof(infoParams)));

    if (pDomain != nullptr)
        *pDomain = infoParams.info[0];
    if (pBus != nullptr)
        *pBus    = infoParams.info[1];
    if (pDev != nullptr)
        *pDev    = infoParams.info[2];
    if (pFunc != nullptr)
        *pFunc   = infoParams.info[3];

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize the library interface
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwSwitchLibIfKernel::Initialize()
{
    RC rc;
    LW_STATUS status;
    
    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "LwSwitch library already initialized\n");
        return RC::OK;
    }
    
    LWSWITCH_GET_DEVICES_V2_PARAMS params = {};
    status = lwswitch_api_get_devices(&params);
    rc = RmApiStatusToModsRC(status);

    if (rc == RC::LWRM_NOTHING_TO_DO)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "LwSwitch library nothing to initialize\n");
        m_bInitialized = true;
        return RC::OK;
    }
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : LwSwitch library device discovery failed!!\n", __FUNCTION__);
        return rc;
    }

    set<UINT64> allowedDevices;
    GpuPtr()->GetAllowedPciDevices(&allowedDevices);
    
    for (UINT32 i = 0; i < params.deviceCount; i++)
    {
        const auto& devInfo = params.info[i];
        
        if (allowedDevices.size() != 0)
        {
            auto it = find_if(allowedDevices.begin(), allowedDevices.end(),
                              [&devInfo](const UINT64& allowedDev) -> bool
                              {
                                  UINT32 domain   = (allowedDev >> 48) & 0xFFFF;
                                  UINT32 bus      = (allowedDev >> 32) & 0xFFFF;
                                  UINT32 device   = (allowedDev >> 16) & 0xFFFF;
                                  UINT32 function =  allowedDev        & 0xFFFF;
                                  return (devInfo.pciDomain == domain &&
                                          devInfo.pciBus == bus &&
                                          devInfo.pciDevice == device &&
                                          devInfo.pciFunction == function);
                              });
            if (it == allowedDevices.end())
            {
                continue;
            }
        }
        
        lwswitch_device* pDev = nullptr;
        status = lwswitch_api_create_device(&devInfo.uuid, &pDev);
        rc = RmApiStatusToModsRC(status);
        if (rc != RC::OK)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                "%s : LwSwitch library failed to create device!!\n", __FUNCTION__);
            return rc;
        }
        
        m_Devices.push_back(pDev);
    }

    m_bInitialized = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the library interface
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwSwitchLibIfKernel::Shutdown()
{
    StickyRC rc;

    if (!m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "LwSwitch library not initialized, skipping shutdown\n");
        return RC::OK;
    }

    for (lwswitch_device* dev : m_Devices)
    {
        lwswitch_api_free_device(&dev);
    }
    
    m_bInitialized = false;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize all LwSwitch devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwSwitchLibIfKernel::InitializeAllDevices()
{
    // No-op
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkDevIf::LwSwitchLibIfKernel::GetLwSwitchDeviceInfo(LibDeviceHandle handle, lwswitch_device** ppDevice)
{
    MASSERT(ppDevice);
    
    UINT32 index = (handle & ~LibInterface::HANDLE_TYPE_MASK);
    if (index > m_Devices.size())
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "LwSwitch library invalid device handle\n");
        return RC::ILWALID_ARGUMENT;
    }
    
    *ppDevice = m_Devices[index];
    
    return RC::OK;
}
