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
 * @file   cmdmgmt_supersurface.c
 * @brief  Provides an interface to receive PMU configuration commands combined
 *         in a super surface for reduced latency communication.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "main.h"
#include "cmdmgmt/cmdmgmt.h"

/* ------------------------- Application Includes --------------------------- */
#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Construct the super-surface member descriptor ID.
 */
#define SSMD_ID(_member,_type)                                                 \
    (DRF_NUM(_RM_PMU_SUPER_SURFACE, _MEMBER_ID, _GROUP,                        \
             RM_PMU_SUPER_SURFACE_MEMBER_##_member)             |              \
     DRF_DEF(_RM_PMU_SUPER_SURFACE, _MEMBER_ID, _TYPE, _##_type))

/*!
 * @brief   Construct the single super-surface member descriptor entry.
 */
#define SSMD_ENTRY(_member,_type,_field)                                       \
    {                                                                          \
        .id     = SSMD_ID(_member, _type),                                     \
        .offset = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(_field),                  \
        .size   = RM_PMU_SUPER_SURFACE_MEMBER_SIZE(_field),                    \
    }

/* ------------------------- Static Variables ------------------------------- */
/*!
 * Hard-coded table of all super-surface members required for the LWGPU driver.
 *
 * Maintained by the LWGPU team (do NOT add here your sub-surfaces by default).
 */
static RM_PMU_SUPER_SURFACE_MEMBER_DESCRIPTOR SuperSurfaceMemberDescriptors[]
    GCC_ATTRIB_SECTION("dmem_cmdmgmtSsmd", "SuperSurfaceMemberDescriptors") =
{
    SSMD_ENTRY(VOLT_DEVICE_GRP, SET,        pascalAndLater.volt.voltDeviceGrpSet),

    SSMD_ENTRY(VOLT_POLICY_GRP, SET,        pascalAndLater.volt.voltPolicyGrpSet),
    SSMD_ENTRY(VOLT_POLICY_GRP, GET_STATUS, pascalAndLater.volt.voltPolicyGrpGetStatus),

    SSMD_ENTRY(VOLT_RAIL_GRP,   SET,        pascalAndLater.volt.voltRailGrpSet),
    SSMD_ENTRY(VOLT_RAIL_GRP,   GET_STATUS, pascalAndLater.volt.voltRailGrpGetStatus),

    SSMD_ENTRY(ADC_DEVICE_GRP,  SET,        pascalAndLater.clk.clkAdcDeviceGrpSet),
    SSMD_ENTRY(ADC_DEVICE_GRP,  GET_STATUS, pascalAndLater.clk.clkAdcDeviceGrpGetStatus),

    SSMD_ENTRY(CLK_DOMAIN_GRP,  SET,        pascalAndLater.clk.clkDomainGrpSet),

    SSMD_ENTRY(CLK_FREQ_CONTROLLER_GRP,  SET, pascalAndLater.clk.clkFreqControllerGrpSet),

    SSMD_ENTRY(NAFLL_DEVICE_GRP,  SET,        pascalAndLater.clk.clkNafllDeviceGrpSet),
    SSMD_ENTRY(NAFLL_DEVICE_GRP,  GET_STATUS, pascalAndLater.clk.clkNafllDeviceGrpGetStatus),

    SSMD_ENTRY(CLK_PROG_GRP,  SET, pascalAndLater.clk.clkProgGrpSet),

    SSMD_ENTRY(CLK_VF_POINT_GRP, SET,        pascalAndLater.clk.clkVfPointGrpSet.pri),
    SSMD_ENTRY(CLK_VF_POINT_GRP, GET_STATUS, pascalAndLater.clk.clkVfPointGrpGetStatus.pri),

    SSMD_ENTRY(FREQ_DOMAIN_GRP,  SET, pascalAndLater.clk.clkFreqDomainGrpSet),

    SSMD_ENTRY(VFE_EQU_GRP,  SET,        pascalAndLater.perf.vfeEquGrpSet),
    SSMD_ENTRY(VFE_VAR_GRP,  SET,        pascalAndLater.perf.vfeVarGrpSet),
    SSMD_ENTRY(VFE_VAR_GRP,  GET_STATUS, pascalAndLater.perf.vfeVarGrpGetStatus),

    SSMD_ENTRY(THERM_CHANNEL_GRP, SET, all.therm.thermChannelGrpSet),
    SSMD_ENTRY(THERM_DEVICE_GRP,  SET, all.therm.thermDeviceGrpSet),

    SSMD_ENTRY(CHANGE_SEQ_GRP,  SET, turingAndLater.changeSeq.scriptLwrr),

    SSMD_ENTRY(PSTATE_GRP, SET,        pascalAndLater.perf.pstateGrpSet),
    SSMD_ENTRY(PSTATE_GRP, GET_STATUS, pascalAndLater.perf.pstateGrpGetStatus),

    // This must remain last entry in the table. Insert new entries above!
    {
        .id = DRF_DEF(_RM_PMU_SUPER_SURFACE, _MEMBER_ID, _GROUP, _ILWALID),
    },
};

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   DMA out into the super-surface statically allocated hard-coded table
 *          of all super-surface members required for the LWGPU driver.
 */
FLCN_STATUS
cmdmgmtSuperSurfaceMemberDescriptorsInit_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, cmdmgmtSsmd)
    };

    // Ensure we do not corrupt the super-surface data.
    ct_assert(sizeof(SuperSurfaceMemberDescriptors) <=
              RM_PMU_SUPER_SURFACE_MEMBER_SIZE(ssmd));

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = ssurfaceWr(SuperSurfaceMemberDescriptors,
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ssmd),
            sizeof(SuperSurfaceMemberDescriptors));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
