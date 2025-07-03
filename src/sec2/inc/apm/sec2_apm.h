/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SEC2_APM_H
#define SEC2_APM_H

/* ------------------------ System includes -------------------------------- */

typedef union
{
    LwU8 eventType;
} DISPATCH_APM;

LwBool apmCaptureDynamicState(LwU64)
    GCC_ATTRIB_SECTION("imem_apm", "apmCaptureDynamicState");

FLCN_STATUS apmMsrExtend(LwU64, LwU32, LwU8 *, LwBool)
    GCC_ATTRIB_SECTION("imem_apm", "apmMsrExtend");

#endif // SEC2_APM_H
