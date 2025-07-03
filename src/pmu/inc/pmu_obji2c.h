/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJI2C_H
#define PMU_OBJI2C_H

/*!
 * @file pmu_obji2c.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */

/*! @brief Internal Command structure for HW I2C to perform I2C transaction */
typedef struct
{
    LwU32       portId;
    LwU32       bRead;
    LwU32       cntl;
    LwU32       data;
    LwU32       bytesRemaining;
    LwU8       *pMessage;
    LwBool      bBlockProtocol;
    FLCN_STATUS status;
    LwBool      bStopRequired;
} I2C_HW_CMD;

/*!
 * @brief  Data structure used to communicate between the I2C management
 *         subsystem and the I2C transaction subsystems without revealing too
 *         much detail as to how the commands are structured.
 */
typedef struct
{
    RM_PMU_I2C_CMD_GENERIC *pGenCmd; //!< The generic, shared command data.
    LwBool bRead;           //!< Whether thr transaction ilwolves reading.
    LwU8   segment;         //!< EDDC segment value, used only in EDID read
    LwU16  messageLength;   //!< The length of the message.
    LwU8  *pMessage;        //!< The message (required.) Input or output.
} I2C_PMU_TRANSACTION;

#include "config/g_i2c_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
#define MAX_BR04_I2C_PORTS    1
#define MAX_GPU_I2C_PORTS     15
#define MAX_I2C_PORTS         (MAX_GPU_I2C_PORTS+MAX_BR04_I2C_PORTS)

/* ------------------------ External Definitions --------------------------- */
/*!
 * I2C object Definition
 */
typedef struct
{
    LwU8    dummy;    // unused -- for compilation purposes only
} OBJI2C;

extern OBJI2C I2c;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

#endif // PMU_OBJI2C_H

