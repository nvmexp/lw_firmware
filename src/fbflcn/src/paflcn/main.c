/* _LWRM_COPYRIGHT_BEGIN_
 
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   main.c
 */


// location: ./tools/falcon-gcc/main/4.1.6/Linux/falcon-elf-gcc/lib/gcc/falcon-elf/4.3.2/include/falcon-intrinsics.h
//           ./tools/falcon-gcc/falcon6/6.4.0/Linux/falcon-elf-gcc/lib/gcc/falcon-elf/4.3.2/include/falcon-intrinsics.h

#include <falcon-intrinsics.h>
#include "rmfbflcncmdif.h"

#include "dev_fbpa.h"
#include "dev_pafalcon_csb.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"

#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_interrupts.h"
#include "priv.h"
#include "seq_decoder.h"


#define PAFALCON_UCODE_REVISION 0x01001001
const static LwU32 gREV = PAFALCON_UCODE_REVISION;

//OBJFBFLCN Fbflcn;

extern void *__stack_chk_guard;
extern LwU32 _spMin;
extern LwU32 _stack;

void paflcnInterruptHandler(void);

PAFLCN_SEQUENCER_INFO_FIELD seqInfoField GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("sequencerInfo","seqInfoField");

static void GCC_ATTRIB_NO_STACK_PROTECT()
canary_setup(void)
{
    // using 32 ns timer
    __stack_chk_guard = (void*) DRF_VAL(_CFBFALCON, _FALCON_PTIMER0, _VAL,
            CSB_REG_RD32(LW_CFBFALCON_FALCON_PTIMER0));
}

// Overrides libssp definition so falcon dumps some error info
extern void GCC_ATTRIB_NO_STACK_PROTECT()
_wrap____stack_chk_fail(void)
{
    LwU32 status = REG_RD32(CSB, LW_CFBFALCON_SYNC_STATUS);

    // clear running bit, set errCode & DONE bit
    status = (status & 0x0000FFFD) | (FBFLCN_ERROR_CODE_STACK_CANARY_CORRUPTED<<16) | SEQ_DONE;
    REG_WR32(CSB, LW_CFBFALCON_SYNC_STATUS, status);
    REG_WR32(CSB, LW_CFBFALCON_SYNC_STATUS2, status);   // let FB know
    FBFLCN_HALT(FBFLCN_ERROR_CODE_STACK_CANARY_CORRUPTED);
}

// stack canary test buffer macro
#define TEST_BUFFER_OVERFLOW(bufferMemsetSize)            \
    do                                                    \
    {                                                     \
        volatile LwU32 buf;                               \
        memsetLwU8((void*)&buf, 0xFF, bufferMemsetSize);  \
    } while (0)

// canary test function
int functionWithSSPEnabledToCorruptStack(void)
{
    TEST_BUFFER_OVERFLOW(32);
    return -1;
}

GCC_ATTRIB_SECTION("sequencerData", "gsequencerData")
LwU32 gbl_pa_seq[PA_MAX_SEQ_SIZE] __attribute__ ((aligned (256)));

LwU32 gbl_dmemdata;
LwU32 gbl_dmemopcaddr;
LwU32 gbl_seq_cnt;


GCC_ATTRIB_NO_STACK_PROTECT()
int
main
(
        int argc,
        char **Argv
)
{
    // Setup SSP canary
    canary_setup();

    // Set SP_MIN
    CSB_REG_WR32(LW_CPAFALCON_FALCON_SP_MIN, DRF_VAL(_CPAFALCON, _FALCON_SP_MIN, _VALUE, (LwU32) &_spMin));

    FW_MBOX_WR32_STALL(2, gREV);
    FalconInterruptSetup();

    // test canary with a forced stack corruption
    //functionWithSSPEnabledToCorruptStack();

    LwU32 cg2_slcg = LW_CPAFALCON_FALCON_CG2_SLCG_ENABLED;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))
    // the enable on the tsync input that drives the real time normal is too short to fully
    // allow the randomized settle delay in rtl sims of up to 5 clk stages. The recomendation
    // from the IP team was to disable clk gating on that path. 
    cg2_slcg = FLD_SET_DRF(_CFBFALCON_FALCON,_CG2_SLCG,_FALCON_TSYNC, _DISABLED, cg2_slcg);
#endif // (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))
    REG_WR32(CSB,LW_CPAFALCON_FALCON_CG2, cg2_slcg);


    LwU32 rtnCode = LW_OK;
    // write something to make sure it gets added to linker data section
    gbl_pa_seq[(PA_MAX_SEQ_SIZE-1)] = 0xDEADBEEF;

    // let TB know where the sequencer starts
    FW_MBOX_WR32(4, ((LwU32)&gbl_pa_seq[0]));

    // let FB know we've booted up ok
    REG_WR32(CSB, LW_CFBFALCON_SYNC_STATUS, 0x0002);

    do {
        rtnCode = paStartSeq();

        if (rtnCode != LW_OK)
        {
            seqHalt(GET_PC(),0,rtnCode,(FBFLCN_ERROR_CODE_HALT_COMMAND<<16));
        }
    } while (rtnCode == LW_OK);

    // stop coded detected by the unit level environment for now, don't remove
    seqHalt(GET_PC(),0,rtnCode,(FBFLCN_ERROR_CODE_REACHED_END_OF_MAIN<<16));
    return 0;
}

void
paflcnInterruptHandler
(
        void
)
{
    // halt based on exterr
    // extra debug info in:
    // LW_CFBFALCON_FALCON_CSBERR_INFO/ADDR
    // LW_CFBFALCON_FALCON_CSBERRSTAT_PC
    LwU32 irqstat = REG_RD32(CSB, LW_CFBFALCON_FALCON_IRQSTAT);
    
    FW_MBOX_WR32(10, gbl_dmemdata);
    seqHalt(irqstat, gbl_dmemopcaddr, gbl_seq_cnt, (FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_PRIV_ERROR<<16));
    return;
}
