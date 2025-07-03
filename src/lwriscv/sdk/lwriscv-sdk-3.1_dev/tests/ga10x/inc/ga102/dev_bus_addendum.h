/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ga102_dev_bus_addendum_h__
#define __ga102_dev_bus_addendum_h__

#define LW_PBUS_EMULATION_STUB0_GSP                               12:12 /*       */
#define LW_PBUS_EMULATION_STUB0_GSP_REMOVED                  0x00000000 /*       */
#define LW_PBUS_EMULATION_STUB0_GSP_PRESENT                  0x00000001 /*       */

//
// This register is used to accumulate the total GPU Context utilization time
//
#define LW_PBUS_SW_SCRATCH_GPU_UTIL_COUNTERS_CONTEXT               LW_PBUS_SW_SCRATCH(0x21)
#define LW_PBUS_SW_SCRATCH_GPU_UTIL_COUNTERS_CONTEXT_TIME_MS       31:0

//
// This register is used to accumulate the total GPU SM utilization time
//
#define LW_PBUS_SW_SCRATCH_GPU_UTIL_COUNTERS_SM                    LW_PBUS_SW_SCRATCH(0x22)
#define LW_PBUS_SW_SCRATCH_GPU_UTIL_COUNTERS_SM_TIME_MS            31:0

//
// This bit is set by the RM to indicate, that the GPU reset is required
//
#define LW_PBUS_SW_SCRATCH30_GPU_RESET_REQUIRED                    0:0
#define LW_PBUS_SW_SCRATCH30_GPU_RESET_REQUIRED_OFF                0x00000000
#define LW_PBUS_SW_SCRATCH30_GPU_RESET_REQUIRED_ON                 0x00000001

//
// This bit is set by the RM to indicate, that a GPU reset is recommended after
// work is drained
//
#define LW_PBUS_SW_SCRATCH30_GPU_DRAIN_AND_RESET_RECOMMENDED       1:1
#define LW_PBUS_SW_SCRATCH30_GPU_DRAIN_AND_RESET_RECOMMENDED_NO    0x00000000
#define LW_PBUS_SW_SCRATCH30_GPU_DRAIN_AND_RESET_RECOMMENDED_YES   0x00000001

//
// Per-lwlink enable bits, filled by the RM, consumed by preOS apps
//
#define LW_PBUS_SW_SCRATCH30_LWLINK_ENABLED_ALL                    7:2
#define LW_PBUS_SW_SCRATCH30_LWLINK_ENABLED(i)                     (2+(i)):(2+(i))
#define LW_PBUS_SW_SCRATCH30_LWLINK_ENABLED_OFF                    0x00000000
#define LW_PBUS_SW_SCRATCH30_LWLINK_ENABLED_ON                     0x00000001
#define LW_PBUS_SW_SCRATCH30_LWLINK_ENABLED__SIZE                  6

//
// These bit are set by the RM to indicate the status and the speed of lwlinks
//
#define LW_PBUS_SW_SCRATCH30_LWLINK_STATUS_SPEED                   31:8
#define LW_PBUS_SW_SCRATCH30_LWLINK_STATUS(i)                      (11+(i)*4):(11+(i)*4)
#define LW_PBUS_SW_SCRATCH30_LWLINK_STATUS__SIZE                   6
#define LW_PBUS_SW_SCRATCH30_LWLINK_STATUS_UP                      0x00000000
#define LW_PBUS_SW_SCRATCH30_LWLINK_STATUS_DOWN                    0x00000001
#define LW_PBUS_SW_SCRATCH30_LWLINK_SPEED(i)                       (10+(i)*4):(8+(i)*4)
#define LW_PBUS_SW_SCRATCH30_LWLINK_SPEED__SIZE                    6
#define LW_PBUS_SW_SCRATCH30_LWLINK_SPEED_ILWALID                  0x00000000
#define LW_PBUS_SW_SCRATCH30_LWLINK_SPEED_19_GHZ                   0x00000001
#define LW_PBUS_SW_SCRATCH30_LWLINK_SPEED_20_GHZ                   0x00000002
#define LW_PBUS_SW_SCRATCH30_LWLINK_SPEED_25_GHZ                   0x00000003

//
// Per-lwlink enable bits, filled by the RM, consumed by preOS apps, starting at link 6
//
#define LW_PBUS_SW_SCRATCH32_LWLINK_ENABLED_ALL                    29:24
#define LW_PBUS_SW_SCRATCH32_LWLINK_ENABLED(i)                     (24+((i)-6)):(24+((i)-6))
#define LW_PBUS_SW_SCRATCH32_LWLINK_ENABLED_OFF                    0x00000000
#define LW_PBUS_SW_SCRATCH32_LWLINK_ENABLED_ON                     0x00000001
#define LW_PBUS_SW_SCRATCH32_LWLINK_ENABLED__SIZE                  6

//
// These bit are set by the RM to indicate the status and the speed of lwlinks,
// starting at link 6
//
#define LW_PBUS_SW_SCRATCH32_LWLINK_STATUS_SPEED                   23:0
#define LW_PBUS_SW_SCRATCH32_LWLINK_STATUS(i)                      (3+(((i)-6)*4)):(3+(((i)-6)*4))
#define LW_PBUS_SW_SCRATCH32_LWLINK_STATUS__SIZE                   6
#define LW_PBUS_SW_SCRATCH32_LWLINK_STATUS_UP                      0x00000000
#define LW_PBUS_SW_SCRATCH32_LWLINK_STATUS_DOWN                    0x00000001
#define LW_PBUS_SW_SCRATCH32_LWLINK_SPEED(i)                       (2+(((i)-6)*4)):(((i)-6)*4)
#define LW_PBUS_SW_SCRATCH32_LWLINK_SPEED__SIZE                    6
#define LW_PBUS_SW_SCRATCH32_LWLINK_SPEED_ILWALID                  0x00000000
#define LW_PBUS_SW_SCRATCH32_LWLINK_SPEED_19_GHZ                   0x00000001
#define LW_PBUS_SW_SCRATCH32_LWLINK_SPEED_20_GHZ                   0x00000002
#define LW_PBUS_SW_SCRATCH32_LWLINK_SPEED_25_GHZ                   0x00000003

// These bits reflect SMC states
#define LW_PBUS_SW_SCRATCH1_INFOROM_SEN                             LW_PBUS_SW_SCRATCH(1)
#define LW_PBUS_SW_SCRATCH1_INFOROM_SEN_PRESENT                     13:13
#define LW_PBUS_SW_SCRATCH1_INFOROM_SEN_PRESENT_FALSE               0x00000000
#define LW_PBUS_SW_SCRATCH1_INFOROM_SEN_PRESENT_TRUE                0x00000001
#define LW_PBUS_SW_SCRATCH1_INFOROM_SEN_SMC                         14:14
#define LW_PBUS_SW_SCRATCH1_INFOROM_SEN_SMC_DISABLE                 0x00000000
#define LW_PBUS_SW_SCRATCH1_INFOROM_SEN_SMC_ENABLE                  0x00000001
#define LW_PBUS_SW_SCRATCH1_SMC_MODE                                15:15
#define LW_PBUS_SW_SCRATCH1_SMC_MODE_OFF                            0x00000000
#define LW_PBUS_SW_SCRATCH1_SMC_MODE_ON                             0x00000001

// scratch bits for preOS SMBPBI OOB Clock Limit Set
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK                          LW_PBUS_SW_SCRATCH(36)
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_MIN                      18:8
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_MIN_ILWALID              0x00000000
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_MIN_CLEAR                0x00007fff
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_MIN_OFFSET               0x0000012b
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_MAX                      29:19
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_MAX_ILWALID              0x00000000
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_MAX_CLEAR                0x00007fff
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_MAX_OFFSET               0x0000012b
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_LIMITS_PERSIST           30:30
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_LIMITS_PERSIST_NO        0x00000000
#define LW_PBUS_SW_SCRATCH36_SMBPBI_PREOS_OOB_CLOCK_LIMITS_PERSIST_YES       0x00000001

#endif // __ga102_dev_bus_addendum_h__
