/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_INTERRUPTS_H
#define FBFLCN_INTERRUPTS_H

#include <lwtypes.h>
#include "lwuproc.h"
#include "config/fbfalcon-config.h"

#include "dev_fbfalcon_csb.h"

#define HW_INTERRUPT_BIT_FBIF_ACK0 LW_CFBFALCON_FALCON_IRQSTAT_EXT_EXTIRQ1
#define HW_INTERRUPT_BIT_FBIF_ACK1 LW_CFBFALCON_FALCON_IRQSTAT_EXT_EXTIRQ2
#define HW_INTERRUPT_BIT_CMD_TRIGGER LW_CFBFALCON_FALCON_IRQSTAT_EXT_EXTIRQ3
#define HW_INTERRUPT_BIT_TIMER_TRIGGER LW_CFBFALCON_FALCON_IRQSTAT_EXT_EXTIRQ4
#define HW_INTERRUPT_BIT_MAILBOX_TRIGGER LW_CFBFALCON_FALCON_IRQSTAT_EXT_EXTIRQ5

#define MAILBOX0_TRIGGER_BIT_OK_TO_SWITCH 0



void FalconInterruptSetup(void)
    GCC_ATTRIB_SECTION("init", "FalconInterruptSetup");

void maskHaltInterrupt(void)
    GCC_ATTRIB_SECTION("resident", "maskHaltInterrupt");

void FalconInterruptEnablePeriodicTimer(void)
    GCC_ATTRIB_SECTION("resident", "FalconInterruptEnablePeriodicTimer");

void FalconInterruptDisablePeriodicTimer(void)
    GCC_ATTRIB_SECTION("resident", "FalconInterruptDisablePeriodicTimer");

#endif  // FBFLCN_INTERRUPTS_H
