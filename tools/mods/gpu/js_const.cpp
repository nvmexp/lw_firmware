/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  gpu\js_const.cpp
 * @brief Define JavaScript constants
 *
 */

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "lwRmReg.h"
#include "Lwcm.h"
#include "lwmisc.h"
#include "lwos.h"
#include "ctrl/ctrl2080/ctrl2080thermal.h"
#include "ctrl/ctrl2080/ctrl2080bus.h"
#include "ctrl/ctrl2080/ctrl2080pmgr.h"

// these are the Resman Registry Values for clock gating:

// As per the new regKeys for power features
GLOBAL_JS_PROP_CONST(RmOffElcgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_ELCG, _DISABLE),
                     "RmOffElcgAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmDisableElpgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_ELPG, _DISABLE),
                     "RmDisableElpgAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmDisableBlcgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_BLCG2, _DISABLE),
                     "RmDisableBlcgAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmDisableFspgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_FSPG, _DISABLE),
                     "RmDisableFspgAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmDisablePwrRail,
                     DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_PWR_RAIL_GATE, _DISABLE),
                     "RmDisablePwrRail: registry val for RMPowerFeature");

GLOBAL_JS_PROP_CONST(RmEnableElcgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_ELCG, _DEFAULT),
                     "RmEnableElcgAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmEnableElpgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_ELPG, _DEFAULT),
                     "RmEnableElpgAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmEnableBlcgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_BLCG2, _ENABLE),
                     "RmEnableBlcgAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmEnableFspgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_FSPG, _ENABLE),
                     "RmEnableFspgAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmEnablePwrRail,
                     DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_PWR_RAIL_GATE, _ENABLE),
                     "RmEnablePwrRail: registry val for RMPowerFeature");

GLOBAL_JS_PROP_CONST(RmElcgEngSpec, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_ELCG, _PER_ENG),
                     "RmElcgEngSpec: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmBlcgEngSpec, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_BLCG2, _PER_ENG),
                     "RmBlcgEngSpec: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmElpgEngSpec, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_ELPG, _PER_ENG),
                     "RmElpgEngSpec: registry val for RMPowerFeature");
// GR-ELCG will be disabled if RmElcgBlockGr is set
GLOBAL_JS_PROP_CONST(RmElcgBlockGr, DRF_DEF(_REG, _STR_RM, _ELCG_GR, _DISABLED),
                     "RmElcgBlockGr: registry val for RMElcg");
GLOBAL_JS_PROP_CONST(RmFspgEngSpec, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_FSPG, _PER_ENG),
                     "RmFspgEngSpec: registry val for RMPowerFeature");

GLOBAL_JS_PROP_CONST(RmAelpgDisable,
                     DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_ADAPTIVE_POWER, _DISABLE),
                     "RmAelpgDisable: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmAelpgEnable,
                     DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_ADAPTIVE_POWER, _ENABLE),
                     "RmAelpgEnable: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmBlcgIdleAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_BLCG2, _IDLE),
                     "RmBlcgIdleAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmBlcgStallAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_BLCG2, _STALL),
                     "RmBlcgStallAll: registry val for RMPowerFeature");
GLOBAL_JS_PROP_CONST(RmBlcgQuiescentAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES_BLCG2, _QUIESCENT),
                     "RmBlcgQuiescentAll: registry val for RMPowerFeature");

GLOBAL_JS_PROP_CONST(RmPowerFeatureElpgMask, DRF_SHIFTMASK(LW_REG_STR_RM_POWER_FEATURES_ELPG),
                     "RMPowerFeature regkey ELPG field");

GLOBAL_JS_PROP_CONST(RmGOMMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_POWER_FEATURES2_OPERATION_MODE),
                     "RmGOMMask: registry mask for RMPowerFeature2");
GLOBAL_JS_PROP_CONST(RmGOMDisable,
                     DRF_DEF(_REG_STR, _RM_POWER_FEATURES2, _OPERATION_MODE, _DISABLE),
                     "RmGOMDisable: registry val for RMPowerFeature2");
GLOBAL_JS_PROP_CONST(RmGOMPerMode,
                     DRF_DEF(_REG_STR, _RM_POWER_FEATURES2, _OPERATION_MODE, _PER_MODE),
                     "RmGOMPerMode: registry val for RMPowerFeature2");

GLOBAL_JS_PROP_CONST(RmSlcgMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_POWER_FEATURES2_SLCG),
                     "RmSlcgMask: registry mask for SLCG");
GLOBAL_JS_PROP_CONST(RmDisableSlcgAll, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES2_SLCG, _DISABLE),
                     "RmDisableSlcAll: registry val for RMPowerFeature2");
GLOBAL_JS_PROP_CONST(RmSlcgEngSpec, DRF_DEF(_REG, _STR_RM, _POWER_FEATURES2_SLCG, _PER_ENG),
                     "RmSlcgEngSpec: registry val for RMPowerFeature2");

GLOBAL_JS_PROP_CONST(RmGc6RomMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_POWER_FEATURES2_GC6_ROM),
                     "RmGc6RomMask: registry mask for GC6 ROM feature");
GLOBAL_JS_PROP_CONST(RmGc6RomDisable,
                     DRF_DEF(_REG, _STR_RM, _POWER_FEATURES2_GC6_ROM, _DISABLE),
                     "RmGc6RomDisable: registry val for RMPowerFeature2");

GLOBAL_JS_PROP_CONST(RmGc6RomlessMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_POWER_FEATURES2_GC6_ROMLESS),
                     "RmGc6RomlessMask: registry mask for GC6 ROM less feature");
GLOBAL_JS_PROP_CONST(RmGc6RomlessDisable,
                     DRF_DEF(_REG, _STR_RM, _POWER_FEATURES2_GC6_ROMLESS, _DISABLE),
                     "RmGc6RomlessDisable: registry val for RMPowerFeature2");

GLOBAL_JS_PROP_CONST(RmGOMComputeMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_OPERATION_MODE_COMPUTE),
                     "RmGOMComputeMask: registry mask for RMGpuOperationMode");
GLOBAL_JS_PROP_CONST(RmGOMComputeEnable,
                     DRF_DEF(_REG_STR, _RM_OPERATION_MODE, _COMPUTE, _ENABLE),
                     "RmGOMComputeEnable: registry val for RMGpuOperationMode");
GLOBAL_JS_PROP_CONST(RmGOMTexMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_OPERATION_MODE_TEX),
                     "RmGOMTexMask: registry mask for RMGpuOperationMode");
GLOBAL_JS_PROP_CONST(RmGOMTexEnable,
                     DRF_DEF(_REG_STR, _RM_OPERATION_MODE, _TEX, _ENABLE),
                     "RmGOMTexEnable: registry val for RMGpuOperationMode");
GLOBAL_JS_PROP_CONST(RmGOMDfmaMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_OPERATION_MODE_DFMA),
                     "RmGOMDfmaMask: registry mask for RMGpuOperationMode");
GLOBAL_JS_PROP_CONST(RmGOMDfmaEnable,
                     DRF_DEF(_REG_STR, _RM_OPERATION_MODE, _DFMA, _ENABLE),
                     "RmGOMDfmaEnable: registry val for RMGpuOperationMode");

GLOBAL_JS_PROP_CONST(RmFspgGrDisable, DRF_DEF(_REG, _STR_RM, _FSPG_GRAPHICS, _DISABLE),
                     "RmFspgGrDisable: registry val for RMFspg");
GLOBAL_JS_PROP_CONST(RmFspgVideoDisable, DRF_DEF(_REG, _STR_RM, _FSPG_VIDEO, _DISABLE),
                     "RmFspgVideoDisable: registry val for RMFspg");
GLOBAL_JS_PROP_CONST(RmFspgVicDisable, DRF_DEF(_REG, _STR_RM, _FSPG_VIC, _DISABLE),
                     "RmFspgVic: registry val for RMFspg");
GLOBAL_JS_PROP_CONST(RmFspgFbDisable, DRF_DEF(_REG, _STR_RM, _FSPG_FB, _DISABLE),
                     "RmFspgFbDisable: registry val for RMFspg");

// Properties used for Power Policies
GLOBAL_JS_PROP_CONST(RmPwrPolicyOverrideDisabled,
                     DRF_DEF(_REG_STR, _RM_PMGR_PWR_POLICY_OVERRIDE, _DISABLE, _DISABLED),
                     "RmPwrPolicyOverrideDisabled: registry val for RmPmgrPwrPolicyOverride");
GLOBAL_JS_PROP_CONST(RmPwrPolicyOverrideLimitsUnlimited,
                     DRF_DEF(_REG_STR, _RM_PMGR_PWR_POLICY_OVERRIDE, _LIMITS, _UNLIMITED),
                     "RmPwrPolicyOverrideLimitsUnlimited: registry val for RmPmgrPwrPolicyOverride"); //$
GLOBAL_JS_PROP_CONST(RmPwrPolicyOverrideRatedLimitMaximum,
                     DRF_DEF(_REG_STR, _RM_PMGR_PWR_POLICY_OVERRIDE, _RATED_LIMIT, _MAXIMUM),
                     "RmPwrPolicyOverrideRatedLimitMaximum: registry val for RmPmgrPwrPolicyOverride"); //$

// Lwlink Power Profiles
GLOBAL_JS_PROP_CONST(RMLwLinkControlLinkPMSingleLaneModeDisable,
                     DRF_DEF(_REG_STR, _RM_LWLINK_LINK_PM_CONTROL, _SINGLE_LANE_MODE, _DISABLE),
                     "RMLwLinkControlLinkPMSingleLaneModeDisable: registry val for lwlink_disable_single_lane_mode");
GLOBAL_JS_PROP_CONST(RMLwLinkControlLinkPMProdWriteDisable,
                     DRF_DEF(_REG_STR, _RM_LWLINK_LINK_PM_CONTROL, _PROD_WRITES, _DISABLE),
                     "RMLwLinkControlLinkPMProdWriteDisable: registry val for lwlink_disable_pm_prod_writes");
GLOBAL_JS_PROP_CONST(RMLwLinkControlLinkPML2ModeDisable,
                     DRF_DEF(_REG_STR, _RM_LWLINK_LINK_PM_CONTROL, _L2_MODE, _DISABLE),
                     "RMLwLinkControlLinkPML2ModeDisable: registry val for lwlink_disable_L2_mode");

// Properties used for Thermal Sensors
GLOBAL_JS_PROP_CONST(VbiosTable, LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_FLAGS_VBIOS,
                     "VbiosTable: registry val for thermal's VbiosTable");
GLOBAL_JS_PROP_CONST(LwrrentTable, LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_FLAGS_LWRRENT,
                     "LwrrentTable: registry val for thermal's LwrrentTable used by RM");
GLOBAL_JS_PROP_CONST(RegistryTable, LW2080_CTRL_THERMAL_GET_THERMAL_TABLE_FLAGS_REGISTRY,
                     "RegistryTable: registry val for thermal's RegistryTable");

// Properties used for PCIE link status check
GLOBAL_JS_PROP_CONST(CORRECTABLE_ERROR, LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_CORR_ERROR,
                     "PCIE correctable error");
GLOBAL_JS_PROP_CONST(NON_FATAL_ERROR, LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_NON_FATAL_ERROR,
                     "PCIE non fatal error");
GLOBAL_JS_PROP_CONST(FATAL_ERROR, LW2080_CTRL_BUS_INFO_PCIE_LINK_ERRORS_FATAL_ERROR,
                     "PCIE fatal error error");

GLOBAL_JS_PROP_CONST(AslmAllowed, DRF_DEF(_REG, _STR_CL_ASLM_CFG, _GEN2_LINK_UPGRADE, _YES),
                     "AslmCfg: enabling ASLM on current chipset");

// Properties used for Clock Slowdown
GLOBAL_JS_PROP_CONST(RmEnableLwClkSlowdown, DRF_DEF(_REG, _STR_RM_CLOCK_SLOWDOWN, _LW, _ENABLE),
                     "RmEnableLwClkSlowdown: registry val for RMClkSlowDown");
GLOBAL_JS_PROP_CONST(RmDisableLwClkSlowdown, DRF_DEF(_REG, _STR_RM_CLOCK_SLOWDOWN, _LW, _DISABLE),
                     "RmDisableLwClkSlowdown: registry val for RMClkSlowDown");
GLOBAL_JS_PROP_CONST(RmEnableHostClkSlowdown,
                     DRF_DEF(_REG, _STR_RM_CLOCK_SLOWDOWN, _HOST, _ENABLE),
                     "RmEnableHostClkSlowdown: registry val for RMClkSlowDown");
GLOBAL_JS_PROP_CONST(RmDisableHostClkSlowdown,
                     DRF_DEF(_REG, _STR_RM_CLOCK_SLOWDOWN, _HOST, _DISABLE),
                     "RmDisableHostClkSlowdown: registry val for RMClkSlowDown");

// Properties used for RM bandwidth feature
GLOBAL_JS_PROP_CONST(RmBandAsrMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_BANDWIDTH_FEATURES_ASR),
                     "RmBandAsrMask: registry mask for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandAsrDisable,
                     DRF_DEF(_REG, _STR_RM, _BANDWIDTH_FEATURES_ASR, _DISABLE),
                     "RmBandAsrDisable: registry val for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandAsrEnable,
                     DRF_DEF(_REG, _STR_RM, _BANDWIDTH_FEATURES_ASR, _ENABLE),
                     "RmBandAsrEnable: registry val for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandDynMempoolMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_BANDWIDTH_FEATURES_DYNAMIC_MEMPOOL),
                     "RmBandDynMempoolMask: registry mask for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandDynMempoolDisable,
                     DRF_DEF(_REG, _STR_RM, _BANDWIDTH_FEATURES_DYNAMIC_MEMPOOL, _DISABLE),
                     "RmBandDynMempoolDisable: registry val for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandDynMempoolEnable,
                     DRF_DEF(_REG, _STR_RM, _BANDWIDTH_FEATURES_DYNAMIC_MEMPOOL, _ENABLE),
                     "RmBandDynMempoolEnable: registry val for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandImpMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_BANDWIDTH_FEATURES_IMP),
                     "RmBandImpMask: registry mask for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandImpDisable,
                     DRF_DEF(_REG, _STR_RM, _BANDWIDTH_FEATURES_IMP, _DISABLE),
                     "RmBandImpDisable: registry val for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandImpEnable,
                     DRF_DEF(_REG, _STR_RM, _BANDWIDTH_FEATURES_IMP, _ENABLE),
                     "RmBandImpEnable: registry val for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandForceMinMempoolMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_BANDWIDTH_FEATURES_FORCE_MIN_MEMPOOL),
                     "RmBandForceMinMempool: registry mask for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandForceMinMempoolDisable,
                     DRF_DEF(_REG, _STR_RM, _BANDWIDTH_FEATURES_FORCE_MIN_MEMPOOL, _DISABLE),
                     "RmBandForceMinMempoolDisable: registry val for RmBandwidthFeature");
GLOBAL_JS_PROP_CONST(RmBandForceMinMempoolEnable,
                     DRF_DEF(_REG, _STR_RM, _BANDWIDTH_FEATURES_FORCE_MIN_MEMPOOL, _ENABLE),
                     "RmBandForceMinMempoolEnable: registry val for RmBandwidthFeature");

GLOBAL_JS_PROP_CONST(RmPstate20Disable,
                     DRF_DEF(_REG, _STR_RM, _PSTATE20_DECOUPLED_DOMAINS, _DISABLE),
                     "RmPstate20: Disable RmPstate20 decoupled domains behaviour");

GLOBAL_JS_PROP_CONST(RmPmuPerfmonSamplingDefault,
                     LW_REG_STR_RM_PMU_PERFMON_SAMPLING_DEFAULT,
                     "RmPerfmonSamplingDefault: registy value used as default");
GLOBAL_JS_PROP_CONST(RmPmuPerfmonSamplingEnableMask,
                     DRF_SHIFTMASK(LW_REG_STR_RM_PMU_PERFMON_SAMPLING_ENABLE),
                     "RmPerfmonSamplingEnableMask: registry mask for RmPerfmonSamplingEnable field"); //$
GLOBAL_JS_PROP_CONST(RmPmuPerfmonSamplingDisable,
                     DRF_DEF(_REG, _STR_RM, _PMU_PERFMON_SAMPLING_ENABLE, _DIS),
                     "RmPerfmonSampling: registry val to disable PMU perfmon sampling");

GLOBAL_JS_PROP_CONST(RMPcieLinkGen2Disable,
                     DRF_DEF(_REG, _STR_RM, _PCIE_LINK_SPEED_ALLOW_GEN2, _DISABLE),
                     "RMPcieLinkSpeed: RM will disallow Gen2, bypassing LW_XVE_VSEC_LWIDIA_SPECIFIC_FEATURES_HIERARCHY VBIOS");//$
GLOBAL_JS_PROP_CONST(RMPcieLinkGen2Enable,
                     DRF_DEF(_REG, _STR_RM, _PCIE_LINK_SPEED_ALLOW_GEN2, _ENABLE),
                     "RMPcieLinkSpeed: RM will enable Gen2, bypassing LW_XVE_VSEC_LWIDIA_SPECIFIC_FEATURES_HIERARCHY VBIOS");//$
GLOBAL_JS_PROP_CONST(RMPcieLinkGen3Disable,
                     DRF_DEF(_REG, _STR_RM, _PCIE_LINK_SPEED_ALLOW_GEN3, _DISABLE),
                     "RMPcieLinkSpeed: RM will disallow Gen3, bypassing LW_XVE_VSEC_LWIDIA_SPECIFIC_FEATURES_HIERARCHY VBIOS");//$
GLOBAL_JS_PROP_CONST(RMPcieLinkGen3Enable,
                     DRF_DEF(_REG, _STR_RM, _PCIE_LINK_SPEED_ALLOW_GEN3, _ENABLE),
                     "RMPcieLinkSpeed: RM will enable Gen3, bypassing LW_XVE_VSEC_LWIDIA_SPECIFIC_FEATURES_HIERARCHY VBIOS");//$
GLOBAL_JS_PROP_CONST(RMPcieLinkGen4Disable,
                     DRF_DEF(_REG, _STR_RM, _PCIE_LINK_SPEED_ALLOW_GEN4, _DISABLE),
                     "RMPcieLinkSpeed: RM will disallow Gen4, bypassing LW_XVE_VSEC_LWIDIA_SPECIFIC_FEATURES_HIERARCHY VBIOS");//$
GLOBAL_JS_PROP_CONST(RMPcieLinkGen4Enable,
                     DRF_DEF(_REG, _STR_RM, _PCIE_LINK_SPEED_ALLOW_GEN4, _ENABLE),
                     "RMPcieLinkSpeed: RM will enable Gen4, bypassing LW_XVE_VSEC_LWIDIA_SPECIFIC_FEATURES_HIERARCHY VBIOS");//$
GLOBAL_JS_PROP_CONST(RMPcieLinkGen5Disable,
                     DRF_DEF(_REG, _STR_RM, _PCIE_LINK_SPEED_ALLOW_GEN5, _DISABLE),
                     "RMPcieLinkSpeed: RM will disallow Gen5, bypassing LW_XVE_VSEC_LWIDIA_SPECIFIC_FEATURES_HIERARCHY VBIOS");//$
GLOBAL_JS_PROP_CONST(RMPcieLinkGen5Enable,
                     DRF_DEF(_REG, _STR_RM, _PCIE_LINK_SPEED_ALLOW_GEN5, _ENABLE),
                     "RMPcieLinkSpeed: RM will enable Gen5, bypassing LW_XVE_VSEC_LWIDIA_SPECIFIC_FEATURES_HIERARCHY VBIOS");//$


// Properties used for SMMU configuration in CheetAh chip
GLOBAL_JS_PROP_CONST(RmSmmuConfigAllDefault, DRF_DEF(_REG_STR_RM, _SMMU_CONFIG, _ALL, _DEFAULT),
                     "RmSmmuConfigAllDefault: registry val for RmSmmuConfig to use the default settings for all engines");//$
GLOBAL_JS_PROP_CONST(RmSmmuConfigAllDisable, DRF_DEF(_REG_STR_RM, _SMMU_CONFIG, _ALL, _DISABLE),
                     "RmSmmuConfigAllDisable: registry val for RmSmmuConfig to disable SMMU translation for all engines");//$
GLOBAL_JS_PROP_CONST(RmSmmuConfigAllEnable, DRF_DEF(_REG_STR_RM, _SMMU_CONFIG, _ALL, _ENABLE),
                     "RmSmmuConfigAllEnable: registry val for RmSmmuConfig to enable SMMU translation for all engines");//$
GLOBAL_JS_PROP_CONST(RmSmmuConfigDisplayDefault,
                     DRF_DEF(_REG_STR_RM, _SMMU_CONFIG, _DISPLAY, _DEFAULT),
                     "RmSmmuConfigDisplayDefault: registry val for RmSmmuConfig to use the default setting for display");//$
GLOBAL_JS_PROP_CONST(RmSmmuConfigDisplayDisable,
                     DRF_DEF(_REG_STR_RM, _SMMU_CONFIG, _DISPLAY, _DISABLE),
                     "RmSmmuConfigDisplayDisable: registry val for RmSmmuConfig to disable SMMU translation for display");//$
GLOBAL_JS_PROP_CONST(RmSmmuConfigDisplayEnable,
                     DRF_DEF(_REG_STR_RM, _SMMU_CONFIG, _DISPLAY, _ENABLE),
                     "RmSmmuConfigDisplayEnable: registry val for RmSmmuConfig to enable SMMU translation for display");//$

// Properties used for controlling RM SMMU test app at runtime
GLOBAL_JS_PROP_CONST(RmSmmuTestModeDefault, LWOS32_ATTR2_SMMU_ON_GPU_ENABLE + 1,
                     "RmSmmuTestModeDefault: Default mode of operation of RM SMMU Test app");

// Properties for modstest.js
GLOBAL_JS_PROP_CONST(PmgrPwrPolicyMaxPolicies, LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES,
                     "Maximum number of RM_PMU_PMGR_PWR_POLICY entries which can be supported in the RM or PMU");//$

// Properties used for Priv Security
GLOBAL_JS_PROP_CONST(RmPrivSelwrityDisableDefault,
                     DRF_DEF(_REG_STR_RM, _PRIV_SELWRITY, _DISABLE, _DEFAULT),
                     "RmPrivSelwrityDisableDefault");
GLOBAL_JS_PROP_CONST(RmPrivSelwrityDisableFalse,
                     DRF_DEF(_REG_STR_RM, _PRIV_SELWRITY, _DISABLE, _FALSE),
                     "RmPrivSelwrityDisableFalse");
GLOBAL_JS_PROP_CONST(RmPrivSelwrityDisableTrue,
                     DRF_DEF(_REG_STR_RM, _PRIV_SELWRITY, _DISABLE, _TRUE),
                     "RmPrivSelwrityDisableTrue");
GLOBAL_JS_PROP_CONST(RmPrivSelwrityModeDefault,
                     DRF_DEF(_REG_STR_RM, _PRIV_SELWRITY, _MODE, _DEFAULT),
                     "RmPrivSelwrityModeDefault");
GLOBAL_JS_PROP_CONST(RmPrivSelwrityModeProd,
                     DRF_DEF(_REG_STR_RM, _PRIV_SELWRITY, _MODE, _PROD),
                     "RmPrivSelwrityModeProd");
GLOBAL_JS_PROP_CONST(RmPrivSelwrityModeDebug,
                     DRF_DEF(_REG_STR_RM, _PRIV_SELWRITY, _MODE, _DEBUG),
                     "RmPrivSelwrityModeDebug");

#ifdef LW_VERIF_FEATURES
GLOBAL_JS_PROP_CONST(RmNonContigAllocConfigSetDefaultTrue,
                     DRF_DEF(_REG_STR_RM, _NONCONTIG_ALLOC_CONFIG, _SET_DEFAULT, _TRUE),
                     "RmNonContigAllocConfigSetDefaultTrue");
GLOBAL_JS_PROP_CONST(RmNonContigAllocConfigPageShuffleEnable,
                     DRF_DEF(_REG_STR_RM, _NONCONTIG_ALLOC_CONFIG, _PAGE_SHUFFLE, _ENABLE),
                     "RmNonContigAllocConfigPageShuffleEnable");
#endif

GLOBAL_JS_PROP_CONST(RmPerfLimitsOverrideLockSet,
                     DRF_DEF(_REG_STR_RM, _PERF_LIMITS_OVERRIDE, _LOCK, _TRUE),
                     "Lock the perf arbiter");
GLOBAL_JS_PROP_CONST(RmPerfChangeSeqOverrideLockSet,
                     DRF_DEF(_REG_STR_RM, _PERF_CHANGE_SEQ_OVERRIDE, _LOCK, _SET),
                     "Lock the perf change sequencer");
GLOBAL_JS_PROP_CONST(RmPerfChangeSeqOverrideSkipVoltRangeTrim,
                     DRF_DEF(_REG_STR_RM, _PERF_CHANGE_SEQ_OVERRIDE, _SKIP_VOLT_RANGE_TRIM, _SET),
                     "Skip voltage range trimming");

// RM Userd instance location variables
GLOBAL_JS_PROP_CONST(RmInstLolwserdDefault, DRF_DEF(_REG_STR_RM, _INST_LOC, _USERD, _DEFAULT), "RmInstLolwserdDefault");
GLOBAL_JS_PROP_CONST(RmInstLolwserdCoh,     DRF_DEF(_REG_STR_RM, _INST_LOC, _USERD, _COH),     "RmInstLolwserdCoh");
GLOBAL_JS_PROP_CONST(RmInstLolwserdNonCoh,  DRF_DEF(_REG_STR_RM, _INST_LOC, _USERD, _NCOH),    "RmInstLolwserdNonCoh");
GLOBAL_JS_PROP_CONST(RmInstLolwserdVid,     DRF_DEF(_REG_STR_RM, _INST_LOC, _USERD, _VID),     "RmInstLolwserdVid");
GLOBAL_JS_PROP_CONST(RmInstLolwserdMask,    DRF_SHIFTMASK(LW_REG_STR_RM_INST_LOC_USERD),       "RmInstLolwserdMask");

GLOBAL_JS_PROP_CONST(RmPmuBootstrapModeRm,         LW_REG_STR_RM_PMU_BOOTSTRAP_MODE_RM,           "RmPmuBootstrapModeRm");         //$
GLOBAL_JS_PROP_CONST(RmPmuBootstrapModeManual,     LW_REG_STR_RM_PMU_BOOTSTRAP_MODE_MANUAL,       "RmPmuBootstrapModeManual");     //$
GLOBAL_JS_PROP_CONST(RmPmuBootstrapModeVbios,      LW_REG_STR_RM_PMU_BOOTSTRAP_MODE_VBIOS,        "RmPmuBootstrapModeVbios");      //$
GLOBAL_JS_PROP_CONST(RmPmuBootstrapModeDisabled,   LW_REG_STR_RM_PMU_BOOTSTRAP_MODE_DISABLED,     "RmPmuBootstrapModeDisabled");   //$
GLOBAL_JS_PROP_CONST(RmPmuBootstrapModeRmLpwrOnly, LW_REG_STR_RM_PMU_BOOTSTRAP_MODE_RM_LPWR_ONLY, "RmPmuBootstrapModeRmLpwrOnly"); //$

GLOBAL_JS_PROP_CONST(RmDebugSMCDynamicTpcFsEnabledDevinitOwn,
                     LW_REG_STR_RM_DEBUG_SMC_DYNAMIC_TPC_FS_ENABLED_DEVINIT_OWN,
                     "SMC Dynamic TPC Floorsweeping enabled and performed by devinit");
