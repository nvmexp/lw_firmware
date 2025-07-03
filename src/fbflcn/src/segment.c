/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>


// include manuals
#include "dev_fb.h"
#include "dev_top.h"
#include "dev_fbfalcon_csb.h"

#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "segment.h"
#include "priv.h"



//
// loadSegment
//   loadSegment loads a segment from memory into the falcons imem space
//
void
loadSegment
(
        SegmentName_t segment
)
{
    // check segment is valid
    if ((segment < 1) || (segment >= SEGMENT_COUNT))
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_SEGMENT_REQUEST_ERROR);
    }
    else
    {

        // use the same context as bootloader
        // base address for imem was set in the bootloader
        LwU8 ctxDma = CTX_DMA_FBFALCON_READ_FROM_MWPR_0;


        // when a DMA read request for IMEM is queued in the DMA controller, the block number is taken
        // from the im_offset (bit 8..8+N-1), and the tag value is taken from the fb_offset (bit 8..8+Ti-1).
        // The 'imread' instruction has two operands and being used as 'imread s1 s2', where
        // fb_offset = s1 & 0xffff_ff00, and
        // im_offset = s2 & 0x0000_ff00 & (imem_size-1)
        // s1 can be seen as the source offset, s2 the destination offset.
        // -> This means the tag is computed from the source offset which is basically tied to the physical
        //    address.  This will work well for a single binary to be read in segments with the physical addr
        //    matching the virtual address (when including the tag)
        // -> In case of overlays with different physical addresses but the same virtual address the tag
        //    needs to reflect the virtual address to achieve that we need to modify the base address.


        LwU16 ctx = 0;
        falc_rspr(ctx, CTX);  // Read existing value
        ctx = (((ctx) & ~(0x7 << CTX_DMAIDX_SHIFT_IMREAD)) | ((ctxDma) << CTX_DMAIDX_SHIFT_IMREAD));
        falc_wspr(CTX, ctx); // Write new value


        LwU32 destOffs = _ovly_table[INIT_SEGMENT][VIRTUAL_ADDR];
        LwU32 destEnd = _ovly_table[INIT_SEGMENT][VIRTUAL_ADDR] + _ovly_table[segment][SIZE];
        LwU32 srcOffs = _ovly_table[segment][PHYS_ADDR];

        while (destOffs < destEnd)
        {
            falc_imread(srcOffs, destOffs);
            srcOffs  += FALCON_MEM_BLOCK_SIZE;
            destOffs += FALCON_MEM_BLOCK_SIZE;
        }
        falc_imwait();
    }
}


