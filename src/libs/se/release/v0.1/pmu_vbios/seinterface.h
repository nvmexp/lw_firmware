/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEINTERFACE_H
#define SEINTERFACE_H

#include "setypes.h"

/* ------------------------ Function Prototypes ---------------------------- */

SE_STATUS seRsaOperation(PSE_CRYPT_CONTEXT pCryptContext)
    __attribute__((section(".imem_libSE")));

#endif // SEINTERFACE_H

