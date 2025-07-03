/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef GSP_PARTSWITCH_H
#define GSP_PARTSWITCH_H

#if LWRISCV_PARTITION_SWITCH
//
// The header will be included for both RTOS, FMC build thus cannot include 
// specific profile configuration.
// 
/* ------------------------ System includes --------------------------------- */
#ifndef __ASSEMBLER__
#include <lwtypes.h>

/* ------------------------ Types definitions ------------------------------- */
#define PARTITION_RTOS_ID                       0
#define PARTITION_HDCP_SELWREACTION_ID          1

#if PARTITION_SWITCH_TEST
struct switch_time_measurement 
{
    LwU64 time;
    LwU64 instret;
    LwU64 cycle;
};
#endif // PARTITION_SWITCH_TEST

typedef enum
{
#if PARTITION_SWITCH_TEST
    PARTSWITCH_FUNC_MEASURE_SWITCHING_TIME  = 0x8781,
#endif // PARTITION_SWITCH_TEST
    PARTSWITCH_FUNC_HDCP_SELWREACTION       = 0x5432,
} PARTSWITCH_FUNC_ENUM;

#if PARTITION_SWITCH_TEST
// Implemented in test task, but shared with kernel
LwBool partitionSwitchTest(void);
#endif // PARTITION_SWITCH_TEST

#endif // __ASSEMBLER__

#define PARTSWITCH_SHARED_CARVEOUT_VA           0x40000
#define PARTSWITCH_SHARED_CARVEOUT_PA           (LW_RISCV_AMAP_DMEM_START + 0x1000)
#define PARTSWITCH_SHARED_CARVEOUT_SIZE         0x1000
#endif // LWRISCV_PARTITION_SWITCH

#endif // GSP_PARTSWITCH_H

