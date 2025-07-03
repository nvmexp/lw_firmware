/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef I2C_OBJI2C_H
#define I2C_OBJI2C_H

/* ------------------------ Application includes --------------------------- */
#include "lib_intfci2c.h"
#include "config/g_i2c_hal.h"
#include "config/i2c-config.h"

/* ------------------------ External definitions --------------------------- */
/*
 * I2C Object Definition
*/
typedef struct
{
    I2C_HAL_IFACES    hal;
} OBJI2C;

extern OBJI2C I2c;

#endif // I2C_OBJI2C_H
