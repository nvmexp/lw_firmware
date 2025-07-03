//sw/dev/gpu_drv/chips_a/uproc/fbflcn/src/fbflcn_table.c#31 - edit change 23871950 (text)
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

// include manuals
#include "dev_fbfalcon_pri.h"
#include "dev_fbfalcon_csb.h"
#include "dev_therm.h"
#include "dev_fuse.h"
#include "dev_fbpa.h"

#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "data.h"
#include "priv.h"

#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
extern OBJFBFLCN Fbflcn;
#include "config/g_memory_private.h"
#include "config/g_fbfalcon_private.h"


// compile time check if we build either GDDR or HBM target
CASSERT(FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR) ^ FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM), pick_either_hbm_or_gddr__fbflcn_table_c)

// define aperture for vbios table data
static LwU8 gVbiosTable[CFG_BIOS_TABLE_ALLOCATION]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("vbiosTable", "gVbiosTable")
    GCC_ATTRIB_ALIGN(0x4);

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))

typedef LwU8 DirtTableId;

static const DirtTableId miniDirtTableIdList[] = {
        FW_MEMORY_PERFORMANCE_TABLE_ID,
        FW_MEMORY_INFORMATION_TABLE_ID,
        FW_MEMORY_PLL_INFORMATION_TABLE_ID,
        FW_MEMORY_CLOCK_TABLE_ID,
        FW_MEMORY_TWEAK_TABLE_ID,
        FW_MEMORY_BOOT_TRAINING_TABLE_ID,
        FW_MEMORY_TRAINING_TABLE_ID,
        FW_BIT_INTERNAL_USE_ONLY,
        FW_MEMORY_TRAINING_PATTERN_TABLE_ID,
#if ((FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_GPIO_TABLE)))
        LW_GFW_DIRT_GPIO_TABLE,
#endif
};

#define MINI_DIRT_TABLE_ENTRIES  sizeof(miniDirtTableIdList)/sizeof(DirtTableId)

sMiniDirtTableEntry gMiniDirtTable[MINI_DIRT_TABLE_ENTRIES];
#endif

// compile time asserts for alignment and size
// LwU32 alignment assumptions are built into the ucode do not remove / modify this w/o a dislwssion
CASSERT( ( (((LwU32)(&gVbiosTable[0])) % sizeof(LwU32)) == 0), fbflcn_table_c);
CASSERT( ( (CFG_BIOS_TABLE_ALLOCATION % sizeof(LwU32)) == 0), fbflcn_table_c);

// define references to bios table data
BIOS_TABLE_STRUCT gBiosTable
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("vbiosTableReferences.static", "gBiosTable")
    GCC_ATTRIB_ALIGN(0x4);
// compile time asserts for alignment and size
// LwU32 alignment assumptions are built into the ucode do not remove / modify this w/o a dislwssion
CASSERT( ( (((LwU32)(&gBiosTable)) % sizeof(LwU32)) == 0), fbflcn_table_c);
CASSERT( ( (sizeof(BIOS_TABLE_STRUCT) % sizeof(LwU32)) == 0), fbflcn_table_c);

// define reference to selected targets bios data based of frequency
BIOS_TARGET_TABLE gTT
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("vbiosTableReferences.dynamic", "gTT")
    GCC_ATTRIB_ALIGN(0x4);
// compile time asserts for alignment and size
// LwU32 alignment assumptions are built into the ucode do not remove / modify this w/o a dislwssion
CASSERT( ( (((LwU32)(&gTT)) % sizeof(LwU32)) == 0), fbflcn_table_c);
CASSERT( ( (sizeof(BIOS_TARGET_TABLE) % sizeof(LwU32)) == 0), fbflcn_table_c);


// logger on table processing checks, if enabled all checks that would lead to a halt when failing
// will report processing variables.

#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100)) && FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))
#define NUM_PATRAM_ENTRIES      32
#define PATRAM_BURSTS_PER_ENTRY 8
LwU32 CharPatramDQ_rd[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY];  // RdPatternDp
LwU8  CharPatramDBI_rd[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY]; // RdPatternDpContd
LwU8  CharPatramECC_rd[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY]; // RdPatternDpContd(2)
LwU32 CharPatramDQ_wr[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY];  // WrPatternDp        
LwU8  CharPatramDBI_wr[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY]; // WrPatternDpContd   
LwU8  CharPatramECC_wr[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY]; // WrPatternDpContd(2)
// note nitroc generated patram (training.rom) needs columns to be reversed for array
#elif (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
LwU32 WrPatternDp[200];
LwU32 WrPatternDpContd[200];
LwU32 RdPatternDp[200];
LwU32 RdPatternDpContd[200];
#else
LwU32 WrPatternDp[260];
LwU32 WrPatternDpContd[260];
LwU32 RdPatternDp[260];
LwU32 RdPatternDpContd[260];
#endif
LwU32 AddrPatternDp[256];


//
// isPrivSecEnabled
//   check opt_priv_sec_en fuse whether priv security is enabled. This fuse is
//   used to overwrite priv security but it does not change the readings of the
//   PLM registers.  Any check on PLM's should use this function to bypass when
//   the fuse is used to disable priv security
//

LwBool isPrivSecEnabled
(
        void
)
{
    LwU32 privSecVal = REG_RD32(BAR0, LW_FUSE_OPT_PRIV_SEC_EN);
    if (FLD_TEST_DRF(_FUSE_OPT, _PRIV_SEC_EN, _DATA, _YES, privSecVal))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}

/*
 * initTable:
 *  setup table at boot, this should be called only once
 *
 */
LW_STATUS
initTable
(
        void
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_INCLUDED_IN_BINARY))
    return LW_OK;
#endif


 	//FW_MBOX_WR32(1, isPrivSecEnabled());
 	//FW_MBOX_WR32(2, REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03_PRIV_LEVEL_MASK));
 	//FW_MBOX_WR32(3, REG_RD32(BAR0, LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2));
    // check if we have the correct security level set on the
    //  LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03 registers

    if(isPrivSecEnabled() == LW_TRUE)
    {
        LwU32 regPlm;
        regPlm = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03_PRIV_LEVEL_MASK);

        if (!(FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_03_PRIV_LEVEL_MASK,
                _READ_PROTECTION_LEVEL0,  _ENABLE, regPlm)))
        {
//	        FW_MBOX_WR32(8, FBFLCN_ERROR_CODE_SELWRITY_VIOLATION_AON_SCRATCH_GROUP3);
            return FBFLCN_ERROR_CODE_SELWRITY_VIOLATION_AON_SCRATCH_GROUP3;
        }
    }

    // error code defintions in ucode_postcodes.h
    //LwU32 scratchCode = REF_NUM(LW_UCODE_POST_CODE_FBFALCON_FRTS_PROG_CODE,
    //        LW_UCODE_FBFALCON_FRTS_PROG_NOT_STARTED) |
    //                REF_NUM(LW_UCODE_POST_CODE_FBFALCON_FRTS_ERR_CODE,
    //                        LW_UCODE_ERR_CODE_NOERROR);
    //
    //FW_MBOX_WR32_STALL(15, scratchCode);

    //* Step A.  Check if FRTS command has been successfully completed and if
    //  ACR is ready for LS uCode to fetch

    LwU32 frtsOutput = REG_RD32(BAR0,
            LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2);

    if (FLD_TEST_REF(
            LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_CMD_SUCCESSFUL,
            _NO, frtsOutput))
    {
        // TODO: this is the first check for availability of FRTS, this should
        //       be communicated back to RM instead of simply halting, hash
        //       out abort and communication w/ RM
// 	    FW_MBOX_WR32(9, FBFLCN_ERROR_CODE_AON_SCRATCH_GROUP_UNSUCCESSFUL);
        return FBFLCN_ERROR_CODE_AON_SCRATCH_GROUP_UNSUCCESSFUL;
    }


    //* Step B. Read FRTS secure structure for FW Descriptor Struct location
    //          in WPR(FB) or Sysmem

    //LwU32 frtsInput = REG_RD32(BAR0,
    //                    LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2);
    LwU32 wpr_ctrl =  REF_VAL(
            LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_USAGE,
            REG_RD32(BAR0, LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2));

    LwU32 wpr_offset = REF_VAL(
            LW_PGC6_AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1_WPR_OFFSET,
            REG_RD32(BAR0, LW_PGC6_AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1));

    //LwU32 wpr_size = REF_VAL(
    //        LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_WPR_SIZE,
    //        REG_RD32(BAR0, LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0));


    //* Step C.  Parsing FRTS secure structure for final Media type and VBIOS
    //           section start offset
    //* Media Type could be system memory,  or FB
    //  WPR ID ( 0 means regular FB.  WPR ID only valid when media type is FB

    LwU8 fwImageMediaType = (LwU8)REF_VAL( LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE, wpr_ctrl);
    LwU8 fwImageWprId = (LwU8)REF_VAL( LW_IN_CMD_FRTS_SEC_CTRL_WPR_ID, wpr_ctrl);
    //LwU8 fwImageIsWpr = (LwU8)REF_VAL( LW_IN_CMD_FRTS_SEC_CTRL_WPR_ID, wpr_ctrl);

    LwU32 data = 0;
    LwU8 ctxDma = CTX_DMA_FBFALCON_READ_VBIOSTABLES;

    if ( LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_FB == fwImageMediaType ) {
        //lwr->cfg0 = FLD_SET_DRF_NUM(_PFB,_FBPA_CFG0,_DRAM_ACK,enable,lwr->cfg0);
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_TARGET , _LOCAL_FB, data);
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_MEM_TYPE, _PHYSICAL, data);
    }
    else if ( LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_SYSMEM_NONCOHERENT ==  fwImageMediaType) {
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_TARGET, _NONCOHERENT_SYSMEM, data);
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_MEM_TYPE, _PHYSICAL, data);
    }
    else if ( LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_SYSMEM_COHERENT == fwImageMediaType )
    {
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_TARGET, _COHERENT_SYSMEM, data );
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_MEM_TYPE, _VIRTUAL, data);
    }
    else
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_CERT21_FRTS_DMA_UNEXPECTED_MEDIA_TYPE);
    }

    // DMA Setup - Step 1.  Set FBIF_TRANSCFG(0)
    // See https://rmopengrok.lwpu.com/source/xref/chips_a/uproc/acr/src/acr/
    //             maxwell/acrucgm200.c#109

    REG_WR32_STALL(BAR0, LW_CFBFALCON_FBIF_TRANSCFG(ctxDma), data);

    // DMA Setup - Step 2.  Set FBIF_REGIONCFG for WPR
    // See https://rmopengrok.lwpu.com/source/xref/chips_a/uproc/acr/src/
    //             acrlib/maxwell/acruclibgm200.c#1127
    LwU32 mask = ~ ( 0xF << (ctxDma*4) );

    data = REG_RD32(CSB, LW_CFBFALCON_FBIF_REGIONCFG);
    data = (data & mask) | ((fwImageWprId & 0xF) << (ctxDma * 4));
    REG_WR32_STALL(CSB, LW_CFBFALCON_FBIF_REGIONCFG,  data);

    // FW section memory location is in 4K unit.
    //LwU64 fbAddr = (LwU64)wpr_offset;
    //fbAddr *= 4096;

    // wpr_offset is 4K aligned, compute correct 64 bit address

    LwU32 vbiosTableStart_lo = (wpr_offset & 0x000FFFFF) << 12;
    LwU32 vbiosTableStart_hi = (wpr_offset & 0xFFF00000) >> 20;
    // program as base address for dma access through fbif
    falc_wspr(DMB, (( (vbiosTableStart_lo & 0xFFFFFF00) >> 8) |
            ( (vbiosTableStart_hi & 0x000000FF) << 24)));
    falc_wspr(DMB1, ( (vbiosTableStart_hi & 0xFFFFFF00) >> 8));

    LwU16 ctx = 0;
    falc_rspr(ctx, CTX);  // Read existing value
    ctx = (((ctx) & ~(0x7 << CTX_DMAIDX_SHIFT_DMREAD)) | ((ctxDma) << CTX_DMAIDX_SHIFT_DMREAD));
    falc_wspr(CTX, ctx); // Write new value

    return LW_OK;
}



/*
 * loadFb2DMem
 *  Load a data segment from dram to fbfalcon's dmem through the fbif dma
 *  engine. Due to shared address alignement requirements for both source
 *  and destination its diffilwlt to use a bigger blit area so each request
 *  fetches just one dword.
 *    from: start address offset in fb dram
 *    to: absolute start address in falcon dmem
 *    size: size of fetch (dword aligned)
 *
 *                   to:      from:    end:     size:
 * 1) load FRTS:      0x0100   0x0000   0x0028   0x0028
 * 2) dirt:           0x0128   0x0028   0x06a8   0x0680
 * 3) meminfotable    0x07a8   0x0908   0x0914   0x000c
 * 4) pllinfotable    0x07b4   0x0c4c   0x0d5c   0x0110
 */


void
loadFb2DMem
(
        LwU32 from,
        LwU32 to,
        LwU32 size
)
{
    LwU32 end = from + size;
    while (from < end)
    {
        //
        // bits 18:16 of second operand of dmread defines the actual size of
        // transfer (6 = chunk of 256, 0 = chunk of 4)
        //
        falc_dmread(from, to + 0x00000);
        from += 4;
        to += 4;

    }
    falc_dmwait();
    // for gddr6_table_load_halt_tp bug 200565863 and 2761992
    // NOTE: only for RTL sim, comment out after creating binary
    // do not remove
    //FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DEBUG_HALT)

}


/*
 * front padding
 *  aligns the table to a dword for dma fetch minimal size
 */

inline
LwU8
frontPadding
(
        LwU32 *pAdr
)
{
    LwU8 padding = (*pAdr) % sizeof(LwU32);
    (*pAdr) = (*pAdr) & 0xFFFFFFFC;
    return padding;
}


/*
 * back padding
 *  aligns table in the back to be dma size aligned
 */

inline
LwU16
backPadding
(
        LwU16 size,
        LwU8 padding
)
{
    LwU16 structSizePadded = LW_ALIGN_UP(size+ padding, sizeof(LwU32));
    return structSizePadded;
}


/*
 * loadTableSegment
 *   load a table segement from fb memory to dmem in fbfalcon and
 *   adds the necessary padding at the front and the end to be
 *   dword aligned to match dma access size.
 *
 *     from:   source location table offset in FB
 *     to:     destination location in dmem, this adress will be
 *             updated to point to the next free location in the
 *             dmem once the structure has been read.
 *     size:   table segment size
 *     segmentId: an identifier pointing to the table being loaded,
 *                purely for error reporting.
 *     return: pointer to the loaded data segment in dmem.
 *
 */

LwU32
loadTableSegment
(
        LwU32 from,
        LwU32 *to,
        LwU32 size
)
{

    LwU32 fromOffset = from;

    // front padding for dword start alignment
    // this will round down the fromOffset to be dword aligned
    LwU8 padding = frontPadding(&fromOffset);
    // end padding for dword end alignement
    LwU16 structSizePadded = backPadding(size, padding);

    LwU32 endAdr = structSizePadded + *to;
    LwBool bError;
    bError = (endAdr >= (LwU32)&gVbiosTable[CFG_BIOS_TABLE_ALLOCATION-1]);
    if (bError)
    {
        //FW_MBOX_WR32_STALL(7, size);        // (0xe8)
        //FW_MBOX_WR32_STALL(8, (LwU32)to);   // (3fc8) 3fc8
        //FW_MBOX_WR32_STALL(9, (*to));       // (0xa30) a30
        //FW_MBOX_WR32_STALL(10, (LwU32)from);   // 4424
        //FW_MBOX_WR32_STALL(12, endAdr);        // (0xb1c) 47d4
        FBFLCN_HALT(FBFLCN_ERROR_CODE_VBIOS_TABLE_OVERFLOW);

    }

    // load perfMemTweak2xHeader and assign pointer to content
    loadFb2DMem (fromOffset, (*to), structSizePadded);

    LwU32 tablePtr = (*to) + padding;

    // update the dmem pointer to point to the next empty location
    (*to) = (*to) + structSizePadded;

    // return pointer to start of struct loaded
    return tablePtr;
}

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
pMiniDirtTableEntry
findMiniDirtIdEntry
(
        LwU8 dirtId
)
{
    LwU32 inx;
    for (inx = 0; inx < MINI_DIRT_TABLE_ENTRIES; inx++)
    {
        if (gMiniDirtTable[inx].dirtId == dirtId) {
            return &gMiniDirtTable[inx];
        }
    }
    return NULL;
}
#endif

/*
 * find dirt table entry
 *
 * returns: offset start relative to the VBIOS image
 *          https://confluence.lwpu.com/display/GFW/
 *                Firmware+Runtime+Security#FirmwareRuntimeSelwrity-OffsetField
 *
 */

FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR
findDirtIdTableEntry
(
        LwU8 dirt_id
)
{
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry = gBiosTable.vdpaTable;
    LwU16 entry_count = gBiosTable.fw_descriptor->vdpa_entry_count;


    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pValidVdpaEntry = NULL;
    LwU32 inx;
    for (inx = 0; inx < entry_count; inx++)
    {
        // VDPA table entries defintions in cert20/cert20.h,
        // the usage field in the the VDPA_ENTRY_TYPE contains the id to the
        // table
        LwU8 usage = (LwU8)REF_VAL(FBFLCN_VDPA_ENTRY_TYPE_USAGE,
                pVdpaEntry->type);

        // DIRT_Table_IDs defined in data_id_reference_table.h
        if ( usage == dirt_id )
        {
            pValidVdpaEntry = pVdpaEntry;
            break;
        }

        // change this code to not use the structure definition and its size
        // this is to ease the transition from cert2.0 to cert3.0
        pVdpaEntry =  (FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR)((LwU8*)pVdpaEntry + gBiosTable.fw_descriptor->vdpa_entry_size);
    }

    if (pValidVdpaEntry == NULL)
    {
        FW_MBOX_WR32_STALL(12, entry_count);
        FW_MBOX_WR32_STALL(13, dirt_id);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_VDPA_POINTER_NOT_FOUND);
    }

    return pValidVdpaEntry;
}

/*
 * load VDPA table
 * the vdpa table is a mapping of table index'es to location in the vbios,
 * think of it as the Table of Contents of the vbios data. This is dynamic
 * data creted by devinit or extracted into the frts file with a tool.
 */

void
loadVDPATable
(
        LwU32 *pdmem
)
{
    // Read the FW_IMAGE_DESCRIPTOR
    LwU32 fwImageOffset = 0;
    LwU32 fwImageSize = sizeof(FW_IMAGE_DESCRIPTOR);

    LwU32 fwImageStartAdr = loadTableSegment(fwImageOffset, pdmem, fwImageSize);
    FW_IMAGE_DESCRIPTOR* pFwDescriptor = (FW_IMAGE_DESCRIPTOR*)fwImageStartAdr;
    gBiosTable.fw_descriptor = pFwDescriptor;

    // check for magic word "GFWI" (0x49574647) in the fw image descriptor
    // identifier
    LwBool bError;
    bError = (pFwDescriptor->identifier != LW_FW_IMAGE_DESC_IDENTIFIER_SIG);
    if (bError)
    {
        //FW_MBOX_WR32_STALL(4, pFwDescriptor->identifier);
        //FW_MBOX_WR32_STALL(5, pFwDescriptor->version);
        //FW_MBOX_WR32_STALL(6, pFwDescriptor->size);
        //FW_MBOX_WR32_STALL(7, pFwDescriptor->flag);
        //FW_MBOX_WR32_STALL(8, pFwDescriptor->vdpa_header_offset);
        //FW_MBOX_WR32_STALL(9, pFwDescriptor->vdpa_entry_offset);
        //FW_MBOX_WR32_STALL(10, pFwDescriptor->vdpa_entry_count);
        //FW_MBOX_WR32_STALL(11, pFwDescriptor->vdpa_entry_size);
        //FW_MBOX_WR32_STALL(12, pFwDescriptor->fw_Image_offset);
        //FW_MBOX_WR32_STALL(13, pFwDescriptor->fw_Image_size);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_FW_IMAGE_IDENTIFIER_NOT_GFWI);
    }

    // load dirt table
    LwU32 vdpaEntryOffset = pFwDescriptor->vdpa_entry_offset;
    LwU32 vpdaSize = pFwDescriptor->vdpa_entry_count *
            pFwDescriptor->vdpa_entry_size;

    LwU32 vdpaTableAdr = loadTableSegment(vdpaEntryOffset, pdmem, vpdaSize);
    gBiosTable.vdpaTable = (FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR) vdpaTableAdr;

}


#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_PERFORMANCE_TABLE))
/*
 * load Performane Table
 *   performance state are used to read nominal frequency for P0 in order to get correct training frequency
 *   in boottraining and for P8 to drop by to boot frequency afterwards
 *
 */
void
loadPerformanceTable
(
        LwU32 *pdmem
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(FW_MEMORY_PERFORMANCE_TABLE_ID);
#else
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
    pVdpaEntry = findDirtIdTableEntry(FW_MEMORY_PERFORMANCE_TABLE_ID);
#endif

    if (pVdpaEntry == NULL)
    {
        return;
    }

    LwU32 pTTableOffset = pVdpaEntry->offset_start;
    LwU32 pTTableSize = REF_VAL(FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE,
            pVdpaEntry->size);

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    LwU32 performanceTableOffset = pTTableOffset;
#else
    LwU32 performanceTableOffset = pTTableOffset +
            gBiosTable.fw_descriptor->fw_Image_offset;
#endif

    LwU32 performanceTableAdr = loadTableSegment(
            performanceTableOffset, pdmem, pTTableSize);

    Perf6xHeader* pPTH = (Perf6xHeader*) performanceTableAdr;
    gBiosTable.pPTH = pPTH;

    LwU8 pTHeaderVersion = TABLE_VAL(pPTH->Version);
    LwU8 pTHeaderSize = TABLE_VAL(pPTH->HeaderSize);
    if ((pTHeaderVersion != PERF_VERSION_6X) || (pTHeaderSize != sizeof(Perf6xHeader)))
    {
        //FW_MBOX_WR32(10, pTHeaderVersion);
        //FW_MBOX_WR32(11, PERF_VERSION_6X);
        //FW_MBOX_WR32(12, pTHeaderSize);
        //FW_MBOX_WR32(13, sizeof(Perf6xHeader));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
    }
    LwU8 pTBaseEntryCount = TABLE_VAL(pPTH->BaseEntryCount);
    LwU8 pTBaseEntrySize = TABLE_VAL(pPTH->BaseEntrySize);
    LwU8 pTClockEntryCount = TABLE_VAL(pPTH->ClockEntryCount);
    LwU8 pTClockEntrySize = TABLE_VAL(pPTH->ClockEntrySize);
    LwU32 performanceTableSize = sizeof(Perf6xHeader) + pTBaseEntryCount * (
            pTBaseEntrySize +
            (pTClockEntrySize * pTClockEntryCount)
    );
    LwBool bError = (performanceTableSize > pTTableSize);
    if (bError)
    {
        //FW_MBOX_WR32_STALL(5, *pdmem);
        //FW_MBOX_WR32_STALL(6, TABLE_VAL(pPTH->Version));
        //FW_MBOX_WR32_STALL(7, pTBaseEntryCount);
        //FW_MBOX_WR32_STALL(8, pTBaseEntrySize);
        //FW_MBOX_WR32_STALL(9, pTClockEntryCount);
        //FW_MBOX_WR32_STALL(10, pTClockEntrySize);
        //FW_MBOX_WR32_STALL(11, FW_MEMORY_PERFORMANCE_TABLE_ID);
        //FW_MBOX_WR32_STALL(12, pTTableSize);
        //FW_MBOX_WR32_STALL(13, performanceTableSize);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_PERFORMANCE_TABLE_SIZE_ERROR)
    }
}

LW_STATUS
loadNominalFrequencies
(
        void
)
{
    // we are interested in the dramclk (entry 2) of the P0 table only
    gBiosTable.nominalFrequencyP0 = 0;
    gBiosTable.nominalFrequencyP8 = 0;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if (gBiosTable.pPTH->Flags0.PH6xF0PstateReq == PERF6XHEATER_PSTATE_DISABLED) {
        return FBFLCN_ERROR_CODE_PSTATE_DISABLED_ERROR;
    }
#endif

    LwU32 localPerfBaseAdr = (LwU32)gBiosTable.pPTH + gBiosTable.pPTH->HeaderSize;
    LwU8* p = (LwU8*)localPerfBaseAdr;
    LwU8 i = 0;
    for (i=0; i<gBiosTable.pPTH->BaseEntryCount; i++)
    {
        Perf6xBaseEntry* pPerfBaseEntry = (Perf6xBaseEntry*) p;
        LwU8 state = pPerfBaseEntry->State;
        if (state == PSTATE_MCLK_FREQ_ENTRY_STATE_P0)
        {
            // get the pointer to the 2nd Clock Entry (DRAMCLK)
            Perf6xClockEntry* pPerfClockEntry = (Perf6xClockEntry*)(p + gBiosTable.pPTH->BaseEntrySize + 2*gBiosTable.pPTH->ClockEntrySize);
            gBiosTable.nominalFrequencyP0 = pPerfClockEntry->Param0.PCEP0Nominal;
        }
        if (state == PSTATE_MCLK_FREQ_ENTRY_STATE_P8)
        {
            // get the pointer to the 2nd Clock Entry (DRAMCLK)
            Perf6xClockEntry* pPerfClockEntry = (Perf6xClockEntry*)(p + gBiosTable.pPTH->BaseEntrySize + 2*gBiosTable.pPTH->ClockEntrySize);
            gBiosTable.nominalFrequencyP8 = pPerfClockEntry->Param0.PCEP0Nominal;;
        }
        // increment to the next perf base entry
        p = p + gBiosTable.pPTH->BaseEntrySize + (gBiosTable.pPTH->ClockEntrySize * gBiosTable.pPTH->ClockEntryCount );
    }

    LwBool bError =  (gBiosTable.nominalFrequencyP0 == 0);
    if (bError)
    {
//        FW_MBOX_WR32(7, gBiosTable.pPTH->HeaderSize);
//        FW_MBOX_WR32(9, (LwU32)gBiosTable.pPTH);
//        FW_MBOX_WR32(10, i);
//        FW_MBOX_WR32(11, gBiosTable.pPTH->BaseEntryCount);
//        FW_MBOX_WR32(12, localPerfBaseAdr);
//        FW_MBOX_WR32(13, gBiosTable.pPTH->BaseEntrySize);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MISSING_NOMINAL_PSTATE_FREQUENCY)
    }
    return LW_OK;
}
#endif


/*
 * load Memory Information Table
 */

void
loadMemoryInformationTable
(
        LwU32 *pdmem
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(FW_MEMORY_INFORMATION_TABLE_ID);
#else
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
    pVdpaEntry = findDirtIdTableEntry(FW_MEMORY_INFORMATION_TABLE_ID);
#endif

    if (pVdpaEntry == NULL)
    {
        // the findDirtTable function will assert a halt on null already.
        return;
    }

    LwU32 mitTableOffset = pVdpaEntry->offset_start;
    LwU32 mitTableSize = REF_VAL(FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE,
            pVdpaEntry->size);


#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    LwU32 memoryInformationTableOffset = mitTableOffset;
#else
    LwU32 memoryInformationTableOffset = mitTableOffset +
            gBiosTable.fw_descriptor->fw_Image_offset;
#endif

    LwU32 memoryInformationTableAdr = loadTableSegment(
            memoryInformationTableOffset, pdmem, mitTableSize);

    MemInfoHeader* mIHp = (MemInfoHeader*) memoryInformationTableAdr;
    gBiosTable.mIHp = mIHp;

    LwU8 mitEntrySize = TABLE_VAL(mIHp->EntrySize);
    LwU8 mitEntryCount = TABLE_VAL(mIHp->EntryCount);
    LwU8 mitHeaderSize = TABLE_VAL(mIHp->HeaderSize);

#if (FBFALCONCFG_CHIP_ENABLED(GV10X))
    // gv100 does not have input validation on table data
#else
    LwU8 mitHeaderVersion = TABLE_VAL(mIHp->Version);
    // check table version and header size
    if ((mitHeaderVersion != MEMINFO_VERSION) || (mitHeaderSize != sizeof(MemInfoHeader)))
    {
        //FW_MBOX_WR32(10, mitHeaderSize);
        //FW_MBOX_WR32(11, MEMINFO_VERSION);
        //FW_MBOX_WR32(12, mitHeaderSize);
        //FW_MBOX_WR32(13, sizeof(MemInfoHeader));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
    }
#endif

    // check that we have loaded the full table as referenced in its header and that the loaded
    // amount of data is bigger than the sum of the contained structs.
    LwU32 tableSize = mitHeaderSize + (mitEntryCount * mitEntrySize);
    LwBool bError = (mitTableSize < tableSize);
    if (bError)
    {
        //FW_MBOX_WR32_STALL(5, mitEntrySize);
        //FW_MBOX_WR32_STALL(6, mitEntryCount);
        //FW_MBOX_WR32_STALL(7, mitHeaderSize);
        //FW_MBOX_WR32_STALL(8, *pdmem);
        //FW_MBOX_WR32_STALL(9, memoryInformationTableOffset);
        //FW_MBOX_WR32_STALL(10, TABLE_VAL(mIHp->Version));
        //FW_MBOX_WR32_STALL(11, FW_MEMORY_INFORMATION_TABLE_ID);
        //FW_MBOX_WR32_STALL(12, tableSize);
        //FW_MBOX_WR32_STALL(13, mitTableSize);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MIT_TABLE_SIZE_ERROR)
    }

    // check that lookup method is strap-based.  Nothing else is supported at the moment
    // check that the strap falls into the range of available entries
    MemInfoFlags flags;
    EXTRACT_TABLE(&mIHp->Flags, flags);
    if ((flags.MIFEntLUMeth != MEMINFO_FLAGS_ENT_LU_METH_STRAP) ||
            (gBiosTable.strap >= (mitEntryCount-1)))
    {
        //FW_MBOX_WR32(6, mitEntryCount);
        //FW_MBOX_WR32(8, gBiosTable.strap);
        //FW_MBOX_WR32(12, TABLE_VAL(mIHp->Flags));
        //FW_MBOX_WR32(13, flags.MIFEntLUMeth);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MIT_TABLE_ENTRY_LOOKUP_ERROR);
    }

    // select entry based of strap
    LwU32 memInfoTableAdr = memoryInformationTableAdr + mitHeaderSize + (gBiosTable.strap * mitEntrySize);

    MemInfoEntry* mIEp = (MemInfoEntry*) memInfoTableAdr;
    gBiosTable.mIEp = mIEp;
}


/*
 * load PLL Information Table
 */

void
loadPLLInformationTable
(
        LwU32 *pdmem
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(FW_MEMORY_PLL_INFORMATION_TABLE_ID);
#else
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
    pVdpaEntry = findDirtIdTableEntry(FW_MEMORY_PLL_INFORMATION_TABLE_ID);
#endif

    if (pVdpaEntry == NULL)
    {
        // the findDirtTable function will assert a halt on null already.
        return;
    }
    LwU32 mpitTableOffset = pVdpaEntry->offset_start;
    LwU32 mpitTableSize = REF_VAL(FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE,
            pVdpaEntry->size);

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    LwU32 pitOffset = mpitTableOffset;
#else
    LwU32 pitOffset = mpitTableOffset +
            gBiosTable.fw_descriptor->fw_Image_offset;
#endif

    LwU32 pitAdr = loadTableSegment( pitOffset, pdmem, mpitTableSize);

    PLLInfo5xHeader* pIHp = (PLLInfo5xHeader*) pitAdr;
    gBiosTable.pIHp = pIHp;

    LwU8 pitHeaderSize = TABLE_VAL(pIHp->HeaderSize);
    LwU8 pitEntrySize = TABLE_VAL(pIHp->EntrySize);
    LwU8 pitEntryCount = TABLE_VAL(pIHp->EntryCount);

#if (FBFALCONCFG_CHIP_ENABLED(GV10X))
    // gv100 did not have table data validation
#else
    LwU8 pitHeaderVersion = TABLE_VAL(pIHp->Version);
    // check table version and header size
    if ((pitHeaderVersion != PLL_INFO_5X_VERSION) || (pitHeaderSize != sizeof(PLLInfo5xHeader)))
    {
        //FW_MBOX_WR32(10, pitHeaderVersion);
        //FW_MBOX_WR32(11, PLL_INFO_5X_VERSION);
        //FW_MBOX_WR32(12, pitHeaderSize);
        //FW_MBOX_WR32(13, sizeof(PLLInfo5xHeader));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
    }
#endif

    LwU32 tableSize = pitHeaderSize + (pitEntrySize * pitEntryCount);

    // make sure we read the correct amount of data
    LwBool bError = (mpitTableSize < tableSize);
    if (bError)
    {
        //FW_MBOX_WR32_STALL(6, TABLE_VAL(pIHp->Version));
        //FW_MBOX_WR32_STALL(7, pitHeaderSize);
        //FW_MBOX_WR32_STALL(8, pitEntrySize);
        //FW_MBOX_WR32_STALL(9, pitEntryCount);
        //FW_MBOX_WR32_STALL(10, *pdmem);
        //FW_MBOX_WR32_STALL(11, FW_MEMORY_PLL_INFORMATION_TABLE_ID);
        //FW_MBOX_WR32_STALL(12, mpitTableSize);
        //FW_MBOX_WR32_STALL(13, tableSize);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_PLLINFOTABLE_TABLE_SIZE_ERROR);
    }
}

void
loadPLLInformationEntries
(
        void
)
{
    //
    // loop through the pll information table entries and find the HBM or GDDR PLL
    //
    gBiosTable.pIEp = 0x0;
    gBiosTable.pIEp_refmpll = 0x0;
    gBiosTable.pIEp_mpll = 0x0;

    LwU8 i = 0;
    LwU8 pitEntryCount = gBiosTable.pIHp->EntryCount;
    LwU32 pitEntriesAdr = (LwU32)gBiosTable.pIHp + gBiosTable.pIHp->HeaderSize;
    LwU8 pitEntrySize = gBiosTable.pIHp->EntrySize;

#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))

    PLLInfo5xEntry* pIEp = NULL;
    // indicators for entries found
    LwBool bFoundHBMEntry = LW_FALSE;

    for (i=0; i<pitEntryCount; i++)
    {
        PLLInfo5xEntry* pIEp_inx = (PLLInfo5xEntry*)(pitEntriesAdr + (i * pitEntrySize));

        if (TABLE_VAL(pIEp_inx->PLLID) == PLL_INFORMATION_TABLE_HBMPLL_ID)
        {
            pIEp = pIEp_inx;
            bFoundHBMEntry = LW_TRUE;
        }
    }
    // halt if the required plls can not be found
    if (bFoundHBMEntry == LW_FALSE) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HBM_PLL_INFO_NOT_FOUND);
    }
    gBiosTable.pIEp = pIEp;

#elif (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))

    PLLInfo5xEntry* pIEp_refmpll = NULL;
    PLLInfo5xEntry* pIEp_mpll = NULL;

    // indicators for entries found
    LwBool bFoundMpllEntry = LW_FALSE;
    LwBool bFoundRefMpllEntry = LW_FALSE;

    for (i=0; i<pitEntryCount; i++)
    {
        PLLInfo5xEntry* pIEp_inx = (PLLInfo5xEntry*) (pitEntriesAdr + (i * pitEntrySize));

        if (TABLE_VAL(pIEp_inx->PLLID) == PLL_INFORMATION_TABLE_REFMPLL_ID)
        {
            pIEp_refmpll = pIEp_inx;
            bFoundRefMpllEntry = LW_TRUE;
        }
        else if (TABLE_VAL(pIEp_inx->PLLID) == PLL_INFORMATION_TABLE_MPLL_ID)
        {
            pIEp_mpll = pIEp_inx;
            bFoundMpllEntry = LW_TRUE;
        }
    }
    // halt if the required plls can not be found
    if (bFoundMpllEntry == LW_FALSE) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_GDDR_MPLL_INFO_NOT_FOUND);
    }
    if (bFoundRefMpllEntry == LW_FALSE) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_GDDR_REFMPLL_INFO_NOT_FOUND);
    }
    gBiosTable.pIEp_refmpll = pIEp_refmpll;
    gBiosTable.pIEp_mpll = pIEp_mpll;

#else

    // binary needs to have either Support for GDDR or HBM enabled
    missing memory type support feature

#endif
}


/*
 * load Memory Clock Table
 */

void
loadMemoryClockTable
(
        LwU32 *pdmem
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(FW_MEMORY_CLOCK_TABLE_ID);
#else
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
    pVdpaEntry = findDirtIdTableEntry(FW_MEMORY_CLOCK_TABLE_ID);
#endif

    if (pVdpaEntry == NULL)
    {
        return;
    }

    LwU32 pmt_table_offset = pVdpaEntry->offset_start;
    LwU32 pmt_table_size = REF_VAL(FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE,
            pVdpaEntry->size);

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    LwU32 perfMemClkHeader_Offset = pmt_table_offset;
#else
    LwU32 perfMemClkHeader_Offset = pmt_table_offset +
            gBiosTable.fw_descriptor->fw_Image_offset;
#endif

    LwU32 perfMemClkHeader_Start = loadTableSegment(perfMemClkHeader_Offset,
            pdmem, pmt_table_size);

    PerfMemClk11Header* pMCHp = (PerfMemClk11Header*) perfMemClkHeader_Start;
    gBiosTable.pMCHp = pMCHp;

    LwU8 pMCH_Header_Size = TABLE_VAL(pMCHp->HeaderSize);
    LwU8 BaseEntrySize = TABLE_VAL(pMCHp->BaseEntrySize);
    LwU8 EntryCount = TABLE_VAL(pMCHp->EntryCount);
    LwU8 StrapEntrySize = TABLE_VAL(pMCHp->StrapEntrySize);
    LwU8 StrapEntryCount = TABLE_VAL(pMCHp->StrapEntryCount);

    LwU32 table_size = pMCH_Header_Size + (EntryCount * (BaseEntrySize +
            (StrapEntrySize * StrapEntryCount)));

#if (FBFALCONCFG_CHIP_ENABLED(GV10X))
    // table validation was not implmented on gv100/105
#else
    //LwU8 pMCHHeaderSize = TABLE_VAL(pMCHp->HeaderSize);
    LwU8 pMCHHeaderVersion = TABLE_VAL(pMCHp->Version);

    // check table version and header size
    // FIXME: header size is not stable due to bug 2051282 for the added vref/vdddq pin fields,
    //        enable this once bios has submitted (bug 2060971)
    if ((pMCHHeaderVersion != PERF_MCLK_11_VERSION)
            /* || (pMCHHeaderSize != sizeof(PerfMemClk11Header)) */
    )
    {
        //FW_MBOX_WR32(10, pMCHHeaderVersion);
        //FW_MBOX_WR32(11, PERF_MCLK_11_VERSION);
        //FW_MBOX_WR32(12, pMCHHeaderSize);
        //FW_MBOX_WR32(13, sizeof(PerfMemClk11Header));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
    }
#endif

    // check that we have loaded the full table
    LwBool bError = (pmt_table_size < table_size);
    if (bError)
    {
        //FW_MBOX_WR32_STALL(10, pMCHp->Version);
        //FW_MBOX_WR32_STALL(11, FW_MEMORY_CLOCK_TABLE_ID);
        //FW_MBOX_WR32_STALL(12, pmt_table_size);
        //FW_MBOX_WR32_STALL(13, table_size);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEMORYCLOCKTABLE_TABLE_SIZE_ERROR)
    }

    LwU32 PerfMemClk11BaseEntry_Start = perfMemClkHeader_Start +
            pMCH_Header_Size;

    PerfMemClk11BaseEntry* pMCBEp =
            (PerfMemClk11BaseEntry*) PerfMemClk11BaseEntry_Start;
    gBiosTable.pMCBEp = pMCBEp;
    gBiosTable.pMCBEentryCount = EntryCount;
    gBiosTable.pMCBEentrySize = BaseEntrySize;
    gBiosTable.pMCBEstrapCount = StrapEntryCount;
    gBiosTable.pMCBEstrapSize = StrapEntrySize;
}



/*
 * sanity check for table values
 * this code is run in emulation runs only to get coverage on accurate table loading
 * when running with the dummy mclk switch (which does not use table values)
 */
GCC_ATTRIB_SECTION("init", "sanityCheckHalt")
void sanityCheckHalt
(
        LwU32 index,
        LwU32 config,
        LwU32 refValue,
        LwU32 tableValue
)
{
    FW_MBOX_WR32(11, config);
    FW_MBOX_WR32(12, refValue);
    FW_MBOX_WR32(13, tableValue);
    FBFLCN_HALT( (index << 16) | FBFLCN_ERROR_CODE_TABLE_SANITY_CHECK_FAILURE);
}

void sanityCheckMemTweakTable
(
        void
)
{
    static PerfMemTweak2xBaseEntry mtBuffer;

    PerfMemTweak2xBaseEntry* pMTBEp = gBiosTable.pMTBEp;  // address to the first MemTweakBaseEntry

    LwU32 i;
    for (i=0; i<6; i++)
    {
        PerfMemTweak2xBaseEntry* mp = (PerfMemTweak2xBaseEntry*) ((LwU8*)pMTBEp +
                i*(gBiosTable.pMTBEentrySize +
                        (gBiosTable.pMTBEextEntryCount * gBiosTable.pMTBEextEntrySize)));
        EXTRACT_TABLE(mp,mtBuffer);
        switch(i)
        {
        case 0:
            if (mtBuffer.Config0 != 0x1A68D24E) {
                sanityCheckHalt(0,0,0x1A68D24E,mtBuffer.Config0);
            }
            break;
        case 1:
            if (mtBuffer.Config0 != 0x1962C34A) {
                sanityCheckHalt(1,0,0x1962C34A,mtBuffer.Config0);
            }
            break;
        case 2:
            if (mtBuffer.Config0 != 0x175AB444) {
                sanityCheckHalt(2,0,0x175AB444,mtBuffer.Config0);
            }
            break;
        case 3:
            if (mtBuffer.Config0 != 0x134C9639) {
                sanityCheckHalt(3,0,0x134C9639,mtBuffer.Config0);
            }
            break;
        case 4:
            if (mtBuffer.Config0 != 0x0F3C782D) {
                sanityCheckHalt(4,0,0x0F3C782D,mtBuffer.Config0);
            }
            break;
        case 5:
            if (mtBuffer.Config0 != 0x040E190B) {
                sanityCheckHalt(5,0,0x040E190B,mtBuffer.Config0);
            }
            break;

        }
    }
}

/*
 * load Mem Tweak Table
 */

void loadMemTweakTable
(
        LwU32 *pdmem
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(FW_MEMORY_TWEAK_TABLE_ID);
#else
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
    pVdpaEntry = findDirtIdTableEntry(FW_MEMORY_TWEAK_TABLE_ID);
#endif
    if (pVdpaEntry == NULL)
    {
        return;
    }

    LwU32 mtwtOffset = pVdpaEntry->offset_start;
    LwU32 mtwtSize = REF_VAL(
            FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE,pVdpaEntry->size);

    //-------------------------------------------------------------------------
    // load perfMemTweakHeader
    //
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    LwU32 perfMemTweakHeader_Offset = mtwtOffset;
#else
    LwU32 perfMemTweakHeader_Offset = mtwtOffset +
            gBiosTable.fw_descriptor->fw_Image_offset;
#endif
    LwU32 perfMemTweak2xHeader_Start = loadTableSegment(
            perfMemTweakHeader_Offset, pdmem, mtwtSize);
    PerfMemTweak2xHeader* pMTHp =
            (PerfMemTweak2xHeader*) perfMemTweak2xHeader_Start;
    gBiosTable.pMTHp = pMTHp;

    //-------------------------------------------------------------------------
    // load perMemTweakEntries
    //

    LwU32 entryCount = TABLE_VAL(pMTHp->EntryCount);
    LwU32 baseEntrySize = TABLE_VAL(pMTHp->BaseEntrySize);
    LwU32 extEntryCount = TABLE_VAL(pMTHp->ExtEntryCount);
    LwU32 extEntrySize = TABLE_VAL(pMTHp->ExtEntrySize);
    LwU32 headerSize = TABLE_VAL(pMTHp->HeaderSize);

#if (FBFALCONCFG_CHIP_ENABLED(GV10X))
    // table data validation was not active on gv100/gv105
#else
    // check table version and header size
    LwU16 headerVersion = TABLE_VAL(pMTHp->Version);
    if ((headerVersion != PERF_MTWEAK_2X_VERSION) || (headerSize != sizeof(PerfMemTweak2xHeader)))
    {
        //FW_MBOX_WR32(10, headerVersion);
        //FW_MBOX_WR32(11, PERF_MTWEAK_2X_VERSION);
        //FW_MBOX_WR32(12, headerSize);
        //FW_MBOX_WR32(13, sizeof(PerfMemTweak2xHeader));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
    }
#endif

    LwU32 tableSize = headerSize + ( entryCount * (baseEntrySize +
            (extEntryCount * extEntrySize)));
    LwBool bError = (mtwtSize < tableSize);
    if (bError)
    {
        //FW_MBOX_WR32_STALL(10, TABLE_VAL(pMTHp->Version));
        //FW_MBOX_WR32_STALL(11, FW_MEMORY_TWEAK_TABLE_ID);
        //FW_MBOX_WR32_STALL(12, mtwtSize);
        //FW_MBOX_WR32_STALL(13, tableSize);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEMTWEAKTABLE_TABLE_SIZE_ERROR);
    }

    LwU32 perfMemTweakBaseEntry_Start = perfMemTweak2xHeader_Start +
            headerSize;

    PerfMemTweak2xBaseEntry* pMTBEp =
            (PerfMemTweak2xBaseEntry*) perfMemTweakBaseEntry_Start;

    gBiosTable.pMTBEp = pMTBEp;  // address to the first MemTweakBaseEntry
    gBiosTable.pMTBEentryCount = entryCount;
    gBiosTable.pMTBEentrySize = baseEntrySize;  //perfMemTweakBaseEntry_Size;
    gBiosTable.pMTBEextEntryCount = extEntryCount;
    gBiosTable.pMTBEextEntrySize = extEntrySize;
}

#if ((!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)) || ((FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100)) && FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_TRAINING_TABLE)))

void
parseTrainingPatramTable
(
        void
)
{
    MemTrainBaseEntry*  pMTBEpRdDly = NULL;
    MemTrainBaseEntry*  pMTBEpWrDly = NULL;
    MemTrainBaseEntry*  pMTBEpDatVref = NULL;
    MemTrainBaseEntry*  pMTBEpDqsVref = NULL;
    MemTrainBaseEntry*  pMTBEpAddrDly = NULL;
    MemTrainBaseEntry*  pMTBEpQuseDly = NULL;
    MemTrainBaseEntry*  pMTBEpRdDbi = NULL;
    MemTrainBaseEntry*  pMTBEpWrDbi = NULL;
    MemTrainBaseEntry*  pMTBEpRdEdc = NULL;
    MemTrainBaseEntry*  pMTBEpWrEdc = NULL;
    MemTrainStrapEntry* pMTSEpRdDly = NULL;
    MemTrainStrapEntry* pMTSEpWrDly = NULL;
    MemTrainStrapEntry* pMTSEpDatVref = NULL;
    MemTrainStrapEntry* pMTSEDqsVref = NULL;
    MemTrainStrapEntry* pMTSEpAddrDly = NULL;
    MemTrainStrapEntry* pMTSEpQuseDly = NULL;
    MemTrainStrapEntry* pMTSEpRdDbi = NULL;
    MemTrainStrapEntry* pMTSEpWrDbi = NULL;
    MemTrainStrapEntry* pMTSEpRdEdc = NULL;
    MemTrainStrapEntry* pMTSEpWrEdc = NULL;

    MemTrainHeader* pMemTHp =  gBiosTable.pMemTHp;

    LwU8 MemTrainingTableHeaderSize = pMemTHp->HeaderSize;
    LwU8 MemTrainingTableBaseEntrySize = pMemTHp->BaseEntrySize;
    LwU8 MemTrainingTableStrapEntrySize = pMemTHp->StrapEntrySize;
    LwU8 MemTrainingTableStrapEntryCount = pMemTHp->StrapEntryCount;
    LwU8 MemTrainingTableEntryCount = pMemTHp->EntryCount;

    LwU16 memTrainingTableVersion = pMemTHp->Version;

    // check table version and header size
    if ((memTrainingTableVersion != MEMTRAIN_VERSION) || (MemTrainingTableHeaderSize != sizeof(MemTrainHeader)))
    {
        //FW_MBOX_WR32(10, memTrainingTableVersion);
        //FW_MBOX_WR32(11, MEMTRAIN_VERSION);
        //FW_MBOX_WR32(12, MemTrainingTableHeaderSize);
        //FW_MBOX_WR32(13, sizeof(MemTrainHeader));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
    }

    LwU8 TotalStrapSizePerBaseEntry = MemTrainingTableStrapEntryCount * MemTrainingTableStrapEntrySize;

    LwU8 i = 0;
    for (i = 0 ; i < MemTrainingTableEntryCount; i++ )
    {
        MemTrainBaseEntry* pMTBEp = (MemTrainBaseEntry*) ((LwU8*)pMemTHp + MemTrainingTableHeaderSize + (i * MemTrainingTableBaseEntrySize) + (i * TotalStrapSizePerBaseEntry));

        LwU8 Val =*(LwU8*)pMTBEp;
        Val = (Val & 0x0F);

        switch(Val)
        {
        case MEMTRAIN_ENTRY_PARAM_RDDLY:
        {
            pMTBEpRdDly = pMTBEp;
            pMTSEpRdDly = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
            break;
        }
        case MEMTRAIN_ENTRY_PARAM_WRDLY:
        {
            pMTBEpWrDly  = pMTBEp;
            pMTSEpWrDly = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
            break;
        }
        //          case MEMTRAIN_ENTRY_PARAM_DATVREF:
        //    {
        //             pMTBEpDatVref  = pMTBEp;
        //       pMTSEpDatVref = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
        //       FW_MBOX_WR32_STALL(13, 0x0A0B030B);
        //       break;
        //    }
        //          case MEMTRAIN_ENTRY_PARAM_DQSVREF:
        //    {
        //             pMTBEpDqsVref = pMTBEp;
        //       pMTSEDqsVref = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
        //       FW_MBOX_WR32_STALL(13, 0x0A0B040B);
        //       break;
        //    }
        case MEMTRAIN_ENTRY_PARAM_ADDRDLY:
        {
            pMTBEpAddrDly  = pMTBEp;
            pMTSEpAddrDly = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
            break;
        }
        //          case MEMTRAIN_ENTRY_PARAM_QUSEDLY:
        //    {
        //             pMTBEpQuseDly  = pMTBEp;
        //       pMTSEpQuseDly = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
        //       FW_MBOX_WR32_STALL(13, 0x0A0B060B);
        //       break;
        //    }
        case MEMTRAIN_ENTRY_PARAM_RDDBI:
        {
            pMTBEpRdDbi  = pMTBEp;
            pMTSEpRdDbi = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
            break;
        }
        case MEMTRAIN_ENTRY_PARAM_WRDBI:
        {
            pMTBEpWrDbi  = pMTBEp;
            pMTSEpWrDbi = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
            break;
        }
        case MEMTRAIN_ENTRY_PARAM_RDEDC:
        {
            pMTBEpRdEdc  = pMTBEp;
            pMTSEpRdEdc = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
            break;
        }
        case MEMTRAIN_ENTRY_PARAM_WREDC:
        {
            pMTBEpWrEdc  = pMTBEp;
            pMTSEpWrEdc = (MemTrainStrapEntry*) ((LwU8*)pMTBEp + MemTrainingTableBaseEntrySize + (gBiosTable.strap * MemTrainingTableStrapEntrySize)) ;
            break;
        }
        }
    }

    //    if ((pMTBEpRdDly == NULL) || (pMTBEpWrDly == NULL) || (pMTBEpDatVref == NULL) ||
    //  (pMTBEpDqsVref == NULL) || (pMTBEpAddrDly == NULL) || (pMTBEpQuseDly == NULL) ||
    //  (pMTBEpRdDbi == NULL) || (pMTBEpWrDbi == NULL) || (pMTBEpRdEdc == NULL) ||
    //  (pMTBEpWrEdc == NULL))
    if ((pMTBEpRdDly == NULL) || (pMTBEpWrDly == NULL) ||
            (pMTBEpAddrDly == NULL) || (pMTBEpRdDbi == NULL) || (pMTBEpWrDbi == NULL) || (pMTBEpRdEdc == NULL) ||
            (pMTBEpWrEdc == NULL))
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_TABLE_RD_WR_EDC_DBI_ID_MISSING);
    }

    if ((pMTSEpRdDly == NULL) || (pMTSEpWrDly == NULL) ||
            (pMTSEpAddrDly == NULL) ||
            (pMTSEpRdDbi == NULL) || (pMTSEpWrDbi == NULL) || (pMTSEpRdEdc == NULL) ||
            (pMTSEpWrEdc == NULL))
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_TABLE_RD_WR_EDC_DBI_STRAP_MISSING);
    }

    gBiosTable.instMTPEp.pMTBEpRdDly    =     pMTBEpRdDly;
    gBiosTable.instMTPEp.pMTBEpWrDly    =     pMTBEpWrDly;
    gBiosTable.instMTPEp.pMTBEpDatVref  =     pMTBEpDatVref;
    gBiosTable.instMTPEp.pMTBEpDqsVref  =     pMTBEpDqsVref;
    gBiosTable.instMTPEp.pMTBEpAddrDly  =     pMTBEpAddrDly;
    gBiosTable.instMTPEp.pMTBEpQuseDly  =     pMTBEpQuseDly;
    gBiosTable.instMTPEp.pMTBEpRdDbi    =     pMTBEpRdDbi;
    gBiosTable.instMTPEp.pMTBEpWrDbi    =     pMTBEpWrDbi;
    gBiosTable.instMTPEp.pMTBEpRdEdc    =     pMTBEpRdEdc;
    gBiosTable.instMTPEp.pMTBEpWrEdc    =     pMTBEpWrEdc;
    gBiosTable.instMTPEp.pMTSEpRdDly    =     pMTSEpRdDly;
    gBiosTable.instMTPEp.pMTSEpWrDly    =     pMTSEpWrDly;
    gBiosTable.instMTPEp.pMTSEpDatVref  =     pMTSEpDatVref;
    gBiosTable.instMTPEp.pMTSEDqsVref   =     pMTSEDqsVref;
    gBiosTable.instMTPEp.pMTSEpAddrDly  =     pMTSEpAddrDly;
    gBiosTable.instMTPEp.pMTSEpQuseDly  =     pMTSEpQuseDly;
    gBiosTable.instMTPEp.pMTSEpRdDbi    =     pMTSEpRdDbi;
    gBiosTable.instMTPEp.pMTSEpWrDbi    =     pMTSEpWrDbi;
    gBiosTable.instMTPEp.pMTSEpRdEdc    =     pMTSEpRdEdc;
    gBiosTable.instMTPEp.pMTSEpWrEdc    =     pMTSEpWrEdc;


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    MemTrainPatternEntry11* pMTPEp_save[10];
    MemTrainPattern11Header* pMemPatHp = gBiosTable.pMemPatHp;
#else
    MemTrainPatternEntry10* pMTPEp_save[10];
    MemTrainPattern10Header* pMemPatHp = gBiosTable.pMemPatHp;
#endif

    LwU8 MemTrainingPatTableHeaderSize = TABLE_VAL(pMemPatHp->HeaderSize);
    LwU8 MemTrainingPatTableBaseEntrySize = TABLE_VAL(pMemPatHp->BaseEntrySize);
    LwU8 MemTrainingPatTableEntryCount = TABLE_VAL(pMemPatHp->EntryCount);

    LwU8 memTrainingPatTableVersion = TABLE_VAL(pMemPatHp->Version);

    // check table version and header size
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if ((memTrainingPatTableVersion != MEMTRAIN_PATTERN_11_VERSION) || (MemTrainingPatTableHeaderSize != sizeof(MemTrainPattern11Header)))
#else
        if ((memTrainingPatTableVersion != MEMTRAIN_PATTERN_10_VERSION) || (MemTrainingPatTableHeaderSize != sizeof(MemTrainPattern10Header)))
#endif
        {
            //FW_MBOX_WR32(9, FW_MEMORY_TRAINING_PATTERN_TABLE_ID);
            //FW_MBOX_WR32(10, memTrainingPatTableVersion);
            //FW_MBOX_WR32(12, MemTrainingPatTableHeaderSize);
            //FW_MBOX_WR32(13, sizeof(MemTrainPattern10Header));
            FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
        }

    //LwU8 PrevTrainingPatramEndLoc = 0;
    LwU32 PatramTrainingTableEntry = (LwU32)pMemPatHp + MemTrainingPatTableHeaderSize;
    i =0;

    // compoute pointer to 1st enty (p)  Table_Start + header size

    for (i=0 ; i < MemTrainingPatTableEntryCount; i++)
    {
        // take value from entry p
        // compute pointer to the buffer
        //

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
        MemTrainPatternEntry10* pMTPEp = (MemTrainPatternEntry10*) PatramTrainingTableEntry;
#else
        MemTrainPatternEntry11* pMTPEp = (MemTrainPatternEntry11*) PatramTrainingTableEntry;
#endif
        //LwU8 PatternCount = TABLE_VAL(pMTPEp->BaseEntry.PatternCount);
        pMTPEp_save[i] = pMTPEp;


        //PrevTrainingPatramEndLoc = MemTrainingPatTableBaseEntrySize + PatternCount ;

        PatramTrainingTableEntry = PatramTrainingTableEntry + MemTrainingPatTableBaseEntrySize + FBFLCN_MAX_PATRAM_BUFFER_SIZE;

    }

    i = 0;
    for (i=0; i < MemTrainingPatTableEntryCount; i++)
    {
        if (pMTPEp_save[i] == NULL)
        {
            FW_MBOX_WR32_STALL(14, i);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_TABLE_ENTRY_MISSING);
        }
        else
        {
            gBiosTable.instMTPEp.pMTPEp_save[i] = pMTPEp_save[i];
        }
    }


}


void
loadTrainingPatramTable
(
        LwU32 *pdmem
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(FW_MEMORY_TRAINING_TABLE_ID);
#else
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
    pVdpaEntry = findDirtIdTableEntry(FW_MEMORY_TRAINING_TABLE_ID);
#endif

    if ( pVdpaEntry == NULL)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_TABLE_VDPA_ENTRY_MISSING);
    }

    LwU32 vdbpaMemTrainingTableOffset = pVdpaEntry->offset_start;
    LwU32 vdpaMemTrainingTableSize = REF_VAL(FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE, pVdpaEntry->size);

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    LwU32 MemTrainingTable_Offset = vdbpaMemTrainingTableOffset;
#else
    LwU32 MemTrainingTable_Offset = vdbpaMemTrainingTableOffset + gBiosTable.fw_descriptor->fw_Image_offset;
#endif

    LwU32 MemTrainingTable_Start = loadTableSegment(MemTrainingTable_Offset, pdmem, vdpaMemTrainingTableSize);
    gBiosTable.pMemTHp = (MemTrainHeader*) MemTrainingTable_Start;

    LwU8 MemTrainingTableHeaderSize = gBiosTable.pMemTHp->HeaderSize;
    LwU16 memTrainingTableVersion = gBiosTable.pMemTHp->Version;

    // check table version and header size
    if ((memTrainingTableVersion != MEMTRAIN_VERSION) || (MemTrainingTableHeaderSize != sizeof(MemTrainHeader)))
    {
        //FW_MBOX_WR32(9, FW_MEMORY_TRAINING_TABLE_ID);
        //FW_MBOX_WR32(10, memTrainingTableVersion);
        //FW_MBOX_WR32(11, MEMTRAIN_VERSION);
        //FW_MBOX_WR32(12, MemTrainingTableHeaderSize);
        //FW_MBOX_WR32(13, sizeof(MemTrainHeader));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
    }


#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    pMiniDirtTableEntry pVdpaEntryMemPatram;
    pVdpaEntryMemPatram = findMiniDirtIdEntry(FW_MEMORY_TRAINING_PATTERN_TABLE_ID);
#else
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntryMemPatram;
    pVdpaEntryMemPatram = findDirtIdTableEntry(FW_MEMORY_TRAINING_PATTERN_TABLE_ID);
#endif

    if ( pVdpaEntryMemPatram == NULL)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_TABLE_VDPA_ENTRY_MISSING);
    }

    LwU32 vdbpaMemTrainingPatramTableOffset = pVdpaEntryMemPatram->offset_start;

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    LwU32 MemTrainingPatramTable_Offset = vdbpaMemTrainingPatramTableOffset;
#else
    LwU32 MemTrainingPatramTable_Offset = vdbpaMemTrainingPatramTableOffset + gBiosTable.fw_descriptor->fw_Image_offset;
#endif

    //FIXME
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU32 MemTrainingPatramTable_Start = loadTableSegment(MemTrainingPatramTable_Offset, pdmem, 5 + (FBFLCN_MAX_PATRAM_BUFFER_SIZE * 8));
#else
    LwU32 MemTrainingPatramTable_Start = loadTableSegment(MemTrainingPatramTable_Offset, pdmem, sizeof(MemTrainPattern11Header) + (FBFLCN_MAX_PATRAM_BUFFER_SIZE * 8));
#endif

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    gBiosTable.pMemPatHp = (MemTrainPattern10Header*)  MemTrainingPatramTable_Start;
#else
    gBiosTable.pMemPatHp = (MemTrainPattern11Header*)  MemTrainingPatramTable_Start;
#endif
    LwU8 MemTrainingPatTableHeaderSize = TABLE_VAL(gBiosTable.pMemPatHp->HeaderSize);
    LwU8 memTrainingPatTableVersion = TABLE_VAL(gBiosTable.pMemPatHp->Version);

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    // check table version and header size
    if ((memTrainingPatTableVersion != MEMTRAIN_PATTERN_10_VERSION) || (MemTrainingPatTableHeaderSize != sizeof(MemTrainPattern10Header)))
#else
        if ((memTrainingPatTableVersion != MEMTRAIN_PATTERN_11_VERSION) || (MemTrainingPatTableHeaderSize != sizeof(MemTrainPattern11Header)))
#endif
        {
            //FW_MBOX_WR32(9, FW_MEMORY_TRAINING_PATTERN_TABLE_ID);
            //FW_MBOX_WR32(10, memTrainingPatTableVersion);
            //FW_MBOX_WR32(12, MemTrainingPatTableHeaderSize);
            //FW_MBOX_WR32(13, sizeof(MemTrainPattern10Header));
            FBFLCN_HALT(FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR);
        }
}
#endif


void loadVBIOSInternalUseOnly
(
        LwU32 *pdmem
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    pMiniDirtTableEntry pVdpaEntry;
    pVdpaEntry = findMiniDirtIdEntry(FW_BIT_INTERNAL_USE_ONLY);
#else
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
    pVdpaEntry = findDirtIdTableEntry(FW_BIT_INTERNAL_USE_ONLY);
#endif
    if (pVdpaEntry == NULL)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_INTERNAL_USE_ONLY_ERROR);
    }

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    LwU32 bitOffset = pVdpaEntry->offset_start;
#else
    LwU32 bitOffset = pVdpaEntry->offset_start + gBiosTable.fw_descriptor->fw_Image_offset;
#endif

    LwU32 bitTableSize = REF_VAL(FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE,
            pVdpaEntry->size);
    // only want to read the relevant header
    bitTableSize = ( bitTableSize > sizeof(BITInternalUseOnlyV2) ) ? sizeof(BITInternalUseOnlyV2) : bitTableSize;

    LwU32 bitTableStart = loadTableSegment(
            bitOffset, pdmem, bitTableSize);

    BITInternalUseOnlyV2* pIUO = (BITInternalUseOnlyV2*) bitTableStart;

    gBiosTable.pIUO = pIUO;
}

/*
 * Strap to Index lookup (legacy lookup)
 * this uses the strap to index mapping in the Memory Information Table Entries
 *
 * https://rmopengrok.lwpu.com/source/xref/r384_00/drivers/resman/kernel/
 *          devinit/lw/devinit_memory.c#322
 *
 */

void
decodeStrap
(
        void
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(DECODE_HBM_MEMORY_DEVICE_ID))
    // strap can not be read on ATE system. This will have to be replaced
    // with code reading out the memory info from IEEE1500 and reverse lookup
    // the strap through the meminfo entries.
    LwU8 status = memoryDecodeI1500Strap_HAL();

    // if I1500 detection fails we fall back to the old readout through the
    // secure scratch below.
    // For the ATE binary we have to abort as the secure scratch is not available
    // in that case we fall back to strap 0
    // This should be reviewed as it seems to make more sense to halt but
    if (status == LW_OK)
    {
        return;
    }
    if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_INCLUDED_IN_BINARY))
    {
        // Note:  If this fails in the ATE tests in the //hw stand_sim environment, the reason is most likely
        // a wrong setup in the ieee1500 handler at the beginning of the test. That setup needs to match the
        // vendor information of the sim strap (strap 0) in the memory information table of the bios.
        // the setup is manual and done in vmodels/fbpa_uvm_tb/tests/fbpa_rand_test.sv through
        // denali_backdoor_write_device_id
        // This could be done automatically but as of now uvm does not have access to the bios and regcalc does
        // not have access to the denali_backdoor_write
        FW_MBOX_WR32_STALL(13, status);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_I1500_STRAP_READ_FAILED);
    }
#endif


    //
    // VBIOS provides the index in the LW_THERM_SELWRE_WR_SCRATCH(3)
    //   as long as we're using this after devinit this is the quickest
    //   solution to get to the index

    // check if we have the correct security level set on the
    // LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03 registers

    // Starting with Turing the LW_THERM_SELWRE_WR_SCRATCH has an arrayed PLM,
    // see bug 2038205

#ifdef LW_THERM_SELWRE_WRITE_SCRATCH_PRIV_LEVEL_MASK__SIZE_1
    LwU32 regPlm = REG_RD32(BAR0, LW_THERM_SELWRE_WRITE_SCRATCH_PRIV_LEVEL_MASK(3));
    if (FLD_TEST_DRF(_THERM, _SELWRE_WRITE_SCRATCH_PRIV_LEVEL_MASK,
            _READ_PROTECTION_LEVEL0,  _ENABLE, regPlm))
    {
        // we have access, continue
    }
    else
    {
        FW_MBOX_WR32_STALL(12,
                LW_THERM_SELWRE_WRITE_SCRATCH_PRIV_LEVEL_MASK(3));
        FW_MBOX_WR32_STALL(13, regPlm);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_STRAP_ACCESS_FAILED);
    }
#else
    // only used for gv100
    LwU32 regPlm = REG_RD32(BAR0, LW_THERM_SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK);
    if (FLD_TEST_DRF(_THERM, _SELWRE_WR_SCRATCH_PRIV_LEVEL_MASK,
            _READ_PROTECTION_LEVEL0,  _ENABLE, regPlm))
    {
        // we have access, continue
    }
    else
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_STRAP_ACCESS_FAILED);
    }
#endif

    LwU32 thermScratch = REG_RD32(BAR0, LW_THERM_SELWRE_WR_SCRATCH(3));
    LwU8 thermIndex = REF_VAL(6:2,thermScratch);

    gBiosTable.strap = thermIndex;

}

/*
 * load vbios tables
 * read and parse bios table to copy all the relevant table entries into dmem
 */
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_INCLUDED_IN_BINARY))

LW_STATUS
loadTable
(
        void
)
{
    LW_STATUS status = LW_OK;

    // conserve gVbiosTable in memory space by having at least one direct access
    LwU32 dmemPointer = (LwU32)&gVbiosTable[0];
    FW_MBOX_WR32_STALL(14, dmemPointer);

    // get strap information
    // 
    decodeStrap();

    // revisit all table that need post processing, strap selection etc.

    //
    // 1. load GPIO table
    //
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_GPIO_TABLE))
    gBiosTable.gGPIO_PIN_FBVDDQ = fbfalconFindGpioPin_HAL(&Fbflcn,LW_GPIO_FUNC_FBVDDQ_SELECT);
    gBiosTable.gGPIO_PIN_VREF = fbfalconFindGpioPin_HAL(&Fbflcn,LW_GPIO_FUNC_FBVREF_SELECT);
#endif

    //
    // 2. load Performance table
    //
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_PERFORMANCE_TABLE))
    status = loadNominalFrequencies();
#endif

    //
    // 3. load Memory Information table
    //


    //
    // 4. load PLL Information table
    //
    loadPLLInformationEntries();

    //
    // 5. load Memory Clock table
    //

    //
    // 6. load Mem Tweak table
    //

    //
    // 7. load Boot Training table
    //
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_TRAINING_TABLE))
    EXTRACT_TABLE(gBiosTable.pBootTrainingTable,gTT.bootTrainingTable);

    //
    // 8. load Training Patram table
    //
  #if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
    parseTrainingPatramTable();
    LoadTrainingPatram();
  #else     // hbm
    LwBool skipBoot = gTT.bootTrainingTable.MemBootTrainTblFlags.flag_skip_boot_training;
    if (!skipBoot)
    {
        parseTrainingPatramTable();
        LoadDataTrainingPatram(0);
        LoadDataTrainingPatram(1);	
    }
  #endif
#endif
    return status;

}

#else

LW_STATUS
loadTable
(
        void
)
{
    LW_STATUS status = LW_OK;

    // preset default values that are parsed from bios at boot
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_GPIO_TABLE))
    gBiosTable.gGPIO_PIN_FBVDDQ = LW_GPIO_NUM_UNDEFINED;
    gBiosTable.gGPIO_PIN_VREF = LW_GPIO_NUM_UNDEFINED;
#endif

    //
    // update scratch code to FRTS_PROG_START
    //
    LwU32 scratchCode = REF_NUM(LW_UCODE_POST_CODE_FBFALCON_FRTS_PROG_CODE,
            LW_UCODE_FBFALCON_FRTS_PROG_START);

    FW_MBOX_WR32_STALL(15, scratchCode);

    // dmem aligned pointer
    LwU32 dmemPointer = (LwU32)&gVbiosTable[0];

    decodeStrap();

    loadVDPATable(&dmemPointer);

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))
    // extract local mini dirt it table from the vpda table for a subset of entries

    // clear the mini dirt table to all zero
    memsetLwU8(&gMiniDirtTable[0],0,sizeof(gMiniDirtTable));

    LwU32 i;
    LwU32 miniDirtIndex = 0;
    for(i=0; i<MINI_DIRT_TABLE_ENTRIES; i++)
    {
        FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR pVdpaEntry;
        pVdpaEntry = findDirtIdTableEntry(miniDirtTableIdList[i]);
        if (pVdpaEntry != NULL) {
            gMiniDirtTable[miniDirtIndex].dirtId = miniDirtTableIdList[i];
            gMiniDirtTable[miniDirtIndex].offset_start = pVdpaEntry->offset_start + gBiosTable.fw_descriptor->fw_Image_offset;
            gMiniDirtTable[miniDirtIndex].size = pVdpaEntry->size;
            miniDirtIndex++;
            // gMiniDirtTable has the same number of entries as the ID list, overflow is not possible

            // Future Improvements:
            // checking validity of table could reduce separate error checking and halt message code when each table is parsed
            // and reduce code size by probably about 200B
        }
    }

    // this pointer become invalid when vdpa table is overloaded
    gBiosTable.fw_descriptor = NULL;
    gBiosTable.vdpaTable = NULL;

    // reset the vbios buffer, removing the vdpa lookup table
    dmemPointer = (LwU32)&gVbiosTable[0];
#endif

    //
    // 1. load GPIO table
    //
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_GPIO_TABLE))
    // parse gpio table for power pins
    fbfalconLoadGpioTable_HAL(&Fbflcn,dmemPointer);
    gBiosTable.gGPIO_PIN_FBVDDQ = fbfalconFindGpioPin_HAL(&Fbflcn,LW_GPIO_FUNC_FBVDDQ_SELECT);
    gBiosTable.gGPIO_PIN_VREF = fbfalconFindGpioPin_HAL(&Fbflcn,LW_GPIO_FUNC_FBVREF_SELECT);
    // reset the vbios buffer, removing the gpio table as its no longer needed
    dmemPointer = (LwU32)&gVbiosTable[0];
#endif

    //
    // 2. load Performance table
    //
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_PERFORMANCE_TABLE))
    loadPerformanceTable(&dmemPointer);
    status = loadNominalFrequencies();
#endif

    //
    // 3. load Memory Information table
    //
    // Memory Information table was only used for strap to index translation
    // this is no longer required so for now there's no need for this tables
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
    loadMemoryInformationTable(&dmemPointer);
#endif

    //
    // 4. load PLL Information table
    //
    loadPLLInformationTable(&dmemPointer);
    loadPLLInformationEntries();

    //
    // 5. load Memory Clock table
    //
    loadMemoryClockTable(&dmemPointer);

    //
    // 6. load Mem Tweak table
    //
    loadMemTweakTable(&dmemPointer);

    //
    // 7. load Boot Training table
    //
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_TRAINING_TABLE))
    fbfalconLoadBootTrainingTable_HAL (&Fbflcn,&dmemPointer);
    EXTRACT_TABLE(gBiosTable.pBootTrainingTable,gTT.bootTrainingTable);
#endif

    //
    // 8. load Training Patram table
    //
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_TRAINING_TABLE))
  #if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
    // for GDDR we load the training tables in all cases and program
    // pattern ram for all settings.
    loadTrainingPatramTable(&dmemPointer);
    parseTrainingPatramTable();
    LoadTrainingPatram();
  #else     // hbm
    // for HBM the training pattern are only needed when boot time training is enabled
    // and pattern are only applied for read/write training.
    LwBool skipBoot = gTT.bootTrainingTable.MemBootTrainTblFlags.flag_skip_boot_training;
    if (!skipBoot)
    {
        loadTrainingPatramTable(&dmemPointer);
        parseTrainingPatramTable();
        LoadDataTrainingPatram(0);
        LoadDataTrainingPatram(1);	
    }
  #endif
#endif

    //
    // 9. load VBIOS internal use only table
    //
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
    loadVBIOSInternalUseOnly(&dmemPointer);
#endif

    //
    // update scratch code to INFRORMATION_TABLE_PARSED
    // FIXME: this should probably move to a secure registers,  e.g the new SELWRE_RD_SCRATCH
    FW_MBOX_WR32_STALL(15,
            (LW_UCODE_FBFALCON_FRTS_PROG_MEMORY_INFORMATION_TABLE_PARSED << 16));

    return status;
}

#endif  // !(FBFALCONCFG_FEATURE_ENABLED(VBIOS_INCLUDED_IN_BINARY))


#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR) || FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
/*
 * Write Training patram vbios tables in to fbpa
 */

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
LwU8
#else
LwU16
#endif
LoadDataTrainingPatram
(
        LwBool WritePattern

)
{

    MemTrainingPatternEntry* pMTPEp_loc;
    pMTPEp_loc = (MemTrainingPatternEntry*) (&gBiosTable.instMTPEp);

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    MemTrainPatternEntry10*     dat_pattern;
    MemTrainPatternEntry10*     dbi_pattern;
    MemTrainPatternEntry10*     edc_pattern;
#else
    MemTrainPatternEntry11*     dat_pattern;
    MemTrainPatternEntry11*     dbi_pattern;
    MemTrainPatternEntry11*     edc_pattern;
#endif
    MemTrainStrapEntry* pmtsepdata;
    MemTrainStrapEntry* pmtsepdbi;
    MemTrainStrapEntry* pmtsepedc;


    if (WritePattern == 1) {
        pmtsepdata = (MemTrainStrapEntry*) pMTPEp_loc->pMTSEpWrDly;
        pmtsepdbi = (MemTrainStrapEntry*) pMTPEp_loc->pMTSEpWrDbi;
        pmtsepedc = (MemTrainStrapEntry*) pMTPEp_loc->pMTSEpWrEdc;
    } else {
        pmtsepdata = (MemTrainStrapEntry*) pMTPEp_loc->pMTSEpRdDly;
        pmtsepdbi = (MemTrainStrapEntry*) pMTPEp_loc->pMTSEpRdDbi;
        pmtsepedc = (MemTrainStrapEntry*) pMTPEp_loc->pMTSEpRdEdc;
    }
    LwU8 DataPatternEntryIdx =  TABLE_VAL(pmtsepdata->PatternEntryIdx);
    LwU8 DbiPatternEntryIdx  =  TABLE_VAL(pmtsepdbi->PatternEntryIdx);
    LwU8 EdcPatternEntryIdx  =  TABLE_VAL(pmtsepedc->PatternEntryIdx);

    dat_pattern = pMTPEp_loc->pMTPEp_save[DataPatternEntryIdx];
    dbi_pattern = pMTPEp_loc->pMTPEp_save[DbiPatternEntryIdx];
    edc_pattern = pMTPEp_loc->pMTPEp_save[EdcPatternEntryIdx];

    if ((dat_pattern == NULL) ||
            (dbi_pattern == NULL) ||
            (edc_pattern == NULL))
    {
//        FW_MBOX_WR32_STALL(9, WritePattern);
//        FW_MBOX_WR32_STALL(10, DataPatternEntryIdx );
//        FW_MBOX_WR32_STALL(11, DbiPatternEntryIdx);
//        FW_MBOX_WR32_STALL(12, EdcPatternEntryIdx);
//        FW_MBOX_WR32_STALL(13, (LwU32)&gBiosTable.instMTPEp);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_LOAD_PTR_MISSING);
    }


#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU8 DataPatternCount = TABLE_VAL(dat_pattern->BaseEntry.PatternCount);
    LwU8 EdcPatternCount = TABLE_VAL(edc_pattern->BaseEntry.PatternCount);
    LwU8 DbiPatternCount = TABLE_VAL(dbi_pattern->BaseEntry.PatternCount);
#else
    LwU16 DataPatternCount = TABLE_VAL(dat_pattern->BaseEntry.PatternCount);
    LwU16 EdcPatternCount = TABLE_VAL(edc_pattern->BaseEntry.PatternCount);
    LwU16 DbiPatternCount = TABLE_VAL(dbi_pattern->BaseEntry.PatternCount);
#endif

    if ((EdcPatternCount < DataPatternCount) ||
            (DbiPatternCount < DataPatternCount) ||
            (EdcPatternCount != DbiPatternCount))
    {
        //FW_MBOX_WR32_STALL(1, DataPatternCount);
        //FW_MBOX_WR32_STALL(1, DbiPatternCount);
        //FW_MBOX_WR32_STALL(1, EdcPatternCount);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_RD_DATA_EDC_DBI_COUNT_MISMATCH);
    }
    LwU8 DataPatternSecEntryIdx = TABLE_VAL(dat_pattern->BaseEntry.PatternEntryIdx);
    LwU8 DbiPatternSecEntryIdx = TABLE_VAL(dbi_pattern->BaseEntry.PatternEntryIdx);
    LwU8 EdcPatternSecEntryIdx = TABLE_VAL(edc_pattern->BaseEntry.PatternEntryIdx);
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    MemTrainPatternEntry10* dat_sec_pattern = pMTPEp_loc->pMTPEp_save[DataPatternSecEntryIdx];
    MemTrainPatternEntry10* dbi_sec_pattern = pMTPEp_loc->pMTPEp_save[DbiPatternSecEntryIdx];
    MemTrainPatternEntry10* edc_sec_pattern = pMTPEp_loc->pMTPEp_save[EdcPatternSecEntryIdx];
#else
    MemTrainPatternEntry11* dat_sec_pattern = pMTPEp_loc->pMTPEp_save[DataPatternSecEntryIdx];
    MemTrainPatternEntry11* dbi_sec_pattern = pMTPEp_loc->pMTPEp_save[DbiPatternSecEntryIdx];
    MemTrainPatternEntry11* edc_sec_pattern = pMTPEp_loc->pMTPEp_save[EdcPatternSecEntryIdx];
#endif

    if ((dat_sec_pattern == NULL) ||
            (dbi_sec_pattern == NULL) ||
            (edc_sec_pattern == NULL))
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_LOAD_SEC_PTR_MISSING);
    }
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU8 i = 0;
    LwU8 j = 0;
    LwU8 k = 0;
#else
    LwU16 i = 0;
    LwU16 j = 0;
    LwU16 k = 0;
#endif

    LwU32 rddat = 0;
    LwU8  rdedc = 0;
    LwU8  rddbi = 0;
    LwU8 rddatTemp[4];
    LwU8 PatternPtr[2];
    LwU8 EdcPatternIdx[2];
    LwU8 DbiPatternIdx[2];
    for (i =0; i <= (DataPatternCount/2); i++)
    {

        PatternPtr[0] = TABLE_VAL(dat_pattern->PatternBuffer[i]);
        PatternPtr[1] = (PatternPtr[0] & 0xF0)>> 4;
        PatternPtr[0] = (PatternPtr[0] & 0x0F);
        EdcPatternIdx[0] = TABLE_VAL(edc_pattern->PatternBuffer[i]);
        EdcPatternIdx[1] = (EdcPatternIdx[0] & 0xF0) >> 4;
        EdcPatternIdx[0] = EdcPatternIdx[0] & 0x0F;
        DbiPatternIdx[0] = TABLE_VAL(dbi_pattern->PatternBuffer[i]);
        DbiPatternIdx[1] = (DbiPatternIdx[0] & 0xF0) >> 4;
        DbiPatternIdx[0] = DbiPatternIdx[0] & 0x0F;
        for  (k = 0 ; k < 2; k++ )
        {
            for (j =0; j<4; j++)
            {
                rddatTemp[j] = (TABLE_VAL(dat_sec_pattern->PatternBuffer[(PatternPtr[k] * 4)+j]));
                rddat = (rddat << 8) | rddatTemp[j];
            }

            //EDC
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
            rdedc = TABLE_VAL(edc_sec_pattern->PatternBuffer[(EdcPatternIdx[k] * 4)]);
#else
            rdedc = TABLE_VAL(EdcPatternIdx[k]);
#endif

            //DBI
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
            rddbi = TABLE_VAL(dbi_sec_pattern->PatternBuffer[(DbiPatternIdx[k] * 4)]);
#else
            rddbi = TABLE_VAL(DbiPatternIdx[k]);
#endif

            //TODO Possibly integrate HBM load function?
#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100)) && FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))
            if (WritePattern == 1)
            {
                CharPatramDQ_wr[(i * 2) + k] = rddat;
                CharPatramDBI_wr[(i * 2) + k] = rddbi;
                CharPatramECC_wr[(i * 2) + k] = rdedc;
            }
            else
            {
                CharPatramDQ_rd[(i * 2) + k] = rddat;
                CharPatramDBI_rd[(i * 2) + k] = rddbi;
                CharPatramECC_rd[(i * 2) + k] = rdedc;
            }
#else
            LwU32 dpcontd = rdedc & (0xF); //restricting the edc to 4 bits, as LW_PFB_FBPA_TRAINING_DP_CNTD_EDC is [7:4]
            dpcontd = (dpcontd << 4 ) | rddbi;

            if (WritePattern == 1)
            {
                WrPatternDp[(i * 2) + k] = rddat;
                WrPatternDpContd[(i * 2) + k] = dpcontd;
            }
            else
            {
                RdPatternDp[(i * 2) + k] = rddat;
                RdPatternDpContd[(i * 2) + k] = dpcontd;
            }
#endif
        }
    }

    return DataPatternCount;
}

LwU32
LoadAddrTrainingPatram
(
        void
)
{

    MemTrainingPatternEntry* pMTPEp_loc;
    pMTPEp_loc = (MemTrainingPatternEntry*) (&gBiosTable.instMTPEp);


    MemTrainStrapEntry*         pmtsepaddr;
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    MemTrainPatternEntry10*     addr_pattern;
    MemTrainPatternEntry10*     addr_sec_pattern;
#else
    MemTrainPatternEntry11*     addr_pattern;
    MemTrainPatternEntry11*     addr_sec_pattern;
#endif
    addr_pattern = NULL;
    addr_sec_pattern = NULL;

    pmtsepaddr = (MemTrainStrapEntry*) pMTPEp_loc->pMTSEpAddrDly;

    LwU8 AddrPatternEntryIdx = TABLE_VAL(pmtsepaddr->PatternEntryIdx);

    addr_pattern = pMTPEp_loc->pMTPEp_save[AddrPatternEntryIdx];

    if (addr_pattern == NULL)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_LOAD_PTR_MISSING);
    }

    LwU8 AddrPatternCount = TABLE_VAL(addr_pattern->BaseEntry.PatternCount);
    LwU8 AddrPatternSecEntryIdx = TABLE_VAL(addr_pattern->BaseEntry.PatternEntryIdx);
    addr_sec_pattern = pMTPEp_loc->pMTPEp_save[AddrPatternSecEntryIdx];


    if (addr_sec_pattern == NULL)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_LOAD_SEC_PTR_MISSING);
    }

    LwU8 i = 0;
    LwU8 j = 0;
    LwU8 k = 0;
    LwU32 AddrPat = 0;
    LwU8 AddrTemp[4];
    LwU8 PatternPtr[2];
    LwU16 addrpatindex =0;


    do {
        for ( i = 0; i <= (AddrPatternCount/2); i++)
        {
            PatternPtr[0] = TABLE_VAL(addr_pattern->PatternBuffer[i]);
            PatternPtr[1] = (PatternPtr[0] & 0xF0)>>4;
            PatternPtr[0] = (PatternPtr[0] & 0x0F);

            for ( j = 0; j < 2; j++)
            {
                if ( (i * 2 + j)  < (AddrPatternCount) ) {
                    for (k=0; k < 4; k++)
                    {
                        if (PatternPtr[j] >= 8) {
                            LwU8 tempindex;
                            tempindex = (j ==0)? 1:0;
                            AddrTemp[k] = (TABLE_VAL(addr_sec_pattern->PatternBuffer[(PatternPtr[tempindex] * 4) + k]));
                        } else {
                            AddrTemp[k] = (TABLE_VAL(addr_sec_pattern->PatternBuffer[(PatternPtr[j] * 4) + k]));
                        }
                        AddrPat = (AddrPat << 8) | AddrTemp[k];
                    }
                    AddrPatternDp[addrpatindex] = AddrPat;
                    addrpatindex= addrpatindex + 1;
                    if (addrpatindex >= MAX_ADDR_TRAINING_BUFFER_SIZE) {
                        break;
                    }

                }
            }
            if (addrpatindex >= MAX_ADDR_TRAINING_BUFFER_SIZE) {
                break;
            }

        }
    } while (addrpatindex < MAX_ADDR_TRAINING_BUFFER_SIZE);
    return (addrpatindex);
}

// Load write array patterns into registers
void
FuncLoadWrPatterns
(
        LwU32 DdrMode
)
{
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU32 gbl_prbs_mode ;
#endif
    LwU32 gbl_dual_mode ;
    LwU32 gbl_periodic_rd ;
    LwU32 gbl_periodic_wr ;
    LwU32 gbl_rd ;
    LwU32 gbl_wr ;
    LwU32 fbpa_training_patram;
    LwU32 fbpa_training_patram_select;
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU8  PatternSize;
#else
    LwU16 PatternSize;
#endif

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    gbl_prbs_mode = 0;
#endif
    gbl_periodic_wr = 0;
    gbl_wr = 0;

    if (DdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
        gbl_dual_mode = 0;
        gbl_rd = 8;
        gbl_periodic_rd = 8;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    } else if (DdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        gbl_dual_mode = 1;
        gbl_rd = 32;
        gbl_periodic_rd = 32;
#endif
    } else {
        gbl_dual_mode = 1;
        gbl_rd = 16;
        gbl_periodic_rd = 16;
    }

    fbpa_training_patram = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    fbpa_training_patram = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_PATRAM,  _DUAL_MODE,      gbl_dual_mode,  fbpa_training_patram);
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_PATRAM,  fbpa_training_patram);

    //this register is PL3 - falcon secondary boot may not have access to RMW.
    //inspite of adding this define the ucode is accessing this register
    //have temp fixed memory.def to stub the function for ga10x - FIXME
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    memorySetTrainingControlPrbsMode_HAL(&Fbflcn, gbl_prbs_mode);
#endif

    fbpa_training_patram_select= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM_SELECT);
    fbpa_training_patram_select = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_PATRAM_SELECT,   _PERIODIC_RD,    gbl_periodic_rd,        fbpa_training_patram_select);
    fbpa_training_patram_select = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_PATRAM_SELECT,   _PERIODIC_WR,    gbl_periodic_wr,        fbpa_training_patram_select);
    fbpa_training_patram_select = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_PATRAM_SELECT,   _RD,     gbl_rd,         fbpa_training_patram_select);
    fbpa_training_patram_select = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_PATRAM_SELECT,   _WR,     gbl_wr,         fbpa_training_patram_select);
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_PATRAM_SELECT,   fbpa_training_patram_select);

    PatternSize = LoadDataTrainingPatram(1);

    // For HBM another function is called to load the data from arrays
#if (!(FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
    LwU32 gbl_act_adr = 0;
    LwU32 gbl_adr = 0;
    LwU32 gbl_dp;
    LwU32 fbpa_training_pattern_ptr;
    fbpa_training_pattern_ptr= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0));

    LwU32 fbpa_training_dp;
    LwU32 fbpa_training_dp_cntd;
    fbpa_training_dp      = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DP(0));       // FIXME confirm coding style with Stefan
    fbpa_training_dp_cntd = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DP_CNTD(0));       // FIXME confirm coding style with Stefan

    //PatternSize =  PatternSize + 1; //offsetting to make 96 instead of 95
    //if (gbl_dual_mode == 1) {
    //  PatternSize = PatternSize * 2;
    //}


    for (gbl_dp=0; gbl_dp <= PatternSize; gbl_dp=gbl_dp+1) {
        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _ACT_ADR,        gbl_act_adr,    fbpa_training_pattern_ptr);
        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _ADR,    gbl_adr,        fbpa_training_pattern_ptr);
        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _DP,     gbl_dp,         fbpa_training_pattern_ptr);
        REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),     fbpa_training_pattern_ptr);
        REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),     fbpa_training_pattern_ptr);

        fbpa_training_dp = WrPatternDp[gbl_dp];
        fbpa_training_dp_cntd = WrPatternDpContd[gbl_dp];
        fbpa_training_dp_cntd = FLD_SET_DRF(_PFB,_FBPA_TRAINING_DP_CNTD,_SEL,_WR,fbpa_training_dp_cntd);
        REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_DP_CNTD(0),     fbpa_training_dp_cntd);
        REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_DP_CNTD(1),     fbpa_training_dp_cntd);
        REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_DP(0),          fbpa_training_dp);
        REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_DP(1),          fbpa_training_dp);

    }
#endif

}


// Load read array patterns into registers
GCC_ATTRIB_SECTION("resident", "FuncLoadRdPatterns")
void
FuncLoadRdPatterns
(
        LwU32 DdrMode
)
{
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU32 gbl_prbs_mode ;
#endif
    LwU32 gbl_dual_mode ;
    LwU32 gbl_periodic_rd ;
    LwU32 gbl_periodic_wr ;
    LwU32 gbl_rd ;
    LwU32 gbl_wr ;
    LwU32 fbpa_training_patram;
    LwU32 fbpa_training_patram_select;
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU8  PatternSize;
#else
    LwU16 PatternSize;
#endif

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    gbl_prbs_mode = 0;
#endif
    gbl_periodic_wr = 0;
    gbl_wr = 0;

    if (DdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
        gbl_dual_mode = 0;
        gbl_rd = 8;
        gbl_periodic_rd = 8;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    } else if (DdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        gbl_dual_mode = 1;
        gbl_rd = 32;
        gbl_periodic_rd = 32;
#endif
    } else {
        gbl_dual_mode = 1;
        gbl_rd = 16;
        gbl_periodic_rd = 16;
    }

    fbpa_training_patram= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    fbpa_training_patram = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_PATRAM,  _DUAL_MODE,      gbl_dual_mode,  fbpa_training_patram);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM,  fbpa_training_patram);

    //this register is PL3 - falcon secondary boot may not have access to RMW.
    //inspite of adding this define the ucode is accessing this register
    //have temp fixed memory.def to stub the function for ga10x - FIXME
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    memorySetTrainingControlPrbsMode_HAL(&Fbflcn, gbl_prbs_mode);
#endif
    fbpa_training_patram_select= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM_SELECT);
    fbpa_training_patram_select = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_PATRAM_SELECT,   _PERIODIC_RD,    gbl_periodic_rd,        fbpa_training_patram_select);
    fbpa_training_patram_select = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_PATRAM_SELECT,   _PERIODIC_WR,    gbl_periodic_wr,        fbpa_training_patram_select);
    fbpa_training_patram_select = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_PATRAM_SELECT,   _RD,     gbl_rd,         fbpa_training_patram_select);
    fbpa_training_patram_select = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_PATRAM_SELECT,   _WR,     gbl_wr,         fbpa_training_patram_select);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM_SELECT,   fbpa_training_patram_select);

    PatternSize = LoadDataTrainingPatram(0);

    // For HBM another function is called to load the data from arrays
#if (!(FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
    LwU32 gbl_act_adr = 0;
    LwU32 gbl_adr = 0;
    LwU32 gbl_dp;
    LwU32 fbpa_training_pattern_ptr;
    fbpa_training_pattern_ptr= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0));

    LwU32 fbpa_training_dp;
    LwU32 fbpa_training_dp_cntd;
    fbpa_training_dp      = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DP(0));       // FIXME confirm coding style with Stefan
    fbpa_training_dp_cntd = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DP_CNTD(0));       // FIXME confirm coding style with Stefan

    //PatternSize += 1; //offsetting to make 96 instead of 95
    //if (gbl_dual_mode == 1) {
    //  PatternSize *= 2;
    //}

    for (gbl_dp=0; gbl_dp <= PatternSize; gbl_dp=gbl_dp+1) {
        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _ACT_ADR,        gbl_act_adr,    fbpa_training_pattern_ptr);
        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _ADR,    gbl_adr,        fbpa_training_pattern_ptr);
        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _DP,     gbl_dp,         fbpa_training_pattern_ptr);
        REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),     fbpa_training_pattern_ptr);
        REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),     fbpa_training_pattern_ptr);

        fbpa_training_dp = RdPatternDp[gbl_dp];
        fbpa_training_dp_cntd = RdPatternDpContd[gbl_dp];
        fbpa_training_dp_cntd = FLD_SET_DRF(_PFB,_FBPA_TRAINING_DP_CNTD,_SEL,_RD,fbpa_training_dp_cntd);
        REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_DP_CNTD(0),     fbpa_training_dp_cntd);
        REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_DP_CNTD(1),     fbpa_training_dp_cntd);
        REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_DP(0),          fbpa_training_dp);
        REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_DP(1),          fbpa_training_dp);

    }
#endif
}


// Load address array patterns into registers
GCC_ATTRIB_SECTION("resident", "FuncLoadAdrPatterns")
void
FuncLoadAdrPatterns
(
        LwU32 DdrMode
)
{
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU32 gbl_prbs_mode = 0;
#endif
    LwU32 gbl_dual_mode = 1;
    if (DdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
        gbl_dual_mode = 0;
    }

    LwU32 fbpa_training_patram;
    fbpa_training_patram= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    fbpa_training_patram = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_PATRAM,  _DUAL_MODE,      gbl_dual_mode,  fbpa_training_patram);
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_PATRAM,  fbpa_training_patram);

    //this register is PL3 - falcon secondary boot may not have access to RMW.
    //inspite of adding this define the ucode is accessing this register
    //have temp fixed memory.def to stub the function for ga10x - FIXME
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    memorySetTrainingControlPrbsMode_HAL(&Fbflcn, gbl_prbs_mode);
#endif

    // TODO: get optimal values from silicon
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    LwU32 gbl_addr_end = 31;
    LwU32 gbl_addr_start = 16;
#else
    LwU32 gbl_addr_end = 0x4F;
    LwU32 gbl_addr_start = 0x40;
#endif
    LwU32 fbpa_training_patram_select2;
    fbpa_training_patram_select2 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM_SELECT2);
    fbpa_training_patram_select2 = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_PATRAM_SELECT2,  _ADDR_END,       gbl_addr_end,   fbpa_training_patram_select2);
    fbpa_training_patram_select2 = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_PATRAM_SELECT2,  _ADDR_START,     gbl_addr_start,         fbpa_training_patram_select2);
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_TRAINING_PATRAM_SELECT2,  fbpa_training_patram_select2);


    LwU32 PatternCount;
    PatternCount = LoadAddrTrainingPatram();

    LwU32 gbl_adr;
    LwU32 gbl_act_adr = 0;    // TRAINING_PATTERN_PTR_ACT_ADR=DISABLED for address training
    LwU32 gbl_dp =0;
    LwU32 fbpa_training_pattern_ptr;
    LwU32 fbpa_training_adr;
    fbpa_training_pattern_ptr= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0));
    fbpa_training_adr= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0));


    for (gbl_adr=0; gbl_adr < MAX_ADDR_TRAINING_BUFFER_SIZE; gbl_adr=gbl_adr+1) {


        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _ADR,    gbl_adr,        fbpa_training_pattern_ptr);
        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _ACT_ADR,        gbl_act_adr,    fbpa_training_pattern_ptr);
        fbpa_training_pattern_ptr = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_PATTERN_PTR,     _DP,        gbl_dp,    fbpa_training_pattern_ptr);
        REG_WR32_STALL(LOG, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),     fbpa_training_pattern_ptr);
        REG_WR32_STALL(LOG, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),     fbpa_training_pattern_ptr);    // FIXME add check for halfFbpa before programming PTR(1)

        fbpa_training_adr = AddrPatternDp[gbl_adr];
        REG_WR32_STALL(LOG, LW_PFB_FBPA_TRAINING_ADR(0),     fbpa_training_adr);
        REG_WR32_STALL(LOG, LW_PFB_FBPA_TRAINING_ADR(1),     fbpa_training_adr);

    }
}


//Adding a WAR for loading simpattern, must be removed when address training
//is no longer part of mclk switch and boot time training is up.
void
FuncLoadG6SimAdrPattern
(
        void
)
{
    /*
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM,0x0c0f8111);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM_SELECT2,0x00001f10);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000000);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000000);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x99aa43f4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x99aa43f4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000001);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000001);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa37c26e4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa37c26e4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000002);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000002);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb93dfb41);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb93dfb41);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000003);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000003);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd7992fd2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd7992fd2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000004);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000004);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xaaae82f8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xaaae82f8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000005);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000005);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf633c3d0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf633c3d0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000006);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000006);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6ce5b5bc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6ce5b5bc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000007);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000007);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2bc56156);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2bc56156);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000008);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000008);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6a25930e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6a25930e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000009);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000009);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf0f5ba9c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf0f5ba9c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000000a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000000a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x01e97549);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x01e97549);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000000b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000000b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf528dcf6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf528dcf6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000000c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000000c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd2fccb96);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd2fccb96);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000000d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000000d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x7ac2a5f7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x7ac2a5f7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000000e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000000e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xca0d55a7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xca0d55a7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000000f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000000f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5fb176ff);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5fb176ff);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000010);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000010);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb0567af5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb0567af5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000011);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000011);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5e811ee1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5e811ee1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000012);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000012);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2d0cedd2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2d0cedd2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000013);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000013);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x23496ac0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x23496ac0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000014);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000014);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x76feea97);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x76feea97);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000015);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000015);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2f1c0a5c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2f1c0a5c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000016);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000016);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xbd421da9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xbd421da9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000017);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000017);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf6ce415d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf6ce415d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000018);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000018);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xaf5f1f36);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xaf5f1f36);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000019);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000019);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb34b8e62);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb34b8e62);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000001a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000001a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x113891d1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x113891d1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000001b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000001b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x0a8ed1e2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x0a8ed1e2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000001c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000001c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xbba4d95b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xbba4d95b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000001d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000001d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x26c3e219);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x26c3e219);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000001e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000001e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9b9b21d9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9b9b21d9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000001f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000001f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x3c20ae9a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x3c20ae9a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000020);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000020);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x4b4b9a25);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x4b4b9a25);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000021);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000021);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe2846d45);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe2846d45);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000022);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000022);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x02166c9e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x02166c9e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000023);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000023);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x918e93a1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x918e93a1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000024);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000024);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xcdaab048);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xcdaab048);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000025);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000025);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x3ab4eafd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x3ab4eafd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000026);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000026);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x579e56f1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x579e56f1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000027);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000027);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6fe6aab8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6fe6aab8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000028);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000028);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xdce89158);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xdce89158);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000029);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000029);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xed2583a1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xed2583a1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000002a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000002a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xc53a5076);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xc53a5076);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000002b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000002b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x3a48b858);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x3a48b858);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000002c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000002c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x99f40347);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x99f40347);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000002d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000002d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe362120a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe362120a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000002e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000002e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x605682ce);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x605682ce);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000002f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000002f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x30018609);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x30018609);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000030);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000030);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xbd27da2c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xbd27da2c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000031);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000031);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x4d980803);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x4d980803);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000032);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000032);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5cd64ef8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5cd64ef8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000033);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000033);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xab421df3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xab421df3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000034);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000034);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2e464e45);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2e464e45);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000035);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000035);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xeb91a5f8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xeb91a5f8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000036);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000036);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x4606bcf4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x4606bcf4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000037);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000037);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x53d6abb5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x53d6abb5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000038);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000038);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x05c52e3f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x05c52e3f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000039);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000039);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5e0079ed);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5e0079ed);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000003a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000003a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2a55c1a9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2a55c1a9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000003b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000003b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x8b2a1675);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x8b2a1675);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000003c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000003c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x293471b4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x293471b4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000003d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000003d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6f7c59b1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6f7c59b1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000003e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000003e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x849d7f08);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x849d7f08);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000003f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000003f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb620a837);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb620a837);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000040);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000040);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x491ceff4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x491ceff4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000041);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000041);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb93b4c52);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb93b4c52);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000042);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000042);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6920e270);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6920e270);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000043);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000043);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x17255672);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x17255672);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000044);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000044);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe84d3f07);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe84d3f07);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000045);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000045);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xdeda16d3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xdeda16d3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000046);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000046);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf6494554);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf6494554);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000047);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000047);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x7c6a7fe0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x7c6a7fe0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000048);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000048);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x8f66fef2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x8f66fef2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000049);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000049);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xcdd4601c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xcdd4601c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000004a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000004a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x080af41a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x080af41a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000004b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000004b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x05183395);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x05183395);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000004c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000004c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x33df1539);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x33df1539);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000004d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000004d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x23c6a031);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x23c6a031);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000004e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000004e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9ad0bf46);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9ad0bf46);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000004f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000004f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe92f5553);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe92f5553);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000050);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000050);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xbbc79ca5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xbbc79ca5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000051);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000051);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb9834ea7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb9834ea7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000052);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000052);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x762cfad7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x762cfad7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000053);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000053);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x548a331c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x548a331c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000054);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000054);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xffe28d47);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xffe28d47);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000055);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000055);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa3a80452);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa3a80452);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000056);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000056);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x92908ff0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x92908ff0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000057);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000057);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xaad86007);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xaad86007);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000058);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000058);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x97e860bc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x97e860bc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000059);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000059);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x0af68443);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x0af68443);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000005a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000005a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x0e2f04bb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x0e2f04bb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000005b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000005b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa9fce857);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa9fce857);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000005c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000005c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x79de36b1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x79de36b1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000005d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000005d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x8a80dff1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x8a80dff1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000005e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000005e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x66409e8a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x66409e8a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000005f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000005f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe71332c9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe71332c9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000060);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000060);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x290b3da9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x290b3da9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000061);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000061);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x36f72ab5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x36f72ab5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000062);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000062);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6942023e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6942023e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000063);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000063);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb1d0232c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb1d0232c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000064);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000064);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x03d563fd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x03d563fd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000065);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000065);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb458707b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb458707b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000066);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000066);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf6e2f6ec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf6e2f6ec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000067);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000067);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf73a5237);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf73a5237);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000068);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000068);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9dc99524);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9dc99524);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000069);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000069);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x652fe3b6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x652fe3b6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000006a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000006a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x094c2ebd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x094c2ebd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000006b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000006b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd10d6e96);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd10d6e96);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000006c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000006c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xec940734);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xec940734);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000006d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000006d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x93a27694);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x93a27694);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000006e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000006e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xbb7e3a15);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xbb7e3a15);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000006f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000006f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe7ca1b45);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe7ca1b45);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000070);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000070);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x17f15ca8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x17f15ca8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000071);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000071);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb8205373);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb8205373);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000072);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000072);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x780a08f6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x780a08f6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000073);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000073);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x1bb4df21);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x1bb4df21);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000074);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000074);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xade99e63);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xade99e63);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000075);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000075);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa6d0e892);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa6d0e892);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000076);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000076);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x44859ea7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x44859ea7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000077);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000077);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x521bf9bc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x521bf9bc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000078);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000078);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd23251f5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd23251f5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000079);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000079);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5b077b0b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5b077b0b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000007a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000007a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2f25db90);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2f25db90);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000007b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000007b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x21653170);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x21653170);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000007c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000007c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x39a5301c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x39a5301c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000007d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000007d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6a797500);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6a797500);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000007e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000007e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x57108167);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x57108167);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000007f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000007f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x14e1eeb6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x14e1eeb6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000080);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000080);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa58d3f8c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa58d3f8c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000081);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000081);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6fcee314);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6fcee314);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000082);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000082);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5b87d591);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5b87d591);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000083);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000083);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa27938b8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa27938b8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000084);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000084);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x666457f2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x666457f2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000085);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000085);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x38b8d51d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x38b8d51d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000086);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000086);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x544385c2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x544385c2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000087);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000087);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xc51c6724);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xc51c6724);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000088);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000088);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xbeb75136);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xbeb75136);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000089);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000089);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6e409617);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6e409617);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000008a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000008a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd460b2e6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd460b2e6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000008b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000008b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa8f49d42);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa8f49d42);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000008c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000008c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xdd876301);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xdd876301);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000008d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000008d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x31bdef5b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x31bdef5b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000008e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000008e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x0b8d4643);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x0b8d4643);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000008f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000008f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6e846248);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6e846248);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000090);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000090);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x67e3787e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x67e3787e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000091);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000091);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa511eb0f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa511eb0f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000092);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000092);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6cb0f4de);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6cb0f4de);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000093);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000093);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd1d9eaec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd1d9eaec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000094);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000094);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x03dc7c63);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x03dc7c63);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000095);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000095);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x1ca6c9e0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x1ca6c9e0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000096);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000096);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x8898951e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x8898951e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000097);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000097);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5fa3e83b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5fa3e83b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000098);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000098);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x27a3e132);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x27a3e132);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x00000099);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x00000099);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x548d1fee);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x548d1fee);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000009a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000009a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x1c4eaaaf);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x1c4eaaaf);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000009b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000009b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xccc3efa9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xccc3efa9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000009c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000009c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x36d3e9bb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x36d3e9bb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000009d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000009d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2b194d08);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2b194d08);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000009e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000009e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe8164ca9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe8164ca9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x0000009f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x0000009f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x4b16d068);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x4b16d068);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x19d575e6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x19d575e6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x49e3c419);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x49e3c419);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5babc9f0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5babc9f0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x38dc69fd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x38dc69fd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x604f57af);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x604f57af);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xad41d5e1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xad41d5e1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xc0c0afdd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xc0c0afdd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xfcadd80f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xfcadd80f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x53c77470);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x53c77470);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000a9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000a9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf973f2e7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf973f2e7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000aa);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000aa);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x73df171d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x73df171d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ab);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ab);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x0f8a0781);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x0f8a0781);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ac);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ac);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x535e3666);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x535e3666);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ad);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ad);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x0f2198ad);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x0f2198ad);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ae);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ae);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x383a5ad9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x383a5ad9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000af);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000af);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x3e6e08c3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x3e6e08c3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5911126d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5911126d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd58a5e25);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd58a5e25);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x72b19e17);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x72b19e17);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xbe6fb365);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xbe6fb365);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9c79260e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9c79260e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xeb56d364);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xeb56d364);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6daac35f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6daac35f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x4ea66eec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x4ea66eec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x22c731bb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x22c731bb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000b9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000b9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x066b2895);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x066b2895);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ba);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ba);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe8b2baa1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe8b2baa1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000bb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000bb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xeb9734e9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xeb9734e9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000bc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000bc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x700e7358);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x700e7358);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000bd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000bd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9ac8d02f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9ac8d02f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000be);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000be);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x7e3a4956);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x7e3a4956);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000bf);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000bf);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x75962046);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x75962046);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xff0424fd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xff0424fd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xb7ab906a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xb7ab906a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9594b0e4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9594b0e4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9fe11de3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9fe11de3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2a71a3fa);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2a71a3fa);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6be1b7ee);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6be1b7ee);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x09cfd746);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x09cfd746);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xde5c9260);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xde5c9260);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9af3841b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9af3841b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000c9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000c9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe62669cd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe62669cd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ca);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ca);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xad53d5ec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xad53d5ec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000cb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000cb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x3d14093c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x3d14093c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000cc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000cc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6480132c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6480132c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000cd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000cd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x974034b3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x974034b3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ce);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ce);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xc8b212df);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xc8b212df);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000cf);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000cf);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd42e411f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd42e411f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xddd410bf);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xddd410bf);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf915695c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf915695c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x6eea4828);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x6eea4828);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x36252973);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x36252973);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf88c9e2c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf88c9e2c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x1b0ae445);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x1b0ae445);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x74ee1d72);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x74ee1d72);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xea07a538);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xea07a538);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xd7a1aad8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xd7a1aad8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000d9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000d9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x78193ea5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x78193ea5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000da);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000da);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x5b9237ee);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x5b9237ee);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000db);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000db);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf9942718);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf9942718);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000dc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000dc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x736460ba);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x736460ba);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000dd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000dd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe14f9a9e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe14f9a9e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000de);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000de);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x04a5e476);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x04a5e476);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000df);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000df);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x60587ab9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x60587ab9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xea59551b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xea59551b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xdf3a7eb0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xdf3a7eb0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x7bd8bff4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x7bd8bff4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xdef44f52);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xdef44f52);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x557e819c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x557e819c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf918746d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf918746d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xf4a80205);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xf4a80205);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x51615780);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x51615780);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x8109497d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x8109497d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000e9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000e9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe92d5e72);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe92d5e72);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ea);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ea);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xc6034dd8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xc6034dd8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000eb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000eb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x31fd125a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x31fd125a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ec);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x38a90f1b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x38a90f1b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ed);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ed);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x3520b995);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x3520b995);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ee);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ee);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x2cf32d5c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x2cf32d5c);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ef);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ef);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xca3d91c2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xca3d91c2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f0);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x1c1f1dba);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x1c1f1dba);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f1);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa7623d5b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa7623d5b);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xe7899a9a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xe7899a9a);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f3);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xa6bbd1fe);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xa6bbd1fe);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f4);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x66c4eb85);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x66c4eb85);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f5);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x36538fac);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x36538fac);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f6);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xdb073b5e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xdb073b5e);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f7);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xfbd37686);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xfbd37686);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f8);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x36a5f7d2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x36a5f7d2);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000f9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000f9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x66acffc9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x66acffc9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000fa);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000fa);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x69a6331d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x69a6331d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000fb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000fb);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x9bc1001f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x9bc1001f);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000fc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000fc);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xae62c9a9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xae62c9a9);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000fd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000fd);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0xc37e7c7d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0xc37e7c7d);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000fe);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000fe);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x7825af12);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x7825af12);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),0x000000ff);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),0x000000ff);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),0x1c24d028);
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),0x1c24d028);
     */
    return;
}


void
funcLoadPascalG5AdrPattern
(
        void
)
{
    /*
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000000);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000000);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000001);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000001);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000002);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000002);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000003);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000003);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000004);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000004);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000005);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000005);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000006);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000006);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000007);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000007);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000008);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000008);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000009);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000009);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000000a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000000a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000000b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000000b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000000c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000000c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000000d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000000d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000000e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000000e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000000f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000000f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000010);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000010);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000011);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000011);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000012);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000012);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000013);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000013);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000014);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000014);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000015);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000015);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000016);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000016);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000017);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000017);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000018);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000018);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000019);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000019);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000001a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000001a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000001b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000001b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000001c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000001c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000001d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000001d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000001e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000001e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000001f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000001f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000020);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000020);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000021);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000021);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000022);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000022);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000023);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000023);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000024);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000024);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000025);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000025);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000026);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000026);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000027);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000027);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000028);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000028);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000029);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000029);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000002a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000002a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000002b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000002b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000002c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000002c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000002d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000002d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000002e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000002e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000002f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000002f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000030);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000030);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000031);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000031);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000032);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000032);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000033);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000033);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000034);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000034);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000035);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000035);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000036);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000036);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000037);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000037);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000038);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000038);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000039);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000039);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000003a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000003a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000003b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000003b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000003c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000003c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000003d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000003d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000003e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000003e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000003f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000003f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000040);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000040);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000041);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000041);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000042);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000042);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000043);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000043);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000044);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000044);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000045);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000045);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000046);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000046);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000047);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000047);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000048);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000048);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000049);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000049);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000004a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000004a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000004b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000004b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000004c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000004c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000004d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000004d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000004e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000004e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000004f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000004f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000050);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000050);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000051);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000051);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000052);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000052);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000053);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000053);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000054);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000054);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000055);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000055);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000056);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000056);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000057);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000057);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000058);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000058);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000059);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000059);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000005a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000005a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000005b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000005b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000005c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000005c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000005d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000005d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000005e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000005e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000005f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000005f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000060);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000060);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000061);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000061);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000062);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000062);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000063);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000063);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000064);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000064);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000065);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000065);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000066);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000066);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000067);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000067);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000068);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000068);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000069);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000069);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000006a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000006a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000006b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000006b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000006c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000006c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000006d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000006d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000006e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000006e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000006f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000006f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000070);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000070);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000071);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000071);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000072);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000072);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000073);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000073);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000074);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000074);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000075);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000075);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000076);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000076);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000077);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000077);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000078);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000078);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000079);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000079);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000007a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000007a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000007b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000007b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000007c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000007c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000007d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000007d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000007e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000007e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000007f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000007f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000080);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000080);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000081);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000081);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000082);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000082);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000083);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000083);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000084);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000084);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000085);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000085);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000086);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000086);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000087);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000087);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000088);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000088);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000089);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000089);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000008a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000008a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000008b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000008b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000008c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000008c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000008d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000008d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000008e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000008e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000008f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000008f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000090);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000090);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000091);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000091);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000092);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000092);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000093);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000093);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000094);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000094);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000095);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000095);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000096);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000096);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000097);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000097);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000098);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000098);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x00000099);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x00000099);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000009a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000009a);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000009b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000009b);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000009c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000009c);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000009d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000009d);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000009e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000009e);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x0000009f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x0000009f);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000a9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000a9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000aa);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000aa);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ab);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ab);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ac);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ac);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ad);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ad);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ae);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ae);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000af);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000af);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000b9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000b9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ba);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ba);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000bb);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000bb);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000bc);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000bc);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000bd);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000bd);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000be);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000be);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000bf);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000bf);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000c9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000c9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ca);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ca);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000cb);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000cb);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000cc);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000cc);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000cd);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000cd);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ce);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ce);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000cf);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000cf);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000d9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000d9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000da);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000da);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000db);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000db);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000dc);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000dc);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x55555555 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000dd);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000dd);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xaaaaaaaa );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000de);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000de);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000df);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000df);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000e9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000e9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ea);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ea);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000eb);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000eb);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ec);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ec);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x0000ffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ed);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ed);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffff0000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ee);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ee);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ef);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ef);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f0);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f1);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f2);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f3);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f4);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f5);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f6);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f7);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f8);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000f9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000f9);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000fa);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000fa);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000fb);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000fb);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000fc);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000fc);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00ff00ff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000fd);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000fd);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xff00ff00 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000fe);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000fe);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0x00000000 );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(0),  0x000000ff);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1),  0x000000ff);
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(0),  0xffffffff );
REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_ADR(1),  0xffffffff );
     */
    return;
}


// Load training array patterns into registers
void
LoadTrainingPatram
(
        void
)
{
    LwU32 fbio_broadcast;
    LwU32 DdrMode;
    LwU32 SelwreWrScratch;
    LwU8 UnitSim;

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    SelwreWrScratch = REG_RD32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1));
#else
    SelwreWrScratch = 0;
#endif
    //if Unit sim the 0th bit of SelwreWrScratch will be 1

    UnitSim = SelwreWrScratch & 0xF;

    fbio_broadcast = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);
    DdrMode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE,fbio_broadcast);
    if (UnitSim == 0x0001) {
        DdrMode = SelwreWrScratch & 0xF00;
        DdrMode = DdrMode >> 8;
    }

    FuncLoadWrPatterns(DdrMode);
    FuncLoadRdPatterns(DdrMode);

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
    if (DdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
        //if ((DdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) ||
        FuncLoadAdrPatterns(DdrMode);
    }
#else
    FuncLoadAdrPatterns(DdrMode);
#endif
}

#endif

