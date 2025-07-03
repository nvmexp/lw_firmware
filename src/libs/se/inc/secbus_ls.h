/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SECBUS_LS_H
#define SECBUS_LS_H

#include "setypes.h"

/* ------------------------ Defines ---------------------------------------- */
#define SE_SECBUS_REG_RD32_ERRCHK(addr, pData)           seSelwreBusReadRegisterErrChk(addr, pData)
#define SE_SECBUS_REG_WR32_ERRCHK(addr, data)            seSelwreBusWriteRegisterErrChk(addr, data)

/* ------------------------ Function Prototypes ---------------------------- */
// Secure bus support
SE_STATUS seSelwreBusWriteRegisterErrChk(LwU32 addr, LwU32 val)
    GCC_ATTRIB_SECTION("imem_libSE", "seSelwreBusWriteRegisterErrChk");
SE_STATUS seSelwreBusReadRegisterErrChk(LwU32 addr, LwU32 *pData)
    GCC_ATTRIB_SECTION("imem_libSE", "seSelwreBusReadRegisterErrChk");


#endif // SECBUS_LS_H
