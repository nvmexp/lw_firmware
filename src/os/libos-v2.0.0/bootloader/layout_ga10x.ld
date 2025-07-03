/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

OUTPUT_ARCH( "riscv" )
ENTRY(start)

MEMORY {
    imem : ORIGIN = 0x100000, LENGTH = 0x16000
    stack_chk_guard : ORIGIN = 0x181400, LENGTH = 0x8
}


SECTIONS
{
  . = ABSOLUTE(0);
  .text : {
    *(.start)
    *(.text*)
    *(.bss*)
    *(.data*)
    *(.sdata*)
    *(.sbss*)
    *(.rodata*)
    *(.got*)
  }  >imem

  .trash : {
    *(.interp*)
    *(.note.gnu.build-id)
    *(.dynsym)
    *(.dynstr)
    *(.gnu.hash)
  }  >imem

  .stack_chk_guard : {
    *(.stack_chk_guard)
  }  >stack_chk_guard
}
