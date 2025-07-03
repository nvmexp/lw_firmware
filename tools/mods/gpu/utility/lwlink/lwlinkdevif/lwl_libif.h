/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/types.h"
#include "core/include/rc.h"
#include "device/interface/lwlink.h"
#include <vector>
#include <memory>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief LwLink library interface definition for interacting with the
    //! lwlink libraries.  There will be multiple implementations of this
    //! depending upon platform and user vs. kernel mode
    class LibInterface
    {
        public:
            using LibDeviceHandle = Device::LibDeviceHandle;
            enum
            {
                 HANDLE_TYPE_MASK   = 0xff000000U
            };
            enum LibControlId
            {
                 CONTROL_GET_LINK_CAPS
                ,CONTROL_GET_LINK_STATUS
                ,CONTROL_CLEAR_COUNTERS
                ,CONTROL_GET_COUNTERS
                ,CONTROL_GET_ERR_INFO
                ,CONTROL_INJECT_ERR
                ,CONTROL_CONFIG_EOM
                ,CONTROL_LWSWITCH_START = 0x100

                ,CONTROL_LWSWITCH_GET_INFO = CONTROL_LWSWITCH_START
                ,CONTROL_LWSWITCH_GET_BIOS_INFO
                ,CONTROL_LWSWITCH_SET_SWITCH_PORT_CONFIG
                ,CONTROL_LWSWITCH_SET_PORT_TEST_MODE
                ,CONTROL_LWSWITCH_SET_INGRESS_REQUEST_TABLE
                ,CONTROL_LWSWITCH_SET_INGRESS_REQUEST_VALID
                ,CONTROL_LWSWITCH_SET_INGRESS_RESPONSE_TABLE
                ,CONTROL_LWSWITCH_SET_REMAP_POLICY
                ,CONTROL_LWSWITCH_SET_ROUTING_ID
                ,CONTROL_LWSWITCH_SET_ROUTING_LAN

                // LwSwitch Performance Metric Control and Collection
                ,CONTROL_LWSWITCH_GET_INTERNAL_LATENCY
                ,CONTROL_LWSWITCH_SET_LATENCY_BINS
                ,CONTROL_LWSWITCH_GET_LWLIPT_COUNTERS
                ,CONTROL_LWSWITCH_SET_LWLIPT_COUNTER_CONFIG
                ,CONTROL_LWSWITCH_GET_LWLIPT_COUNTER_CONFIG

                // LwSwitch Error Data Control and Collection
                ,CONTROL_LWSWITCH_GET_ERRORS
                ,CONTROL_LWSWITCH_INJECT_LINK_ERROR
                ,CONTROL_LWSWITCH_GET_LWLINK_ECC_ERRORS

                // LwSwitch Register Access
                ,CONTROL_LWSWITCH_REGISTER_READ
                ,CONTROL_LWSWITCH_REGISTER_WRITE

                // LwSwitch Host2Jtag interfaces
                ,CONTROL_LWSWITCH_READ_JTAG_CHAIN
                ,CONTROL_LWSWITCH_WRITE_JTAG_CHAIN

                // LwSwitch Pex Functions
                ,CONTROL_LWSWITCH_PEX_GET_COUNTERS
                ,CONTROL_LWSWITCH_PEX_CLEAR_COUNTERS
                ,CONTROL_LWSWITCH_PEX_GET_LANE_COUNTERS
                ,CONTROL_LWSWITCH_PEX_CONFIG_EOM
                ,CONTROL_LWSWITCH_PEX_GET_EOM_STATUS
                ,CONTROL_LWSWITCH_PEX_GET_UPHY_DLN_CFG_SPACE
                ,CONTROL_LWSWITCH_SET_PCIE_LINK_SPEED

                // LwSwitch I2C interfaces
                ,CONTROL_LWSWITCH_I2C_GET_PORT_INFO
                ,CONTROL_LWSWITCH_I2C_TABLE_GET_DEV_INFO
                ,CONTROL_LWSWITCH_I2C_INDEXED

                // LwSwitch Temp/Voltage
                ,CONTROL_LWSWITCH_GET_TEMPERATURE
                ,CONTROL_LWSWITCH_GET_VOLTAGE
                ,CONTROL_LWSWITCH_READ_UPHY_PAD_LANE_REG

                ,CONTROL_LWSWITCH_GET_FOM_VALUES

                ,CONTROL_LWSWITCH_GET_LP_COUNTERS

                ,CONTROL_LWSWITCH_SET_THERMAL_SLOWDOWN

                // LwSwitch Cable Controller interfaces
                ,CONTROL_LWSWITCH_CCI_GET_CAPABILITIES
                ,CONTROL_LWSWITCH_CCI_GET_TEMPERATURE
                ,CONTROL_LWSWITCH_CCI_GET_FW_REVISIONS
                ,CONTROL_LWSWITCH_CCI_GET_GRADING_VALUES
                ,CONTROL_LWSWITCH_CCI_GET_MODULE_FLAGS
                ,CONTROL_LWSWITCH_CCI_GET_MODULE_STATE
                ,CONTROL_LWSWITCH_CCI_GET_VOLTAGE
                ,CONTROL_LWSWITCH_CCI_CMIS_PRESENCE
                ,CONTROL_LWSWITCH_CCI_CMIS_LWLINK_MAPPING
                ,CONTROL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ
                ,CONTROL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_WRITE

                // LwSwitch Multicast Routing Table interfaces
                ,CONTROL_LWSWITCH_SET_MC_RID_TABLE

                ,CONTROL_LWSWITCH_RESET_AND_DRAIN_LINKS

                ,CONTROL_TEGRA_START = 0x200

                ,CONTROL_TEGRA_GET_ERROR_RECOVERIES = CONTROL_TEGRA_START
                ,CONTROL_TEGRA_TRAIN_INTRANODE_CONN
                ,CONTROL_TEGRA_GET_LP_COUNTERS
                ,CONTROL_TEGRA_CLEAR_LP_COUNTERS
            };
            virtual ~LibInterface() {}
            virtual RC Control
            (
                LibDeviceHandle  handle,
                LibControlId     controlId,
                void *           pParams,
                UINT32           paramSize
            ) = 0;
            virtual RC FindDevices(vector<LibDeviceHandle> *pDevHandles) = 0;
            virtual RC GetBarInfo
            (
                LibDeviceHandle handle,
                UINT32          barNum,
                UINT64         *pBarAddr,
                UINT64         *pBarSize,
                void          **ppBar0
            ) = 0;
            virtual RC GetLinkMask(LibDeviceHandle handle, UINT64 *pLinkMask) = 0;
            virtual RC GetPciInfo
            (
                LibDeviceHandle handle,
                UINT32         *pDomain,
                UINT32         *pBus,
                UINT32         *pDev,
                UINT32         *pFunc
            ) = 0;
            virtual RC Initialize() = 0;
            virtual RC InitializeAllDevices() = 0;
            virtual RC InitializeDevice(LibDeviceHandle handle) = 0;
            virtual bool IsInitialized() = 0;
            virtual RC Shutdown() = 0;

            static RC LwLinkLibRetvalToModsRc(INT32 retval);
            static shared_ptr<LibInterface> CreateIbmNpuLibIf();
            static shared_ptr<LibInterface> CreateLwSwitchLibIf();
            static shared_ptr<LibInterface> CreateTegraLibIf();
    };

    typedef shared_ptr<LibInterface> LibIfPtr;
};
