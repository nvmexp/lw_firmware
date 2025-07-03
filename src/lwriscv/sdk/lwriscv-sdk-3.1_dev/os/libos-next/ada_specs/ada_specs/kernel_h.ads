pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;
with lwtypes_h;

package kernel_h is

   --  unsupported macro: LIBOS_SECTION_IMEM_PINNED __attribute__((section(".kernel.hot.text")))
   --  unsupported macro: LIBOS_SECTION_DMEM_PINNED(name) __attribute__((section(".kernel.hot.data." #name))) name
   --  unsupported macro: LIBOS_SECTION_DMEM_COLD(name) __attribute__((section(".kernel.cold.data." #name))) name
   --  unsupported macro: LIBOS_SECTION_TEXT_STARTUP __attribute__((section(".kernel.startup")))
   --  unsupported macro: LIBOS_NORETURN __attribute__((noreturn, used, nothrow))
   --  unsupported macro: LIBOS_NAKED __attribute__((naked))
   --  unsupported macro: LIBOS_SECTION_TEXT_COLD __attribute__((noinline, section(".kernel.cold.text")))
   --  unsupported macro: LIBOS_DECLARE_LINKER_SYMBOL_AS_INTEGER(x) extern LwU8 notaddr_ ##x[];
   --  unsupported macro: LIBOS_REFERENCE_LINKER_SYMBOL_AS_INTEGER(x) static const LwU64 x = (LwU64)notaddr_ ##x;
   --  arg-macro: procedure KernelMmioWrite (addr, value)
   --    *(volatile LwU32 *)(LIBOS_CONFIG_PRI_MAPPING_BASE + addr - LW_RISCV_AMAP_INTIO_START) := (value);
   --  arg-macro: function KernelMmioRead (addr)
   --    return *(volatile LwU32 *)(LIBOS_CONFIG_PRI_MAPPING_BASE + addr - LW_RISCV_AMAP_INTIO_START);
   --  arg-macro: function LO32 (x)
   --    return (LwU32)(x);
   --  arg-macro: function HI32 (x)
   --    return (LwU32)(x >> 32);
  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

  -- Use for accessing linker symbols where the value isn't an address
  -- but rather a value (such as a size).
  -- CSR helpers
   function KernelCsrRead (csr : lwtypes_h.LwU64) return lwtypes_h.LwU64  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:46
   with Import => True, 
        Convention => C, 
        External_Name => "KernelCsrRead";

   procedure KernelCsrWrite (csr : lwtypes_h.LwU64; value : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:54
   with Import => True, 
        Convention => C, 
        External_Name => "KernelCsrWrite";

   procedure KernelCsrSet (csr : lwtypes_h.LwU64; value : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:59
   with Import => True, 
        Convention => C, 
        External_Name => "KernelCsrSet";

   procedure KernelCsrClear (csr : lwtypes_h.LwU64; value : lwtypes_h.LwU64)  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:60
   with Import => True, 
        Convention => C, 
        External_Name => "KernelCsrClear";

   procedure KernelFenceFull  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:62
   with Import => True, 
        Convention => C, 
        External_Name => "KernelFenceFull";

  -- Per chip entry point for kernel
   procedure KernelBootstrap  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:65
   with Import => True, 
        Convention => C, 
        External_Name => "KernelBootstrap";

   procedure KernelInit  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:70
   with Import => True, 
        Convention => C, 
        External_Name => "KernelInit";

   procedure KernelPanic  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:73
   with Import => True, 
        Convention => C, 
        External_Name => "KernelPanic";

  -- MISRA workarounds for sign colwersion
   function CastUInt32 (val : lwtypes_h.LwS32) return lwtypes_h.LwU32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:79
   with Import => True, 
        Convention => C, 
        External_Name => "CastUInt32";

   function CastUInt64 (val : lwtypes_h.LwS64) return lwtypes_h.LwU64  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:81
   with Import => True, 
        Convention => C, 
        External_Name => "CastUInt64";

   function CastInt32 (val : lwtypes_h.LwU32) return lwtypes_h.LwS32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:83
   with Import => True, 
        Convention => C, 
        External_Name => "CastInt32";

   function Widen (val : lwtypes_h.LwU32) return lwtypes_h.LwU64  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:85
   with Import => True, 
        Convention => C, 
        External_Name => "Widen";

   function Log2OfPowerOf2 (pow2 : lwtypes_h.LwU64) return lwtypes_h.LwU32  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/kernel/kernel.h:87
   with Import => True, 
        Convention => C, 
        External_Name => "Log2OfPowerOf2";

end kernel_h;
