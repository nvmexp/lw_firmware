
OUTPUT_ARCH( "riscv" )

SECTIONS
{
  . = 0x10000;

  /* The rodata section is generally marked as SHF_MERGE
   * This must be cleared or constants between isolated
   * images may be merged
   */
  .rodata : {
      *(.rodata*)
      *(COMMON)
  }
}

