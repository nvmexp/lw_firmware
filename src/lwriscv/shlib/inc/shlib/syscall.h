/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SYSCALL_H_
#define SYSCALL_H_
/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <lwctassert.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <portSyscall.h>

//
// List of application-specific syscalls
//

#define MAKE_APP_SYSCALL(X) (LW_SYSCALL_APPLICATION_START + X)

#define LW_APP_SYSCALL_PUTS                             MAKE_APP_SYSCALL(0)
#define LW_APP_SYSCALL_CMDQ_REGISTER_NOTIFIER           MAKE_APP_SYSCALL(1)
#define LW_APP_SYSCALL_CMDQ_UNREGISTER_NOTIFIER         MAKE_APP_SYSCALL(2)
#define LW_APP_SYSCALL_CMDQ_ENABLE_NOTIFIER             MAKE_APP_SYSCALL(3)
#define LW_APP_SYSCALL_CMDQ_DISABLE_NOTIFIER            MAKE_APP_SYSCALL(4)
#define LW_APP_SYSCALL_FIRE_SWGEN                       MAKE_APP_SYSCALL(6)

#if SCHEDULER_ENABLED
#define LW_APP_SYSCALL_NOTIFY_REGISTER_NOTIFIER         MAKE_APP_SYSCALL(10)
#define LW_APP_SYSCALL_NOTIFY_UNREGISTER_NOTIFIER       MAKE_APP_SYSCALL(11)
#define LW_APP_SYSCALL_NOTIFY_PROCESSING_DONE           MAKE_APP_SYSCALL(12)
#define LW_APP_SYSCALL_NOTIFY_REQUEST_GROUP             MAKE_APP_SYSCALL(13)
#define LW_APP_SYSCALL_NOTIFY_RELEASE_GROUP             MAKE_APP_SYSCALL(14)
#define LW_APP_SYSCALL_NOTIFY_QUERY_GROUPS              MAKE_APP_SYSCALL(15)
#define LW_APP_SYSCALL_NOTIFY_QUERY_IRQ                 MAKE_APP_SYSCALL(16)
#define LW_APP_SYSCALL_NOTIFY_ENABLE_IRQ                MAKE_APP_SYSCALL(17)
#define LW_APP_SYSCALL_NOTIFY_DISABLE_IRQ               MAKE_APP_SYSCALL(18)
#define LW_APP_SYSCALL_NOTIFY_ACK_IRQ                   MAKE_APP_SYSCALL(19)
#endif

#define LW_APP_SYSCALL_SHUTDOWN                         MAKE_APP_SYSCALL(20)
#define LW_APP_SYSCALL_OOPS                             MAKE_APP_SYSCALL(22)
#define LW_APP_SYSCALL_FBHUB_GATED                      MAKE_APP_SYSCALL(23)
#define LW_APP_SYSCALL_FBHUB_UNGATED                    MAKE_APP_SYSCALL(24)
#define LW_APP_SYSCALL_ODP_PIN_SECTION                  MAKE_APP_SYSCALL(25)
#define LW_APP_SYSCALL_ODP_UNPIN_SECTION                MAKE_APP_SYSCALL(26)

#define LW_APP_SYSCALL_SET_TIMESTAMP                    MAKE_APP_SYSCALL(27)

#if LWRISCV_PARTITION_SWITCH
#define LW_APP_SYSCALL_PARTITION_SWITCH                 MAKE_APP_SYSCALL(28)
#endif // LWRISCV_PARTITION_SWITCH

#if LWRISCV_PRINT_RAW_MODE
#define LW_APP_SYSCALL_DBG_DISPATCH_RAW_DATA            MAKE_APP_SYSCALL(29)
#endif // LWRISCV_PRINT_RAW_MODE

#define LW_APP_SYSCALL_WFI                              MAKE_APP_SYSCALL(30)

#define LW_APP_SYSCALL_SET_VBIOS_MCLK                   MAKE_APP_SYSCALL(31)
#define LW_APP_SYSCALL_GET_VBIOS_MCLK                   MAKE_APP_SYSCALL(32)

/* Software interrupts */
#define SYS_INTR_SWGEN0   0
#define SYS_INTR_SWGEN1   1

#endif // SYSCALL_H_
