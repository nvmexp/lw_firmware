/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_libintfci2c.c
 * @brief  Container-object for DPU i2c routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
/* ------------------------ Application includes --------------------------- */
#include "lwdpu.h"
#include "dpu_objdpu.h"
#include "lib_intfci2c.h"
#include "dpu_gpuarch.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function Definitions ------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief This is an interface function for i2c library, to decide whether
 *        to use swi2c or not.
 *
 * @return LwBool         returns LW_TRUE if SW I2c needs to be used.
 *
 * @sa libIntfcI2lwseSwI2c
 */
LwBool
libIntfcI2lwseSwI2c(void)
{

#if (DPU_PROFILE_v0207)
    if (!DISP_IP_VER_GREATER_OR_EQUAL(DISP_VERSION_V02_08))
    {
        return LW_TRUE;
    }
#endif

    return LW_FALSE;
}
