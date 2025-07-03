/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lib_dpaux.h
 */

#ifndef LIB_DPAUX_H
#define LIB_DPAUX_H

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
/* ------------------------- Macros and Defines ---------------------------- */

/*!
 * Defines how frequently to check for an event in busy wait loop.
 */
#define DP_AUX_DELAY_US                         10

/*!
 * There can be a HPD IRQ in which HW reads 5 AUX reads during which any
 * transactions would be left pending, each transaction can be deferred 7
 * times and might pass on 8th time. Hence 5*8*1msec = 40msec.
 */
#define DP_AUX_WAIT_US                          (40 * 1000)

/*!
 * If the DisplayPort transmitter does not receive a reply from the DisplayPort
 * receiver it must wait for a Reply Time-out period of 400us before initiating
 * the next AUX CH Request transaction. The timer for the Reply Time-out starts
 * ticking after the DisplayPort transmitter has finished sending the AUX CH
 * STOP condition
 */
#define DP_AUX_REPLY_TIMEOUT_US                 400

#define DP_AUX_NUM_ATTEMPTS                     (DP_AUX_WAIT_US / DP_AUX_DELAY_US)

// Experimental value from DP HDCP22 CTS ( Unigraf ) continuous testing.
#define UPROC_DP_AUX_SEMA_ACQUIRE_TIMEOUT_US    (160000)
#define DP_AUX_SEMA_ACQUIRE_ATTEMPTS            (UPROC_DP_AUX_SEMA_ACQUIRE_TIMEOUT_US / DP_AUX_DELAY_US)

/*!
 * Defines how frequently to check aux state becomes idle.
 */
#define DP_AUX_WAIT_IDLE_DELAY_US_SHIFT         (4)
#define DP_AUX_WAIT_IDLE_DELAY_US               (1<<DP_AUX_WAIT_IDLE_DELAY_US_SHIFT)

/* ------------------------ Types definitions ------------------------------ */

/*! @brief Command structure for FLCN-internal tasks to perform DPAUX. */
typedef struct
{
    /*!
     * Physical DPAUX port.
     */
    LwU8            port;
    /*!
     * One of the DPAUX read/wr commands, eg. I2CRD/AUXWR.
     */
    LwU8            cmd;
    /*!
     * DPAUX addr to be accessed.
     */
    LwU32           addr;
    /*!
     * Size of DPAUX read/write. This field is in/out. Caller sets
     * it to number of bytes to read/write. Callee sets it to actual
     * number of bytes read/written.
     */
    LwU16            size;
    /*!
     * For reads points to buffer to receive data read.
     * For writes points to buffer that holds data to be written.
     */
    LwU8            *pData;
} DPAUX_CMD;

#endif // LIB_DPAUX_H
