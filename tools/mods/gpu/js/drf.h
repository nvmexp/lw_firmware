/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_DRF_H
#define INCLUDED_DRF_H

//------------------------------------------------------------------------------
// Register access functions.
//------------------------------------------------------------------------------


#ifndef BIT
#define BIT(b)                  (1U<<(b))
#endif
#define DEVICE_BASE(d)          (0?d)
#define DEVICE_EXTENT(d)        (1?d)
#define DRF_BASE(drf)           (0?drf)
#define DRF_EXTENT(drf)         (1?drf)
#define DRF_SHIFT(drf)          ((0?drf) % 32)
#define DRF_MASK(drf)           (0xFFFFFFFF>>>(31-((1?drf) % 32)+((0?drf) % 32)))
#define DRF_SHIFTMASK(drf)      (DRF_MASK(drf)<<(DRF_SHIFT(drf)))
#define DRF_SIZE(drf)           (DRF_EXTENT(drf)-DRF_BASE(drf)+1)
#define DRF_DEF(d,r,f,c)        ((LW ## d ## r ## f ## c)<<DRF_SHIFT(LW ## d ## r ## f))
#define DRF_NUM(d,r,f,n)        (((n)&DRF_MASK(LW ## d ## r ## f))<<DRF_SHIFT(LW ## d ## r ## f))
#define DRF_VAL(d,r,f,v)        (((v)>>>DRF_SHIFT(LW ## d ## r ## f))&DRF_MASK(LW ## d ## r ## f))

#define REF_DEF(drf,d)          (((drf ## d)&DRF_MASK(drf))<<DRF_SHIFT(drf))
#define REF_VAL(drf,v)          (((v)>>>DRF_SHIFT(drf))&DRF_MASK(drf))
#define REF_NUM(drf,n)          (((n)&DRF_MASK(drf))<<DRF_SHIFT(drf))
#define FLD_SET_DRF(d,r,f,c,v)  ((v & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_NUM(d,r,f,LW##d##r##f##c))
#define FLD_TEST_DRF(d,r,f,c,v) ((DRF_VAL(d, r, f, v) == LW##d##r##f##c))

#define FLD_SET_DRF_NUM(d,r,f,n,v)    ((v & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_NUM(d,r,f,n))
#define FLD_SET_DRF_DEF(d,r,f,c,v)    ((v) = ((v) & ~DRF_SHIFTMASK(LW##d##r##f)) | DRF_DEF(d,r,f,c))

#define DRF_IDX_DEF(d,r,f,i,c)        ((LW ## d ## r ## f ## c)<<DRF_SHIFT(LW##d##r##f(i)))
#define DRF_IDX_NUM(d,r,f,i,n)        (((n)&DRF_MASK(LW##d##r##f(i)))<<DRF_SHIFT(LW##d##r##f(i)))
#define DRF_IDX_VAL(d,r,f,i,v)        (((v)>>>DRF_SHIFT(LW##d##r##f(i)))&DRF_MASK(LW##d##r##f(i)))
#define FLD_IDX_TEST_DRF(d,r,f,i,c,v) ((DRF_IDX_VAL(d, r, f, i, v) == LW##d##r##f##c))

#define FLD_TEST_DRF_IDX(d,r,f,c,i,v) ((DRF_VAL(d, r, f, v) == LW##d##r##f##c(i)))

#define SUBDEV_REG_WR_DRF_NUM(s, d,r,f,n) \
   (s.RegWr32( LW##d##r, DRF_NUM(d,r,f,n) ))

#define SUBDEV_REG_WR_DRF_DEF(s,d,r,f,c) \
   (s.RegWr32( LW##d##r, DRF_DEF(d,r,f,c) ))

#define SUBDEV_FLD_WR_DRF_NUM(s,d,r,f,n)                        \
   (s.RegWr32( LW##d##r,                                        \
      ( s.RegRd32(LW##d##r)                                     \
         & ~(DRF_MASK(LW##d##r##f) << DRF_SHIFT(LW##d##r##f)) ) \
      | DRF_NUM(d,r,f,n) ))

#define SUBDEV_FLD_WR_DRF_DEF(s,d,r,f,c)                        \
   (s.RegWr32( LW##d##r,                                        \
      ( s.RegRd32( LW##d##r)                                    \
         & ~(DRF_MASK(LW##d##r##f) << DRF_SHIFT(LW##d##r##f)) ) \
      | DRF_DEF(d,r,f,c) ))

#define SUBDEV_FLD_WR_DRF_DEF_RAM(s,i,d,r,f,c)                  \
   (s.RegWr32( LW ## d ## r(i),                                 \
      ( s.RegRd32(LW##d##r(i))                                  \
         & ~(DRF_MASK(LW##d##r##f) << DRF_SHIFT(LW##d##r##f)) ) \
      | DRF_DEF(d,r,f,c) ))

#define SUBDEV_REG_RD_DRF(s,d,r,f) \
   ( (s.RegRd32(LW##d##r) >>> DRF_SHIFT(LW##d##r##f)) & DRF_MASK(LW##d##r##f) )

//------------------------------------------------------------------------------
// Frame buffer access functions.
//------------------------------------------------------------------------------

#define FB_RD08(a)   ( Gpu.FbRd08(a)   )
#define FB_RD16(a)   ( Gpu.FbRd16(a)   )
#define FB_RD32(a)   ( Gpu.FbRd32(a)   )

#define FB_WR08(a,d) ( Gpu.FbWr08(a,d) )
#define FB_WR16(a,d) ( Gpu.FbWr16(a,d) )
#define FB_WR32(a,d) ( Gpu.FbWr32(a,d) )

//------------------------------------------------------------------------------
// Deprecated register access macros.
//------------------------------------------------------------------------------
#define REG_RD08(a)   ( Gpu.RegRd08(a)   )
#define REG_RD16(a)   ( Gpu.RegRd16(a)   )
#define REG_RD32(a)   ( Gpu.RegRd32(a)   )

#define REG_WR08(a,d) ( Gpu.RegWr08(a,d) )
#define REG_WR16(a,d) ( Gpu.RegWr16(a,d) )
#define REG_WR32(a,d) ( Gpu.RegWr32(a,d) )

#define REG_WR_DRF_NUM(d,r,f,n) \
   REG_WR32( LW##d##r, DRF_NUM(d,r,f,n) )

#define REG_WR_DRF_DEF(d,r,f,c) \
   REG_WR32( LW##d##r, DRF_DEF(d,r,f,c) )

#define FLD_WR_DRF_NUM(d,r,f,n)                                 \
   REG_WR32( LW##d##r,                                          \
      ( REG_RD32(LW##d##r)                                      \
         & ~(DRF_MASK(LW##d##r##f) << DRF_SHIFT(LW##d##r##f)) ) \
      | DRF_NUM(d,r,f,n) )

#define FLD_WR_DRF_DEF(d,r,f,c)                                 \
   REG_WR32( LW##d##r,                                          \
      ( REG_RD32( LW##d##r)                                     \
         & ~(DRF_MASK(LW##d##r##f) << DRF_SHIFT(LW##d##r##f)) ) \
      | DRF_DEF(d,r,f,c) )

#define FLD_WR_DRF_DEF_RAM(i,d,r,f,c)                           \
   REG_WR32( LW ## d ## r(i),                                   \
      ( REG_RD32(LW##d##r(i))                                   \
         & ~(DRF_MASK(LW##d##r##f) << DRF_SHIFT(LW##d##r##f)) ) \
      | DRF_DEF(d,r,f,c) )

#define REG_RD_DRF(d,r,f) \
   ( (REG_RD32(LW##d##r) >>> DRF_SHIFT(LW##d##r##f)) & DRF_MASK(LW##d##r##f) )

#define FLD_DRF_NUM(o,d,r,f,n) FLD_SET_DRF_NUM(d,r,f,n,o)
#define FLD_DRF_DEF(o,d,r,f,c) FLD_SET_DRF(d,r,f,c,o)

#endif // !INCLUDED_DRF_H
