/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DATA_H_
#define DATA_H_

#include "memory.h"
//#include "config/fbfalcon-config.h"


#define SHA_HASH_REGISTERS 8

#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))|| FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE)
#include "sha256.h"
#endif


// xref: /chips_a/drivers/cheetah/platform/drivers/memory/inc/mem/memdecl.h
#define ADDR_UNKNOWN 0              // Address space is unknown
#define ADDR_SYSMEM  1              // System memory (PCI)
#define ADDR_FBMEM   2              // Frame buffer memory space
#define ADDR_AGPMEM  3              // AGP memory space
#define ADDR_REGMEM  4              // LW register memory space
#define ADDR_VIRTUAL 5              // Virtual address space only


#define AON_SELWRE_SCRATCH_GROUP13_MEDIA_FIELD   7:0
#define AON_SELWRE_SCRATCH_GROUP13_INFO_FIELD  31:16
//  [31] boot training data available in pafalcon dmem
//  [30] debug data available in pafalcon dmem
//  [24] coldboot completed (this is equivalent to gc6 exit)
typedef struct AON_STATE_INFO {
    LwU8 trainingCompleted:    1;   // flag enabled in boottime training binary upon successful completion
    LwU8 unused:               5;
    LwU8 dataInSystemMemory:   1;
    LwU8 dataInPaFalconMemory: 1;   // indicator that the boottime training binary has logged data
                                    // to the pafalcon for debug purposes
    LwU8 debugDataBlks:        8;   // number of blocks with debug data for obvref from the boottime
                                    // training binary
} AON_STATE_INFO;


#define DB_BYTE_SIZE 8192
#define DB_WORD_SIZE (DB_BYTE_SIZE/4)

#define VREF_DFE_CTRL_REGISTERS  ((MAX_PARTS) * (SUBPS) * (BYTE_PER_SUBP) * 3)



typedef struct __attribute__((__packed__)) TRAINING_DATA {

    LwU8  rdHybridDfeDqDbiValid;
    LwU8  rdHybridDfeEdcValid;

    LwU8  rdVrefDqDbiValid;
    LwU8  rdVrefEdcValid;

    LwU8  wrHybridDfeDqDbiValid;
    LwU8  wrVrefDqDbiValid;

    // func_run_rd_tr_hybrid_non_vref
    // rd:
    //   func_read_tag(gTD.td.rdHybridDfeDq, gTD.td.rdHybridDfeDbi, gTD.td.rdHybridDfeEdc, !bFlagG6EdcTrackingEn);
    //   gTD.td.rdHybridDfeDqDbiValid = 1;
    //   gTD.td.rdHybridDfeEdcValid = !bFlagG6EdcTrackingEn;
    //
    // wr: func_program_hybrid_opt_in_fbio
    // if (rdHybridDfeDqDbiValid)  {
    //    func_program_hybrid_opt_in_fbio(gTD.td.rdHybridDfeDq, gTD.td.rdHybridDfeDbi, gTD.td.rdHybridDfeEdc, rdHybridDfeEdcValid);
    // }
    LwU32 rdHybridDfeDq[TOTAL_DQ_BITS];
    LwU32 rdHybridDfeEdc[TOTAL_DBI_BITS];
    LwU32 rdHybridDfeDbi[TOTAL_DBI_BITS];

    //
    // rd (1st): func_run_rd_tr_hybrid_non_vref
    // rd (2nd): func_run_rd_tr_hybrid_vref
    // (1st)  func_save_2d_vref_values(gTD.td.rdVrefDq, gTD.td.rdVrefDbi, gTD.td.rdVrefEdc, !bFlagG6EdcTrackingEn);
    // (2nd) func_read_tag
    // 1st reads hw priv reg values,  2nd reads training tags
    //   gTD.td.rdVrefDqDbiValid = 1;
    //   gTD.td.rdVrefEdcValid = !bFlagG6EdcTrackingEn;
    //
    // wr:
    // if (rdVrefDqDbiValid) {
    //  func_program_optimal_vref_fbio(gTD.td.rdVrefDq, gTD.td.rdVrefDbi, gTD.td.rdVrefEdc, rdVrefEdcValid);
    // }
    //
    LwU32 rdVrefDq[TOTAL_DQ_BITS];
    LwU32 rdVrefEdc[TOTAL_DBI_BITS];
    LwU32 rdVrefDbi[TOTAL_DBI_BITS];

    // wr:
     // if (wrHybridDfeDqDbiValid)
     // {
     //   func_program_hybrid_opt_in_dram(wrHybridDfeDq,1,0);
     //   func_program_hybrid_opt_in_dram(wrHybridDfeDbi,0,1);
     //}
    LwU32 wrHybridDfeDq[TOTAL_DQ_BITS];
    LwU32 wrHybridDfeEdc[TOTAL_DBI_BITS];
    LwU32 wrHybridDfeDbi[TOTAL_DBI_BITS];

    // wr:
    // if (wrVrefDqDbiValid)
    // {
    //   func_program_vref_in_dram(wrVrefDq,1,0);
    //   func_program_vref_in_dram(wrVrefDbi,0,1);
    //}
    LwU32 wrVrefDq[TOTAL_DQ_BITS];
    LwU32 wrVrefDbi[TOTAL_DBI_BITS];

    // wr: switch to write when recovering in 2nd binary  (stefans)
    //     add flag when reading (stefans)

    //LwU32 vrefCodeCtrl[TOTAL_VREF_CODE_CTRLS];
    //LwU32 vrefDfeCtrl[TOTAL_VREF_DFE_CTRLS];
    // Adress training values
    // Delay Values are in packed register content format,  the training engine is writing these directly
    // to hw during training run. The buffer is used only for restore for driver reload and gc6 exit
    //LwU8  cmdDelayValid;
    //LwU32 cmdDelay[TOTAL_CMD_REGS];

} TRAINING_DATA;


typedef struct __attribute__((__packed__)) REGISTER_DATA {
    LwU32 cmdDelay[TOTAL_CMD_REGS];
    LwU32 vrefCodeCtrl[TOTAL_VREF_CODE_CTRLS];
    LwU32 vrefDfeCtrl[TOTAL_VREF_DFE_CTRLS];
    LwU32 vrefDfeG6xCtrl[TOTAL_VREF_DFEG6X_CTRLS];
    LwU32 vrefTrackingShadow[TOTAL_VREF_TRACKING_SHADOW_CTRLS];

    //// Additional buffer definitions for read back and cross compare check
    //LwU32 cmdDelayRb[TOTAL_CMD_REGS];
    //LwU32 vrefCodeCtrlRb[TOTAL_VREF_CODE_CTRLS];
    //LwU32 vrefDfeCtrlRb[TOTAL_VREF_DFE_CTRLS];
    //LwU32 vrefDfeG6xCtrlRb[TOTAL_VREF_DFEG6X_CTRLS];
    //
    //LwU32 cmdDelayDiff[TOTAL_CMD_REGS];
    //LwU32 vrefCodeCtrlDiff[TOTAL_VREF_CODE_CTRLS];
    //LwU32 vrefDfeCtrlDiff[TOTAL_VREF_DFE_CTRLS];
    //LwU32 vrefDfeG6xCtrlDiff[TOTAL_VREF_DFEG6X_CTRLS];
    //
    //// updated padding to adjust to debug buffers
    //LwU32 doNotUse_256BAlign[((64 - ((3*TOTAL_CMD_REGS + 3*TOTAL_VREF_CODE_CTRLS + 3*TOTAL_VREF_DFE_CTRLS + 3*TOTAL_VREF_DFEG6X_CTRLS) % 64) ) % 64)];


    // padding the data structure to 256
    LwU32 doNotUse_256BAlign[((64 - ((TOTAL_CMD_REGS + TOTAL_VREF_CODE_CTRLS + TOTAL_VREF_DFE_CTRLS + TOTAL_VREF_DFEG6X_CTRLS + TOTAL_VREF_TRACKING_SHADOW_CTRLS) % 64) ) % 64)];
} REGISTER_DATA;




#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))

extern LwU32 __pa_start;
extern LwU32 __pa_end;
extern LwU32 __sequencerDataStart;
extern LwU32 __trainingDataStart;

// We need to make sure that there is enougth memory allocated if FBIO_REG_STRUCT
// is bigger than the pafalcon insertion
//CASSERT( ( sizeof(_gFD) != (&_flexDataEnd - &_flexDataStart)), fbflcn_table_c);
#endif  // FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER)


void dataPrune(void)
    GCC_ATTRIB_SECTION("resident", "dataPrune");

void dataFill(void)   /* debug */
    GCC_ATTRIB_SECTION("resident", "dataFill");
LwBool dataSave(void)
    GCC_ATTRIB_SECTION("resident", "dataSave");

LwBool isTrainingDataAreaEmpty(void)
    GCC_ATTRIB_SECTION("resident", "isTrainingDataAreaEmpty");

void restoreRegister(void)
    GCC_ATTRIB_SECTION("init", "restoreRegister");

void restoreData(void)
    GCC_ATTRIB_SECTION("resident", "restoreData");

void setupRegionCfgForDataWpr(void)
    GCC_ATTRIB_SECTION("resident", "setupRegionCfgForDataWpr");

void removeAccessToRegionCfgPlm(void)
    GCC_ATTRIB_SECTION("resident", "removeAccessToRegionCfgPlm");

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))

void checkGroupPlm(LwU32 plmAdr)
    GCC_ATTRIB_SECTION("resident", "checkGroupPlm");

void writeAon13ScratchInfo(void)
    GCC_ATTRIB_SECTION("resident", "writeAon13ScratchInfo");

void readAonStateInfo(void)
    GCC_ATTRIB_SECTION("resident", "readAonStateInfo");

#endif

#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM)) || FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE)

void saveToSysMem(void)
    GCC_ATTRIB_SECTION("resident", "saveToSysMem");

void restoreFromSysMem(void)
    GCC_ATTRIB_SECTION("init", "restoreFromSysMem");

void storeShaHash(void)
    GCC_ATTRIB_SECTION("resident", "storeShaHash");

void compareToShaHash(LwU32* pShaHash)
    GCC_ATTRIB_SECTION("resident", "restoreShaHash");

#endif


#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))

LwBool setupGC6IslandRegisters(void)
    GCC_ATTRIB_SECTION("resident", "setupGC6IslandRegisters");

void setupRegionCfgForSysMemBuffer(void)
    GCC_ATTRIB_SECTION("resident", "setupRegionCfgForSysMemBuffer");

LwBool dataSaveToSysmemCarveout(void)
    GCC_ATTRIB_SECTION("resident", "dataSaveToSysmemCarveout");

LwBool dataReadFromSysmemCarveout(void)
    GCC_ATTRIB_SECTION("init", "dataReadFromSysmemCarveout");

#endif

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER)) || FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE)

void waitForPAFalconScrubbing(void)
    GCC_ATTRIB_SECTION("init", "waitForPAFalconScrubbing");

// dmem write to PA Falcon used by the pafalcon bootloader in the mclk switch binary
void dmemWritePAFalconWithCheck(LwU32* pSourceStart, LwU32 sourceSize, LwBool readbackCheck)
    GCC_ATTRIB_SECTION("init", "dmemWritePAFalcon");

// simplified linear write to the paflacon dmem, deployed in boot time training binary
// IMPROVE: this should be unified with dmemWritePAFalconWithCheck used for bootloading
//          pa falcon in the mclk switch binary
void dmemWritePAFalcon (LwU32* pSourceStart,LwU32 sourceSize,LwU32 destAddr);

void dmemReadPAFalcon(LwU32* pDestStart, LwU32 sourceSize)
    GCC_ATTRIB_SECTION("init", "dmemReadPAFalcon");

// copies debug data from temp location in the vbios area to the data buffer before
// saving to sysmem
void copyDebugBuffer(void) GCC_ATTRIB_SECTION("init", "copyDebugBuffer");

#endif

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))

void bootloadPAFalcon(void)
    GCC_ATTRIB_SECTION("init", "bootloadPAFalcon");

void startPaFalcon(void)
    GCC_ATTRIB_SECTION("init", "startPaFalcon");

void verifyPaFalcon(void)
    GCC_ATTRIB_SECTION("init", "verifyPaFalcon");

#endif  // (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))

#endif /* DATA_H_ */
