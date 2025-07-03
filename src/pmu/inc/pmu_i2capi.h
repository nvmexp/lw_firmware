/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pmu_i2capi.h
 * @brief Defines used to communicate with the I2C submission library.
 */

#ifndef _PMU_I2CAPI_H
#define _PMU_I2CAPI_H

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
/*!
 * @brief   Message structure for PMU-internal tasks to perform I2C.
 *
 * The I2C task writes a message of this type into the event queue specified in
 * the call to @ref i2c upon completion of the request. See @ref i2c::hQueue
 * for additional information.
 */
typedef struct
{
    LwU8 errorCode;
    LwU8 pad[3];
} I2C_INT_MESSAGE;

/*!
 * This function is a core entry point to perform an I2C transaction.
 */
FLCN_STATUS i2c(LwU8 portId, LwU32 flags, LwU16 address, LwU32 index, LwU16 size, LwU8 *pMessage, LwrtosQueueHandle hQueue)
    GCC_ATTRIB_SECTION("imem_libi2c", "i2c");

#define i2cReadIndexed(p,f,a,i,s,m,q)  i2c(p, I2C_READ|(f), a, i, s, m, q)
#define i2cWriteIndexed(p,f,a,i,s,m,q) i2c(p, I2C_WRITE|(f), a, i, s, m, q)

#endif /* _PMU_I2CAPI_H */

