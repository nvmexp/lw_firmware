/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dio.c
 * @brief  DIO Interface Functions
 *
 * DIO Read Write Functions
 */

/* ------------------------ System includes -------------------------------- */
#include "gspsw.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_dio_hal.h"
#include "config/g_profile.h"
#include "config/g_sections_riscv.h"
#include "config/gsp-config.h"
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * @brief   DIO Register read to SNIC
 *
 * @In      address  Address of the register to be read
 *
 * @out     pData    Buffer to hold read data
 * @return  Void
 */
sysSYSLIB_CODE FLCN_STATUS dioReadReg(LwU32 address, LwU32 *pData)
{
    return dioReadReg_HAL(&dioHal, address, pData);
}


/*!
 * @brief   DIO Register write to SNIC
 *
 * @In      address  Address of the register to be written
 * @In      data     Data to be written
 *
 * @return  Void
 */
sysSYSLIB_CODE FLCN_STATUS dioWriteReg(LwU32 address, LwU32 data)
{
    return dioWriteReg_HAL(&dioHal, address, data);
}

