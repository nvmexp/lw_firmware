/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
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

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_fbpa.h"


#include "fbflcn_access.h"
#include "fbflcn_table.h"

#include "osptimer.h"
#include "memory.h"
#include "config/g_memory_hal.h"
#include "fbflcn_hbm_mclk_switch.h"


//
// getGPIOIndexForFBVDDQ
//   find the GPIO index for the FB VDDQ GPIO control. This index is can be read from the
//   PMCT Header as a copy of the gpio tables.
//
LwU8
memoryGetGpioIndexForFbvddq_GH100
(
        void
)
{
    if (gBiosTable.gGPIO_PIN_FBVDDQ == LW_GPIO_NUM_UNDEFINED)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_UNDEFINED_GPIO_FBVDDQ_ACCESS);
    }
    return gBiosTable.gGPIO_PIN_FBVDDQ;
}

//
// getGPIOIndexForVref
//   find the GPIO index for the VREF GPIO control. This index is can be read from the
//   PMCT Header as a copy of the gpio tables.
//
LwU8
memoryGetGpioIndexForVref_GH100
(
        void
)
{
    if (gBiosTable.gGPIO_PIN_VREF == LW_GPIO_NUM_UNDEFINED)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_UNDEFINED_GPIO_VREF_ACCESS);
    }
    return gBiosTable.gGPIO_PIN_VREF;
}


GCC_ATTRIB_SECTION("memory", "memorySetMrsValuesHbm_GH100")
void
memorySetMrsValuesHbm_GH100
(
    void
)
{

    // Enable DRAM_ACK
    //enable_dram_ack_ga100(1);
    lwr->cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACK, 1, lwr->cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0, lwr->cfg0);

    //REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS, new->mrs0);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS1, new->mrs1);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS2, new->mrs2);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS3, new->mrs3);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS4, new->mrs4);
//#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100))
#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X)) && (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS5, new->mrs5);
#endif
}


GCC_ATTRIB_SECTION("memory", "memoryGetMrsValueHbm_GH100")
void
memoryGetMrsValueHbm_GH100
(
        void
)
{
    // MRS values are not fully stored in the mem tweak tables.  The are in the
    // memory index table used to boot and will contain all the physical & static
    // settings (e.g 4 high vs 8 high).
    // mclk switches have to replace only partial subsets of data typicaly from
    // other registers from the tables

    // read the current values


    //lwr->mrs0 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS);
    lwr->mrs1 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS1);
    lwr->mrs2 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS2);
    lwr->mrs3 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS3);
    lwr->mrs4 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS4);
//#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100))
#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X)) && (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
    lwr->mrs5 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS5);
#endif
    //gbl_mrs4 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS4);  // full static values with no update required during mclk switch
    //lwr->mrs15 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS15);

    LwU8 hbm_mode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE,
            REG_RD32(LOG, LW_PFB_FBPA_FBIO_BROADCAST));

    if (hbm_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM3)
    {
        // MR0
        //   OP[7]   Test Mode                  static
        //   OP[6]   Add/CMD Parity             static
        //   OP[5]   DQ Write Parity            static
        //   OP[4]   DQ Read Parity             static
        //   OP[3]   rsvd
        //   OP[2]   TCSR                       static
        //   OP[1]   DBIac Write                table
        //   OP[0]   DBIac Read                 table
        //
        // MR1
        //   OP[7:5] Parity Latency [PL]        static (max:3)
        //   OP[4:0] Write Latency [WL]         table  (min:4,max:16) <- mem tweak table: config1.wl
        //
        // MR2
        //   OP[7:0] Read Latency [RL]          table  (min:4,max:63) <- mem tweak table: config1.cl
        //
        // MR3
        //   OP[7:0] Write Recovery [WR]        table  (min:4,max:63)<- mem tweak table: config2.wr
        //
        // MR4  lwrrently all static values, no update required during mclk switch
        //   OP[7:0] Active to Prechare [RAS]   table  (min:4,max:63)<- mem tweak table: config0.ras
        //
        // MR5
        //   OP[7:4] RFU
        //   OP[3:0] Read to Precharge [RTP]           (min:2,max:15)
        //
        // MR6
        //   OP[7:6] RFU
        //   OP[5:3] Pullup Driver Strength
        //   OP[2:0] Pulldown Driver Strength
        //
        // MR7
        //   OP[7]   CATTRIP
        //   OP[6]   RFU
        //   OP[5:3] DWord MISR Control
        //   OP[2]   RFU
        //   OP[1]   DWord Read Mux Control
        //   OP[0]   DWord Loopback
        //
        // MR8
        //   OP[7:6] RFU
        //   OP[5:4] RFM Levels (RFML)
        //   OP[3]   WDQS-to-CK Training (WDQS2CK)
        //   OP[2]   ECS error log auto reset
        //   OP[1]   Duty Cycle Monitor (DCM)
        //   OP[0]   DA Port Lockout
        //
        // MR9
        //   OP[7]   ECS Error Type and Address Reset (ECSRES)
        //   OP[6]   ECS multi-bit error correction (ECSCEM)
        //   OP[5]   Auto ECS during Self Refresh (ECSSRF)
        //   OP[4]   Auto ECS via REFab (ECSREF)
        //   OP[3]   Error Vector Pattern (ECCVEC)
        //   OP[2]   Error Vector Input Mode (ECCTM)
        //   OP[1]   Severity Reporting (SEVR)
        //   OP[0]   Meta Data (MD)
        //
        // MR10
        //   OP[7:0] Vendor Specific
        //
        // MR11
        //   OP[7:4] DCA code for WDQS1 (PC1)
        //   OP[3:0] DCA code for WDQS0 (PC0)
        //
        // MR12
        //   OP[7:0] Reserved for-Vendor Specific
        //
        // MR13
        //   OP[7:0] RFU
        //
        // MR14
        //   OP[7]   RFU
        //   OP[6:1] Reference voltage for AWORD inputs (VREFCA)
        //   OP[0]   RFU
        //
        // MR15
        //   OP[7]   RFU
        //   OP[6:1] Reference voltage for DWORD inputs (VREFD)
        //   OP[0]   RFU

        // HBM3 ---------------------------------------------------------
#define LW_PFB_FBPA_GENERIC_MRS1_ADR_HBM3_WL                               4:0 /* ----F */
#define LW_PFB_FBPA_GENERIC_MRS1_ADR_HBM3_PL                               7:5 /* ----F */
#define LW_PFB_FBPA_GENERIC_MRS2_ADR_HBM3_RL                               7:0 /* ----F */
#define LW_PFB_FBPA_GENERIC_MRS3_ADR_HBM3_WR                               7:0 /* ----F */
#define LW_PFB_FBPA_GENERIC_MRS4_ADR_HBM3_RAS                              7:0 /* ----F */
        // MRS1
        LwU32 mrs1 = lwr->mrs1;

        // write latency (wl), this should be programmed to WL-1
        mrs1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_HBM3_WL,
                (REF_VAL(LW_PFB_FBPA_CONFIG1_WL,new->config1)),    // get WL from config1
                mrs1);

        new->mrs1 = mrs1;

        // need to set PL, no need.

        // MRS2
        LwU32 mrs2 = lwr->mrs2;
        // read latency (cl) this should be programmed to CL-2
        mrs2 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS2, _ADR_HBM3_RL,
                (REF_VAL(LW_PFB_FBPA_CONFIG1_CL,new->config1)),   // get CL from config1
                mrs2);

        new->mrs2=mrs2;

        // MRS3
        LwU32 mrs3 = lwr->mrs3;
        // write recovery
        mrs3 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS3, _ADR_HBM3_WR,
                REF_VAL(LW_PFB_FBPA_CONFIG2_WR, new->config2),    // get wr from config2
                mrs3);
        new->mrs3 = mrs3;

        // MRS4
        LwU32 mrs4 = lwr->mrs4;
        // active to precharge (ras)
        mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4, _ADR_HBM3_RAS,
                REF_VAL(LW_PFB_FBPA_CONFIG0_RAS,new->config0),   // get ras from config0
                mrs4);
        new->mrs4=mrs4;


//#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100))
#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X)) && (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
        // MRS5
        // only field is [3:0] Read to Precharge [RTP]           (min:2,max:15)
        LwU32 mrs5 = lwr->mrs5;
        // active to precharge (ras)
        mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _HBM3_RTP,
                gTT.perfMemTweakBaseEntry.pmtbe391384.Timing1_R2P, mrs5);
        new->mrs5=mrs5;
#endif

        // MRS6
        // drive strength have turned into a boot time options,
        //LwU8 drv_strength = 0x7;  // FIXME: needs to come from table
        //mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_HBM_DRV, drv_strength, mrs1);

    }
    else
    {
        // MR0
        //   OP[7]: Test Mode                   static
        //   OP[6]: Add/CMD Parity              static
        //   OP[5]: DQ Write Parity             static
        //   OP[4]: DQ Read Parity              static
        //   OP[3]: rsvd
        //   OP[2]: TCSR                        static
        //   OP[1]: DBIac Write                 table
        //   OP[0]: DBIac Read                  table
        //
        // MR1
        //   OP[7:5] Driver Strength            table  <- mem tweak table: pmtbe375368.DrvStrength   THIS IS ONLY 2 BIT, NEED 3
        //   OP[4:0] Write Recovery [WR]        table  <- mem tweak table: config2.wr
        //
        // MR2
        //   OP[7:3] Read Latency [RL]          table  <- mem tweak table: config1.cl
        //   OP[2:0] Write Latency [WL]         table  <- mem tweak table: config1.wl
        //
        // MR3
        //   OP[7] BL                           static
        //   OP[6] Bank Group                   static
        //   OP[5:0] Active to Prechare [RAS]   table   <- mem tweak table: config0.ras
        //
        // MR4  lwrrently all static values, no update required during mclk switch
        //   OP[7:4] rsvd
        //   OP[3:2] Parity Latency [PL]        static
        //   OP[1]   DM                         static
        //   OP[0]   ECC                        static
        //
        // MR15
        //   OP[7:3] rsvd
        //   OP[2:0] Optional Internal Vref     table

        // MRS0
        //LwU32 mrs0 = lwr->mrs0;

        // dbi settings have turned into boot time options removing the need
        // to write/update MRS0
        // read dbi
        //LwU8 dbi_read = 1;     // FIXME:  needs to come from table
        //mrs0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS, _ADR_HBM_RDBI, dbi_read, mrs0);

        // write dbi
        //LwU8 dbi_write = 1;    // FIXME:  needs to come from table
        //mrs0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS, _ADR_HBM_WDBI, dbi_write, mrs0);
        //new->mrs0 = mrs0;

        // MRS1
        LwU32 mrs1 = lwr->mrs1;

        // drive strength have turned into a boot time options,
        //LwU8 drv_strength = 0x7;  // FIXME: needs to come from table
        //mrs1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_HBM_DRV, drv_strength, mrs1);

        // write recovery
        mrs1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_HBM_WR,
                REF_VAL(LW_PFB_FBPA_CONFIG2_WR, new->config2),    // get wr from config2
                mrs1);
        new->mrs1 = mrs1;


        // MRS2
        LwU32 mrs2 = lwr->mrs2;
        // read latency (cl) this should be programmed to CL-2
        mrs2 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS2, _ADR_HBM_RL,
                (REF_VAL(LW_PFB_FBPA_CONFIG1_CL,new->config1) - 2),    // get CL from config1
                mrs2);

        // write latency (wl), this should be programmed to WL-1
        mrs2 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS2, _ADR_HBM_WL,
                (REF_VAL(LW_PFB_FBPA_CONFIG1_WL,new->config1) - 1),    // get WL from config1
                mrs2);
        new->mrs2=mrs2;

        // MRS3
        LwU32 mrs3 = lwr->mrs3;
        // active to precharge (ras)
        mrs3 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS3, _ADR_HBM_RAS,
                REF_VAL(LW_PFB_FBPA_CONFIG0_RAS,new->config0),   // get ras from config0
                mrs3);
        new->mrs3 = mrs3;

        // MRS4
        LwU32 mrs4 = lwr->mrs4;
        // mrs 4 need to be programmed based on read latency (cl) for HBM 2E
        if(REF_VAL(LW_PFB_FBPA_CONFIG1_CL,new->config1) > 33)
        {
              mrs4 |= (0x1 << 5);     // set bit 5
        }
        else
        {
              mrs4 &= ~((LwU32)0x1 << 5);     // clear bit 5
        }
        // mrs 4 need to be programmed based on write latency (wl) for HBM 2E
        if(REF_VAL(LW_PFB_FBPA_CONFIG1_WL,new->config1) > 8)
        {
              mrs4 |= (0x1 << 4);     // set bit 4
        }
        else
        {
              mrs4 &= ~((LwU32)0x1 << 4);     // clear bit 4
        }
        new->mrs4=mrs4;

#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X)) && (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
        new->mrs5 = lwr->mrs5;
#endif
        // MSR15
        //LwU32 mrs15 = lwr->mrs15;

        // internal vref has turned into a boot time options, removing the need
        // to update mrs15 during an mclk switch/
        // optional internal vref
        //LwU8 ivref = 7;  // FIXME: this needs to come from the table
        //mrs15 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS15, _ADR_HBM_IVREF, ivref, mrs15);
        //new->mrs15 = mrs15;
    }
}
