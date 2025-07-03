/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objfb.c
 * @brief  OBJFB constructor
 *
 * Constructs FB hal interface
 *
 * @section _pmu_objfb OBJFB Notes
 * Sequencer task is the only client who calls fb now, so all fb related
 * functions are called in sequencer task.
 * @endsection
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objfb.h"

#include "config/g_fb_private.h"
#if(FB_MOCK_FUNCTIONS_GENERATED)
#include "config/g_fb_mock.c"
#endif

/* ------------------------- Global Variables ------------------------------- */
OBJFB Fb;

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS
constructFb_IMPL(void)
{
    return FLCN_OK;
}
