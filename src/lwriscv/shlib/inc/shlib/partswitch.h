/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef SHLIB_PARTSWITCH_H
#define SHLIB_PARTSWITCH_H

#include <lwtypes.h>

#if LWRISCV_PARTITION_SWITCH
LwU64 partitionSwitch(LwU64 partition, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 arg5);

// Don't use function below, it's for internal use only.
extern LwU64 partitionSwitchKernel(LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 arg5, LwU64 partition);
#endif

#endif // PARTSWITCH_H
