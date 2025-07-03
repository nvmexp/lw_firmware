/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_lwswitch_libif_user.h"
#include "lwl_devif_mgr.h"
#include "lwl_devif.h"
#include "lwlink.h"
#include "g_lwconfig.h"
#include "export_lwswitch.h"
#include "mods_lwswitch.h"
#include "ctrl_dev_lwswitch.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "core/include/tee.h"
#include "lwgputypes.h"
#include "core/include/jscript.h"
#include "core/include/gpu.h"

namespace LwLinkDevIf
{
    const UINT32 LWSWITCH_HANDLE      = 0xaa000000;

    RC GetLwSwitchDeviceInfo(LibInterface::LibDeviceHandle handle, lwlink_device_handle *pDev)
    {
        UINT32 index = handle & ~LibInterface::HANDLE_TYPE_MASK;
        return LibInterface::LwLinkLibRetvalToModsRc(lwswitch_mods_get_device_info(index,
                                                                                   &pDev->linkMask,
                                                                                   &pDev->pciInfo));
    }
};

//------------------------------------------------------------------------------
//! \brief Create an the instance of the lwswitch library interface
//!
//! \return Pointer to the lwswitch library interface
//!
/* static */ LwLinkDevIf::LibIfPtr LwLinkDevIf::LibInterface::CreateLwSwitchLibIf()
{
    return make_shared<LwLinkDevIf::LwSwitchLibIfUser>();
}

//------------------------------------------------------------------------------
//! \brief Destructor, shutdown the library if initialized
//!
LwLinkDevIf::LwSwitchLibIfUser::~LwSwitchLibIfUser()
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
/* virtual */ RC LwLinkDevIf::LwSwitchLibIfUser::Control
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
            libControlId = CTRL_LWSWITCH_GET_LWLINK_CAPS;
            break;
        case CONTROL_GET_LINK_STATUS:
            libControlId = CTRL_LWSWITCH_GET_LWLINK_STATUS;
            break;
        case CONTROL_CLEAR_COUNTERS:
            libControlId = CTRL_LWSWITCH_CLEAR_COUNTERS;
            break;
        case CONTROL_GET_COUNTERS:
            libControlId = CTRL_LWSWITCH_GET_COUNTERS;
            break;
        case CONTROL_GET_ERR_INFO:
            libControlId = CTRL_LWSWITCH_GET_ERR_INFO;
            break;
        case CONTROL_LWSWITCH_GET_INFO:
            libControlId = CTRL_LWSWITCH_GET_INFO;
            break;
        case CONTROL_LWSWITCH_GET_BIOS_INFO:
            libControlId = CTRL_LWSWITCH_GET_BIOS_INFO;
            break;
        case CONTROL_CONFIG_EOM:
            libControlId = CTRL_LWSWITCH_CONFIG_EOM;
            break;

        case CONTROL_LWSWITCH_SET_SWITCH_PORT_CONFIG:
            libControlId = CTRL_LWSWITCH_SET_SWITCH_PORT_CONFIG;
            break;
        case CONTROL_LWSWITCH_SET_PORT_TEST_MODE:
            libControlId = CTRL_LWSWITCH_SET_PORT_TEST_MODE;
            break;
        case CONTROL_LWSWITCH_SET_INGRESS_REQUEST_TABLE:
            libControlId = CTRL_LWSWITCH_SET_INGRESS_REQUEST_TABLE;
            break;
        case CONTROL_LWSWITCH_SET_INGRESS_REQUEST_VALID:
            libControlId = CTRL_LWSWITCH_SET_INGRESS_REQUEST_VALID;
            break;
        case CONTROL_LWSWITCH_SET_INGRESS_RESPONSE_TABLE:
            libControlId = CTRL_LWSWITCH_SET_INGRESS_RESPONSE_TABLE;
            break;
        case CONTROL_LWSWITCH_SET_REMAP_POLICY:
            libControlId = CTRL_LWSWITCH_SET_REMAP_POLICY;
            break;
        case CONTROL_LWSWITCH_SET_ROUTING_ID:
            libControlId = CTRL_LWSWITCH_SET_ROUTING_ID;
            break;
        case CONTROL_LWSWITCH_SET_ROUTING_LAN:
            libControlId = CTRL_LWSWITCH_SET_ROUTING_LAN;
            break;

        // LwSwitch Performance Metric Control and Collection
        case CONTROL_LWSWITCH_GET_INTERNAL_LATENCY:
            libControlId = CTRL_LWSWITCH_GET_INTERNAL_LATENCY;
            break;
        case CONTROL_LWSWITCH_SET_LATENCY_BINS:
            libControlId = CTRL_LWSWITCH_SET_LATENCY_BINS;
            break;
        case CONTROL_LWSWITCH_GET_LWLIPT_COUNTERS:
            libControlId = CTRL_LWSWITCH_GET_LWLIPT_COUNTERS;
            break;
        case CONTROL_LWSWITCH_SET_LWLIPT_COUNTER_CONFIG:
            libControlId = CTRL_LWSWITCH_SET_LWLIPT_COUNTER_CONFIG;
            break;
        case CONTROL_LWSWITCH_GET_LWLIPT_COUNTER_CONFIG:
            libControlId = CTRL_LWSWITCH_GET_LWLIPT_COUNTER_CONFIG;
            break;

        // LwSwitch Error Data Control and Collection
        case CONTROL_LWSWITCH_GET_ERRORS:
            libControlId = CTRL_LWSWITCH_GET_ERRORS;
            break;
        case CONTROL_LWSWITCH_INJECT_LINK_ERROR:
            libControlId = CTRL_LWSWITCH_INJECT_LINK_ERROR;
            break;
        case CONTROL_LWSWITCH_GET_LWLINK_ECC_ERRORS:
            libControlId = CTRL_LWSWITCH_GET_LWLINK_ECC_ERRORS;
            break;

        // LwSwitch Register Access
        case CONTROL_LWSWITCH_REGISTER_READ:
            libControlId = CTRL_LWSWITCH_REGISTER_READ;
            break;
        case CONTROL_LWSWITCH_REGISTER_WRITE:
            libControlId = CTRL_LWSWITCH_REGISTER_WRITE;
            break;

        // LwSwitch Host2Jtag interfaces
        case CONTROL_LWSWITCH_READ_JTAG_CHAIN:
            libControlId = CTRL_LWSWITCH_READ_JTAG_CHAIN;
            break;
        case CONTROL_LWSWITCH_WRITE_JTAG_CHAIN:
            libControlId = CTRL_LWSWITCH_WRITE_JTAG_CHAIN;
            break;

        // LwSwitch Pex Counters
        case CONTROL_LWSWITCH_PEX_GET_COUNTERS:
            libControlId = CTRL_LWSWITCH_PEX_GET_COUNTERS;
            break;
        case CONTROL_LWSWITCH_PEX_CLEAR_COUNTERS:
            libControlId = CTRL_LWSWITCH_PEX_CLEAR_COUNTERS;
            break;
        case CONTROL_LWSWITCH_PEX_GET_LANE_COUNTERS:
            libControlId = CTRL_LWSWITCH_PEX_GET_LANE_COUNTERS;
            break;
        case CONTROL_LWSWITCH_PEX_CONFIG_EOM:
            libControlId = CTRL_LWSWITCH_PEX_SET_EOM;
            break;
        case CONTROL_LWSWITCH_PEX_GET_EOM_STATUS:
            libControlId = CTRL_LWSWITCH_PEX_GET_EOM_STATUS;
            break;
        case CONTROL_LWSWITCH_PEX_GET_UPHY_DLN_CFG_SPACE:
            libControlId = CTRL_LWSWITCH_PEX_GET_UPHY_DLN_CFG_SPACE;
            break;
        case CONTROL_LWSWITCH_SET_PCIE_LINK_SPEED:
            libControlId = CTRL_LWSWITCH_SET_PCIE_LINK_SPEED;
            break;

        // LwSwitch I2C interfaces
        case CONTROL_LWSWITCH_I2C_GET_PORT_INFO:
            libControlId = CTRL_LWSWITCH_I2C_GET_PORT_INFO;
            break;
        case CONTROL_LWSWITCH_I2C_TABLE_GET_DEV_INFO:
            libControlId = CTRL_LWSWITCH_I2C_GET_DEV_INFO;
            break;
        case CONTROL_LWSWITCH_I2C_INDEXED:
            libControlId = CTRL_LWSWITCH_I2C_INDEXED;
            break;

        // LwSwitch Temp/Voltage
        case CONTROL_LWSWITCH_GET_TEMPERATURE:
            libControlId = CTRL_LWSWITCH_GET_TEMPERATURE;
            break;
        case CONTROL_LWSWITCH_GET_VOLTAGE:
            libControlId = CTRL_LWSWITCH_GET_VOLTAGE;
            break;

        case CONTROL_LWSWITCH_READ_UPHY_PAD_LANE_REG:
            libControlId = CTRL_LWSWITCH_READ_UPHY_PAD_LANE_REG;
            break;

        case CONTROL_LWSWITCH_GET_FOM_VALUES:
            libControlId = CTRL_LWSWITCH_GET_FOM_VALUES;
            break;

        case CONTROL_LWSWITCH_GET_LP_COUNTERS:
            libControlId = CTRL_LWSWITCH_GET_LWLINK_LP_COUNTERS;
            break;

        case CONTROL_LWSWITCH_SET_THERMAL_SLOWDOWN:
            libControlId = CTRL_LWSWITCH_SET_THERMAL_SLOWDOWN;
            break;

        // LwSwitch Cable Controller interfaces
        case CONTROL_LWSWITCH_CCI_GET_CAPABILITIES:
            libControlId = CTRL_LWSWITCH_CCI_GET_CAPABILITIES;
            break;
        case CONTROL_LWSWITCH_CCI_GET_TEMPERATURE:
            libControlId = CTRL_LWSWITCH_CCI_GET_TEMPERATURE;
            break;
        case CONTROL_LWSWITCH_CCI_GET_FW_REVISIONS:
            libControlId = CTRL_LWSWITCH_CCI_GET_FW_REVISIONS;
            break;
        case CONTROL_LWSWITCH_CCI_GET_GRADING_VALUES:
            libControlId = CTRL_LWSWITCH_CCI_GET_GRADING_VALUES;
            break;
        case CONTROL_LWSWITCH_CCI_GET_MODULE_FLAGS:
            libControlId = CTRL_LWSWITCH_CCI_GET_MODULE_FLAGS;
            break;
        case CONTROL_LWSWITCH_CCI_GET_MODULE_STATE:
            libControlId = CTRL_LWSWITCH_CCI_GET_MODULE_STATE;
            break;
        case CONTROL_LWSWITCH_CCI_GET_VOLTAGE:
            libControlId = CTRL_LWSWITCH_CCI_GET_VOLTAGE;
            break;
        case CONTROL_LWSWITCH_CCI_CMIS_PRESENCE:
            libControlId = CTRL_LWSWITCH_CCI_CMIS_PRESENCE;
            break;
        case CONTROL_LWSWITCH_CCI_CMIS_LWLINK_MAPPING:
            libControlId = CTRL_LWSWITCH_CCI_CMIS_LWLINK_MAPPING;
            break;
        case CONTROL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ:
            libControlId = CTRL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ;
            break;
        case CONTROL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_WRITE:
            libControlId = CTRL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_WRITE;
            break;

        // LwSwitch Multicast Routing Table interfaces
        case CONTROL_LWSWITCH_SET_MC_RID_TABLE:
            libControlId = CTRL_LWSWITCH_SET_MC_RID_TABLE;
            break;

        case CONTROL_LWSWITCH_RESET_AND_DRAIN_LINKS:
            libControlId = CTRL_LWSWITCH_RESET_AND_DRAIN_LINKS;
            break;

        default:
            Printf(Tee::PriError, "%s : Unknown control Id %d\n", __FUNCTION__, controlId);
            return RC::SOFTWARE_ERROR;
    }

    LwlStatus status = lwswitch_mods_ctrl(handle & ~LibInterface::HANDLE_TYPE_MASK, libControlId, pParams, paramSize);

    const RC rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
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
RC LwLinkDevIf::LwSwitchLibIfUser::FindDevices(vector<LibDeviceHandle> *pDevHandles)
{
    if (!m_bInitialized)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                "%s : LwSwitch library not initialized!!\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(), "Find LwSwitch ports\n");

    pDevHandles->clear();

    UINT32 index = 0;
    lwlink_device_handle dev;
    RC rc = GetLwSwitchDeviceInfo(index, &dev);
    while (rc == OK)
    {
        LibDeviceHandle newHandle = LWSWITCH_HANDLE | index;
        pDevHandles->push_back(newHandle);
        rc = GetLwSwitchDeviceInfo(++index, &dev);
    }

    if (rc != RC::LWRM_NOT_FOUND)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : LwSwitch device discovery failed!!\n", __FUNCTION__);
        return rc;
    }
    else
    {
        rc.Clear();
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
RC LwLinkDevIf::LwSwitchLibIfUser::GetBarInfo
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

    RC rc;
    lwlink_device_handle dev;
    CHECK_RC(GetLwSwitchDeviceInfo(handle, &dev));

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
RC LwLinkDevIf::LwSwitchLibIfUser::GetLinkMask(LibDeviceHandle handle, UINT64 *pLinkMask)
{
    MASSERT(handle != Device::ILWALID_LIB_HANDLE);
    RC rc;
    lwlink_device_handle dev;
    CHECK_RC(GetLwSwitchDeviceInfo(handle, &dev));
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
RC LwLinkDevIf::LwSwitchLibIfUser::GetPciInfo
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
    CHECK_RC(GetLwSwitchDeviceInfo(handle, &dev));

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
RC LwLinkDevIf::LwSwitchLibIfUser::Initialize()
{
    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "LwSwitch library already initialized\n");
        return OK;
    }

    set<UINT64> allowedDevices;
    GpuPtr()->GetAllowedPciDevices(&allowedDevices);

    vector<lwswitch_mods_pci_info> libAllowedDevices;

    transform(allowedDevices.begin(),
              allowedDevices.end(),
              back_inserter(libAllowedDevices),
              [](const UINT64 pci_id) -> lwswitch_mods_pci_info
    {
        return { static_cast<LwU32>((pci_id >> 48) & 0xFFFF),
                 static_cast<LwU32>((pci_id >> 32) & 0xFFFF),
                 static_cast<LwU32>((pci_id >> 16) & 0xFFFF),
                 static_cast<LwU32>(pci_id & 0xFFFF) };
    });

    LwlStatus status = lwswitch_mods_lib_load(&libAllowedDevices[0],
                                              static_cast<LwU32>(libAllowedDevices.size()),
                                              Manager::GetLwSwitchPrintLevel());
    const RC rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
    if (OK != rc)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                "%s : LwSwitch library failed to load\n", __FUNCTION__);
        return rc;
    }

    m_bInitialized = true;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the library interface
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwSwitchLibIfUser::Shutdown()
{
    StickyRC rc;

    if (!m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "LwSwitch library not initialized, skipping shutdown\n");
        return OK;
    }

    LwlStatus status = lwswitch_mods_lib_unload();
    rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
    m_bInitialized = false;
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize all LwSwitch devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwSwitchLibIfUser::InitializeAllDevices()
{
    if (!m_bInitialized)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                "%s : LwSwitch library not initialized\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    LwlStatus status = lwswitch_mods_initialize_all_devices();
    return LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
}

