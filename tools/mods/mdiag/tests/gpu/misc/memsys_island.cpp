/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**************************************************************************************************
 * Test     :   memsys_island.cpp                                                                 *
 * Purpose  :   Test file for various directed tests for MemSys Island feature.                   *
 * Author   :   Vibhor Dodeja (vdodeja@lwpu.com)                                                *
 **************************************************************************************************/

// Include Header
#include "memsys_island.h"

// Include standard library
#include <bitset>

// Standard Includes
#include "mdiag/tests/stdtest.h"

// Helper Macros to colwert macros to string
#define STR_NX(A) #A
#define STR(A) STR_NX(A)

// Macros to get the manual file for the ref chip
#define REF_CHIP ampere/ga102
#define GET_MAN(man) STR(REF_CHIP/man.h)

// Include manuals
#include GET_MAN(dev_graphics_nobundle)        // For LW_PGRAPH register range.
#include GET_MAN(dev_pwr_pri)         // For LW_PPWR_PMU_IDLE_MASK_SUPP_IDX_GR (Idle mask index for GR engine)
#include GET_MAN(dev_therm_vmacro)    // For LW_THERM_ENGINE_GRAPHICS (Index for GR engine)
#include GET_MAN(dev_top)             // For LW_PTOP_DEVICE_INFO2_DEV_TYPE_ENUM_* (Eng IDs required to get their reset IDs)


// Local Defines
#define TEST_NAME "MemSys_Island" 

// Parameter Declaration
extern const ParamDecl memsys_island_params[] = {
    SIMPLE_PARAM("-idle_wake_up", "Trigger wake up from MSI by forcing some unit busy."),
    SIMPLE_PARAM("-pri_ring_old_init", "Use legacy method for Pri Ring initialisation."),
    SIMPLE_PARAM("-speedy", "Check MSI entry-exit sequence."),
    UNSIGNED_PARAM("-idle_mask_0", "First 32 bits for Idle mask. (Default 0x20000f)"),
    UNSIGNED_PARAM("-idle_mask_1", "Middle 32 bits for idle mask. (Default 0x1020)"),
    UNSIGNED_PARAM("-idle_mask_2", "Last 32 bits for idle mask. (Default 0x0)"),
    UNSIGNED_PARAM("-idle_threshold", "Value for idle threshold for LPWR_ENG to trigger entry. (Default 0x1030"),
    UNSIGNED_PARAM("-interpoll_wait", "Duration (in us) to wait between register polls. (Default 1)"),
    UNSIGNED_PARAM("-max_poll", "Max number of reads while polling. (Default 100)"),
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

// Constructor
MemSys_Island::MemSys_Island (ArgReader *params ) :
    LWGpuSingleChannelTest (params ) {
        InfoPrintf("%s: %s: Reading Parameters...\n", TEST_NAME, __FUNCTION__);
        // Default Variable Init
        pri_ring_old_init = false;
        speedy_test = false;
        idle_wake_up = false;


        // Override Variables based on parameters.
        if ( params->ParamPresent("-idle_wake_up") ) {
            idle_wake_up = true;
            InfoPrintf("%s: %s: Idle trigger for wake up enabled.\n");
        }

        if ( params->ParamPresent("-pri_ring_old_init") ) {
            pri_ring_old_init = true;
            InfoPrintf("%s: %s: Using legacy method for Pri Ring initialisation.\n", TEST_NAME, __FUNCTION__);
        }
        
        if ( params->ParamPresent("-speedy") ) {
            speedy_test = true;
            InfoPrintf("%s: %s: MSI Speedy test enabled.\n", TEST_NAME, __FUNCTION__);
        }

        idle_mask[0] = params->ParamUnsigned("-idle_mask_0", 0x20000f);
        idle_mask[1] = params->ParamUnsigned("-idle_mask_1", 0x1020);
        idle_mask[2] = params->ParamUnsigned("-idle_mask_2", 0x0);
        idle_thres = params->ParamUnsigned("-idle_threshold", 0x1030);

        interPollWait = params->ParamUnsigned("-interpoll_wait", 1);
        maxPoll = params->ParamUnsigned("-max_poll", 100);

        // Print Parameters
        auto print = [](char* name, UINT32 val, bool hex = true) {
            InfoPrintf("%s: %s: Parameter: %s = %s%x.\n", TEST_NAME, __FUNCTION__, name, hex?"0x":"", val);
        };

        print("Idle Mask 0", idle_mask[0]);
        print("Idle Mask 1", idle_mask[1]);
        print("Idle Mask 2", idle_mask[2]);
        print("Idle threshold", idle_thres, false);
        print("Inter poll wait", interPollWait, false);
        print("Max Poll", maxPoll);

        InfoPrintf("%s: %s:, Parameter reading complete.\n", TEST_NAME, __FUNCTION__);
    }

// Destructor
MemSys_Island::~MemSys_Island ( void ) {

}

STD_TEST_FACTORY(memsys_island, MemSys_Island)

// Test Setup
int MemSys_Island::Setup ( void ) {
    InfoPrintf ( "Starting Setup() for %s.\n", TEST_NAME );

    lwgpu = LWGpuResource::FindFirstResource();
    m_regMap = lwgpu->GetRegisterMap();

    hReg = new regmap_helper(lwgpu, m_regMap);

    getStateReport()->init("memsys_island");
    getStateReport()->enable();

    
    InfoPrintf ( "Setup() done for %s.\n", TEST_NAME );
    return 1;
}

// Test Cleanup
void MemSys_Island::CleanUp ( void ) {
    InfoPrintf("Starting CleanUp() for %s.\n", TEST_NAME);
    if ( lwgpu ) {
        DebugPrintf ( "%s: %s: Releasing GPU.\n", TEST_NAME, __FUNCTION__ );
        lwgpu = 0;
    }
    delete hReg;
    LWGpuSingleChannelTest::CleanUp();
    InfoPrintf("CleanUp() done for %s.\n", TEST_NAME);
}

// Test Run
void MemSys_Island::Run ( void ) {
    // Start up
    InfoPrintf ( "Starting Run() for %s.\n", TEST_NAME );
    SetStatus(TEST_INCOMPLETE);

    // Initialising variables used across tests.
    engineIdx = to_string(LW_THERM_ENGINE_GRAPHICS);
    idleMaskIdx = to_string(LW_PPWR_PMU_IDLE_MASK_SUPP_IDX_GR);
    engPriBase = 0;
    engID = "0";   


    // Call different test functions based on parameter here.
    // If they failed, setstatus fail, print and return.
    if ( speedy_test ) {
        InfoPrintf ( "Launching MSI Speedy Test.\n" );
        if ( MSI_Speedy_Test() ) {
            SetStatus(TEST_FAILED);
            getStateReport()->runFailed("MSI Speedy test failed.\n");
            return;
        }
        InfoPrintf ( " MSI Speedy test completed.\n");
    }


    // Finish up
    SetStatus(TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
    InfoPrintf("Run() done for %s.\n", TEST_NAME);
}

/**************************************************************************************************
 * TEST DEFINITIONS                                                                               *
 **************************************************************************************************/

/**************************************************************************************************
 * MSI_Speedy_Test                                                                                *
 *      Speedy test exercises the entry-exit sequence for MSI, with a few checks in resident      *
 *      state and post wake up                                                                    *
 **************************************************************************************************/
 int MemSys_Island::MSI_Speedy_Test() {
    InfoPrintf("%s: %s: Starting Test.\n", TEST_NAME, __FUNCTION__);
    int errCnt = 0;
    
    // Gather runlist info.
    engPriBase = getRunlistInfo("RUNLIST_PRI_BASE");
    if ( engPriBase == 0xbadfbadf ) {
        ErrPrintf("%s: %s: Error getting runlist info: RUNLIST_PRI_BASE.\n", TEST_NAME, __FUNCTION__);
        return 1;
    }
    resetID = getRunlistInfo("RESET_ID");
    if ( resetID == 0xbadfbadf ) {
        ErrPrintf("%s: %s: Error getting runlist info: RESET_ID.\n", TEST_NAME, __FUNCTION__);
        return 1;
    }
    UINT32 tmp = getRunlistInfo("RLENG_ID");
    if ( tmp == 0xbadfbadf ) {
        ErrPrintf("%s: %s: Error getting runlist info: RLENG_ID.\n", TEST_NAME, __FUNCTION__);
        return 1;
    }
    engID = to_string(tmp);
    InfoPrintf("%s: %s: Populating runlist info for GRAPHICS - PRI_BASE: 0x%x; RLENG_ID: %s; RESET_ID: %d.\n",
                TEST_NAME, __FUNCTION__, engPriBase, engID.c_str(), resetID);



    // Run Init
    InfoPrintf("%s: %s: Launching Init. \n", TEST_NAME, __FUNCTION__);
    if ( int err = MSI_GPU_Init() ) {
        InfoPrintf("%s: %s: Test completed.\n", TEST_NAME, __FUNCTION__);
        ErrPrintf("%s: %s: Test failed in Init phase. ErrCount = %d\n", TEST_NAME, __FUNCTION__, err);
        return 1;
    }



    // Enter MSI
    InfoPrintf("%s: %s: Launching MSI Entry. \n", TEST_NAME, __FUNCTION__);
    if ( int err = MSI_Entry() ) {
        InfoPrintf("%s: %s: Test completed.\n", TEST_NAME, __FUNCTION__);
        ErrPrintf("%s: %s: Test failed during MSI Entry. ErrCount = %d\n", TEST_NAME, __FUNCTION__, err);
        return 1;
    }
    


    // Resident state checks
    // Check 1: MMU Ilwalidate
    InfoPrintf("%s: %s: Issuing MMU ilwalidate as a resident state check.\n", TEST_NAME, __FUNCTION__);
    errCnt += hReg->regWr("PFB","PRI_MMU_ILWALIDATE",{{"ALL_VA","TRUE"},{"ALL_PDB","TRUE"},{"HUBTLB_ONLY","FALSE"},{"TRIGGER","TRUE"}});
    errCnt += abs(hReg->regPoll("PFB","PRI_MMU_CTRL","PRI_FIFO_EMPTY","TRUE", interPollWait, maxPoll));
    
    // TODO: Check 2: PRI access to random unblocked register
    
    if (errCnt) {
        InfoPrintf("%s: %s: Test completed.\n", TEST_NAME, __FUNCTION__);
        ErrPrintf("%s: %s: Test failed during resident state checks. ErrCount = %d.\n", TEST_NAME, __FUNCTION__, errCnt);
        return 1;
    }



    // Trigger Wake up - Done by TB.



    // Exit MSI
    InfoPrintf("%s: %s: Launching MSI Exit. \n", TEST_NAME, __FUNCTION__);
    if ( int err = MSI_Exit() ) {
        InfoPrintf("%s: %s: Test completed.\n", TEST_NAME, __FUNCTION__);
        ErrPrintf("%s: %s: Test failed during MSI Exit. ErrCount = %d\n", TEST_NAME, __FUNCTION__, err);
        return 1;
    }



    // Post wake up checks.
    // Check 1: MMU Ilwalidate
    errCnt += hReg->regWr("PFB","PRI_MMU_ILWALIDATE",{{"ALL_VA","TRUE"},{"ALL_PDB","TRUE"},{"HUBTLB_ONLY","FALSE"},{"TRIGGER","TRUE"}});
    errCnt += abs(hReg->regPoll("PFB","PRI_MMU_CTRL","PRI_FIFO_EMPTY","TRUE", interPollWait, maxPoll));
 
    // TODO: Check 2: PRI access to random GR GPC register
    
    if (errCnt) {
        InfoPrintf("%s: %s: Test completed.\n", TEST_NAME, __FUNCTION__);
        ErrPrintf("%s: %s: Test failed during resident state checks. ErrCount = %d.\n", TEST_NAME, __FUNCTION__, errCnt);
        return 1;
    }
    
    InfoPrintf("%s: %s: Test Completed.\n", TEST_NAME, __FUNCTION__);
    return 0;
 }

/**************************************************************************************************
 * MSI_GPU_Init                                                                                   *
 *      Initialise the chip before running any test. Includes clearing resets, pri ring init,     *
 *      priv blocker init, idle threshold/mask programming and LPWR_ENG init.                     *
 **************************************************************************************************/
int MemSys_Island::MSI_GPU_Init() {
    InfoPrintf("%s: %s: Starting init...\n", TEST_NAME, __FUNCTION__);
    int errCnt = 0;

    // Enable HUB, XBAR, L2
    errCnt += hReg->regWr("PMC","ELPG_ENABLE",{{"XBAR","ENABLED"},{"L2","ENABLED"},{"HUB","ENABLED"}});


    // PRI RING init
    // De-assert pri-ring reset
    errCnt += hReg->regWr("PPRIV","MASTER_RING_GLOBAL_CTL","RING_RESET","DEASSERTED");
    
    if ( pri_ring_old_init ) { // OLD METHOD FOR INIT
        // Enumerate Ring
        errCnt += hReg->regWr("PPRIV" , "MASTER_RING_COMMAND", "CMD_ENUMERATE_STATIONS_BC_GRP", "ALL");
        errCnt += hReg->regWr("PPRIV", "MASTER_RING_COMMAND", "CMD", "ENUMERATE_STATIONS");
        
        // Poll for completion.
        errCnt += abs(hReg->regPoll("PPRIV", "MASTER_RING_COMMAND", 0, interPollWait, maxPoll));
        // Set Seed
        errCnt += hReg->regWr("PPRIV", "MASTER_RING_COMMAND_DATA", "START_RING_SEED", 0x503B4B49);
        // Start Ring
        errCnt += hReg->regWr("PPRIV", "MASTER_RING_COMMAND", "CMD", "START_RING");
    } else {   // NEW METHOD FOR INIT
        // Enumerate and start ring 
        errCnt += hReg->regWr("PPRIV", "MASTER_RING_COMMAND", "CMD", "ENUMERATE_AND_START_RING");
    }

    // Configure pri ring to wait for startup before accepting transactions
    errCnt += hReg->regWr("PPRIV", "SYS_PRIV_DECODE_CONFIG", "RING", "WAIT_FOR_RING_START_COMPLETE");
    bool valid;
    hReg->regRd("PPRIV", "SYS_PRIV_DECODE_CONFIG", valid);
    // ^^ Flush read

    // Check if ring started
    errCnt += abs(hReg->regPoll("PPRIV", "MASTER_RING_START_RESULTS", "CONNECTIVITY", "PASS", interPollWait, maxPoll)); 


    // Enable engines.
    errCnt += hReg->regWr("PMC","ENABLE",0xffffffff);
    errCnt += hReg->regWr("PMC","DEVICE_ENABLE("+to_string(resetID/32)+")",0xffffffff);

    // Enable GPC
//    errCnt += hReg->regWr("PGRAPH", "PRI_GPCS_GPCCS_ENGINE_RESET_CTL", "GPC_ENGINE_RESET", "DISABLED");


    // MMU Bind
    errCnt += mmuBind(true);


    // Program PRIV Blocker ranges
    // TODO: Fill complete range
    for ( string unit : {"PSEC", "PGSP"} ) {
        errCnt += hReg->regWr(unit, "PRIV_BLOCKER_RANGEC", {{"OFFS","INIT"},{"BLK","BL"},{"DOMAIN","GR"},{"AINCW","TRUE"},{"AINCR", "FALSE"}});
        errCnt += hReg->regWr(unit, "PRIV_BLOCKER_RANGED", DRF_BASE(LW_PGRAPH) );
        errCnt += hReg->regWr(unit, "PRIV_BLOCKER_RANGED", DRF_EXTENT(LW_PGRAPH) );
        errCnt += hReg->regWr(unit, "PRIV_BLOCKER_GR_BL_CFG", "RANGE0", "ENABLED");
    }


    // Program Idle threshold
    errCnt += hReg->regWr("PPWR","PMU_PG_IDLEFILTH("+engineIdx+")",idle_thres);

    
    // Idle Mask programming
    for (int i = 0; i < 3; i++ ) {
        string reg_name = "PMU_IDLE_MASK" + (i?"_"+to_string(i):"") + "_SUPP("+idleMaskIdx+")";
        errCnt += hReg->regWr("PPWR",reg_name,idle_mask[i]);
    }


    // Enable LPWR ENG
    errCnt += hReg->regWr("PPWR", "PMU_PG_CTRL("+engineIdx+")", {{"ENG","ENABLE"},{"ENG_TYPE","LPWR"},{"IDLE_MASK_VALUE","IDLE"}});


    // Enable Interrupts
    errCnt += hReg->regWr("PPWR", "PMU_PG_INTREN("+engineIdx+")", 0xffffffff);


    // Trigger LPWR_ENG
    errCnt += hReg->regWr("PPWR", "PMU_PG_CTRL("+engineIdx+")", "CFG_RDY", "TRUE");

    InfoPrintf("%s: %s: Init done - %d error(s) found!\n", TEST_NAME, __FUNCTION__, errCnt);
    return errCnt;
}

/**************************************************************************************************
 * MSI_Entry                                                                                      *
 *      Exercises the entry sequence for MSI feature.                                             *
 **************************************************************************************************/
 int MemSys_Island::MSI_Entry() {
     InfoPrintf("%s: %s: Starting MSI Entry.\n", TEST_NAME, __FUNCTION__);
     int errCnt = 0;
     bool valid = false;


     // Wait for PG_ON intr
     errCnt += hReg->regPoll("PPWR", "PMU_PG_INTRSTAT("+engineIdx+")", "PG_ON", "SET", interPollWait, maxPoll);

     
     // Check Idle
     if ( !isGPCIdle() ) {
         InfoPrintf("%s: %s: MSI entry done.\n", TEST_NAME, __FUNCTION__);
         ErrPrintf("%s: %s: MSI Entry failed due to Non-idle.\n", TEST_NAME, __FUNCTION__);
         return 1;
     }


     // Disable Host Scheduling
     errCnt += hReg->regWr("RUNLIST", "SCHED_DISABLE", "RUNLIST", "DISABLED", engPriBase);


     // Check Idle
     if ( !isGPCIdle() ) {
         InfoPrintf("%s: %s: MSI entry done.\n", TEST_NAME, __FUNCTION__);
         ErrPrintf("%s: %s: MSI Entry failed due to Non-idle.\n", TEST_NAME, __FUNCTION__);
         return 1;
     }


     // Enable Engine Holdoff
     errCnt += hReg->regWr("THERM", "ENG_HOLDOFF", "ENG"+engineIdx, "BLOCKED");
     errCnt += abs(hReg->regPoll("THERM", "ENG_HOLDOFF_ENTER", "ENG"+engineIdx, "DONE", interPollWait, maxPoll));


     // Enable PRIV blockers
     errCnt += togglePrivBlocker(true);
    

     // Check Idle
     if ( !isGPCIdle() ) {
         InfoPrintf("%s: %s: MSI entry done.\n", TEST_NAME, __FUNCTION__);
         ErrPrintf("%s: %s: MSI Entry failed due to Non-idle.\n", TEST_NAME, __FUNCTION__);
       return 1;
     }


     // Preempt Sequence : Disable Holdoff intr
     errCnt += hReg->regWr("THERM", "ENG_HOLDOFF_INTR_EN", "ENG"+engineIdx, "DISABLED");


     // Preempt Sequence : Initiate Preempt
     errCnt += hReg->regWr("RUNLIST", "PREEMPT", "TYPE", "RUNLIST", engPriBase);


     // Preempt Sequence : Release holdoff
     errCnt += hReg->regWr("THERM", "ENG_HOLDOFF", "ENG"+engineIdx, "NOTBLOCKED");
     errCnt += abs(hReg->regPoll("THERM", "ENG_HOLDOFF_ENTER", "ENG"+engineIdx, "NOT_DONE", interPollWait, maxPoll));


     // Preempt Sequence : Release holdoff intr
     errCnt += hReg->regWr("THERM", "ENG_HOLDOFF_INTR_EN", "ENG"+engineIdx, "ENABLED");


     // Preempt Sequence : Wait for completion
     errCnt += abs(hReg->regPoll("RUNLIST", "PREEMPT", "RUNLIST_PREEMPT_PENDING", "FALSE", interPollWait, maxPoll, true, engPriBase));


     // Reengage holdoff
     errCnt += hReg->regWr("THERM", "ENG_HOLDOFF", "ENG"+engineIdx, "BLOCKED");
     errCnt += abs(hReg->regPoll("THERM", "ENG_HOLDOFF_ENTER", "ENG"+engineIdx, "DONE", interPollWait, maxPoll));


     // Enable host scheduling
     errCnt += hReg->regWr("RUNLIST", "SCHED_DISABLE", "RUNLIST", "ENABLED", engPriBase);


     // Check Idle
     if ( !isGPCIdle() ) {
         InfoPrintf("%s: %s: MSI entry done.\n", TEST_NAME, __FUNCTION__);
         ErrPrintf("%s: %s: MSI Entry failed due to Non-idle.\n", TEST_NAME, __FUNCTION__);
         return 1;
     }

     // Ctx save-restore - TRY (TODO) for any test that includes FECS uCode.
     // MMU unbind
     errCnt += mmuBind(false);

     
     // Assert "RESET_EXT" clamp and wait
     // errCnt += hReg->regWr("PPWR", "PMU_LWVDD_RG_CLAMP_EN", "RESET_EXT", "ASSERTED");
     Platform::Delay(1);

     // Assert resets and wait
     // errCnt += hReg->regWr("PPWR", "PMU_GPC_RG_RESET_CONTROL", <value>);
     Platform::Delay(1);

     // Gate clk
        // If clk not on ALT path, ungate ALT path and switch to ALT path
     switch ( hReg->regChk("PTRIM", "GPC_BCAST_STATUS_SEL_VCO", "GPC2CLK_OUT", "BYPASS") ) {
         case -1: errCnt++; break;
         case 0: break;
         default: 
                errCnt += hReg->regWr("PTRIM","SYS_GPCPLL_CLK_SWITCH_DIVIDER","GATE_CLOCK","INIT");
                valid = false;
                hReg->regRd("PTRIM","SYS_GPCPLL_CLK_SWITCH_DIVIDER",valid);
                if (!valid) errCnt++;

                errCnt += hReg->regWr("PTRIM", "GPC_BCAST_SEL_VCO", 0x2);//"GPC2CLK_OUT", "BYPASS");
                errCnt += abs(hReg->regChk("PTRIM", "GPC_BCAST_SEL_VCO", "GPC2CLK_OUT", "BYPASS"));
                errCnt += abs(hReg->regPoll("PTRIM", "GPC_BCAST_STATUS_SEL_VCO", "GPC2CLK_OUT", "BYPASS", interPollWait, maxPoll));
     }

        // Gate ALT path
     errCnt += hReg->regWr("PTRIM","SYS_GPCPLL_CLK_SWITCH_DIVIDER","GATE_CLOCK","GATED");
     valid = false;
     hReg->regRd("PTRIM","SYS_GPCPLL_CLK_SWITCH_DIVIDER",valid);
     if (!valid) errCnt++;
        // TODO - GPCPLL shutdown if needed.
    

     // Assert clamps
     // errCnt += hReg->regWr("PPWR", "PMU_LWVDD_RG_CLAMP_EN", <value>);
     Platform::Delay(1);
     // Trigger rail shutdown - TODO
     // Clear interrupt
     errCnt += hReg->regWr("PPWR", "PMU_PG_INTRSTAT("+engineIdx+")", "PG_ON", "CLEAR");


     InfoPrintf("%s: %s: MSI Entry completed - %d error(s) found!\n", TEST_NAME, __FUNCTION__, errCnt);
     return errCnt;
 }

/**************************************************************************************************
 * MSI_Exit                                                                                       *
 *      Exercises the exit sequence for the MSI feature                                           *
 **************************************************************************************************/
int MemSys_Island::MSI_Exit() {
    InfoPrintf("%s: %s: Starting MSI exit...\n", TEST_NAME, __FUNCTION__);
    int errCnt = 0;
    bool valid;

    
    // Wait for interrupts
    bool done = false;
    UINT32 pollCnt = 0;
    string intrList[3][3] = {{"PSEC","PRIV_BLOCKER_INTR","GR"},
                             {"PGSP","PRIV_BLOCKER_INTR","GR"},
                             {"PPWR","PMU_INTR_1","HOLDOFF"}};
    while ( !done && pollCnt < maxPoll ) {
        for (string* reg : intrList ) {
            int res = hReg->regChk(reg[0],reg[1],reg[2],"PENDING");
            if (res == -1) errCnt++;
            if (res == 0) {
                done = true;
                break;
            }
        }
        if (!done) Platform::Delay(interPollWait);
        pollCnt++;    
    }
    if (!done) {
        WarnPrintf("%s: %s: Timeout while polling for interrupts.\n", TEST_NAME, __FUNCTION__);
    }


    // Trigger Rail wakeup - TODO
    // Ungate GPCCLK
    errCnt += hReg->regWr("PTRIM","SYS_GPCPLL_CLK_SWITCH_DIVIDER","GATE_CLOCK","INIT");
    valid = false;
    hReg->regRd("PTRIM","SYS_GPCPLL_CLK_SWITCH_DIVIDER",valid);
    if (!valid) errCnt++;
    

    // Disable "RESET_EXT" clamp
    // errCnt += hReg->regWr("PPWR", "PMU_LWVDD_RG_CLAMP_EN", "RESET_EXT", "DEASSERTED");
    Platform::Delay(1);

    // Disable clamps
    // errCnt += hReg->regWr("PPWR", "PMU_LWVDD_RG_CLAMP_EN", <value>);
    Platform::Delay(1);

    // Trigger RAM repair - TODO

    // Deassert resets
    // errCnt += hReg->regWr("PPWR", "PMU_GPC_RG_RESET_CONTROL", <value>);
    Platform::Delay(1);
        // De-assert engine_reset_
        errCnt += hReg->regWr("PGRAPH", "PRI_GPCS_GPCCS_ENGINE_RESET_CTL", "GPC_ENGINE_RESET", "DISABLED");
        Platform::Delay(1);


        // Switch to VCO path 
    errCnt += hReg->regWr("PTRIM", "GPC_BCAST_SEL_VCO", 0x3); //"GPC2CLK_OUT", "VCO");
    errCnt += abs(hReg->regPoll("PTRIM", "GPC_BCAST_STATUS_SEL_VCO", "GPC2CLK_OUT", "VCO", interPollWait, maxPoll));



    // Wait for RAM scrubbing to complete
    errCnt += abs(hReg->regPoll("PGRAPH", "PRI_GPCS_GPCCS_FALCON_DMACTL", {{"DMEM_SCRUBBING", "DONE"},{"IMEM_SCRUBBING", "DONE"}}, interPollWait, maxPoll));

    
    // GPCCS falcon bootstrap - TRY (TODO) for some test with FECS uCode
    // Ctx restore - TRY (TODO) for some test with FECS uCode


    // Disengage PRIV blockers
    errCnt += togglePrivBlocker(false);
    


    // Disengage holdoff
    errCnt += hReg->regWr("THERM", "ENG_HOLDOFF", "ENG"+engineIdx, "NOTBLOCKED");
    errCnt += abs(hReg->regPoll("THERM", "ENG_HOLDOFF_ENTER", "ENG"+engineIdx, "NOT_DONE", interPollWait, maxPoll));


    // Clear Interrupts
    if (!hReg->regChk("PSEC","PRIV_BLOCKER_INTR","GR","PENDING"))
        errCnt += hReg->regWr("PSEC", "PRIV_BLOCKER_INTR","GR", "RESET");
    if (!hReg->regChk("PGSP","PRIV_BLOCKER_INTR","GR","PENDING"))
        errCnt += hReg->regWr("PGSP", "PRIV_BLOCKER_INTR", "GR", "RESET");
    if (!hReg->regChk("PPWR","PMU_INTR_1", "HOLDOFF", "PENDING"))
        errCnt += hReg->regWr("PPWR", "PMU_INTR_1", "HOLDOFF", "CLEAR");
        

    InfoPrintf("%s: %s: MSI Exit completed - %d error(s) found!\n", TEST_NAME, __FUNCTION__, errCnt);
    return errCnt;
}

/**************************************************************************************************
 * HELPER FUNCTIONS                                                                               *
 **************************************************************************************************/
 
/**************************************************************************************************
 * isIdle                                                                                         *
 *      Function to check if GR_ENG is idle. Includes ESCHED_IDLE                                 *
 **************************************************************************************************/
 bool MemSys_Island::isGPCIdle() {
     if ( !hReg->regChk("RUNLIST","ENGINE_STATUS0("+engID+")", "ENGINE", "IDLE", engPriBase ) &&
            !hReg->regChk("RUNLIST", "ENGINE_STATUS0("+engID+")", "CTX_STATUS", "INVALID", engPriBase) )
         return true;
     else 
         return false;
 }

/**************************************************************************************************
 * getRunlistInfo                                                                                 *        
 *      Collect data for GR_ENG specified in the PTOP_DEVICE_INFO2 table.                         *
 **************************************************************************************************/
UINT32 MemSys_Island::getRunlistInfo(string field_n) {
    // Declaration of variables.
    const UINT32 ERR = 0xbadfbadf;
    bool valid;

    string regName = "LW_PTOP_DEVICE_INFO2";
    string devType = regName + "_DEV_TYPE_ENUM";
    UINT32 devTypeVal = 0; // _GRAPHICS
    
    unique_ptr<IRegisterClass> reg;
    unique_ptr<IRegisterField> devTypeFld;

    if ( !(reg = m_regMap->FindRegister(regName.c_str())) ) {
        ErrPrintf("%s: %s: Register %s not found.\n", TEST_NAME, __FUNCTION__);
        return ERR;
    }
    if ( !(devTypeFld = reg->FindField(devType.c_str())) ) {
        ErrPrintf("%s: %s: Field %s not found.\n", TEST_NAME, __FUNCTION__);
        return ERR;
    }

    UINT32 numRows = hReg->regRd("PTOP", "DEVICE_INFO_CFG", "NUM_ROWS", valid);
    if (!valid) {
        ErrPrintf("%s: %s: Error reading register field %s.\n", TEST_NAME, __FUNCTION__, "LW_PTOP_DEVICE_INFO_CFG_NUM_ROWS");
        return ERR;
    }

    // Function to get value of a field from aclwmulated string
    auto rdVal = [] ( string info, IRegisterField* fld ) {
        string tmp = info.substr(fld->GetStartBit(), (fld->GetEndBit() - fld->GetStartBit() + 1));
        // Reverse again to get in correct order.
        reverse(tmp.begin(), tmp.end());
        return bitset<32>(tmp).to_ulong();
    };

    // Actual reading of config registers
    for ( UINT32 count = 0; count < numRows; count++ ) {
        string devInfo = "";
        string cnt = to_string(count);

        // Accumulate 3-word register in devInfo
        // 95...64|63...32|31...0
        while ( !hReg->regChk("PTOP", "DEVICE_INFO2("+cnt+")", "ROW_CHAIN", 1/*"MORE"*/) ) {
            devInfo = bitset<32>(hReg->regRd("PTOP", "DEVICE_INFO2("+cnt+")", valid)).to_string() + devInfo;
            count++;
            cnt = to_string(count);
        }
        if ( devInfo.empty() ) {
            continue;
        }
        devInfo = bitset<32>(hReg->regRd("PTOP", "DEVICE_INFO2("+cnt+")", valid)).to_string() + devInfo;

        // Reversing devInfo for easier location of bits.
        reverse(devInfo.begin(), devInfo.end());

        // Read field only if _DEV_TYPE matches argument.
        if ( rdVal(devInfo, devTypeFld.get()) == devTypeVal) {
            string fieldName = regName + "_DEV_" + field_n;
            unique_ptr<IRegisterField> field;
            if ( !(field= reg->FindField(fieldName.c_str())) ) {
                ErrPrintf("%s: %s: Field %s not found.\n", TEST_NAME, __FUNCTION__, fieldName.c_str());
                return ERR;
            }
            UINT32 tmp = rdVal(devInfo,field.get());
            // DEVICE/RUNLIST_PRI_BASE fields are dword aligned
            if ( field_n == "DEVICE_PRI_BASE" || field_n == "RUNLIST_PRI_BASE" ) return (tmp << (field->GetStartBit()%32));
            else return tmp;
        }
    }
    ErrPrintf("%s: %s: _DEV_TYPE_GRAPHICS not found in register %s.\n", TEST_NAME, __FUNCTION__, regName.c_str()); 
    return ERR;
}

/**************************************************************************************************
 * MMUBind                                                                                        *
 *      Issue bind/unbind requests to MMU                                                         *
 **************************************************************************************************/
 int MemSys_Island::mmuBind(bool bind) {
     // Check if queue empty
     if ( hReg->regChk("PFB", "PRI_MMU_CTRL", "PRI_FIFO_EMPTY", "TRUE") ) {
         ErrPrintf("%s: %s: MMU queue not empty, can't bind/unbind.\n", TEST_NAME, __FUNCTION__);
         return 1;
     }

     // Check if bind requests pending
     if ( hReg->regChk("PFB", "PRI_MMU_BIND", "TRIGGER", "FALSE") ) {
         ErrPrintf("%s: %s: Bind requests already pending, skipping bind/unbind.\n", TEST_NAME, __FUNCTION__);
         return 1;
     }

     int errCnt = 0;
     // Set aperture
     if (bind) errCnt += hReg->regWr("PFB", "PRI_MMU_BIND_IMB", "APERTURE", "SYS_MEM_C");
     else errCnt += hReg->regWr("PFB", "PRI_MMU_BIND_IMB", "APERTURE", 1);

     // Trigger bind/unbind
     errCnt += hReg->regWr("PFB", "PRI_MMU_BIND", {{"ENGINE_ID", "VEID0"}, {"TRIGGER", "TRUE"}});

     // Poll for completion
     errCnt += abs(hReg->regPoll("PFB", "PRI_MMU_BIND", "TRIGGER", "FALSE", interPollWait, maxPoll));

     return errCnt;
 }

/**************************************************************************************************
 * togglePrivBlocker                                                                              *
 *      Enable/Disable priv blockers                                                              *
 **************************************************************************************************/
 int MemSys_Island::togglePrivBlocker(bool enable) {
     int errCnt = 0;
     if (enable) {
         for ( string unit : {"PSEC","PGSP"} ) {
             errCnt += hReg->regWr(unit, "PRIV_BLOCKER_CTRL", "BLOCKMODE", "BLOCK_GR");
             errCnt += abs(hReg->regPoll(unit, "PRIV_BLOCKER_STAT", "STATUS", "BLOCKED", interPollWait, maxPoll));
         }
     } else {
         for ( string unit : {"PSEC","PGSP"} ) {
             errCnt += hReg->regWr(unit, "PRIV_BLOCKER_CTRL", "BLOCKMODE", "BLOCK_NONE");
             errCnt += abs(hReg->regPoll(unit, "PRIV_BLOCKER_STAT", "STATUS", "BLOCKED", interPollWait, maxPoll, false));
         }
     }
     return errCnt;
 }
 
