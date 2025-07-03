/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ACRDRFMACROS_H
#define ACRDRFMACROS_H

#include "acrutils.h"

// Additional DRF macros
#define REG_RD_DRF(bus,d,r,f)                       (((ACR_REG_RD32(bus,LW ## d ## r))>>DRF_SHIFT(LW ## d ## r ## f))&DRF_MASK(LW ## d ## r ## f))
#define REG_WR_DRF_NUM(bus,d,r,f,n)                 ACR_REG_WR32(bus, LW ## d ## r, DRF_NUM(d,r,f,n))
#define FLD_WR_DRF_DEF(bus,d,r,f,c)                 ACR_REG_WR32(bus,LW##d##r,(ACR_REG_RD32(bus,LW##d##r)&~(DRF_MASK(LW##d##r##f)<<DRF_SHIFT(LW##d##r##f)))|DRF_DEF(d,r,f,c))
#define FLD_TEST_DRF_DEF(bus,d,r,f,c)               (REG_RD_DRF(bus, d, r, f) == LW##d##r##f##c)

// Additional DRF macros for indexed registers
#define REG_IDX_WR_DRF_NUM(bus,d,r,i,f,n)           ACR_REG_WR32(bus, LW ## d ## r(i), DRF_NUM(d,r,f,n))
#define REG_IDX_WR_DRF_DEF(bus,d,r,i,f,c)           ACR_REG_WR32(bus, LW ## d ## r(i), DRF_DEF(d,r,f,c))
#define REG_IDX_RD_DRF(bus,d,r,i,f)                 (((ACR_REG_RD32(bus, LW ## d ## r(i)))>>DRF_SHIFT(LW ## d ## r ## f))& DRF_MASK(LW ## d ## r ## f))



#endif //ACRDRFMACROS_H
