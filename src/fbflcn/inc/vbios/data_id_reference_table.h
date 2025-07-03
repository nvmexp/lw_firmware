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

#endif  // !defined __GFW_DATA_ID_REFERENCE_TABLE_H__
