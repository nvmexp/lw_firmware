/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SELWRESCRUBREG_H
#define SELWRESCRUBREG_H

#include <config/g_selwrescrub_private.h>
#include <lwtypes.h>
#include <lwmisc.h>

//
// BAR0/CSB register access macros
//
#define FALC_CSB_REG_RD32(addr)                         FALC_CSB_REG_RD32_STALL(addr)
#define FALC_CSB_REG_WR32(addr, data)                   FALC_CSB_REG_WR32_STALL(addr, data)
#define FALC_CSB_REG_RD32_STALL(addr)                   selwrescrubReadRegister_HAL(pSelwrecrub, addr)
#define FALC_CSB_REG_WR32_STALL(addr, data)             selwrescrubWriteRegister_HAL(pSelwrescrub, addr, data)
#define FALC_CSB_REG_WR32_INLINE(addr, data)            _selwrescrubWriteCsbInlineWithPriErrHandling(addr, data)
#define FALC_CSB_REG_RD32_INLINE(addr)                  _selwrescrubReadCsbInlineWithPriErrHandling(addr)

#define FALC_BAR0_REG_RD32(addr)                        selwrescrubReadBar0_HAL(pSelwrescrub, addr)
#define FALC_BAR0_REG_WR32(addr,data)                   selwrescrubWriteBar0_HAL(pSelwrescrub, addr, data)

//Macros for register accesses
#define FALC_REG_RD32(bus,addr)                         FALC_##bus##_REG_RD32(addr)
#define FALC_REG_WR32(bus,addr,data)                    FALC_##bus##_REG_WR32(addr, data)
#define FALC_REG_RD32_STALL(bus,addr)                   FALC_##bus##_REG_RD32_STALL(addr)
#define FALC_REG_WR32_STALL(bus,addr,data)              FALC_##bus##_REG_WR32_STALL(addr,data)

#define REG_RD_DRF(bus,d,r,f)                           (((FALC_REG_RD32(bus,LW ## d ## r))>>DRF_SHIFT(LW ## d ## r ## f))&DRF_MASK(LW ## d ## r ## f))
#define REG_WR_DRF_DEF(bus,d,r,f,c)                     FALC_REG_WR32(bus, LW ## d ## r, DRF_DEF(d,r,f,c))
#define REG_WR_DRF_NUM(bus,d,r,f,n)                     FALC_REG_WR32(bus, LW ## d ## r, DRF_NUM(d,r,f,n))
#define FLD_WR_DRF_DEF(bus,d,r,f,c)                     FALC_REG_WR32(bus,LW##d##r,(FALC_REG_RD32(bus,LW##d##r)&~(DRF_MASK(LW##d##r##f)<<DRF_SHIFT(LW##d##r##f)))|DRF_DEF(d,r,f,c))
#define FLD_TEST_DRF_DEF(bus,d,r,f,c)                   (REG_RD_DRF(bus, d, r, f) == LW##d##r##f##c)

// Additional DRF macros for indexed registers
#define REG_IDX_WR_DRF_NUM(bus,d,r,i,f,n)               FALC_REG_WR32(bus, LW ## d ## r(i), DRF_NUM(d,r,f,n))
#define REG_IDX_WR_DRF_DEF(bus,d,r,i,f,c)               FALC_REG_WR32(bus, LW ## d ## r(i), DRF_DEF(d,r,f,c))
#define REG_IDX_RD_DRF(bus,d,r,i,f)                     (((FALC_REG_RD32(bus, LW ## d ## r(i)))>>DRF_SHIFT(LW ## d ## r ## f))& DRF_MASK(LW ## d ## r ## f))

#define REGISTER_READ_BAD_RESULT                        (0xBAD00000)
#define CSB_INTERFACE_MASK_VALUE                        (0xFFFF0000)
#define CSB_INTERFACE_BAD_VALUE                         (0xBADF0000)
#endif //SELWRESCRUBREG_H
