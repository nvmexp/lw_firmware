/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_AUTH_H
#define PMU_AUTH_H

#include "flcntypes.h"

#define SRM_MODULUS_SIZE    128
#define SRM_DIVISOR_SIZE    20
#define SRM_GENERATOR_SIZE  128
#define SRM_PUBLIC_KEY_SIZE 128

/* Function Prototypes */
FLCN_STATUS hdcpValidateSrm(RM_FLCN_MEM_DESC *, LwU32);
FLCN_STATUS hdcpValidateRevocationList(RM_FLCN_MEM_DESC *, LwU32, RM_FLCN_MEM_DESC *, LwU32);
void hdcpCallwlateV(LwU64, LwU16, LwU32, RM_FLCN_MEM_DESC *, LwU8, LwU8 *);

#endif // PMU_AUTH_H
