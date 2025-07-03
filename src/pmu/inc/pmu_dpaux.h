/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dpaux.h
 */

#ifndef PMU_DPAUX_H
#define PMU_DPAUX_H

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "unit_api.h"

/* ------------------------- Macros and Defines ---------------------------- */

/*!
 * Defines how frequently to check for an event in busy wait loop.
 */
#define DP_AUX_DELAY_US                 10

/*!
 * There can be a HPD IRQ in which HW reads 5 AUX reads during which any
 * transactions would be left pending, each transaction can be deferred 7
 * times and might pass on 8th time. Hence 5*8*1msec = 40msec.
 */
#define DP_AUX_WAIT_US                  (40 * 1000)

/*!
 * If the DisplayPort transmitter does not receive a reply from the DisplayPort
 * receiver it must wait for a Reply Time-out period of 400us before initiating
 * the next AUX CH Request transaction. The timer for the Reply Time-out starts
 * ticking after the DisplayPort transmitter has finished sending the AUX CH
 * STOP condition
 */
#define DP_AUX_REPLY_TIMEOUT_US         400

#define DP_AUX_NUM_ATTEMPTS             (DP_AUX_WAIT_US / DP_AUX_DELAY_US)
#define DP_AUX_SEMA_ACQUIRE_ATTEMPTS    (DP_AUX_SEMA_ACQUIRE_TIMEOUT / DP_AUX_DELAY_US)

/* ------------------------ Types definitions ------------------------------ */

/*! @brief Command structure for PMU-internal tasks to perform DPAUX. */
typedef struct
{
    DISP2UNIT_HDR       hdr;    //<! Must be first element of the structure
                                //<! Must be set to I2C_EVENT_TYPE_DPAUX

    /*!
     * Physical DPAUX port.
     */
    LwU8                port;
    /*!
     * One of the DPAUX read/wr commands, eg. I2CRD/AUXWR.
     */
    LwU8                cmd;
    /*!
     * DPAUX addr to be accessed.
     */
    LwU32               addr;
    /*!
     * Size of DPAUX read/write. This field is in/out. Caller sets
     * it to number of bytes to read/write. Callee sets it to actual
     * number of bytes read/written.
     */
    LwU8                size;
    /*!
     * For reads points to buffer to receive data read.
     * For writes points to buffer that holds data to be written.
     */
    LwU8               *pData;
    /*!
     * Queue handle provided by the caller. The i2c task sends the status
     * message by writing it to this queue.
     */
    LwrtosQueueHandle   hQueue;
} DPAUX_CMD;

/* ------------------------ Function Prototypes ---------------------------- */

void    dpauxCommand(DPAUX_CMD *pDpAuxCmd);
LwBool  dpauxIsHpdStatusPlugged(void);

#endif // PMU_DPAUX_H
