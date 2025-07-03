/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_CSB_H
#define SOE_CSB_H

/*!
 * @file soe_csb.h
 */

#include "regmacros.h"

#define CSB_REG_RD32_ERRCHK(addr, pReadVal)        soeCsbErrChkRegRd32_HAL(&Soe, addr, pReadVal)
#define CSB_REG_WR32_ERRCHK(addr, data)            soeCsbErrChkRegWr32NonPosted_HAL(&Soe, addr, data)
#define CSB_REG_RD32_HS_ERRCHK(addr, pReadVal)     soeCsbErrChkRegRd32Hs_HAL(&Soe, addr, pReadVal)
#define CSB_REG_WR32_HS_ERRCHK(addr, data)         soeCsbErrChkRegWr32NonPostedHs_HAL(&Soe, addr, data)

#endif // SOE_CSB_H

