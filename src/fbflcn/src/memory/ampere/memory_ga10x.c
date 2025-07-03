/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

#include "fbflcn_helpers.h"
#include "fbflcn_defines.h"
#include "priv.h"
#include "data.h"

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_fbpa.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pwr_pri.h"
#include "dev_disp.h"
#include "dev_fuse.h"
#include "dev_top.h"
#include "fbflcn_hbm_mclk_switch.h"
#include "memory_gddr.h"
#include "osptimer.h"
#include "memory.h"
#include "config/g_memory_hal.h"

#include "priv.h"

extern REGISTER_DATA gRD;

#if (FBFALCONCFG_CHIP_ENABLED(GA10X))
// extend the max vblank timeout for bug 200637376 on ampere due to reprogramming
// for mscg vs vblank timer making it possible to miss the first vblank.
#define SEQ_VBLANK_TIMEOUT_NS ((45478) * 1000 * 2)
#else
#define SEQ_VBLANK_TIMEOUT_NS ((45478) * 1000)
#endif

GCC_ATTRIB_SECTION("memory", "memoryWaitForOkToSwitchVal")
void
memoryWaitForDisplayOkToSwitchVal_GA10X
(
		LwU8 target
)
{
    FLCN_TIMESTAMP  waitTimerNs = { 0 };
    osPTimerTimeNsLwrrentGet(&waitTimerNs);

	LwU32 mb0;
	LwU8 ok_to_switch = LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT;
	do {
		mb0 = REG_RD32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(0));
		ok_to_switch = REF_VAL(LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH,mb0);
	}
	while ( (ok_to_switch != target) &&
			(osPTimerTimeNsElapsedGet(&waitTimerNs) < SEQ_VBLANK_TIMEOUT_NS) );
}


GCC_ATTRIB_SECTION("memory", "memoryWaitForDisplayOkToSwitch_GA10X")
void
memoryWaitForDisplayOkToSwitch_GA10X
(
        void
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_CORE_RUNTIME))
    //
    // detect presence of display
    //
	LwBool displayIsPresent = ( REF_VAL(LW_FUSE_STATUS_OPT_DISPLAY_DATA, REG_RD32(BAR0, LW_FUSE_STATUS_OPT_DISPLAY)) == LW_FUSE_CTRL_OPT_DISPLAY_DATA_ENABLE);

	if (displayIsPresent)
	{
		//
		// VGA mode support was simplified for Ampere (bug 2598475)
		//
		LwBool vgaOwner = (REF_VAL(LW_PDISP_FE_CORE_HEAD_STATE_SHOWING_ACTIVE, REG_RD32(BAR0, LW_PDISP_FE_CORE_HEAD_STATE(0))) == LW_PDISP_FE_CORE_HEAD_STATE_SHOWING_ACTIVE_VBIOS);
		if (vgaOwner)
		{
			memoryWaitForDisplayOkToSwitchVal_HAL(&Fbflcn,LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT);
		}
		memoryWaitForDisplayOkToSwitchVal_HAL(&Fbflcn,LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_READY);
	}
#endif

    // halt on timeout
    //   display team requested to continue with mclk switch after a tieout (bug 2169913)
    //   so we no longer halt here.

}



//#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
extern LwU8 gPlatformType;
LwBool gbl_en_fb_mclk_sw = LW_TRUE;
LwBool gbl_fb_use_ser_priv = LW_FALSE;
LwBool gbl_selfref_opt = LW_FALSE;
LwBool gbl_pa_bdr_load = LW_FALSE;
LwU32 gSwConfig;
LwU8 gDDRMode;
//#endif

GCC_ATTRIB_SECTION("init", "memoryReadSwConfig_GA10X")
void
memoryReadSwConfig_GA10X
(
        void
)
{
    gPlatformType = REF_VAL(LW_PTOP_PLATFORM_TYPE,REG_RD32(BAR0,LW_PTOP_PLATFORM));

    gSwConfig = REG_RD32(BAR0, LW_PFB_FBPA_SW_CONFIG);
    if (DRF_VAL(_PFB,_FBPA_SW_CONFIG,_PTRIM_OPT,gSwConfig) != LW_PFB_FBPA_SW_CONFIG_PTRIM_OPT_INIT)
    {
        gbl_fb_use_ser_priv = LW_TRUE;
    }
    else
    {
        gbl_fb_use_ser_priv = LW_FALSE;
    }

    if (DRF_VAL(_PFB,_FBPA_SW_CONFIG,_PA_FALCON_OPT,gSwConfig) != LW_PFB_FBPA_SW_CONFIG_PA_FALCON_OPT_INIT)
    {
        // gbl_en_fb_mclk_sw means the mclk switch is run on the fbfalcon, set this to false to execute
        // with the sequencer on the pa falcon
        gbl_en_fb_mclk_sw = LW_FALSE;
        if (!gbl_fb_use_ser_priv)
        {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_PTRIM_OPT_ERROR);
        }
    }
    else
    {
        gbl_en_fb_mclk_sw = LW_TRUE;
    }
    // TODO: - stefans
    // TEMP fix to override setting for emulation
    // the default setting through the SW Config register is using the fbfalcon for the mclk switch
    // which will be our initial setting on silicon
    // For the emulator on the other side we want the full process with the bootload of the pafalcon, as
    // we don't have any form of plusarg support for this, I hard tie it to the emulation runs.
    if (gPlatformType == LW_PTOP_PLATFORM_TYPE_EMU)
    {
        gbl_en_fb_mclk_sw = LW_FALSE;
        gbl_fb_use_ser_priv = LW_TRUE;
        gSwConfig = FLD_SET_DRF(_PFB_FBPA,_SW_CONFIG,_PTRIM_OPT,_SET, gSwConfig);
        gSwConfig = FLD_SET_DRF(_PFB_FBPA,_SW_CONFIG,_PA_FALCON_OPT,_SET, gSwConfig);
        REG_WR32(BAR0, LW_PFB_FBPA_SW_CONFIG, gSwConfig);
    }


    if (DRF_VAL(_PFB,_FBPA_0_SW_CONFIG,_SELFREF_OPT,gSwConfig) != LW_PFB_FBPA_SW_CONFIG_SELFREF_OPT_INIT)
    {
        gbl_selfref_opt = LW_TRUE;
    }
    else
    {
        gbl_selfref_opt = LW_FALSE;
    }

    if (REF_VAL(LW_CFBFALCON_SYNC_CTRL_SEGMENT,REG_RD32(CSB, LW_CFBFALCON_SYNC_CTRL)) == 0xFF)
    {
        gbl_pa_bdr_load = LW_TRUE;
    }
    else
    {
        gbl_pa_bdr_load = LW_FALSE;
    }

    gDDRMode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST));
}


