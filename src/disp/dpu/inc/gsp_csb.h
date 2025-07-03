/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef GSP_CSB_H
#define GSP_CSB_H

/*!
 * @file gsp_csb.h
 */

#include "regmacros.h"

#define CSB_REG_RD32_ERRCHK(addr, pReadVal)        dpuCsbErrChkRegRd32_HAL(&Dpu.hal, addr, pReadVal)
#define CSB_REG_WR32_ERRCHK(addr, data)            dpuCsbErrChkRegWr32NonPosted_HAL(&Dpu.hal, addr, data)
#define CSB_REG_RD32_HS_ERRCHK(addr, pReadVal)     dpuCsbErrChkRegRd32Hs_HAL(&Dpu.hal, addr, pReadVal)
#define CSB_REG_WR32_HS_ERRCHK(addr, data)         dpuCsbErrChkRegWr32NonPostedHs_HAL(&Dpu.hal, addr, data)

#endif // GSP_CSB_H

