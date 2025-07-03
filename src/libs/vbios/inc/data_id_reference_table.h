/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @note    This file is branched directly from the VBIOS tree for data
 *          structures that need to be shared between VBIOS microcodes and
 *          driver microcodes. Check the P4 file history to see from where it
 *          was branched.
 *
 *          This means that the file should almost never be directly modified.
 *          If updates to this file are needed for new functionality, changes
 *          to the original source file from the VBIOS tree should be merged
 *          into this file.
 *
 *          Additional definitions that are common but that are not pulled
 *          directly from the VBIOS headers should be placed in addendum files.
 */

// Data ID Reference Table
// List of unique IDs for shared Firmware data structures
// http://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Data_ID_Reference_Table
// Bug 1785571

#if !defined __GFW_DATA_ID_REFERENCE_TABLE_H__

#define __GFW_DATA_ID_REFERENCE_TABLE_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#define LW_GFW_DIRT_NULL                                    0x0

#define LW_GFW_DIRT_DATA_RANGE_TABLE                        0x1

#define LW_GFW_DIRT_PLL_INFO_TABLE                          0x2
#define LW_GFW_DIRT_CLOCKS_TABLE                            0x3
#define LW_GFW_DIRT_CLOCK_PROGRAMMING_TABLE                 0x4
#define LW_GFW_DIRT_NAFLL_DEVICE_TABLE                      0x5
#define LW_GFW_DIRT_ADC_DEVICE_TABLE                        0x6
#define LW_GFW_DIRT_FREQUENCY_CONTROLLER_TABLE              0x7

#define LW_GFW_DIRT_DEVINIT_SCRIPTS                         0x8

#define LW_GFW_DIRT_MEMORY_STRAP_TRANSLATION_TABLE          0x9
#define LW_GFW_DIRT_MEMORY_INFORMATION_TABLE                0xA
#define LW_GFW_DIRT_MEMORY_TRAINING_TABLE                   0xB
#define LW_GFW_DIRT_MEMORY_TRAINING_PATTERN_TABLE           0xC
#define LW_GFW_DIRT_MEMORY_PARTITION_INFORMATION_TABLE      0xD

#define LW_GFW_DIRT_PERFORMANCE_TABLE                       0xE
#define LW_GFW_DIRT_MEMORY_CLOCK_TABLE                      0xF
#define LW_GFW_DIRT_MEMORY_TWEAK_TABLE                      0x10
#define LW_GFW_DIRT_THERMAL_DEVICE_TABLE                    0x11
#define LW_GFW_DIRT_POWER_SENSORS_TABLE                     0x12
#define LW_GFW_DIRT_POWER_POLICY_TABLE                      0x13
#define LW_GFW_DIRT_VOLTAGE_FREQUENCY_EQUATION_TABLE        0x14
#define LW_GFW_DIRT_VIRTUAL_PSTATE_TABLE                    0x15
#define LW_GFW_DIRT_POWER_TOPOLOGY_TABLE                    0x16
#define LW_GFW_DIRT_POWER_EQUATION_TABLE                    0x17
#define LW_GFW_DIRT_PERFORMANCE_TEST_SPECIFICATIONS_TABLE   0x18
#define LW_GFW_DIRT_THERMAL_CHANNEL_TABLE                   0x19
#define LW_GFW_DIRT_THERMAL_ADJUSTMENT_TABLE                0x1A
#define LW_GFW_DIRT_THERMAL_POLICY_TABLE                    0x1B
#define LW_GFW_DIRT_PSTATE_MEMORY_CLOCK_FREQUENCY_TABLE     0x1C
#define LW_GFW_DIRT_FAN_COOLER_TABLE                        0x1D
#define LW_GFW_DIRT_FAN_POLICY_TABLE                        0x1E
#define LW_GFW_DIRT_DIDT_TABLE                              0x1F
#define LW_GFW_DIRT_FAN_TEST_TABLE                          0x20
#define LW_GFW_DIRT_VOLTAGE_RAIL_TABLE                      0x21
#define LW_GFW_DIRT_VOLTAGE_DEVICE_TABLE                    0x22
#define LW_GFW_DIRT_VOLTAGE_POLICY_TABLE                    0x23
#define LW_GFW_DIRT_LOWPOWER_TABLE                          0x24
#define LW_GFW_DIRT_THERMAL_MONITOR_TABLE                   0x25
#define LW_GFW_DIRT_OVERCLOCKING_TABLE                      0x26

#define LW_GFW_DIRT_TMDS_INFO_TABLE                         0x27

#define LW_GFW_DIRT_VIRTUAL_STRAP_FIELD_TABLE               0x28
#define LW_GFW_DIRT_VIRTUAL_STRAP_FIELD_REGISTER_TABLE      0x29

#define LW_GFW_DIRT_DP_INFO_TABLE                           0x2A

#define LW_GFW_DIRT_FALCON_UCODE_TABLE                      0x2B

#define LW_GFW_DIRT_DCB_TABLE                               0x2C

#define LW_GFW_DIRT_BIT_TABLE                               0x2D
#define LW_GFW_DIRT_BIT_INTERNAL_USE_ONLY                   0x2E

#define LW_GFW_DIRT_FALCON_UCODE_PREOS                      0x2F
#define LW_GFW_DIRT_FALCON_UCODE_GC6                        0x30
#define LW_GFW_DIRT_FALCON_UCODE_COMPACTION                 0x31
#define LW_GFW_DIRT_FALCON_UCODE_UDE                        0x32
#define LW_GFW_DIRT_FALCON_UCODE_FWSECLIC                   0x33

#define LW_GFW_DIRT_DISPLAY_SCRIPT_TABLE                    0x34
#define LW_GFW_DIRT_DEVINIT_TABLES                          0x35
#define LW_GFW_DIRT_FALCON_UCODE_FBINIT                     0x36

#define LW_GFW_DIRT_PCIE_TABLE                              0x37
#define LW_GFW_DIRT_PCIE_PLATFORM_TABLE                     0x38
#define LW_GFW_DIRT_GR_TABLE                                0x39
#define LW_GFW_DIRT_MS_TABLE                                0x3A
#define LW_GFW_DIRT_DI_TABLE                                0x3B
#define LW_GFW_DIRT_GC6_TABLE                               0x3C
#define LW_GFW_DIRT_PSI_TABLE                               0x3D
#define LW_GFW_DIRT_LWLINK_TABLE                            0x3E

#define LW_GFW_DIRT_MEMORY_BOOT_TIME_TRAINING_TABLE         0x3F
#define LW_GFW_DIRT_FALCON_UCODE_LSUDE                      0x40
#define LW_GFW_DIRT_GPU_DEVID_TABLE                         0x41
#define LW_GFW_DIRT_BCRT2X_META_DATA                        0x42
#define LW_GFW_DIRT_FALCON_UCODE_HULK                       0x43
#define LW_GFW_DIRT_FLOORSWEEPING_TABLE                     0x44
#define LW_GFW_DIRT_IST_TABLE                               0x45

#define LW_GFW_DIRT_FALCON_UCODE_GC6_DBG                    0x46
#define LW_GFW_DIRT_FALCON_UCODE_COMPACTION_DBG             0x47
#define LW_GFW_DIRT_FALCON_UCODE_FWSEC_DBG                  0x48
#define LW_GFW_DIRT_FALCON_UCODE_HULK_DBG                   0x49
#define LW_GFW_DIRT_FALCON_UCODE_GC6_PROD                   0x4A
#define LW_GFW_DIRT_FALCON_UCODE_COMPACTION_PROD            0x4B
#define LW_GFW_DIRT_FALCON_UCODE_FWSEC_PROD                 0x4C
#define LW_GFW_DIRT_FALCON_UCODE_HULK_PROD                  0x4D

#define LW_GFW_DIRT_ROM_DIRECTORY                           0x4E
#define LW_GFW_DIRT_X86_PCI_ROM_HEADER                      0x4F
#define LW_GFW_DIRT_X86_PCI_ROM_FOOTER                      0x50
#define LW_GFW_DIRT_EXT0_PCI_ROM_HEADER                     0x51
#define LW_GFW_DIRT_EXT0_PCI_ROM_FOOTER                     0x52
#define LW_GFW_DIRT_EXT1_PCI_ROM_HEADER                     0x53
#define LW_GFW_DIRT_EXT1_PCI_ROM_FOOTER                     0x54
#define LW_GFW_DIRT_UEFI                                    0x55
#define LW_GFW_DIRT_INFOROM                                 0x56
#define LW_GFW_DIRT_BOOTBLOCK                               0x57
#define LW_GFW_DIRT_FLASHSTATUS                             0x58
#define LW_GFW_DIRT_CERT_CONTROL_HEADER                     0x59
#define LW_GFW_DIRT_CERT_LINKED_LIST                        0x5A
#define LW_GFW_DIRT_CERT_VDPA_TABLE                         0x5B
#define LW_GFW_DIRT_DS_TABLE                                0x5C
#define LW_GFW_DIRT_LOWPOWER_GCOFF                          0x5D
#define LW_GFW_DIRT_LOWPOWER_ADAPTIVEPWR                    0x5E
#define LW_GFW_DIRT_PERF_TOPOLOGY_TABLE                     0x5F
#define LW_GFW_DIRT_PERF_CONTROLLER_TABLE                   0x60
#define LW_GFW_DIRT_PERF_POLICY_TABLE                       0x61
#define LW_GFW_DIRT_PERFPWRMODEL_TABLE                      0X62
#define LW_GFW_DIRT_VF_CONTROLLER_TABLE                     0x63
#define LW_GFW_DIRT_LWLINK_CONFIG_DATA                      0x64
#define LW_GFW_DIRT_CCB_TABLE                               0x65
#define LW_GFW_DIRT_I2CDEV_TABLE                            0x66
#define LW_GFW_DIRT_CONNECTOR_TABLE                         0x67
#define LW_GFW_DIRT_PERF_SENSOR_TABLE                       0x68
#define LW_GFW_DIRT_BIT_PTRS_TABLE                          0x69
#define LW_GFW_DIRT_FALCON_UCODE_SWITCHBOOT                 0x6A
#define LW_GFW_DIRT_FALCON_UCODE_SWITCHBOOT_DBG             0x6B
#define LW_GFW_DIRT_FALCON_UCODE_SWITCHBOOT_PROD            0x6C
#define LW_GFW_DIRT_AFTER_ROM_DIRECTORY_PADDING             0x6D
#define LW_GFW_DIRT_END_OF_CERT_BLOCK_PADDING               0x6E
#define LW_GFW_DIRT_SELWRE_BOOT_FEATURES_TABLE              0x6F
#define LW_GFW_DIRT_TMRS_SEQUENCE_LIST_TABLE                0x70
#define LW_GFW_DIRT_TMRS_SEQUENCE_TABLE                     0x71
#define LW_GFW_DIRT_TMRS_CODE_TABLE                         0x72
#define LW_GFW_DIRT_CLOCK_ENUMERATION_TABLE                 0x73
#define LW_GFW_DIRT_CLOCK_PROP_REGIME_TABLE                 0x74  // Propagation Regime
#define LW_GFW_DIRT_CLOCK_PROP_TOP_TABLE                    0x75  // Propagation Topology 
#define LW_GFW_DIRT_CLOCK_PROP_TOP_REL_TABLE                0x76  // Propagation Topology Relationship
#define LW_GFW_DIRT_LEGACY_VBIOS_RUNTIME                    0x77
#define LW_GFW_DIRT_LEGACY_VBIOS_DATA_RESIDENT              0x78
#define LW_GFW_DIRT_LEGACY_VBIOS_DISCARD                    0x79
#define LW_GFW_DIRT_NEURAL_NET_ENGINE_VARIABLE_TABLE        0x7A
#define LW_GFW_DIRT_NEURAL_NET_ENGINE_LAYER_TABLE           0x7B
#define LW_GFW_DIRT_NEURAL_NET_ENGINE_DESCRIPTOR_TABLE      0x7C
#define LW_GFW_DIRT_PERF_PM_SENSOR_TABLE                    0x7D
#define LW_GFW_DIRT_LOW_POWER_GRRG_TABLE                    0x7E
#define LW_GFW_DIRT_LOW_POWER_EI_TABLE                      0x7F
#define LW_GFW_DIRT_FAN_ARBITER_TABLE                       0x80
#define LW_GFW_DIRT_ILLUMINATION_DEVICE_TABLE               0x81
#define LW_GFW_DIRT_ILLUMINATION_ZONE_TABLE                 0x82
#define LW_GFW_DIRT_FAN_ACOUSTICS_QUALIFICATION_TABLE       0x83
#define LW_GFW_DIRT_THERMAL_POLICY_OVERRIDE_TABLE           0x84
#define LW_GFW_DIRT_DEVINIT_FMC                             0x86
#define LW_GFW_DIRT_POSTLTSSM                               0x87
#define LW_GFW_DIRT_PRELTSSM                                0x88
#define LW_GFW_DIRT_POSTLTSSM_DEVINIT_CFGDATA_TABLE         0x89
#define LW_GFW_DIRT_PRELTSSM_DEVINIT_CFGDATA_TABLE          0x8A
#define LW_GFW_DIRT_MINION_UCODE                            0x8B

#endif  // !defined __GFW_DATA_ID_REFERENCE_TABLE_H__
