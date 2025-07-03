/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MCPDRF_H
#define INCLUDED_MCPDRF_H

// Define macros similar to lw drf, but ignore the LW prefix and simplify them for register
// fields only.
// These bit filed related macro could be used for register as well as hw data structures
// rf: register field (in format of "high(extent):low(base)")
// c: the sufix of the value of the register filed (in format of "_#####")
// n: value of the bit field, pre-shifted
// v: value (the number in the proper bit filed)

#define RF_BASE(rf)             (0?rf)
#define RF_EXTENT(rf)           (1?rf)
#define RF_SHIFT(rf)            ((0?rf)&0xffffffff)
#define RF_MASK(rf)             (0xffffffff>>(31-((1?rf)&0x1f)+((0?rf)&0x1f)))
#define RF_SHIFTMASK(rf)        (RF_MASK(rf)<<RF_SHIFT(rf))
#define RF_SIZE(rf)             (RF_EXTENT(rf)-RF_BASE(rf)+1)

#define RF_DEF(rf,c)            ((rf##c)<<RF_SHIFT(rf))
#define RF_NUM(rf,n)            (((n)&RF_MASK(rf))<<RF_SHIFT(rf))
#define RF_VAL(rf,v)            (((v)>>RF_SHIFT(rf))&RF_MASK(rf))

/*
Note that the following define cause compliation error.
#define FLD_SET_RF_DEF(rf,c,v)  (v=((v & ~RF_SHIFTMASK(rf)) | RF_DEF(rf,c)))
Work around with the following, by use the expansion of RF_DEF(rf, c)
*/
#define FLD_SET_RF_DEF(rf,c,v)  ((v & ~RF_SHIFTMASK(rf)) | ((rf##c)<<RF_SHIFT(rf)))
#define FLD_SET_RF_NUM(rf,n,v)  ((v & ~RF_SHIFTMASK(rf)) | RF_NUM(rf,n))
#define FLD_TEST_RF_DEF(rf,c,v) ((RF_VAL(rf,v) == rf##c))
#define FLD_TEST_RF_NUM(rf,n,v) ((RF_VAL(rf,v) == n))

#define RF_SET(rf)              RF_SHIFTMASK(rf)
#endif // INCLUDED_MCPDRF_H

