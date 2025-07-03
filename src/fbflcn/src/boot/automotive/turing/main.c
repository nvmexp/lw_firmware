/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
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
#include "dev_fbfalcon_csb.h"
#include "dev_fbfalcon_pri.h"

#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_interrupts.h"
#include "fbflcn_table.h"
#include "fbflcn_hbm_mclk_switch.h"
#include "fbflcn_ieee1500_gv100.h"
#include "fbflcn_mutex.h"
#include "data.h"
#include "priv.h"
#include "revocation.h"
#include "fbflcn_access.h"

#include "csb.h"


// compile time check for proper compile targets
// automotive target is for GDDR memory only
CASSERT( FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR), boot_automotive_turing_main_c);

// TODO: is there any real use for this revision ?
#define FBFALCON_UCODE_REVISION 0x02000001
const static LwU32 gREV = FBFALCON_UCODE_REVISION;

extern void *__stack_chk_guard;
extern LwU32 _spMin;
extern LwU32 _stack;

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
    FBFLCN_HALT(FBFLCN_ERROR_CODE_STACK_CANARY_CORRUPTED);
}


GCC_ATTRIB_NO_STACK_PROTECT()
int
main
(
        int argc,
        char **Argv
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    REG_WR32(BAR0, LW_CFBFALCON_FALCON_DMEM_PRIV_RANGE0, 0x0000ffff);
#endif

    // Setup SSP canary
    canary_setup();

    // Set SP_MIN
    CSB_REG_WR32(LW_CFBFALCON_FALCON_SP_MIN, DRF_VAL(_CFBFALCON, _FALCON_SP_MIN, _VALUE, (LwU32) &_spMin));

    // init interrupts settings
    //  this should be first before doing any reg access as it covers some of the error handling
    //  covering external accesses.
    FalconInterruptSetup();

    fbfalconCheckRevocation_HAL();


    // post boot confirmation to falcon monitor
    // this is used by uvm/testbench as acknowledgement of bootloader and falcon start.
    LwU32 fbpaRev = REG_RD32(BAR0, LW_PFB_FBPA_FALCON_CTRL_VER);


    FW_MBOX_WR32_STALL(2, gREV);
    FW_MBOX_WR32_STALL(3, fbpaRev);

    fbfalconDetectPartitions_HAL();

    LwU32 initError = initTable();
    if (initError != 0)
    {
        FBFLCN_HALT(initError);
    }

    loadTable();

    fbfalconSelectTable_HAL(&Fbflcn,gBiosTable.nominalFrequencyP0);

//    LwU32 crcBios2 = createCRC((LwU32*)&gVbiosTable[0],CFG_BIOS_TABLE_ALLOCATION);
//    FW_MBOX_WR32_STALL(7, crcBios2);
//    LwU32 crcBiosTable2 = createCRC((LwU32*)&gBiosTable,sizeof(BIOS_TABLE_STRUCT));
//    FW_MBOX_WR32_STALL(8, crcBiosTable2);
//    LwU32 crcGTT2= createCRC((LwU32*)&gTT,sizeof(BIOS_TARGET_TABLE));
//    FW_MBOX_WR32_STALL(9, crcGTT2);

    // start training
    REG_WR32_STALL(BAR0, LW_PFBFALCON_BOOTTIME_TRAINING_STATUS,LW_PFBFALCON_BOOTTIME_TRAINING_STATUS_CODE_TRAINING_STARTED);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    privprofilingEnable();
    privprofilingReset();
#endif

    
    // 1) flag_skip_boot_training  is used to execute or pass a mclk swithc
    if (gTT.bootTrainingTable.MemBootTrainTblFlags.flag_skip_boot_training)
    {
        // when disabled the automotive boot binary will execute nothing and come up in
        // P8 boot setup.
        FW_MBOX_WR32_STALL(1, 0xe0000000);
    }
    else
    {
        LwU32 fbStopTimeNs = 0;
        FW_MBOX_WR32_STALL(6, 0xdd000066);
        fbStopTimeNs = doMclkSwitch(gBiosTable.nominalFrequencyP0);
        FW_MBOX_WR32_STALL(1, fbStopTimeNs);
    }

    // end training
    REG_WR32_STALL(BAR0, LW_PFBFALCON_BOOTTIME_TRAINING_STATUS,LW_PFBFALCON_BOOTTIME_TRAINING_STATUS_CODE_TRAINING_COMPLETED);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    privprofilingDisable();
#endif

    REG_WR32_STALL(BAR0, LW_CFBFALCON_SELWRE_WR_SCRATCH(0),LW_CFBFALCON_SELWRE_WR_SCRATCH_VALUE_MAGIC);

    // lower plm on egine reset (anyone can reset the falcon now)
    // full engine reset was added at the same time that the code base moved to LS and
    // required the reset downgrade on state unload
    LwU32 plm = REG_RD32(BAR0, LW_CFBFALCON_FALCON_RESET_PRIV_LEVEL_MASK);
    plm = FLD_SET_DRF(_CFBFALCON,_FALCON_RESET_PRIV_LEVEL_MASK,_READ_PROTECTION,_ALL_LEVELS_ENABLED,plm);
    plm = FLD_SET_DRF(_CFBFALCON,_FALCON_RESET_PRIV_LEVEL_MASK,_WRITE_PROTECTION,_ALL_LEVELS_ENABLED,plm);
    REG_WR32_STALL(BAR0, LW_CFBFALCON_FALCON_RESET_PRIV_LEVEL_MASK,plm);

    FBFLCN_HALT(FBFLCN_ERROR_CODE_REACHED_END_OF_MAIN);
    return 0;
}

