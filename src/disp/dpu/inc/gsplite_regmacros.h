/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef GSPLITE_REGMACROS_H
#define GSPLITE_REGMACROS_H

#ifdef GSPLITE_RTOS
#include "regmacros.h"

// Include falcon manual
#include "dev_gsp_csb.h"
#include "dev_gsp_csb_addendum.h"
#include "config/g_dpu_hal.h"

//
// Defining macros
//

//////////////////////////////// PLEASE READ ////////////////////////////////
// Coverity detects defect when the stx_i or ldx_i instruction is used in  //
// the CSB access (Coverity thinks these instructions can return certain   //
// error code which needs to be handled while it's not). So we will ignore //
// those defects if they are the same case here.                           //
//////////////////////////////// PLEASE READ ////////////////////////////////

//
// Macros used to access non LW_CGSP_* registers (via BAR0)
//
#define EXT_REG_RD32(r)                       dpuBar0RegRd32_HAL(&Dpu.hal, r)
#define EXT_REG_WR32(r,v)                     dpuBar0RegWr32NonPosted_HAL(&Dpu.hal, r,v)
#define EXT_REG_WR32_STALL(r,v)               dpuBar0RegWr32NonPosted_HAL(&Dpu.hal, r,v)
#define EXT_REG_RD32_ERRCHK(r,v)              dpuBar0ErrChkRegRd32_HAL(&Dpu.hal, r,v)
#define EXT_REG_WR32_ERRCHK(r,v)              dpuBar0ErrChkRegWr32NonPosted_HAL(&Dpu.hal, r,v)

//
// Macros used to access FALCON registers in GSPLITE
//
#define DFLCN_DRF_BASE()                      DRF_BASE(LW_CGSP_FALCON)
#define DFLCN_DRF(r)                          LW_CGSP_FALCON_##r
#define DFLCN_REG_RD32(p)                     REG_RD32(CSB, LW_CGSP_FALCON_ ## p)
#define DFLCN_REG_RD32_STALL(p)               REG_RD32_STALL(CSB, LW_CGSP_FALCON_ ## p)
#define DFLCN_REG_WR32(p,v)                   REG_WR32(CSB, LW_CGSP_FALCON_ ## p, v)
#define DFLCN_REG_WR32_STALL(p,v)             REG_WR32_STALL(CSB, LW_CGSP_FALCON_ ## p, v)
#define DFLCN_REG_IDX_RD_DRF(r,i,f)           REG_FLD_IDX_RD_DRF(CSB,_CGSP_FALCON_,r,i,f)
#define DFLCN_REG_RD_DRF(r,f)                 REG_FLD_RD_DRF(CSB,_CGSP_FALCON_,r,f)
#define DFLCN_REG_WR_NUM(r,f,n)               REG_FLD_WR_DRF_NUM(CSB,_CGSP_FALCON_,r,f,n)
#define DFLCN_REG_WR_NUM_STALL(r,f,n)         REG_FLD_WR_DRF_NUM_STALL(CSB,_CGSP_FALCON_,r,f,n)
#define DFLCN_REG_WR_DEF(r,f,v)               REG_FLD_WR_DRF_DEF(CSB, _CGSP_FALCON_,r,f,v)
#define DFLCN_REG_WR_DEF_STALL(r,f,v)         REG_FLD_WR_DRF_DEF_STALL(CSB, _CGSP_FALCON_,r,f,v)


#define DFLCN_DRF_DEF(r,f,c)                  DRF_DEF(_CGSP_FALCON_,r,f,c)
#define DFLCN_DRF_SHIFTMASK(rf)               DRF_SHIFTMASK(LW_CGSP_FALCON_##rf)
#define DFLCN_DRF_NUM(r,f,n)                  DRF_NUM(_CGSP_FALCON_,r,f,n)
#define DFLCN_DRF_VAL(r,f,v)                  DRF_VAL(_CGSP_FALCON_,r,f,v) 
#define DFLCN_FLD_TEST_DRF(r,f,c,v)           FLD_TEST_DRF(_CGSP_FALCON_,r,f,c,v) 
#define DFLCN_FLD_SET_DRF(r,f,c,v)            FLD_SET_DRF(_CGSP_FALCON_,r,f,c,v) 
#define DFLCN_FLD_SET_DRF_NUM(r,f,n,v)        FLD_SET_DRF_NUM(_CGSP_FALCON_,r,f,n,v) 


//
// Macros used to access FBIF registers in GSPLITE
//
#define DFBIF_REG_RD32(p)                     REG_RD32(CSB, LW_CGSP_FBIF_ ## p)
#define DFBIF_REG_WR32(p,v)                   REG_WR32(CSB, LW_CGSP_FBIF_ ## p, v)
#define DFBIF_REG_WR32_STALL(p,v)             REG_WR32_STALL(CSB, LW_CGSP_FBIF_ ## p, v)


#define DFBIF_DRF_DEF(r,f,c)                  DRF_DEF(_CGSP_FBIF_,r,f,c)
#define DFBIF_DRF_SHIFTMASK(rf)               DRF_SHIFTMASK(LW_CGSP_FBIF_##rf)
#define DFBIF_DRF_NUM(r,f,n)                  DRF_NUM(_CGSP_FBIF_,r,f,n)
#define DFBIF_DRF_VAL(r,f,v)                  DRF_VAL(_CGSP_FBIF_,r,f,v) 
#define DFBIF_FLD_TEST_DRF(r,f,c,v)           FLD_TEST_DRF(_CGSP_FBIF_,r,f,c,v) 
#define DFBIF_FLD_SET_DRF(r,f,c,v)            FLD_SET_DRF(_CGSP_FBIF_,r,f,c,v) 
#define DFBIF_FLD_SET_DRF_NUM(r,f,n,v)        FLD_SET_DRF_NUM(_CGSP_FBIF_,r,f,n,v) 
#define DFBIF_DRF_IDX_VAL(r,f,i,v)            DRF_IDX_VAL(_CGSP_FBIF_,r,f,i,v)
#define DFBIF_FLD_IDX_SET_DRF_NUM(r,f,i,n,v)  FLD_IDX_SET_DRF_NUM(_CGSP_FBIF_,r,f,i,n,v)

//
// Macros used to access engine registers in GSPLITE
//
#define DENG_DRF(r)                          LW_CGSP_##r
#define DENG_REG_RD32(p)                     REG_RD32(CSB, LW_CGSP_ ## p)
#define DENG_REG_WR32(p,v)                   REG_WR32(CSB, LW_CGSP_ ## p, v)
#define DENG_REG_WR32_STALL(p,v)             REG_WR32_STALL(CSB, LW_CGSP_ ## p, v)
#define DENG_REG_WR_DEF_STALL(f, p,v)        REG_FLD_WR_DRF_DEF_STALL(CSB, _CGSP_, f, p, v)


#define DENG_DRF_DEF(r,f,c)                  DRF_DEF(_CGSP_,r,f,c)
#define DENG_DRF_SHIFTMASK(rf)               DRF_SHIFTMASK(LW_CGSP_##rf)
#define DENG_DRF_NUM(r,f,n)                  DRF_NUM(_CGSP_,r,f,n)
#define DENG_DRF_VAL(r,f,v)                  DRF_VAL(_CGSP_,r,f,v) 
#define DENG_FLD_TEST_DRF(r,f,c,v)           FLD_TEST_DRF(_CGSP_,r,f,c,v) 
#define DENG_FLD_SET_DRF(r,f,c,v)            FLD_SET_DRF(_CGSP_,r,f,c,v) 
#define DENG_FLD_SET_DRF_NUM(r,f,n,v)        FLD_SET_DRF_NUM(_CGSP_,r,f,n,v) 
#define DENG_DRF_IDX_VAL(r,f,i,v)            DRF_IDX_VAL(_CGSP_,r,f,i,v)
#define DENG_FLD_IDX_SET_DRF_NUM(r,f,i,n,v)  FLD_IDX_SET_DRF_NUM(_CGSP_,r,f,i,n,v)

#endif
#endif // GSPLITE_REGMACROS_H
