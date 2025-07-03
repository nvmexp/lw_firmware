/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACR_RISCV_STUBS_H
#define ACR_RISCV_STUBS_H

 __attribute__((unused))
static inline ACR_STATUS   acrScpGetRandomNumber_GH100(LwU32 *pRand32)
{
    return ACR_OK;
}

#endif // ACR_RISCV_STUBS_H
