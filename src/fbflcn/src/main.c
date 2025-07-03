/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
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
#include "cpu-intrinsics.h"
#include "osptimer.h"
#include "fbflcn_objfbflcn.h"

#include "dev_fbpa.h"
#include "dev_fbfalcon_csb.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"
#include "dev_top.h"
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
#include "dev_fsp_pri.h"
#endif
#include "rmfbflcncmdif.h"

#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_interrupts.h"
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT))
#include "fbflcn_queues.h"
#endif  // FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT)
#include "fbflcn_table.h"
#include "fbflcn_hbm_mclk_switch.h"
#include "fbflcn_gddr_mclk_switch.h"
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TEMP_TRACKING))
#include "temp_tracking.h"
#include "dev_trim.h"
#else
#include "fbflcn_ieee1500_gv100.h"
#endif
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_HBM_PERIODIC_TR))
#include "fbflcn_hbm_periodic_training.h"
#endif
#include "fbflcn_mutex.h"
#include "data.h"
#include "priv.h"
#include "revocation.h"
#include "memory.h"


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TRAINING_DATA_SUPPORT))
#include "memory_gddr.h"
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
#include "dev_pafalcon_pri.h"
#include "seq_decoder.h"
#include "segment.h"
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))
#include "fbflcn_hbm_boot_time_training_gh100.h"
#endif

#include "csb.h"
#include "lwuproc.h"

#include "config/fbfalcon-config.h"
#include "config/g_memory_hal.h"
#include "config/g_fbfalcon_hal.h"


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
extern AON_STATE_INFO gAonStateInfo;
#endif
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT))
// access to queue resources
extern FBFLCN_QUEUE_RESOURCE gQueueResource[FBFLCN_CMD_QUEUE_NUM];


#endif  // FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT)

// TODO: is there any real use for this revision ?
#define FBFALCON_UCODE_REVISION 0x01000001
const static LwU32 gREV = FBFALCON_UCODE_REVISION;

OBJFBFLCN Fbflcn;
LwU8 gPlatformType = 0;
LwU8 gFbflcnLsMode;
LwU8 gPrivSecEn;
extern LwU32 gSwConfig;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
extern LwU32 gDB[DB_WORD_SIZE];
#endif

extern void *__stack_chk_guard;
extern LwU32 _spMin;
extern LwU32 _stack;

// crosscheck on define configurations to filter illegal setups
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))
 #if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
  Sorry, this does not make sense a single binary does not have to save
  data to PAFalcon to transfer.
 #endif

 #if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
  Sorry, this could work but needs a review of the sequence. Lwrrently the PAFAlcon
  sequence also implies segment bootloading which can not be done in an unified binary
  As its lwrrently not a use case i prefer to disable it until its part of a POR
 #endif

 #if (!(FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
  Sorry, Single binary only supported for HBM
 #endif
#endif



// initStatus indicates status of bios loading, used to track any errors during table loadings
// but does not stop exelwtion. Its blocking an mclk switch request from being exelwted.
extern LW_STATUS initStatus;

static void GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_SECTION("resident", "canary_setup")
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
extern void GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_SECTION("resident", "_wrap____stack_chk_fail")
_wrap____stack_chk_fail(void)
{
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


//
// init code for fbfalcon
//  this should contain various setups that are use at init and provides an option to free resident
//  program space by removing the code from imem once the initialization has finished.
GCC_ATTRIB_SECTION("init", "initRegs")
void
initRegs
(
        void
)
{
    // post boot confirmation to falcon monitor
    // this is used by uvm/testbench as acknowledgement of bootloader and falcon start.
    FW_MBOX_WR32_STALL(2, gREV);
    FW_MBOX_WR32_STALL(3, gREV);
    if (localEnabledFBPAMask != 0)
    {
        LwU32 fbpaRev = REG_RD32(BAR0, LW_PFB_FBPA_FALCON_CTRL_VER);
        FW_MBOX_WR32_STALL(3, fbpaRev);
    }
    else
    {
        // #partitions == 0, in 0FB mode, disable timer based procedures
        // fall back to the same mechanism used for table loading, using
        // initStatus to complete initialization but block any further tasks
        // from exelwting
        initStatus = FBFLCN_ERROR_CODE_0FB_SETUP_ERROR;
    }


    // turn on power gating
    //   issue with tsync for system timer reads on the synchronizer randomization
    //   forced us to disable the tsync power gating on rtl, see bug: 3357434
    //
    LwU32 cg2_slcg = LW_CFBFALCON_FALCON_CG2_SLCG_ENABLED;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))
    cg2_slcg = FLD_SET_DRF(_CFBFALCON_FALCON,_CG2_SLCG,_FALCON_TSYNC, _DISABLED, cg2_slcg);
#endif // (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))
    REG_WR32(CSB,LW_CFBFALCON_FALCON_CG2, cg2_slcg);
}


//
// initFromInitSection
//  includes all initialization code located in the init section. This code will be removed
//  to load the mclk code base.
GCC_ATTRIB_SECTION("init", "initFromInitSection")
void
initFromInitSection
(
        void
)
{

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))

    // remove bootloader by ilwalidating its tag
    // this is required as the fbfalcon code contains instructions at the same physical address (0xfe00)
    // as the bootloader and would lead to 2 tags for the same address
    falc_imilw(254);
    falc_imilw(255);
#endif

    // init interrupts settings
    FalconInterruptSetup();

    fbfalconCheckFusing_HAL();

    // detect memory configs
    fbfalconDetectPartitions_HAL();

    // load sw config and set control flags
    // can't read FBPA regs when 0FB (partitions==0) used
    if (localEnabledFBPAMask != 0)
    {
        memoryReadSwConfig_HAL();
    }

    // detect HBM memory configs & setup
    memoryInitHbmChannelConfig_HAL();

    // detect HBM3 device id
    memoryReadI1500DeviceId_HAL();

    // retain security level
    LwU32 fbflcnSctl = REG_RD32(CSB, LW_CFBFALCON_FALCON_SCTL);
    gFbflcnLsMode = DRF_VAL ( _CFBFALCON, _FALCON_SCTL, _LSMODE, fbflcnSctl );

#if (!(FBFALCONCFG_FEATURE_ENABLED(VBIOS_INCLUDED_IN_BINARY)))
    gPrivSecEn = DRF_VAL(_FUSE, _OPT_PRIV_SEC_EN, _DATA, REG_RD32(BAR0, LW_FUSE_OPT_PRIV_SEC_EN));
#else
    gPrivSecEn = 0;     // disable for ATE
#endif

    // Security level checks
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING)))
    // check for LS mode when ACR is enabled, this will not be necessary
    // when run straight from the bios
    fbfalcolwerifyFalconSelwreRunMode_HAL();
#endif

    // check revocation
    fbfalconCheckRevocation_HAL();


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_BUG_3088142_DRAM_BW_55PCT))
    REG_WR32(BAR0,LW_PFB_FBPA_BW_TRACKER_VC1,0x8CD);
#endif

// check for access right to gc6 island scratch register group 12/13
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
    checkGroupPlm(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12_PRIV_LEVEL_MASK);
    checkGroupPlm(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13_PRIV_LEVEL_MASK);
    readAonStateInfo();
#endif
#endif

    //
    // read back debug data from pafalcon from boottime training
    //
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
    // the pafalcon is not available in pafalcon and gAonStateInfo is initialized as part
    // of the boottime training which is missing in fmodel runs as well, hence we explicitely
    // disable access to PA falcon memory when running in fmodel.
    if ((gAonStateInfo.dataInPaFalconMemory == 1) && (gPlatformType != LW_PTOP_PLATFORM_TYPE_FMODEL))
    {
        dmemReadPAFalcon((LwU32*)&gDB[0], (gAonStateInfo.debugDataBlks * 0x100));
    }
#endif

    //
    // Reset PA Falcon
    //  do this early to allow time for the automatic imem/dmem scrubbing
    //
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    extern LwBool gbl_pa_bdr_load;     // surpport by unit sim environment for backdoor loading
    extern LwBool gbl_en_fb_mclk_sw;

    if ((gPlatformType == LW_PTOP_PLATFORM_TYPE_FMODEL) || (gbl_pa_bdr_load)) {
        // ... skip pafalcon access
    } else {
        // reset the pa falcon
        REG_WR32_STALL(BAR0, LW_PPAFALCON_FALCON_ENGINE, LW_PPAFALCON_FALCON_ENGINE_RESET_TRUE);
        REG_WR32_STALL(BAR0, LW_PPAFALCON_FALCON_ENGINE, LW_PPAFALCON_FALCON_ENGINE_RESET_FALSE);

        // Trigger the PA Falcon to do mem scrubbing, this should happen as early as possible
        // to run in parallel w/ the setup process on the fbfalcon.
        // REG_RD32(BAR0, LW_PPAFALCON_FALCON_DMACTL);
    }

#endif

    //
    // Bootload PA Falcon
    //
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if ((gPlatformType == LW_PTOP_PLATFORM_TYPE_FMODEL) || (gbl_pa_bdr_load)) {
        // ... skip pafalcon access
    } else {

        if (gbl_en_fb_mclk_sw == LW_FALSE)
        {
            //
            // Bootload the PA Falcon
            //

            // make sure PA falcon has completed mem scrubbing
            waitForPAFalconScrubbing();

            // Set PA Falcon to LS mode if the fbfalcon is in LS mode and set associted registers
            LwU32 fbflcnSctl = REG_RD32(CSB, LW_CFBFALCON_FALCON_SCTL);
            LwU8 fbflcnLsMode = DRF_VAL ( _CFBFALCON, _FALCON_SCTL, _LSMODE, fbflcnSctl );
            if (fbflcnLsMode == LW_CFBFALCON_FALCON_SCTL_LSMODE_TRUE)
            {
                // ls falcon bootstrap sequence applied to the PA Falcon
                // security requests dolwmented in https://confluence.lwpu.com/display/~suppal/LS+Falcon+Bootstrap
                static LwU32 ACR_WRITE_L2 =  0xc;

                // SCTL PRIV_LEVEL_MASK
                LwU32 plm_val;
                plm_val = REG_RD32(BAR0,LW_PPAFALCON_FALCON_SCTL_PRIV_LEVEL_MASK);
                plm_val = FLD_SET_DRF(_PPAFALCON_FALCON,_SCTL_PRIV_LEVEL_MASK,_READ_PROTECTION, _ALL_LEVELS_ENABLED , plm_val);
                plm_val = FLD_SET_DRF_NUM(_PPAFALCON_FALCON,_SCTL_PRIV_LEVEL_MASK,_WRITE_PROTECTION, ACR_WRITE_L2 , plm_val);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_SCTL_PRIV_LEVEL_MASK, plm_val);

                // SCTL Register Values
                //   SCTL_RESET_LVLM_EN => FALSE
                //   SCTL_STALLREQ_CLR_EN => FALSE
                //   SCTL_AUTH_EN => TRUE
                LwU32 paflcnSctl = 0;
                paflcnSctl = FLD_SET_DRF(_PPAFALCON_FALCON,_SCTL,_AUTH_EN,_TRUE, paflcnSctl);
                paflcnSctl = FLD_SET_DRF(_PPAFALCON_FALCON,_SCTL,_RESET_LVLM_EN,_FALSE, paflcnSctl);
                paflcnSctl = FLD_SET_DRF(_PPAFALCON_FALCON,_SCTL,_STALLREQ_CLR_EN,_FALSE, paflcnSctl);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_SCTL,paflcnSctl);

                // CPUCTL_ALIAS_EN -> FALSE
                LwU32 cpuctl_val = 0;
                cpuctl_val = FLD_SET_DRF(_PPAFALCON_FALCON,_CPUCTL,_ALIAS_EN,_FALSE,cpuctl_val);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_CPUCTL,cpuctl_val);

                // IMEM_PRIV_LEVEL_MASK
                plm_val = REG_RD32(BAR0, LW_PPAFALCON_FALCON_IMEM_PRIV_LEVEL_MASK);
                plm_val = FLD_SET_DRF(_PPAFALCON_FALCON,_IMEM_PRIV_LEVEL_MASK,_READ_PROTECTION, _ALL_LEVELS_ENABLED , plm_val);
                plm_val = FLD_SET_DRF_NUM(_PPAFALCON_FALCON,_IMEM_PRIV_LEVEL_MASK,_WRITE_PROTECTION, ACR_WRITE_L2 , plm_val);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_IMEM_PRIV_LEVEL_MASK, plm_val);

                // DMEM_PRIV_LEVEL_MASK
                plm_val = REG_RD32(BAR0, LW_PPAFALCON_FALCON_DMEM_PRIV_LEVEL_MASK);
                plm_val = FLD_SET_DRF(_PPAFALCON_FALCON,_DMEM_PRIV_LEVEL_MASK,_READ_PROTECTION, _ALL_LEVELS_ENABLED , plm_val);
                plm_val = FLD_SET_DRF_NUM(_PPAFALCON_FALCON,_DMEM_PRIV_LEVEL_MASK,_WRITE_PROTECTION, ACR_WRITE_L2 , plm_val);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_DMEM_PRIV_LEVEL_MASK, plm_val);

                // CPUCTL_PRIV_LEVEL_MASK
                plm_val = REG_RD32(BAR0, LW_PPAFALCON_FALCON_CPUCTL_PRIV_LEVEL_MASK);
                plm_val = FLD_SET_DRF(_PPAFALCON_FALCON,_CPUCTL_PRIV_LEVEL_MASK,_READ_PROTECTION, _ALL_LEVELS_ENABLED , plm_val);
                plm_val = FLD_SET_DRF_NUM(_PPAFALCON_FALCON,_CPUCTL_PRIV_LEVEL_MASK,_WRITE_PROTECTION, ACR_WRITE_L2 , plm_val);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_CPUCTL_PRIV_LEVEL_MASK, plm_val);

                // DEBUG_PRIV_LEVEL_MASK
                //  DEBUG1 is covered by mthdctx priv level mask
                // EXE_PRIV_LEVEL_MASK
                plm_val = REG_RD32(BAR0, LW_PPAFALCON_FALCON_EXE_PRIV_LEVEL_MASK);
                plm_val = FLD_SET_DRF(_PPAFALCON_FALCON,_EXE_PRIV_LEVEL_MASK,_READ_PROTECTION, _ALL_LEVELS_ENABLED , plm_val);
                plm_val = FLD_SET_DRF_NUM(_PPAFALCON_FALCON,_EXE_PRIV_LEVEL_MASK,_WRITE_PROTECTION, ACR_WRITE_L2 , plm_val);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_EXE_PRIV_LEVEL_MASK, plm_val);

                // IRQTM_PRIV_LEVEL_MASK
                plm_val = REG_RD32(BAR0, LW_PPAFALCON_FALCON_IRQTMR_PRIV_LEVEL_MASK);
                plm_val = FLD_SET_DRF(_PPAFALCON_FALCON,_IRQTMR_PRIV_LEVEL_MASK,_READ_PROTECTION, _ALL_LEVELS_ENABLED , plm_val);
                plm_val = FLD_SET_DRF_NUM(_PPAFALCON_FALCON,_IRQTMR_PRIV_LEVEL_MASK,_WRITE_PROTECTION, ACR_WRITE_L2 , plm_val);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_IRQTMR_PRIV_LEVEL_MASK, plm_val);

                // MTHDCTX_PRIV_LEVEL_MASK
                plm_val = REG_RD32(BAR0, LW_PPAFALCON_FALCON_MTHDCTX_PRIV_LEVEL_MASK);
                plm_val = FLD_SET_DRF(_PPAFALCON_FALCON,_MTHDCTX_PRIV_LEVEL_MASK,_READ_PROTECTION, _ALL_LEVELS_ENABLED , plm_val);
                plm_val = FLD_SET_DRF_NUM(_PPAFALCON_FALCON,_MTHDCTX_PRIV_LEVEL_MASK,_WRITE_PROTECTION, ACR_WRITE_L2 , plm_val);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_MTHDCTX_PRIV_LEVEL_MASK, plm_val);

                // FBIF_REGIONCFG_PRIV_LEVEL_MASK
                //  pafalcon does not have and fbif implementation
                //plm_val = REG_RD32(BAR0, LW_PPAFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK);
                //plm_val = (((plm_val >> 8) << 8) | (ACR_READ_L0_WRITE_L2 & 0xFF));
                //REG_WR32(BAR0, LW_PPAFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK, plm_val);

                // check if falcon is halted
                LwU32 cpuctl = REG_RD32(BAR0, LW_PPAFALCON_FALCON_CPUCTL);
                if (REF_VAL(LW_PPAFALCON_FALCON_CPUCTL_HALTED, cpuctl) == LW_PPAFALCON_FALCON_CPUCTL_HALTED_FALSE)
                {
                    // From our boot seqeuence this should never happen, if the falcon is running
                    // already the probably somebody is tampering with it. For now just halt
                    FBFLCN_HALT(FBFLCN_ERROR_CODE_CORE_RUNNING_AT_INIT);
                };


                // Program LS Mode
                paflcnSctl = REG_RD32(BAR0, LW_PPAFALCON_FALCON_SCTL);
                paflcnSctl = FLD_SET_DRF(_PPAFALCON_FALCON,_SCTL,_AUTH_EN,_TRUE, paflcnSctl);
                paflcnSctl = FLD_SET_DRF(_PPAFALCON_FALCON,_SCTL,_RESET_LVLM_EN,_TRUE, paflcnSctl);
                paflcnSctl = FLD_SET_DRF(_PPAFALCON_FALCON,_SCTL,_STALLREQ_CLR_EN,_TRUE, paflcnSctl);
                paflcnSctl = FLD_SET_DRF(_PPAFALCON_FALCON,_SCTL,_LSMODE,_TRUE, paflcnSctl);
                paflcnSctl = FLD_SET_DRF_NUM(_PPAFALCON_FALCON,_SCTL,_LSMODE_LEVEL,0x2, paflcnSctl);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_SCTL,paflcnSctl);

                // check if SCTL_AUTH_EN is still true
                LwU32 paflcnSctlCheck = 0;
                paflcnSctlCheck = REG_RD32(BAR0, LW_PPAFALCON_FALCON_SCTL);
                if (REF_VAL(LW_PPAFALCON_FALCON_SCTL_AUTH_EN,paflcnSctlCheck) != LW_PPAFALCON_FALCON_SCTL_AUTH_EN_TRUE)
                {
                    FBFLCN_HALT(FBFLCN_ERROR_CODE_CORE_AUTH_EN_VIOLATION);
                }

                // Set CPUCTL_ALIAS_EN to TRUE
                cpuctl = REG_RD32(BAR0,LW_PPAFALCON_FALCON_CPUCTL);
                cpuctl = FLD_SET_DRF(_PPAFALCON_FALCON,_CPUCTL,_ALIAS_EN,_TRUE,cpuctl);
                REG_WR32(BAR0, LW_PPAFALCON_FALCON_CPUCTL,cpuctl);

            }

            // bootload the PA falcon through priv
            bootloadPAFalcon();


            // start the PA falcon
            startPaFalcon();

            // verify PA Falcons are up with a quick sync check
            verifyPaFalcon();

        }
        else
        {
            // priv register setup on pafalcon when its not in use

        }
        // general priv setup on the pa falcon
	
        // turn on power gating
        //   issue with tsync for system timer reads on the synchronizer randomization
        //   forced us to disable the tsync power gating on rtl, see bug: 3357434
        //
        LwU32 cg2_slcg = LW_PPAFALCON_FALCON_CG2_SLCG_ENABLED;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))
        cg2_slcg = FLD_SET_DRF(_PPAFALCON_FALCON,_CG2_SLCG,_FALCON_TSYNC, _DISABLED, cg2_slcg);
#endif  // (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))

        REG_WR32(BAR0,LW_PPAFALCON_FALCON_CG2, cg2_slcg );
    }
#endif


#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
    dataPrune();
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
    if (gAonStateInfo.dataInPaFalconMemory == 1)
    {
        copyDebugBuffer();
    }
#endif
#endif

    initQueueResources();

    initStatus = initTable();
    if (initStatus == LW_OK)
    {
        initStatus = loadTable();
    }
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if (initStatus == LW_OK)
    {
        if (gPlatformType == LW_PTOP_PLATFORM_TYPE_EMU) {
            sanityCheckMemTweakTable();
        }
    }
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT))

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    clearEvent();
#endif

#endif  // FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT)


#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
    // Fix for bug 200625661 related to gc6 exit we see stalls on the priv interface
    // that triggered the timeout of the bar0 master (most likely due to the big
    // amount of posted writes) it does not trigger priv timeout suggesting the
    // bar0 timeout was significantly smaller than the priv timeout.
    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_TIMEOUT, 0x07FFFF);
    // restore regsiter at gc6 exit
    if( FLD_TEST_REF( LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT, _PENDING,
            REG_RD32( BAR0, LW_PGC6_SCI_SW_INTR0_STATUS_LWRR) ) )
    {
        restoreFromSysMem();
        // suppress all 1st mclk switch  based training mechanisms after a register
        // restore
        GblAddrTrPerformedOnce = LW_TRUE;
        GblVrefTrackPerformedOnce = LW_TRUE;
        gSaveRequiredAfterFirstMclkSwitch = LW_FALSE;
    }

#else

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TRAINING_DATA_SUPPORT))
    //
    // UP TO TURING
    //  the training data is saved in the WPR of the falcon binary
    //
    if (initStatus == LW_OK)
    {
        LwU8 skipBootTraining = gTT.bootTrainingTable.MemBootTrainTblFlags.flag_skip_boot_training;

        setupRegionCfgForDataWpr();


        // boottime data loading should happen after sucesful table loading as it depends
        // on enable flags from the boottraining table (though at the moment these flags
        // are entirely contained in

        // if boot time training wsa run we save values right at boot and driver reload
        // and restore right at gc6
        if (skipBootTraining == 0)
        {
            if (isTrainingDataAreaEmpty())
            {
                // read back from registers into memory & save to wpr
                restoreData();
                // save to wpr, this is failing we redo training on the first mclk switch
                if (dataSave())
                {
                    GblAddrTrPerformedOnce = LW_TRUE;
                    GblVrefTrackPerformedOnce = LW_TRUE;
                }
            }
            else
            {
                // write data back to registers
                restoreRegister();
                // read back all the trained values from the memory, suppress training step in first mclk switch
                GblAddrTrPerformedOnce = LW_TRUE;
                GblVrefTrackPerformedOnce = LW_TRUE;
            }

        }

        // without boot time training we only restore at boot, and defer save off to an event requester which
        // is typically the first mclk switch
        if (skipBootTraining == 1)
        {
            // In all cases restore data if available in wpr.  This is the restore path for GC6
            // or any scenaries where the fbfalcon reboots but the wpr stayed allive
            if (!isTrainingDataAreaEmpty())
            {
                // write data back to registers
                restoreRegister();
                GblAddrTrPerformedOnce = LW_TRUE;
                GblVrefTrackPerformedOnce = LW_TRUE;
            }
        }
    }
#endif

#endif

#if ((FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM)) || (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TRAINING_DATA_SUPPORT)))
    // Settle time for restored training values.
    // This was a possible workaround for a high rate of priv timeout failures in bug 200470194
    // Separating the restore which puts lots of request on the priv system from further exelwtion
    // downstream supposedly takes some of that pressure off.  We don't know if this is part of the
    // problem or how much it reduces it  (alone its not a complete fix and does not remove all
    // priv timeouts seen in the fbfalcon)
    OS_PTIMER_SPIN_WAIT_US(INITIAL_TRAINING_RESTORE_DELAY_US);
#endif

    // setup power savings etc.
    initRegs();
}


GCC_ATTRIB_NO_STACK_PROTECT() GCC_ATTRIB_SECTION("resident", "main")
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

    //
    // Code that requires the .init section to be loaded
    //
    initFromInitSection();

    //
    // Replace .init section with .memory section
    //
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))

    // load the mclk code segment
    loadSegment(MCLK_SEGMENT);

#endif

    /*--------------------------------------------------------------------------------
     * Engage bootime training in a single binary use case
     */
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))

    // select table for P0
    fbfalconSelectTable_HAL(&Fbflcn, gBiosTable.nominalFrequencyP0);

    // start training
    REG_WR32_STALL(CSB, LW_CFBFALCON_BOOTTIME_TRAINING_STATUS, LW_CFBFALCON_BOOTTIME_TRAINING_STATUS_CODE_TRAINING_STARTED);

    // look for boottime training flag
    LwU8 skipBootTraining = gBiosTable.pBootTrainingTable->MemBootTrainTblFlags.flag_skip_boot_training;

    if(!skipBootTraining) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
        privprofilingEnable();
        privprofilingReset();
#endif

        FLCN_TIMESTAMP bootTrTimeNs ={0};
        osPTimerTimeNsLwrrentGet(&bootTrTimeNs);

        LwU32 result  __attribute__((unused));
        // debug
		FW_MBOX_WR32_STALL(11, gDisabledFBPAMask);
		FW_MBOX_WR32_STALL(12, gNumberOfPartitions);
		FW_MBOX_WR32_STALL(13, localEnabledFBPAMask);
        result = doBootTimeTraining();

#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
        gAonStateInfo.trainingCompleted = 1;
#endif

        LwU32 bootTrElapsedTime = osPTimerTimeNsElapsedGet(&bootTrTimeNs);
        FW_MBOX_WR32(14, bootTrElapsedTime);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
        privprofilingDisable();
#endif
    }
    // training code completed.
    REG_WR32_STALL(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(0) ,LW_CFBFALCON_BOOTTIME_TRAINING_STATUS_CODE_TRAINING_COMPLETED);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
    // Update status register in FSP (added status to register on target side to avoid polling) Bug 200755547
    REG_WR32_STALL(BAR0, LW_PFSP_FALCON_COMMON_SCRATCH_GROUP_1(0), 0xFF);
#endif

    REG_WR32_STALL(CSB, LW_CFBFALCON_SELWRE_WR_SCRATCH(0),LW_CFBFALCON_SELWRE_WR_SCRATCH_VALUE_MAGIC);

    // for unified binary in sim to catch mclk_switch after boot time training
    fbflcnQInitDirect();
#endif
    /*
     * End of Boottime training in single binary
     *---------------------------------------------------------------------------------------------------------------*/

    //
    // Code that requires the memory section to be loaded
    //
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TEMP_TRACKING))
    if (initStatus == LW_OK)
    {
        extern void setPrevDramclkMode(void);
        extern void gddr_retrieve_lwrrent_freq(void);

        extern LwU16 gbl_lwrrent_freqMHz;
        extern LwU32 gbl_lwrrent_freqKHz;

        LwU32 freqMHz;

        gbl_lwrrent_freqKHz = gbl_lwrrent_freqMHz*1000;
        setPrevDramclkMode();
        gddr_retrieve_lwrrent_freq();
        freqMHz = gbl_lwrrent_freqMHz;

        fbfalconSelectTable_HAL(&Fbflcn,freqMHz);

        initTempTracking();

        LwU32 err = startTempTracking();
        if (err != 0)
        {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_START_TEMP_TRACKING);
        }

    }
#endif
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TREFI_HBM))
    if (initStatus == LW_OK)
    {
        // detect boot setup and position to the current table in use
        LwU32 freqMHz = detectPLLFrequencyMHz();
        fbfalconSelectTable_HAL(&Fbflcn,freqMHz);

        initTempTracking();

        LwU32 err = startTempTracking();
        if (err != 0)
        {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_START_TEMP_TRACKING);
        }
    }
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_HBM_PERIODIC_TR))
    if (initStatus == LW_OK)
    {
        // start
        initPeriodicTraining();
    }
#endif

    // test canary with a forced stack corruption
    //functionWithSSPEnabledToCorruptStack();

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    // clear handoff registers for save and restore buffer
    FW_MBOX_WR32(2, 0x0);
    FW_MBOX_WR32(3, 0x0);
    FW_MBOX_WR32(4, 0x0);
    FW_MBOX_WR32(5, 0x0);
#endif
#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
    // Tnitialize content of sha register storage to match values in the falcon binary.
    // This creates a consistent environment even in verif cases where its possible that
    // the mclk switch is not run at boot or before entering GC6.
    // The conflict comes from the fact that the sha value for a buffer of all zeros at
    // reset is actually not zero, so the reset values on the sha register is incorrect
    // if used directly. 
    //
    // only do at cold boot but not from a GC6 exit
    if ( !FLD_TEST_REF( LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT, _PENDING,
            REG_RD32( BAR0, LW_PGC6_SCI_SW_INTR0_STATUS_LWRR)))
    {
    	storeShaHash();
    }
#endif

// all other arch besides gh100 lwrrently
#if (!FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING))
    fbflcnQInitDirect();
#endif

#if (!((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X) && (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR)))))
    clearEvent();
#endif
    LwU8 status = 1;

    // flag to guarantee that initial setup has completed before we execute the first mclk switch
    // this is required only in the context of using sysmem to load in data at boot.
    LwBool setupBeforeFirstMclkSwitchDone = LW_TRUE;
#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
    setupBeforeFirstMclkSwitchDone = LW_FALSE;
#endif

    while(status == 1)
    {

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        if (testIntEvent(~EVENT_MASK_CLEAR))
        {
            joinIntEvent();     // joins gIntEventMask with gEventMask and clears gIntEventMask
        }
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
        //  wait for system memory setup up to the first mclk switch
        if (!setupBeforeFirstMclkSwitchDone)
        {
            setupBeforeFirstMclkSwitchDone = setupGC6IslandRegisters();

            // init system memory buffer in configs that don't train
            if (setupBeforeFirstMclkSwitchDone)
            {
                // only do this in verif elwironments
                if ((gPlatformType == LW_PTOP_PLATFORM_TYPE_FMODEL) || (gPlatformType == LW_PTOP_PLATFORM_TYPE_EMU))
                {
                    // this is not an exit from GC6
                    if ( !FLD_TEST_REF( LW_PGC6_SCI_SW_INTR0_STATUS_LWRR_GC6_EXIT, _PENDING,
                            REG_RD32( BAR0, LW_PGC6_SCI_SW_INTR0_STATUS_LWRR)))
                    {
                        // setup region
                        setupRegionCfgForSysMemBuffer();
                        // fill buffer with some data
                        dataFill();
                        // save data to sysmem and create a valid sha hash.
                        dataSaveToSysmemCarveout();
                    }
                }
            }
        }
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT))
        // EVENT:  CMD_QUEUE0
        //   system has been pinged with a com request on command queue and is waiting for
        //   decode and response
        if (testEvent(EVENT_MASK_CMD_QUEUE0))
        {
            if (setupBeforeFirstMclkSwitchDone)
            {
                // remove event, this needs to happen early as interrupts with a new event
                // could already happen while processing this step
                processCmdQueueDirect(FBFLCN_CMD_QUEUE_RM2FBFLCN);
                disableEvent(EVENT_MASK_CMD_QUEUE0);
            }
#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
            else
            {
                FW_MBOX_WR32(13, FBFLCN_CMD_QUEUE_RM2FBFLCN);
                FBFLCN_HALT(FBFLCN_ERROR_CODE_QUEUE_REQUEST_BEFORE_SYSMEM_ALLOCATION);
            }
#endif  // (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
        }

        else if (testEvent(EVENT_MASK_CMD_QUEUE1))
        {
            if (setupBeforeFirstMclkSwitchDone)
            {
                processCmdQueueDirect(FBFLCN_CMD_QUEUE_PMU2FBFLCN);
                disableEvent(EVENT_MASK_CMD_QUEUE1);
            }
#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
            else
            {
                FW_MBOX_WR32(13, FBFLCN_CMD_QUEUE_PMU2FBFLCN);
                FBFLCN_HALT(FBFLCN_ERROR_CODE_QUEUE_REQUEST_BEFORE_SYSMEM_ALLOCATION);
            }
#endif  // (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))

        }

#if (FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT))
        else if (testEvent(EVENT_MASK_CMD_QUEUE2))
        {
            // check fuse
            processCmdQueue2DirectForHalt(FBFLCN_CMD_QUEUE_PMU2FBFLCN_SECONDARY);
            disableEvent(EVENT_MASK_CMD_QUEUE2);
        }
#endif  // FBFALCONCFG_FEATURE_ENABLED(MINING_LOCK_SUPPORT)

#endif  // FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_CMDQUEUE_SUPPORT)


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_HBM_PERIODIC_TR))
        else if (testEvent(EVENT_MASK_IEEE1500_PERIODIC_TR))
        {
            disableEvent(EVENT_MASK_IEEE1500_PERIODIC_TR);
            runHbmPeriodicTr();
        }
#endif 

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TREFI_HBM))
        else if (testEvent(EVENT_MASK_IEEE1500_TEMP_MEASURE))
        {
            disableEvent(EVENT_MASK_IEEE1500_TEMP_MEASURE);
            if (temperature_read_event() == 0)
            {
                //status = 0;
                FBFLCN_HALT(FBFLCN_ERROR_CODE_TEMPERATURE_READ_FAILED);
            }
        }
#elif (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TEMP_TRACKING))
        else if (testEvent(EVENT_MASK_TEMP_TRACKING))
        {
            disableEvent(EVENT_MASK_TEMP_TRACKING);
            temperature_read_event();
        }
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TRAINING_DATA_SUPPORT))
        else if (testEvent(EVENT_SAVE_TRAINING_DATA))
        {
            disableEvent(EVENT_SAVE_TRAINING_DATA);

            // read back from registers into memory & save to wpr
#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
            saveToSysMem();
#else
            dataSave();
#endif

        }
#endif

        else if (testEvent(EVENT_MASK_TIMER_EVENT))
        {
            disableEvent(EVENT_MASK_TIMER_EVENT);
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TEMP_TRACKING))
            enableEvent(EVENT_MASK_TEMP_TRACKING);
#else
            enableEvent(EVENT_MASK_IEEE1500_TEMP_MEASURE);
#endif
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_HBM_PERIODIC_TR))
            enableEvent(EVENT_MASK_IEEE1500_PERIODIC_TR);
#endif
        }

    }

    // stop coded detected by the unit level environment for now, don't remove
    REG_WR32_STALL(CSB, LW_CFBFALCON_FALCON_MAILBOX0,0x12345678);
    FBFLCN_HALT(FBFLCN_ERROR_CODE_REACHED_END_OF_MAIN);

    return 0;
}

