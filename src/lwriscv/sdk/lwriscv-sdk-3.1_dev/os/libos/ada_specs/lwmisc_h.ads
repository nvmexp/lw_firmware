pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with lwtypes_h;
with System;

package lwmisc_h is

   --  arg-macro: function BIT (b)
   --    return 2**(b);
   --  arg-macro: function BIT32 (b)
   --    return (LwU32)2**(b);
   --  arg-macro: function BIT64 (b)
   --    return (LwU64)2**(b);
   --  arg-macro: function LWBIT (b)
   --    return 2**(b);
   --  arg-macro: function LWBIT_TYPE (b, t)
   --    return ((t)1)<<(b);
   --  arg-macro: procedure LWBIT32 (b)
   --    LWBIT_TYPE(b, LwU32)
   --  arg-macro: procedure LWBIT64 (b)
   --    LWBIT_TYPE(b, LwU64)
   --  arg-macro: function BIT_IDX_32 (n)
   --    return ((((n) and 16#FFFF0000#) /= 0) ? 16#10#: 0) or ((((n) and 16#FF00FF00#) /= 0) ? 16#08#: 0) or ((((n) and 16#F0F0F0F0#) /= 0) ? 16#04#: 0) or ((((n) and 16#CCCCCCCC#) /= 0) ? 16#02#: 0) or ((((n) and 16#AAAAAAAA#) /= 0) ? 16#01#: 0) ;
   --  arg-macro: function BIT_IDX_64 (n)
   --    return ((((n) and 16#FFFFFFFF00000000#) /= 0) ? 16#20#: 0) or ((((n) and 16#FFFF0000FFFF0000#) /= 0) ? 16#10#: 0) or ((((n) and 16#FF00FF00FF00FF00#) /= 0) ? 16#08#: 0) or ((((n) and 16#F0F0F0F0F0F0F0F0#) /= 0) ? 16#04#: 0) or ((((n) and 16#CCCCCCCCCCCCCCCC#) /= 0) ? 16#02#: 0) or ((((n) and 16#AAAAAAAAAAAAAAAA#) /= 0) ? 16#01#: 0) ;
   --  arg-macro: procedure IDX_32 (n32)
   --    { LwU32 idx := 0; if (((n32) and 16#FFFF0000#) /= 0) { idx += 16; } if (((n32) and 16#FF00FF00#) /= 0) { idx += 8; } if (((n32) and 16#F0F0F0F0#) /= 0) { idx += 4; } if (((n32) and 16#CCCCCCCC#) /= 0) { idx += 2; } if (((n32) and 16#AAAAAAAA#) /= 0) { idx += 1; } (n32) := idx; }
   --  arg-macro: function DRF_ISBIT (bitval, drf)
   --    return  bitval ? drf ;
   --  arg-macro: function DEVICE_BASE (d)
   --    return 0?d;
   --  arg-macro: function DEVICE_EXTENT (d)
   --    return 1?d;
   --  arg-macro: function DRF_BASE (drf)
   --    return 0?drf;
   --  arg-macro: function DRF_EXTENT (drf)
   --    return 1?drf;
   --  arg-macro: function DRF_SHIFT (drf)
   --    return (DRF_ISBIT(0,drf)) mod 32;
   --  arg-macro: function DRF_SHIFT_RT (drf)
   --    return (DRF_ISBIT(1,drf)) mod 32;
   --  arg-macro: function DRF_MASK (drf)
   --    return 16#FFFFFFFF#>>(31-((DRF_ISBIT(1,drf)) mod 32)+((DRF_ISBIT(0,drf)) mod 32));
   --  unsupported macro: DRF_DEF(d,r,f,c) ((LW ## d ## r ## f ## c)<<DRF_SHIFT(LW ## d ## r ## f))
   --  unsupported macro: DRF_NUM(d,r,f,n) (((n)&DRF_MASK(LW ## d ## r ## f))<<DRF_SHIFT(LW ## d ## r ## f))
   --  arg-macro: function DRF_SHIFTMASK (drf)
   --    return DRF_MASK(drf)<<(DRF_SHIFT(drf));
   --  arg-macro: function DRF_SIZE (drf)
   --    return DRF_EXTENT(drf)-DRF_BASE(drf)+1;
   --  unsupported macro: DRF_VAL(d,r,f,v) (((v)>>DRF_SHIFT(LW ## d ## r ## f))&DRF_MASK(LW ## d ## r ## f))
   --  unsupported macro: DRF_VAL_SIGNED(d,r,f,v) (((DRF_VAL(d,r,f,(v)) ^ (LWBIT(DRF_SIZE(LW ## d ## r ## f)-1)))) - (LWBIT(DRF_SIZE(LW ## d ## r ## f)-1)))
   --  unsupported macro: DRF_IDX_DEF(d,r,f,i,c) ((LW ## d ## r ## f ## c)<<DRF_SHIFT(LW ##d ##r ##f(i)))
   --  unsupported macro: DRF_IDX_OFFSET_DEF(d,r,f,i,o,c) ((LW ## d ## r ## f ## c)<<DRF_SHIFT(LW ##d ##r ##f(i,o)))
   --  unsupported macro: DRF_IDX_NUM(d,r,f,i,n) (((n)&DRF_MASK(LW ##d ##r ##f(i)))<<DRF_SHIFT(LW ##d ##r ##f(i)))
   --  unsupported macro: DRF_IDX_VAL(d,r,f,i,v) (((v)>>DRF_SHIFT(LW ##d ##r ##f(i)))&DRF_MASK(LW ##d ##r ##f(i)))
   --  unsupported macro: DRF_IDX_OFFSET_VAL(d,r,f,i,o,v) (((v)>>DRF_SHIFT(LW ##d ##r ##f(i,o)))&DRF_MASK(LW ##d ##r ##f(i,o)))
   --  unsupported macro: DRF_VAL_FRAC(d,r,x,y,v,z) ((DRF_VAL(d,r,x,(v))*z) + ((DRF_VAL(d,r,y,v)*z) / (1<<DRF_SIZE(LW ##d ##r ##y))))
   --  arg-macro: function DRF_SHIFT64 (drf)
   --    return (DRF_ISBIT(0,drf)) mod 64;
   --  arg-macro: function DRF_MASK64 (drf)
   --    return LW_U64_MAX>>(63-((DRF_ISBIT(1,drf)) mod 64)+((DRF_ISBIT(0,drf)) mod 64));
   --  arg-macro: function DRF_SHIFTMASK64 (drf)
   --    return DRF_MASK64(drf)<<(DRF_SHIFT64(drf));
   --  unsupported macro: DRF_DEF64(d,r,f,c) (((LwU64)(LW ## d ## r ## f ## c))<<DRF_SHIFT64(LW ## d ## r ## f))
   --  unsupported macro: DRF_NUM64(d,r,f,n) ((((LwU64)(n))&DRF_MASK64(LW ## d ## r ## f))<<DRF_SHIFT64(LW ## d ## r ## f))
   --  unsupported macro: DRF_VAL64(d,r,f,v) ((((LwU64)(v))>>DRF_SHIFT64(LW ## d ## r ## f))&DRF_MASK64(LW ## d ## r ## f))
   --  unsupported macro: DRF_VAL_SIGNED64(d,r,f,v) (((DRF_VAL64(d,r,f,(v)) ^ (LWBIT64(DRF_SIZE(LW ## d ## r ## f)-1)))) - (LWBIT64(DRF_SIZE(LW ## d ## r ## f)-1)))
   --  unsupported macro: DRF_IDX_DEF64(d,r,f,i,c) (((LwU64)(LW ## d ## r ## f ## c))<<DRF_SHIFT64(LW ##d ##r ##f(i)))
   --  unsupported macro: DRF_IDX_OFFSET_DEF64(d,r,f,i,o,c) ((LwU64)(LW ## d ## r ## f ## c)<<DRF_SHIFT64(LW ##d ##r ##f(i,o)))
   --  unsupported macro: DRF_IDX_NUM64(d,r,f,i,n) ((((LwU64)(n))&DRF_MASK64(LW ##d ##r ##f(i)))<<DRF_SHIFT64(LW ##d ##r ##f(i)))
   --  unsupported macro: DRF_IDX_VAL64(d,r,f,i,v) ((((LwU64)(v))>>DRF_SHIFT64(LW ##d ##r ##f(i)))&DRF_MASK64(LW ##d ##r ##f(i)))
   --  unsupported macro: DRF_IDX_OFFSET_VAL64(d,r,f,i,o,v) (((LwU64)(v)>>DRF_SHIFT64(LW ##d ##r ##f(i,o)))&DRF_MASK64(LW ##d ##r ##f(i,o)))
   --  unsupported macro: FLD_SET_DRF64(d,r,f,c,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW ##d ##r ##f)) | DRF_DEF64(d,r,f,c))
   --  unsupported macro: FLD_SET_DRF_NUM64(d,r,f,n,v) ((((LwU64)(v)) & ~DRF_SHIFTMASK64(LW ##d ##r ##f)) | DRF_NUM64(d,r,f,n))
   --  unsupported macro: FLD_IDX_SET_DRF64(d,r,f,i,c,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW ##d ##r ##f(i))) | DRF_IDX_DEF64(d,r,f,i,c))
   --  unsupported macro: FLD_IDX_OFFSET_SET_DRF64(d,r,f,i,o,c,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW ##d ##r ##f(i,o))) | DRF_IDX_OFFSET_DEF64(d,r,f,i,o,c))
   --  unsupported macro: FLD_IDX_SET_DRF_DEF64(d,r,f,i,c,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW ##d ##r ##f(i))) | DRF_IDX_DEF64(d,r,f,i,c))
   --  unsupported macro: FLD_IDX_SET_DRF_NUM64(d,r,f,i,n,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW ##d ##r ##f(i))) | DRF_IDX_NUM64(d,r,f,i,n))
   --  unsupported macro: FLD_SET_DRF_IDX64(d,r,f,c,i,v) (((LwU64)(v) & ~DRF_SHIFTMASK64(LW ##d ##r ##f)) | DRF_DEF64(d,r,f,c(i)))
   --  unsupported macro: FLD_TEST_DRF64(d,r,f,c,v) (DRF_VAL64(d, r, f, (v)) == LW ##d ##r ##f ##c)
   --  unsupported macro: FLD_TEST_DRF_AND64(d,r,f,c,v) (DRF_VAL64(d, r, f, (v)) & LW ##d ##r ##f ##c)
   --  arg-macro: function FLD_TEST_DRF_NUM64 (d, r, f, n, v)
   --    return DRF_VAL64(d, r, f, (v)) = (n);
   --  unsupported macro: FLD_IDX_TEST_DRF64(d,r,f,i,c,v) (DRF_IDX_VAL64(d, r, f, i, (v)) == LW ##d ##r ##f ##c)
   --  unsupported macro: FLD_IDX_OFFSET_TEST_DRF64(d,r,f,i,o,c,v) (DRF_IDX_OFFSET_VAL64(d, r, f, i, o, (v)) == LW ##d ##r ##f ##c)
   --  unsupported macro: REF_DEF64(drf,d) (((drf ## d)&DRF_MASK64(drf))<<DRF_SHIFT64(drf))
   --  arg-macro: function REF_VAL64 (drf, v)
   --    return ((LwU64)(v)>>DRF_SHIFT64(drf))andDRF_MASK64(drf);
   --  arg-macro: function REF_NUM64 (drf, n)
   --    return ((LwU64)(n)andDRF_MASK64(drf))<<DRF_SHIFT64(drf);
   --  unsupported macro: FLD_TEST_REF64(drf,c,v) (REF_VAL64(drf, v) == drf ##c)
   --  unsupported macro: FLD_TEST_REF_AND64(drf,c,v) (REF_VAL64(drf, v) & drf ##c)
   --  arg-macro: function FLD_SET_REF_NUM64 (drf, n, v)
   --    return ((LwU64)(v) and ~DRF_SHIFTMASK64(drf)) or REF_NUM64(drf,n);
   --  unsupported macro: FLD_SET_DRF(d,r,f,c,v) (((v) & ~DRF_SHIFTMASK(LW ##d ##r ##f)) | DRF_DEF(d,r,f,c))
   --  unsupported macro: FLD_SET_DRF_NUM(d,r,f,n,v) (((v) & ~DRF_SHIFTMASK(LW ##d ##r ##f)) | DRF_NUM(d,r,f,n))
   --  unsupported macro: FLD_SET_DRF_DEF(d,r,f,c,v) ((v) = ((v) & ~DRF_SHIFTMASK(LW ##d ##r ##f)) | DRF_DEF(d,r,f,c))
   --  unsupported macro: FLD_IDX_SET_DRF(d,r,f,i,c,v) (((v) & ~DRF_SHIFTMASK(LW ##d ##r ##f(i))) | DRF_IDX_DEF(d,r,f,i,c))
   --  unsupported macro: FLD_IDX_OFFSET_SET_DRF(d,r,f,i,o,c,v) (((v) & ~DRF_SHIFTMASK(LW ##d ##r ##f(i,o))) | DRF_IDX_OFFSET_DEF(d,r,f,i,o,c))
   --  unsupported macro: FLD_IDX_SET_DRF_DEF(d,r,f,i,c,v) (((v) & ~DRF_SHIFTMASK(LW ##d ##r ##f(i))) | DRF_IDX_DEF(d,r,f,i,c))
   --  unsupported macro: FLD_IDX_SET_DRF_NUM(d,r,f,i,n,v) (((v) & ~DRF_SHIFTMASK(LW ##d ##r ##f(i))) | DRF_IDX_NUM(d,r,f,i,n))
   --  unsupported macro: FLD_SET_DRF_IDX(d,r,f,c,i,v) (((v) & ~DRF_SHIFTMASK(LW ##d ##r ##f)) | DRF_DEF(d,r,f,c(i)))
   --  unsupported macro: FLD_TEST_DRF(d,r,f,c,v) ((DRF_VAL(d, r, f, (v)) == LW ##d ##r ##f ##c))
   --  unsupported macro: FLD_TEST_DRF_AND(d,r,f,c,v) ((DRF_VAL(d, r, f, (v)) & LW ##d ##r ##f ##c))
   --  arg-macro: function FLD_TEST_DRF_NUM (d, r, f, n, v)
   --    return (DRF_VAL(d, r, f, (v)) = (n));
   --  unsupported macro: FLD_IDX_TEST_DRF(d,r,f,i,c,v) ((DRF_IDX_VAL(d, r, f, i, (v)) == LW ##d ##r ##f ##c))
   --  unsupported macro: FLD_IDX_OFFSET_TEST_DRF(d,r,f,i,o,c,v) ((DRF_IDX_OFFSET_VAL(d, r, f, i, o, (v)) == LW ##d ##r ##f ##c))
   --  unsupported macro: REF_DEF(drf,d) (((drf ## d)&DRF_MASK(drf))<<DRF_SHIFT(drf))
   --  arg-macro: function REF_VAL (drf, v)
   --    return ((v)>>DRF_SHIFT(drf))andDRF_MASK(drf);
   --  arg-macro: function REF_NUM (drf, n)
   --    return ((n)andDRF_MASK(drf))<<DRF_SHIFT(drf);
   --  unsupported macro: FLD_TEST_REF(drf,c,v) (REF_VAL(drf, (v)) == drf ##c)
   --  unsupported macro: FLD_TEST_REF_AND(drf,c,v) (REF_VAL(drf, (v)) & drf ##c)
   --  arg-macro: function FLD_SET_REF_NUM (drf, n, v)
   --    return ((v) and ~DRF_SHIFTMASK(drf)) or REF_NUM(drf,n);
   --  unsupported macro: CR_DRF_DEF(d,r,f,c) ((CR ## d ## r ## f ## c)<<DRF_SHIFT(CR ## d ## r ## f))
   --  unsupported macro: CR_DRF_NUM(d,r,f,n) (((n)&DRF_MASK(CR ## d ## r ## f))<<DRF_SHIFT(CR ## d ## r ## f))
   --  unsupported macro: CR_DRF_VAL(d,r,f,v) (((v)>>DRF_SHIFT(CR ## d ## r ## f))&DRF_MASK(CR ## d ## r ## f))
   --  arg-macro: procedure DRF_EXPAND_MW (drf)
   --    drf
   --  unsupported macro: DRF_PICK_MW(drf,v) ((v)? DRF_EXPAND_ ##drf)
   --  arg-macro: function DRF_WORD_MW (drf)
   --    return DRF_PICK_MW(drf,0)/32;
   --  arg-macro: function DRF_BASE_MW (drf)
   --    return DRF_PICK_MW(drf,0)mod32;
   --  arg-macro: function DRF_EXTENT_MW (drf)
   --    return DRF_PICK_MW(drf,1)mod32;
   --  arg-macro: function DRF_SHIFT_MW (drf)
   --    return DRF_PICK_MW(drf,0)mod32;
   --  arg-macro: function DRF_MASK_MW (drf)
   --    return 16#FFFFFFFF#>>((31-(DRF_EXTENT_MW(drf))+(DRF_BASE_MW(drf)))mod32);
   --  arg-macro: function DRF_SHIFTMASK_MW (drf)
   --    return (DRF_MASK_MW(drf))<<(DRF_SHIFT_MW(drf));
   --  arg-macro: function DRF_SIZE_MW (drf)
   --    return DRF_EXTENT_MW(drf)-DRF_BASE_MW(drf)+1;
   --  unsupported macro: DRF_DEF_MW(d,r,f,c) ((LW ##d ##r ##f ##c) << DRF_SHIFT_MW(LW ##d ##r ##f))
   --  unsupported macro: DRF_NUM_MW(d,r,f,n) (((n)&DRF_MASK_MW(LW ##d ##r ##f))<<DRF_SHIFT_MW(LW ##d ##r ##f))
   --  unsupported macro: DRF_VAL_MW_1WORD(d,r,f,v) ((((v)[DRF_WORD_MW(LW ##d ##r ##f)])>>DRF_SHIFT_MW(LW ##d ##r ##f))&DRF_MASK_MW(LW ##d ##r ##f))
   --  arg-macro: function DRF_SPANS (drf)
   --    return (DRF_PICK_MW(drf,0)/32) /= (DRF_PICK_MW(drf,1)/32);
   --  arg-macro: function DRF_WORD_MW_LOW (drf)
   --    return DRF_PICK_MW(drf,0)/32;
   --  arg-macro: function DRF_WORD_MW_HIGH (drf)
   --    return DRF_PICK_MW(drf,1)/32;
   --  arg-macro: function DRF_MASK_MW_LOW (drf)
   --    return 16#FFFFFFFF#;
   --  arg-macro: function DRF_MASK_MW_HIGH (drf)
   --    return 16#FFFFFFFF#>>(31-(DRF_EXTENT_MW(drf)));
   --  arg-macro: function DRF_SHIFT_MW_LOW (drf)
   --    return DRF_PICK_MW(drf,0)mod32;
   --  arg-macro: function DRF_SHIFT_MW_HIGH (drf)
   --    return 0;
   --  arg-macro: function DRF_MERGE_SHIFT (drf)
   --    return (32-((DRF_PICK_MW(drf,0)mod32)))mod32;
   --  unsupported macro: DRF_VAL_MW_2WORD(d,r,f,v) (((((v)[DRF_WORD_MW_LOW(LW ##d ##r ##f)])>>DRF_SHIFT_MW_LOW(LW ##d ##r ##f))&DRF_MASK_MW_LOW(LW ##d ##r ##f)) | (((((v)[DRF_WORD_MW_HIGH(LW ##d ##r ##f)])>>DRF_SHIFT_MW_HIGH(LW ##d ##r ##f))&DRF_MASK_MW_HIGH(LW ##d ##r ##f)) << DRF_MERGE_SHIFT(LW ##d ##r ##f)))
   --  unsupported macro: DRF_VAL_MW(d,r,f,v) ( DRF_SPANS(LW ##d ##r ##f) ? DRF_VAL_MW_2WORD(d,r,f,v) : DRF_VAL_MW_1WORD(d,r,f,v) )
   --  unsupported macro: DRF_IDX_DEF_MW(d,r,f,i,c) ((LW ##d ##r ##f ##c)<<DRF_SHIFT_MW(LW ##d ##r ##f(i)))
   --  unsupported macro: DRF_IDX_NUM_MW(d,r,f,i,n) (((n)&DRF_MASK_MW(LW ##d ##r ##f(i)))<<DRF_SHIFT_MW(LW ##d ##r ##f(i)))
   --  unsupported macro: DRF_IDX_VAL_MW(d,r,f,i,v) ((((v)[DRF_WORD_MW(LW ##d ##r ##f(i))])>>DRF_SHIFT_MW(LW ##d ##r ##f(i)))&DRF_MASK_MW(LW ##d ##r ##f(i)))
   --  unsupported macro: FLD_IDX_OR_DRF_DEF(d,r,f,c,s,v) do { LwU32 idx; for (idx = 0; idx < (LW ## d ## r ## f ## s); ++idx) { v |= DRF_IDX_DEF(d,r,f,idx,c); } } while(0)
   --  arg-macro: function FLD_MERGE_MW (drf, n, v)
   --    return ((v)(DRF_WORD_MW(drf)) and ~DRF_SHIFTMASK_MW(drf)) or n;
   --  arg-macro: function FLD_ASSIGN_MW (drf, n, v)
   --    return (v)(DRF_WORD_MW(drf)) := FLD_MERGE_MW(drf, n, v);
   --  arg-macro: function FLD_IDX_MERGE_MW (drf, i, n, v)
   --    return ((v)(DRF_WORD_MW(drf(i))) and ~DRF_SHIFTMASK_MW(drf(i))) or n;
   --  arg-macro: function FLD_IDX_ASSIGN_MW (drf, i, n, v)
   --    return (v)(DRF_WORD_MW(drf(i))) := FLD_MERGE_MW(drf(i), n, v);
   --  unsupported macro: FLD_SET_DRF_MW(d,r,f,c,v) FLD_MERGE_MW(LW ##d ##r ##f, DRF_DEF_MW(d,r,f,c), v)
   --  unsupported macro: FLD_SET_DRF_NUM_MW(d,r,f,n,v) FLD_ASSIGN_MW(LW ##d ##r ##f, DRF_NUM_MW(d,r,f,n), v)
   --  unsupported macro: FLD_SET_DRF_DEF_MW(d,r,f,c,v) FLD_ASSIGN_MW(LW ##d ##r ##f, DRF_DEF_MW(d,r,f,c), v)
   --  unsupported macro: FLD_IDX_SET_DRF_MW(d,r,f,i,c,v) FLD_IDX_MERGE_MW(LW ##d ##r ##f, i, DRF_IDX_DEF_MW(d,r,f,i,c), v)
   --  unsupported macro: FLD_IDX_SET_DRF_DEF_MW(d,r,f,i,c,v) FLD_IDX_MERGE_MW(LW ##d ##r ##f, i, DRF_IDX_DEF_MW(d,r,f,i,c), v)
   --  unsupported macro: FLD_IDX_SET_DRF_NUM_MW(d,r,f,i,n,v) FLD_IDX_ASSIGN_MW(LW ##d ##r ##f, i, DRF_IDX_NUM_MW(d,r,f,i,n), v)
   --  unsupported macro: FLD_TEST_DRF_MW(d,r,f,c,v) ((DRF_VAL_MW(d, r, f, (v)) == LW ##d ##r ##f ##c))
   --  arg-macro: function FLD_TEST_DRF_NUM_MW (d, r, f, n, v)
   --    return (DRF_VAL_MW(d, r, f, (v)) = n);
   --  unsupported macro: FLD_IDX_TEST_DRF_MW(d,r,f,i,c,v) ((DRF_IDX_VAL_MW(d, r, f, i, (v)) == LW ##d ##r ##f ##c))
   --  unsupported macro: DRF_VAL_BS(d,r,f,v) ( DRF_SPANS(LW ##d ##r ##f) ? DRF_VAL_BS_2WORD(d,r,f,(v)) : DRF_VAL_BS_1WORD(d,r,f,(v)) )
   --  unsupported macro: ENG_RD_REG(g,o,d,r) GPU_REG_RD32(g, ENG_REG ##d(o,d,r))
   --  unsupported macro: ENG_WR_REG(g,o,d,r,v) GPU_REG_WR32(g, ENG_REG ##d(o,d,r), (v))
   --  unsupported macro: ENG_RD_DRF(g,o,d,r,f) ((GPU_REG_RD32(g, ENG_REG ##d(o,d,r))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))
   --  unsupported macro: ENG_WR_DRF_DEF(g,o,d,r,f,c) GPU_REG_WR32(g, ENG_REG ##d(o,d,r),(GPU_REG_RD32(g,ENG_REG ##d(o,d,r))&~(GPU_DRF_MASK(LW ##d ##r ##f)<<GPU_DRF_SHIFT(LW ##d ##r ##f)))|GPU_DRF_DEF(d,r,f,c))
   --  unsupported macro: ENG_WR_DRF_NUM(g,o,d,r,f,n) GPU_REG_WR32(g, ENG_REG ##d(o,d,r),(GPU_REG_RD32(g,ENG_REG ##d(o,d,r))&~(GPU_DRF_MASK(LW ##d ##r ##f)<<GPU_DRF_SHIFT(LW ##d ##r ##f)))|GPU_DRF_NUM(d,r,f,n))
   --  unsupported macro: ENG_TEST_DRF_DEF(g,o,d,r,f,c) (ENG_RD_DRF(g, o, d, r, f) == LW ##d ##r ##f ##c)
   --  unsupported macro: ENG_RD_IDX_DRF(g,o,d,r,f,i) ((GPU_REG_RD32(g, ENG_REG ##d(o,d,r(i)))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))
   --  unsupported macro: ENG_TEST_IDX_DRF_DEF(g,o,d,r,f,c,i) (ENG_RD_IDX_DRF(g, o, d, r, f, (i)) == LW ##d ##r ##f ##c)
   --  unsupported macro: ENG_IDX_RD_REG(g,o,d,i,r) GPU_REG_RD32(g, ENG_IDX_REG ##d(o,d,i,r))
   --  unsupported macro: ENG_IDX_WR_REG(g,o,d,i,r,v) GPU_REG_WR32(g, ENG_IDX_REG ##d(o,d,i,r), (v))
   --  unsupported macro: ENG_IDX_RD_DRF(g,o,d,i,r,f) ((GPU_REG_RD32(g, ENG_IDX_REG ##d(o,d,i,r))>>GPU_DRF_SHIFT(LW ## d ## r ## f))&GPU_DRF_MASK(LW ## d ## r ## f))
   --  unsupported macro: DRF_VAL_BS_1WORD(d,r,f,v) ((DRF_READ_1WORD_BS(d,r,f,v)>>DRF_SHIFT_MW(LW ##d ##r ##f))&DRF_MASK_MW(LW ##d ##r ##f))
   --  unsupported macro: DRF_VAL_BS_2WORD(d,r,f,v) (((DRF_READ_4BYTE_BS(LW ##d ##r ##f,v)>>DRF_SHIFT_MW_LOW(LW ##d ##r ##f))&DRF_MASK_MW_LOW(LW ##d ##r ##f)) | (((DRF_READ_1WORD_BS_HIGH(d,r,f,v)>>DRF_SHIFT_MW_HIGH(LW ##d ##r ##f))&DRF_MASK_MW_HIGH(LW ##d ##r ##f)) << DRF_MERGE_SHIFT(LW ##d ##r ##f)))
   --  arg-macro: function DRF_READ_1BYTE_BS (drf, v)
   --    return (LwU32)(((const LwU8*)(v))(DRF_WORD_MW(drf)*4));
   --  arg-macro: function DRF_READ_2BYTE_BS (drf, v)
   --    return DRF_READ_1BYTE_BS(drf,v)or ((LwU32)(((const LwU8*)(v))(DRF_WORD_MW(drf)*4+1))<<8);
   --  arg-macro: function DRF_READ_3BYTE_BS (drf, v)
   --    return DRF_READ_2BYTE_BS(drf,v)or ((LwU32)(((const LwU8*)(v))(DRF_WORD_MW(drf)*4+2))<<16);
   --  arg-macro: function DRF_READ_4BYTE_BS (drf, v)
   --    return DRF_READ_3BYTE_BS(drf,v)or ((LwU32)(((const LwU8*)(v))(DRF_WORD_MW(drf)*4+3))<<24);
   --  arg-macro: function DRF_READ_1BYTE_BS_HIGH (drf, v)
   --    return (LwU32)(((const LwU8*)(v))(DRF_WORD_MW_HIGH(drf)*4));
   --  arg-macro: function DRF_READ_2BYTE_BS_HIGH (drf, v)
   --    return DRF_READ_1BYTE_BS_HIGH(drf,v)or ((LwU32)(((const LwU8*)(v))(DRF_WORD_MW_HIGH(drf)*4+1))<<8);
   --  arg-macro: function DRF_READ_3BYTE_BS_HIGH (drf, v)
   --    return DRF_READ_2BYTE_BS_HIGH(drf,v)or ((LwU32)(((const LwU8*)(v))(DRF_WORD_MW_HIGH(drf)*4+2))<<16);
   --  arg-macro: function DRF_READ_4BYTE_BS_HIGH (drf, v)
   --    return DRF_READ_3BYTE_BS_HIGH(drf,v)or ((LwU32)(((const LwU8*)(v))(DRF_WORD_MW_HIGH(drf)*4+3))<<24);
   --  arg-macro: function LW_TWO_N_MINUS_ONE (n)
   --    return ((2**(n/2))<<((n+1)/2))-1;
   --  unsupported macro: DRF_READ_1WORD_BS(d,r,f,v) ((DRF_EXTENT_MW(LW ##d ##r ##f)<8)?DRF_READ_1BYTE_BS(LW ##d ##r ##f,(v)): ((DRF_EXTENT_MW(LW ##d ##r ##f)<16)?DRF_READ_2BYTE_BS(LW ##d ##r ##f,(v)): ((DRF_EXTENT_MW(LW ##d ##r ##f)<24)?DRF_READ_3BYTE_BS(LW ##d ##r ##f,(v)): DRF_READ_4BYTE_BS(LW ##d ##r ##f,(v)))))
   --  unsupported macro: DRF_READ_1WORD_BS_HIGH(d,r,f,v) ((DRF_EXTENT_MW(LW ##d ##r ##f)<8)?DRF_READ_1BYTE_BS_HIGH(LW ##d ##r ##f,(v)): ((DRF_EXTENT_MW(LW ##d ##r ##f)<16)?DRF_READ_2BYTE_BS_HIGH(LW ##d ##r ##f,(v)): ((DRF_EXTENT_MW(LW ##d ##r ##f)<24)?DRF_READ_3BYTE_BS_HIGH(LW ##d ##r ##f,(v)): DRF_READ_4BYTE_BS_HIGH(LW ##d ##r ##f,(v)))))
   --  arg-macro: function BIN_2_GRAY (n)
   --    return (n)xor((n)>>1);
   --  arg-macro: procedure GRAY_2_BIN_64b (n)
   --    (n)^=(n)>>1; (n)^=(n)>>2; (n)^=(n)>>4; (n)^=(n)>>8; (n)^=(n)>>16; (n)^=(n)>>32;
   --  arg-macro: function LOWESTBIT (x)
   --    return  (x) and (((x)-1) xor (x)) ;
   --  arg-macro: procedure HIGHESTBIT (n32)
   --    { HIGHESTBITIDX_32(n32); n32 := LWBIT(n32); }
   --  arg-macro: function ONEBITSET (x)
   --    return  (x)  and then  (((x) and ((x)-1)) = 0) ;
   --  arg-macro: procedure NUMSETBITS_32 (n32)
   --    { n32 := n32 - ((n32 >> 1) and 16#55555555#); n32 := (n32 and 16#33333333#) + ((n32 >> 2) and 16#33333333#); n32 := (((n32 + (n32 >> 4)) and 16#0F0F0F0F#) * 16#01010101#) >> 24; }
   --  arg-macro: procedure LOWESTBITIDX_32 (n32)
   --    { n32 := LOWESTBIT(n32); IDX_32(n32); }
   --  arg-macro: procedure HIGHESTBITIDX_32 (n32)
   --    { LwU32 count := 0; while (n32 >>= 1) { count++; } n32 := count; }
   --  arg-macro: procedure ROUNDUP_POW2 (n32)
   --    { n32--; n32 |= n32 >> 1; n32 |= n32 >> 2; n32 |= n32 >> 4; n32 |= n32 >> 8; n32 |= n32 >> 16; n32++; }
   --  arg-macro: procedure ROUNDUP_POW2_U64 (n64)
   --    { n64--; n64 |= n64 >> 1; n64 |= n64 >> 2; n64 |= n64 >> 4; n64 |= n64 >> 8; n64 |= n64 >> 16; n64 |= n64 >> 32; n64++; }
   --  arg-macro: procedure LW_SWAP_U8 (a, b)
   --    { LwU8 temp; temp := a; a := b; b := temp; }
   --  arg-macro: procedure LW_SWAP_U32 (a, b)
   --    { LwU32 temp; temp := a; a := b; b := temp; }
   --  unsupported macro: FOR_EACH_INDEX_IN_MASK(maskWidth,index,mask) { LwU ##maskWidth lclMsk = (LwU ##maskWidth)(mask); for ((index) = 0U; lclMsk != 0U; (index)++, lclMsk >>= 1U) { if (((LwU ##maskWidth)LWBIT64(0) & lclMsk) == 0U) { continue; }
   --  unsupported macro: FOR_EACH_INDEX_IN_MASK_END } }
   LW_ANYSIZE_ARRAY : constant := 1;  --  /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:729
   --  arg-macro: function LW_CEIL (a, b)
   --    return ((a)+(b)-1)/(b);
   --  arg-macro: procedure LW_DIV_AND_CEIL (a, b)
   --    LW_CEIL(a,b)
   --  arg-macro: function LW_MIN (a, b)
   --    return ((a) < (b)) ? (a) : (b);
   --  arg-macro: function LW_MAX (a, b)
   --    return ((a) > (b)) ? (a) : (b);
   --  arg-macro: function LW_ABS (a)
   --    return (a)>=0?(a):(-(a));
   --  arg-macro: function LW_SIGN (s)
   --    return (LwS8)(((s) > 0) - ((s) < 0));
   --  arg-macro: function LW_ZERO_SIGN (s)
   --    return (LwS8)((((s) >= 0) * 2) - 1);
   --  arg-macro: function LW_OFFSETOF (type, member)
   --    return (LwU32)__builtin_offsetof(type, member);
   --  arg-macro: function LW_UNSIGNED_ROUNDED_DIV (a, b)
   --    return ((a) + ((b) / 2)) / (b);
   --  arg-macro: function LW_UNSIGNED_DIV_CEIL (a, b)
   --    return ((a) + (b - 1)) / (b);
   --  arg-macro: function LW_SUBTRACT_NO_UNDERFLOW (a, b)
   --    return (a)>(b) ? (a)-(b) : 0;
   --  arg-macro: function LW_RIGHT_SHIFT_ROUNDED (a, shift)
   --    return ((a) >> (shift)) + notnot((LWBIT((shift) - 1) and (a)) = LWBIT((shift) - 1));
   --  arg-macro: function LW_ALIGN_DOWN (v, gran)
   --    return (v) and ~((gran) - 1);
   --  arg-macro: function LW_ALIGN_UP (v, gran)
   --    return ((v) + ((gran) - 1)) and ~((gran)-1);
   --  arg-macro: function LW_ALIGN_DOWN64 (v, gran)
   --    return (v) and ~(((LwU64)gran) - 1);
   --  arg-macro: function LW_ALIGN_UP64 (v, gran)
   --    return ((v) + ((gran) - 1)) and ~(((LwU64)gran)-1);
   --  arg-macro: function LW_IS_ALIGNED (v, gran)
   --    return 0 = ((v) and ((gran) - 1));
   --  arg-macro: function LW_IS_ALIGNED64 (v, gran)
   --    return 0 = ((v) and (((LwU64)gran) - 1));

  --**************************************************************************|*                                                                           *|
  --|
  --|*       Copyright 1993-2019 LWPU, Corporation.  All rights reserved.      *|
  --|*                                                                           *|
  --|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
  --|*     international laws.  LWPU, Corp. of Sunnyvale,  California owns     *|
  --|*     copyrights, patents, and has design patents pending on the design     *|
  --|*     and  interface  of the LW chips.   Users and  possessors  of this     *|
  --|*     source code are hereby granted a nonexclusive, royalty-free copy-     *|
  --|*     right  and design patent license  to use this code  in individual     *|
  --|*     and commercial software.                                              *|
  --|*                                                                           *|
  --|*     Any use of this source code must include,  in the user dolwmenta-     *|
  --|*     tion and  internal comments to the code,  notices to the end user     *|
  --|*     as follows:                                                           *|
  --|*                                                                           *|
  --|*     Copyright  1993-2019  LWPU,  Corporation.   LWPU  has  design     *|
  --|*     patents and patents pending in the U.S. and foreign countries.        *|
  --|*                                                                           *|
  --|*     LWPU, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
  --|*     OF THIS SOURCE CODE FOR ANY PURPOSE. IT IS PROVIDED "AS IS" WITH-     *|
  --|*     OUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  LWPU, CORPORATION     *|
  --|*     DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOURCE CODE, INCLUD-     *|
  --|*     ING ALL IMPLIED WARRANTIES  OF MERCHANTABILITY  AND FITNESS FOR A     *|
  --|*     PARTICULAR  PURPOSE.  IN NO EVENT  SHALL LWPU,  CORPORATION  BE     *|
  --|*     LIABLE FOR ANY SPECIAL,  INDIRECT,  INCIDENTAL,  OR CONSEQUENTIAL     *|
  --|*     DAMAGES, OR ANY DAMAGES  WHATSOEVER  RESULTING  FROM LOSS OF USE,     *|
  --|*     DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR     *|
  --|*     OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION  WITH THE     *|
  --|*     USE OR PERFORMANCE OF THIS SOURCE CODE.                               *|
  --|*                                                                           *|
  --|*     RESTRICTED RIGHTS LEGEND:  Use, duplication, or disclosure by the     *|
  --|*     Government is subject  to restrictions  as set forth  in subpara-     *|
  --|*     graph (c) (1) (ii) of the Rights  in Technical Data  and Computer     *|
  --|*     Software  clause  at DFARS  52.227-7013 and in similar clauses in     *|
  --|*     the FAR and NASA FAR Supplement.                                      *|
  --|*                                                                           *|
  -- \************************************************************************** 

  -- * lwmisc.h
  --  

  -- Miscellaneous macros useful for bit field manipulations
  -- STUPID HACK FOR CL 19434692.  Will revert when fix CL is delivered bfm -> chips_a.
  -- It is recommended to use the following bit macros to avoid macro name
  -- collisions with other src code bases.
  -- Index of the 'on' bit (assuming that there is only one).
  -- Even if multiple bits are 'on', result is in range of 0-31.
  -- Index of the 'on' bit (assuming that there is only one).
  -- Even if multiple bits are 'on', result is in range of 0-63.
  -- Desctructive, assumes only one bit set in n32
  -- Deprecated in favor of BIT_IDX_32.
  --!
  -- * DRF MACRO README:
  -- *
  -- * Glossary:
  -- *      DRF: Device, Register, Field
  -- *      FLD: Field
  -- *      REF: Reference
  -- *
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA                   0xDEADBEEF
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_GAMMA             27:0
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA             31:28
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_ZERO   0x00000000
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_ONE    0x00000001
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_TWO    0x00000002
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_THREE  0x00000003
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_FOUR   0x00000004
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_FIVE   0x00000005
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_SIX    0x00000006
  -- * #define LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA_SEVEN  0x00000007
  -- *
  -- *
  -- * Device = _DEVICE_OMEGA
  -- *   This is the common "base" that a group of registers in a manual share
  -- *
  -- * Register = _REGISTER_ALPHA
  -- *   Register for a given block of defines is the common root for one or more fields and constants
  -- *
  -- * Field(s) = _FIELD_GAMMA, _FIELD_ZETA
  -- *   These are the bit ranges for a given field within the register
  -- *   Fields are not required to have defined constant values (enumerations)
  -- *
  -- * Constant(s) = _ZERO, _ONE, _TWO, ...
  -- *   These are named values (enums) a field can contain; the width of the constants should not be larger than the field width
  -- *
  -- * MACROS:
  -- *
  -- * DRF_SHIFT:
  -- *      Bit index of the lower bound of a field
  -- *      DRF_SHIFT(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 28
  -- *
  -- * DRF_SHIFT_RT:
  -- *      Bit index of the higher bound of a field
  -- *      DRF_SHIFT_RT(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 31
  -- *
  -- * DRF_MASK:
  -- *      Produces a mask of 1-s equal to the width of a field
  -- *      DRF_MASK(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 0xF (four 1s starting at bit 0)
  -- *
  -- * DRF_SHIFTMASK:
  -- *      Produces a mask of 1s equal to the width of a field at the location of the field
  -- *      DRF_SHIFTMASK(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 0xF0000000
  -- *
  -- * DRF_DEF:
  -- *      Shifts a field constant's value to the correct field offset
  -- *      DRF_DEF(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, _THREE) == 0x30000000
  -- *
  -- * DRF_NUM:
  -- *      Shifts a number to the location of a particular field
  -- *      DRF_NUM(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, 3) == 0x30000000
  -- *      NOTE: If the value passed in is wider than the field, the value's high bits will be truncated
  -- *
  -- * DRF_SIZE:
  -- *      Provides the width of the field in bits
  -- *      DRF_SIZE(LW_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA) == 4
  -- *
  -- * DRF_VAL:
  -- *      Provides the value of an input within the field specified
  -- *      DRF_VAL(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, 0xABCD1234) == 0xA
  -- *      This is sort of like the ilwerse of DRF_NUM
  -- *
  -- * DRF_IDX...:
  -- *      These macros are similar to the above but for fields that accept an index argumment
  -- *
  -- * FLD_SET_DRF:
  -- *      Set the field bits in a given value with the given field constant
  -- *      LwU32 x = 0x00001234;
  -- *      x = FLD_SET_DRF(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, _THREE, x);
  -- *      x == 0x30001234;
  -- *
  -- * FLD_SET_DRF_NUM:
  -- *      Same as FLD_SET_DRF but instead of using a field constant a literal/variable is passed in
  -- *      LwU32 x = 0x00001234;
  -- *      x = FLD_SET_DRF_NUM(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, 0xF, x);
  -- *      x == 0xF0001234;
  -- *
  -- * FLD_IDX...:
  -- *      These macros are similar to the above but for fields that accept an index argumment
  -- *
  -- * FLD_TEST_DRF:
  -- *      Test if location specified by drf in 'v' has the same value as LW_drfc
  -- *      FLD_TEST_DRF(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, _THREE, 0x3000ABCD) == LW_TRUE
  -- *
  -- * FLD_TEST_DRF_NUM:
  -- *      Test if locations specified by drf in 'v' have the same value as n
  -- *      FLD_TEST_DRF_NUM(_DEVICE_OMEGA, _REGISTER_ALPHA, _FIELD_ZETA, 0x3, 0x3000ABCD) == LW_TRUE
  -- *
  -- * REF_DEF:
  -- *      Like DRF_DEF but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
  -- *      REF_DEF(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, _THREE) == 0x30000000
  -- *
  -- * REF_VAL:
  -- *      Like DRF_VAL but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
  -- *      REF_VAL(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, 0xABCD1234) == 0xA
  -- *
  -- * REF_NUM:
  -- *      Like DRF_NUM but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
  -- *      REF_NUM(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, 0xA) == 0xA00000000
  -- *
  -- * FLD_SET_REF_NUM:
  -- *      Like FLD_SET_DRF_NUM but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
  -- *      LwU32 x = 0x00001234;
  -- *      x = FLD_SET_REF_NUM(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, 0xF, x);
  -- *      x == 0xF0001234;
  -- *
  -- * FLD_TEST_REF:
  -- *      Like FLD_TEST_DRF but maintains full symbol name (use in cases where "LW" is not prefixed to the field)
  -- *      FLD_TEST_REF(SOME_OTHER_PREFIX_DEVICE_OMEGA_REGISTER_ALPHA_FIELD_ZETA, _THREE, 0x3000ABCD) == LW_TRUE
  -- *
  -- * Other macros:
  -- *      There a plethora of other macros below that extend the above (notably Multi-Word (MW), 64-bit, and some
  -- *      reg read/write variations). I hope these are self explanatory. If you have a need to use them, you
  -- *      probably have some knowlege of how they work.
  --  

  -- cheetah mobile uses lwmisc_macros.h and can't access lwmisc.h... and sometimes both get included.
  -- Use Coverity Annotation to mark issues as false positives/ignore when using single bit defines.
  -- Signed version of DRF_VAL, which takes care of extending sign bit.
  -- Fractional version of DRF_VAL which reads Fx.y fixed point number (x.y)*z
  -- 64 Bit Versions
  -- 32 Bit Versions
  -- FLD_SET_DRF_DEF is deprecated! Use an explicit assignment with FLD_SET_DRF instead.
  -- FLD_SET_DRF_DEF is deprecated! Use an explicit assignment with FLD_SET_DRF instead.
  -- Multi-word (MW) field manipulations.  For multi-word structures (e.g., Fermi SPH),
  -- fields may have bit numbers beyond 32.  To avoid errors using "classic" multi-word macros,
  -- all the field extents are defined as "MW(X)".  For example, MW(127:96) means
  -- the field is in bits 0-31 of word number 3 of the structure.
  -- DRF_VAL_MW() macro is meant to be used for native endian 32-bit aligned 32-bit word data,
  -- not for byte stream data.
  -- DRF_VAL_BS() macro is for byte stream data used in fbQueryBIOS_XXX().
  -- DRF_VAL_MW is the ONLY multi-word macro which supports spanning. No other MW macro supports spanning lwrrently
  -- Logically OR all DRF_DEF constants indexed from zero to s (semiinclusive).
  -- Caution: Target variable v must be pre-initialized.
  --------------------------------------------------------------------------//
  --                                                                        //
  -- Common defines for engine register reference wrappers                  //
  --                                                                        //
  -- New engine addressing can be created like:                             //
  -- \#define ENG_REG_PMC(o,d,r)                     LW##d##r               //
  -- \#define ENG_IDX_REG_CE(o,d,i,r)                CE_MAP(o,r,i)          //
  --                                                                        //
  -- See FB_FBPA* for more examples                                         //
  --------------------------------------------------------------------------//
  -- DRF_READ_1WORD_BS() and DRF_READ_1WORD_BS_HIGH() do not read beyond the bytes that contain
  -- the requested value. Reading beyond the actual data causes a page fault panic when the
  -- immediately following page happened to be protected or not mapped.
  -- Callwlate 2^n - 1 and avoid shift counter overflow
  -- On Windows amd64, 64 << 64 => 1
  -- operates on a 64-bit data type
  -- Destructive operation on n32
  -- Destructive operation on n32
  --!
  -- * Callwlate number of bits set in a 32-bit unsigned integer.
  -- * Pure typesafe alternative to @ref NUMSETBITS_32.
  --  

   function lwPopCount32 (x : lwtypes_h.LwU32) return lwtypes_h.LwU32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:571
   with Import => True, 
        Convention => C, 
        External_Name => "lwPopCount32";

  --!
  -- * Callwlate number of bits set in a 64-bit unsigned integer.
  --  

   function lwPopCount64 (x : lwtypes_h.LwU64) return lwtypes_h.LwU32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:584
   with Import => True, 
        Convention => C, 
        External_Name => "lwPopCount64";

  --!
  -- * Determine how many bits are set below a bit index within a mask.
  -- * This assigns a dense ordering to the set bits in the mask.
  -- *
  -- * For example the mask 0xCD contains 5 set bits:
  -- *     lwMaskPos32(0xCD, 0) == 0
  -- *     lwMaskPos32(0xCD, 2) == 1
  -- *     lwMaskPos32(0xCD, 3) == 2
  -- *     lwMaskPos32(0xCD, 6) == 3
  -- *     lwMaskPos32(0xCD, 7) == 4
  --  

   function lwMaskPos32 (mask : lwtypes_h.LwU32; bitIdx : lwtypes_h.LwU32) return lwtypes_h.LwU32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:606
   with Import => True, 
        Convention => C, 
        External_Name => "lwMaskPos32";

  -- Destructive operation on n32
  -- Destructive operation on n32
  -- Destructive operation on n32
  --!
  -- * Round up a 32-bit unsigned integer to the next power of 2.
  -- * Pure typesafe alternative to @ref ROUNDUP_POW2.
  -- *
  -- * param[in] x must be in range [0, 2^31] to avoid overflow.
  --  

   function lwNextPow2_U32 (x : lwtypes_h.LwU32) return lwtypes_h.LwU32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:648
   with Import => True, 
        Convention => C, 
        External_Name => "lwNextPow2_U32";

   function lwPrevPow2_U32 (x : lwtypes_h.LwU32) return lwtypes_h.LwU32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:663
   with Import => True, 
        Convention => C, 
        External_Name => "lwPrevPow2_U32";

  -- Destructive operation on n64
  --!
  -- * @brief   Macros allowing simple iteration over bits set in a given mask.
  -- *
  -- * @param[in]       maskWidth   bit-width of the mask (allowed: 8, 16, 32, 64)
  -- *
  -- * @param[in,out]   index       lvalue that is used as a bit index in the loop
  -- *                              (can be declared as any LwU* or LwS* variable)
  -- * @param[in]       mask        expression, loop will iterate over set bits only
  --  

  -- Size to use when declaring variable-sized arrays
  -- Returns ceil(a/b)
  -- Clearer name for LW_CEIL
  -- Returns absolute value of provided integer expression
  -- Returns 1 if input number is positive, 0 if 0 and -1 if negative. Avoid
  -- macro parameter as function call which will have side effects.
  -- Returns 1 if input number is >= 0 or -1 otherwise. This assumes 0 has a
  -- positive sign.
  -- Returns the offset (in bytes) of 'member' in struct 'type'.
  -- Performs a rounded division of b into a (unsigned). For SIGNED version of
  -- LW_ROUNDED_DIV() macro check the comments in http://lwbugs/769777.
  --!
  -- * Performs a ceiling division of b into a (unsigned).  A "ceiling" division is
  -- * a division is one with rounds up result up if a % b != 0.
  -- *
  -- * @param[in] a    Numerator
  -- * @param[in] b    Denominator
  -- *
  -- * @return a / b + a % b != 0 ? 1 : 0.
  --  

  --!
  -- * Performs subtraction where a negative difference is raised to zero.
  -- * Can be used to avoid underflowing an unsigned subtraction.
  -- *
  -- * @param[in] a    Minuend
  -- * @param[in] b    Subtrahend
  -- *
  -- * @return a > b ? a - b : 0.
  --  

  --!
  -- * Performs a rounded right-shift of 32-bit unsigned value "a" by "shift" bits.
  -- * Will round result away from zero.
  -- *
  -- * @param[in] a      32-bit unsigned value to shift.
  -- * @param[in] shift  Number of bits by which to shift.
  -- *
  -- * @return  Resulting shifted value rounded away from zero.
  --  

  -- Power of 2 alignment.
  --    (Will give unexpected results if 'gran' is not a power of 2.)
   function LWMISC_MEMSET
     (s : System.Address;
      c : lwtypes_h.LwU8;
      n : lwtypes_h.LwLength) return System.Address  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:844
   with Import => True, 
        Convention => C, 
        External_Name => "LWMISC_MEMSET";

   function LWMISC_MEMCPY
     (dest : System.Address;
      src : System.Address;
      n : lwtypes_h.LwLength) return System.Address  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:859
   with Import => True, 
        Convention => C, 
        External_Name => "LWMISC_MEMCPY";

   function LWMISC_STRNCPY
     (dest : Interfaces.C.char_array;
      src : Interfaces.C.char_array;
      n : lwtypes_h.LwLength) return Interfaces.C.char_array  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/inc/lwpu-sdk/lwmisc.h:874
   with Import => True, 
        Convention => C, 
        External_Name => "LWMISC_STRNCPY";

end lwmisc_h;
