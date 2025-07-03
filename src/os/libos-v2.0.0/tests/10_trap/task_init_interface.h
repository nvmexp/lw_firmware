/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

#define TASK_INIT_UTIL_SELWRITY_VIOLATION_TYPE_OTHER                   0
#define TASK_INIT_UTIL_SELWRITY_VIOLATION_TYPE_STACK_CANARY_CORRUPTED  1

typedef struct
{
    LwU8  command;
    LwU8  type;
} selwrity_violation_args;

#define TASK_INIT_UTIL_CMD_STATE              1
#define TASK_INIT_UTIL_CMD_STACK              2
#define TASK_INIT_UTIL_CMD_STATE_AND_STACK    3
#define TASK_INIT_UTIL_CMD_SELWRITY_VIOLATION 4

typedef union
{
    LwU8                    command;
    selwrity_violation_args secArgs;
    LwU64                   padding;
} task_init_util_payload;
