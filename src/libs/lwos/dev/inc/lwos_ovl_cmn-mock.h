/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_OVL_CMN_MOCK_H
#define LWOS_OVL_CMN_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "lwtypes.h"
#include "lwos_ovl_desc.h"
#include "fff.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @brief   Initial LWOS common OVL mocking
 */
void lwosOvlCmnMockInit(void);

/*!
 * @copydoc lwosTaskOverlayDescListExec
 *
 * @brief   Mock implementation of @ref lwosTaskOverlayDescListExec
 */
DECLARE_FAKE_VOID_FUNC(lwosTaskOverlayDescListExec_MOCK
    OSTASK_OVL_DESC *, LwU32, LwBool);

#endif // LWOS_OVL_CMN_MOCK_H
