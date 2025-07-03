#ifndef FBFLCN_GDDR_MCLK_SWITCH_H
#define FBFLCN_GDDR_MCLK_SWITCH_H

#include <lwtypes.h>
#include "lwuproc.h"

#include "fbflcn_defines.h"
#include "config/fbfalcon-config.h"


#define VBIOS_TABLE_SUPPORT

#ifndef VBIOS_TABLE_SUPPORT
#define LEGACY_STANDSIM_SUPPORT
#endif // ~VBIOS_TABLE_SUPPORT

LwBool GblAddrTrPerformedOnce;
LwBool gSaveRequiredAfterFirstMclkSwitch;

LwU32 doMclkSwitchPrimary (LwU32 targetFreqMHz, LwBool bPreAddressBootTraining, LwBool bPostAddressBootTraining, LwBool bPostWckRdWrBootTraining)
	GCC_ATTRIB_SECTION("memory", "doMclkSwitchPrimary");
LwU32 doMclkSwitchInMultipleStages(LwU32 targetFreqKHz, LwBool bPreAddressBootTraining, LwBool bPostAddressBootTraining, LwBool bPostWckRdWrBootTraining)
	GCC_ATTRIB_SECTION("memory", "doMclkSwitchInMultipleStages");
void gddr_edc_tracking (void)
  GCC_ATTRIB_SECTION("memory", "gddr_edc_tracking");
#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
void gddr_in_boot_training(LwBool in_boot_trng)
  GCC_ATTRIB_SECTION("memory", "gddr_in_boot_training");
#endif
#endif // FBFLCN_GDDR_MCLK_SWITCH_H
