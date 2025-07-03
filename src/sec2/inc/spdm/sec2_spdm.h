/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SEC2_SPDM_H
#define SEC2_SPDM_H

/* ------------------------ System includes -------------------------------- */
#include "unit_dispatch.h"

/* ------------------------ Types definitions ------------------------------ */
typedef union
{
    LwU8          eventType;
    DISP2UNIT_CMD cmd;
} DISPATCH_SPDM;

#endif // SEC2_SPDM_H
