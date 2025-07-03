OUTPUT_FORMAT("elf32-falcon", "elf32-falcon", "elf32-falcon")
OUTPUT_ARCH(falcon)

ENTRY(_start)

MEMORY
{
{{ EMIT_MEMORY_SECTIONS() }}
}

PROVIDE (__spMin = _spMin);
PROVIDE (__stack = LENGTH(DATA) - 0x1);
PROVIDE (__dataEnd = _dataEnd);

SECTIONS
{
    /*
     * RESIDENT CODE SECTION
     *
     * Define the components which comprise the Resident Code Section. The
     * Resident Code Section is a logical code section which physically maps
     * to the .text section of the image.
     */

	__resident_code_start = .;
	.text :
	{
		*(.start)
     	*(.resident.*)
     	*(.text.*)
     	*(.imem*)
     	. = ALIGN(0x100);
    }
    
    /* overlays using imem-va */

    .init ALIGN(0x100) :
    {      
    	__init_code_start = .;
    	*(.init.*) *.o(.init)
    	*(.data_handling.*) *.o(.data_handling)
    	__init_code_end = .;
    	. = ALIGN(0x100);
    	__resident_code_end = .;     /* this will allow to soruce the init code w/ the bootloader */
    } 
    .mclk ALIGN(0x100) :
    {
    	__mclk_code_start = .;
    	*(.memory.*) *.o(.memory)
		__mclk_code_end = .;
		. = ALIGN(0x100);
    } 
    /* temperature tracking is lwrrenty resident code, this segment is not deployed */
    .temp ALIGN(0x100) :
    {
    	__temp_tracking_code_start = .;
    	*(.temp.*) *.o(.temp)
		__temp_tracking_code_end = .;
		. = ALIGN(0x100);
    } 
    
    
    
    /*
     * RESIDENT DATA SECTION
     *  the start of the cmd queue section is aligned to 0x100 to have an alignement with
     *  eventual security masks
     */


    .data : {
       _dataBaseStart = .; 
       __ovly_table = .;
       LONG(ABSOLUTE(ADDR(.init)));        LONG(SIZEOF(.init));        LONG(LOADADDR(.init));        LONG(0x74696E69);
       LONG(ABSOLUTE(ADDR(.mclk)));        LONG(SIZEOF(.mclk));        LONG(LOADADDR(.mclk));        LONG(0x6B6C636D);
       LONG(ABSOLUTE(ADDR(.temp)));        LONG(SIZEOF(.temp));        LONG(LOADADDR(.temp));        LONG(0x706D6574);
       LONG(ABSOLUTE(ADDR(.data_paflcn))); LONG(SIZEOF(.data_paflcn)); LONG(LOADADDR(.data_paflcn)); LONG(0x6C666170);
       __pafalcon_location_table = .;
       INCLUDE "./_out/fbfalcon/{{ EMIT_PROFILE_DEFINITION('%s','PAFALCON_INCLUDE') }}/g_fbfalconuc_{{ EMIT_PROFILE_DEFINITION('%s','PAFALCON_INCLUDE') }}_painfo_hexdump.ld"
       *(.rodata)
       *(.data)

       . = ALIGN(256);
       *(.bss)
       *(COMMON)
       . = ALIGN(256);
       _dataEnd = .;
       ASSERT ((___stack_chk_guard < _dataEnd), "Error: Canary not defined inside basic data space");
     }
     
     OVERLAY ALIGN(0x100):
     {
    	.data_paflcn
    	{  
           __pa_start = .;
           LONG(0x6C666170);       /* 'pafl' */
           __pa_desc_start = .;
           INCLUDE "./_out/fbfalcon/{{ EMIT_PROFILE_DEFINITION('%s','PAFALCON_INCLUDE') }}/g_fbfalconuc_{{ EMIT_PROFILE_DEFINITION('%s','PAFALCON_INCLUDE') }}_desc_hexdump.ld"
           __pa_desc_end = .;
           . = ALIGN(0x100);
           __pa_code_start = .;
           INCLUDE "./_out/fbfalcon/{{ EMIT_PROFILE_DEFINITION('%s','PAFALCON_INCLUDE') }}/g_fbfalconuc_{{ EMIT_PROFILE_DEFINITION('%s','PAFALCON_INCLUDE') }}_imem_hexdump.ld"
           __pa_code_end = .;
           . = ALIGN(0x100);
           __pa_data_start = .;
           INCLUDE "./_out/fbfalcon/{{ EMIT_PROFILE_DEFINITION('%s','PAFALCON_INCLUDE') }}/g_fbfalconuc_{{ EMIT_PROFILE_DEFINITION('%s','PAFALCON_INCLUDE') }}_dmem_hexdump.ld"
           __pa_data_end = .;
           . = ALIGN(0x100);
           __pa_end = .;
           _dataBaseEnd = .;
           /* pafalcon imem/dmem is 8K each, check that we don't include more than that */
           ASSERT (SIZEOF(.data_paflcn) < (16 * 1024), "section .data_paflcn is exceeding 16K");
    	}
    	.data_sequencer
    	{
    	   _sequencerDataStart = .;
    	   *(.sequencerData.*)
    	   . = ALIGN(0x100);
    	   _sequencerDataEnd = .;
           /* pafalcon dmem is 8K, sequencer can have most of it but lets do a safety check at 6K */
           ASSERT (SIZEOF(.data_sequencer) < (6 * 1024), "section .data_sequencer is exceeding 6K");
    	}
    	.data_training
    	{
           _trainingDataStart = .;
           *(.fbioRegisterSpace.*)
           . = ALIGN(0x100);
           _trainingDataEnd = .;
           /* debug Data needs to aligned behind sequencer data and training data */
           _debugDataOffset = MAX(_trainingDataEnd,_sequencerDataEnd) - _trainingDataEnd;
           . = . + _debugDataOffset;
           _debugDataStart = .;
    	   *(.debugDataSpace.*)          
    	   _debugDataEnd = .;
    	   /* check that there is no overlap between pa sequencer space and debug data space */
    	   ASSERT (_sequencerDataEnd <= _debugDataStart, "overlap on sequencer data and debug data");
           /* being an overlay to the pafalcon binary make sure this can't gobble up excessive space */
           ASSERT (SIZEOF(.data_training) < (16 * 1024), "section .data_training is exceeding 16K");
    	} 
     } > DATA
     
     OVERLAY ALIGN(0x100):
     {
        .vbios
        {
           . = ALIGN(256);
           _vbiosTableStart = .;
           *(.vbiosTable.*)
           *(.vbiosTableReferences.*)
           _vbiosTableEnd = .;
        }
        .debugbuffer
        {
           . = ALIGN(256);
           *(.db.*)
        }
     } > DATA
     
     .data2 : 
     {
       __dataOverlayEnd = .;
       . = ALIGN(256);
       *(.ppArea.*)

       _spMin = .;
       . = LENGTH(DATA) - SIZEOF(.data) - (__dataOverlayEnd - __pa_start); 

     }
    
    
    _dataBaseFB     = ADDR(.data);
    _dataSize       = SIZEOF(.data) + SIZEOF(.data2);
    _dataBaseDMEM   = ADDR(.data);
    _dataLength     = LENGTH(DATA);

    .desc ORIGIN( DESC ):
    {
        LONG(__resident_code_start);
        LONG(__resident_code_end);
        LONG(_dataBaseEnd - _dataBaseStart);
        LONG(0); /* __num_imem_overlays */
        LONG(0); /* __num_imem_hs_overlays */
        LONG(0); /* __num_dmem_overlays */
    } > DESC 
     
    /*
     * DWARF2 debug sections. Symbols in the DWARF debugging sections are
     * relative to the beginning of the section so we begin them at 0.
     */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_ranges   0 : { *(.debug_ranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /DISCARD/ : { *(.note.GNU-stack) }

}

NOCROSSREFS(.init .mclk .temp);
NOCROSSREFS(.ovl);
