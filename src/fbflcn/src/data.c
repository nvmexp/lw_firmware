/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_access.h"
#include "data.h"
#include "priv.h"
#include "memory.h"
#include "segment.h"
#include "fbflcn_gddr_boot_time_training_tu10x.h"

#include "dev_gc6_island.h"
#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
extern OBJFBFLCN Fbflcn;
#include "config/fbfalcon-config.h"
#include "config/g_memory_private.h"
#include "config/g_fbfalcon_private.h"
#include "memory.h"
#include "config/g_memory_hal.h"

#include "dev_fb.h"
#include "dev_fbpa.h"
#include "dev_fbfalcon_csb.h"
#include "dev_fuse.h"
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER)) || (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
#include "dev_pafalcon_pri.h"
#include "seq_decoder.h"
#endif

#include "dev_top.h"

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
#include "sha256.h"
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_BOOT_TRAINING) && FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))

#define TRAINING_DATA_SIZE_BYTE (sizeof(TRAINING_DATA))
#define TRAINING_DATA_SIZE_DW ((sizeof(TRAINING_DATA) + (sizeof(LwU32)-1)) / sizeof(LwU32))

// define boot training data segment
// the size of the segement is defined through profiles
TRAINING_DATA gTD
GCC_ATTRIB_USED()
GCC_ATTRIB_SECTION("trainingData", "gTD");

// make sure the structure define never exceeds the allocated dmem area (compile time)
// FIXME: adjust allocation down to reasonable area
CASSERT(sizeof(TRAINING_DATA) <= CFG_TRAINING_TABLE_ALLOCATION, data_c);

// make sure the allocated area aligns to 256B chunk (compile time)
CASSERT((CFG_TRAINING_TABLE_ALLOCATION % 256) == 0, data_c);

#endif

#define REGISTER_DATA_SIZE_BYTE (sizeof(REGISTER_DATA))
#define REGISTER_DATA_SIZE_DW ((sizeof(REGISTER_DATA) + (sizeof(LwU32)-1)) / sizeof(LwU32))

extern LwU8 gPlatformType;
extern LwU8 gFbflcnLsMode;
extern LwU8 gPrivSecEn;
extern LwU8 gDDRMode;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
AON_STATE_INFO gAonStateInfo;
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))|| FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE)
//
// the sha hash contains the result from a sha256 algorithm run but is setup a bit different for the various runs:
// 1) f-model and emulation
//    In this run there is no valid training data and some of these test are run with different/incomplete bioses that might
//    not even do an mclk switch.  In this case we fill the buffer with sequential data on setup and run the sha mechanism.
//    This allows to test the system buffer and sha code
// 2) rtl
//    For exelwtion time reason at unit level the sha algorithm is diasabled. The test will run with the initial values in the sha
//    hash.  This does cover at least the registers in the simulation and the sha check code.
// 3) silicion
//    Everything is enabled and correct data should be shuttled around. The sha init value will simply be overwritten
//
LwU32 gShaHash[SHA_HASH_REGISTERS] = { 0x22221111, 0x33332222, 0x44443333, 0x55554444, 0x66665555, 0x77776666, 0x88887777, 0x99998888 } ;
#endif

// define fbio register data segment
// this is used in the mclk switch binary only to recover the fbio register settings
REGISTER_DATA gRD
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("fbioRegisterSpace", "gRD");
LwU32 gDD[DB_WORD_SIZE]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("debugDataSpace", "gDD");


#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))

// TEMP buffer for data save and restore
LwU32 gDB[DB_WORD_SIZE]
   GCC_ATTRIB_USED()
   GCC_ATTRIB_SECTION("db", "db");

#endif

void
dataPrune
(
		void
)
{
    LwU8* td = (LwU8*)&gRD;
    if ((sizeof(gRD) % 256) != 0) {
        FW_MBOX_WR32_STALL(13, sizeof(gRD));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_TRAINING_BUFFER_NOT_MULTIPE_256B);
    }

    LwU32 i;
    for (i=0; i<sizeof(REGISTER_DATA); i++)
    {
        *(td++) = 0x0;
    }


    for (i=0; i<DB_WORD_SIZE; i++)
    {
        gDD[i] = 0x0;
    }
}



/*
 * dataFill
 *   this is a testing function for fill the gc6 save/restore area with data, its used for code implementation testing
 *   at unit level as well as deployed in gc6 testing on emulation.
 */
void dataFill
(
        void
)
{
    LwU32 rdSize = (sizeof(REGISTER_DATA) + 3) / sizeof(LwU32);
    LwU32* pRd = (LwU32*)&gRD;
    LwU32 i;
    for (i=0;i<rdSize;i++)
    {
        *(pRd++) = i;
    }
}

void setupRegionCfgForDataWpr
(
		void
)
{
	LwU32 data = 0;
	LwU8 ctxDma = CTX_DMA_FBFALCON_WRITE_TO_MWPR_1;
	LwU8 wprId = 1;

	// Setup - Step 1 set FBIF_TRANSCFG
	data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_TARGET , _LOCAL_FB, data);
	data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_MEM_TYPE, _PHYSICAL, data);
	REG_WR32_STALL(CSB, LW_CFBFALCON_FBIF_TRANSCFG(ctxDma), data);

	//write status
	FW_MBOX_WR32_STALL(15, REG_RD32(CSB, LW_CFBFALCON_FBIF_TRANSCFG(ctxDma)) );

	// Setup - Step 2.  Set FBIF_REGIONCFG for WPR
	LwU32 mask = ~ ( 0xF << (ctxDma*4) );

	data = REG_RD32(CSB, LW_CFBFALCON_FBIF_REGIONCFG);
	data = (data & mask) | ((wprId & 0xF) << (ctxDma * 4));
	REG_WR32_STALL(CSB, LW_CFBFALCON_FBIF_REGIONCFG,  data);
}


void removeAccessToRegionCfgPlm
(
		void
)
{
	// we can remove access privileges on REGIONCFG and
	// TRANSCFG(i) as no further dma setup is required.
	//
	LwU32 regionCfgPLM = REG_RD32(CSB, LW_CFBFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK);
	regionCfgPLM = FLD_SET_DRF(_CFBFALCON,_FBIF_REGIONCFG_PRIV_LEVEL_MASK,_WRITE_PROTECTION, _ALL_LEVELS_DISABLED, regionCfgPLM );
	REG_WR32_STALL(CSB, LW_CFBFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK,regionCfgPLM);
}


#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
void saveToSysMem
(
        void
)
{
    setupRegionCfgForSysMemBuffer();
    restoreData();
    dataSaveToSysmemCarveout();
    gAonStateInfo.dataInPaFalconMemory = 0;
    writeAon13ScratchInfo();
}


void restoreFromSysMem
(
        void
)
{
    setupRegionCfgForSysMemBuffer();
    dataReadFromSysmemCarveout();
    restoreRegister();
}

#else
LwBool
dataSave
(
		void
)
{

	// check if ACR is engaged and the fbfalcon is in ls mode.
	// the data save path is lwrrently only available through wpr
	// TODO: (stefans) We should find a proper solution through sysmem as well, through for now this is working as we
	//       can defer to do the training in the first mclk swithc
	LwU32 fbflcnSctl = REG_RD32(CSB, LW_CFBFALCON_FALCON_SCTL);
	LwU8 fbflcnLsMode = DRF_VAL ( _CFBFALCON, _FALCON_SCTL, _LSMODE, fbflcnSctl );
	if (fbflcnLsMode != LW_CFBFALCON_FALCON_SCTL_LSMODE_TRUE)
	{
		return LW_FALSE;
	}

	LwU8 ctxDma = CTX_DMA_FBFALCON_WRITE_TO_MWPR_1;

	//write status
	FW_MBOX_WR32_STALL(15, REG_RD32(CSB, LW_CFBFALCON_FBIF_REGIONCFG) );

	// start address of data mwpr
	LwU32 mwprAdrLo= REF_VAL(
			LW_PFB_PRI_MMU_FALCON_FBFALCON_CFGA_ADDR_LO,
			REG_RD32(BAR0, LW_PFB_PRI_MMU_FALCON_FBFALCON_CFGA(1)));

	// end address of data mwpr
	LwU32 mwprAdrHi = REF_VAL(
			LW_PFB_PRI_MMU_FALCON_FBFALCON_CFGB_ADDR_HI,
			REG_RD32(BAR0, LW_PFB_PRI_MMU_FALCON_FBFALCON_CFGB(1)));

	LwU32 mwprSize = (mwprAdrHi - mwprAdrLo) * 0x1000;    // 4K blocks

	LwU32 dataWprStart_lo = (mwprAdrLo & 0x000FFFFF) << 12;
	LwU32 dataWprStart_hi = (mwprAdrLo & 0xFFF00000) >> 20;

	// program as base address for dma access through fbif
	// reducing to 256B block size
	falc_wspr(DMB, (( (dataWprStart_lo & 0xFFFFFF00) >> 8) |
			( (dataWprStart_hi & 0x000000FF) << 24)));
#if PA_47_BIT_SUPPORTED
	falc_wspr(DMB1, ( (dataWprStart_hi & 0xFFFFFF00) >> 8));
#endif

	LwU16 ctx = 0;
	falc_rspr(ctx, CTX);  // Read existing value
	ctx = (((ctx) & ~(0x7 << CTX_DMAIDX_SHIFT_DMWRITE)) | ((ctxDma) << CTX_DMAIDX_SHIFT_DMWRITE));
	falc_wspr(CTX, ctx); // Write new value

	LwU32 size = (sizeof(REGISTER_DATA) + 3) / 4;
	REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(2),size );

	// do check that we will not overrun the mwpr area
	LwBool bError;
	bError = ((size * 0x4) >= mwprSize);

	if (bError)
	{
		FW_MBOX_WR32_STALL(6, REG_RD32(CSB, LW_CFBFALCON_FALCON_SCTL));
		FW_MBOX_WR32_STALL(7, sizeof(REGISTER_DATA));
		FW_MBOX_WR32_STALL(8, mwprSize);
		FW_MBOX_WR32_STALL(9, size);
		FW_MBOX_WR32_STALL(12, mwprAdrLo);
		FW_MBOX_WR32_STALL(13, mwprAdrHi);
	    FBFLCN_HALT(FBFLCN_ERROR_CODE_TRAINING_DATA_EXCEEDS_MWPR1_HI);
	}

	LwU32 from = 0;
	LwU32 to = 0;
	from = (LwU32)&gRD;
	to = 0x0;

	LwU16 i=0;

	for (i=0; i<size; i++)
	{
		//
		// bits 18:16 of second operand of dmread defines the actual size of
		// transfer (6 = chunk of 256, 0 = chunk of 4)
		//
		//falc_dmwrite(from,to + (6<<16) );

		falc_dmwrite(from,to);
		falc_dmwait();

		from += 4;
		to += 4;
	}
#if (FBFALCONCFG_CHIP_ENABLED(GA10X))
	//to notify tb that data save and restore is done
	REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDA7A);
#endif
	return LW_TRUE;
}
#endif



/*
 * isTrainingDataLoaded
 */
LwBool
isTrainingDataAreaEmpty
(
		void
)
{
	LwU32* p = (LwU32*)&gRD;
	LwU32 i = 0;
	for (i=0;i<100;i++)
	{
	    if (*(p++) != 0)
	    {
	        return LW_FALSE;
	    }
	}
	return LW_TRUE;
}


// this is a dulicate of the set_cmd_hold_update function in
// fbflcn_gddr_boot_time_training_ga10x.c
void setCmdHoldUpdate
(
    LwBool enable
)
{
    // This register is written only by HW, so it should generally have
    // the same value. Using subp1 value here. Generally good to use
    // subp1 value for cases where both subp0 and subp1 values are
    // expected to be same and we are reading just one for RMW.
    LwU32 priv_cmd = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD);
    priv_cmd = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_PRIV_CMD, _HOLD_UPDATE,        enable, priv_cmd);
    //priv_cmd = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_PRIV_CMD, _DISABLE_DQS_UPDATE, enable, priv_cmd);

    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_PRIV_CMD, priv_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD, priv_cmd);
}

/*
 * restoreRegister
 * this will copy the training data from the data structure to the endpoint register structure in design
 * this flow will be triggered on gc6exit
 */

void
restoreRegister
(
		void
)
{
	// switch priv source to 0x1 in FBIO_CMD_DELAY
	LwU32 privSrcCmd = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY);
	LwU32 privSrcP0Cmd =  FLD_SET_DRF_NUM(_PFB_FBPA_FBIO, _CMD_DELAY, _CMD_PRIV_SRC, 0x1, privSrcCmd);
	REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY,privSrcP0Cmd);

	memoryMoveCmdTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.cmdDelay[0], REGISTER_WRITE);

	// switch back priv source to original state
	REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY,privSrcCmd);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
	// switch priv source to 0x2 in FBIO_DELAY
	LwU32 privSrc = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DELAY);
	LwU32 privSrcP0 =  FLD_SET_DRF_NUM(_PFB_FBPA_FBIO, _DELAY, _PRIV_SRC, 0x2, privSrc);
	REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_DELAY,privSrcP0);

	if (gDDRMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
	{
	    setCmdHoldUpdate(1);
	    memoryMoveVrefCodeTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefCodeCtrl[0], REGISTER_WRITE);
	    memoryMoveVrefDfeTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefDfeCtrl[0], REGISTER_WRITE);
	    memoryMoveVrefDfeG6xTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefDfeG6xCtrl[0], REGISTER_WRITE);
	    memoryMoveVrefTrackingShadowValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefTrackingShadow[0], REGISTER_WRITE);
	    setCmdHoldUpdate(0);

	    memoryProgramEdgeOffsetBkv_HAL();
	}

	// switch back priv source to original state
	REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_DELAY,privSrc);
#endif
#endif

}


/*
 * restoreData
 * this will copy the register content from the design into the table stucture in memory
 * this flow will be triggered on driver reload and cold boot.
 *
 */

void
restoreData
(
		void
)
{
	// switch priv source to 0x1
	LwU32 privSrcCmd = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY);
	LwU32 privSrcP0Cmd =  FLD_SET_DRF_NUM(_PFB_FBPA_FBIO, _CMD_DELAY, _CMD_PRIV_SRC, 0x1, privSrcCmd);
	REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY,privSrcP0Cmd);

	memoryMoveCmdTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.cmdDelay[0], REGISTER_READ);

	// switch back priv source to original state
	REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY,privSrcCmd);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
	if (gDDRMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
	{
	    // switch priv source to 0x2 in FBIO_DELAY
	    LwU32 privSrc = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DELAY);
	    LwU32 privSrcP0 =  FLD_SET_DRF_NUM(_PFB_FBPA_FBIO, _DELAY, _PRIV_SRC, 0x2, privSrc);
	    REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_DELAY,privSrcP0);

	    // read the vref & dfe codes from the registers
	    memoryMoveVrefCodeTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefCodeCtrl[0], REGISTER_READ);
	    memoryMoveVrefDfeTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefDfeCtrl[0], REGISTER_READ);
	    memoryMoveVrefDfeG6xTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefDfeG6xCtrl[0], REGISTER_READ);
        memoryMoveVrefTrackingShadowValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefTrackingShadow[0], REGISTER_READ);
	    // switch back priv source to original state
	    REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_DELAY,privSrc);

	}
#endif
#endif
}

/*
void
restoreDataWithSelfcheck
(
        void
)
{
    // switch priv source to 0x1
    LwU32 privSrcCmd = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY);
    LwU32 privSrcP0Cmd =  FLD_SET_DRF_NUM(_PFB_FBPA_FBIO, _CMD_DELAY, _CMD_PRIV_SRC, 0x1, privSrcCmd);
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY,privSrcP0Cmd);

    memoryMoveCmdTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.cmdDelayRb[0], REGISTER_READ);

    // switch back priv source to original state
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY,privSrcCmd);

    // switch priv source to 0x2 in FBIO_DELAY
    LwU32 privSrc = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DELAY);
    LwU32 privSrcP0 =  FLD_SET_DRF_NUM(_PFB_FBPA_FBIO, _DELAY, _PRIV_SRC, 0x2, privSrc);
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_DELAY,privSrcP0);

    memoryMoveVrefCodeTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefCodeCtrlRb[0], REGISTER_READ);
    memoryMoveVrefDfeTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefDfeCtrlRb[0], REGISTER_READ);
    memoryMoveVrefDfeG6xTrainingValues_HAL(&Fbflcn, (LwU32*)&gRD.vrefDfeG6xCtrlRb[0], REGISTER_READ);

    // switch back priv source to original state
    REG_WR32_STALL(BAR0, LW_PFB_FBPA_FBIO_DELAY,privSrc);

    LwU32 ci = 0;
    LwU16 cmdErr = 0;
    LwU16 cmdZero = 0;
    for (ci=0;ci<TOTAL_CMD_REGS;ci++)
    {
        if ((gRD.cmdDelay[ci] & gRD.cmdDelayRb[ci]) == gRD.cmdDelayRb[ci])
        {
            gRD.cmdDelayDiff[ci] = 0x11111111;
        }
        else
        {
            gRD.cmdDelayDiff[ci] = 0xffffff11;
            cmdErr++;
        }
        if ((gRD.cmdDelay[ci] == 0x0) || (gRD.cmdDelayRb[ci] == 0x0))
        {
            gRD.cmdDelayDiff[ci] = 0x00000011;
            cmdZero++;
        }
    }
    LwU16 vrefErr = 0;
    LwU16 vrefZero = 0;
    for (ci=0;ci<TOTAL_VREF_CODE_CTRLS;ci++)
    {
        if ((gRD.vrefCodeCtrl[ci] & gRD.vrefCodeCtrlRb[ci]) == gRD.vrefCodeCtrlRb[ci])
        {
            gRD.vrefCodeCtrlDiff[ci] = 0x22222222;
        }
        else
        {
            gRD.vrefCodeCtrlDiff[ci] = 0xffffff22;
            vrefErr++;
        }
        if ((gRD.vrefCodeCtrl[ci] == 0x0) || (gRD.vrefCodeCtrlRb[ci] == 0x0))
        {
            gRD.vrefCodeCtrlDiff[ci] = 0x00000022;
            vrefZero++;
        }
    }

    LwU16 dfeErr = 0;
    LwU16 dfeZero = 0;
    for (ci=0;ci<TOTAL_VREF_DFE_CTRLS;ci++)
    {
        if ((gRD.vrefDfeCtrl[ci] & gRD.vrefDfeCtrlRb[ci]) == gRD.vrefDfeCtrlRb[ci])
        {
            gRD.vrefDfeCtrlDiff[ci] = 0x33333333;
        }
        else
        {
            gRD.vrefDfeCtrlDiff[ci] = 0xffffff33;
            dfeErr++;
        }
        if ((gRD.vrefDfeCtrl[ci] == 0x0) || (gRD.vrefDfeCtrlRb[ci] == 0x0))
        {
            gRD.vrefDfeCtrlDiff[ci] = 0x00000033;
            dfeZero++;
        }
    }

    LwU16 dfeG6XErr = 0;
    LwU16 dfeG6XZero = 0;
    for (ci=0;ci<TOTAL_VREF_DFEG6X_CTRLS;ci++)
    {
        if ((gRD.vrefDfeG6xCtrl[ci] & gRD.vrefDfeG6xCtrlRb[ci]) == gRD.vrefDfeG6xCtrlRb[ci])
        {
            gRD.vrefDfeG6xCtrlDiff[ci] = 0x44444444;
        }
        else
        {
            gRD.vrefDfeG6xCtrlDiff[ci] = 0xffffff44;
            dfeG6XErr++;
        }
        if ((gRD.vrefDfeG6xCtrl[ci] == 0x0) || (gRD.vrefDfeG6xCtrlRb[ci] == 0x0))
        {
            gRD.vrefDfeG6xCtrlDiff[ci] = 0x00000044;
            dfeG6XZero++;
        }
    }
    LwBool error = (dfeG6XZero || dfeG6XErr || dfeZero || dfeErr || vrefZero || vrefErr || cmdZero || cmdErr);
    if (error)
    {
        FW_MBOX_WR32(10, (cmdZero << 16) | cmdErr);
        FW_MBOX_WR32(11, (vrefZero << 16) | vrefErr);
        FW_MBOX_WR32(12, (dfeZero << 16) | dfeErr);
        FW_MBOX_WR32(13, (dfeG6XZero << 16) | dfeG6XErr);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_TRAINING_SELFTEST_FAILURE);
    }
}
*/

#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM)) || FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE)

void storeShaHash
(
        void 
)
{
    LwU32* pShaHash = &gShaHash[0];
    // save hash to always on register
    // check for access right to gc6 island scratch register
    checkGroupPlm(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_22_PRIV_LEVEL_MASK);
    // write the hash to registers on the gc6 island
    LwU8 inx;
    for (inx=0; inx<SHA_HASH_REGISTERS; inx++)
    {
        REG_WR32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_22(inx),*(pShaHash++));
    }
}

void compareToShaHash
(
        LwU32* pShaHash
)
{
    // save hash to always on register
    // check for access right to gc6 island scratch register
    checkGroupPlm(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_22_PRIV_LEVEL_MASK);
    // read the hash from registers on the gc6 island
    LwU8 inx;
    for (inx=0; inx<SHA_HASH_REGISTERS; inx++)
    {
        LwU32 shaData = REG_RD32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_22(inx));
        if (shaData != *(pShaHash))
        {
            FW_MBOX_WR32_STALL(11, shaData);
            FW_MBOX_WR32_STALL(12, *(pShaHash));
            FW_MBOX_WR32_STALL(13, inx);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_TRAINING_SHA256_MISSMATCH);
        }
        pShaHash++;
    }
}

#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))

void checkGroupPlm(LwU32 plmAdr)
{
    // plm check only exelwted when priv sec is enabled
    if (gPrivSecEn == LW_FUSE_OPT_PRIV_SEC_EN_DATA_NO)
    {
        return;
    }
    LwU32 regPlm = REG_RD32(BAR0, plmAdr);

    LwU8 access = 0;
    if (gFbflcnLsMode)
    {
        access = FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_12_PRIV_LEVEL_MASK,
                _WRITE_PROTECTION_LEVEL2,  _ENABLE, regPlm);
    }
    else
    {
        access = FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_12_PRIV_LEVEL_MASK,
                _WRITE_PROTECTION_LEVEL0,  _ENABLE, regPlm);

    }
    if (access == 0)
    {
        FW_MBOX_WR32(11, gFbflcnLsMode);
        FW_MBOX_WR32(12, regPlm);
        FW_MBOX_WR32(13, plmAdr);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_PRIV_PGC6_AON_PLM_WRITE_VIOLATION_ERROR);
    }
}

//
// Prerequisites for sending FB falcon boot train data information through scratches
// only when both PMU and FB falcon have sent the INIT message to RM, in
// LW_PFBFALCON_FIRMWARE_MAILBOX(2)[31:0]: address[63:32] -> addr_hi
// LW_PFBFALCON_FIRMWARE_MAILBOX(3)[31:0]: address[31:0]  -> addr_lo
// LW_PFBFALCON_FIRMWARE_MAILBOX(4)[7:0] : media type
// LW_PFBFALCON_FIRMWARE_MAILBOX(5)[7:0] : buffer size
//
// the buffer location is then held in secure registers accross a gc6 exit
// addr_lo  -> AON..GROUP_12(0) [31:0]
// addr_hi  -> AON..GROUP_12(1) [31:0]
// media type  -> AON..GROUP_13(0)[7:0]
// buffer size -> AON..GROUP_13(1)
// info_field  -> AON..GROUP_23(0) [31:24]


void writeAon13ScratchInfo(void)
{
    LwU32 aon13_0 = REG_RD32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(0));
    LwU32 media = REF_VAL(AON_SELWRE_SCRATCH_GROUP13_MEDIA_FIELD, aon13_0);

    LwU16* pInfo = (LwU16*)&gAonStateInfo;

    LwU32 aonGroup13_0 = REF_NUM(AON_SELWRE_SCRATCH_GROUP13_MEDIA_FIELD, media) |
                         REF_NUM(AON_SELWRE_SCRATCH_GROUP13_INFO_FIELD, *(pInfo));
    REG_WR32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(0), aonGroup13_0);
}

void readAonStateInfo(void)
{
    checkGroupPlm(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13_PRIV_LEVEL_MASK);

    LwU32 aon13_0 = REG_RD32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(0));
    LwU32 info = REF_VAL(AON_SELWRE_SCRATCH_GROUP13_INFO_FIELD, aon13_0);

    LwU16* pInfo = (LwU16*)&gAonStateInfo;
    *(pInfo) = info;
}

#endif

#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM)) || (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
// Implemention: http://lwbugs/200560699
//
LwBool setupGC6IslandRegisters(void)
{
    LwU32 bufferSize;
    bufferSize = REG_RD32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(5));
    if (bufferSize == 0)
    {
        return LW_FALSE;
    }

    LwU32 adrLow = REG_RD32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(3));
    REG_WR32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(0),adrLow);

    LwU32 adrHi = REG_RD32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(2));
    REG_WR32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(1),adrHi);

    // copy media field to aon scratch group 13(0)
    // extract info fields and save it back without touching
    LwU32 mediaType = REG_RD32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(4));
    LwU32 aon13Info = REF_VAL(AON_SELWRE_SCRATCH_GROUP13_INFO_FIELD,
            REG_RD32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(0)));

    LwU32 aonGroup13_0 = REF_NUM(AON_SELWRE_SCRATCH_GROUP13_MEDIA_FIELD, mediaType) |
            REF_NUM(AON_SELWRE_SCRATCH_GROUP13_INFO_FIELD, aon13Info);
    REG_WR32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(0), aonGroup13_0);

    REG_WR32(BAR0,LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(1),bufferSize);

    return LW_TRUE;
}


void setupRegionCfgForSysMemBuffer
(
        void
)
{
    LwU32 data = 0;
    LwU8 ctxDma = CTX_DMA_FBFALCON_FOR_SYSMEM_STORAGE;
    LwU8 wprId = 0;

    LwU32 mediaType = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(0)) & 0xff;

    if (mediaType == ADDR_FBMEM) {
        //lwr->cfg0 = FLD_SET_DRF_NUM(_PFB,_FBPA_CFG0,_DRAM_ACK,enable,lwr->cfg0);
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_TARGET , _LOCAL_FB, data);
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_MEM_TYPE, _PHYSICAL, data);
    }
    else if (mediaType == ADDR_SYSMEM) {
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_TARGET, _NONCOHERENT_SYSMEM, data);
        data = FLD_SET_DRF(_CFBFALCON,_FBIF_TRANSCFG,_MEM_TYPE, _PHYSICAL, data);
    }
    else
    {
        FW_MBOX_WR32(8, REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13_PRIV_LEVEL_MASK) );
        FW_MBOX_WR32(9, REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(0)) );
        FW_MBOX_WR32(10, REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(1)) );
        FW_MBOX_WR32(5, REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12_PRIV_LEVEL_MASK) );
        FW_MBOX_WR32(6, REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(0)) );
        FW_MBOX_WR32(7, REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(1)) );
        FW_MBOX_WR32(11, ADDR_FBMEM);
        FW_MBOX_WR32(12, ADDR_SYSMEM);
        FW_MBOX_WR32(13, mediaType);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_SAVE_AND_RESTORE_UNEXPECTED_MEDIA_TYPE);
    }
    REG_WR32_STALL(CSB, LW_CFBFALCON_FBIF_TRANSCFG(ctxDma), data);

    //write status
    FW_MBOX_WR32_STALL(15, REG_RD32(CSB, LW_CFBFALCON_FBIF_TRANSCFG(ctxDma)) );

    // Setup - Step 2.  Set FBIF_REGIONCFG for WPR
    
    LwU32 mask = ~ ( 0xF << (ctxDma*4) );

    data = REG_RD32(CSB, LW_CFBFALCON_FBIF_REGIONCFG);
    data = (data & mask) | ((wprId & 0xF) << (ctxDma * 4));
    REG_WR32_STALL(CSB, LW_CFBFALCON_FBIF_REGIONCFG,  data);
}

/*
void debugCall2(LwU32 loc)
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
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        addr = CSB_REG_RD32(LW_CFBFALCON_FIRMWARE_MAILBOX(5));
        if (addr <= 0x10000)
        {
            LwU32 block = addr / 0x100;
            REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMT(0), block);
            REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMC(0),
                    REF_NUM(LW_PPAFALCON_FALCON_DMEMC_OFFS, ((addr % 0x100)/ 4) ) |
                    REF_NUM(LW_PPAFALCON_FALCON_DMEMC_BLK, block) |
                    REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCW, 1) |
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

LwBool
dataSaveToSysmemCarveout
(
        void
)
{

    LwU8 ctxDma = CTX_DMA_FBFALCON_FOR_SYSMEM_STORAGE;

    //write status
    FW_MBOX_WR32_STALL(15, REG_RD32(CSB, LW_CFBFALCON_FBIF_REGIONCFG) );

    //access to aon scratch group is checked at init
    LwU32 bufAdrLo = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(0));
    LwU32 bufAdrHi = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(1));

    // size of system memory aperture
    LwU32 carveoutSize = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(1));


    // program as base address for dma access through fbif
    // reducing to 256B block size
    falc_wspr(DMB, (( (bufAdrLo & 0xFFFFFF00) >> 8) |
            ( (bufAdrHi & 0x000000FF) << 24)));
    falc_wspr(DMB1, ( (bufAdrHi & 0xFFFFFF00) >> 8));

    LwU16 ctx = 0;
    falc_rspr(ctx, CTX);  // Read existing value
    ctx = (((ctx) & ~(0x7 << CTX_DMAIDX_SHIFT_DMWRITE)) | ((ctxDma) << CTX_DMAIDX_SHIFT_DMWRITE));
    falc_wspr(CTX, ctx); // Write new value

    // sha will run only on the effectiv size of the structure and exclude
    // the padding
    LwU32 size = (sizeof(REGISTER_DATA) + (sizeof(LwU32) - 1)) / sizeof(LwU32);
    // fbif operations are run on the whole 256B blocks for speed
    LwU32 size256 = (sizeof(REGISTER_DATA) + 0xFF) / FALCON_MEM_BLOCK_SIZE;

    // do check that we will not overrun the mwpr area
    LwBool bError;
    bError = ((size * sizeof(LwU32)) >= carveoutSize);

    if (bError)
    {
        FW_MBOX_WR32_STALL(9, sizeof(REGISTER_DATA));
        FW_MBOX_WR32_STALL(10, carveoutSize);
        FW_MBOX_WR32_STALL(11, size);
        FW_MBOX_WR32_STALL(12, bufAdrLo);
        FW_MBOX_WR32_STALL(13, bufAdrHi);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_TRAINING_INSUFFICIENT_ACCESS_SYS_SAVE);
    }

    // create sha hash for the data buffer
    LwU8* devInfo = (LwU8*)&gShaHash;

    // exclude the save if in RTL
    if (gPlatformType != LW_PTOP_PLATFORM_TYPE_RTL)
    {
        sha256(devInfo,(LwU8*)&gRD,sizeof(REGISTER_DATA));
    }

    storeShaHash();

    // The dmread means read data from DMA interface to DMEM while dmwrite
    // write data from DMEM to DMA interface. Both of these instruction has
    // two operands and being used as "dmread s1 s2" or "dmwrite s1 s2", where
    // enc_size = s2[18:16]
    //dsize = (1<<enc_size)*4 bytes = (1 << (enc_size+2)) bytes
    //fb_offset = s1 & (dsize-1)
    //dm_offset = s2 & 0x0000ffff & (dsize-1)

    LwU32 s1 = 0x0;
    LwU32 s2 = (LwU32)&gRD;

    LwU32 i=0;

    for (i=0; i<size256; i++)
    {
        // bits 18:16 of second operand of dmread defines the actual size of
        // transfer (6 = chunk of 256, 0 = chunk of 4)
        falc_dmwrite(s1,s2 + (6<<16));
        s1 += FALCON_MEM_BLOCK_SIZE;
        s2 += FALCON_MEM_BLOCK_SIZE;
        //FW_MBOX_WR32(1, s1);
        //FW_MBOX_WR32(2, s2);
        //FW_MBOX_WR32(3, size256);
        //FW_MBOX_WR32(4, sizeof(REGISTER_DATA));
        //debugCall2(0x70000000 | i);
    }
    falc_dmwait();

    // save restored debug data
    if (gAonStateInfo.dataInPaFalconMemory)
    {
        s2 = (LwU32)&gDD[0];
        // OPTIMIZATION:  probably don't need to write the full 8K of buffer to sysmem,
        //   lwrrently there's only a bit over 3K in there from debug data from the
        //   bootime training
        size256 = DB_BYTE_SIZE / 256;
        for (i=0; i<size256; i++)
        {
            // bits 18:16 of second operand of dmread defines the actual size of
            // transfer (6 = chunk of 256, 0 = chunk of 4)
            falc_dmwrite(s1,s2 + (6<<16));
            s1 += FALCON_MEM_BLOCK_SIZE;
            s2 += FALCON_MEM_BLOCK_SIZE;
            //FW_MBOX_WR32(1, s1);
            //FW_MBOX_WR32(2, s2);
            //FW_MBOX_WR32(3, size256);
            //FW_MBOX_WR32(4, sizeof(REGISTER_DATA));
            //debugCall2(0x90000000 | i);
        }
        falc_dmwait();
    }

    //to notify tb that data save and restore is done
    REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDA7A);

    LwBool success = !bError;
    return success;
}


LwBool
dataReadFromSysmemCarveout
(
        void
)
{
    LwU8 ctxDma = CTX_DMA_FBFALCON_FOR_SYSMEM_STORAGE;

    //write status
    FW_MBOX_WR32_STALL(15, REG_RD32(CSB, LW_CFBFALCON_FBIF_REGIONCFG) );

    // start address system memory aperture
    LwU32 bufAdrLo = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(0));
    LwU32 bufAdrHi = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_12(1));

    // size of system memory aperture
    LwU32 carveoutSize = REG_RD32(BAR0, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_13(1)) ;

    LwU32 dataStart_lo = bufAdrLo;
    LwU32 dataStart_hi = bufAdrHi;

    // program as base address for dma access through fbif
    // reducing to 256B block size
    falc_wspr(DMB, (( (dataStart_lo & 0xFFFFFF00) >> 8) |
            ( (dataStart_hi & 0x000000FF) << 24)));
    falc_wspr(DMB1, ( (dataStart_hi & 0xFFFFFF00) >> 8));

    LwU16 ctx = 0;
    falc_rspr(ctx, CTX);  // Read existing value
    ctx = (((ctx) & ~(0x7 << CTX_DMAIDX_SHIFT_DMREAD)) | ((ctxDma) << CTX_DMAIDX_SHIFT_DMREAD));
    falc_wspr(CTX, ctx); // Write new value

    // sha will run only on the effectiv size of the structure and exclude
    // the padding
    LwU32 size = (sizeof(REGISTER_DATA) + (sizeof(LwU32) - 1)) / sizeof(LwU32);
    // fbif operations are run on the whole 256B blocks for speed
    LwU32 size256 = (sizeof(REGISTER_DATA) + 0xFF) / FALCON_MEM_BLOCK_SIZE;

    // do check that we will not overrun the mwpr area
    LwBool bError;
    bError = ((size * sizeof(LwU32)) >= carveoutSize);

    if (bError)
    {
        FW_MBOX_WR32_STALL(10, carveoutSize);
        FW_MBOX_WR32_STALL(11, size);
        FW_MBOX_WR32_STALL(12, bufAdrLo);
        FW_MBOX_WR32_STALL(13, bufAdrHi);
        if (bError)
        {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_TRAINING_DATA_EXCEEDS_SYSMEM_SIZE);
        }
    }

    LwU32 s1 = 0;
    LwU32 s2 = (LwU32)&gRD;

    LwU16 i=0;
    for (i=0; i<size256; i++)
    {
        //
        // bits 18:16 of second operand of dmread defines the actual size of
        // transfer (6 = chunk of 256, 0 = chunk of 4)

        falc_dmread(s1,s2 + (6<<16) );
        s1 += FALCON_MEM_BLOCK_SIZE;
        s2 += FALCON_MEM_BLOCK_SIZE;
    }
    falc_dmwait();

    // do sha256 hash and compare to reference
    if (gPlatformType != LW_PTOP_PLATFORM_TYPE_RTL)
    {
        LwU8* devInfo = (LwU8*)&gShaHash;
        sha256(devInfo,(LwU8*)&gRD,sizeof(REGISTER_DATA));
    }

    // compare the hash to the registers on the gc6 island
    compareToShaHash((LwU32*)&gShaHash[0]);

    //to notify tb that data save and restore is done
    REG_WR32(BAR0,LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDA8A);

    LwBool success = !bError;
    return success;
}
#endif  // (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR))
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER)) || FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE)


//
// The Falcons perform an automatic scrubbing process on the first priv access after a reset. Scrubbing will
// clear IMEM and DMEM to zero, but it will block all access to the core. This functions makes sure we wait
// for scrubbing to complete before we boot the pafalcon core
//
void waitForPAFalconScrubbing
(
        void
)
{
    FLCN_TIMESTAMP   timeStart;
    osPTimerTimeNsLwrrentGet(&timeStart);
    LwU32 timeoutns = 10000000;  // give it a max of 10ms to scrub

    LwU8 scrubbingDone = 0;

    do
    {
        LwU32 dmactl = REG_RD32(BAR0, LW_PPAFALCON_FALCON_DMACTL);
        LwU8 scrubbingDMEM;
        scrubbingDMEM = REF_VAL(LW_PPAFALCON_FALCON_DMACTL_DMEM_SCRUBBING, dmactl);
        LwU8 scrubbingIMEM;
        scrubbingIMEM = REF_VAL(LW_PPAFALCON_FALCON_DMACTL_IMEM_SCRUBBING, dmactl);
        scrubbingDone = (scrubbingDMEM == LW_PPAFALCON_FALCON_DMACTL_DMEM_SCRUBBING_DONE) &&
                (scrubbingIMEM == LW_PPAFALCON_FALCON_DMACTL_IMEM_SCRUBBING_DONE);
        if (osPTimerTimeNsElapsedGet(&timeStart) > timeoutns)
        {
            LwU32 elapsed = osPTimerTimeNsElapsedGet(&timeStart);
            FW_MBOX_WR32_STALL(6,timeoutns);
            FW_MBOX_WR32_STALL(7,timeStart.parts.lo);
            FW_MBOX_WR32_STALL(8,timeStart.parts.hi);
            FW_MBOX_WR32_STALL(9,scrubbingDone);
            FW_MBOX_WR32_STALL(10,elapsed);
            FW_MBOX_WR32_STALL(11,scrubbingDMEM);
            FW_MBOX_WR32_STALL(12,scrubbingIMEM);
            FW_MBOX_WR32_STALL(13,scrubbingIMEM);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_MEM_SCRUB_TIMEOUT);
        }
    }
    while (scrubbingDone == 0);
}

void dmemReadPAFalcon
(
        LwU32* pDestStart,
        LwU32 sourceSize
)
{
    LwU32* pCode = pDestStart;

    LwU32 destStartDmem = 0;
    LwU32 destStartBlockDmem = destStartDmem / FALCON_MEM_BLOCK_SIZE;
    if (sourceSize > 8192)
    {
        FW_MBOX_WR32_STALL(13,sourceSize);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_PAFALCON_BINARY_DMEM_OVERFLOW_ERROR);
    }

    LwU32 destEndBlockDmem = destStartBlockDmem +
            ((sourceSize + FALCON_MEM_BLOCK_SIZE - 1) / FALCON_MEM_BLOCK_SIZE);

    LwU32 i;

    for (i=destStartBlockDmem; i<destEndBlockDmem ; i++) {
        REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMT(0),i);
        int j;
        for (j = 0x0; j<FALCON_MEM_BLOCK_SIZE/4; j++) {
            if (j==0)
            {
                REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMC(0),
                        REF_NUM(LW_PPAFALCON_FALCON_DMEMC_OFFS, j) |
                        REF_NUM(LW_PPAFALCON_FALCON_DMEMC_BLK, i) |
                        REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCW, 1) |
                        REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCR, 1)
                );
            }
            LwU32 data = REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0));
            *(pCode++) = data;
        }

    }
}

void dmemWritePAFalcon (
        LwU32* pSourceStart,
        LwU32 sourceSize,
        LwU32 destAddr
)
{
    LwU32 sizeDmem = destAddr + sourceSize;
    if (sizeDmem > 8192)
    {
        FW_MBOX_WR32_STALL(11,0x1);
        FW_MBOX_WR32_STALL(12,destAddr);
        FW_MBOX_WR32_STALL(13,sourceSize);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_PAFALCON_BINARY_DMEM_OVERFLOW_ERROR);
    }
    REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMC(0),
            REF_NUM(LW_PPAFALCON_FALCON_DMEMC_ADDRESS, destAddr) |
            REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCW, 1) |
            REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCR, 0)
    );
    LwU32 i;
    LwU32* pSource = pSourceStart;
    for (i=0; i< (sourceSize+3)/4; i++) {
        REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMD(0), *(pSource++));
    }
}


void dmemWritePAFalconWithCheck
(
        LwU32* pSourceStart,
        LwU32 sourceSize,
        LwBool readbackCheck
)
{
    LwU8 reading;
    LwU8 readingLoop = 0;
    if (readbackCheck) {
        readingLoop = 1;
    }

    for (reading=0; reading<=readingLoop; reading++)
    {
        LwU32* pCode = pSourceStart;

        LwU32 destStartBlockDmem = 0;
        LwU32 sizeDmem = sourceSize;
        if (sizeDmem > 8192)
        {
            FW_MBOX_WR32_STALL(11,0x2);
            FW_MBOX_WR32_STALL(13,sourceSize);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_PAFALCON_BINARY_DMEM_OVERFLOW_ERROR);
        }

        LwU32 destEndBlockDmem = destStartBlockDmem +
                ((sizeDmem + FALCON_MEM_BLOCK_SIZE - 1) / FALCON_MEM_BLOCK_SIZE);

        LwU32 i;

        for (i=destStartBlockDmem; i<destEndBlockDmem ; i++) {
            REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMT(0),i);
            int j;
            int skipped = 0;
            for (j = 0x0; j<FALCON_MEM_BLOCK_SIZE/4; j++) {
                // get data from buffer and offset by 2 (to strip header and crc)
                LwU32 data = *(pCode++);
                // skip writing when data is 0 and when its not the last address
                // the last write is necessary to register the tag for the block
                if ((data == 0x0) && (reading == 0) && (j < (FALCON_MEM_BLOCK_SIZE/4 - 1))) {
                    skipped = 1;
                    continue;
                }
                if ((j==0) || (skipped == 1))
                {
                    REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMC(0),
                            REF_NUM(LW_PPAFALCON_FALCON_DMEMC_OFFS, j) |
                            REF_NUM(LW_PPAFALCON_FALCON_DMEMC_BLK, i) |
                            REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCW, 1) |
                            REF_NUM(LW_PPAFALCON_FALCON_DMEMC_AINCR, 1)
                    );
                    skipped = 0;
                }
                if (reading)
                {
                    LwU32 dd = REG_RD32(CSB, LW_PPAFALCON_FALCON_DMEMD(0));  // 9a31c4
                    if (data != dd) {
                        FW_MBOX_WR32_STALL(10,data);
                        FW_MBOX_WR32_STALL(11,j);
                        FW_MBOX_WR32_STALL(12,i);
                        FW_MBOX_WR32_STALL(13,1);  // indicate dmem
                        FBFLCN_HALT(FBFLCN_ERROR_CODE_PAFALCON_BINARY_DOWNLOAD_ERROR);
                    }
                }
                else
                {
                    REG_WR32(CSB, LW_PPAFALCON_FALCON_DMEMD(0), data);  // 9a31c4
                }
            }
        }
    }
}

void
copyDebugBuffer(void)
{
    memcpy((LwU8*)&gDD[0],(LwU8*)&gDB, min(gAonStateInfo.debugDataBlks * 0x100,DB_BYTE_SIZE) );
}

#endif

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
//
// boot load PA Falcon binary through priv from fbfalcon dmem to pafalcon imem/dmem
//  load operation is followed by a read back and check to guarantee binary is alwrate
//
LwU32 extern _pa_code_end;
LwU32 extern _pa_code_start;
LwU32 extern _pa_data_end;
LwU32 extern _pa_data_start;
LwU32 extern _pa_desc_end;
LwU32 extern _pa_desc_start;
LwU32 extern _pa_start;
LwU32 extern _pa_end;

void bootloadPAFalcon
(
		void
)
{

    // parse the top of the data buffer for desc information.
    LwU32* pBinaryPtr = &_pa_start;
    LwU32 codeId = *(pBinaryPtr++);
    LwU32 residentCodeStart = *(pBinaryPtr++);
    LwU32 residentCodeEnd = *(pBinaryPtr++);
    LwU32 residentDataSize = *pBinaryPtr;

    FW_MBOX_WR32_STALL(9, codeId);
    FW_MBOX_WR32_STALL(10,residentCodeStart);
    FW_MBOX_WR32_STALL(11,residentCodeEnd);
    FW_MBOX_WR32_STALL(12,residentDataSize);

    // check the segement it at the top of the binary, once the space is reused
    // for data etc we can not proceed.
    if (codeId != _ovly_table[PAFLCN_SEGMENT][IDENTIFIER]) {
            FW_MBOX_WR32_STALL(11,_ovly_table[PAFLCN_SEGMENT][IDENTIFIER]);
            FW_MBOX_WR32_STALL(12,codeId);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_PAFALCON_BINARY_HEADER_ERROR);
        }


    // in this programming model code has to start at 0, if this fails then check if the
    // uploaded binary might include a bootloader by mistake.
    if (residentCodeStart != 0)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_PAFALCON_DESC_ERROR);
    }

    LwU8 reading;
    for (reading=0; reading<2; reading++)
    {
        LwU32 destStartImem = 0;
        LwU32 destStartBlockImem = destStartImem / FALCON_MEM_BLOCK_SIZE;
        LwU32 size = (LwU32)&_pa_code_end - (LwU32)&_pa_code_start;
        LwU32 destEndBlockImem = destStartBlockImem +
                ((size + FALCON_MEM_BLOCK_SIZE - 1) / FALCON_MEM_BLOCK_SIZE);

        // skip over the descriptor
        LwU32* pCode = &_pa_code_start;

        // using autoincrement with skipped loading if the data content is 0 (which is fairly common for
        // the data section).  auto increment requires access to IMEMC only at the beginning of the block,
        // but skipping an entry will break the auto increment, so each time we skip an entry IMEMC has to
        // be written again.  (In other words reduction in priv traffic is only achieved if more than 2 fields
        // are 0 back to back)
        LwU32 i;
        for (i=destStartBlockImem; i < destEndBlockImem; i++) {
            REG_WR32(CSB, LW_PPAFALCON_FALCON_IMEMT(0),i);      // 9a3188
            int j;
            int skipped = 0;
            for (j = 0x0; j<FALCON_MEM_BLOCK_SIZE/4; j++) {
                // get instruction data from buffer
                LwU32 data = *(pCode++);
                if ((data == 0x0) && (reading == 0) && (j < (FALCON_MEM_BLOCK_SIZE/4 - 1))) {
                    skipped = 1;
                    continue;
                }
                if ((j==0) || (skipped == 1))
                {
                    REG_WR32(CSB, LW_PPAFALCON_FALCON_IMEMC(0),   // 9a3180
                            REF_NUM(LW_PPAFALCON_FALCON_IMEMC_OFFS, j) |
                            REF_NUM(LW_PPAFALCON_FALCON_IMEMC_BLK, i) |
                            REF_NUM(LW_PPAFALCON_FALCON_IMEMC_AINCW, 1) |
                            REF_NUM(LW_PPAFALCON_FALCON_IMEMC_AINCR, 1) |
                            REF_NUM(LW_PPAFALCON_FALCON_IMEMC_SELWRE, 0)
                    );
                    skipped = 0;
                }
                if (reading)
                {
                    LwU32 dd = REG_RD32(CSB, LW_PPAFALCON_FALCON_IMEMD(0));  // 9a3184
                    if (data != dd) {
                        FW_MBOX_WR32_STALL(10,data);
                        FW_MBOX_WR32_STALL(11,j);
                        FW_MBOX_WR32_STALL(12,i);
                        FW_MBOX_WR32_STALL(13,0);  // indicate imem
                        FBFLCN_HALT(FBFLCN_ERROR_CODE_PAFALCON_BINARY_DOWNLOAD_ERROR);
                    }
                }
                else
                {
                    REG_WR32(CSB, LW_PPAFALCON_FALCON_IMEMD(0), data);  // 9a3184
                }
            }
        }
    }

    LwU32 sizeDmem = (LwU32)&_pa_data_end - (LwU32)&_pa_data_start;
    dmemWritePAFalconWithCheck(&_pa_data_start,sizeDmem,LW_TRUE);
    
    LwU32 dmemc;
    LwU32 i;
    LwU32 seqInfoSize = sizeof(PAFLCN_SEQUENCER_INFO_FIELD)/4;

    dmemc = REG_RD32(BAR0, LW_PPAFALCON_FALCON_DMEMC(0));
    // set auto increment write
    //need to set BLK, OFFS, SETTAG, SETLVL?
    dmemc = FLD_SET_DRF(_PPAFALCON_FALCON, _DMEMC, _AINCW, _TRUE, dmemc);
    dmemc = FLD_SET_DRF(_PPAFALCON_FALCON, _DMEMC, _VA, _FALSE, dmemc);  // needed?
    dmemc = FLD_SET_DRF_NUM(_PPAFALCON_FALCON, _DMEMC, _ADDRESS, _pafalcon_location_table[SEQUENCER_INFO], dmemc);
    REG_WR32(LOG, LW_PPAFALCON_FALCON_DMEMC(0), dmemc);

    for (i = 0; i < MAX_PARTS; i++)
    {
        if (isPartitionEnabled(i))
        {
            LwU32 j;
            LwU32 seqInfoField[seqInfoSize];
            LwU32 dmemdAddr = unicast(LW_PPAFALCON_FALCON_DMEMD(0),i);    // base=9a31c4

            if (fbfalconIsHalfPartitionEnabled_HAL(&Fbflcn,i))
            {
                seqInfoField[0] = 0x1;
            }
            else
            {
                seqInfoField[0] = 0;    // full partition
            }
            seqInfoField[1] = i;

            for (j = 0; j < seqInfoSize; j++)
            {
		        FW_MBOX_WR32((j+1), seqInfoField[j]);
                REG_WR32(LOG, dmemdAddr, seqInfoField[j]);
            }
        }
    }
}


void
startPaFalcon
(
        void
)
{
    // clear block bind
    REG_WR32_STALL(BAR0,LW_PPAFALCON_FALCON_DMACTL, 0x0);
    // set the bootvector to 0
    REG_WR32_STALL(BAR0, LW_PPAFALCON_FALCON_BOOTVEC, 0x0);
    REG_WR32_STALL(BAR0, LW_PPAFALCON_FALCON_CPUCTL, REF_NUM(LW_PPAFALCON_FALCON_CPUCTL_STARTCPU, LW_PPAFALCON_FALCON_CPUCTL_STARTCPU_TRUE));
}


void
verifyPaFalcon
(
        void
)
{
    LwU8 p;
    LwU32 sync_status;
    LwU32 start_time;
    LwU32 partition_mask = 0;
    LwU32 done_mask = 0;

#define TIMER_10US          10000
#define PA_FALCON_STARTED  0x0002  

    for (p = 0; p < MAX_PARTS; p++)
    {
        if (isPartitionEnabled(p))
        {
            sync_status = REG_RD32(BAR0, unicast(LW_PPAFALCON_SYNC_STATUS,p));
            partition_mask |= (1 << p);

            FW_MBOX_WR32(p+2, sync_status);
            if (sync_status == PA_FALCON_STARTED)
            {
                done_mask |= (1 << p);
            }
        }
    }

    FW_MBOX_WR32(8, done_mask);
    start_time = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);

    while (done_mask != partition_mask)
    {
        for (p = 0; p < MAX_PARTS; p++)
        {
            if (((done_mask & (1<<p))==0) && isPartitionEnabled(p))
            {
                sync_status = REG_RD32(BAR0, unicast(LW_PPAFALCON_SYNC_STATUS,p));
                FW_MBOX_WR32(p+2, sync_status);

                if (sync_status == PA_FALCON_STARTED)
                {
                    done_mask |= (1 << p);
                }
            }
        }

        LwU32 end_time = REG_RD32(CSB, LW_CFBFALCON_FALCON_PTIMER0);
        if ((end_time - start_time) > TIMER_10US)
        {
            FW_MBOX_WR32(9, partition_mask);
            FW_MBOX_WR32(10, done_mask);
            FW_MBOX_WR32(11, start_time);
            FW_MBOX_WR32(12, end_time);
            FW_MBOX_WR32(13, (end_time - start_time));
            FBFLCN_HALT(FBFLCN_ERROR_CODE_PAFALCON_START_ERROR);
        }
    }
}
#endif  // #if(FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))

