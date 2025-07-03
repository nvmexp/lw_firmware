/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_H
#define CMDMGMT_H

/*!
 * @file cmdmgmt.h
 *
 * Provides the definitions and interfaces for all items related to command and
 * message queue management.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "g_cmdmgmt.h"

#ifndef G_CMDMGMT_H
#define G_CMDMGMT_H

/* ------------------------- Includes --------------------------------------- */
#include <lwtypes.h>
#include "cmdmgmt/cmdmgmt_rpc_impl.h"
#include "unit_api.h"
#include "lwostimer.h"
#include "rmflcncmdif.h"

/* ------------------------- External Definitions --------------------------- */

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * Dispatch structure for sending GPIO signals to the command dispatcher. For
 * this structure, 'disp2unitEvt' will always be EVT_SIGNAL.  The information
 * on the specific GPIO being dispatched is contained in 'gpioSignal'.
 */
typedef struct
{
    LwU8  disp2unitEvt;
    LwU8  gpioSignal;
} DISPATCH_CMDMGMT_SIGNAL_GPIO;

// possible dispatched GPIOs
#define DISPATCH_CMDMGMT_SIGNAL_GPIO_PBI                   (0x0)

/*!
 * This is the generic structure that may be used to send any type of event to
 * the command-dispatcher.  The type of event may be EVT_COMMAND or EVT_SIGNAL
 * and is stored in 'disp2unitEvt'.  EVT_COMMAND is used when the ISR detects
 * a command in the PMU command queues.  EVT_SIGNAL is used when the ISR gets
 * an interrupt it is not capable of handling.  No data is passed with the
 * EVT_COMMAND event.  For signals, the 'signal' structure may be used to
 * determine the signal type and signal attributes.
 */
typedef union
{
    LwU8                            disp2unitEvt;
    DISPATCH_CMDMGMT_SIGNAL_GPIO    signal;
} DISPATCH_CMDMGMT;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
void * cmdmgmtOffsetToPtr(LwU32 offset)
    GCC_ATTRIB_SECTION("imem_resident", "cmdmgmtRMHOffsetToPtr");

/* ------------------------- Global Variables ------------------------------ */

#endif // G_CMDMGMT_H
#endif // CMDMGMT_H
