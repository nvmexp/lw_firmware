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

#include "lwl_tegra_libif_kernel.h"
#include "lwl_devif.h"
#include "core/include/xp.h"
#include "lwl_devif_mgr.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define __EXPORTED_HEADERS__
#define UNDEF_EXPORTED_HEADERS
#undef BIT_IDX_32
#include <linux/types.h>
#include <linux/ioctl.h>
#include "linux/cheetah-lwlink-uapi.h"
#undef UNDEF_EXPORTED_HEADERS
#undef __EXPORTED_HEADERS__

namespace LwLinkDevIf
{
    const UINT32 TEGRA_HANDLE      = 0xcc000000;

    // List of potential lwlink devices, listed like GPU classes (newest first)
    const char  * s_LwLinkDevs[] =
    {
        "/dev/t19x-lwlink-endpt"
    };
}

//------------------------------------------------------------------------------
//! \brief Create an the instance of the cheetah library interface
//!
//! \return Pointer to the cheetah library interface
//!
/* static */ LwLinkDevIf::LibIfPtr LwLinkDevIf::LibInterface::CreateTegraLibIf()
{
    return make_shared<LwLinkDevIf::LwTegraLibIfKernel>();
}

//------------------------------------------------------------------------------
//! \brief Call the cheetah control interface
//!
//! \param handle    : Device handle to apply the control to
//! \param controlId : Control call to make
//! \param pParams   : Control parameters
//! \param paramSize : Size of the control parameters
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwLinkDevIf::LwTegraLibIfKernel::Control
(
    LibDeviceHandle  handle,
    LibControlId     controlId,
    void *           pParams,
    UINT32           paramSize
)
{
    if (m_LwLinkFd == -1)
    {
        Printf(Tee::PriLow, "LwLink is not supported!\n");
        return RC::SOFTWARE_ERROR;
    }

    MASSERT(handle != Device::ILWALID_LIB_HANDLE);

    UINT32 drvIoctlId;
    switch (controlId)
    {
        case CONTROL_GET_LINK_CAPS:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_GET_LWLINK_CAPS;
            break;
        case CONTROL_GET_LINK_STATUS:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_GET_LWLINK_STATUS;
            break;
        case CONTROL_CLEAR_COUNTERS:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_CLEAR_COUNTERS;
            break;
        case CONTROL_GET_COUNTERS:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_GET_COUNTERS;
            break;
        case CONTROL_GET_ERR_INFO:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_GET_ERR_INFO;
            break;
        case CONTROL_CONFIG_EOM:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_SETUP_EOM;
            break;
        case CONTROL_TEGRA_GET_ERROR_RECOVERIES:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_GET_ERROR_RECOVERIES;
            break;
        case CONTROL_TEGRA_TRAIN_INTRANODE_CONN:
            drvIoctlId = TEGRA_CTRL_LWLINK_TRAIN_INTRANODE_CONN;
            break;
        case CONTROL_TEGRA_GET_LP_COUNTERS:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_GET_LP_COUNTERS;
            break;
        case CONTROL_TEGRA_CLEAR_LP_COUNTERS:
            drvIoctlId = TEGRA_CTRL_CMD_LWLINK_CLEAR_LP_COUNTERS;
            break;
        default:
            Printf(Tee::PriError, "%s : Unknown control Id %d\n", __FUNCTION__, controlId);
            return RC::SOFTWARE_ERROR;
    }

    const int ret = ioctl(m_LwLinkFd, drvIoctlId, pParams);
    if (ret)
    {
        Printf(Tee::PriError, "Can't execute lwlink control %u\n", controlId);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Find all CheetAh devices
//!
//! \param pDevHandles :  Pointer to list of device handles of the devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwTegraLibIfKernel::FindDevices(vector<LibDeviceHandle> *pDevHandles)
{
    if (!m_bInitialized)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                "%s : CheetAh library not initialized!!\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(), "Find CheetAh Lwlink ports\n");

    pDevHandles->clear();
    if (m_LwLinkFd != -1)
    {
        LibDeviceHandle newHandle = TEGRA_HANDLE;
        pDevHandles->push_back(newHandle);
    }

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
            "-------Finish CheetAh Lwlink detection-----\n");
    return OK;
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
RC LwLinkDevIf::LwTegraLibIfKernel::GetBarInfo
(
    LibDeviceHandle handle,
    UINT32          barNum,
    UINT64         *pBarAddr,
    UINT64         *pBarSize,
    void          **ppBar0
)
{
    if ((handle != TEGRA_HANDLE) || (m_LwLinkFd == -1))
        return RC::DEVICE_NOT_FOUND;

    if (pBarAddr != nullptr)
        *pBarAddr = 0ULL;
    if (pBarSize != nullptr)
        *pBarSize = 0;
    if (ppBar0 != nullptr)
        *ppBar0 = nullptr;

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get the link mask for the specified device
//!
//! \param handle     : Device handle to get link mask for
//! \param pLinkMask  : Pointer to returned link mask
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwTegraLibIfKernel::GetLinkMask(LibDeviceHandle handle, UINT64 *pLinkMask)
{
    if ((handle != TEGRA_HANDLE) || (m_LwLinkFd == -1))
        return RC::DEVICE_NOT_FOUND;

    *pLinkMask = 0x1;
    return OK;
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
RC LwLinkDevIf::LwTegraLibIfKernel::GetPciInfo
(
    LibDeviceHandle handle,
    UINT32         *pDomain,
    UINT32         *pBus,
    UINT32         *pDev,
    UINT32         *pFunc
)
{
    if ((handle != TEGRA_HANDLE) || (m_LwLinkFd == -1))
        return RC::DEVICE_NOT_FOUND;

    if (pDomain != nullptr)
        *pDomain = 0;
    if (pBus != nullptr)
        *pBus    = 0;
    if (pDev != nullptr)
        *pDev    = 0;
    if (pFunc != nullptr)
        *pFunc   = 0;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Initialize the library interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwTegraLibIfKernel::Initialize()
{
    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "CheetAh kernel library already initialized\n");
        return OK;

    }

    if (Manager::GetIgnoreTegra())
    {
        Printf(Tee::PriLow,
               "Skipping Lwlink CheetAh Library Interface Initialization\n");
        return RC::OK;
    }

    const char * foundNode = nullptr;

    // Open the driver
    for (auto const &lwlinkDev : s_LwLinkDevs)
    {
        struct stat buf;
        if (0 != stat(lwlinkDev, &buf))
        {
            Printf(Tee::PriLow, "%s not found\n", lwlinkDev);
            continue;
        }
        foundNode = lwlinkDev;
        break;
    }

    if (foundNode != nullptr)
    {
        Printf(Tee::PriLow, "LwLink device node %s found\n", foundNode);
        // Check access permissions
        if (0 != access(foundNode, R_OK | W_OK))
        {
            Printf(Tee::PriWarn,
                   "Insufficient access rights for %s, lwlink will not be supported\n",
                   foundNode);
            return OK;
        }

        // The first one that is present is the one it should use pass or fail
        m_LwLinkFd = open(foundNode, O_RDWR);
        if (m_LwLinkFd == -1)
        {
            // When we try to open this device the first time after boot,
            // it fails.  Just retry.
            m_LwLinkFd = open(foundNode, O_RDWR);
            if (m_LwLinkFd == -1)
            {
                Printf(Tee::PriError, "Unable to open %s.\n", foundNode);
                return RC::FILE_BUSY;
            }
        }
    }

    m_bInitialized = true;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the library interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwTegraLibIfKernel::Shutdown()
{
    if (!m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "CheetAh kernel library not initialized, skipping shutdown\n");
        return OK;
    }

    if (m_LwLinkFd != -1)
    {
        close(m_LwLinkFd);
        m_LwLinkFd = -1;
    }

    m_bInitialized = false;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Initialize all CheetAh devices
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkDevIf::LwTegraLibIfKernel::InitializeAllDevices()
{
    if (!m_bInitialized)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
                "%s : CheetAh library not initialized\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}
