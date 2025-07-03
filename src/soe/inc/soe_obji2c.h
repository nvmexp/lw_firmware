/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJI2C_H
#define SOE_OBJI2C_H

/*!
 * @file    soe_obji2c.h 
 * @copydoc soe_obji2c.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_i2c_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern I2C_HAL_IFACES I2cHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
void constructI2c(void) GCC_ATTRIB_SECTION("imem_init", "constructI2c");
void i2cPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "i2cPreInit");

#endif // SOE_OBJI2C_H
