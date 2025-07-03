// Open Questions:
// 1. Where do I find if a clock is noise aware or not? (Lwrrently have all of
//    them marked as noise unaware)
//
#include "ctrl/ctrl2080/ctrl2080clk.h"
#include "ctrl/ctrl2080/ctrl2080vfe.h"
#include "pmu_objclk.h"
#include "pmu/pmuifclk.h"
#include "pmu/pmuifperfmon.h"

// Test Data
/*!
 * @brief CLK_DOMAIN data passed down by the host driver.
 *
 * Have some questions:
 * @li Where do I find if a clock domain is noise aware or not?
 * @li How do I get/callwlate clkPos?
 * @li Is the maximum number of VF lwrves for automotive 1?
 */
const RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET_UNION UT_ClockDomains[10] =
{
    {
        // 0: GPCCLK (Primary)
        .v35Primary =
        {
            // RM_PMU_CLK_CLK_DOMAIN_35_PROG_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_3X_PROG_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                        .super =
                        {
                            // RM_PMU_BOARDOBJ
                            .super =
                            {
                                .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY,
                                .grpIdx = 0,
                            },
                            .domain = clkWhich_GpcClk,
                            .apiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK,
                            .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_GPC2CLK,
                        },
                        .bNoiseAwareCapable = LW_FALSE,
                    },
                    .clkProgIdxFirst = 0x00,
                    .clkProgIdxLast = 0x00,
                    .bForceNoiseUnawareOrdering = LW_FALSE,
                    // LW2080_CTRL_CLK_FREQ_DELTA
                    .factoryDelta =
                    {
                        .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                        .data =
                        {
                            .staticOffset = 0,
                        },
                    },
                    .freqDeltaMinMHz = -1000,
                    .freqDeltaMaxMHz = 1000,
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        // LW2080_CTRL_CLK_FREQ_DELTA
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset = 0,
                            },
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0,
                        },
                    },
                },
                .preVoltOrderingIndex = 3,
                .postVoltOrderingIndex = 1,
                .clkPos = 0,
                .clkVFLwrveCount = 1,
                // LW2080_CTRL_CLK_CLK_DOMAIN_INFO_35_PROG_CLK_MON
                .clkMonInfo =
                {
                    .lowThresholdVfeIdx = 0,
                    .highThresholdVfeIdx = 0,
                },
                // LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON
                .clkMonCtrl =
                {
                    .flags = 0,
                    .lowThresholdOverride = 0,
                    .highThresholdOverride = 0,
                },
                .porVoltDeltauV =
                {
                    0,
                    0,
                    0,
                    0,
                },
            },

            // RM_PMU_CLK_CLK_DOMAIN_3X_PRIMARY_OBJECT_SET
            .primary =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                // LW2080_CTRL_BOARDOBJGRP_MASK_E32
                .secondaryIdxsMask =
                {
                    // LW2080_CTRL_BOARDOBJGRP_MASK
                    .super =
                    {
                        .pData[0] = 0x00000115,
                    }
                },
            },
            // LW2080_CTRL_BOARDOBJGRP_MASK_E32
            .primarySecondaryDomainsGrpMask =
            {
                .super =
                {
                    .pData[0] = 0x00000115,
                }
            },
        }
    },
    {
        // 1: XBARCLK (Secondary)
        .v35Secondary =
        {
            // RM_PMU_CLK_CLK_DOMAIN_35_PROG_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_3X_PROG_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                        .super =
                        {
                            // RM_PMU_BOARDOBJ
                            .super =
                            {
                                .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY,
                                .grpIdx = 1,
                            },
                            .domain = clkWhich_XbarClk,
                            .apiDomain = LW2080_CTRL_CLK_DOMAIN_XBARCLK,
                            .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                        },
                        .bNoiseAwareCapable = LW_FALSE,
                    },
                    .clkProgIdxFirst = 1,
                    .clkProgIdxLast = 1,
                    .bForceNoiseUnawareOrdering = LW_FALSE,
                    // LW2080_CTRL_CLK_FREQ_DELTA
                    .factoryDelta =
                    {
                        .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                        .data =
                        {
                            .staticOffset.deltakHz = 0,
                        }
                    },
                    .freqDeltaMinMHz = 0,
                    .freqDeltaMaxMHz = 0,
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        // LW2080_CTRL_CLK_FREQ_DELTA
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            },
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0
                        },
                    },
                },
                .preVoltOrderingIndex = 4,
                .postVoltOrderingIndex = 2,
                .clkPos = 0,
                .clkVFLwrveCount = 1,
                // LW2080_CTRL_CLK_CLK_DOMAIN_INFO_35_PROG_CLK_MON
                .clkMonInfo =
                {
                    .lowThresholdVfeIdx = 0,
                    .highThresholdVfeIdx = 0,
                },
                // LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON
                .clkMonCtrl =
                {
                    .flags = 0,
                    .lowThresholdOverride = 0,
                    .highThresholdOverride = 0,
                },
                .porVoltDeltauV =
                {
                    0,
                    0,
                    0,
                    0,
                },
            },
            // RM_PMU_CLK_CLK_DOMAIN_3X_SECONDARY_BOARDOBJ_SET
            .secondary =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                .primaryIdx = 0,
            },
        },
    },
    {
        // 2: DRAMCLK (Fixed)
        .v3xFixed =
        {
            // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_BOARDOBJ
                    .super =
                    {
                        .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED,
                        .grpIdx = 2,
                    },
                    .domain = clkWhich_MClk,
                    .apiDomain = LW2080_CTRL_CLK_DOMAIN_MCLK,
                    .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                },
                .bNoiseAwareCapable = LW_FALSE,
            },
            .freqMHz = 6001,
        },
    },
    {
        // 3: SYSCLK (Secondary)
        .v35Secondary =
        {
            // RM_PMU_CLK_CLK_DOMAIN_35_PROG_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_3X_PROG_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                        .super =
                        {
                            // RM_PMU_BOARDOBJ
                            .super =
                            {
                                .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY,
                                .grpIdx = 3,
                            },
                            .domain = clkWhich_SysClk,
                            .apiDomain = LW2080_CTRL_CLK_DOMAIN_SYSCLK,
                            .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                        },
                        .bNoiseAwareCapable = LW_FALSE,
                    },
                    .clkProgIdxFirst = 0x03,
                    .clkProgIdxLast = 0x03,
                    .bForceNoiseUnawareOrdering = LW_FALSE,
                    // LW2080_CTRL_CLK_FREQ_DELTA
                    .factoryDelta =
                    {
                        .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                        .data =
                        {
                            .staticOffset.deltakHz = 0,
                        },
                    },
                    .freqDeltaMinMHz = 0,
                    .freqDeltaMaxMHz = 0,
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        // LW2080_CTRL_CLK_FREQ_DELTA
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            },
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0,
                        },
                    },
                },
                .preVoltOrderingIndex = 0x03,
                .postVoltOrderingIndex = 0x03,
                .clkPos = 0,
                .clkVFLwrveCount = 1,
                // LW2080_CTRL_CLK_CLK_DOMAIN_INFO_35_PROG_CLK_MON
                .clkMonInfo =
                {
                    .lowThresholdVfeIdx = 0,
                    .highThresholdVfeIdx = 0,
                },
                // LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON
                .clkMonCtrl =
                {
                    .flags = 0,
                    .lowThresholdOverride = 0,
                    .highThresholdOverride = 0,
                },
                .porVoltDeltauV =
                {
                    0,
                    0,
                    0,
                    0,
                },
            },
            // RM_PMU_CLK_CLK_DOMAIN_3X_SECONDARY_BOARDOBJ_SET
            .secondary =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                .primaryIdx = 0,
            },
        },
    },
    {
        // 4: HUBCLK (Fixed)
        .v3xFixed =
        {
            // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_BOARDOBJ
                    .super =
                    {
                        .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED,
                        .grpIdx = 4,
                    },
                    .domain = clkWhich_HubClk,
                    .apiDomain = LW2080_CTRL_CLK_DOMAIN_HUBCLK,
                    .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                },
                .bNoiseAwareCapable = LW_FALSE,
            },
            .freqMHz = 27,
        }
    },
    {
        // 5: LWDCLK (Secondary)
        .v35Secondary =
        {
            // RM_PMU_CLK_CLK_DOMAIN_35_PROG_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_3X_PROG_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                        .super =
                        {
                            // RM_PMU_BOARDOBJ
                            .super =
                            {
                                .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY,
                                .grpIdx = 9,
                            },
                            .domain = clkWhich_LwdClk,
                            .apiDomain = LW2080_CTRL_CLK_DOMAIN_LWDCLK,
                            .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                        },
                        .bNoiseAwareCapable = LW_FALSE,
                    },
                    .clkProgIdxFirst = 0x02,
                    .clkProgIdxLast = 0x02,
                    .bForceNoiseUnawareOrdering = LW_FALSE,
                    // LW2080_CTRL_CLK_FREQ_DELTA
                    .factoryDelta =
                    {
                        .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                        .data =
                        {
                            .staticOffset.deltakHz = 0,
                        },
                    },
                    .freqDeltaMinMHz = 0,
                    .freqDeltaMaxMHz = 0,
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            },
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0,
                        }
                    },
                },
                .preVoltOrderingIndex = 0x06,
                .postVoltOrderingIndex = 0x04,
                .clkPos = 0,
                .clkVFLwrveCount = 1,
                // LW2080_CTRL_CLK_CLK_DOMAIN_INFO_35_PROG_CLK_MON
                .clkMonInfo =
                {
                    .lowThresholdVfeIdx = 0,
                    .highThresholdVfeIdx = 0,
                },
                // LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON
                .clkMonCtrl =
                {
                    .flags = 0,
                    .lowThresholdOverride = 0,
                    .highThresholdOverride = 0,
                },
                .porVoltDeltauV =
                {
                    0,
                    0,
                    0,
                    0,
                },
            },
            // RM_PMU_CLK_CLK_DOMAIN_3X_SECONDARY_BOARDOBJ_SET
            .secondary =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                .primaryIdx = 0,
            },
        },
    },
    {
        // 6: PWRCLK (Fixed)
        .v3xFixed =
        {
            // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_BOARDOBJ
                    .super =
                    {
                        .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED,
                        .grpIdx = 6,
                    },
                    .domain = clkWhich_PwrClk,
                    .apiDomain = LW2080_CTRL_CLK_DOMAIN_PWRCLK,
                    .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                },
                .bNoiseAwareCapable = LW_FALSE,
            },
            .freqMHz = 540,
        },
    },
    {
        // 7: DISPCLK (Fixed)
        .v3xFixed =
        {
            // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_BOARDOBJ
                    .super =
                    {
                        .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED,
                        .grpIdx = 6,
                    },
                    .domain = clkWhich_DispClk,
                    .apiDomain = LW2080_CTRL_CLK_DOMAIN_DISPCLK,
                    .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                },
                .bNoiseAwareCapable = LW_FALSE,
            },
            .freqMHz = 27,
        }
    },
    {
        // 8: PCIE Gen Speed (Fixed)
        .v3xFixed =
        {
            // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_BOARDOBJ
                    .super =
                    {
                        .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED,
                        .grpIdx = 8,
                    },
                    .domain = clkWhich_PCIEGenClk,
                    .apiDomain = LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK,
                    .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                },
                .bNoiseAwareCapable = LW_FALSE,
            },
            .freqMHz = 3,
        }
    },
    {
        // 9: HOSTCLK (Secondary)
        .v35Secondary =
        {
            // RM_PMU_CLK_CLK_DOMAIN_35_PROG_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_DOMAIN_3X_PROG_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_DOMAIN_3X_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET
                        .super =
                        {
                            // RM_PMU_BOARDOBJ
                            .super =
                            {
                                .type = LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY,
                                .grpIdx = 9,
                            },
                            .domain = clkWhich_HostClk,
                            .apiDomain = LW2080_CTRL_CLK_DOMAIN_HOSTCLK,
                            .perfDomainGrpIdx = RM_PMU_DOMAIN_GROUP_PSTATE,
                        },
                        .bNoiseAwareCapable = LW_FALSE,
                    },
                    .clkProgIdxFirst = 0x04,
                    .clkProgIdxLast = 0x04,
                    .bForceNoiseUnawareOrdering = LW_FALSE,
                    // LW2080_CTRL_CLK_FREQ_DELTA
                    .factoryDelta =
                    {
                        .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                        .data =
                        {
                            .staticOffset.deltakHz = 0,
                        },
                    },
                    .freqDeltaMinMHz = 0,
                    .freqDeltaMaxMHz = 0,
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            },
                        },
                    },
                },
                .preVoltOrderingIndex = 0x04,
                .postVoltOrderingIndex = 0x04,
                .clkPos = 0,
                .clkVFLwrveCount = 1,
                // LW2080_CTRL_CLK_CLK_DOMAIN_INFO_35_PROG_CLK_MON
                .clkMonInfo =
                {
                    .lowThresholdVfeIdx = 0,
                    .highThresholdVfeIdx = 0,
                },
                // LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON
                .clkMonCtrl =
                {
                    .flags = 0,
                    .lowThresholdOverride = 0,
                    .highThresholdOverride = 0,
                },
                .porVoltDeltauV =
                {
                    0,
                    0,
                    0,
                    0,
                },
            },
            // RM_PMU_CLK_CLK_DOMAIN_3X_SECONDARY_BOARDOBJ_SET
            .secondary =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                .primaryIdx = 0,
            },
        },
    },
};

// Clock Prog
const RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET_UNION UT_ClockProg[] =
{
    {
        // RM_PMU_CLK_CLK_PROG_35_PRIMARY_RATIO_BOARDOBJ_SET
        .v35PrimaryRatio =
        {
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_BOARDOBJ
                        .super =
                        {
                            .type = LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO,
                            .grpIdx = 0,
                        },
                    },
                    .source = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL,
                    .freqMaxMHz = 1440,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_SOURCE_DATA
                    .sourceData =
                    {
                        .pll.pllIdx = 255,
                        .pll.freqStepSizeMHz = 0,
                    },
                },
                // RM_PMU_CLK_CLK_PROG_3X_PRIMARY_BOARDOBJ_SET
                .primary =
                {
                    .super =
                    {
                        .rsvd = 0,
                    },
                    .bOCOVEnabled = LW_TRUE,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY
                    .vfEntries = // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY_MAX_ENTRIES
                    {
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                    },
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        // LW2080_CTRL_CLK_FREQ_DELTA
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data.staticOffset.deltakHz = 0,
                        },
                        .voltDeltauV = // LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS
                        {
                            0,
                            0,
                            0,
                            0,
                        },
                    },
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_SOURCE_DATA
                    .sourceData =
                    {
                        .nafll.baseVFSmoothVoltuV = 0x00086470,
                        .nafll.maxVFRampRate = 0x00000021,
                        .nafll.maxFreqStepSizeMHz = 0,
                    },
                },
                // LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL
                .voltRailSecVfEntries = // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY_MAX_ENTRIES
                {
                    {
                        .secVfEntries = // LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL_MAX
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            },
                        },
                    },
                },
            },
            // RM_PMU_CLK_CLK_PROG_3X_PRIMARY_RATIO_BOARDOBJ_SET
            .ratio =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_RATIO_SECONDARY_ENTRY
                .secondaryEntries = // LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES
                {
                    {
                        // XBARCLK
                        .clkDomIdx = 1,
                        .ratio = 96,
                    },
                    {
                        // LWDCLK
                        .clkDomIdx = 5,
                        .ratio = 100,
                    },
                    {
                        // SYSCLK
                        .clkDomIdx = 3,
                        .ratio = 93,
                    },
                    {
                        // HOSTCLK
                        .clkDomIdx = 9,
                        .ratio = 89,
                    },
                    {
                        .clkDomIdx = LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID,
                        .ratio = 0,
                    },
                    {
                        .clkDomIdx = LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID,
                        .ratio = 0,
                    },
                },
            },
        },
    },
};

const RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET_UNION UT_ClockProgPrimaryTable[] =
{
    // 0
    {
        // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
        .v35PrimaryTable =
        {
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_BOARDOBJ
                        .super =
                        {
                            .type = LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE,
                            .grpIdx = 3,
                        },
                    },
                    .source = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
                    .freqMaxMHz = 810,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_SOURCE_DATA
                    .sourceData =
                    {
                        .pll.pllIdx = 0xFF,
                        .pll.freqStepSizeMHz = 0,
                    },
                },
                // RM_PMU_CLK_CLK_PROG_3X_PRIMARY_BOARDOBJ_SET
                .primary =
                {
                    // RM_PMU_BOARDOBJ_INTERFACE
                    .super =
                    {
                        .rsvd = 0,
                    },
                    .bOCOVEnabled = LW_FALSE,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY
                    .vfEntries =
                    {
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                    },
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            }
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0,
                        }
                    },
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_SOURCE_DATA
                    .sourceData =
                    {
                        .nafll.baseVFSmoothVoltuV = 0,
                        .nafll.maxVFRampRate = 0,
                        .nafll.maxFreqStepSizeMHz = 0,
                    },
                },
                // LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL
                .voltRailSecVfEntries =
                {
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                },
            },
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
            .table =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY
                .secondaryEntries =
                {
                    {
                        .clkDomIdx = 4, // HUBCLK
                        .freqMHz = 202,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                }
            },
        },
    },
    // 1
    {
        // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
        .v35PrimaryTable =
        {
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_BOARDOBJ
                        .super =
                        {
                            .type = LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE,
                            .grpIdx = 0,
                        },
                    },
                    .source = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
                    .freqMaxMHz = 810,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_SOURCE_DATA
                    .sourceData =
                    {
                        .pll.pllIdx = 0xFF,
                        .pll.freqStepSizeMHz = 0,
                    },
                },
                // RM_PMU_CLK_CLK_PROG_3X_PRIMARY_BOARDOBJ_SET
                .primary =
                {
                    // RM_PMU_BOARDOBJ_INTERFACE
                    .super =
                    {
                        .rsvd = 0,
                    },
                    .bOCOVEnabled = LW_FALSE,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY
                    .vfEntries =
                    {
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                    },
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            }
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0,
                        }
                    },
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_SOURCE_DATA
                    .sourceData =
                    {
                        .nafll.baseVFSmoothVoltuV = 0,
                        .nafll.maxVFRampRate = 0,
                        .nafll.maxFreqStepSizeMHz = 0,
                    },
                },
                // LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL
                .voltRailSecVfEntries =
                {
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                },
            },
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
            .table =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY
                .secondaryEntries =
                {
                    {
                        .clkDomIdx = 4, // HUBCLK
                        .freqMHz = 202,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                }
            },
        },
    },
    // 2
    {
        // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
        .v35PrimaryTable =
        {
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_BOARDOBJ
                        .super =
                        {
                            .type = LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE,
                            .grpIdx = 0,
                        },
                    },
                    .source = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
                    .freqMaxMHz = 810,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_SOURCE_DATA
                    .sourceData =
                    {
                        .pll.pllIdx = 0xFF,
                        .pll.freqStepSizeMHz = 0,
                    },
                },
                // RM_PMU_CLK_CLK_PROG_3X_PRIMARY_BOARDOBJ_SET
                .primary =
                {
                    // RM_PMU_BOARDOBJ_INTERFACE
                    .super =
                    {
                        .rsvd = 0,
                    },
                    .bOCOVEnabled = LW_FALSE,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY
                    .vfEntries =
                    {
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                    },
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            }
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0,
                        }
                    },
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_SOURCE_DATA
                    .sourceData =
                    {
                        .nafll.baseVFSmoothVoltuV = 0,
                        .nafll.maxVFRampRate = 0,
                        .nafll.maxFreqStepSizeMHz = 0,
                    },
                },
                // LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL
                .voltRailSecVfEntries =
                {
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                },
            },
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
            .table =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY
                .secondaryEntries =
                {
                    {
                        .clkDomIdx = 4, // HUBCLK
                        .freqMHz = 202,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                }
            },
        },
    },
    // 3
    {
        // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
        .v35PrimaryTable =
        {
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_BOARDOBJ
                        .super =
                        {
                            .type = LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE,
                            .grpIdx = 0,
                        },
                    },
                    .source = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
                    .freqMaxMHz = 810,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_SOURCE_DATA
                    .sourceData =
                    {
                        .pll.pllIdx = 0xFF,
                        .pll.freqStepSizeMHz = 0,
                    },
                },
                // RM_PMU_CLK_CLK_PROG_3X_PRIMARY_BOARDOBJ_SET
                .primary =
                {
                    // RM_PMU_BOARDOBJ_INTERFACE
                    .super =
                    {
                        .rsvd = 0,
                    },
                    .bOCOVEnabled = LW_FALSE,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY
                    .vfEntries =
                    {
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                    },
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            }
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0,
                        }
                    },
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_SOURCE_DATA
                    .sourceData =
                    {
                        .nafll.baseVFSmoothVoltuV = 0,
                        .nafll.maxVFRampRate = 0,
                        .nafll.maxFreqStepSizeMHz = 0,
                    },
                },
                // LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL
                .voltRailSecVfEntries =
                {
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                },
            },
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
            .table =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY
                .secondaryEntries =
                {
                    {
                        .clkDomIdx = 4, // HUBCLK
                        .freqMHz = 202,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                }
            },
        },
    },
    // 4
    {
        // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
        .v35PrimaryTable =
        {
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_BOARDOBJ_SET
            .super =
            {
                // RM_PMU_CLK_CLK_PROG_3X_BOARDOBJ_SET
                .super =
                {
                    // RM_PMU_CLK_CLK_PROG_BOARDOBJ_SET
                    .super =
                    {
                        // RM_PMU_BOARDOBJ
                        .super =
                        {
                            .type = LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE,
                            .grpIdx = 0,
                        },
                    },
                    .source = LW2080_CTRL_CLK_PROG_1X_SOURCE_PLL,
                    .freqMaxMHz = 810,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_SOURCE_DATA
                    .sourceData =
                    {
                        .pll.pllIdx = 0xFF,
                        .pll.freqStepSizeMHz = 0,
                    },
                },
                // RM_PMU_CLK_CLK_PROG_3X_PRIMARY_BOARDOBJ_SET
                .primary =
                {
                    // RM_PMU_BOARDOBJ_INTERFACE
                    .super =
                    {
                        .rsvd = 0,
                    },
                    .bOCOVEnabled = LW_FALSE,
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY
                    .vfEntries =
                    {
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                        {
                            .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .cpmMaxFreqOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                        },
                    },
                    // LW2080_CTRL_CLK_CLK_DELTA
                    .deltas =
                    {
                        .freqDelta =
                        {
                            .type = LW2080_CTRL_CLK_CLK_FREQ_DELTA_TYPE_STATIC,
                            .data =
                            {
                                .staticOffset.deltakHz = 0,
                            }
                        },
                        .voltDeltauV =
                        {
                            0,
                            0,
                            0,
                            0,
                        }
                    },
                    // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_SOURCE_DATA
                    .sourceData =
                    {
                        .nafll.baseVFSmoothVoltuV = 0,
                        .nafll.maxVFRampRate = 0,
                        .nafll.maxFreqStepSizeMHz = 0,
                    },
                },
                // LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL
                .voltRailSecVfEntries =
                {
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                    {
                        .secVfEntries =
                        {
                            {
                                .vfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .dvcoOffsetVfeIdx = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxFirst = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                                .vfPointIdxLast = LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID,
                            }
                        },
                    },
                },
            },
            // RM_PMU_CLK_CLK_PROG_35_PRIMARY_TABLE_BOARDOBJ_SET
            .table =
            {
                // RM_PMU_BOARDOBJ_INTERFACE
                .super =
                {
                    .rsvd = 0,
                },
                // LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_TABLE_SECONDARY_ENTRY
                .secondaryEntries =
                {
                    {
                        .clkDomIdx = 4, // HUBCLK
                        .freqMHz = 202,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                    {
                        .clkDomIdx = 0xFF,
                        .freqMHz = 0,
                    },
                }
            },
        },
    },
};
