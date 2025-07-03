pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with Interfaces.C.Extensions;
with System;

package lwtypes_h is

   LWRM_64 : constant := 1;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:66

   LWRM_TRUE64 : constant := 1;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:68
   --  arg-macro: function LwU16_HI08 (n)
   --    return (LwU8)(((LwU16)(n)) >> 8);
   --  arg-macro: function LwU16_LO08 (n)
   --    return (LwU8)((LwU16)(n));
   --  arg-macro: function LwU16_BUILD (msb, lsb)
   --    return ((msb) << 8)or(lsb);

   -- LwU64_fmtX : aliased constant String := "llX" & ASCII.NUL;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:206
   LwU64_fmtx : aliased constant String := "llx" & ASCII.NUL;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:207
   LwU64_fmtu : aliased constant String := "llu" & ASCII.NUL;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:208
   LwU64_fmto : aliased constant String := "llo" & ASCII.NUL;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:209
   LwS64_fmtd : aliased constant String := "lld" & ASCII.NUL;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:210
   LwS64_fmti : aliased constant String := "lli" & ASCII.NUL;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:211
   --  unsupported macro: LW_TRUE ((LwBool)(0 == 0))
   --  unsupported macro: LW_FALSE ((LwBool)(0 != 0))
   --  unsupported macro: LW_TRISTATE_FALSE ((LwTristate) 0)
   --  unsupported macro: LW_TRISTATE_TRUE ((LwTristate) 1)
   --  unsupported macro: LW_TRISTATE_INDETERMINATE ((LwTristate) 2)
   --  arg-macro: function LwU64_HI32 (n)
   --    return (LwU32)((((LwU64)(n)) >> 32) and 16#ffffffff#);
   --  arg-macro: function LwU64_LO32 (n)
   --    return (LwU32)(( (LwU64)(n)) and 16#ffffffff#);
   --  arg-macro: function LwU40_HI32 (n)
   --    return (LwU32)((((LwU64)(n)) >> 8) and 16#ffffffff#);
   --  arg-macro: function LwU40_HI24of32 (n)
   --    return (LwU32)( (LwU64)(n) and 16#ffffff00#);
   --  arg-macro: function LwU32_HI16 (n)
   --    return (LwU16)((((LwU32)(n)) >> 16) and 16#ffff#);
   --  arg-macro: function LwU32_LO16 (n)
   --    return (LwU16)(( (LwU32)(n)) and 16#ffff#);
   --  arg-macro: function LwP64_VALUE (n)
   --    return n;

   LwP64_fmt : aliased constant String := "%p" & ASCII.NUL;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:286
   --  arg-macro: function KERNEL_POINTER_FROM_LwP64 (p, v)
   --    return (p)(v);
   --  arg-macro: function LwP64_PLUS_OFFSET (p, o)
   --    return LwP64)((LwU64)(p) + (LwU64)(o);
   --  unsupported macro: LwUPtr_fmtX LwU64_fmtX
   --  unsupported macro: LwUPtr_fmtx LwU64_fmtx
   --  unsupported macro: LwUPtr_fmtu LwU64_fmtu
   --  unsupported macro: LwUPtr_fmto LwU64_fmto
   --  unsupported macro: LwSPtr_fmtd LwS64_fmtd
   --  unsupported macro: LwSPtr_fmti LwS64_fmti
   --  unsupported macro: LwP64_NULL (LwP64)0
   --  arg-macro: procedure LwU64_ALIGN32_PACK (pDst, pSrc)
   --    do { (pDst).lo := LwU64_LO32(*(pSrc)); (pDst).hi := LwU64_HI32(*(pSrc)); } while (LW_FALSE)
   --  arg-macro: procedure LwU64_ALIGN32_UNPACK (pDst, pSrc)
   --    do { (*(pDst)) := LwU64_ALIGN32_VAL(pSrc); } while (LW_FALSE)
   --  arg-macro: function LwU64_ALIGN32_VAL (pSrc)
   --    return (LwU64) ((LwU64)((pSrc).lo) or (((LwU64)(pSrc).hi) << 32));
   --  arg-macro: function LwU64_ALIGN32_IS_ZERO (_pU64)
   --    return ((_pU64).lo = 0)  and then  ((_pU64).hi = 0);
   --  arg-macro: procedure LwU64_ALIGN32_ADD (pDst, pSrc1, pSrc2)
   --    do { LwU64 __dst, __src1, __scr2; LwU64_ALIGN32_UNPACK(and__src1, (pSrc1)); LwU64_ALIGN32_UNPACK(and__scr2, (pSrc2)); __dst := __src1 + __scr2; LwU64_ALIGN32_PACK((pDst), and__dst); } while (LW_FALSE)
   --  arg-macro: procedure LwU64_ALIGN32_SUB (pDst, pSrc1, pSrc2)
   --    do { LwU64 __dst, __src1, __scr2; LwU64_ALIGN32_UNPACK(and__src1, (pSrc1)); LwU64_ALIGN32_UNPACK(and__scr2, (pSrc2)); __dst := __src1 + __scr2; LwU64_ALIGN32_PACK((pDst), and__dst); } while (LW_FALSE)
   --  arg-macro: function LwP64_LVALUE (n)
   --    return n;
   --  arg-macro: function LwP64_SELECTOR (n)
   --    return 0;
   --  arg-macro: function LW_PTR_TO_LwP64 (n)
   --    return LwP64)(LwUPtr)(n;
   --  arg-macro: function LW_SIGN_EXT_PTR_TO_LwP64 (p)
   --    return (LwP64)(LwS64)(LwSPtr)(p);
   --  arg-macro: function KERNEL_POINTER_TO_LwP64 (p)
   --    return (LwP64)(uintptr_t)(p);

   LW_S8_MIN : constant := (-128);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:480
   LW_S8_MAX : constant := (+127);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:481
   LW_U8_MIN : constant := (0);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:482
   LW_U8_MAX : constant := (+255);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:483
   LW_S16_MIN : constant := (-32768);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:484
   LW_S16_MAX : constant := (+32767);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:485
   LW_U16_MIN : constant := (0);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:486
   LW_U16_MAX : constant := (+65535);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:487
   LW_S32_MIN : constant := (-2147483647 - 1);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:488
   LW_S32_MAX : constant := (+2147483647);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:489
   LW_U32_MIN : constant := (0);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:490
   LW_U32_MAX : constant := (+4294967295);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:491
   LW_S64_MIN : constant := (-9223372036854775807 - 1);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:492
   LW_S64_MAX : constant := (+9223372036854775807);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:493
   LW_U64_MIN : constant := (0);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:494
   LW_U64_MAX : constant := (+18446744073709551615);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:495
   --  arg-macro: procedure CAST_LW_PTR (p)
   --    p
   --  arg-macro: procedure LW_ALIGN_BYTES (size)
   --    __attribute__ ((aligned (size)))
   --  arg-macro: procedure LW_DECLARE_ALIGNED (TYPE_VAR, ALIGN)
   --    TYPE_VAR __attribute__ ((aligned (ALIGN)))

   LW_TYPEOF_SUPPORTED : constant := 1;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:534
   --  unsupported macro: LW_NOINLINE __attribute__((__noinline__))
   --  unsupported macro: LW_INLINE __inline__
   --  unsupported macro: LW_FORCEINLINE __attribute__((__always_inline__)) __inline__
   --  unsupported macro: LW_ATTRIBUTE_UNUSED __attribute__((__unused__))
   --  arg-macro: function LW_TYPES_FXP_INTEGER (x, y)
   --    return (x)+(y)-1):(y;
   --  arg-macro: procedure LW_TYPES_FXP_FRACTIONAL (x, y)
   --    ((y)-1):0
   --  arg-macro: function LW_TYPES_FXP_FRACTIONAL_MSB (x, y)
   --    return (y)-1):((y)-1;

   LW_TYPES_FXP_FRACTIONAL_MSB_ONE : constant := 16#00000001#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:740
   LW_TYPES_FXP_FRACTIONAL_MSB_ZERO : constant := 16#00000000#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:741
   LW_TYPES_FXP_ZERO : constant := (0);  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:742
   --  arg-macro: function LW_TYPES_UFXP_INTEGER_MAX (x, y)
   --    return ~(LWBIT((y))-1);
   --  arg-macro: function LW_TYPES_UFXP_INTEGER_MIN (x, y)
   --    return 0;
   --  arg-macro: function LW_TYPES_SFXP_INTEGER_SIGN (x, y)
   --    return (x)+(y)-1):((x)+(y)-1;

   LW_TYPES_SFXP_INTEGER_SIGN_NEGATIVE : constant := 16#00000001#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:754
   LW_TYPES_SFXP_INTEGER_SIGN_POSITIVE : constant := 16#00000000#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:755
   --  arg-macro: procedure LW_TYPES_SFXP_S32_SIGN_EXTENSION (x, y)
   --    31:(x)
   --  arg-macro: procedure LW_TYPES_SFXP_S32_SIGN_EXTENSION_POSITIVE (x, y)
   --    16#00000000#
   --  arg-macro: function LW_TYPES_SFXP_S32_SIGN_EXTENSION_NEGATIVE (x, y)
   --    return LWBIT(32-(x))-1;
   --  arg-macro: function LW_TYPES_SFXP_INTEGER_MAX (x, y)
   --    return LWBIT((x))-1;
   --  arg-macro: function LW_TYPES_SFXP_INTEGER_MIN (x, y)
   --    return ~(LWBIT((x))-1);
   --  unsupported macro: LW_TYPES_U32_TO_UFXP_X_Y(x,y,integer) ((LwUFXP ##x ##_ ##y) (((LwU32) (integer)) << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))))
   --  unsupported macro: LW_TYPES_U32_TO_UFXP_X_Y_SCALED(x,y,integer,scale) ((LwUFXP ##x ##_ ##y) ((((((LwU32) (integer)) << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y))))) / (scale)) + ((((((LwU32) (integer)) << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))) % (scale)) > ((scale) >> 1)) ? 1U : 0U)))
   --  unsupported macro: LW_TYPES_UFXP_X_Y_TO_U32(x,y,fxp) ((LwU32) (DRF_VAL(_TYPES, _FXP, _INTEGER((x), (y)), ((LwUFXP ##x ##_ ##y) (fxp)))))
   --  unsupported macro: LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(x,y,fxp) (LW_TYPES_UFXP_X_Y_TO_U32(x, y, (fxp)) + !!DRF_VAL(_TYPES, _FXP, _FRACTIONAL_MSB((x), (y)), ((LwUFXP ##x ##_ ##y) (fxp))))
   --  unsupported macro: LW_TYPES_U64_TO_UFXP_X_Y(x,y,integer) ((LwUFXP ##x ##_ ##y) (((LwU64) (integer)) << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))))
   --  unsupported macro: LW_TYPES_U64_TO_UFXP_X_Y_SCALED(x,y,integer,scale) ((LwUFXP ##x ##_ ##y) (((((LwU64) (integer)) << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))) + ((scale) >> 1)) / (scale)))
   --  unsupported macro: LW_TYPES_UFXP_X_Y_TO_U64(x,y,fxp) ((LwU64) (DRF_VAL(_TYPES, _FXP, _INTEGER((x), (y)), ((LwUFXP ##x ##_ ##y) (fxp)))))
   --  unsupported macro: LW_TYPES_UFXP_X_Y_TO_U64_ROUNDED(x,y,fxp) (LW_TYPES_UFXP_X_Y_TO_U64(x, y, (fxp)) + !!DRF_VAL(_TYPES, _FXP, _FRACTIONAL_MSB((x), (y)), ((LwUFXP ##x ##_ ##y) (fxp))))
   --  unsupported macro: LW_TYPES_S32_TO_SFXP_X_Y(x,y,integer) ((LwSFXP ##x ##_ ##y) (((LwU32) (integer)) << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))))
   --  unsupported macro: LW_TYPES_S32_TO_SFXP_X_Y_SCALED(x,y,integer,scale) ((LwSFXP ##x ##_ ##y) (((((LwS32) (integer)) << DRF_SHIFT(LW_TYPES_FXP_INTEGER((x), (y)))) + ((scale) >> 1)) / (scale)))
   --  unsupported macro: LW_TYPES_SFXP_X_Y_TO_S32(x,y,fxp) ((LwS32) ((DRF_VAL(_TYPES, _FXP, _INTEGER((x), (y)), ((LwSFXP ##x ##_ ##y) (fxp)))) | ((DRF_VAL(_TYPES, _SFXP, _INTEGER_SIGN((x), (y)), (fxp)) == LW_TYPES_SFXP_INTEGER_SIGN_NEGATIVE) ? DRF_NUM(_TYPES, _SFXP, _S32_SIGN_EXTENSION((x), (y)), LW_TYPES_SFXP_S32_SIGN_EXTENSION_NEGATIVE((x), (y))) : DRF_NUM(_TYPES, _SFXP, _S32_SIGN_EXTENSION((x), (y)), LW_TYPES_SFXP_S32_SIGN_EXTENSION_POSITIVE((x), (y))))))
   --  unsupported macro: LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(x,y,fxp) (LW_TYPES_SFXP_X_Y_TO_S32(x, y, (fxp)) + !!DRF_VAL(_TYPES, _FXP, _FRACTIONAL_MSB((x), (y)), ((LwSFXP ##x ##_ ##y) (fxp))))
   --  unsupported macro: LW_TYPES_SINGLE_SIGN 31:31

   LW_TYPES_SINGLE_SIGN_POSITIVE : constant := 16#00000000#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:880
   LW_TYPES_SINGLE_SIGN_NEGATIVE : constant := 16#00000001#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:881
   --  unsupported macro: LW_TYPES_SINGLE_EXPONENT 30:23

   LW_TYPES_SINGLE_EXPONENT_ZERO : constant := 16#00000000#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:883
   LW_TYPES_SINGLE_EXPONENT_BIAS : constant := 16#0000007F#;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:884
   --  unsupported macro: LW_TYPES_SINGLE_MANTISSA 22:0
   --  arg-macro: function LW_TYPES_SINGLE_MANTISSA_TO_UFXP9_23 (single)
   --    return (LwUFXP9_23)(FLD_TEST_DRF(_TYPES, _SINGLE, _EXPONENT, _ZERO, single) ? LW_TYPES_U32_TO_UFXP_X_Y(9, 23, 0) : (LW_TYPES_U32_TO_UFXP_X_Y(9, 23, 1) + DRF_VAL(_TYPES, _SINGLE, _MANTISSA, single)));
   --  arg-macro: function LW_TYPES_SINGLE_EXPONENT_BIASED (single)
   --    return (LwS32)(DRF_VAL(_TYPES, _SINGLE, _EXPONENT, single) - LW_TYPES_SINGLE_EXPONENT_BIAS);
   --  arg-macro: procedure LW_TYPES_CELSIUS_TO_LW_TEMP (cel)
   --    LW_TYPES_S32_TO_SFXP_X_Y(24,8,(cel))
   --  arg-macro: procedure LW_TYPES_LW_TEMP_TO_CELSIUS_TRUNCED (lwt)
   --    LW_TYPES_SFXP_X_Y_TO_S32(24,8,(lwt))
   --  arg-macro: procedure LW_TYPES_LW_TEMP_TO_CELSIUS_ROUNDED (lwt)
   --    LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(24,8,(lwt))
   --  arg-macro: function LW_NBITS_IN_TYPE (type)
   --    return 8 * sizeof(type);
   --  arg-macro: function LW_TYPES_LWSFXP11_5_TO_LW_TEMP (x)
   --    return (LwTemp)(x) << 3;
   --  arg-macro: function LW_TYPES_LWUFXP11_5_WATTS_TO_LWU32_MILLI_WATTS (x)
   --    return (((LwU32)(x)) * ((LwU32)1000)) >> 5;

  --**************************************************************************|*                                                                           *|
  --|
  --|*       Copyright 1993-2019 LWPU, Corporation.  All rights reserved.      *|
  --|*                                                                           *|
  --|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
  --|*     international laws.  Users and possessors of this source code are     *|
  --|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
  --|*     use this code in individual and commercial software.                  *|
  --|*                                                                           *|
  --|*     Any use of this source code must include,  in the user dolwmenta-     *|
  --|*     tion and  internal comments to the code,  notices to the end user     *|
  --|*     as follows:                                                           *|
  --|*                                                                           *|
  --|*       Copyright 1993-2019 LWPU, Corporation.  All rights reserved.      *|
  --|*                                                                           *|
  --|*     LWPU, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
  --|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
  --|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  LWPU, CORPOR-     *|
  --|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
  --|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
  --|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
  --|*     LWPU, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
  --|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
  --|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
  --|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
  --|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
  --|*                                                                           *|
  --|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
  --|*     item,"  as that  term is  defined at  8 C.F.R. 2.101 (OCT 1995),      *|
  --|*     consisting  of "commercial  computer  software"  and  "commercial     *|
  --|*     computer  software  documentation,"  as such  terms  are  used in     *|
  --|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
  --|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
  --|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
  --|*     all U.S. Government End Users  acquire the source code  with only     *|
  --|*     those rights set forth herein.                                        *|
  --|*                                                                           *|
  -- \************************************************************************** 

  --**************************************************************************|*                                                                           *|
  --|
  --|*                         LW Architecture Interface                         *|
  --|*                                                                           *|
  --|*  <lwtypes.h> defines common widths used to access hardware in of LWPU's *|
  --|*  Unified Media Architecture (TM).                                         *|
  --|*                                                                           *|
  -- \************************************************************************** 

  -- XAPIGEN - this file is not suitable for (nor needed by) xapigen.          
  --           Rather than #ifdef out every such include in every sdk          
  --           file, punt here.                                                
  -- XAPIGEN sdk macros for C  
  --**************************************************************************|*                                 Typedefs                                  *|
  --|
  -- \************************************************************************** 

  --Typedefs for MISRA COMPLIANCE
  -- Floating point types
  -- IEEE Single Precision (S1E8M23)          
  -- IEEE Double Precision (S1E11M52)         
  -- IEEE Single Precision (S1E8M23)          
   subtype LwF32 is float;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:97

  -- IEEE Double Precision (S1E11M52)         
   subtype LwF64 is double;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:98

  -- 8-bit: 'char' is the only 8-bit in the C89 standard and after.
  -- "void": enumerated or multiple fields     
  -- 0 to 255                                  
  -- -128 to 127                               
  -- "void": enumerated or multiple fields     
   subtype LwV8 is unsigned_char;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:108

  -- 0 to 255                                  
   subtype LwU8 is unsigned_char;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:109

  -- -128 to 127                               
   subtype LwS8 is signed_char;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:110

  -- 16-bit: If the compiler tells us what we can use, then use it.
  -- "void": enumerated or multiple fields  
   subtype LwV16 is unsigned_short;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:116

  -- 0 to 65535                             
   subtype LwU16 is unsigned_short;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:117

  -- -32768 to 32767                        
   subtype LwS16 is short;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:118

  -- The minimal standard for C89 and after
  -- "void": enumerated or multiple fields    
  -- 0 to 65535                               
  -- -32768 to 32767                          
  -- "void": enumerated or multiple fields    
  -- 0 to 65535                               
  -- -32768 to 32767                          
  -- Macros to get the MSB and LSB of a 16 bit unsigned number
  -- Macro to build a LwU16 from msb and lsb bytes.
   type LWREGSTR is access all LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:145

  -- 32-bit: If the compiler tells us what we can use, then use it.
  -- "void": enumerated or multiple fields  
   subtype LwV32 is unsigned;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:151

  -- 0 to 4294967295                        
   subtype LwU32 is unsigned;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:152

  -- -2147483648 to 2147483647              
   subtype LwS32 is int;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:153

  -- Older compilers
  -- For historical reasons, LwU32/LwV32 are defined to different base intrinsic
  -- types than LwS32 on some platforms.
  -- Mainly for 64-bit linux, where long is 64 bits and win9x, where int is 16 bit.
  -- "void": enumerated or multiple fields    
  -- 0 to 4294967295                          
  -- "void": enumerated or multiple fields    
  -- 0 to 4294967295                          
  -- The minimal standard for C89 and after
  -- "void": enumerated or multiple fields    
  -- 0 to 4294967295                          
  -- Mac OS 32-bit still needs this
  -- -2147483648 to 2147483647                
  -- -2147483648 to 2147483647                
  -- -2147483648 to 2147483647                
  -- 64-bit types for compilers that support them, plus some obsolete variants
  -- 0 to 18446744073709551615                       
  -- -9223372036854775808 to 9223372036854775807     
  -- 0 to 18446744073709551615                       
   subtype LwU64 is Extensions.unsigned_long_long;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:202

  -- -9223372036854775808 to 9223372036854775807     
   subtype LwS64 is Long_Long_Integer;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:203

  -- Microsoft since 2003 -- https://msdn.microsoft.com/en-us/library/29dh1w7z.aspx
  -- 0 to 18446744073709551615                       
  -- -9223372036854775808 to 9223372036854775807     
  -- * Can't use opaque pointer as clients might be compiled with mismatched
  -- * pointer sizes. TYPESAFE check will eventually be removed once all clients
  -- * have transistioned safely to LwHandle.
  -- * The plan is to then eventually scale up the handle to be 64-bits.
  --  

  -- * For compatibility with modules that haven't moved typesafe handles.
  --  

   subtype LwHandle is LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:242

  -- Boolean type  
   subtype LwBool is LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:246

  -- Tristate type: LW_TRISTATE_FALSE, LW_TRISTATE_TRUE, LW_TRISTATE_INDETERMINATE  
   subtype LwTristate is LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:251

  -- Macros to extract the low and high parts of a 64-bit unsigned integer  
  -- Also designed to work if someone happens to pass in a 32-bit integer  
  -- Macros to get the MSB and LSB of a 32 bit unsigned number  
  --**************************************************************************|*                                                                           *|
  --|
  --|*  64 bit type definitions for use in interface structures.                 *|
  --|*                                                                           *|
  -- \************************************************************************** 

  -- 64 bit void pointer                      
   type LwP64 is new System.Address;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:280

  -- pointer sized unsigned int               
   subtype LwUPtr is LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:281

  -- pointer sized signed int                 
   subtype LwSPtr is LwS64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:282

  -- length to agree with sizeof              
   subtype LwLength is LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:283

  -- 64 bit void pointer                      
  -- pointer sized unsigned int               
  -- pointer sized signed int                 
  -- length to agree with sizeof              
  --!
  -- * Helper macro to pack an @ref LwU64_ALIGN32 structure from a @ref LwU64.
  -- *
  -- * @param[out] pDst   Pointer to LwU64_ALIGN32 structure to pack
  -- * @param[in]  pSrc   Pointer to LwU64 with which to pack
  --  

  --!
  -- * Helper macro to unpack a @ref LwU64_ALIGN32 structure into a @ref LwU64.
  -- *
  -- * @param[out] pDst   Pointer to LwU64 in which to unpack
  -- * @param[in]  pSrc   Pointer to LwU64_ALIGN32 structure from which to unpack
  --  

  --!
  -- * Helper macro to unpack a @ref LwU64_ALIGN32 structure as a @ref LwU64.
  -- *
  -- * @param[in]  pSrc   Pointer to LwU64_ALIGN32 structure to unpack
  --  

  --!
  -- * Helper macro to check whether the 32 bit aligned 64 bit number is zero.
  -- *
  -- * @param[in]  _pU64   Pointer to LwU64_ALIGN32 structure.
  -- *
  -- * @return
  -- *  LW_TRUE     _pU64 is zero.
  -- *  LW_FALSE    otherwise.
  --  

  --!
  -- * Helper macro to sub two 32 aligned 64 bit numbers on 64 bit processor.
  -- *
  -- * @param[in]       pSrc1   Pointer to LwU64_ALIGN32 scource 1 structure.
  -- * @param[in]       pSrc2   Pointer to LwU64_ALIGN32 scource 2 structure.
  -- * @param[in/out]   pDst    Pointer to LwU64_ALIGN32 dest. structure.
  --  

  --!
  -- * Helper macro to sub two 32 aligned 64 bit numbers on 64 bit processor.
  -- *
  -- * @param[in]       pSrc1   Pointer to LwU64_ALIGN32 scource 1 structure.
  -- * @param[in]       pSrc2   Pointer to LwU64_ALIGN32 scource 2 structure.
  -- * @param[in/out]   pDst    Pointer to LwU64_ALIGN32 dest. structure.
  --  

  --!
  -- * Structure for representing 32 bit aligned LwU64 (64-bit unsigned integer)
  -- * structures. This structure must be used because the 32 bit processor and
  -- * 64 bit processor compilers will pack/align LwU64 differently.
  -- *
  -- * One use case is RM being 64 bit proc whereas PMU being 32 bit proc, this
  -- * alignment difference will result in corrupted transactions between the RM
  -- * and PMU.
  -- *
  -- * See the @ref LwU64_ALIGN32_PACK and @ref LwU64_ALIGN32_UNPACK macros for
  -- * packing and unpacking these structures.
  -- *
  -- * @note The intention of this structure is to provide a datatype which will
  -- *       packed/aligned consistently and efficiently across all platforms.
  -- *       We don't want to use "LW_DECLARE_ALIGNED(LwU64, 8)" because that
  -- *       leads to memory waste on our 32-bit uprocessors (e.g. FALCONs) where
  -- *       DMEM efficiency is vital.
  --  

  --!
  --     * Low 32 bits.
  --      

  --!
  --     * High 32 bits.
  --      

   type LwU64_ALIGN32 is record
      lo : aliased LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:422
      hi : aliased LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:426
   end record
   with Convention => C_Pass_By_Copy;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:427

  -- XXX Obsolete -- get rid of me...
   subtype LwP64_VALUE_T is LwP64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:430

  -- Useful macro to hide required double cast  
  -- obsolete stuff   
  -- MODS needs to be able to build without these definitions because they collide
  --   with some definitions used in mdiag.  

   subtype V008 is LwV8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:443

   subtype V016 is LwV16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:444

   subtype V032 is LwV32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:445

   subtype U008 is LwU8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:446

   subtype U016 is LwU16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:447

   subtype U032 is LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:448

   subtype S008 is LwS8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:449

   subtype S016 is LwS16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:450

   subtype S032 is LwS32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:451

  -- more obsolete stuff  
  -- need to provide these on macos9 and macosX  
  --**************************************************************************|*                                                                           *|
  --|
  --|*  Limits for common types.                                                 *|
  --|*                                                                           *|
  -- \************************************************************************** 

  -- Explanation of the current form of these limits:
  -- *
  -- * - Decimal is used, as hex values are by default positive.
  -- * - Casts are not used, as usage in the preprocessor itself (#if) ends poorly.
  -- * - The subtraction of 1 for some MIN values is used to get around the fact
  -- *   that the C syntax actually treats -x as NEGATE(x) instead of a distinct
  -- *   number.  Since 214748648 isn't a valid positive 32-bit signed value, we
  -- *   take the largest valid positive signed number, negate it, and subtract 1.
  --  

  -- Aligns fields in structs  so they match up between 32 and 64 bit builds  
  -- XXX This is dangerously nonportable!  We really shouldn't provide a default
  -- version of this that doesn't do anything.
  -- LW_DECLARE_ALIGNED() can be used on all platforms.
  -- This macro form accounts for the fact that __declspec on Windows is required
  -- before the variable type,
  -- and LW_ALIGN_BYTES is required after the variable name.
  -- LWRM_IMPORT is defined on windows for lwrm4x build (lwrm4x.lib).
  -- Check for typeof support. For now restricting to GNUC compilers.
  --**************************************************************************|*                       Function Declaration Types                          *|
  --|
  -- \************************************************************************** 

  -- stretching the meaning of "lwtypes", but this seems to least offensive
  -- place to re-locate these from lwos.h which cannot be included by a number
  -- of builds that need them
  -- GreenHills compiler defines __GNUC__, but doesn't support
  --     * __inline__ keyword.  

  -- Don't force inline on DEBUG builds -- it's annoying for debuggers.  
  -- GreenHills compiler defines __GNUC__, but doesn't support
  --         * __attribute__ or __inline__ keyword.  

  -- GCC 3.1 and beyond support the always_inline function attribute.
  -- RVDS 2.2 also supports forceinline, but ADS 1.2 does not
  --     * The 'warn_unused_result' function attribute prompts GCC to issue a
  --     * warning if the result of a function tagged with this attribute
  --     * is ignored by a caller.  In combination with '-Werror', it can be
  --     * used to enforce result checking in RM code; at this point, this
  --     * is only done on UNIX.
  --      

  --!
  -- * Fixed-point master data types.
  -- *
  -- * These are master-types represent the total number of bits contained within
  -- * the FXP type.  All FXP types below should be based on one of these master
  -- * types.
  --  

   subtype LwSFXP16 is LwS16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:682

   subtype LwSFXP32 is LwS32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:683

   subtype LwUFXP16 is LwU16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:684

   subtype LwUFXP32 is LwU32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:685

   subtype LwUFXP64 is LwU64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:686

  --!
  -- * Fixed-point data types.
  -- *
  -- * These are all integer types with precision indicated in the naming of the
  -- * form: Lw<sign>FXP<num_bits_above_radix>_<num bits below radix>.  The actual
  -- * size of the data type is callwlated as num_bits_above_radix +
  -- * num_bit_below_radix.
  -- *
  -- * All of these FXP types should be based on one of the master types above.
  --  

   subtype LwSFXP11_5 is LwSFXP16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:699

   subtype LwSFXP4_12 is LwSFXP16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:700

   subtype LwSFXP8_8 is LwSFXP16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:701

   subtype LwSFXP8_24 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:702

   subtype LwSFXP10_22 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:703

   subtype LwSFXP16_16 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:704

   subtype LwSFXP18_14 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:705

   subtype LwSFXP20_12 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:706

   subtype LwSFXP24_8 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:707

   subtype LwSFXP27_5 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:708

   subtype LwSFXP28_4 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:709

   subtype LwSFXP29_3 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:710

   subtype LwSFXP31_1 is LwSFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:711

   subtype LwUFXP0_16 is LwUFXP16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:713

   subtype LwUFXP4_12 is LwUFXP16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:714

   subtype LwUFXP8_8 is LwUFXP16;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:715

   subtype LwUFXP3_29 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:716

   subtype LwUFXP4_28 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:717

   subtype LwUFXP8_24 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:718

   subtype LwUFXP9_23 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:719

   subtype LwUFXP10_22 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:720

   subtype LwUFXP16_16 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:721

   subtype LwUFXP20_12 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:722

   subtype LwUFXP24_8 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:723

   subtype LwUFXP25_7 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:724

   subtype LwUFXP28_4 is LwUFXP32;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:725

   subtype LwUFXP40_24 is LwUFXP64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:727

   subtype LwUFXP48_16 is LwUFXP64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:728

   subtype LwUFXP52_12 is LwUFXP64;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:729

  --!
  -- * Utility macros used in colwerting between signed integers and fixed-point
  -- * notation.
  -- *
  -- * - COMMON - These are used by both signed and unsigned.
  --  

  --!
  -- * - UNSIGNED - These are only used for unsigned.
  --  

  --!
  -- * - SIGNED - These are only used for signed.
  --  

  --!
  -- * Colwersion macros used for colwerting between integer and fixed point
  -- * representations.  Both signed and unsigned variants.
  -- *
  -- * Warning:
  -- * Note that most of the macros below can overflow if applied on values that can
  -- * not fit the destination type.  It's caller responsibility to ensure that such
  -- * situations will not occur.
  -- *
  -- * Some colwersions perform some commonly preformed tasks other than just
  -- * bit-shifting:
  -- *
  -- * - _SCALED:
  -- *   For integer -> fixed-point we add handling divisors to represent
  -- *   non-integer values.
  -- *
  -- * - _ROUNDED:
  -- *   For fixed-point -> integer we add rounding to integer values.
  --  

  -- 32-bit Unsigned FXP:
  -- 64-bit Unsigned FXP
  -- 32-bit Signed FXP:
  -- Some compilers do not support left shift negative values
  -- so typecast integer to LwU32 instead of LwS32
  --!
  -- * Macros representing the single-precision IEEE 754 floating point format for
  -- * "binary32", also known as "single" and "float".
  -- *
  -- * Single precision floating point format wiki [1]
  -- *
  -- * _SIGN
  -- *     Single bit representing the sign of the number.
  -- * _EXPONENT
  -- *     Unsigned 8-bit number representing the exponent value by which to scale
  -- *     the mantissa.
  -- *     _BIAS - The value by which to offset the exponent to account for sign.
  -- * _MANTISSA
  -- *     Explicit 23-bit significand of the value.  When exponent != 0, this is an
  -- *     implicitly 24-bit number with a leading 1 prepended.  This 24-bit number
  -- *     can be conceptualized as FXP 9.23.
  -- *
  -- * With these definitions, the value of a floating point number can be
  -- * callwlated as:
  -- *     (-1)^(_SIGN) *
  -- *         2^(_EXPONENT - _EXPONENT_BIAS) *
  -- *         (1 + _MANTISSA / (1 << 23))
  --  

  -- [1] : http://en.wikipedia.org/wiki/Single_precision_floating-point_format
  --!
  -- * Helper macro to return a IEEE 754 single-precision value's mantissa as an
  -- * unsigned FXP 9.23 value.
  -- *
  -- * @param[in] single   IEEE 754 single-precision value to manipulate.
  -- *
  -- * @return IEEE 754 single-precision values mantissa represented as an unsigned
  -- *     FXP 9.23 value.
  --  

  --!
  -- * Helper macro to return an IEEE 754 single-precision value's exponent,
  -- * including the bias.
  -- *
  -- * @param[in] single   IEEE 754 single-precision value to manipulate.
  -- *
  -- * @return Signed exponent value for IEEE 754 single-precision.
  --  

  --!
  -- * LwTemp - temperature data type introduced to avoid bugs in colwersion between
  -- * various existing notations.
  --  

   subtype LwTemp is LwSFXP24_8;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwtypes.h:919

  --!
  -- * Macros for LwType <-> Celsius temperature colwersion.
  --  

  --!
  -- * Macro for LwType -> number of bits colwersion
  --  

  --!
  -- * Macro to colwert SFXP 11.5 to LwTemp.
  --  

  --!
  -- * Macro to colwert UFXP11.5 Watts to LwU32 milli-Watts.
  --  

end lwtypes_h;
