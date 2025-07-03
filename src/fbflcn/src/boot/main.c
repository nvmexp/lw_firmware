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

#include "osptimer.h"
#include "fbflcn_objfbflcn.h"

#include "dev_fbpa.h"
#include "dev_fbfalcon_csb.h"
#include "dev_fbfalcon_pri.h"
#include "dev_fuse.h"
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
#include "dev_fsp_pri.h"
#endif
#include "rmfbflcncmdif.h"

#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_interrupts.h"
#include "fbflcn_table.h"
#include "fbflcn_hbm_mclk_switch.h"
#include "fbflcn_ieee1500_gv100.h"
#include "fbflcn_mutex.h"
#include "fbflcn_gddr_boot_time_training_tu10x.h"
#include "data.h"
#include "priv.h"
#include "revocation.h"
#include "fbflcn_access.h"
#include "config/fbfalcon-config.h"

#include "csb.h"

#include "config/fbfalcon-config.h"
#include "config/g_memory_hal.h"

extern AON_STATE_INFO gAonStateInfo;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
#include "dev_pafalcon_pri.h"
#endif


// TODO: is there any real use for this revision ?
#define FBFALCON_UCODE_REVISION 0x01000001
const static LwU32 gREV = FBFALCON_UCODE_REVISION;

extern void *__stack_chk_guard;
extern LwU32 _spMin;
extern LwU32 _stack;

LwU8 gPlatformType = 0;
LwU8 gFbflcnLsMode;
LwU8 gPrivSecEn;
extern LW_STATUS initStatus;


static void GCC_ATTRIB_NO_STACK_PROTECT()
canary_setup(void)
{
    // Ideally the canary value is set through a random number generator but the fbfalcon
    // does lwrrently not support SCP nor does it have the private interface for SE
    // This has been filed as bug 2857774 as a potential addition to the next generation.
    // In the meantime we use the timer value
    // using 32 ns timer
    LwU32 randomNumber = CSB_REG_RD32(LW_CFBFALCON_FALCON_PTIMER0);
    LwU32 lowerBits = randomNumber & 0xffff;

    randomNumber = (randomNumber ^ (lowerBits<<16)) | lowerBits;

    __stack_chk_guard = (void*) DRF_VAL(_CFBFALCON, _FALCON_PTIMER0, _VAL,randomNumber);
}

// Overrides libssp definition so falcon dumps some error info
extern void GCC_ATTRIB_NO_STACK_PROTECT()
_wrap____stack_chk_fail(void)
{
    FBFLCN_HALT(FBFLCN_ERROR_CODE_STACK_CANARY_CORRUPTED);
}


/*
void debugCall(LwU32 loc)
{
    FW_MBOX_WR32(6, loc);
    FW_MBOX_WR32(5, 0xbbbbbbbb);
    FW_MBOX_WR32(7, 0xbbbbbbbb);
    while (CSB_REG_RD32(LW_CFBFALCON_FIRMWARE_MAILBOX(7)) != 0xaaaaaaaa)
    {
        OS_PTIMER_SPIN_WAIT_US(500);
        LwU32 addr = CSB_REG_RD32(LW_CFBFALCON_FIRMWARE_MAILBOX(7));
        if (addr < 0x10000) {
            LwU32* p = (LwU32*)(addr);
            FW_MBOX_WR32(8, *(p++));
            FW_MBOX_WR32(9, *(p++));
            FW_MBOX_WR32(10, *(p++));
            FW_MBOX_WR32(11, *(p++));
            FW_MBOX_WR32(12, *(p++));
            FW_MBOX_WR32(13, *(p++));
            FW_MBOX_WR32(14, *(p++));
            FW_MBOX_WR32(15, *(p++));
        }
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        addr = CSB_REG_RD32(LW_CFBFALCON_FIRMWARE_MAILBOX(5));
        if (addr <= 0x10000)
        {
            LwU32 block = addr / 0x100;
            REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMT(0), block);
            REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMC(0),
                    REF_NUM(LW_PPAFALCON_FALCON_DMEMC_OFFS, ((addr % 0x100)/ 4) ) |
                    REF_NUM(LW_PPAFALCON_FALCON_DMEMC_BLK, block) |
                    REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCW, 0) |
                    REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCR, 1)
            );

            FW_MBOX_WR32(8, REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0)));
            FW_MBOX_WR32(9, REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0)));
            FW_MBOX_WR32(10, REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0)));
            FW_MBOX_WR32(11, REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0)));
            FW_MBOX_WR32(12, REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0)));
            FW_MBOX_WR32(13, REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0)));
            FW_MBOX_WR32(14, REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0)));
            FW_MBOX_WR32(15, REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0)));
        }
#endif
    }
}
*/

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
    CSB_REG_WR32(LW_CFBFALCON_FALCON_SP_MIN, DRF_VAL(_CFBFALCON, _FALCON_SP_MIN, _VALUE, (LwU32) &_spMin));

    // init interrupts settings
    //  this should be first before doing any reg access as it covers some of the error handling
    //  covering external accesses.
    FalconInterruptSetup();

    fbfalconCheckFusing_HAL();

    // retain security level
    LwU32 fbflcnSctl = REG_RD32(CSB, LW_CFBFALCON_FALCON_SCTL);
    gFbflcnLsMode = DRF_VAL ( _CFBFALCON, _FALCON_SCTL, _LSMODE, fbflcnSctl );

#if (!(FBFALCONCFG_FEATURE_ENABLED(VBIOS_INCLUDED_IN_BINARY)))
    gPrivSecEn = DRF_VAL(_FUSE, _OPT_PRIV_SEC_EN, _DATA, REG_RD32(BAR0, LW_FUSE_OPT_PRIV_SEC_EN));
#else
    gPrivSecEn = 0;     // disable for ATE
#endif

    fbfalconDetectPartitions_HAL();

    FW_MBOX_WR32_STALL(2, gREV);
    if (localEnabledFBPAMask != 0)
    {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        // load sw config and set control flags
        memoryReadSwConfig_HAL();
#endif

        // post boot confirmation to falcon monitor
        // this is used by uvm/testbench as acknowledgement of bootloader and falcon start.
        LwU32 fbpaRev = REG_RD32(BAR0, LW_PFB_FBPA_FALCON_CTRL_VER);
        FW_MBOX_WR32_STALL(3, fbpaRev);
    }

    fbfalconCheckRevocation_HAL();

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
    // clear status register in FSP (added status to register on target side to avoid polling) Bug 200755547
    REG_WR32_STALL(BAR0, LW_PFSP_FALCON_COMMON_SCRATCH_GROUP_1(0), 0x00);
#endif

    memoryInitHbmChannelConfig_HAL();

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    // check that secure scratch register is accesible
    checkGroupPlm(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13_PRIV_LEVEL_MASK);

    // Set control flags.  This should only be done in the boottime training binary
    // as the information will be persistent in the aon scratch register until the
    // next cold boot.
    gAonStateInfo.trainingCompleted = 0;
    gAonStateInfo.unused = 0;
    gAonStateInfo.dataInSystemMemory = 0;
    gAonStateInfo.dataInPaFalconMemory = 0;
    gAonStateInfo.debugDataBlks = 0;
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
    //
    // prepare the pafalcon for storage, reset and let it scrub its dmem/imem
    //
    REG_WR32_STALL(BAR0, LW_PPAFALCON_FALCON_ENGINE, LW_PPAFALCON_FALCON_ENGINE_RESET_TRUE);
    REG_WR32_STALL(BAR0, LW_PPAFALCON_FALCON_ENGINE, LW_PPAFALCON_FALCON_ENGINE_RESET_FALSE);

#endif

    initStatus = initTable();
    if (initStatus == LW_OK)
    {
        initStatus = loadTable();        // copy bios table to dmem
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#if (!(FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
        LoadTrainingPatram();
#else
        LoadDataTrainingPatram(0);
        LoadDataTrainingPatram(1);
#endif
#endif
        fbfalconSelectTable_HAL(&Fbflcn, gBiosTable.nominalFrequencyP0);
    }

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
    // finish setup of pa falcon
    waitForPAFalconScrubbing();
#endif

    // start training
    REG_WR32_STALL(BAR0, LW_PFBFALCON_BOOTTIME_TRAINING_STATUS,LW_PFBFALCON_BOOTTIME_TRAINING_STATUS_CODE_TRAINING_STARTED);

    LwU32 BootTrainTableFlags;
    BootTrainTableFlags = TABLE_VAL(gBiosTable.pBootTrainingTable->MemBootTrainTblFlags);
    BOOT_TRAINING_FLAGS *pTrainingFlags = (BOOT_TRAINING_FLAGS*) &BootTrainTableFlags;

    LwU8 skipBootTraining = pTrainingFlags->flag_skip_boot_training;

    if(!skipBootTraining) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
        privprofilingEnable();
        privprofilingReset();
#endif

        //FLCN_TIMESTAMP bootTrTimeNs ={0};
        //osPTimerTimeNsLwrrentGet(&bootTrTimeNs);
        LwU32 result  __attribute__((unused));
#if ((!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)) || (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100)))
        result = doBootTimeTraining();
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        gAonStateInfo.trainingCompleted = 1;
#endif
        //LwU32 bootTrElapsedTime = osPTimerTimeNsElapsedGet(&bootTrTimeNs);
        //FW_MBOX_WR32(3, bootTrElapsedTime);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
        privprofilingDisable();
#endif
    } 

    //debugCall(0x1);

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
    // training debug data is saved during the training runs
    // set control flags:
    gAonStateInfo.dataInPaFalconMemory = 1;
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    // write gAonStateInfo to scratch register
    writeAon13ScratchInfo();
    //debugCall(0x7);
#endif

    // training code completed.
    REG_WR32_STALL(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(0) ,LW_PFBFALCON_BOOTTIME_TRAINING_STATUS_CODE_TRAINING_COMPLETED);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
    // Update status register in FSP (added status to register on target side to avoid polling) Bug 200755547
    REG_WR32_STALL(BAR0, LW_PFSP_FALCON_COMMON_SCRATCH_GROUP_1(0), 0xFF);
#endif

    REG_WR32_STALL(CSB, LW_CFBFALCON_SELWRE_WR_SCRATCH(0),LW_CFBFALCON_SELWRE_WR_SCRATCH_VALUE_MAGIC);

    // lower plm on egine reset (anyone can reset the falcon now)
    // full engine reset was added at the same time that the code base moved to LS and
    // required the reset downgrade on state unload
    LwU32 plm = REG_RD32(CSB, LW_CFBFALCON_FALCON_RESET_PRIV_LEVEL_MASK);
    plm = FLD_SET_DRF(_CFBFALCON,_FALCON_RESET_PRIV_LEVEL_MASK,_READ_PROTECTION,_ALL_LEVELS_ENABLED,plm);
    plm = FLD_SET_DRF(_CFBFALCON,_FALCON_RESET_PRIV_LEVEL_MASK,_WRITE_PROTECTION,_ALL_LEVELS_ENABLED,plm);
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_RESET_PRIV_LEVEL_MASK,plm);

    // As part of bug 2441530, the end of boot time training is using the propgagated interrupt from the
    // fbfalcon halt.  There is no need to mask the halt interrutps and main simply exits with the halt
    // in the start code.
    return 0;

}

