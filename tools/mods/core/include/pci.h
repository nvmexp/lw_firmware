/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2014,2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// PCI interface.

#ifndef PCI_H__
#define PCI_H__

#ifdef NO_ERROR
#undef NO_ERROR
#endif

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#define PCI_BAR_OFFSET(i)    (0x10+4*(i)) // i = 0 - 5. 6,7 reserved
#define PCI_CAP_PTR_OFFSET   0x34  // cfg 13
#define PCI_INTR_LINE_OFFSET 0x3c  // cfg 15
#define PCI_CAP_ID_POWER_MGT 0x01
#define PCI_CAP_ID_MSI       0x05
#define PCI_CAP_ID_PCIX      0x07
#define PCI_CAP_ID_SSVID     0x0D
#define PCI_CAP_ID_PCIE      0x10
#define PCI_CAP_ID_MSIX      0x11

// Note: PCI Domain should properly have 16 bits.  Need to rework the
// address into a 64bit value.  4 bits should be sufficient for now.
//
#define PCI_CONFIG_ADDRESS_DOMAIN       31:28
#define PCI_CONFIG_ADDRESS_OFFSET_HI    27:24
#define PCI_CONFIG_ADDRESS_BUS          23:16
#define PCI_CONFIG_ADDRESS_DEVICE       15:11
#define PCI_CONFIG_ADDRESS_FUNCTION     10:8
#define PCI_CONFIG_ADDRESS_BDF          23:8
#define PCI_CONFIG_ADDRESS_OFFSET_LO     7:0

#define PCI_FIRST_EXT_CAP_OFFSET  0x100

#define LW_PCI_VENDOR_ID                        0x00
#define LW_PCI_DEVICE_ID                        0x02
#define LW_PCI_COMMAND                          0x04
#define LW_PCI_CONTROL_STATUS                   0x06
#define LW_PCI_SUBSYSTEM_VENDOR                 0x2c
#define LW_PCI_SUBSYSTEM_ID                     0x2e

// Pci Command Bits
#define LW_PCI_COMMAND_IO_SPACE_EN              0:0
#define LW_PCI_COMMAND_MEM_SPACE_EN             1:1
#define LW_PCI_COMMAND_BUS_MASTER_EN            2:2
#define LW_PCI_COMMAND_INTX_DISABLE           10:10

// BAR Bits
#define LW_PCI_BAR_REG_ADDR_TYPE                2:1
#define LW_PCI_BAR_REG_ADDR_TYPE_32BIT            0
#define LW_PCI_BAR_REG_ADDR_TYPE_64BIT            2

// MSI Capability
#define LW_PCI_CAP_MSI_CTRL                               0x02
#define LW_PCI_CAP_MSI_CTRL_ENABLE                         0:0
#define LW_PCI_CAP_MSI_CTRL_ENABLE_FALSE                     0
#define LW_PCI_CAP_MSI_CTRL_ENABLE_TRUE                      1
#define LW_PCI_CAP_MSI_CTRL_64BIT                          7:7
#define LW_PCI_CAP_MSI_CTRL_PER_VEC_MASKING                8:8
#define LW_PCI_CAP_MSI_CTRL_MULT_MESSAGE                   6:4
#define LW_PCI_CAP_MSI_MASK_32BIT                         0x0C
#define LW_PCI_CAP_MSI_MASK_64BIT                         0x10

// MSIX Capability
#define LW_PCI_CAP_MSIX_CTRL                              0x02
#define LW_PCI_CAP_MSIX_CTRL_TABLE_SIZE                   10:0
#define LW_PCI_CAP_MSIX_CTRL_MASKALL                     14:14
#define LW_PCI_CAP_MSIX_CTRL_ENABLE                      15:15
#define LW_PCI_CAP_MSIX_TABLE                             0x04
#define LW_PCI_CAP_MSIX_TABLE_BIR                          2:0
#define LW_PCI_CAP_MSIX_TABLE_BIR_10H                        0
#define LW_PCI_CAP_MSIX_TABLE_BIR_14H                        1
#define LW_PCI_CAP_MSIX_TABLE_BIR_18H                        2
#define LW_PCI_CAP_MSIX_TABLE_BIR_1CH                        3
#define LW_PCI_CAP_MSIX_TABLE_BIR_20H                        4
#define LW_PCI_CAP_MSIX_TABLE_BIR_24H                        5
#define LW_PCI_CAP_MSIX_TABLE_OFFSET                      31:3

// MSIX Vector Table Defines
#define LW_PCI_MSIX_TABLE_ENTRY_SIZE            16   
#define LW_PCI_MSIX_TABLE_CONTROL(i)            (12+LW_PCI_MSIX_TABLE_ENTRY_SIZE*(i))
#define LW_PCI_MSIX_TABLE_CONTROL_MASK          0:0
#define LW_PCI_MSIX_TABLE_CONTROL_MASK_ENABLED  0
#define LW_PCI_MSIX_TABLE_CONTROL_MASK_DISABLED 1

#define LW_PCI_CAP_DEVICE_CAP                   0x4
#define LW_PCI_CAP_DEVICE_CAP_FLR               28:28

#define LW_PCI_CAP_DEVICE_CTL                   0x8
#define LW_PCI_CAP_DEVICE_CTL_FLR               15:15
#define LW_PCI_CAP_DEVICE_CTL_ERROR_BITS        19:16

#define LW_PCI_CAP_PCIE_LINK_CAP                   0xC
#define LW_PCI_CAP_PCIE_LINK_CAP_LINK_SPEED        3:0
#define LW_PCI_CAP_PCIE_LINK_CAP_LINK_WIDTH        9:4
#define LW_PCI_CAP_PCIE_LINK_CAP_CLK_PWR_MGT       18:18
#define LW_PCI_CAP_PCIE_LINK_CAP_LINK_ACT_REPORT   20:20
#define LW_PCI_CAP_PCIE_LINK_CAP_LINK_PORT_NUMBER  31:24

#define LW_PCI_EXT_CAP_HEADER_VERSION              19:16

#define LW_PCI_CAP_PCIE_LINK_CTRL                          0x10
#define LW_PCI_CAP_PCIE_LINK_CTRL_ASPM                     1:0
#define LW_PCI_CAP_PCIE_LINK_CTRL_RSVD                     2:2
#define LW_PCI_CAP_PCIE_LINK_CTRL_RCB                      3:3
#define LW_PCI_CAP_PCIE_LINK_CTRL_LINK_DISABLE             4:4
#define LW_PCI_CAP_PCIE_LINK_CTRL_LINK_DISABLE_FALSE         0
#define LW_PCI_CAP_PCIE_LINK_CTRL_LINK_DISABLE_TRUE          1
#define LW_PCI_CAP_PCIE_LINK_CTRL_LINK_RETRAIN             5:5
#define LW_PCI_CAP_PCIE_LINK_CTRL_CMN_CLK_CFG              6:6
#define LW_PCI_CAP_PCIE_LINK_CTRL_EXT_SYNC                 7:7
#define LW_PCI_CAP_PCIE_LINK_CTRL_EN_CLK_PWR_MGT           8:8
#define LW_PCI_CAP_PCIE_LINK_CTRL_HW_AUTO_WIDTH_DISABLE    9:9
#define LW_PCI_CAP_PCIE_LINK_CTRL_BW_MGT_INTR_ENABLE     10:10
#define LW_PCI_CAP_PCIE_LINK_CTRL_AUTO_BW_INTR_ENABLE    11:11

#define LW_PCI_CAP_PCIE_LINK_STS                           0x12
#define LW_PCI_CAP_PCIE_LINK_STS_LINK_SPEED                3:0
#define LW_PCI_CAP_PCIE_LINK_STS_LINK_WIDTH                9:4
#define LW_PCI_CAP_PCIE_LINK_STS_ACTIVE                    13:13

#define LW_PCI_CAP_PCIE_SLOT_CAP                           0x14
#define LW_PCI_CAP_PCIE_SLOT_CAP_HOTPLUG_CAPABLE           5:5
#define LW_PCI_CAP_PCIE_SLOT_CAP_NO_CMD_COMPLETE           18:18

#define LW_PCI_CAP_PCIE_SLOT_CTRL                          0x18
#define LW_PCI_CAP_PCIE_SLOT_CTRL_HOTPLUG_INT              5:5
#define LW_PCI_CAP_PCIE_SLOT_CTRL_HOTPLUG_INT_DISABLED     0
#define LW_PCI_CAP_PCIE_SLOT_CTRL_HOTPLUG_INT_ENABLED      1

#define LW_PCI_CAP_PCIE_SLOT_STS                           0x1A
#define LW_PCI_CAP_PCIE_SLOT_STS_CMD_COMPLETE              4:4

#define LW_PCI_CAP_PCIE_ROOT_CTRL                          0x1C

#define LW_PCI_CAP_PCIE_CAP2                               0x24
#define LW_PCI_CAP_PCIE_CAP2_ATOMICS_ROUTE_SUPPORTED       6:6
#define LW_PCI_CAP_PCIE_CAP2_ATOMICS_ROUTE_SUPPORTED_NO    0
#define LW_PCI_CAP_PCIE_CAP2_ATOMICS_ROUTE_SUPPORTED_YES   1
#define LW_PCI_CAP_PCIE_CAP2_32BIT_ATOMICS_SUPPORTED       7:7
#define LW_PCI_CAP_PCIE_CAP2_32BIT_ATOMICS_SUPPORTED_NO    0
#define LW_PCI_CAP_PCIE_CAP2_32BIT_ATOMICS_SUPPORTED_YES   1
#define LW_PCI_CAP_PCIE_CAP2_64BIT_ATOMICS_SUPPORTED       8:8
#define LW_PCI_CAP_PCIE_CAP2_64BIT_ATOMICS_SUPPORTED_NO    0
#define LW_PCI_CAP_PCIE_CAP2_64BIT_ATOMICS_SUPPORTED_YES   1
#define LW_PCI_CAP_PCIE_CAP2_128BIT_ATOMICS_SUPPORTED      9:9
#define LW_PCI_CAP_PCIE_CAP2_128BIT_ATOMICS_SUPPORTED_NO   0
#define LW_PCI_CAP_PCIE_CAP2_128BIT_ATOMICS_SUPPORTED_YES  1
#define LW_PCI_CAP_PCIE_CAP2_LTR_SUPPORTED                 11:11
#define LW_PCI_CAP_PCIE_CAP2_LTR_SUPPORTED_NO              0
#define LW_PCI_CAP_PCIE_CAP2_LTR_SUPPORTED_YES             1

#define LW_PCI_CAP_PCIE_CTRL2                              0x28
#define LW_PCI_CAP_PCIE_CTRL2_ATOMIC_REQUESTOR             6:6
#define LW_PCI_CAP_PCIE_CTRL2_ATOMIC_REQUESTOR_DISABLE     0
#define LW_PCI_CAP_PCIE_CTRL2_ATOMIC_REQUESTOR_ENABLE      1
#define LW_PCI_CAP_PCIE_CTRL2_ATOMIC_EGRESS_BLOCK          7:7
#define LW_PCI_CAP_PCIE_CTRL2_ATOMIC_EGRESS_BLOCK_DISABLE  0
#define LW_PCI_CAP_PCIE_CTRL2_ATOMIC_EGRESS_BLOCK_ENABLE   1
#define LW_PCI_CAP_PCIE_CTRL2_LTR                          10:10
#define LW_PCI_CAP_PCIE_CTRL2_LTR_DISABLE                  0
#define LW_PCI_CAP_PCIE_CTRL2_LTR_ENABLE                   1

#define LW_PCI_CAP_PCIE_LINK_CTRL2                              0x30
#define LW_PCI_CAP_PCIE_LINK_CTRL2_TARGET_LINK_SPEED            3:0
#define LW_PCI_CAP_PCIE_LINK_CTRL2_TARGET_LINK_SPEED_GEN1       1
#define LW_PCI_CAP_PCIE_LINK_CTRL2_TARGET_LINK_SPEED_GEN2       2
#define LW_PCI_CAP_PCIE_LINK_CTRL2_TARGET_LINK_SPEED_GEN3       3
#define LW_PCI_CAP_PCIE_LINK_CTRL2_TARGET_LINK_SPEED_GEN4       4
#define LW_PCI_CAP_PCIE_LINK_CTRL2_TARGET_LINK_SPEED_GEN5       5

#define LW_PCI_CAP_PCIE_SLOT_CTRL2                              0x38

// PCI-X Cap Registers
#define LW_PCI_CAP_PCIX_CMD                                     0x02

// AER Extended Cap Registers
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_OFFSET                 0x04
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_DATA_LINK_PROTOCOL     4:4 
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_SURPRISE_DOWN          5:5
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_POISONED_TLP           12:12
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_FLOW_CTRL_PROTOCOL     13:13
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_COMPLETION_TIMEOUT     14:14
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_COMPLETER_ABORT        15:15
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UNEXPECTED_COMPLETION  16:16
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_RECEIVER_OVERFLOW      17:17
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_MALFORMED_TLP          18:18
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ECRC                   19:19
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UNSUPPORTED_REQ        20:20
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ACS_VIOLATION          21:21
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UCORR_INTERNAL         22:22
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_MC_BLOCKED_TLP         23:23
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ATOMICOP_BLOCKED       24:24
#define LW_PCI_CAP_AER_UNCORR_ERR_STATUS_TLP_PREFIX_BLOCKED     25:25

#define LW_PCI_CAP_AER_UNCORR_MASK                              0x08
#define LW_PCI_CAP_AER_UNCORR_SEVERITY                          0x0C
#define LW_PCI_CAP_AER_CORR_ERR_STATUS_OFFSET                   0x10
#define LW_PCI_CAP_AER_CORR_MASK                                0x14

#define LW_PCI_CAP_AER_CTRL_OFFSET                              0x18
#define LW_PCI_CAP_AER_CTRL_FEP_BITS                            4:0
#define LW_PCI_CAP_AER_CTRL_MUTIPLE_HEADER_REC_SUPP             9:9
#define LW_PCI_CAP_AER_CTRL_MUTIPLE_HEADER_REC_EN               10:10
#define LW_PCI_CAP_AER_CORR_HEADER_LOG_OVERFLOW                 15:15

#define LW_PCI_L1SS_CAPS                                  0x04
#define LW_PCI_L1SS_CAPS_L12_PCI_PM                       0:0
#define LW_PCI_L1SS_CAPS_L11_PCI_PM                       1:1
#define LW_PCI_L1SS_CAPS_L12_ASPM                         2:2
#define LW_PCI_L1SS_CAPS_L11_ASPM                         3:3
#define LW_PCI_L1SS_CAPS_L1_PM_SS                         4:4

// Value/Scale default vaules were copied from RM defaults in
// drivers/resman/inc/physical/gpu/bif/bif.h.
#define LW_PCI_L1SS_CONTROL                                 0x08
#define LW_PCI_L1SS_CONTROL_L12_PCI_PM                      0:0
#define LW_PCI_L1SS_CONTROL_L11_PCI_PM                      1:1
#define LW_PCI_L1SS_CONTROL_L12_ASPM                        2:2
#define LW_PCI_L1SS_CONTROL_L11_ASPM                        3:3
#define LW_PCI_L1SS_CONTROL_CMN_MODE_REST_TIME             15:8
#define LW_PCI_L1SS_CONTROL_CMN_MODE_REST_TIME_DEFAULT     0xFF
#define LW_PCI_L1SS_CONTROL_LTR_L12_THRESH_VALUE           25:16
#define LW_PCI_L1SS_CONTROL_LTR_L12_THRESH_VALUE_DEFAULT    0x32
#define LW_PCI_L1SS_CONTROL_LTR_L12_THRESH_SCALE           31:29
#define LW_PCI_L1SS_CONTROL_LTR_L12_THRESH_SCALE_DEFAULT     0x2

// Reset control register for PCI bridges
#define PCI_HEADER_TYPE1_BRIDGE_CONTROL 0x3e

#define LW_PCI_DPC_CAP_CTRL                               0x4
#define LW_PCI_DPC_CAP_CTRL_RP_EXT                        5:5
#define LW_PCI_DPC_CAP_CTRL_RP_EXT_ENABLED                  1
#define LW_PCI_DPC_CAP_CTRL_RP_EXT_DISABLED                 0
#define LW_PCI_DPC_CAP_CTRL_POISONED_TLP                  6:6
#define LW_PCI_DPC_CAP_CTRL_POISONED_TLP_ENABLED            1
#define LW_PCI_DPC_CAP_CTRL_POISONED_TLP_DISABLED           0
#define LW_PCI_DPC_CAP_CTRL_SW_TRIG                       7:7
#define LW_PCI_DPC_CAP_CTRL_SW_TRIG_ENABLED                 1
#define LW_PCI_DPC_CAP_CTRL_SW_TRIG_DISABLED                0
#define LW_PCI_DPC_CAP_CTRL_RP_PIO_LOG_DWORDS            11:8
#define LW_PCI_DPC_CAP_CTRL_TRIG_ENABLE                 17:16
#define LW_PCI_DPC_CAP_CTRL_TRIG_ENABLE_DISABLED            0
#define LW_PCI_DPC_CAP_CTRL_TRIG_ENABLE_FATAL               1
#define LW_PCI_DPC_CAP_CTRL_TRIG_ENABLE_NON_FATAL           2

// Control register for 16 bit accesses
#define LW_PCI_DPC_CTRL                                   0x6

#define LW_PCI_CAP_SSVID_VENDOR                           0x4
#define LW_PCI_CAP_SSVID_DEVICE                           0x6

// Power Management Capability
#define LW_PCI_CAP_POWER_MGT_CTRL_STS                     0x04
#define LW_PCI_CAP_POWER_MGT_CTRL_STS_POWER_STATE          1:0
#define LW_PCI_CAP_POWER_MGT_CTRL_STS_POWER_STATE_PS_D0      0
#define LW_PCI_CAP_POWER_MGT_CTRL_STS_POWER_STATE_PS_D1      1
#define LW_PCI_CAP_POWER_MGT_CTRL_STS_POWER_STATE_PS_D2      2
#define LW_PCI_CAP_POWER_MGT_CTRL_STS_POWER_STATE_PS_D3      3

// Resizable BAR Extended Capability
#define LW_PCI_CAP_RESIZABLE_BAR_STRIDE                      8
#define LW_PCI_CAP_RESIZABLE_BAR_CAP                      0x04
#define LW_PCI_CAP_RESIZABLE_BAR_CAP_BAR_SIZE_BITS        31:4

#define LW_PCI_CAP_RESIZABLE_BAR_CTRL                     0x08
#define LW_PCI_CAP_RESIZABLE_BAR_CTRL_INDEX                2:0
#define LW_PCI_CAP_RESIZABLE_BAR_CTRL_NUM_RESIZABLE_BARS   7:5
#define LW_PCI_CAP_RESIZABLE_BAR_CTRL_BAR_SIZE            13:8
#define LW_PCI_CAP_RESIZABLE_BAR_CTRL_BAR_SIZE_CAP_UPPER 31:16

// LTR Extended Capability
#define LW_PCI_CAP_LTR_MAX_SNOOP_LAT                      0x04
#define LW_PCI_CAP_LTR_MAX_NOSNOOP_LAT                    0x06

// VC (and MFVC, VC9) Extended Capability
#define LW_PCI_CAP_VC_CAP1                                0x04
#define LW_PCI_CAP_VC_CAP1_EVC_COUNT                       2:0
#define LW_PCI_CAP_VC_CAP1_LPEVC_COUNT                     6:4
#define LW_PCI_CAP_VC_CAP1_ARB_SIZE                      11:10
#define LW_PCI_CAP_VC_CAP2                                0x08
#define LW_PCI_CAP_VC_CAP2_32_PHASE                        1:1
#define LW_PCI_CAP_VC_CAP2_64_PHASE                        2:2
#define LW_PCI_CAP_VC_CAP2_128_PHASE                       3:3
#define LW_PCI_CAP_VC_CAP2_ARB_OFF_QWORD                 31:24
#define LW_PCI_CAP_VC_PORT_CTRL                           0x0c
#define LW_PCI_CAP_VC_PORT_CTRL_LOAD_TABLE                 0:0
#define LW_PCI_CAP_VC_RES_CAP                             0x10
#define LW_PCI_CAP_VC_RES_CAP_32_PHASE                     1:1
#define LW_PCI_CAP_VC_RES_CAP_64_PHASE                     2:2
#define LW_PCI_CAP_VC_RES_CAP_128_PHASE                    3:3
#define LW_PCI_CAP_VC_RES_CAP_128_PHASE_TB                 4:4
#define LW_PCI_CAP_VC_RES_CAP_256_PHASE                    5:5
#define LW_PCI_CAP_VC_RES_CAP_ARB_OFF_QWORD              31:24
#define LW_PCI_CAP_VC_RES_CTRL                            0x14
#define LW_PCI_CAP_VC_RES_CTRL_LOAD_TABLE                16:16
#define LW_PCI_CAP_VC_RES_CTRL_ARB_SELECT                19:17
#define LW_PCI_CAP_VC_RES_CTRL_ENABLE                    31:31
#define LW_PCI_CAP_VC_RES_STATUS                          0x1A
#define LW_PCI_CAP_VC_RES_STATUS_TABLE                     0:0
#define LW_PCI_CAP_VC_PER_VC_STRIDE                        0xc

// PASID Extended Capability
#define LW_PCI_CAP_PASID_CTRL                             0x06
#define LW_PCI_CAP_PASID_CTRL_ENABLE                       0:0
#define LW_PCI_CAP_PASID_CTRL_EXEC                         1:1
#define LW_PCI_CAP_PASID_CTRL_PRIV                         2:2

// PRI Extended Capability
#define LW_PCI_CAP_PRI_CTRL                               0x04
#define LW_PCI_CAP_PRI_CTRL_ENABLE                         0:0
#define LW_PCI_CAP_PRI_ALLOC_REQ                          0x0c

// ATS Extended Capability
#define LW_PCI_CAP_ATS_CTRL                               0x06
#define LW_PCI_CAP_ATS_CTRL_ENABLE                       15:15

// ACS Extended Capability
#define LW_PCI_CAP_ACS_CAP                                0x04
#define LW_PCI_CAP_ACS_CAP_EGRESS_VECTOR_BITS             15:8
#define LW_PCI_CAP_ACS_CTRL                               0x06
#define LW_PCI_CAP_ACS_CTRL_P2P_REDIRECT                   2:2
#define LW_PCI_CAP_ACS_CTRL_P2P_REDIRECT_ENABLE            0x1
#define LW_PCI_CAP_ACS_CTRL_P2P_COMP_REDIRECT              3:3
#define LW_PCI_CAP_ACS_CTRL_P2P_COMP_REDIRECT_ENABLE       0x1
#define LW_PCI_CAP_ACS_CTRL_UPSTREAM_FORWARD               4:4
#define LW_PCI_CAP_ACS_CTRL_UPSTREAM_FORWARD_ENABLE        0x1

// SRIOV Extended Capability
#define LW_PCI_CAP_SRIOV_CTRL                             0x08
#define LW_PCI_CAP_SRIOV_CTRL_VF_ENABLE                    0:0
#define LW_PCI_CAP_SRIOV_CTRL_ARI                          4:4
#define LW_PCI_CAP_SRIOV_NUM_VFS                          0x10
#define LW_PCI_CAP_SRIOV_SYS_PAGE_SIZE                    0x20
#define LW_PCI_CAP_SRIOV_BAR(i)                 (0x24 + 4*(i))
#define LW_PCI_CAP_NUM_BARS                                  6

// Vendor Specific (GPU) Extended Capability
#define LW_PCI_CAP_GPU_VSEC_CAP                           0x04
#define LW_PCI_CAP_ACS_CAP_EGRESS_VECTOR_BITS             15:8
#define LW_PCI_CAP_ACS_CTRL                               0x06


// Class Codes for relevant devices
#define LW_PCI_REV_ID_CLASS_CODE 31:8
#define CLASS_CODE_SATA    0x010185
#define CLASS_CODE_AHCI    0x010601
#define CLASS_CODE_EHCI    0x0c0320
#define CLASS_CODE_AZA     0x040300
#define CLASS_CODE_P2P     0x060400
#define CLASS_CODE_LPC     0x060100
#define CLASS_CODE_SATA_RAID 0x010485
#define CLASS_CODE_USB     0x0c0310
#define CLASS_CODE_USB2    0x0c0320
#define CLASS_CODE_USBCD   0x050100
#define CLASS_CODE_XHCI    0x0C0330
#define CLASS_CODE_XUSB    0x0C03FE     // XHCI device mods
#define CLASS_CODE_SMB     0x0c0500
#define CLASS_CODE_SMB2    0x0c0500

namespace Pci
{
    enum VendorIds
    {
        Lwpu = 0x10DE,
        Intel  = 0x8086,
        Via    = 0x1106,
        Ali    = 0x10B9,
        Plx    = 0x10B5,
        Ampere = 0x1DEF,
        Broadcom = 0x1000,
        Amd = 0x1022
    };

    enum IntelDeviceIds
    {
        IntelZ75 = 0x151
        ,IntelHSW = 0xc01
    };

    enum AmdDeviceIds
    {
        KernczSmbusHostController = 0x790b
    };

    enum ClassCodes
    {
        Undefined            = 0x00,
        StorageController    = 0x01,
        NetworkController    = 0x02,
        VideoController      = 0x03,
        MultimediaController = 0x04,
        MemoryController     = 0x05,
        Bridge               = 0x06
    // Reserved             = 0x07 - 0x0E
    };

    enum ExtendedClassCodes
    {
        CLASS_CODE_UNDETERMINED         = 0x000000,
        CLASS_CODE_VIDEO_CONTROLLER_VGA = 0x030000,
        CLASS_CODE_VIDEO_CONTROLLER_3D  = 0x030200,
        CLASS_CODE_BRIDGE               = 0x060400,
        CLASS_CODE_BRIDGE_SD            = 0x060401,
        CLASS_CODE_LWLINK_EBRIDGE       = 0x068000,
        CLASS_CODE_USB30_XHCI_HOST      = 0x0C0330,
        CLASS_CODE_SERIAL_BUS_CONTROLLER= 0x0C8000
    };

    enum PcieLinkSpeed
    {
        Speed32000MBPS = 32000,
        Speed16000MBPS = 16000,
        Speed8000MBPS  = 8000,
        Speed5000MBPS  = 5000,
        Speed2500MBPS  = 2500,
        SpeedUnknown   = 0,
        SpeedMax = Speed32000MBPS
    };

    enum PcieErrors
    {
        NO_ERROR          = 0,
        CORRECTABLE_ERROR = 0x1,
        NON_FATAL_ERROR   = 0x2,
        FATAL_ERROR       = 0x4,
        UNSUPPORTED_REQ   = 0x8,
    };

    // Each PCIE link has two side of the node. Each node has its own set
    // of control registers
    enum PcieNode
    {
        LOCAL_NODE = 0,  // downstream node (eg. GPU)
        HOST_NODE  = 1,  // upstream node (eg. root complex)
    };

    typedef UINT32 PcieErrorsMask;

    enum ASPMState
    {
        ASPM_DISABLED = 0,
        ASPM_L0S      = (1<<0),
        ASPM_L1       = (1<<1),
        ASPM_L0S_L1   = ((1<<0) | (1<<1)),
        ASPM_UNKNOWN_STATE = (1<<2),
    };

    enum PmL1Substate
    {
        PM_SUB_DISABLED =   0,
        PM_SUB_L11      = 0x1,
        PM_SUB_L12      = 0x2,
        PM_SUB_L11_L12  = 0x3,
        PM_UNKNOWN_SUB_STATE = (1<<2),
    };

    enum DpcCap
    {
        DPC_CAP_NONE          = 0,
        DPC_CAP_BASIC         = (1 << 0),
        DPC_CAP_RP_EXT        = (1 << 1),
        DPC_CAP_POISONED_TLP  = (1 << 2),
        DPC_CAP_SW_TRIG       = (1 << 3),
        DPC_CAP_UNKNOWN       = (1 << 4),
    };

    enum DpcState
    {
        DPC_STATE_DISABLED  =   0,
        DPC_STATE_FATAL     = 0x1,
        DPC_STATE_NON_FATAL = 0x2,
        DPC_STATE_UNKNOWN   = ~0,
    };

    enum ExtendedCapID
    {
         AER_ECI           = 0x01   // Advanced Error Reporting
        ,VC_ECI            = 0x02   // Virtual Channel
        ,MFVC_ECI          = 0x08   // Multi-Function Virtual Channel
        ,VC9_ECI           = 0x09   // Virtual Channel
        ,VSEC_ECI          = 0x0B   // Vendor Specific Extended Capability
        ,ACS_ECI           = 0x0D   // Access Control Services
        ,ATS_ECI           = 0x0F   // Address Translation Services
        ,SRIOV             = 0x10   // Single Root I/O Virtualization
        ,PRI_ECI           = 0x13   // Page Request Interface
        ,RESIZABLE_BAR_ECI = 0x15   //
        ,LTR_ECI           = 0x18   // Latency Tolerance Reporting
        ,SEC_PCIE_ECI      = 0x19   // Secondary PCIE Express
        ,PASID_ECI         = 0x1B   // Process Address Space ID
        ,DPC_ECI           = 0x1D   // Downstream Port Containment
        ,L1SS_ECI          = 0x1E   // L1 PM Substates
    };

    enum FomMode
    {
        EYEO_X,
        EYEO_Y,
        EYEO_Y_U,
        EYEO_Y_L
    };

    enum AtomicTypes
    {
        ATOMIC_32BIT   = (1 << 0),
        ATOMIC_64BIT   = (1 << 1),
        ATOMIC_128BIT  = (1 << 2),
        ATOMIC_UNKNOWN = (1 << 3),
    };

    const char * AspmStateToString(ASPMState aspmState);
    const char * AspmL1SubstateToString(PmL1Substate aspmL1Subtate);

    //! Find Msi base cap of given device.
    //! Return Pci Cfg Offset of Msi cap pointer.
    RC FindCapBase(INT32 DomainNumber, INT32 BusNumber, INT32 DevNumber, INT32 FunNumber,
                    UINT08 CapId, UINT08 * pCapPtr = 0);
    RC GetExtendedCapInfo
    (
        INT32         DomainNumber,
        INT32         BusNumber,
        INT32         DevNumber,
        INT32         FunNumber,
        ExtendedCapID ExtCapId,
        UINT08        BaseCapOffset,
        UINT16 *      pExtCapOffset,
        UINT16 *      pExtCapSize
    );

    RC ReadExtendedCapBlock
    (
        INT32           domainNumber,
        INT32           busNumber,
        INT32           devNumber,
        INT32           funNumber,
        ExtendedCapID   extCapId,
        vector<UINT32>* pExtCapData
    );

    //! Reads BAR base address and size from PCI Config Space Header
    RC GetBarInfo(UINT32 domainNumber, UINT32 busNumber,
                  UINT32 deviceNumber, UINT32 functionNumber,
                  UINT32 barIndex, PHYSADDR* pBaseAddress, UINT64* pBarSize);

    //! Reads BAR base address and size from any PCI Config registers
    RC GetBarInfo(UINT32 domainNumber, UINT32 busNumber,
                  UINT32 deviceNumber, UINT32 functionNumber,
                  UINT32 barIndex, PHYSADDR* pBaseAddress, UINT64* pBarSize,
                  UINT32 barConfigOffset);

    //! Reads IRQ number of PCI device from PCI Config Space Header
    RC GetIRQ(UINT32 domainNumber, UINT32 busNumber,
              UINT32 deviceNumber, UINT32 functionNumber,
              UINT32 * pIrq);

    //! Colwert DPC enums to strings
    string DpcCapMaskToString(UINT32 capMask);
    string DpcStateToString(DpcState cap);

    //! Colwert config address to BDF and back
    UINT32 GetConfigAddress(UINT32 domain, UINT32 bus, UINT32 device,
                             UINT32 function, UINT32 offset);
    void DecodeConfigAddress(UINT32 configAddress, UINT32* pDomain,
                             UINT32* pBus, UINT32* pDevice,
                             UINT32* pFunction, UINT32* pOffset);

    bool GetFLResetSupport(UINT32 domainNumber, UINT32 busNumber,
                           UINT32 deviceNumber, UINT32 functionNumber);
    RC TriggerFLReset(UINT32 domainNumber, UINT32 busNumber,
                      UINT32 deviceNumber, UINT32 functionNumber);
    RC ResizeBar(UINT32 domainNumber, UINT32 busNumber,
                 UINT32 deviceNumber, UINT32 functionNumber,
                 UINT32 pciCapBase, UINT32 barNumber, UINT64 barSizeMb);
    UINT32 LinkSpeedToGen(PcieLinkSpeed linkSpeed);

    RC   ResetFunction
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        UINT32 postResetDelayUs
    );
}

#endif // ! PCI_H__
