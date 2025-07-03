/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_INTFCI2C_H
#define LIB_INTFCI2C_H

/*!
 * @file   lib_intfcI2C.h
 */

/* ------------------------ System includes -------------------------------- */
#include "lwuproc.h"
#include "flcncmn.h"
#include "flcntypes.h"
#include "osptimer.h"
#include "flcnifi2c.h"
#include "lwoslayer.h"
#include <string.h>

/* ------------------------ Application includes --------------------------- */
#include "lib_i2c.h"
#include "lib_swi2c.h"
#include "lib_hwi2c.h"

/* ------------------------- Macros and Defines ---------------------------- */
/* ------------------------ External Functions ----------------------------- */
extern LwBool libIntfcI2lwseSwI2c(void);
/* ------------------------ Public Functions ------------------------------- */
FLCN_STATUS i2cPerformTransaction(I2C_TRANSACTION *pTa)
    GCC_ATTRIB_SECTION("imem_libI2c", "i2cPerformTransaction");
FLCN_STATUS i2cPerformTransactiolwiaHw(I2C_TRANSACTION *pTa)
    GCC_ATTRIB_SECTION("imem_libI2cHw", "i2cPerformTransactiolwiaHw");
FLCN_STATUS i2cPerformTransactiolwiaSw(I2C_TRANSACTION* pTa)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "i2cPerformTransactiolwiaSw");
FLCN_STATUS i2cRecoverBusViaSw(I2C_TRANSACTION* pTa)
    GCC_ATTRIB_SECTION("imem_libI2cSw", "i2cRecoverBusViaSw");
FLCN_STATUS i2cPerformTransactiolwiaTegraHw(I2C_TRANSACTION *pTa);
FLCN_STATUS i2cTegreaHwInit(void);
#endif // LIB_INTFCI2C_H
