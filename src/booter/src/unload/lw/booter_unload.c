/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_unload.c
 */
//
// Includes
//
#include "booter.h"

/*!
 * @brief Function to De-Initialize Booter
 */
BOOTER_STATUS booterDeInit(void)
{
    return booterUnload_HAL(pBooter);
}

