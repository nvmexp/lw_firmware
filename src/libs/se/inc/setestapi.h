/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SETESTAPI_H
#define SETESTAPI_H


/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ System includes -------------------------------- */
#include "setypes.h"

//
// NOTE NOTE NOTE NOTE
// Bug 1932533 File to remove this.  SE test will also be removed and new SE test written.
//
// SE Test RSA functions
//
SE_STATUS seSampleRsaCode(void)
    GCC_ATTRIB_SECTION("imem_libSE", "seSampleRsaCode");

#endif // SETESTAPI_H
