/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SECBUS_HS_H
#define SECBUS_HS_H

#include "setypes.h"

/* ------------------------ Defines ---------------------------------------- */
#define SE_SECBUS_REG_RD32_HS_ERRCHK(addr, pData)        seSelwreBusReadRegisterErrChkHs(addr, pData)
#define SE_SECBUS_REG_WR32_HS_ERRCHK(addr, data)         seSelwreBusWriteRegisterErrChkHs(addr, data)


/* ------------------------ Function Prototypes ---------------------------- */
// Heavy Secure Secure bus support
SE_STATUS seSelwreBusWriteRegisterErrChkHs(LwU32, LwU32)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seSelwreBusWriteRegisterErrChkHs");
SE_STATUS seSelwreBusReadRegisterErrChkHs(LwU32, LwU32*)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seSelwreBusReadRegisterErrChkHs");


#endif // SECBUS_HS_H
