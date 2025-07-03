/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objmujtex.c
 * @brief  Container object for the HW Mutex routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objmutex.h"

#include "config/g_mutex_private.h"
#if(MUTEX_MOCK_FUNCTIONS_GENERATED)
#include "config/g_mutex_mock.c"
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- External definitions --------------------------- */
/* ------------------------- Global variables ------------------------------- */
OBJMUTEX Mutex;

/*!
 * Construct the Mutex object.  This sets up the HAL interface used by the Mutex
 * modul.  This also establishes the mapping between logical mutex IDs and
 * physical mutex IDs.
 */
FLCN_STATUS
constructMutex(void)
{
    mutexEstablishMapping_HAL(&Mutex);

    return FLCN_OK;
}

