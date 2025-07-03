/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_CSB_H
#define SEC2_CSB_H

/*!
 * @file sec2_csb.h
 */

#include "regmacros.h"

#define CSB_REG_RD32_ERRCHK(addr, pReadVal)        sec2CsbErrChkRegRd32_HAL(&Sec2, addr, pReadVal)
#define CSB_REG_WR32_ERRCHK(addr, data)            sec2CsbErrChkRegWr32NonPosted_HAL(&Sec2, addr, data)
#define CSB_REG_RD32_HS_ERRCHK(addr, pReadVal)     sec2CsbErrChkRegRd32Hs_HAL(&Sec2, addr, pReadVal)
#define CSB_REG_WR32_HS_ERRCHK(addr, data)         sec2CsbErrChkRegWr32NonPostedHs_HAL(&Sec2, addr, data)

#endif // SEC2_CSB_H

