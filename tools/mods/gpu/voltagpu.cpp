/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "voltagpu.h"

#include "class/cl0073.h"
#include "core/include/display.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0073.h"  // for LW0073_CTRL_SYSTEM_GET_NUM_HEADS_PARAMS
#include "ctrl/ctrl2080.h"
#include "device/interface/pcie.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpudev.h"
#include "gpu/lwlink/voltalwlink.h"
#include "gpu/perfmon/gv10xpm.h"
#include "gpu/utility/gm20xfalcon.h"
#include "gpu/utility/vbios.h"
#include "js_gpu.h"
#include "rmlsfm.h"
#include "volta/gv100/dev_bus.h"
#include "volta/gv100/dev_ext_devices.h"
#include "volta/gv100/dev_falcon_v4.h"
#include "volta/gv100/dev_fb.h"
#include "volta/gv100/dev_fifo.h"
#include "volta/gv100/dev_gc6_island.h"
#include "volta/gv100/dev_graphics_nobundle.h"
#include "volta/gv100/dev_host.h"
#include "volta/gv100/dev_ltc.h"
#include "volta/gv100/dev_mmu.h"
#include "volta/gv100/dev_lw_xve.h"
#include "volta/gv100/dev_pri_ringstation_fbp.h"
#include "volta/gv100/dev_pri_ringstation_gpc.h"
#include "volta/gv100/dev_pri_ringstation_sys.h"
#include "volta/gv100/dev_pwr_pri.h"
#include "volta/gv100/dev_ram.h"
#include "volta/gv100/dev_sec_pri.h"
#include "volta/gv100/dev_top.h"
#include "volta/gv100/dev_trim.h"
#include "volta/gv100/hwproject.h"

#include <numeric>             // for std::accumulate()
#include <functional>          // for logical_or
#include <algorithm>           // for min

namespace
{
    bool PollGraphicDeviceIdle(void * pArgs)
    {
        GpuSubdevice* pSubdev = (GpuSubdevice*)pArgs;
        UINT32 status = pSubdev->RegRd32(LW_PGRAPH_STATUS);
        return DRF_VAL(_PGRAPH, _STATUS, _STATE, status) ==
                       LW_PGRAPH_STATUS_STATE_IDLE;
    }
    const UINT32 DEFAULT_HBM_DUAL_RANKS = ~0U;
}

const array<ModsGpuRegAddress, 8> VoltaGpuSubdevice::m_Mc2MasterRegs =
{
    {
         MODS_PPRIV_FBP_FBP0_MC_PRIV_FS_CONFIG
        ,MODS_PPRIV_FBP_FBP1_MC_PRIV_FS_CONFIG
        ,MODS_PPRIV_FBP_FBP2_MC_PRIV_FS_CONFIG
        ,MODS_PPRIV_FBP_FBP3_MC_PRIV_FS_CONFIG
        ,MODS_PPRIV_FBP_FBP4_MC_PRIV_FS_CONFIG
        ,MODS_PPRIV_FBP_FBP5_MC_PRIV_FS_CONFIG
        ,MODS_PPRIV_FBP_FBP6_MC_PRIV_FS_CONFIG
        ,MODS_PPRIV_FBP_FBP7_MC_PRIV_FS_CONFIG
    }
};

constexpr UINT32 VoltaGpuSubdevice::m_FbpsPerSite;

VoltaGpuSubdevice::VoltaGpuSubdevice
(
    const char *       name,
    Gpu::LwDeviceId    deviceId,
    const PciDevInfo * pPciDevInfo
) :
    GpuSubdevice(name, deviceId, pPciDevInfo)
   ,m_FbDualRanks(DEFAULT_HBM_DUAL_RANKS)
   ,m_FbMc2MasterMask(0)
{
    ADD_NAMED_PROPERTY(FbMc2MasterMask, UINT32, 0);

    // Used to clear pending interrupts
    IntRegAdd(MODS_PMC_INTR_PDISP, 0,
              MODS_PDISP_FE_RM_INTR_EN_HEAD_LWDPS, 0, 0x0);
    IntRegAdd(MODS_PMC_INTR_PDISP, 0,
              MODS_PDISP_FE_RM_INTR_EN_EXC_WIN,    0, 0x0);
    IntRegAdd(MODS_PMC_INTR_PDISP, 0,
              MODS_PDISP_FE_RM_INTR_EN_EXC_OTHER,  0, 0x0);
    IntRegAdd(MODS_PMC_INTR_PDISP, 0,
              MODS_PDISP_FE_RM_INTR_EN_CTRL_DISP,  0, 0x0);
    IntRegAdd(MODS_PMC_INTR_PDISP, 0,
              MODS_PDISP_FE_RM_INTR_EN_OR,         0, 0x0);
    for (UINT32 headIdx = 0;
         Regs().IsSupported(MODS_PDISP_FE_RM_INTR_EN_HEAD_TIMING, headIdx);
         ++headIdx)
    {
        IntRegAdd(MODS_PMC_INTR_PDISP, 0,
                  MODS_PDISP_FE_RM_INTR_EN_HEAD_TIMING, headIdx, 0x0);
    }

    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLTLC_TX_MULTICAST_ERR_FIRST_0,     0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLTLC_RX_MULTICAST_ERR_FIRST_0,     0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLTLC_TX_MULTICAST_ERR_STATUS_0,    0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLTLC_RX_MULTICAST_ERR_STATUS_0,    0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLTLC_RX_MULTICAST_ERR_FIRST_1,     0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLTLC_RX_MULTICAST_ERR_STATUS_1,    0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLTLC_RX_MULTICAST_ERR_STATUS_1,    0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLIPT_ERR_UC_FIRST_COMMON,          0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWLIPT_ERR_UC_STATUS_COMMON,         0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PIOCTRLMIF_TX_MULTICAST_ERR_FIRST_0,  0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PIOCTRLMIF_TX_MULTICAST_ERR_STATUS_0, 0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PIOCTRLMIF_RX_MULTICAST_ERR_FIRST_0,  0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PIOCTRLMIF_RX_MULTICAST_ERR_STATUS_0, 0, ~0x0); //$
    IntRegAdd(MODS_PMC_INTR_LWLINK, 0, MODS_PLWL_MULTICAST_INTR,                  0, ~0x0); //$

    IntRegAdd(MODS_PMC_INTR_PMGR, 0, MODS_PMGR_RM_INTR_GPIO_LIST_1, 0, ~0x0);
    IntRegAdd(MODS_PMC_INTR_PMGR, 0, MODS_PMGR_RM_INTR_GPIO_LIST_2, 0, ~0x0);
    IntRegAdd(MODS_PMC_INTR_PMGR, 0, MODS_PMGR_RM_INTR_I2C,         0, ~0x0);
    IntRegAdd(MODS_PMC_INTR_PMGR, 0, MODS_PMGR_RM_INTR_AUX,         0, ~0x0);
}

//-------------------------------------------------------------------------------
GpuPerfmon * VoltaGpuSubdevice::CreatePerfmon(void)
{
    return (new GV100GpuPerfmon(this));
}
///
// Falcon's UCODE status is stored in the SCTL register. Since we have to deal
// with many falcons copying the bitfield of UCODE level
//
#define LW_FALCON_SCTL_UCODE_LEVEL                         5:4
#define LW_FALCON_SCTL_UCODE_LEVEL_P0                      0x00000000
#define LW_FALCON_SCTL_UCODE_LEVEL_P1                      0x00000001
#define LW_FALCON_SCTL_UCODE_LEVEL_P2                      0x00000002
#define LW_FALCON_SCTL_UCODE_LEVEL_P3                      0x00000003

//------------------------------------------------------------------------------
// Load the chip specific Method Macro Expansion (MME) data
// | GP100 yes | GP104 ?? | GP107 ?? | GP108 ?? |
/* virtual */ RC VoltaGpuSubdevice::LoadMmeShadowData()
{
    StickyRC rc;

    ClearMmeShadowMethods();
    #define ADD_MME_SHADOW_METHOD(shadowClass, shadowMethod) \
                    AddMmeShadowMethod(shadowClass, shadowMethod, false)
    #define ADD_MME_SHARED_SHADOW_METHOD(shadowClass, shadowMethod) \
                    AddMmeShadowMethod(shadowClass, shadowMethod, true)

    switch (DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GV100)
        case Gpu::GV100:
#endif
#if LWCFG(GLOBAL_CHIP_T194)
        case Gpu::T194: // GV11B
#endif
            #include "mmedata/gvlit1mme.h"
            break;
        default:
            Printf(Tee::PriWarn, "GPU has unknown MME methods\n");
            rc = RC::UNSUPPORTED_FUNCTION;
    }

    #undef ADD_MME_SHADOW_METHOD
    #undef ADD_MME_SHARED_SHADOW_METHOD

    return rc;
}

//------------------------------------------------------------------------------
void VoltaGpuSubdevice::CreatePteKindTable()
{
    m_PteKindTable.clear();

    AddCommolwoltaPteKinds();

}

//------------------------------------------------------------------------------
void VoltaGpuSubdevice::AddCommolwoltaPteKinds()
{
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(INVALID));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(PITCH));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS2_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS4_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS8_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS16_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_2Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS2_2Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS4_2Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS8_2Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS16_2Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS2_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS4_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS8_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS16_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_4CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS2_4CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS4_4CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS8_4CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z16_MS16_4CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS2_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS4_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS8_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS16_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS2_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS4_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS8_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS16_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS2_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS4_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS16_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS2_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS4_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS8_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8Z24_MS16_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_2CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_2CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_2CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_2CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_2ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_2ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_2ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_2ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC12_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS4_VC4_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC8_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(V8Z24_MS8_VC24_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS2_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS4_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS8_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS16_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS2_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS4_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS16_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS2_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS4_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS8_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS16_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS2_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS4_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS8_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24S8_MS16_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_2CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_2CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_2CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_2CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_2ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_2ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_2ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_2ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC12_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS4_VC4_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC8_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(Z24V8_MS8_VC24_4CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS2_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS4_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS8_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS16_1Z));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS2_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS4_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS16_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS2_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS4_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS8_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_MS16_2CZ));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_1CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_1CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_1CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_1CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC12_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS4_VC4_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC8_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8Z24_X16V8S8_MS8_VC24_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_1ZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_1CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_1CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_1CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_1CZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC12_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS4_VC4_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC8_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X16V8S8_MS8_VC24_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS2_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS4_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS8_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS16_1CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS2_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS4_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS8_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS16_2CSZV));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS2_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS4_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS8_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(ZF32_X24S8_MS16_2CS));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(S8_2S));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(GENERIC_16BX2));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_2CBR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_2CBA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_2CRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_2BRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS2_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS2_2CBR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS4_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS4_2CBR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS4_2CBA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS4_2CRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS4_2BRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS4_4CBRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS8_MS16_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS8_MS16_2CRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_2CBR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_2CBA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_2CRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_2BRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS2_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS2_2CBR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS4_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS4_2CBR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS4_2CBA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS4_2CRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS4_2BRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS4_4CBRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS8_MS16_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS8_MS16_2CRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C128_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C128_2CR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C128_MS2_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C128_MS2_2CR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C128_MS4_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C128_MS4_2CR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C128_MS8_MS16_2C));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C128_MS8_MS16_2CR));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(X8C24));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(PITCH_NO_SWIZZLE));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(SMSKED_MESSAGE));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(SMHOST_MESSAGE));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C32_MS2_4CBRA));
    m_PteKindTable.push_back(PTE_KIND_TABLE_ENTRY(C64_MS2_4CBRA));
}

//------------------------------------------------------------------------------
bool VoltaGpuSubdevice::FaultBufferOverflowed() const
{
    return !Regs().Test32(
            MODS_PFB_PRI_MMU_FAULT_STATUS_REPLAYABLE_OVERFLOW_RESET);
}

//------------------------------------------------------------------------------
/* virtual */
// GP100 yes | GP102 ??? | GP104 ??? | GP107 ??? | GP108 ??? |
void VoltaGpuSubdevice::ApplyMemCfgProperties(bool inPreVBIOSSetup)
{
    JavaScriptPtr pJs;
    UINT32 extraPfbCfg0 = 0, extraPfbCfg0Mask = 0;
    if (GetFbGddr5x8Mode())
    {
        extraPfbCfg0     |= Regs().SetField(MODS_PFB_FBPA_CFG0_CLAMSHELL_X16);
        extraPfbCfg0Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG0_CLAMSHELL);

        extraPfbCfg0     |= Regs().SetField(MODS_PFB_FBPA_CFG0_X8_ENABLED);
        extraPfbCfg0Mask |= Regs().LookupMask(MODS_PFB_FBPA_CFG0_X8);
    }

    GetMemCfgOverrides();
    bool reqFilterOverrides = false;
    if (m_InpPerFbpRowBits.size() == 1)
    {
        //We will be using FbRowBits js arg for setting the broadcast reg
        //This will override the value given by Mods argument -fb_row <val>
        Printf(Tee::PriLow,
                "Warning: Possibly overriding the value given to -fb_row\n");
        pJs->SetProperty(Gpu_Object, Gpu_FbRowBits, m_InpPerFbpRowBits[0]);
    }

    reqFilterOverrides = (m_InpPerFbpRowBits.size() > 1) ||
                         (m_FbDualRanks != DEFAULT_HBM_DUAL_RANKS) ||
                         m_FbMc2MasterMask;

    // On hardware or emulation, these are real DRAMs, and you can't just tweak
    // the settings willy-nilly.  You need to use an appropriate VBIOS instead.
    //
    // FB MC2 Master however can be set on hardware so do not check it here
    if (((m_InpPerFbpRowBits.size() > 1) ||
         (m_FbDualRanks != DEFAULT_HBM_DUAL_RANKS)) &&
        (Platform::GetSimulationMode() == Platform::Hardware))
    {
        Printf(Tee::PriError,
               "Cannot override FB settings on emulation/hardware\n");
        MASSERT(!"Generic assertion failure<refer to previous message>.");
        return;
    }

    GpuSubdevice::ApplyMemCfgProperties_Impl(
            inPreVBIOSSetup, extraPfbCfg0, extraPfbCfg0Mask,
            reqFilterOverrides);

    if (inPreVBIOSSetup || !reqFilterOverrides)
        return;

    //Take care of override registers that are not in the
    //list of Arguments supplied to baseclass Override method
    UINT32 fbpIdx = 0, activeFbps = 0;
    const UINT32 stride = Regs().LookupAddress(MODS_FBPA_PRI_STRIDE);
    UINT32 cfg1Reg = Regs().LookupAddress(MODS_PFB_FBPA_0_CFG1);
    const UINT32 cfg1RowBaseVal =
        10 - Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_10);

    activeFbps = Utility::CountBits(GetFsImpl()->FbMask());
    for (; (fbpIdx < activeFbps) && (fbpIdx < m_InpPerFbpRowBits.size());
         fbpIdx++)
    {
        //Per Requested FBPA
        if (!m_InpPerFbpRowBits[fbpIdx])
        {
            continue;
        }
        UINT32 cfg1val = m_InpPerFbpRowBits[fbpIdx];
        UINT32 pfbCfg1 = RegRd32(cfg1Reg);

        Regs().SetField(&pfbCfg1, MODS_PFB_FBPA_CFG1_ROWA,
                        cfg1val - cfg1RowBaseVal);
        Regs().SetField(&pfbCfg1, MODS_PFB_FBPA_CFG1_ROWB,
                        cfg1val - cfg1RowBaseVal);
        RegWr32(cfg1Reg, pfbCfg1);
        cfg1Reg += stride;
    }

    if (m_FbDualRanks != DEFAULT_HBM_DUAL_RANKS)
    {
        UINT32 regValue = Regs().Read32(MODS_PFB_FBPA_HBM_CFG0);
        Regs().SetField(&regValue, MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_ENABLE);

        switch (m_FbDualRanks)
        {
            case 0:
                // Zero means no user argument specified, so keep the default
                // register value.
                break;
            case 8:
                Regs().SetField(&regValue,
                                MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_8);
                break;
            case 16:
                Regs().SetField(&regValue,
                                MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_16);
                break;
            default:
                Printf(Tee::PriError,
                       "invalid -fb_hbm_ranks_per_bank argument value %u.\n",
                       m_FbDualRanks);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }
        Regs().Write32(MODS_PFB_FBPA_HBM_CFG0, regValue);
    }

    if (IsFbMc2MasterValid() && (m_FbMc2MasterMask != 0))
    {
        UINT32 fbpMask = GetFsImpl()->FbpMask();
        for (size_t ii = 0; ii < m_Mc2MasterRegs.size(); ii++)
        {
            if (fbpMask & (1 << ii))
            {
                Regs().Write32(m_Mc2MasterRegs[ii], 4, GetFbMc2MasterValue(ii));
            }
        }
    }
}

#ifdef LW_VERIF_FEATURES
/* virtual */
// GP100 yes | GP102 ??? | GP104 ??? | GP107 ??? | GP108 ??? |
void VoltaGpuSubdevice::FilterMemCfgRegOverrides
(
    vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS>* pOverrides
)
{
    // For each FB config register we are overriding, also override the matching
    // Multicast priv register, in each range.
    // This keeps the devinit override list safe from vbios that uses multicast.
    // See bug 787526.

    const UINT32 offsetMC0 =
        Regs().LookupAddress(MODS_PFB_FBPA_MC_0_PRI_FBPA_CG) -
        Regs().LookupAddress(MODS_PFB_FBPA_PRI_FBPA_CG);
    const UINT32 offsetMC1 =
        Regs().LookupAddress(MODS_PFB_FBPA_MC_1_PRI_FBPA_CG) -
        Regs().LookupAddress(MODS_PFB_FBPA_PRI_FBPA_CG);
    const UINT32 offsetMC2 =
        Regs().LookupAddress(MODS_PFB_FBPA_MC_2_PRI_FBPA_CG) -
        Regs().LookupAddress(MODS_PFB_FBPA_PRI_FBPA_CG);

    //So far GM20x and GM10x are same. (repeat Broadcast reg settings in
    //corresponding unicast regs)
    GpuSubdevice::FilterMemCfgRegOverrides(offsetMC0, offsetMC1,
                                                      offsetMC2, pOverrides);
    UINT32 fbpIdx = 0, activeFbps = 0;
    const UINT32 stride = Regs().LookupAddress(MODS_FBPA_PRI_STRIDE);
    UINT32 cfg1Reg = Regs().LookupAddress(MODS_PFB_FBPA_0_CFG1);
    const UINT32 cfg1RowBaseVal =
        10 - Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_10);

    activeFbps = Utility::CountBits(GetFsImpl()->FbMask());
    const UINT32 fbps = GetFsImpl()->FbMask();
    MASSERT(fbps);

    LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS param = {0};
    for (; (fbpIdx < activeFbps) && (fbpIdx < m_InpPerFbpRowBits.size());
         fbpIdx++)
    {
        //Per Requested FBPA
        if (!(fbps & (1<<fbpIdx)))
        {
            Printf(Tee::PriDebug, "Skip fbpIdx %u\n", fbpIdx);
            continue;
        }
        UINT32 cfg1val = m_InpPerFbpRowBits[fbpIdx];
        if (!cfg1val)
            continue;

        UINT32 pfbCfg1 = 0;
        UINT32 pfbCfg1Mask = 0;
        Regs().SetField(&pfbCfg1, MODS_PFB_FBPA_CFG1_ROWA,
                        cfg1val - cfg1RowBaseVal);
        Regs().SetField(&pfbCfg1, MODS_PFB_FBPA_CFG1_ROWB,
                        cfg1val - cfg1RowBaseVal);
        pfbCfg1Mask |= (Regs().LookupMask(MODS_PFB_FBPA_CFG1_ROWA) |
                        Regs().LookupMask(MODS_PFB_FBPA_CFG1_ROWB));

        bool cfgRegfound = false;
        for (UINT32 i = 0; i < pOverrides->size(); i++)
        {
            //Override the already prepared FBPA_[i] register value if it exists
            param = (*pOverrides)[i];
            if (param.regAddr == cfg1Reg)
            {
                cfgRegfound = true;
                (*pOverrides)[i].orMask = pfbCfg1;
                break;
            }
        }
        if (!cfgRegfound)
        {
            //Add newer index if required
            param.regAddr = cfg1Reg;
            param.orMask = pfbCfg1;
            param.andMask = ~pfbCfg1Mask;
            pOverrides->push_back(param);
        }
        cfg1Reg += stride;
    }

    if (m_FbDualRanks != DEFAULT_HBM_DUAL_RANKS)
    {
        UINT32 dualRankMask =
            Regs().LookupMask(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK);
        UINT32 dualRankVal  =
            Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_ENABLE);

        switch (m_FbDualRanks)
        {
            case 0:
                // Zero means no user argument specified, so keep the default
                // register value.
                break;
            case 8:
                dualRankMask |=
                    Regs().LookupMask(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK);
                dualRankVal  |=
                    Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_8);
                break;
            case 16:
                dualRankMask |=
                    Regs().LookupMask(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK);
                dualRankVal  |=
                    Regs().SetField(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_16);
                break;
            default:
                Printf(Tee::PriError,
                       "Invalid -fb_hbm_ranks_per_bank argument value %u.\n",
                       m_FbDualRanks);
                MASSERT(!"Generic assertion failure<refer to previous message>.");
                break;
        }

        //Add newer index if required
        param.regAddr = Regs().LookupAddress(MODS_PFB_FBPA_HBM_CFG0);
        param.orMask  = dualRankVal;
        param.andMask = ~dualRankMask;
        pOverrides->push_back(param);
    }

    if (IsFbMc2MasterValid() && (m_FbMc2MasterMask != 0))
    {
        UINT32 fbpMask = GetFsImpl()->FbpMask();
        for (size_t ii = 0; ii < m_Mc2MasterRegs.size(); ii++)
        {
            if (!(fbpMask & (1 << ii)))
                continue;

            param.regAddr = Regs().LookupAddress(m_Mc2MasterRegs[ii], 4);
            param.orMask  = GetFbMc2MasterValue(ii);
            param.andMask = 0;
            pOverrides->push_back(param);
        }
    }
}
#endif

//------------------------------------------------------------------------------
/* virtual */ void VoltaGpuSubdevice::SetupFeatures()
{
    //TODO: Verify these features apply to Volta GPUs
    GpuSubdevice::SetupFeatures();

    if (IsInitialized())
    {
        // Setup features that require the system to be properly initialized
        // (i.e. LwRm can be accessed, all GpuSubdevice data setup, registers
        // can be accessed, etc.)

        if (!HasSORXBar())
            ClearHasFeature(GPUSUB_SUPPORTS_SOR_XBAR);

        //For volta+ devices, supports LWLINK connections from GPU to system memory
        if (SupportsInterface<LwLink>() && GetInterface<LwLink>()->IsPostDiscovery())
        {
            vector<LwLink::EndpointInfo> endPointInfo;
            if (GetInterface<LwLink>()->GetDetectedEndpointInfo(&endPointInfo) == OK)
            {
                for (UINT32 i=0; i < endPointInfo.size(); i++)
                    if (endPointInfo[i].devType ==  TestDevice::TYPE_IBM_NPU)
                    {
                        SetHasFeature(GPUSUB_SUPPORTS_CPU_FB_MAPPINGS_THROUGH_LWLINK);
                        break;
                    }
            }
        }
    }
    else
    {
        // Setup features that are independant of system state
        SetHasFeature(GPUSUB_HAS_SEC_DEBUG);
        SetHasFeature(GPUSUB_SUPPORTS_GEN3_PCIE);
        SetHasFeature(GPUSUB_HAS_PRIV_SELWRITY);
        SetHasFeature(GPUSUB_SUPPORTS_PAGE_BLACKLISTING);

        // All display-enabled Maxwell GPUs support Dual Head midframe mclkswitch
        SetHasFeature(GPUSUB_SUPPORTS_DUAL_HEAD_MIDFRAME_SWITCH);

        SetHasFeature(GPUSUB_HAS_PEX_ASPM_L1SS);

        SetHasFeature(GPUSUB_STRAPS_FAN_DEBUG_PWM_66);
        SetHasFeature(GPUSUB_STRAPS_FAN_DEBUG_PWM_33);

        //Setup LW402C_CTRL_CMD_I2C_TABLE_GET_DEV_INFO Call support
        SetHasFeature(GPUSUB_HAS_RMCTRL_I2C_DEVINFO);

        //All Maxwell GPUs support LWPATH rendering extension
        SetHasFeature(GPUSUB_SUPPORTS_LWPR);

        SetHasFeature(GPUSUB_SUPPORTS_MULTIPLE_LAYERS_AND_VPORT);

        // Setup features that are independant of system state
        SetHasFeature(GPUSUB_SUPPORTS_UNCOMPRESSED_RAWIMAGE);

        //Display XBAR is a new feature added from gm20x+ chips.
        SetHasFeature(GPUSUB_SUPPORTS_SOR_XBAR);
        SetHasFeature(GPUSUB_SUPPORTS_SINGLE_HEAD_DUAL_SST);
        SetHasFeature(GPUSUB_SUPPORTS_SINGLE_HEAD_DUAL_MST);

        //ACR is supported on GM20X+ chips but not on GM20b.
        SetHasFeature(GPUSUB_SUPPORTS_ACR_REGION);

        SetHasFeature(GPUSUB_SUPPORTS_HDMI2);

        SetHasFeature(GPUSUB_SUPPORTS_16xMsAA);
        SetHasFeature(GPUSUB_SUPPORTS_TIR);
        SetHasFeature(GPUSUB_SUPPORTS_FILLRECT);
        SetHasFeature(GPUSUB_SUPPORTS_S8_FORMAT);

        SetHasFeature(GPUSUB_SUPPORTS_SYSMEM_REFLECTED_MAPPING);

        // VASpace specific big page size is supported on gm20x+ chips
        SetHasFeature(GPUSUB_SUPPORTS_PER_VASPACE_BIG_PAGE);

        // Secure HW scrubber is present on GP100+ chips (except gp10b)
        SetHasFeature(GPUSUB_HAS_SELWRE_FB_SCRUBBER);

        // GPU has boards.db version 2 entries
        SetHasFeature(GPUSUB_SUPPORTS_BOARDS_DB_V2);

        // FS Check support available Volta+
        SetHasFeature(GPUSUB_SUPPORTS_FS_CHECK);

        // POR Bin data is available Volta+
        SetHasFeature(GPUSUB_SUPPORTS_POR_BIN_CHECK);

        // Volta+ have channel subcontexts
        SetHasFeature(GPUSUB_HAS_SUBCONTEXTS);

        // RM supports retrieving VBIOS GUID
        SetHasFeature(GPUSUB_SUPPORTS_VBIOS_GUID);

        // ECC errors can be priv-injected
        SetHasFeature(GPUSUB_SUPPORTS_ECC_PRIV_INJECTION);

        // Power Regulators can be checked
        SetHasFeature(GPUSUB_SUPPORTS_PWR_REG_CHECK);

        SetHasFeature(GPUSUB_SUPPORTS_VULKAN);

        // L2 Bypass cannot be toggled in the middle of a MODS run
        ClearHasFeature(GPUSUB_SUPPORTS_RUNTIME_L2_MODE_CHANGE);
    }
}

//------------------------------------------------------------------------------
bool VoltaGpuSubdevice::IsFeatureEnabled(Feature feature)
{
    RegHal& regs = Regs();

    MASSERT(HasFeature(feature));
    switch (feature)
    {
        case GPUSUB_HAS_SEC_DEBUG:
        {
            return regs.Test32(MODS_FUSE_OPT_SELWRE_SECENGINE_DEBUG_DIS_DATA_NO);
        }

        case GPUSUB_HAS_KFUSES:
        {
            LwRmPtr pLwRm;
            if (pLwRm->IsClassSupported(LW04_DISPLAY_COMMON, GetParentDevice()))
            {
                LW0073_CTRL_SYSTEM_GET_NUM_HEADS_PARAMS Params = { 0 };
                Params.subDeviceInstance = GetSubdeviceInst();
                RC rc = GetParentDevice()->GetDisplay()->RmControl(
                        LW0073_CTRL_CMD_SYSTEM_GET_NUM_HEADS,
                        &Params, sizeof(Params));
                if (OK != rc)
                {
                    Printf(Tee::PriLow, "Warning: Invalid RmCall to get disp"
                            "heads. We can't surely say if display is disabled.\n");
                }
                else if (Params.numHeads)
                { //Display is enabled
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                //This is a display less chip. (Refer kernel/inc/classhal.h)
                return false;
            }
            //Intentional fall through for case of failed RmCtrl call
        }
        case GPUSUB_HAS_AZALIA_CONTROLLER:
        {
            bool featureEnabled = true;

            // Display engine can be fused out
            if (regs.Test32(MODS_FUSE_STATUS_OPT_DISPLAY_DATA_DISABLE))
            {
                featureEnabled = false;
            }

            // Azalia can be disabled by LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC
            // or by all SORs being strapped out
            if (feature == GPUSUB_HAS_AZALIA_CONTROLLER)
            {
                const UINT32 MAX_SORS = 4;
                const UINT32 SOR_MASK = ((1 << MAX_SORS) - 1) << 8;
                const UINT32 XV1_REG = DRF_BASE(LW_PCFG) + LW_XVE_PRIV_XV_1;
                if (FLD_TEST_DRF(_XVE, _PRIV_XV_1, _CYA_XVE_MULTIFUNC,
                                 _DISABLE, RegRd32(XV1_REG)) ||
                    ((RegRd32(LW_PGC6_SCI_STRAP) & SOR_MASK) == 0))
                {
                    featureEnabled = false;
                }
                MASSERT(featureEnabled || !IsAzaliaMandatory());
            }
            return featureEnabled;
        }
        // -----  GF100GpuSubdevice::IsFeatureEnabled  -----
        case GPUSUB_STRAPS_PEX_PLL_EN_TERM100:
            return regs.Test32(MODS_FUSE_OPT_PEX_PLL_EN_TERM100_DATA_YES);

        // Hardware always assumes there is a ROM and exits gracefully if no ROM
        // is found. IFR (Init from ROM) will no longer check for the ROM's
        // presence so no need to have the NOROM error field. Therefore
        // LW_PEXTDEV_BOOT_0_STRAP_SUB_VENDOR has been removed(see B1040649)
        case GPUSUB_STRAPS_SUB_VENDOR:
            return false;

        // Register no longer exists on Maxwell.
        case GPUSUB_STRAPS_SLOT_CLK_CFG:
            return false;

        case GPUSUB_STRAPS_FAN_DEBUG_PWM_66:
            return FLD_TEST_DRF(_PGC6, _SCI_FAN_DEBUG, _PWM_STRAP, _66,
                                RegRd32(LW_PGC6_SCI_FAN_DEBUG));

        case GPUSUB_STRAPS_FAN_DEBUG_PWM_33:
            return FLD_TEST_DRF(_PGC6, _SCI_FAN_DEBUG, _PWM_STRAP, _33,
                                RegRd32(LW_PGC6_SCI_FAN_DEBUG));

        default:
            return GpuSubdevice::IsFeatureEnabled(feature);
    }
}

//------------------------------------------------------------------------------
/* virtual */ void VoltaGpuSubdevice::SetupBugs()
{
    if (IsInitialized())
    {
        // Setup bugs that require the system to be properly initialized
        // (i.e. LwRm can be accessed, all GpuSubdevice data setup, registers
        // can be accessed, etc.)

        // This bug is for the vttgen misc1_pad which has hardcoded values for
        // all but chip gf106.
        SetHasBug(548966);
        // for gf106, the vttgen i2c_pad has the hardcoded values-bug 569987
        SetHasBug(569987);

        // Maxwell gpu need iddq value to be multiplied by 50
        // Speedo values are correct.
        SetHasBug(598305);
    }
    else
    {
        // Setup bugs that do not require the system to be properly initialized

        // G80 and above should not touch VGA registers on simulation
        // (TODO : This was fixed...where?)
        SetHasBug(143732);

        // Bug 321377 : G82+ cannot set PCLK directly
        SetHasBug(321377);

        //Pri ring gets broken if device in daisy chain gets floorswept.
        //Speedos can't be read in all partitions because of this.
        SetHasBug(598576);

        // Fuse reads are inconsistent. Sometimes get back 0xFFFFFFFF. Need fuse
        // related code to make sure we have consistent reads first
        SetHasBug(622250);

        // In 8+24 VCAA mode one fine-raster transaction carries 1x2 pixels
        // worth of data. So to generate one quad (2X2 pixels) for SM, PROP
        // needs to stitch 2 transaction together.
        // TC might run out of it's resources when it sees the first
        // transaction of the quad, and it may force tile_end and flush the
        // tile with only first half of the quad.
        // That would cause PROP to break quad into 2 transactions with 1x2
        // pixels covered in each and they both will end-up in different
        // subtiles.
        SetHasBug(618730);

        // TXD instructions need inputs for partial derivitives sanitized on
        // non-pixel shaders
        SetHasBug(630560);

        // PCIE correctable errors during link speed toggle
        SetHasBug(361633);

        // Floorsweeping not compatible with GC6 cycles (all GMxxx)
        SetHasBug(1173195);

        // If DVFS is enabled, one cannot rely on PLL_LOCK bit for GPC PLL
        SetHasBug(1371047);

        // Lwca layer wont place USERD in sysmem unless all allocations are in sysmem
        SetHasBug(1862474);

        // Volta LwDisplay tests are not compatible with parallel test groups
        SetHasBug(1888064);

        // Volta (like GP10x) lwrrently requires debug mode for SEC 2 test to run
        SetHasBug(1746899);

        // Bug in LWCA NANOSLEEP instruction
        SetHasBug(2212722);

        //Compute shaders timeout when RC watchdog is enabled
        SetHasBug(2150982);

        // Zero area triangles with pre-snap onservative raster are not rendered correctly
        SetHasBug(2339796);

        // ECC injection is locked on release builds due to lack of priv mask on
        // injection register. Security concern.
        SetHasBug(2389946);

        SetHasBug(998963); // Need to rearm MSI after clearing the interrupt.
    }
}

/*************************************************************************************************/
// This version of  ReadHost2Jtag() allows the caller to target multiple jtag clusters
// (aka chiplets) on the same Host2Jtag read operation which is required for some HBM memory tests.
// The Distributed_DFT_Test_Control.docx describes all the supported types of Jtag access.
/* virtual */
RC VoltaGpuSubdevice::ReadHost2Jtag(
     vector<JtagCluster> &jtagClusters  // vector of { chipletId, instrId } tuples
     ,UINT32 chainLen                   // number of bits in the chain or cluster
     ,vector<UINT32> *pResult           // location to store the data
     ,UINT32 capDis                     // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit, default=0
     ,UINT32 updtDis)                   // set the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit, default=0
{
    UINT32 dwordEn = 0;
    UINT32 dwordEnMax = ((chainLen+31)/32);
    UINT32 data = 0;
    pResult->clear();
    Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;

    RegWr32(LW_PJTAG_ACCESS_CTRL, 0);
    while (dwordEn < dwordEnMax)
    {
        for (UINT32 ii = 0; ii < jtagClusters.size(); ii++)
        {
            // Write DWORD_EN, REG_LENGTH, BURST, for each chain.
            data = 0;
            if ((jtagClusters.size() > 1) && (ii != jtagClusters.size()-1))
            {
                data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _LAST_CLUSTER_ILW, 1, data);
            }
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _REG_LENGTH, chainLen>>11, data);
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _DWORD_EN, dwordEn>>6, data);
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _BURST, 1, data);
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _UPDT_DIS, updtDis, data);
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _CAPT_DIS, capDis, data);
            RegWr32(LW_PJTAG_ACCESS_CONFIG, data);

            data = 0;
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _REG_LENGTH, chainLen, data);
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _INSTR_ID,
                                   jtagClusters[ii].instrId, data);
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _CLUSTER_SEL,
                                   jtagClusters[ii].chipletId, data);
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _DWORD_EN, dwordEn, data);
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _REQ_CTRL, 1, data);
            RegWr32(LW_PJTAG_ACCESS_CTRL, data);
            // if it's the first jtagCtrl write, poll for status==1
            if (ii == 0)
            {
                // We expect CTRL_STATUS to become non-zero within 150us in the
                // worst case
                RC rc = POLLWRAP_HW(PollJtagForNonzero, static_cast<void*>(this), 1);
                if (OK != rc)
                {
                    Printf(pri, "Failed to read good Jtag Ctrl Status\n");
                    return RC::HW_STATUS_ERROR;
                }
            }
        }
        // now read the data
        data = RegRd32(LW_PJTAG_ACCESS_DATA);
        if ((dwordEn == dwordEnMax-1) && ((chainLen % 32) != 0))
        {
            data = data >> (32-(chainLen%32));
        }
        pResult->push_back(data);
        dwordEn++;
    }
    RegWr32(LW_PJTAG_ACCESS_CTRL, 0);

    Printf(pri, "ReadHost2Jtag: Read data size:%u [", (UINT32)pResult->size());
    for (UINT32 i = static_cast<UINT32>(pResult->size()); i > 0; i--)
    {
        Printf(pri, " 0x%x ", (*pResult)[i-1]);
    }
    Printf(pri, " ] from cluster(s) [");
    for (UINT32 i = 0; i < jtagClusters.size(); i++)
    {
        Printf(pri, " [ \"chipletId\":%u \"instrId\":%u ]", jtagClusters[i].chipletId,
               jtagClusters[i].instrId);
    }
    Printf(pri, " ] with chain length %u\n", chainLen);
    return (OK);
}

/*************************************************************************************************/
// This version of WriteHost2Jtag() allows the caller to target multiple jtag clusters
// (aka chiplets) on the same Host2Jtag write operation, which is required for some HBM memory
// tests. The Distributed_DFT_Test_Control.docx describes all the supported types of Jtag access.
/*virtual*/
RC VoltaGpuSubdevice::WriteHost2Jtag(
     vector<JtagCluster> &jtagClusters // vector of { chipletId, instrId } tuples
     ,UINT32 chainLen                  // number of bits in the chain
     ,const vector<UINT32> &inputData  // location to store the data
     ,UINT32 capDis                    // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit, default=0
     ,UINT32 updtDis)                  // sets the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit, default=0
{
    UINT32 dwordEn = 0;
    UINT32 data = 0;

    if (inputData.size() != ((chainLen+31)/32))
    {
        Printf(Tee::PriNormal, "Chainlength is incorrect for sizeof InputData[]\n");
        return RC::BAD_PARAMETER;
    }

    Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;
    Printf(pri, "WriteHost2Jtag: Writing data of size %u [", (UINT32)inputData.size());
    for (UINT32 i = static_cast<UINT32>(inputData.size()); i > 0;  i--)
    {
        Printf(pri, " 0x%x ", inputData[i-1]);
    }
    Printf(pri, " ] to cluster [");
    for (UINT32 i = 0; i < jtagClusters.size(); i++)
    {
        Printf(pri, " [ \"chipletId\":%u \"instrId\":%u ]", jtagClusters[i].chipletId,
               jtagClusters[i].instrId);
    }
    Printf(pri, " ] with chain length %u\n", chainLen);

    RegWr32(LW_PJTAG_ACCESS_CTRL, 0);
    for (UINT32 ii = 0; ii < jtagClusters.size(); ii++)
    {
        data = 0x00;
        // if multiple clusters and not the last cluster
        if ((jtagClusters.size() > 1) && (ii < jtagClusters.size()-1))
        {
            //Note the PLAIN_MODE is really the LAST_CLUSTER_ILW bit. The refmanuals are wrong
            data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _LAST_CLUSTER_ILW, 1, data);
        }
        data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _REG_LENGTH, chainLen>>11, data);
        data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _DWORD_EN, dwordEn>>6, data);
        data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _UPDT_DIS, updtDis, data);
        data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CONFIG, _CAPT_DIS, capDis, data);
        RegWr32(LW_PJTAG_ACCESS_CONFIG, data);

        data = 0;
        data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _REG_LENGTH, chainLen, data);
        data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _INSTR_ID, jtagClusters[ii].instrId, data);
        data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _CLUSTER_SEL, jtagClusters[ii].chipletId, data); //$
        data = FLD_SET_DRF_NUM(_PJTAG, _ACCESS_CTRL, _REQ_CTRL, 1, data);
        RegWr32(LW_PJTAG_ACCESS_CTRL, data);

        if (ii == 0)
        {
            // We expect CTRL_STATUS to become non-zero within 150us in the
            // worst case
            RC rc = POLLWRAP_HW(PollJtagForNonzero, static_cast<void*>(this), 1);
            if (OK != rc)
            {
                Printf(pri, "Failed to read good Jtag Ctrl Status\n");
                return RC::HW_STATUS_ERROR;
            }
        }
    }
    // write the data
    while (dwordEn < inputData.size())
    {
        RegWr32(LW_PJTAG_ACCESS_DATA, inputData[dwordEn]);
        dwordEn++;
    }
    RegWr32(LW_PJTAG_ACCESS_CTRL, 0);
    return OK;
}

//------------------------------------------------------------------------------
// Return the size (in bits) of the selwre_id field.
RC VoltaGpuSubdevice::GetSelwreKeySize(UINT32 *pChkSize, UINT32 *pCfgSize)
{
    RegHal& regs = TestDevice::Regs();

    *pChkSize = regs.LookupMaskSize(MODS_JTAG_SEC_CHK_testmaster_selwreid);
    *pCfgSize = regs.LookupAddress(MODS_JTAG_SEC_CFG_CHAINLENGTH);

    return OK;
}

//------------------------------------------------------------------------------
// Provide the caller with the required cluster information needed to unlock the
// jtag chains.
// Inputs: pInfo->selwreKeys: A vector containing the secure keys, must not be empty.
//
// Outputs: The requirement is to provide info on 3 chains.
// jtagClusters[0] = Command chain
// jtagClusters[1] = Config chain
// jtagClusters[2] = Check chain
RC VoltaGpuSubdevice::GetH2jUnlockInfo(h2jUnlockInfo *pInfo)
{
    RC rc;
    UINT32 hiBit, loBit;

    RegHal& regs = TestDevice::Regs();

    pInfo->numChiplets = regs.LookupAddress(MODS_JTAG_NUM_CLUSTERS);
    regs.LookupField(MODS_JTAG_SEC_CFG_features, &hiBit, &pInfo->cfgFeatureStartBit);
    regs.LookupField(MODS_JTAG_SEC_UNLOCK_STATUS_features, &hiBit, &pInfo->unlockFeatureStartBit);
    pInfo->numFeatureBits = regs.LookupMaskSize(MODS_JTAG_SEC_UNLOCK_STATUS_features);

    // Move the secure keys into the proper bit positions for the SEC_CHK chain.
    pInfo->selwreData.assign((regs.LookupAddress(MODS_JTAG_SEC_CHK_CHAINLENGTH)+31)/32, 0x00);
    regs.LookupField(MODS_JTAG_SEC_CHK_testmaster_selwreid, &hiBit, &loBit);
    CHECK_RC(Utility::CopyBits(pInfo->selwreKeys, // source
                               pInfo->selwreData, // dest
                               0,                 // src offset
                               loBit,            // dest offset
                               regs.LookupMaskSize(MODS_JTAG_SEC_CHK_testmaster_selwreid))); // numbits

    // Setup the bitstream for the SEC_CFG chain.
    pInfo->cfgData.assign((regs.LookupAddress(MODS_JTAG_SEC_CFG_CHAINLENGTH)+31)/32, 0x00);
    regs.LookupField(MODS_JTAG_SEC_CFG_features, &hiBit, &loBit);
    CHECK_RC(Utility::CopyBits(pInfo->cfgMask,
                               pInfo->cfgData,
                               0,
                               loBit,
                               regs.LookupMaskSize(MODS_JTAG_SEC_CFG_features)));

    // Setup the bitstream for the SEC_CHK chain after the secure data has been written
    pInfo->chkData.clear();
    pInfo->chkData.assign((regs.LookupAddress(MODS_JTAG_SEC_CHK_CHAINLENGTH)+31)/32, 0x00);

    // Setup the bitstream for the SEC_CMD chain
    pInfo->cmdData.assign((regs.LookupAddress(MODS_JTAG_SEC_CMD_CHAINLENGTH)+31)/32, 0x00);
    vector<UINT32> temp(1, 1);
    regs.LookupField(MODS_JTAG_SEC_CMD_testmaster_sha_cal_start, &hiBit, &loBit);
    CHECK_RC(Utility::CopyBits(temp,
                               pInfo->cmdData,
                               0,
                               loBit,
                               regs.LookupMaskSize(MODS_JTAG_SEC_CMD_testmaster_sha_cal_start)));

    // Setup the bitstream for the SEC_UNLOCK_STATUS chain.
    pInfo->unlockData.clear();
    pInfo->unlockData.assign((regs.LookupAddress(MODS_JTAG_SEC_UNLOCK_STATUS_CHAINLENGTH)+31)/32,
                             0x00);

    pInfo->clusters.clear();
    pInfo->clusters.push_back(GpuSubdevice::JtagCluster(regs.LookupAddress(MODS_JTAG_SEC_CMD_CLUSTER_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CMD_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CMD_CHAINLENGTH)));
    pInfo->clusters.push_back(GpuSubdevice::JtagCluster(regs.LookupAddress(MODS_JTAG_SEC_CFG_CLUSTER_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CFG_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CFG_CHAINLENGTH)));
    pInfo->clusters.push_back(GpuSubdevice::JtagCluster(regs.LookupAddress(MODS_JTAG_SEC_CHK_CLUSTER_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CHK_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_CHK_CHAINLENGTH)));
    // There are LW_JTAG_NUM_CLUSTERS_tuxxx clusters for the LW_JTAG_SEC_UNLOCK_STATUS_ID_tuxxx
    // chains. This entry gives the 1st cluster ID and total length for all the clusters.
    // access via pInfo->clusters[selwnlock]
    pInfo->clusters.push_back(GpuSubdevice::JtagCluster(0,
                                                        regs.LookupAddress(MODS_JTAG_SEC_UNLOCK_STATUS_ID),
                                                        regs.LookupAddress(MODS_JTAG_SEC_UNLOCK_STATUS_CHAINLENGTH)));
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */
// Unlock the Host2Jtag access.
// For unfused parts the secCfg can be 0xFFFFFFFF, and the contents of selwreKeys
// can be all zeros.
// For fused parts secCfg must be appropriate for the selwreKeys and are provided
// by the Green Hill Server.
RC VoltaGpuSubdevice::UnlockHost2Jtag
(
    const vector<UINT32> &secCfgMask,   // bitmask of features to unlock (see defines above)
    const vector<UINT32> &selwreKeys    // vector of secure keys for the provided bitmask
)
{
    RC rc;
    h2jUnlockInfo unlockInfo;
    unlockInfo.selwreKeys = selwreKeys;
    unlockInfo.cfgMask = secCfgMask;

    // Get the detailed data from the GPU class on how to unlock jtag.
    CHECK_RC(GetH2jUnlockInfo(&unlockInfo));
    CHECK_RC(UnlockHost2Jtag(&unlockInfo));
    return OK;
}

//------------------------------------------------------------------------------
// Verify the feature bits in the unlock status chain match the feature bits in
// the config chain. Note bit fields vary for different types of  GPU so the
// data must be overloaded.
RC VoltaGpuSubdevice::VerifyHost2JtagUnlockStatus(h2jUnlockInfo *pUnlockInfo)
{
    RC rc;
    vector<UINT32> cfgFeatures((pUnlockInfo->numFeatureBits+31)/32, 0x00);
    vector<UINT32> unlockFeatures((pUnlockInfo->numFeatureBits+31)/32, 0x00);
    CHECK_RC(Utility::CopyBits(pUnlockInfo->cfgData,    //src
                      cfgFeatures,                      //dst
                      pUnlockInfo->cfgFeatureStartBit,  //srcBitOffset
                      0,                                //dstBitOffset
                      pUnlockInfo->numFeatureBits));    //numBits

    CHECK_RC(Utility::CopyBits(pUnlockInfo->unlockData,     //src
                      unlockFeatures,                       //dst
                      pUnlockInfo->unlockFeatureStartBit,   //srcBitOffset
                      0,                                    //dstBitOffset
                      pUnlockInfo->numFeatureBits));        //numBits
    if (unlockFeatures != cfgFeatures)
    {
        Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;
        Printf(pri,
               "Failed to verify h2j unlock status. One or more features were not unlocked!\n");
        return RC::UNEXPECTED_RESULT;
    }
    return rc;
}

//------------------------------------------------------------------------------
RC VoltaGpuSubdevice::UnlockHost2Jtag(h2jUnlockInfo *pUnlockInfo)
{
    RC rc;
    Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;

    vector<GpuSubdevice::JtagCluster> jtagClusters;
    Printf(pri, "UnlockHost2Jtag:Start\n");
    // 1.Program JTAG_SEC_CFG chain with the features that need to be unlocked.
    jtagClusters.push_back(pUnlockInfo->clusters[secCfg]);
    CHECK_RC(WriteHost2Jtag(jtagClusters
                   ,pUnlockInfo->clusters[secCfg].chainLen //chainLen
                   ,pUnlockInfo->cfgData)); //InputData

    // 2.Program JTAG_SEC_CHK chain with the secure keys
    jtagClusters[0] = pUnlockInfo->clusters[secChk];
    CHECK_RC(WriteHost2Jtag(jtagClusters,
                            pUnlockInfo->clusters[secChk].chainLen,
                            pUnlockInfo->selwreData));

    //3.Program JTAG_SEC_CMD chain with value 0X1 to start SHA callwlation
    jtagClusters[0] = pUnlockInfo->clusters[secCmd];
    CHECK_RC(WriteHost2Jtag(jtagClusters,
                            pUnlockInfo->clusters[secCmd].chainLen,
                            pUnlockInfo->cmdData));

    //3a. Read back the secure keys from the security engine to give enough time for the SHA2
    //    callwlation to complete.
    jtagClusters[0] = pUnlockInfo->clusters[secChk];
    CHECK_RC(ReadHost2Jtag(jtagClusters,
                           pUnlockInfo->clusters[secChk].chainLen,
                           &pUnlockInfo->chkData));

    //4.Program JTAG_SEC_CMD chain with value 0x00 to clear out pending request.
    pUnlockInfo->cmdData.assign(pUnlockInfo->cmdData.size(), 0x00);
    jtagClusters[0] = pUnlockInfo->clusters[secCmd];
    CHECK_RC(WriteHost2Jtag(jtagClusters,
                            pUnlockInfo->clusters[secCmd].chainLen,
                            pUnlockInfo->cmdData));

    //5.Read JTAG JTAG_SEC_UNLOCK_STATUS register in all chiplets. This is a special register
    //  where the data is generated internally and gets distributed to all chains.
    jtagClusters.clear();
    for (UINT32 i = 0; i < pUnlockInfo->numChiplets; i++)
    {
        jtagClusters.push_back(GpuSubdevice::JtagCluster(
                        i, pUnlockInfo->clusters[selwnlock].instrId));
    }
    CHECK_RC(ReadHost2Jtag(jtagClusters,
                            pUnlockInfo->clusters[selwnlock].chainLen,
                            &pUnlockInfo->unlockData));

    //5a. Verify all of the requested features were unlocked.
    CHECK_RC(VerifyHost2JtagUnlockStatus(pUnlockInfo));

    Printf(pri, "UnlockHost2Jtag:End\n");
    return rc;
}

//------------------------------------------------------------------------------
void VoltaGpuSubdevice::ApplyJtagChanges(UINT32 processFlag)
{
    RC rc;
    Tee::Priority pri = (m_VerboseJtag) ? Tee::PriNormal : Tee::PriLow;
    if (!IsEmulation() &&
        (Platform::GetSimulationMode() == Platform::Hardware) &&
        Platform::HasClientSideResman())
    {
        if (processFlag != PostRMInit)
            return;

        h2jUnlockInfo unlockInfo;
        UINT32 secChkSize = 0;
        UINT32 secCfgSize = 0;
        if (OK != (rc = GetSelwreKeySize(&secChkSize, &secCfgSize)))
        {
            Printf(pri, "WARNING... unable to get sizes of SelwreKeys...\n");
            Printf(pri, "Jtag changes will not be applied.\n");
            return;
        }

        secChkSize = (secChkSize+31) / 32;
        unlockInfo.selwreKeys.assign(secChkSize, 0x00);

        secCfgSize = (secCfgSize+31) / 32;
        unlockInfo.cfgMask.assign(secCfgSize, 0xFFFFFFFF);
        if (OK != (rc = GetH2jUnlockInfo(&unlockInfo)))
        {
            Printf(pri,
                   "WARNING... failed to get Host2Jtag Unlock Info..\n");
            Printf(pri, "Jtag changes will not be applied.\n");
            return;
        }

        if (OK != (rc = UnlockHost2Jtag(&unlockInfo)))
        {
            Printf(pri, "WARNING... Failed to unlock Jtag for access!\n");
        }
    }

    return;
}

//------------------------------------------------------------------------------
/* virtual */bool VoltaGpuSubdevice::DoesFalconExist(UINT32 falconId)
{
    if (OK == InitFalconBases())
        return m_FalconBases.find(falconId) != m_FalconBases.end();
    return false;
}

//------------------------------------------------------------------------------
//! \brief saves the falcon status [NS, LS or HS] into pLevel
//! \ param pRegisterBases is  array containing base addresses
//! of the various falcons
//! \param falconId is the id of the falcon for which mode is
//! queried
//! \param pLevel is address where mode of falconId is stored
//!
/* virtual */ RC VoltaGpuSubdevice::GetFalconStatus
(
    UINT32 falconId,
    UINT32 *pLevel
)
{
    RC rc;
    CHECK_RC(InitFalconBases());

    *pLevel = RegRd32(m_FalconBases[falconId] + LW_PFALCON_FALCON_SCTL);
    *pLevel = DRF_VAL(_FALCON, _SCTL_, UCODE_LEVEL, *pLevel);

    return OK;
}

//------------------------------------------------------------------------------
// m_FalconBases map is filled with the falcon base addresses
// TODO: Verify that we actually have the falcon engines below.
/* virtual */RC VoltaGpuSubdevice::InitFalconBases()
{
    if (!m_FalconBases.empty())
    {
        return OK;
    }

    RC rc = OK;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams = { 0 };
    LwRmPtr pLwRm;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                               LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                               &engineParams,
                               sizeof (engineParams)));

    for (UINT32 i = 0; i < engineParams.engineCount; ++i)
    {
        switch (engineParams.engineList[i])
        {
            case LW2080_ENGINE_TYPE_LWENC0:
                m_FalconBases[LSF_FALCON_ID_LWENC0] = LW_FALCON_LWENC0_BASE;
            break;
            case LW2080_ENGINE_TYPE_LWENC1:
                m_FalconBases[LSF_FALCON_ID_LWENC1] = LW_FALCON_LWENC1_BASE;
            break;
            case LW2080_ENGINE_TYPE_LWENC2:
                m_FalconBases[LSF_FALCON_ID_LWENC2] = LW_FALCON_LWENC2_BASE;
            break;
            case LW2080_ENGINE_TYPE_SEC2:
                m_FalconBases[LSF_FALCON_ID_SEC2] = LW_PSEC_FALCON_IRQSSET;
            break;
            case LW2080_ENGINE_TYPE_GRAPHICS:
                m_FalconBases[LSF_FALCON_ID_FECS]  =
                                        LW_PGRAPH_PRI_FECS_FALCON_IRQSSET;
                m_FalconBases[LSF_FALCON_ID_GPCCS] = Regs().LookupAddress(
                    MODS_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQSSET);
            break;
            case LW2080_ENGINE_TYPE_BSP:
                m_FalconBases[LSF_FALCON_ID_LWDEC] = LW_FALCON_LWDEC_BASE;
            break;
        }
    }

    m_FalconBases[LSF_FALCON_ID_PMU] = LW_FALCON_PWR_BASE;

    return rc;
}

//------------------------------------------------------------------------------
void VoltaGpuSubdevice::PrintVoltaMonitorInfo(UINT32 flags)
{
    RegHal& regs = Regs();
    bool graphicsExists = true;
    bool videoExists = ((flags & MMONINFO_NO_VIDEO) == 0);
    UINT32 pbdmaChannelSize = regs.IsSupported(MODS_PPBDMA_CHANNEL) ?
        regs.LookupArraySize(MODS_PPBDMA_CHANNEL, 1) : 0;

    UINT32 fifoPbdmaStatusSize = regs.IsSupported(MODS_PFIFO_PBDMA_STATUS) ?
            regs.LookupArraySize(MODS_PFIFO_PBDMA_STATUS, 1) : 0;

    UINT32 fifoEngineStatusSize = regs.IsSupported(MODS_PFIFO_ENGINE_STATUS) ?
        regs.LookupArraySize(MODS_PFIFO_ENGINE_STATUS, 1) : 0;

    UINT32 fifoEngineStatusDebugSize = regs.IsSupported(MODS_PFIFO_ENGINE_STATUS_DEBUG) ?
        regs.LookupArraySize(MODS_PFIFO_ENGINE_STATUS_DEBUG, 1) : 0;

    // Bug 1965298: RegHal's having issues with LW_PFIFO_PBDMA_STATUS
    // because it's a scalar register on Fermi, but an array on Kepler+
    // UINT32 fifoPbdmaStatusSize = regs.IsSupported(MODS_PFIFO_PBDMA_STATUS) ?
    //     regs.LookupArraySize(MODS_PFIFO_PBDMA_STATUS, 1) : 0;

    // Check to see if certain engines are removed for emulation.
    // An engine that is stubbed out should not have its status checked.
    if (IsEmulation())
    {
        if (regs.Test32(MODS_PBUS_EMULATION_STUB0_GR_REMOVED))
        {
            graphicsExists = false;
            Printf(Tee::PriNormal, "Graphics engine stubbed out for emulation.\n");
        }

        if (videoExists &&
            regs.Test32(MODS_PBUS_EMULATION_STUB0_MSENC_REMOVED))
        {
            videoExists = false;
            Printf(Tee::PriNormal, "Video engine stubbed out for emulation.\n");
        }

        // Net 7 and below do not have PBDMA greater than 11
        if ((DeviceId() == Gpu::GV100) &&
            IsNetlistRevision0Valid() &&
            (NetlistRevision0() <= 7))
        {
            pbdmaChannelSize = min<UINT32>(pbdmaChannelSize, 11);
            Printf(Tee::PriNormal,
                   "PBDMA Channel 11 and above stubbed out on emulation netlist 6 and below.\n");

            fifoPbdmaStatusSize = min<UINT32>(fifoPbdmaStatusSize, 11);
            Printf(Tee::PriNormal,
                 "FIFO PBDMA Status 11 and above stubbed out on emulation netlist 6 and below.\n");

            fifoEngineStatusSize = min<UINT32>(fifoEngineStatusSize, 12);
            fifoEngineStatusDebugSize = min<UINT32>(fifoEngineStatusDebugSize, 12);
            Printf(Tee::PriNormal,
                "FIFO Engine Status 12 and above stubbed out on emulation netlist 6 and below.\n");
        }
    }

    string statusStr;

    if (graphicsExists)
    {
        UINT32 PgraphStatus = regs.Read32(MODS_PGRAPH_STATUS);
        statusStr = Utility::StrPrintf(" PGRAPH=0x%08x", PgraphStatus);
    }

    if (videoExists)
    {
        UINT32 LwdecStatus  = regs.Read32(MODS_PLWDEC_FALCON_IDLESTATE);
        statusStr += Utility::StrPrintf(" LWDEC=0x%08x", LwdecStatus);
        for (UINT32 ii = 0; regs.IsSupported(MODS_PLWENC_FALCON_IDLESTATE) &&
             ii < regs.LookupArraySize(MODS_PLWENC_FALCON_IDLESTATE, 1); ii++)
        {
            UINT32 LwencStatus  = regs.Read32(MODS_PLWENC_FALCON_IDLESTATE, ii);
            statusStr += Utility::StrPrintf(" LWENC%d=0x%08x", ii, LwencStatus);
        }
    }

    Printf(Tee::PriNormal, "Status:%s\n", statusStr.c_str());

    // Store the reads so we can print all values at once and avoid line splits
    vector<UINT32> temp(fifoPbdmaStatusSize);
    for (UINT32 i = 0; i < fifoPbdmaStatusSize; ++i)
    {
        temp[i] = RegRd32(LW_PFIFO_PBDMA_STATUS(i));
    }
    Printf(Tee::PriNormal, "LW_PFIFO_PBDMA_STATUS : ");
    for (auto val : temp)
    {
        Printf(Tee::PriNormal, "0x%08x ", val);
    }
    Printf(Tee::PriNormal, "\n");

    temp.resize(fifoEngineStatusSize);
    for (UINT32 i = 0; i < fifoEngineStatusSize; ++i)
    {
        temp[i] = regs.Read32(MODS_PFIFO_ENGINE_STATUS, i);
    }
    Printf(Tee::PriNormal, "LW_PFIFO_ENGINE_STATUS : ");
    for (auto val : temp)
    {
        Printf(Tee::PriNormal, "0x%08x ", val);
    }
    Printf(Tee::PriNormal, "\n");

    temp.resize(fifoEngineStatusDebugSize);
    for (UINT32 i = 0; i < fifoEngineStatusDebugSize; ++i)
    {
        temp[i] = regs.Read32(MODS_PFIFO_ENGINE_STATUS_DEBUG, i);
    }
    Printf(Tee::PriDebug, "LW_PFIFO_ENGINE_STATUS_DEBUG : ");
    for (auto val : temp)
    {
        Printf(Tee::PriDebug, "0x%08x ", val);
    }
    Printf(Tee::PriDebug, "\n");

    const UINT32 reg_size = 6;
    UINT32 reg[reg_size];
    fill(reg, reg + reg_size, 0);
    for (UINT32 i = 0; i < pbdmaChannelSize; ++i)
    {
        string label = Utility::StrPrintf("PPBDMA(%d):", i);
        reg[0] = regs.Read32(MODS_PPBDMA_CHANNEL, i);
        reg[1] = regs.Read32(MODS_PPBDMA_PB_HEADER, i);
        reg[2] = regs.Read32(MODS_PPBDMA_GP_GET, i);
        reg[3] = regs.Read32(MODS_PPBDMA_GP_PUT, i);
        reg[4] = regs.Read32(MODS_PPBDMA_GET, i);
        reg[5] = regs.Read32(MODS_PPBDMA_PUT, i);
        if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
        {
            Printf(Tee::PriNormal, "%10s CHANNEL=0x%08x PB_HEADER=0x%08x GP_GET=0x%08x"
                    " GP_PUT=0x%08x GET=0x%08x PUT=0x%08x\n",
                    label.c_str(), reg[0], reg[1], reg[2], reg[3], reg[4], reg[5]);
            fill(reg, reg + reg_size, 0);
        }

        reg[0] = regs.Read32(MODS_PPBDMA_PB_COUNT, i);
        reg[1] = regs.Read32(MODS_PPBDMA_PB_FETCH, i);
        reg[2] = regs.Read32(MODS_PPBDMA_PB_HEADER, i);
        reg[3] = regs.Read32(MODS_PPBDMA_INTR_0, i);
        if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
        {
            Printf(Tee::PriNormal, "%10s PB_COUNT=0x%08x PB_FETCH=0x%08x PB_HEADER=0x%08x"
                   " INTR_0=0x%08x\n",
                    label.c_str(), reg[0], reg[1], reg[2], reg[3]);
            fill(reg, reg + reg_size, 0);
        }

        reg[0] = regs.Read32(MODS_PPBDMA_METHOD0, i);
        reg[1] = regs.Read32(MODS_PPBDMA_DATA0, i);
        reg[2] = regs.Read32(MODS_PPBDMA_METHOD1, i);
        reg[3] = regs.Read32(MODS_PPBDMA_DATA1, i);
        if (0 != std::accumulate(reg, reg + reg_size, 0, logical_or<UINT32>()))
        {
            Printf(Tee::PriNormal, "%10s METHOD0=0x%08x DATA0=0x%08x METHOD1=0x%08x"
                   " DATA1=0x%08x\n",
                    label.c_str(), reg[0], reg[1], reg[2], reg[3]);
            fill(reg, reg + reg_size, 0);
        }
    }

    temp.resize(regs.LookupArraySize(MODS_CE_LCE_STATUS, 1));
    for (UINT32 i = 0; i < temp.size(); ++i)
    {
        temp[i] = regs.Read32(MODS_CE_LCE_STATUS, i);
    }
    Printf(Tee::PriNormal, "CE Status : ");
    for (UINT32 i = 0; i < temp.size(); i++)
    {
        Printf(Tee::PriNormal, "LCE%d=0x%08x ", i, temp[i]);
    }
    Printf(Tee::PriNormal, "\n");
}

//------------------------------------------------------------------------------
// TODO: This was copied from GM20xGpuSubdevice, verify it applies to Pascal.
void VoltaGpuSubdevice::GetMemCfgOverrides()
{
    //Only purpose is to process JS argument PerFbpRowBits
    //In future other MemCfg related arguments can be added here.

    JavaScriptPtr pJs;
    vector <UINT32> inpFbpRowBits;
    RC rc =  pJs->GetProperty(Gpu_Object, Gpu_PerFbpRowBits, &inpFbpRowBits);
    if (OK != rc || (inpFbpRowBits.empty()))
    {
        m_InpPerFbpRowBits.clear();
        Printf(Tee::PriDebug, "Note: fb_row_bits_perfbp isn't specified.\n");
        rc.Clear();
    }
    else
    {
        //Hardcoded value on any platform
        const UINT32 totalSupportedFbpas = RegRd32(LW_PTOP_SCAL_NUM_FBPAS);
        if (inpFbpRowBits.size() > totalSupportedFbpas)
        {
            Printf(Tee::PriLow,
                    "Warning: We discard unsupported Fbpa Cfg request.\n");
            inpFbpRowBits.erase(inpFbpRowBits.begin() + totalSupportedFbpas,
                    inpFbpRowBits.end());
        }
        //Limits for GM20x, are they the same for Pascal?
        const UINT32 minRowBits =
            Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_10);
        const UINT32 maxRowBits =
            Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_14);
        const UINT32 cfg1RowBaseVal =
            10 - Regs().LookupValue(MODS_PFB_FBPA_CFG1_ROWA_10);
        const UINT32 activeFbps = GetFsImpl()->FbMask();
        MASSERT(activeFbps); // Should not be read as non zero (even RTL)
        for (UINT32 ii=0; ii < inpFbpRowBits.size(); ii++)
        {
            if (!(activeFbps & (1<<ii)))
            {
                Printf(Tee::PriDebug,
                        "%s: Skip floorswept fbp %u\n",
                        __FUNCTION__, (1<<ii));
                inpFbpRowBits[ii] = 0;
                continue;
            }

            // To maintain similarlity to Ref-Man. definitions, input values
            // are taken to be Register value + cfg1RowBaseVal
            if ((inpFbpRowBits[ii] < minRowBits + cfg1RowBaseVal) ||
                (inpFbpRowBits[ii] > maxRowBits + cfg1RowBaseVal))
            { //value is out of range; try next regval
                Printf(Tee::PriWarn,
                        "Fbp Row bits (%u) is out of range."
                        " Not overriding Fbp_%u \n", ii, inpFbpRowBits[ii]);
                inpFbpRowBits[ii] = 0;
                continue;
            }
        }
        m_InpPerFbpRowBits = inpFbpRowBits;
    }

    bool fbHbmDualRankEnabled = false;
    rc = pJs->GetProperty(Gpu_Object, Gpu_FbHbmDualRankEnabled, &fbHbmDualRankEnabled);
    if (OK == rc && fbHbmDualRankEnabled)
    {
        m_FbDualRanks = 0;
        pJs->GetProperty(Gpu_Object, Gpu_FbHbmBanksPerRank, &m_FbDualRanks);
        rc.Clear();
    }

    NamedProperty *pFbMc2MasterMaskProp;
    UINT32         fbMc2MasterMask = 0;
    if ((OK != GetNamedProperty("FbMc2MasterMask", &pFbMc2MasterMaskProp)) ||
        (OK != pFbMc2MasterMaskProp->Get(&fbMc2MasterMask)))
    {
        Printf(Tee::PriDebug, "FB MC2 master configuration is not valid!!\n");
    }
    else
    {
        m_FbMc2MasterMask = fbMc2MasterMask;
    }
}

//------------------------------------------------------------------------------
//! \brief Queries the read mask, write mask and start addresses of
//!  the various ACR regions
//! \param self explanatory
//!
/* virtual */ RC VoltaGpuSubdevice::GetWPRInfo(UINT32 regionID,
                            UINT32 *pReadMask,
                            UINT32 *pWriteMask,
                            UINT64 *pStartAddr,
                            UINT64 *pEndAddr)
{
    RC rc;
    UINT32 wprCmd;

    MASSERT(regionID <= LW_PFB_PRI_MMU_WPR_INFO_ALLOW_WPR_ADDR_LO__SIZE_1);

    wprCmd = FLD_SET_DRF_IDX(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _WPR_ADDR_LO,
                             regionID, 0);

    RegWr32(LW_PFB_PRI_MMU_WPR_INFO, wprCmd);

    *pStartAddr = DRF_VAL(_PFB, _PRI_MMU_WPR_INFO, _DATA,
                          RegRd32(LW_PFB_PRI_MMU_WPR_INFO));
    *pStartAddr = (*pStartAddr) << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;
    // Read ReadMask
    wprCmd = FLD_SET_DRF(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _ALLOW_READ, 0);
    RegWr32(LW_PFB_PRI_MMU_WPR_INFO, wprCmd);
    *pReadMask = DRF_IDX_VAL(_PFB, _PRI_MMU_WPR_INFO, _ALLOW_READ_WPR, regionID,
        RegRd32(LW_PFB_PRI_MMU_WPR_INFO));

    // Read WriteMask
    wprCmd = FLD_SET_DRF(_PFB, _PRI_MMU_WPR, _INFO_INDEX, _ALLOW_WRITE, 0);
    RegWr32(LW_PFB_PRI_MMU_WPR_INFO, wprCmd);
    *pWriteMask = DRF_IDX_VAL(_PFB, _PRI_MMU_WPR_INFO, _ALLOW_WRITE_WPR,
                              regionID,
        RegRd32(LW_PFB_PRI_MMU_WPR_INFO));

    return rc;
}

//------------------------------------------------------------------------------
// These defines used to be in dev_ext_devices.h but have been removed. I don't
// see the equivelent defines in dev_fuse.h so I'm defining them here until hw
// gets around to defining them.
// Values 0 & 3 represent 256M!
#ifndef LW_FUSE_OPT_BAR1_SIZE_64M
#define LW_FUSE_OPT_BAR1_SIZE_64M                  1
#define LW_FUSE_OPT_BAR1_SIZE_128M                 2
#define LW_FUSE_OPT_BAR1_SIZE_256M                 3
#define LW_FUSE_OPT_BAR1_SIZE_512M                 4
#define LW_FUSE_OPT_BAR1_SIZE_1G                   5
#define LW_FUSE_OPT_BAR1_SIZE_2G                   6
#define LW_FUSE_OPT_BAR1_SIZE_4G                   7
#define LW_FUSE_OPT_BAR1_SIZE_8G                   8
#define LW_FUSE_OPT_BAR1_SIZE_16G                  9
#define LW_FUSE_OPT_BAR1_SIZE_32G                 10
#define LW_FUSE_OPT_BAR1_SIZE_64G                 11
#define LW_FUSE_OPT_BAR1_SIZE_128G                12
#endif

//------------------------------------------------------------------------------
// | GP100 yes | GP104 ?? | GP107 ?? | GP108 ?? |
/* static */ bool VoltaGpuSubdevice::SetBootStraps
(
    Gpu::LwDeviceId deviceId
    ,void * LinLwBase
    ,UINT32 boot0Strap
    ,UINT32 boot3Strap
    ,UINT64 fbStrap
    ,UINT32 gc6Strap
)
{
    RegHalTable regs;
    regs.Initialize(Device::LwDeviceId(deviceId));

    UINT32 lwrBoot0Strap = 0;
    UINT32 lwrBoot3Strap = 0;
    UINT32 lwrFbStrap = 0;
    UINT32 lwrGc6Strap = 0;
    const bool bFuseSupported = ((Platform::GetSimulationMode() != Platform::Fmodel) &&
                                 (Platform::GetSimulationMode() != Platform::Amodel));

    JavaScriptPtr pJs;

    lwrBoot0Strap = MEM_RD32((char *)LinLwBase + LW_PEXTDEV_BOOT_0);
    lwrBoot3Strap = MEM_RD32((char *)LinLwBase + LW_PEXTDEV_BOOT_3);
    lwrGc6Strap   = MEM_RD32((char *)LinLwBase + LW_PGC6_SCI_STRAP);

    // Apply BAR1 size through PBUS when fuse isnt supported
    if (bFuseSupported)
    {
        lwrFbStrap = MEM_RD32((char *)LinLwBase + regs.LookupAddress(MODS_FUSE_OPT_BAR1_SIZE));
        if (lwrFbStrap == 0)
        {
            lwrFbStrap = LW_FUSE_OPT_BAR1_SIZE_256M;
        }
    }
    else
    {
        lwrFbStrap = MEM_RD32((char *)LinLwBase + LW_PBUS_BAR1_CONFIG);
    }

    if (boot0Strap == 0xffffffff) // option '-boot_0_strap' not specified
    {
        // Default straps
        boot0Strap = lwrBoot0Strap;
    }
    else
    {
        Printf(Tee::PriNormal, "Boot0Strap set to 0x%x\n", boot0Strap);
    }

    // There are no defaults to set for this register on Pascal
    if (boot3Strap == 0xffffffff) // option '-boot_3_strap' not specified
    {
        boot3Strap = lwrBoot3Strap;
    }
    else
    {
        Printf(Tee::PriNormal, "Boot3Strap set to 0x%x\n", boot3Strap);
    }

    if (gc6Strap == 0xffffffff) // option '-gc6_strap' not specified
    {
        gc6Strap = lwrGc6Strap;
        // Set Lwdqro strap?
        INT32 openGL = 0;
        pJs->GetProperty(Gpu_Object, Gpu_StrapOpenGL, &openGL);
        // Now see if we need to apply the -openGL property
        if ((openGL != -1) ||
            (Platform::GetSimulationMode() != Platform::Hardware))
        {
            // On Pascal we default to Lwdqro mode not VdChip like other Gpus.
            if (openGL == -1)
                openGL = 1;
            if (openGL != 0)
            {
                Printf(Tee::PriLow, "Strap as Lwdqro\n");
                gc6Strap = FLD_SET_DRF(_PGC6, _SCI, _STRAP_DEVID_SEL,
                                       _REBRAND, gc6Strap);
            }
            else
            {
                Printf(Tee::PriLow, "Strap as VdChip\n");
                gc6Strap = FLD_SET_DRF(_PGC6, _SCI, _STRAP_DEVID_SEL,
                                       _ORIGINAL, gc6Strap);
            }
        }

        INT32  strapRamcfg;
        pJs->GetProperty(Gpu_Object, Gpu_StrapRamcfg, &strapRamcfg);
        if ((strapRamcfg != -1) ||
            (Platform::GetSimulationMode() != Platform::Hardware))
        {
            if (strapRamcfg == -1)
                strapRamcfg = 0;
            gc6Strap = FLD_SET_DRF_NUM(_PGC6, _SCI, _STRAP_RAMCFG,
                                       strapRamcfg, gc6Strap);
        }
    }
    else
    {
        Printf(Tee::PriNormal, "Gc6Strap set to 0x%x\n", gc6Strap);
    }

    // Only apply fbStrap value of zero on RTL.  Applying fbStrap of zero on
    // Fmodel/Amodel (i.e. forcing a default of 256M) breaks various AS2 tests
    if ((fbStrap != 0) || (Platform::GetSimulationMode() == Platform::RTL))
    {
        static map<UINT64, pair<UINT32, UINT32>> s_Bar1Vals =
        {
             { 0,       { LW_FUSE_OPT_BAR1_SIZE_256M, LW_PBUS_BAR1_CONFIG_SIZE_256M } }
            ,{ 1 << 6,  { LW_FUSE_OPT_BAR1_SIZE_64M,  LW_PBUS_BAR1_CONFIG_SIZE_64M  } }
            ,{ 1 << 7,  { LW_FUSE_OPT_BAR1_SIZE_128M, LW_PBUS_BAR1_CONFIG_SIZE_128M } }
            ,{ 1 << 8,  { LW_FUSE_OPT_BAR1_SIZE_256M, LW_PBUS_BAR1_CONFIG_SIZE_256M } }
            ,{ 1 << 9,  { LW_FUSE_OPT_BAR1_SIZE_512M, LW_PBUS_BAR1_CONFIG_SIZE_512M } }
            ,{ 1 << 10, { LW_FUSE_OPT_BAR1_SIZE_1G,   LW_PBUS_BAR1_CONFIG_SIZE_1G   } }
            ,{ 1 << 11, { LW_FUSE_OPT_BAR1_SIZE_2G,   LW_PBUS_BAR1_CONFIG_SIZE_2G   } }
            ,{ 1 << 12, { LW_FUSE_OPT_BAR1_SIZE_4G,   LW_PBUS_BAR1_CONFIG_SIZE_4G   } }
            ,{ 1 << 13, { LW_FUSE_OPT_BAR1_SIZE_8G,   LW_PBUS_BAR1_CONFIG_SIZE_8G   } }
            ,{ 1 << 14, { LW_FUSE_OPT_BAR1_SIZE_16G,  LW_PBUS_BAR1_CONFIG_SIZE_16G  } }
            ,{ 1 << 15, { LW_FUSE_OPT_BAR1_SIZE_32G,  LW_PBUS_BAR1_CONFIG_SIZE_32G  } }
            ,{ 1 << 16, { LW_FUSE_OPT_BAR1_SIZE_64G,  LW_PBUS_BAR1_CONFIG_SIZE_64G  } }
            ,{ 1 << 17, { LW_FUSE_OPT_BAR1_SIZE_128G, LW_PBUS_BAR1_CONFIG_SIZE_128G } }
        };

        if (s_Bar1Vals.find(fbStrap) == s_Bar1Vals.end())
        {
            Printf(Tee::PriError, "Invalid STRAP_FB setting : %llu\n", fbStrap);
            return false;
        }
        else
        {
            fbStrap = bFuseSupported ? s_Bar1Vals[fbStrap].first : s_Bar1Vals[fbStrap].second;
        }
    }
    else
    {
        // Ensure that fbStrap isnt changed if setting the default value
        fbStrap = lwrFbStrap;
    }

    if (Platform::GetSimulationMode() == Platform::RTL)
    {
        Platform::EscapeWrite("LW_PEXTDEV_BOOT_0", LW_PEXTDEV_BOOT_0,
                              4, boot0Strap);
        Platform::EscapeWrite("LW_PEXTDEV_BOOT_3", LW_PEXTDEV_BOOT_3,
                              4, boot3Strap);
        Platform::EscapeWrite("LW_FUSE_OPT_BAR1_SIZE", regs.LookupAddress(MODS_FUSE_OPT_BAR1_SIZE),
                              4, static_cast<UINT32>(fbStrap));
        Platform::EscapeWrite("LW_PGC6_SCI_STRAP", LW_PGC6_SCI_STRAP,
                              4, gc6Strap);
    }

    if ((Platform::GetSimulationMode() == Platform::Hardware) &&
        (fbStrap > lwrFbStrap))
    {
        Printf(Tee::PriError, "Cannot expand BAR1 on Emulation/Silicon!\n");
        return false;
    }

    if (boot0Strap != lwrBoot0Strap)
    {
        boot0Strap = FLD_SET_DRF(_PEXTDEV, _BOOT_0, _STRAP_OVERWRITE,
                                 _ENABLED, boot0Strap);
        MEM_WR32(static_cast<char*>(LinLwBase) + LW_PEXTDEV_BOOT_0,
                 boot0Strap);
    }

    if (boot3Strap != lwrBoot3Strap)
    {
        boot3Strap = FLD_SET_DRF(_PEXTDEV, _BOOT_3_STRAP_1, _OVERWRITE,
                                 _ENABLED, boot3Strap);
        MEM_WR32((char *)LinLwBase + LW_PEXTDEV_BOOT_3, boot3Strap);
    }

    if (fbStrap != lwrFbStrap)
    {
        if (bFuseSupported)
        {
            MEM_WR32((char*)LinLwBase + regs.LookupAddress(MODS_FUSE_EN_SW_OVERRIDE),
                     regs.SetField(MODS_FUSE_EN_SW_OVERRIDE_VAL_ENABLE));
            MEM_WR32((char*)LinLwBase + regs.LookupAddress(MODS_FUSE_OPT_BAR1_SIZE),
                     static_cast<UINT32>(fbStrap));
        }
        else
        {
            MEM_WR32((char*)LinLwBase + LW_PBUS_BAR1_CONFIG,
                     DRF_NUM(_PBUS, _BAR1_CONFIG, _SIZE, static_cast<UINT32>(fbStrap)) |
                     DRF_DEF(_PBUS, _BAR1_CONFIG, _OVERWRITE, _ENABLED));
        }
    }

    if (gc6Strap != lwrGc6Strap)
    {
        MEM_WR32((char*)LinLwBase + LW_PGC6_SCI_STRAP_OVERRIDE, gc6Strap);
        MEM_WR32((char*)LinLwBase + LW_PGC6_SCI_STRAP, gc6Strap);
    }

    return true;
}

//------------------------------------------------------------------------------
// Pascal's definition of _STRAP_RAMCFG changed.
UINT32 VoltaGpuSubdevice::MemoryStrap()
{
    return DRF_VAL(_PGC6, _SCI, _STRAP_RAMCFG, RegRd32(LW_PGC6_SCI_STRAP));
}

/* virtual */ RC VoltaGpuSubdevice::GetSmEccEnable(bool *pEnable, bool *pOverride) const
{
    UINT32 value = RegRd32(LW_PGRAPH_PRI_FECS_FEATURE_OVERRIDE_ECC);
    if (pEnable)
    {
        *pEnable = (DRF_VAL(_PGRAPH, _PRI_FECS_FEATURE_OVERRIDE_ECC,
                            _SM_LRF, value));
    }

    if (pOverride)
    {
        *pOverride = DRF_VAL(_PGRAPH, _PRI_FECS_FEATURE_OVERRIDE_ECC, _SM_LRF_OVERRIDE, value);
    }

    return OK;
}

//-------------------------------------------------------------------------------
/* virtual */ RC VoltaGpuSubdevice::SetSmEccEnable(bool bEnable, bool bOverride)
{
    UINT32 value = RegRd32(LW_PGRAPH_PRI_FECS_FEATURE_OVERRIDE_ECC);
    value = FLD_SET_DRF_NUM(_PGRAPH, _PRI_FECS_FEATURE_OVERRIDE_ECC,
                            _SM_LRF, bEnable, value);
    value = FLD_SET_DRF_NUM(_PGRAPH, _PRI_FECS_FEATURE_OVERRIDE_ECC,
                            _SM_LRF_OVERRIDE, bOverride, value);
    RegWr32(LW_PGRAPH_PRI_FECS_FEATURE_OVERRIDE_ECC, value);

    return OK;
}

//-------------------------------------------------------------------------------
/* virtual */ UINT32 VoltaGpuSubdevice::GetMmeInstructionRamSize()
{
    // From: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/pascal/pascal/design/IAS/FE/Pascal_FE_IAS.docx //$
    // Refer to section 3.1.6 MME Resizing

    // Number of possible 32-bit MME instructions
    return 3072;
}

//-------------------------------------------------------------------------------
/* virtual */ UINT32 VoltaGpuSubdevice::GetMmeVersion()
{
    // This will be used by MMESimulator to pick the macros in order
    // to get the value of method released by MME
    return 2;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 VoltaGpuSubdevice::GetPLLCfg(PLLType Pll) const
{
    if (Pll == GPC_PLL)
    {
        return RegRd32(LW_PTRIM_GPC_BCAST_GPCPLL_CFG);
    }
    else
    {
        return GpuSubdevice::GetPLLCfg(Pll);
    }

}

//------------------------------------------------------------------------------
/* virtual */ void VoltaGpuSubdevice::SetPLLCfg(PLLType Pll, UINT32 cfg)
{
    if (Pll == GPC_PLL)
    {
        RegWr32(LW_PTRIM_GPC_BCAST_GPCPLL_CFG, cfg);
    }
    else
    {
        GpuSubdevice::SetPLLCfg(Pll, cfg);
    }
}

//------------------------------------------------------------------------------
/* virtual */ void VoltaGpuSubdevice::SetPLLLocked(PLLType Pll)
{
    UINT32 Reg = GetPLLCfg(Pll);

    switch (Pll)
    {
        case SP_PLL0:
            Reg = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL0_CFG, _EN_LCKDET,
                              _POWER_ON, Reg);
            break;
        case SP_PLL1:
            Reg = FLD_SET_DRF(_PVTRIM, _SYS_SPPLL1_CFG, _EN_LCKDET,
                              _POWER_ON, Reg);
            break;
        case SYS_PLL:
            Reg = FLD_SET_DRF(_PTRIM, _SYS_SYSPLL_CFG, _EN_LCKDET,
                              _POWER_ON, Reg);
            break;
        case GPC_PLL:
            // use broadcast register for gp100
            Reg = FLD_SET_DRF(_PTRIM, _GPC_BCAST_GPCPLL_CFG, _EN_LCKDET,
                              _POWER_ON, Reg);
            break;
        case XBAR_PLL:
            Reg = FLD_SET_DRF(_PTRIM, _SYS_XBARPLL_CFG, _EN_LCKDET,
                              _POWER_ON, Reg);
            break;
        default:
            MASSERT(!"Invalid PLL type.");
    }

    SetPLLCfg(Pll, Reg);
}

//------------------------------------------------------------------------------
/* virtual */ bool VoltaGpuSubdevice::IsPLLLocked(PLLType Pll) const
{
    UINT32 Reg = GetPLLCfg(Pll);

    switch (Pll)
    {
        case SP_PLL0:
            return FLD_TEST_DRF(_PVTRIM, _SYS_SPPLL0_CFG, _PLL_LOCK,
                                _TRUE, Reg);
        case SP_PLL1:
            return FLD_TEST_DRF(_PVTRIM, _SYS_SPPLL1_CFG, _PLL_LOCK,
                                _TRUE, Reg);
        case SYS_PLL:
            return FLD_TEST_DRF(_PTRIM, _SYS_SYSPLL_CFG, _PLL_LOCK,
                                _TRUE, Reg);
        case GPC_PLL:
            // use broadcast register for gp100
            return FLD_TEST_DRF(_PTRIM, _GPC_BCAST_GPCPLL_CFG, _PLL_LOCK,
                                _TRUE, Reg);
        case XBAR_PLL:
            return FLD_TEST_DRF(_PTRIM, _SYS_XBARPLL_CFG, _PLL_LOCK,
                                _TRUE, Reg);
        default:
            MASSERT(!"Invalid PLL type.");
    }
    return false;
}

/* virtual */
void VoltaGpuSubdevice::GetPLLList(vector<PLLType> *pPllList) const
{
    static const PLLType MyPllList[] = { SP_PLL0, SP_PLL1, SYS_PLL, GPC_PLL,
        XBAR_PLL }; // No LTC on Volta and up.

    MASSERT(pPllList != NULL);
    pPllList->resize(NUMELEMS(MyPllList));
    copy(&MyPllList[0], &MyPllList[NUMELEMS(MyPllList)], pPllList->begin());
}

//-------------------------------------------------------------------------------
/* virtual */ bool VoltaGpuSubdevice::IsPLLEnabled(PLLType Pll) const
{
    if (Pll == GPC_PLL)
    {
        return Regs().Test32(MODS_PTRIM_GPC_BCAST_GPCPLL_CFG_ENABLE_YES);
    }
    else
    {
        return GpuSubdevice::IsPLLEnabled(Pll);
    }
}

//------------------------------------------------------------------------------
/* virtual */ RC VoltaGpuSubdevice::PrepareEndOfSimCheck(Gpu::ShutDownMode mode)
{
    RC rc;

    const bool bWaitgfxDeviceIdle = true;
    const bool bForceFenceSyncBeforeEos = true;
    CHECK_RC(PrepareEndOfSimCheckImpl(mode, bWaitgfxDeviceIdle, bForceFenceSyncBeforeEos));

    return rc;
}

//------------------------------------------------------------------------------
RC VoltaGpuSubdevice::GetMaxCeBandwidthKiBps(UINT32 *pBwKiBps)
{
    MASSERT(pBwKiBps);

    RC rc;
    UINT64 sysClockFreq;
    if (IsEmOrSim())
    {
        static const UINT64 POR_SYSCLK_FREQ = 868000000ULL;
        sysClockFreq = POR_SYSCLK_FREQ;
    }
    else
    {
        CHECK_RC(GetClock(Gpu::ClkSys, &sysClockFreq, nullptr, nullptr, nullptr));
    }

    // Clock speed in kHz
    FLOAT64 fBwKBps = static_cast<FLOAT64>(sysClockFreq) / 1000.0;

    // HSCE has a 32B interface to HUB. When making 256B requests, HSCE sends 1
    // cycle of headers (read and write header combo) + 8 cycles of write data.
    // So CE can at best send 256/9 = 28.44 B/cycle. CE runs on sysclk, so
    // whatever the sysclk is, will give the max CE BW. GP100 is targeting
    // 720MHz. So HSCE max BW will be 20.47GB/sec. The 28.44 B/cycle is only if
    // requests are 256B. Smaller requests will result in higher header overhead
    // and lower data Bytes per cycle. The CE interface to HUB is not changing.
    // So this is not changing from Pascal to Volta.
    fBwKBps *= 28.44;

    // Colwert Mbps to KiBps
    *pBwKiBps = static_cast<UINT32>(fBwKBps * 1000.0 / 1024.0);

    return rc;
}

//-----------------------------------------------------------------------------
/* static */ bool VoltaGpuSubdevice::PollForFBStopAck(void *pVoidPollArgs)
{
    GpuSubdevice* subdev = static_cast<GpuSubdevice*>(pVoidPollArgs);
    UINT32 regValue = subdev->RegRd32(LW_PPWR_PMU_GPIO_INPUT);
    return FLD_TEST_DRF(_PPWR, _PMU_GPIO_INPUT, _FB_STOP_ACK, _TRUE, regValue);
}

//------------------------------------------------------------------------------
/* virtual */ RC VoltaGpuSubdevice::AssertFBStop()
{
    RC rc;
    RegWr32(LW_PPWR_PMU_GPIO_OUTPUT_SET, 0x4);

    // Wait for ACK
    CHECK_RC(POLLWRAP_HW(PollForFBStopAck, static_cast<void*>(this), 1000));

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ void VoltaGpuSubdevice::DeAssertFBStop()
{
    RegWr32(LW_PPWR_PMU_GPIO_OUTPUT_CLEAR, 0x4);
}

//-----------------------------------------------------------------------------
// Return a vector of offset, value pairs for all of the strap registers.
/*virtual */
RC VoltaGpuSubdevice::GetStraps(vector<pair<UINT32, UINT32>>* pStraps)
{
    pair <UINT32, UINT32> boot0(LW_PEXTDEV_BOOT_0, RegRd32(LW_PEXTDEV_BOOT_0));
    pair <UINT32, UINT32> boot3(LW_PEXTDEV_BOOT_3, RegRd32(LW_PEXTDEV_BOOT_3));
    pair <UINT32, UINT32> sci(LW_PGC6_SCI_STRAP,   RegRd32(LW_PGC6_SCI_STRAP));
    pStraps->clear();
    pStraps->push_back(boot0);
    pStraps->push_back(boot3);
    pStraps->push_back(sci);
    return OK;
}

//*****************************************************************************
// Load the IrqInfo struct (used to hook/unhook interrupts) with the
// default info from GpuSubdevice, and Volta mask registers.
void VoltaGpuSubdevice::LoadIrqInfo
(
    IrqParams* pIrqInfo,
    UINT08    irqType,
    UINT32    irqNumber,
    UINT32    maskInfoCount
)
{
    const RegHal& regs = Regs();

#if defined(LWCPU_AARCH64) && defined(SIM_BUILD)
    if (irqType == MODS_XP_IRQ_TYPE_INT)
    {
        irqInfo = MODS_XP_IRQ_TYPE_CPU;
    }
#endif

    GpuSubdevice::LoadIrqInfo(pIrqInfo, irqType, irqNumber, maskInfoCount);
    for (UINT32 ii = 0; ii < maskInfoCount; ii++)
    {
        auto pMask = &pIrqInfo->maskInfoList[ii];
        pMask->irqPendingOffset = regs.LookupAddress(MODS_PMC_INTR, ii);
        pMask->irqEnabledOffset = regs.LookupAddress(MODS_PMC_INTR_EN, ii);
        pMask->irqEnableOffset  = regs.LookupAddress(MODS_PMC_INTR_EN_SET, ii);
        pMask->irqDisableOffset = regs.LookupAddress(MODS_PMC_INTR_EN_CLEAR, ii);
        pMask->andMask          = 0;
        pMask->orMask           = 0xFFFFFFFF;
        pMask->maskType         = MODS_XP_MASK_TYPE_IRQ_DISABLE;
    }
}

void VoltaGpuSubdevice::SetSWIntrPending
(
    unsigned intrTree,
    bool pending
)
{
    MASSERT(intrTree < GetNumIntrTrees());
    const UINT32 value =
        pending ? Regs().LookupValue(MODS_PMC_INTR_SW_ASSERT_TRUE) :
                  Regs().LookupValue(MODS_PMC_INTR_SW_ASSERT_FALSE);
    Regs().Write32(MODS_PMC_INTR_SW, intrTree,
            Regs().SetField(MODS_PMC_INTR_SW_ASSERT, value));

    // Re-read the register to make sure the write went through (esp. on CheetAh)
    Regs().Read32(MODS_PMC_INTR_SW, intrTree);
}

void VoltaGpuSubdevice::SetIntrEnable
(
    unsigned intrTree,
    IntrEnable state
)
{
    MASSERT(intrTree < GetNumIntrTrees());
    UINT32 regClearValue = 0;
    UINT32 regSetValue   = 0;
    switch (state)
    {
        case intrDisabled:
            regClearValue = Regs().SetField(MODS_PMC_INTR_EN_CLEAR_VALUE, ~0);
            break;

        case intrHardware:
            regClearValue = Regs().SetField(
                                        MODS_PMC_INTR_SOFTWARE,
                                        Regs().LookupValue(MODS_PMC_INTR_SOFTWARE_PENDING));
            regSetValue   = ~regClearValue;
            break;

        case intrSoftware:
            regSetValue   = Regs().SetField(
                                        MODS_PMC_INTR_SOFTWARE,
                                        Regs().LookupValue(MODS_PMC_INTR_SOFTWARE_PENDING));
            regClearValue = ~regSetValue;
            break;

        default:
            MASSERT(!"Invalid state");
            return;
    }

    Regs().Write32(MODS_PMC_INTR_EN_CLEAR, intrTree, regClearValue);
    Regs().Write32(MODS_PMC_INTR_EN_SET,   intrTree, regSetValue);
}

/* virtual */ unsigned VoltaGpuSubdevice::GetNumIntrTrees() const
{
    return Regs().LookupArraySize(MODS_PMC_INTR, 1);
}

//------------------------------------------------------------------------------
UINT32 VoltaGpuSubdevice::Io3GioConfigPadStrap()
{
    if (Regs().Test32(MODS_PGC6_SCI_STRAP_PCIE_CFG_HIGH_POWER))
    {
        return Regs().Read32(MODS_FUSE_OPT_3GIO_PADCFG_LUT_ADR_R3_0);
    }
    else // MODS_PGC6_SCI_STRAP_PCIE_CFG_LOW_POWER
    {
        return Regs().Read32(MODS_FUSE_OPT_3GIO_PADCFG_LUT_ADR_R3_1);
    }
}

//------------------------------------------------------------------------------
UINT32 VoltaGpuSubdevice::RamConfigStrap()
{
    return Regs().Read32(MODS_PGC6_SCI_STRAP_RAMCFG);
}

bool VoltaGpuSubdevice::IsQuadroEnabled()
{
    bool fuseEnabled;

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        fuseEnabled = !(Regs().Test32(MODS_FUSE_OPT_OPENGL_EN_DATA_INIT));
    }
    else
    {
        fuseEnabled = true;
    }

    if (fuseEnabled)
    {
        if (Regs().Test32(MODS_PTOP_DEBUG_1_OPENGL_ON))
        {
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
RC VoltaGpuSubdevice::GetAteSpeedo(UINT32 SpeedoNum, UINT32 *pSpeedo, UINT32 *pVersion)
{
    MASSERT(pSpeedo);
    MASSERT(pVersion);
    RC rc;

    if (SpeedoNum >= NumAteSpeedo())
    {
        return RC::BAD_PARAMETER;
    }

    if (Regs().Test32(MODS_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL2_ENABLED_FUSE1))
    {
        Printf(Tee::PriDebug, "Speedo info is priv-protected. Use HULK to view.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    UINT32 SpeedoData = RegRd32(Regs().LookupAddress(MODS_FUSE_OPT_SPEEDO0) +
                                SpeedoNum * sizeof(UINT32));
    *pSpeedo = SpeedoData;
    *pVersion = Regs().Read32(MODS_FUSE_OPT_SPEEDO_REV_DATA);

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VoltaGpuSubdevice::GetAteSramIddq(UINT32 *pIddq)
{
    MASSERT(pIddq);

    RC rc;
    if (!Regs().IsSupported(MODS_FUSE_OPT_TMDS_VOLT_CTRL_CYA0))
    {
        rc = RC::UNSUPPORTED_FUNCTION;
    }
    else
    {
        UINT32 IddqData = Regs().Read32(MODS_FUSE_OPT_TMDS_VOLT_CTRL_CYA0_DATA);
        if (IddqData == 0)
        {
            rc = RC::UNSUPPORTED_FUNCTION;
        }
        *pIddq    = IddqData * (HasBug(598305) ? 50 : 1);
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC VoltaGpuSubdevice::GetSramBin(INT32* pSramBin)
{
    if (!Regs().IsSupported(MODS_FUSE_OPT_GPC_HT_TSOSC_CAL_RAW_COUNT_3))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // GV100 never used this fuse for its original purpose, so bits
    // 3:0 were reused to store information about the SRAM bin
    UINT32 fusedVal = Regs().Read32(MODS_FUSE_OPT_GPC_HT_TSOSC_CAL_RAW_COUNT_3) & 0xf;

    // Unfortunately, someone decided to make it a signed value,
    // so we have to colwert from unsigned fuses ourselves.
    bool negative = (fusedVal & (1 << 3)) != 0;
    *pSramBin = fusedVal | (negative ? ~0xf : 0);

    return RC::OK;
}

//-----------------------------------------------------------------------------
/*virtual*/ void VoltaGpuSubdevice::EnablePwrBlock()
{
    UINT32 data = RegRd32(LW_PPWR_FALCON_ENGINE);
    data = FLD_SET_DRF(_PPWR, _FALCON_ENGINE, _RESET, _FALSE, data);
    RegWr32(LW_PPWR_FALCON_ENGINE, data);
}

//-----------------------------------------------------------------------------
/*virtual*/ void VoltaGpuSubdevice::DisablePwrBlock()
{
    UINT32 data = RegRd32(LW_PPWR_FALCON_ENGINE);
    data = FLD_SET_DRF(_PPWR, _FALCON_ENGINE, _RESET, _TRUE, data);
    RegWr32(LW_PPWR_FALCON_ENGINE, data);
}

//-----------------------------------------------------------------------------
/*virtual*/ void VoltaGpuSubdevice::EnableSecBlock()
{
    UINT32 data = RegRd32(LW_PSEC_FALCON_ENGINE);
    data = FLD_SET_DRF(_PSEC, _FALCON_ENGINE, _RESET, _FALSE, data);
    RegWr32(LW_PSEC_FALCON_ENGINE, data);
}

//-----------------------------------------------------------------------------
/*virtual*/ void VoltaGpuSubdevice::DisableSecBlock()
{
    UINT32 data = RegRd32(LW_PSEC_FALCON_ENGINE);
    data = FLD_SET_DRF(_PSEC, _FALCON_ENGINE, _RESET, _TRUE, data);
    RegWr32(LW_PSEC_FALCON_ENGINE, data);
}

//------------------------------------------------------------------------------
/* virtual */ void VoltaGpuSubdevice::PrintMonitorInfo()
{
    PrintVoltaMonitorInfo(0);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes |
UINT32 VoltaGpuSubdevice::NumAteSpeedo() const
{
    // refer to LW_FUSE_OPT_SPEEDO0/LW_FUSE_OPT_SPEEDO1/etc in
    // drivers/common/inc/hwref/maxwell/gm107/dev_fuse.h
    return 3;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes |
RC VoltaGpuSubdevice::GetAteIddq(UINT32 *pIddq, UINT32 *pVersion)
{
    // Note: There are 3 implementations for reading ATE Iddq info.
    // 1.GT21xGpuSubdevice::GetAteIddq()
    // 2.GpuSubdevice::GetAteIddq()
    // 3.GF11xGpuSubdevice::GetAteIddq()
    // 1&2 algorithm is identical, only register access is different.
    // 3.Writes to the LW_PTOP_DEBUG_1_FUSES_VISIBLE field which is undefined
    //   for Maxwell.

    // Maxwell has priv_level_selwrity feature that will prevent access to the
    // iddq version and value when its enabled. During normal bringup and
    // production testing this feature is disabled. At the end of testing the
    // feature is enabled and access is denied.
    MASSERT(pIddq && pVersion);

    if (Regs().Test32(MODS_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL2_ENABLED_FUSE1))
    {
        Printf(Tee::PriDebug, "IDDQ info is priv-protected. Use HULK to view.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT32 IddqData = Regs().Read32(MODS_FUSE_OPT_IDDQ_DATA);
    UINT32 IddqRev = Regs().Read32(MODS_FUSE_OPT_IDDQ_REV_DATA);

    RC rc;
    if ((IddqRev == 0) && (IddqData == 0))
    {
        rc = RC::UNSUPPORTED_FUNCTION;
    }

    *pIddq    = IddqData * (HasBug(598305) ? 50 : 1);
    *pVersion = IddqRev;

    return rc;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes |
RC VoltaGpuSubdevice::GetAteRev(UINT32* pRevision)
{
    *pRevision = Regs().Read32(MODS_FUSE_OPT_LWST_ATE_REV_DATA);
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 VoltaGpuSubdevice::GetMaxCeCount() const
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes |
{
    return RegRd32(LW_PTOP_SCAL_NUM_CES);
}

//------------------------------------------------------------------------------
UINT32 VoltaGpuSubdevice::GetMaxPceCount() const
{
    return Regs().LookupAddress(MODS_CE_PCE2LCE_CONFIG__SIZE_1);
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes |
UINT32 VoltaGpuSubdevice::GetMmeShadowRamSize()
{
    // Refer to section 7.5.8.3.3 MME Shadow RAM
    // From: https://p4viewer.lwpu.com/get///hw/doc/gpu/maxwell/maxwell/design/IAS/FE/Maxwell_FE_IAS.doc //$

    // Number of possible 32-bit MME Shadowed Methods
    return 1536;
}

//-------------------------------------------------------------------------------
/* virtual */ UINT32 VoltaGpuSubdevice::GetMaxZlwllCount() const
{
    return RegRd32(LW_PTOP_SCAL_NUM_ZLWLL_BANKS);
}

//------------------------------------------------------------------------------
bool VoltaGpuSubdevice::IsDisplayFuseEnabled()
{
    if (Regs().Test32(MODS_FUSE_STATUS_OPT_DISPLAY_DATA_DISABLE))
    {
        return false;
    }
    return true;
}

//-------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 Yes | GM204 Yes | GM206 Yes | GM204X Yes | GM206X Yes |
/* virtual */ UINT32 VoltaGpuSubdevice::GetTexCountPerTpc() const
{
    // See decoder ring, or the number of pipes in
    // LW_PGRAPH_PRI_GPC0_TPC0_TEX_M_ROUTING_SEL
    return 2;
}

//------------------------------------------------------------------------------
UINT32 VoltaGpuSubdevice::UserStrapVal()
{
    // These registers are removed starting on Maxwell, since the GpuSubdevice
    // implementation is Fermi based just return a default value
    return 0xFFFFFFFF;
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes |
UINT64 VoltaGpuSubdevice::FramebufferBarSize() const
{
    return FbApertureSize() / (1024*1024);
}

//------------------------------------------------------------------------------
RC VoltaGpuSubdevice::SetFbMinimum(UINT32 fbMinMB)
{
    // 8bits == 1KB page size has been dropped by Maxwell, 9 bits is now the
    // minimumColumnSize. RowSize is unchanged.
    const UINT32 minFbColumnBits = 9; //2K page size minimum
    const UINT32 minFbRowBits = 10; //
    const UINT32 minBankBits = 3;
    return SetFbMinimumInternal(
        fbMinMB,
        minFbColumnBits,
        minFbRowBits,
        minBankBits);
}

//------------------------------------------------------------------------------
// Turn on/off the GPU power through the EC
// Note: This API is intended ONLY for Emulation machines that have a netlist
// which supports this backdoor register. Use at you own discretion.
// Note: This API is called by Xp::CallACPIGeneric() to put the GPU into/outof
// reset on the Emulator. Note that this register is not actually part of the
// memory mapped GPU registers in BAR0. There is code in the emulator that
// snoops register address's and routes these address to the PLDA.
RC VoltaGpuSubdevice::SetBackdoorGpuPowerGc6(bool pwrOn)
{
    if (IsEmulation())
    {
        if (pwrOn)
        {
            RC rc;
            LwRmPtr pLwRm;
            LW2080_CTRL_POWER_FEATURE_GC6_INFO_PARAMS gc6Params;
            memset(&gc6Params, 0, sizeof(gc6Params));
            CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(this),
                        (UINT32)LW2080_CTRL_CMD_POWER_FEATURE_GET_GC6_INFO,
                        &gc6Params,
                        sizeof (gc6Params)));
            // If power islands are enabled we write to EC_2, otherwise write to
            // EC_1
            if (FLD_TEST_DRF(2080_CTRL_POWER, _FEATURE_GC6_INFO,
                             _PGISLAND_PRESENT, _TRUE,
                             gc6Params.gc6InfoMask))
            {
                //toggle bit 12
                UINT32 value = RegRd32(LW_BRIDGE_GPU_BACKDOOR_EC_2);
                value = FLD_SET_DRF_NUM(_BRIDGE_GPU_BACKDOOR, _EC_2_WAKEUP_ASSERT,
                                    _GPIO_12, 1, value);
                RegWr32(LW_BRIDGE_GPU_BACKDOOR_EC_2, value);

                value = FLD_SET_DRF_NUM(_BRIDGE_GPU_BACKDOOR, _EC_2_WAKEUP_ASSERT,
                                    _GPIO_12, 0, value);
                Utility::Delay(150); // make sure the pulse is at least 150us.

                RegWr32(LW_BRIDGE_GPU_BACKDOOR_EC_2, value);
            }
            else
            {
                RegWr32(LW_BRIDGE_GPU_BACKDOOR_EC_1, 0x00);
            }
        }
        else // write to EC_1 regardless of power island status.
        {
            // Note: Writing this register has no effect when using Maxwell style GC6
            RegWr32(LW_BRIDGE_GPU_BACKDOOR_EC_1, 0x01);
        }
    }
    return OK;
}
//------------------------------------------------------------------------------
/* virtual */  RC VoltaGpuSubdevice::PrintMiscFuseRegs()
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes |
{
    return GetFsImpl()->PrintMiscFuseRegs();
}

//------------------------------------------------------------------------------
/* virtual */ RC VoltaGpuSubdevice::DumpEccAddress() const
{
    RC rc;
    LwRmPtr pLwRm;
    Tee::Priority pri = Tee::PriNormal;

    LW2080_CTRL_ECC_GET_AGGREGATE_ERROR_ADDRESSES_PARAMS eccParams = { 0 };
    CHECK_RC(pLwRm->ControlBySubdevice(this,
                                       LW2080_CTRL_CMD_ECC_GET_AGGREGATE_ERROR_ADDRESSES,
                                       &eccParams,
                                       sizeof(eccParams)));

    if (FLD_TEST_DRF(2080_CTRL_ECC, _GET_AGGREGATE_ERROR_ADDRESSES_FLAGS,
                     _OVERFLOW, _TRUE, eccParams.flags))
    {
        Printf(Tee::PriWarn, "Number of aggregate ECC addresses with error are more"
                " than maximum entries that can be returned\n");
    }

    map<Ecc::Unit, vector<LW2080_ECC_AGGREGATE_ADDRESS>> eccAddresses;

    for (UINT32 idx = 0; idx < eccParams.addressCount; idx++)
    {
        const Ecc::Unit lwrUnit = Ecc::ToEclwnitFromRmEclwnit(eccParams.entry[idx].unit);
        if (eccAddresses.find(lwrUnit) == eccAddresses.end())
            eccAddresses[lwrUnit] = { };
        eccAddresses[lwrUnit].push_back(eccParams.entry[idx]);
    }

    static const char * s_FbHeader  = "      Type  Part   Subp         Col        Row      Bank\n"
                                      "    ----------------------------------------------------";
    static const char * s_L2Header  = "      Type   LTC  Slice     Address\n"
                                      "    -------------------------------";
    static const char * s_GrHeader  = "      Type   GPC    TPC     Address\n"
                                      "    -------------------------------";
    static const char * s_GpcMmuHeader  = "      Type   GPC    MMU     Address\n"
                                          "    -------------------------------";
    for (Ecc::Unit lwrUnit = Ecc::Unit::FIRST; lwrUnit < Ecc::Unit::MAX;
          lwrUnit = Ecc::GetNextUnit(lwrUnit))
    {
        if (!Ecc::IsAddressReportingSupported(lwrUnit))
            continue;

        Printf(pri, "\n  %s ECC addresses: \n", Ecc::ToString(lwrUnit));

        UINT32 totalCount = 0;
        if (eccAddresses.find(lwrUnit) != eccAddresses.end())
            totalCount = UNSIGNED_CAST(UINT32, eccAddresses[lwrUnit].size());
        Printf(pri, "    Total count: %u\n", totalCount);
        if (!totalCount)
            continue;

        const char * pHeader = nullptr;
        const Ecc::UnitType unitType = Ecc::GetUnitType(lwrUnit);
        if (lwrUnit == Ecc::Unit::DRAM)
        {
            pHeader = s_FbHeader;
        }
        else if (lwrUnit == Ecc::Unit::L2)
        {
            pHeader = s_L2Header;
        }
        else if (lwrUnit == Ecc::Unit::GPCMMU)
        {
            pHeader = s_GpcMmuHeader;
        }
        else if ((lwrUnit != Ecc::Unit::TEX) &&
                 (unitType == Ecc::UnitType::GPC))
        {
            pHeader = s_GrHeader;
        }
        MASSERT(pHeader);
        if (pHeader == nullptr)
        {
            Printf(Tee::PriError, "%s : Unknown ECC type\n", __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }
        Printf(pri, "%s\n", pHeader);
        for (auto const & lwrAddressData : eccAddresses[lwrUnit])
        {
            string type;
            if (lwrUnit == Ecc::Unit::DRAM)
            {
                FrameBuffer * pFb = GetFB();
                const UINT32 columnDivisor = pFb->GetBurstSize() / pFb->GetAmapColumnSize();
                const UINT32 columnMultiplier = pFb->GetPseudoChannelsPerChannel();
                if (FLD_TEST_DRF(2080_ECC, _AGGREGATE_ADDRESS_ERROR_TYPE,
                    _CORR, _TRUE, lwrAddressData.errorType))
                {
                    type = "SBE";
                }
                else if (FLD_TEST_DRF(2080_ECC, _AGGREGATE_ADDRESS_ERROR_TYPE,
                    _UNCORR, _TRUE, lwrAddressData.errorType))
                {
                    type = "DBE";
                }
                else
                {
                    type = "NONE";
                }
                Printf(pri, "    %6s  %4u  %5u  %#10x  %#9x  %#8x\n",
                       type.c_str(),
                       lwrAddressData.location.dram.partition,
                       lwrAddressData.location.dram.sublocation,
                       (lwrAddressData.address.rbc.column / columnDivisor) * columnMultiplier,
                       lwrAddressData.address.rbc.row,
                       lwrAddressData.address.rbc.bank);
            }
            else if ((unitType == Ecc::UnitType::GPC) ||
                     (lwrUnit == Ecc::Unit::L2))
            {
                if (FLD_TEST_DRF(2080_ECC, _AGGREGATE_ADDRESS_ERROR_TYPE,
                    _CORR, _TRUE, lwrAddressData.errorType))
                {
                    type = "CORR";
                }
                else if (FLD_TEST_DRF(2080_ECC, _AGGREGATE_ADDRESS_ERROR_TYPE,
                    _UNCORR, _TRUE, lwrAddressData.errorType))
                {
                    type = "UNCORR";
                }
                else
                {
                    type = "NONE";
                }
                Printf(pri, "    %6s  %4u  %5u  %#10x\n",
                       type.c_str(),
                       lwrAddressData.location.lrf.location,
                       lwrAddressData.location.lrf.sublocation,
                       lwrAddressData.address.rawAddress);
            }
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
// To disable ECC, MODS must override some scratch bits that IFR uses
// during devinit. This function must be called before devinit to have
// any effect.
RC VoltaGpuSubdevice::DisableEcc()
{
    UINT32 scratchVal = Regs().Read32(MODS_PBUS_SW_SCRATCH, 1);

    scratchVal |= BIT(2); // Set INFOROM_EEN_PRESENT
    scratchVal &= ~BIT(3); // Clear INFOROM_EEN_ECC

    Regs().Write32(MODS_PBUS_SW_SCRATCH, 1, scratchVal);

    return OK;
}

// To enable ECC, MODS must override some scratch bits that IFR uses
// during devinit. This function must be called before devinit to have
// any effect.
RC VoltaGpuSubdevice::EnableEcc()
{
    UINT32 scratchVal = Regs().Read32(MODS_PBUS_SW_SCRATCH, 1);

    scratchVal |= BIT(2); // Set INFOROM_EEN_PRESENT
    scratchVal |= BIT(3); // Set INFOROM_EEN_ECC

    Regs().Write32(MODS_PBUS_SW_SCRATCH, 1, scratchVal);

    return OK;
}

//------------------------------------------------------------------------------
RC VoltaGpuSubdevice::DisableDbi()
{
    RegHal& regs = TestDevice::Regs();
    regs.Write32(MODS_PFB_FBPA_FBIO_CONFIG_DBI, 0x0);
    if (FrameBuffer::IsHbm(this))
    {
        UINT32 mrs = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS);
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, mrs & ~0x3);
    }
    else
    {
        UINT32 mrs = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS1);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR5_RDBI_DISABLED);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR5_WDBI_DISABLED);
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS1, mrs);
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC VoltaGpuSubdevice::EnableDbi()
{
    RegHal& regs = TestDevice::Regs();
    regs.Write32(MODS_PFB_FBPA_FBIO_CONFIG_DBI,
                 regs.SetField(MODS_PFB_FBPA_FBIO_CONFIG_DBI_IN_ON_ENABLED) |
                 regs.SetField(MODS_PFB_FBPA_FBIO_CONFIG_DBI_OUT_ON_ENABLED) |
                 regs.SetField(MODS_PFB_FBPA_FBIO_CONFIG_DBI_OUT_MODE_SSO));
    if (FrameBuffer::IsHbm(this))
    {
        UINT32 mrs = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS);
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS, mrs | 0x3);
    }
    else
    {
        UINT32 mrs = regs.Read32(MODS_PFB_FBPA_GENERIC_MRS1);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR5_RDBI_ENABLED);
        regs.SetField(&mrs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR5_WDBI_ENABLED);
        regs.Write32(MODS_PFB_FBPA_GENERIC_MRS1, mrs);
    }
    return RC::OK;
}

namespace
{
    struct IlwalidateCompbitCachePollArgs
    {
        GpuSubdevice *gpuSubdevice;
        UINT32 ltc;
        UINT32 slice;
    };

    static bool PollCompbitCacheSliceIlwalidated(void *pvArgs)
    {
        IlwalidateCompbitCachePollArgs *args =
            static_cast<IlwalidateCompbitCachePollArgs *>(pvArgs);

        UINT32 value = args->gpuSubdevice->RegRd32(LW_PLTCG_LTC0_LTS0_CBC_CTRL_1
            + (args->ltc * LW_LTC_PRI_STRIDE)
            + (args->slice * LW_LTS_PRI_STRIDE));

        return (LW_PLTCG_LTC0_LTS0_CBC_CTRL_1_ILWALIDATE_INACTIVE ==
            DRF_VAL(_PLTCG, _LTC0_LTS0_CBC_CTRL_1, _ILWALIDATE, value));
    }
}

//-----------------------------------------------------------------------------
//! \brief Ilwalidate the compression bit cache
/* virtual */ RC VoltaGpuSubdevice::IlwalidateCompBitCache(FLOAT64 timeoutMs)
{
    RC rc;
    const UINT32 ltcNum    = GetFB()->GetLtcCount();
    const UINT32 ltcSlices = GetFB()->GetMaxSlicesPerLtc();

    MASSERT(ltcNum > 0);
    MASSERT(ltcSlices > 0);

    RegWr32(LW_PLTCG_LTCS_LTSS_CBC_CTRL_1,
        DRF_DEF(_PLTCG, _LTCS_LTSS_CBC_CTRL_1, _ILWALIDATE, _ACTIVE));

    // Wait until comptaglines for all LTCs and slices are set.
    for (UINT32 ltc = 0; ltc < ltcNum; ++ltc)
    {
        for (UINT32 slice = 0; slice < ltcSlices; ++slice)
        {
            IlwalidateCompbitCachePollArgs args;
            args.gpuSubdevice = this;
            args.ltc = ltc;
            args.slice = slice;

            CHECK_RC(POLLWRAP_HW(&PollCompbitCacheSliceIlwalidated, &args,
                                 timeoutMs));
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the number of comptag lines per cache line.
/* virtual */ UINT32 VoltaGpuSubdevice::GetComptagLinesPerCacheLine()
{
    return DRF_VAL(_PLTCG, _LTCS_LTSS_CBC_PARAM, _COMPTAGS_PER_CACHE_LINE,
        RegRd32(LW_PLTCG_LTCS_LTSS_CBC_PARAM));
}
//-----------------------------------------------------------------------------
//! \brief Get the compression bit cache line size.
/* virtual */ UINT32 VoltaGpuSubdevice::GetCompCacheLineSize()
{
    UINT32 compCacheLineSize = DRF_VAL(_PLTCG, _LTCS_LTSS_CBC_PARAM,
        _CACHE_LINE_SIZE, RegRd32(LW_PLTCG_LTCS_LTSS_CBC_PARAM));
    compCacheLineSize = 512 << compCacheLineSize;
    return compCacheLineSize;
}
//-----------------------------------------------------------------------------
//! \brief Get the compression bit cache base address
/* virtual */ UINT32 VoltaGpuSubdevice::GetCbcBaseAddress()
{
    return DRF_VAL(_PLTCG, _LTCS_LTSS_CBC_BASE, _ADDRESS,
        RegRd32(LW_PLTCG_LTCS_LTSS_CBC_BASE));
}

//------------------------------------------------------------------------------
// GM107 yes | GM108 Yes | GM200 yes | GM204 yes | GM206 yes | GM204X yes | GM206X yes |
bool VoltaGpuSubdevice::ApplyFbCmdMapping
(
    const string &FbCmdMapping,
    UINT32 *pPfbCfgBcast,
    UINT32 *pPfbCfgBcastMask
) const
{
    if (FbCmdMapping == "GDDR5_170_X8")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR5_170_X8);
    else if (FbCmdMapping == "GDDR5_170_MIRR_X8")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_GDDR5_170_MIRR_X8);
    else if (FbCmdMapping == "SDDR4_BGA78")
        Regs().SetField(pPfbCfgBcast,
                MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING_SDDR4_BGA78);
    else
        return GpuSubdevice::ApplyFbCmdMapping(
                FbCmdMapping, pPfbCfgBcast, pPfbCfgBcastMask);

    *pPfbCfgBcastMask |=
        Regs().LookupMask(MODS_PFB_FBPA_FBIO_BROADCAST_CMD_MAPPING);
    return true;
}

//------------------------------------------------------------------------------
/* virtual */ RC VoltaGpuSubdevice::PrepareEndOfSimCheckImpl
(
    Gpu::ShutDownMode mode
    ,bool bWaitgfxDeviceIdle
    ,bool bForceFenceSyncBeforeEos
)
{
    RC rc;

    CHECK_RC(GpuSubdevice::PrepareEndOfSimCheck(mode));

    if (bWaitgfxDeviceIdle)
    {
        // Wait LW_PGRAPH_STATUS_STATE_IDLE
        Printf(Tee::PriNormal, "%s: Wait LW_PGRAPH_STATUS_STATE "
            "to be idle\n", __FUNCTION__);
        CHECK_RC(POLLWRAP_HW(&PollGraphicDeviceIdle, this, Tasker::NO_TIMEOUT));

    }

    // always insert fence sync to ensure the poll is completed before EOS if chip is gm20x+.
    // Bug 200048362
    if ((Gpu::ShutDownMode::QuickAndDirty != mode) || (bForceFenceSyncBeforeEos))
    {
        CHECK_RC(InsertEndOfSimFence(LW_PPRIV_SYS_PRI_FENCE,
                                     LW_PPRIV_GPC_GPC0_PRI_FENCE,
                                     LW_GPC_PRIV_STRIDE,
                                     LW_PPRIV_FBP_FBP0_PRI_FENCE,
                                     LW_FBP_PRIV_STRIDE,
                                     bForceFenceSyncBeforeEos));
    }

    return rc;
}

//------------------------------------------------------------------------------
bool VoltaGpuSubdevice::GetFbGddr5x8Mode()
{
    NamedProperty *pFbGddr5x8ModeProp;
    bool           FbGddr5x8Mode = false;
    if ((OK != GetNamedProperty("FbGddr5x8Mode", &pFbGddr5x8ModeProp)) ||
        (OK != pFbGddr5x8ModeProp->Get(&FbGddr5x8Mode)))
    {
        Printf(Tee::PriDebug, "FbGddr5x8Mode is not valid!!\n");
    }
    return FbGddr5x8Mode;
}

RC VoltaGpuSubdevice::PostRmInit()
{
    if (Platform::GetSimulationMode() != Platform::Hardware &&
        Regs().IsSupported(MODS_PFIFO_USERD_WRITEBACK))
    {
        Regs().Write32(MODS_PFIFO_USERD_WRITEBACK_TIMER_SHORT);
        Regs().Write32(MODS_PFIFO_USERD_WRITEBACK_TIMESCALE_0);

        // Insert the write prevention at the start of the masking array so that
        // it will be guaranteed that the register isnt rewritten
        const UINT32 regOff = Regs().LookupAddress(MODS_PFIFO_USERD_WRITEBACK);
        RegMaskRange maskReg(GetGpuInst(), regOff, regOff, 0);
        g_RegWrMask.insert(g_RegWrMask.begin(), maskReg);
    }

    return GpuSubdevice::PostRmInit();
}

/*virtual*/ UINT32 VoltaGpuSubdevice::GetUserdAlignment() const
{
    return (1 << LW_RAMUSERD_BASE_SHIFT);
}

bool VoltaGpuSubdevice::IsFbMc2MasterValid()
{
    // It is unset, so therefore valid
    if (m_FbMc2MasterMask == 0)
        return true;

    const RegHal &regs = Regs();
    const UINT32 fbpasPerFb   = (regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS) /
                                 regs.Read32(MODS_PTOP_SCAL_NUM_FBPS));
    const UINT32 fbpasPerSite = m_FbpsPerSite * fbpasPerFb;

    UINT32 lwrrentSite = 0;
    UINT32 fbpMask = GetFsImpl()->FbpMask();
    bool bValidConfig = true;
    do
    {
        UINT32 siteFbpMask = fbpMask & ((1 << m_FbpsPerSite) - 1);

        if (siteFbpMask)
        {
            UINT32 siteMasterMask = (m_FbMc2MasterMask >>
                                     (lwrrentSite * fbpasPerSite)) &
                                    ((1 << fbpasPerSite) - 1);
            UINT32 siteMasterBits = Utility::CountBits(siteMasterMask);
            if (siteMasterBits == 0)
            {
                Printf(Tee::PriError,
                       "No MC2 master specified for site %u\n", lwrrentSite);
                bValidConfig = false;
            }
            else if (siteMasterBits > 1)
            {
                Printf(Tee::PriError,
                       "Multiple MC2 masters (0x%1x) specified for site %u\n",
                       siteMasterBits, lwrrentSite);
                bValidConfig = false;
            }

            // Ensure that the fbpa flagged as master is on a FB that is enabled
            INT32 fbMaster = Utility::BitScanForward(siteMasterMask) / fbpasPerFb;
            if (!(siteFbpMask & (1 << fbMaster)))
            {
                Printf(Tee::PriError,
                       "MC2 master is on floorswept FBP for site %u\n",
                       lwrrentSite);
                bValidConfig = false;
            }
        }
        lwrrentSite++;
        fbpMask >>= m_FbpsPerSite;
    }
    while (fbpMask);
    if (!bValidConfig)
    {
        Printf(Tee::PriError, "Invalid FB MC2 master configuration 0x%08x\n",
               m_FbMc2MasterMask);
        MASSERT(!"Generic assertion failure<refer to previous message>.");
    }
    return bValidConfig;
}

UINT32 VoltaGpuSubdevice::GetFbMc2MasterValue(size_t fbp)
{
    const RegHal &regs = Regs();
    const UINT32 fbpasPerFb   = (regs.Read32(MODS_PTOP_SCAL_NUM_FBPAS) /
                                 regs.Read32(MODS_PTOP_SCAL_NUM_FBPS));
    const UINT32 fbpasPerSite = m_FbpsPerSite * fbpasPerFb;

    INT32 paMaster = Utility::BitScanForward((m_FbMc2MasterMask >>
                                              (fbpasPerSite * (fbp / m_FbpsPerSite))) &
                                             ((1 << fbpasPerSite) - 1));

    // Validity should already have been checked sot there should be exactly
    // one bit set for the site
    MASSERT(paMaster != -1);

    static UINT32 noMasterOnFbpVal = ~0U;
    if (~0U == noMasterOnFbpVal)
    {
        // Clear all enable bits for the FBPA master in the mask.  Enable bits
        // for each PA are the odd bits in
        for (UINT32 i = 0; i < fbpasPerFb; i++)
            noMasterOnFbpVal &= ~(1 << (1 + (2 * i)));
    }
    UINT32 masterFbpVal = noMasterOnFbpVal;

    // The master bit for a PA in the FBP register
    masterFbpVal |= (1 << (1 + (2 * (paMaster % fbpasPerFb))));

    return ((paMaster / fbpasPerFb) == (fbp % m_FbpsPerSite)) ? masterFbpVal : noMasterOnFbpVal;
}

//------------------------------------------------------------------------------
//
/* virtual */ RC VoltaGpuSubdevice::GetVprSize
(
    UINT64 *pSize,
    UINT64 *pAlignment
)
{
    RC rc;
    PHYSADDR dummyAddr;

    rc = Platform::GetVPR(&dummyAddr, pSize);

    // If GetVPR is unsupported, that means the current platform
    // doesn't support MODS creating a VPR region.
    if (RC::UNSUPPORTED_FUNCTION == rc ||
        !Regs().IsSupported(MODS_MMU_PROTECTION_REGION_ADDR_ALIGN))
    {
        *pSize = 0;
        *pAlignment = 0;
        return OK;
    }

    *pAlignment = 1ULL << Regs().LookupValue(MODS_MMU_PROTECTION_REGION_ADDR_ALIGN);

    return rc;
}

//------------------------------------------------------------------------------
//
/* virtual */ RC VoltaGpuSubdevice::GetAcr1Size
(
    UINT64 *pSize,
    UINT64 *pAlignment
)
{
    RC rc;
    JavaScriptPtr pJs;
    UINT32 regValue;

    // Return size=0 if ACR1 is not supported

    if (!Regs().IsSupported(MODS_PFB_PRI_MMU_WPR_INFO) ||
        !Regs().IsSupported(MODS_MMU_PROTECTION_REGION_ADDR_ALIGN))
    {
        *pSize = 0;
        *pAlignment = 0;
        return OK;
    }

    // Check to see if RM has already allocated an ACR1 region.

    regValue = Regs().SetField(MODS_PFB_PRI_MMU_WPR_INFO_INDEX_WPR1_ADDR_LO);
    Regs().Write32(MODS_PFB_PRI_MMU_WPR_INFO, regValue);
    regValue = Regs().Read32(MODS_PFB_PRI_MMU_WPR_INFO);
    UINT64 acr1Start = Regs().GetField(regValue, MODS_PFB_PRI_MMU_WPR_INFO_DATA);
    acr1Start = acr1Start << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

    regValue = Regs().SetField(MODS_PFB_PRI_MMU_WPR_INFO_INDEX_WPR1_ADDR_HI);
    Regs().Write32(MODS_PFB_PRI_MMU_WPR_INFO, regValue);
    regValue = Regs().Read32(MODS_PFB_PRI_MMU_WPR_INFO);
    UINT64 acr1End = Regs().GetField(regValue, MODS_PFB_PRI_MMU_WPR_INFO_DATA);
    acr1End = acr1End << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

    Printf(Tee::PriDebug, "ACR: acr1Start = 0x%llx  acr1End = 0x%llx\n",
           acr1Start, acr1End);

    // If the ACR2 region is valid, that means RM has already allocated it.
    if (acr1Start <= acr1End)
    {
        *pSize = 0;
    }

    // Otherwise, MODS needs to allocate the ACR2 region (assuming the user
    // has specified one).
    else
    {
        CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr1Size, pSize));
    }

    *pAlignment = 1ULL << Regs().LookupValue(
            MODS_MMU_WRITE_PROTECTION_REGION_ADDR_ALIGN);

    return rc;
}

//------------------------------------------------------------------------------
//
/* virtual */ RC VoltaGpuSubdevice::GetAcr2Size
(
    UINT64 *pSize,
    UINT64 *pAlignment
)
{
    RC rc;
    JavaScriptPtr pJs;
    UINT32 regValue;

    // Return size=0 if ACR1 is not supported

    if (!Regs().IsSupported(MODS_PFB_PRI_MMU_WPR_INFO) ||
        !Regs().IsSupported(MODS_MMU_PROTECTION_REGION_ADDR_ALIGN))
    {
        *pSize = 0;
        *pAlignment = 0;
        return OK;
    }

    // Check to see if RM has already allocated an ACR2 region.

    regValue = Regs().SetField(MODS_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_LO);
    Regs().Write32(MODS_PFB_PRI_MMU_WPR_INFO, regValue);
    regValue = Regs().Read32(MODS_PFB_PRI_MMU_WPR_INFO);
    UINT64 acr2Start = Regs().GetField(regValue, MODS_PFB_PRI_MMU_WPR_INFO_DATA);
    acr2Start = acr2Start << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

    regValue = Regs().SetField(MODS_PFB_PRI_MMU_WPR_INFO_INDEX_WPR2_ADDR_HI);
    Regs().Write32(MODS_PFB_PRI_MMU_WPR_INFO, regValue);
    regValue = Regs().Read32(MODS_PFB_PRI_MMU_WPR_INFO);
    UINT64 acr2End = Regs().GetField(regValue, MODS_PFB_PRI_MMU_WPR_INFO_DATA);
    acr2End = acr2End << LW_PFB_PRI_MMU_WPR_INFO_ADDR_ALIGNMENT;

    Printf(Tee::PriDebug, "ACR: acr2Start = 0x%llx  acr2End = 0x%llx\n",
           acr2Start, acr2End);

    // If the ACR1 region is valid, that means RM has already allocated it.
    if (acr2Start <= acr2End)
    {
        *pSize = 0;
    }

    // Otherwise, MODS needs to allocate the ACR1 region (assuming the user
    // has specified one).
    else
    {
        CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr2Size, pSize));
    }

    *pAlignment = 1ULL << Regs().LookupValue(
            MODS_MMU_WRITE_PROTECTION_REGION_ADDR_ALIGN);

    return rc;
}

//------------------------------------------------------------------------------
//
/* virtual */ RC VoltaGpuSubdevice::SetVprRegisters
(
    UINT64 size,
    UINT64 physAddr
)
{
    UINT32 regValue;
    UINT32 regAddrShift =
        Regs().LookupValue(MODS_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT);

    regValue = Regs().SetField(MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_LO);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA,
        LwU64_LO32(physAddr >> regAddrShift));
    Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

    Printf(Tee::PriDebug, "VPR: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n", regValue);

    regValue = Regs().SetField(MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_ADDR_HI);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA,
        LwU64_LO32((physAddr + size - 1) >> regAddrShift));
    Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

    Printf(Tee::PriDebug, "VPR: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n", regValue);

    JavaScriptPtr pJs;
    UINT32 vprCya0;
    UINT32 vprCya1;
    RC rc1 = pJs->GetProperty(Gpu_Object, Gpu_VprCya0, &vprCya0);
    RC rc2 = pJs->GetProperty(Gpu_Object, Gpu_VprCya1, &vprCya1);

    if ((OK == rc1) && (OK == rc2))
    {
        Printf(Tee::PriDebug, "VPR:  vprCya0 = 0x%08x  vprCya1 = 0x%08x\n", vprCya0, vprCya1);

        UINT64 cyaFull = vprCya0 | ((UINT64) vprCya1 << 32);

        //
        // CYA values go into the _DATA field of VPR_INFO using INDEX values of
        // CYA_LO and CYA_HI.  However, in CYA_LO the first bit of _DATA is
        // oclwpied by the IN_USE field.  So we take the full value, shift by 1
        // to make room for IN_USE and then split between the _DATA fields of
        // CYA_LO and CYA_HI.
        //
        cyaFull <<= 1;

        UINT32 mask = Regs().LookupFieldValueMask(
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA);
        UINT32 maskSize = Regs().LookupMaskSize(
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA);
        UINT32 cyaLo = LwU64_LO32(mask & cyaFull);
        UINT32 cyaHi = LwU64_LO32(mask & (cyaFull >> maskSize));

        Printf(Tee::PriDebug, "VPR:  cyaLo = 0x%08x  cyaHi = 0x%08x\n", cyaLo, cyaHi);

        regValue = Regs().SetField(
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_LO);
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA, cyaLo);
        Regs().SetField(&regValue,
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_VPR_CYA_LO_IN_USE, 1);
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

        Printf(Tee::PriDebug, "VPR: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n", regValue);

        regValue = Regs().SetField(
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_VPR_CYA_HI);
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA, cyaHi);
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

        Printf(Tee::PriDebug, "VPR: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n", regValue);
    }

    do
    {
        regValue = Regs().Read32(MODS_PFB_PRI_MMU_VPR_INFO);

    } while (Regs().Test32(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE));

    regValue = Regs().SetField(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE);
    Regs().Write32(MODS_PFB_PRI_MMU_VPR_INFO, regValue);

    do
    {
        regValue = Regs().Read32(MODS_PFB_PRI_MMU_VPR_INFO);

    } while (Regs().Test32(MODS_PFB_PRI_MMU_VPR_INFO_FETCH_TRUE));

    return OK;
}

//------------------------------------------------------------------------------
//
/* virtual */ RC VoltaGpuSubdevice::SetAcrRegisters
(
    UINT64 acr1Size,
    UINT64 acr1PhysAddr,
    UINT64 acr2Size,
    UINT64 acr2PhysAddr
)
{
    RC rc;
    UINT32 regValue;
    const UINT64 acrAddrAlign = 0x20000ULL;

    // Both VPR and ACR region addresses are shifted the same amount
    // before being written to registers, but the ACR region must be
    // 128K aligned.

    UINT32 regAddrShift =
        Regs().LookupValue(MODS_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT);

    if (acr1Size > 0)
    {
        regValue = Regs().SetField(
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR1_ADDR_LO);
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA,
            LwU64_LO32(acr1PhysAddr >> regAddrShift));
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

        Printf(Tee::PriDebug,
               "ACR: WPR1_ADDR_LO: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n",
               regValue);

        UINT64 acr1End = ALIGN_DOWN((acr1PhysAddr + acr1Size - 1), acrAddrAlign);

        regValue = Regs().SetField(
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR1_ADDR_HI);
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA,
            LwU64_LO32(acr1End >> regAddrShift));
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

        Printf(Tee::PriDebug,
               "ACR: WPR1_ADDR_HI: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n",
               regValue);
    }

    if (acr2Size > 0)
    {
        regValue = Regs().SetField(
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR2_ADDR_LO);
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA,
            LwU64_LO32(acr2PhysAddr >> regAddrShift));
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

        Printf(Tee::PriDebug,
            "ACR: WPR2_ADDR_LO: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n",
            regValue);

        UINT64 acr2End = ALIGN_DOWN((acr2PhysAddr + acr2Size - 1), acrAddrAlign);
        regValue = Regs().SetField(
            MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR2_ADDR_HI);
        Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_DATA,
            LwU64_LO32(acr2End >> regAddrShift));
        Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

        Printf(Tee::PriDebug,
            "ACR: WPR2_ADDR_HI: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n",
            regValue);
    }

    UINT32 acr1ReadMask;
    UINT32 acr1WriteMask;
    UINT32 acr2ReadMask;
    UINT32 acr2WriteMask;
    JavaScriptPtr pJs;
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr1ReadMask, &acr1ReadMask));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr1WriteMask, &acr1WriteMask));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr2ReadMask, &acr2ReadMask));
    CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Acr2WriteMask, &acr2WriteMask));

    // RM always sets the level 3 access bit, so do the same here.
    const UINT32 level3Access = 0x8;
    acr1ReadMask  |= level3Access;
    acr1WriteMask |= level3Access;
    acr2ReadMask  |= level3Access;
    acr2WriteMask |= level3Access;

    regValue = Regs().SetField(
        MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR_ALLOW_READ);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_ALLOW_READ_WPR1,
        acr1ReadMask);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_ALLOW_READ_WPR2,
        acr2ReadMask);
    Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

    Printf(Tee::PriDebug,
        "ACR: WPR_ALLOW_READ: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n",
        regValue);

    regValue = Regs().SetField(
        MODS_PFB_PRI_MMU_VPR_WPR_WRITE_INDEX_WPR_ALLOW_WRITE);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_ALLOW_WRITE_WPR1,
        acr1WriteMask);
    Regs().SetField(&regValue, MODS_PFB_PRI_MMU_VPR_WPR_WRITE_ALLOW_WRITE_WPR2,
        acr2WriteMask);
    Regs().Write32(MODS_PFB_PRI_MMU_VPR_WPR_WRITE, regValue);

    Printf(Tee::PriDebug,
        "ACR: WPR_ALLOW_WRITE: RegWr32(LW_PFB_PRI_MMU_VPR_WPR_WRITE, 0x%08x)\n",
        regValue);

    return rc;
}

//------------------------------------------------------------------------------
//
/* virtual */ RC VoltaGpuSubdevice::HaltAndExitPmuPreOs()
{
    RC rc;
    GM20xFalcon pmuFalcon(this, GM20xFalcon::FalconType::PMU);

    // Instruct PREOS (which runs on the PMU) to exit and halt
    // Reference - pmuPreOsExitAndHalt_GV100()
    // LW_VBIOS_PRIV_SCRATCH_01_PREOS_REQUEST_EXIT_AND_HALT = [9:9]
    static constexpr UINT32 preosExitMask = 0x200;
    UINT32 preOsRegval = Regs().Read32(MODS_PBUS_SW_SCRATCH, 1) | preosExitMask;
    Regs().Write32(MODS_PBUS_SW_SCRATCH, 1, preOsRegval);
    CHECK_RC(pmuFalcon.WaitForHalt(5000));   // 5000ms (number is arbitrarily chosen)

    return rc;
}
