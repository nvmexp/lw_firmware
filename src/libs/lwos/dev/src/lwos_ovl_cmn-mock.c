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
 * @file   lwos_ovl_cmn-mock.c
 * @brief  Mock implementations of the common overlay routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwos_ovl_desc.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Mocked Functions ------------------------------- */
DEFINE_FAKE_VOID_FUNC(lwosTaskOverlayDescListExec_MOCK
    OSTASK_OVL_DESC *, LwU32, LwBool);

/* ------------------------- Public Functions ------------------------------- */
void
lwosOvlCmnMockInit(void)
{
    RESET_FAKE(lwosTaskOverlayDescListExec_MOCK);
}

/* ------------------------- Static Functions  ------------------------------ */
