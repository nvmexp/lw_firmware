/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   selwrescrub_selwrescrubacr.c
 */

/* ------------------------ System includes ------------------------------- */
#include "selwrescrub.h"

/* ------------------------ Application includes --------------------------- */
#include "selwrescrub_objhal.h"
#include "selwrescrub_objselwrescrub.h"

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Global variables ------------------------------- */
OBJSELWRESCRUB Selwrescrub;

/*!
 * Construct the ACR object.  This sets up the HAL interface used by the ACR
 * module.
 */
void
constructSelwrescrub(void)
{
    IfaceSetup->selwrescrubHalIfacesSetupFn(&Selwrescrub.hal);
}

