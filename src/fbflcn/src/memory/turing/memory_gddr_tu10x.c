/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include <falc_debug.h>
#include <falc_trace.h>

#include "config/fbfalcon-config.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_defines.h"
#include "fbflcn_gddr_boot_time_training_tu10x.h"
#include "memory_gddr.h"
#include "data.h"
#include "priv.h"
#include "memory.h"
#include "config/g_memory_hal.h"
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#include "seq_decoder.h"
#endif

#include "fbflcn_objfbflcn.h"

#include "dev_fbpa.h"

#include "dev_fbfalcon_csb.h"

//extern TRAINING_STRUCT gTD;
extern REGISTER_DATA gRD;

EdcTrackingGainTable* gblpedctrgain_start;
EdcTrackingGainTable* gblpedctrgain_setup;
REGBOX lwrrentRegValues, newRegValues;
REGBOX*  lwr_reg = &lwrrentRegValues;
REGBOX*  new_reg = &newRegValues;

LwBool GblVrefTrackPerformedOnce = LW_FALSE;
LwBool GblDfeInitVrefTrAclwmEdgeSet = LW_FALSE;

extern LwBool gbl_en_fb_mclk_sw;

void
memoryMove3RegPerByteValues_TU10X
(
        LwU32 *buf,
        LwU32 startReg,
        REGISTER_ACCESS_TYPE cmd
)
{
    LwU32* pData = buf;
    LwU8 partition;
    for (partition=0; partition<MAX_PARTS; partition++)
    {
        if (isPartitionEnabled(partition))
        {
            LwU8 subp;
            for (subp=0; subp<SUBPS; subp++)
            {
                if ((subp == 1) || (isLowerHalfPartitionEnabled(partition)))
                {
                    LwU8 inx = 0;    // inx loops through bytes w/ 3 entries per byte
                    LwU32 regAdr = unicast_wr_vref_dfe(startReg,partition,subp,inx);
                    for(inx=0; inx<(BYTE_PER_SUBP*3); inx++)
                    {
                        if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_WRITE)
                        {
                            REG_WR32(BAR0, regAdr, *(pData++));
                        }
                        else
                        {
                            *(pData++) = REG_RD32(BAR0, regAdr);
                        }
                        regAdr += 4;
                    }
                }
                else
                {
                    pData += (BYTE_PER_SUBP * 3);
                }
            }
        }
        else
        {
            pData += (SUBPS * BYTE_PER_SUBP* 3);
        }
    }
}


void
memoryMoveVrefCodeTrainingValues_TU10X
(
        LwU32* pBuffer,
        REGISTER_ACCESS_TYPE cmd
)
{
    memoryMove3RegPerByteValues_HAL(&Fbflcn, pBuffer, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1, cmd);

}

/*
 * DFE training values were not saved and restored on the the Turing platform

void
memoryMoveVrefDfeTrainingValues_TU10X
(
        REGISTER_ACCESS_TYPE cmd
)
{
    LwU32* pData = &gRD.vrefDfeCtrl[0];
    memoryMove3RegPerByteValues_HAL(&Fbflcn, pData, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, cmd);
}
*/



// Simple translator to get a unicast address based off the broadcast address
// and the partition index
GCC_ATTRIB_SECTION("resident", "unicast_rd")
LwU32 unicast_rd (LwU32 priv_addr, LwU32 partition)
{
    LwU32 retval;

    retval = priv_addr - 0x000a0000 + (partition * 0x00004000);

    return (retval);
}

// Simple translator to get a unicast address based off the broadcast address
// and the partition index
GCC_ATTRIB_SECTION("resident", "unicast_rd_offset")
LwU32 unicast_rd_offset (LwU32 priv_addr, LwU32 partition, LwU32 offset)
{
    LwU32 retval;

    retval = priv_addr - 0x000a0000 + (partition * 0x00004000) + offset;

    return (retval);
}


// Simple translator to get a unicast address based off the broadcast address
// and the partition index
GCC_ATTRIB_SECTION("resident", "unicast_wr_vref_dfe")
LwU32 unicast_wr_vref_dfe (LwU32 priv_addr, LwU32 partition, LwU32 subp, LwU32 byte)
{
    LwU32 retval;

    retval = priv_addr - 0x000a0000 + (partition * 0x00004000) + (subp * 48) + (byte * 12);

    return (retval);
}

//  FBIOTRNG_SUBP0_CMD_BRLSHFT1 : 0x00900D98
//  FBIOTRNG_SUBP0_CMD_INTERPLTR: 0x00904D70


void
memoryMoveCmdTrainingValues_TU10X
(
        LwU32* pBuffer,
        REGISTER_ACCESS_TYPE cmd
)
{
    LwU8 i;

    for (i=0; i<MAX_PARTS; i++)
    {
        if (isPartitionEnabled(i))
        {
            // do subpartiton 0 if enabled
            if (isLowerHalfPartitionEnabled(i))
            {
                if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_WRITE)
                {
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,i), *(pBuffer));
                    pBuffer++;
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT2,i), *(pBuffer));
                    pBuffer++;
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT3,i), *(pBuffer));
                    pBuffer++;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
                    REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT4,i), *(pBuffer));
                    pBuffer++;
#endif
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR1,i), *(pBuffer));
                    pBuffer++;
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR2,i), *(pBuffer));
                    pBuffer++;
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR3,i), *(pBuffer));
                    pBuffer++;
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR4,i), *(pBuffer));
                    pBuffer++;
                    REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR5,i), *(pBuffer));
                    pBuffer++;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
                    REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR6,i), *(pBuffer));
                    pBuffer++;
                    REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR7,i), *(pBuffer));
                    pBuffer++;
#endif
                }
                else
                {
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,i));
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT2,i));
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT3,i));
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT4,i));
#endif
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR1,i));
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR2,i));
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR3,i));
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR4,i));
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR5,i));
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR6,i));
                    *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_INTRPLTR7,i));
#endif
                }
            }
            else
            {
                pBuffer += (CMD_BRLSHFT_REGS_IN_SUBP + CMD_INTRPLTR_REGS_IN_SUBP);
            }

            // do subpartition 1 - always on when partition is enabled
            if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_WRITE)
            {
                REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT1,i), *(pBuffer));
                pBuffer++;
                REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT2,i), *(pBuffer));
                pBuffer++;
                REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT3,i), *(pBuffer));
                pBuffer++;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
                REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT4,i), *(pBuffer));
                pBuffer++;
#endif
                REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR1,i), *(pBuffer));
                pBuffer++;
                REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR2,i), *(pBuffer));
                pBuffer++;
                REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR3,i), *(pBuffer));
                pBuffer++;
                REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR4,i), *(pBuffer));
                pBuffer++;
                REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR5,i), *(pBuffer));
                pBuffer++;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
                REG_WR32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR6,i), *(pBuffer));
                pBuffer++;
                REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR7,i), *(pBuffer));
                pBuffer++;
#endif

            }
            else
            {
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT1,i));
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT2,i));
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT3,i));
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT4,i));
#endif
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR1,i));
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR2,i));
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR3,i));
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR4,i));
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR5,i));
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR6,i));
                *(pBuffer++) = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_INTRPLTR7,i));
#endif
            }
        }
        else
        {
            pBuffer += SUBPS * (CMD_BRLSHFT_REGS_IN_SUBP + CMD_INTRPLTR_REGS_IN_SUBP);
        }
    }
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_ma2sdr")
void
gddr_pgm_ma2sdr
(
        LwU32 force_ma2sdr
)
{
    LwU32 fbpa_fbio_spare;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    fbpa_fbio_spare = DRF_NUM(_PFB,_FBPA_FBIO_SPARE_VALUE,_FORCE_MA_SDR,force_ma2sdr);
            //force_ma2sdr << DRF_SHIFT(LW_PFB_FBPA_FBIO_SPARE_FORCE_MA2SDR);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SPARE, fbpa_fbio_spare,
            DRF_SHIFTMASK(LW_PFB_FBPA_FBIO_SPARE_VALUE_FORCE_MA_SDR));
#else
    fbpa_fbio_spare= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SPARE);
    fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE,       _FORCE_MA_SDR,  force_ma2sdr,      fbpa_fbio_spare);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE,       fbpa_fbio_spare);
#endif
}

GCC_ATTRIB_SECTION("memory", "gddr_training_qpop_offset")
void
gddr_training_qpop_offset
(
        LwU32 *pqpop_offset
)
{
    LwU32 fbpa_config1;
    LwU32 fbpa_config1_qpop_offset;
    fbpa_config1= REG_RD32(LOG, LW_PFB_FBPA_CONFIG1);
    fbpa_config1_qpop_offset = REF_VAL(LW_PFB_FBPA_CONFIG1_QPOP_OFFSET,fbpa_config1);
    fbpa_config1 = FLD_SET_DRF_NUM(_PFB,    _FBPA_CONFIG1,  _QPOP_OFFSET,    *pqpop_offset,        fbpa_config1);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG1,  fbpa_config1);
    *pqpop_offset = fbpa_config1_qpop_offset;
}

GCC_ATTRIB_SECTION("memory", "gddr_setup_addr_tr")
void
gddr_setup_addr_tr
(
        void
)
{
    LwU32 fbpa_training_patram;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if ((GblDdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
        (GblDdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
        //func_set_prbs_dual_mode(0,1);
        fbpa_training_patram = DRF_DEF(_PFB, _FBPA_TRAINING_PATRAM, _DUAL_MODE, _ENABLE);
        SEQ_REG_RMW32(LOG, LW_PFB_FBPA_TRAINING_PATRAM, fbpa_training_patram, DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_PATRAM_DUAL_MODE));
#else
    if (GblDdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        fbpa_training_patram= REG_RD32(LOG, LW_PFB_FBPA_TRAINING_PATRAM);
        fbpa_training_patram = FLD_SET_DRF(_PFB,    _FBPA_TRAINING_PATRAM,  _DUAL_MODE,      _ENABLE,  fbpa_training_patram);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM,fbpa_training_patram);
#endif
    }


    gddr_pgm_ma2sdr(1);                        // FORCE_MA_TO_SDR_ALIGNMENT = 1 before kicking off address training

#if (!FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if (GblDdrMode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_TADR_CTRL, 0x00000040);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_CTRL, 0x0000d000);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_CORS_CTRL, 0x14024000);     // special for !ga10x
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_FINE_CTRL, 0x03047F81);
    }
#endif
    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_pgm_trng_lower_subp")
void
gddr_pgm_trng_lower_subp
(
        LwU32 fbpa_trng_cmd
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {
        LwU32 fbpa_training_mask;
        fbpa_training_mask  = DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_ADR) |         // 16
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_CHAR_ENGINE) |  // 23
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_CONTROL) |      // 29:28
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_PERIODIC) |     // 24
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_QUSE) |         // 21
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_QUSE_PRIME) |   // 22
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_RD) |           // 18
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_RW_LEVEL) |     // 26
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_STEP) |         // 30
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_STROBE) |       // 27
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_WCK) |          // 17
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_WR) |           // 19
            DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_GO);            // 31

        //LwU32 unicast_wr_vref_dfe (LwU32 priv_addr, LwU32 partition, LwU32 subp, LwU32 byte)
        //retval = priv_addr - 0x000a0000 + (partition * 0x00004000) + (subp * 48) + (byte * 12);
        //addr = LW_PFB_FBPA_TRAINING_CMD(0) - 0x000a0000 + (idx * 0x00004000);

// this should get an ACK even if subp does not exist
    // using special op code
        SEQ_REG_RMWL32(LOG, LW_PFB_FBPA_TRAINING_CMD(0), fbpa_trng_cmd, fbpa_training_mask);
    }
    else
#endif
    {
        // Subp0 training kicked off only if supb isn't disabled;
        // if partition isn't floorswept but just disabled, subp0 could still kick off training and fail.
        LwU8 idx;
        for (idx = 0; idx < MAX_PARTS; idx++)
        {
            if(isLowerHalfPartitionEnabled(idx))
            {
                REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_TRAINING_CMD(0)), idx, 0, 0),fbpa_trng_cmd);
            }
        }
    }
#else
    // Subp0 training kicked off only if supb isn't disabled;
    // if partition isn't floorswept but just disabled, subp0 could still kick off training and fail.
    LwU8 idx;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if(isLowerHalfPartitionEnabled(idx))
        {
            REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_TRAINING_CMD(0)), idx, 0, 0),fbpa_trng_cmd);
        }
    }
#endif
    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_start_addr_training")
void
gddr_start_addr_training
(
        void
)
{
    LwU32 fbpa_training_cmd;
    LwU32 control = 2;
    LwU32 quse = 0;
    LwU32 quse_prime = 0;
    LwU32 rw_level = 1;
    LwU32 step = 0;
    LwU32 strobe = 0;
    LwU32 adr = 1;
    LwU32 go = 1;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    fbpa_training_cmd = 0;
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _ADR,          adr,          fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _CHAR_ENGINE,  0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _CONTROL,      control,      fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _PERIODIC,     0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _QUSE,         quse,         fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _QUSE_PRIME,   quse_prime,   fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _RD,           0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _RW_LEVEL,     rw_level,     fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _STEP,         step,         fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _STROBE,       strobe,       fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _WCK,          0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _WR,           0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _GO,           go,           fbpa_training_cmd);

    LwU32 fbpa_training_mask;
    fbpa_training_mask  = DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_ADR) |     // 16
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_CHAR_ENGINE)           |     // 23
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_CONTROL)               |     // 29:28
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_PERIODIC)              |     // 24
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_QUSE)                  |     // 21
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_QUSE_PRIME)            |     // 22
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_RD)                    |     // 18
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_RW_LEVEL)              |     // 26
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_STEP)                  |     // 30
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_STROBE)                |     // 27
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_WCK)                   |     // 17
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_WR)                    |     // 19
        DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_CMD_GO);                         // 31

    gddr_pgm_trng_lower_subp(fbpa_training_cmd);

    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_TRAINING_CMD(1), fbpa_training_cmd, fbpa_training_mask);
#else
    fbpa_training_cmd = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CMD(0));
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _ADR,          adr,          fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _CHAR_ENGINE,  0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _CONTROL,      control,      fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _PERIODIC,     0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _QUSE,         quse,         fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _QUSE_PRIME,   quse_prime,   fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _RD,           0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _RW_LEVEL,     rw_level,     fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _STEP,         step,         fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _STROBE,       strobe,       fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _WCK,          0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _WR,           0,            fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _GO,           go,           fbpa_training_cmd);

    gddr_pgm_trng_lower_subp(fbpa_training_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(1),fbpa_training_cmd);
#endif
    return;

}

GCC_ATTRIB_SECTION("memory", "gddr_check_training_passed")
LwBool
gddr_check_training_passed
(
        void
)
{
    LwU32 fbpa_trng_status;
    LwU32 subp0_trng_status;
    LwU32 subp1_trng_status;
    LwU32 training_passed;

    fbpa_trng_status = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_STATUS);
    subp0_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS, fbpa_trng_status);
    subp1_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS, fbpa_trng_status);

    training_passed = ((subp0_trng_status == 0) && (subp1_trng_status == 0));

    return training_passed;
}

GCC_ATTRIB_SECTION("memory", "gddr_wait_for_training_done")
void
gddr_wait_for_training_done
(
        void
)
{
    LwU32 fbpa_trng_status;
    LwU32 subp0_trng_status;
    LwU32 subp1_trng_status;

    do {
        // return TRAINING_STATUS_SUBP0_STATUS_FINISHED if subp0 is disabled
        fbpa_trng_status = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_STATUS);
        subp0_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS, fbpa_trng_status);
        subp1_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS, fbpa_trng_status);
    } while ((subp0_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS_RUNNING) || (subp1_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS_RUNNING));

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_addr_training")
LwBool
gddr_addr_training
(
        LwU32 DdrMode
)
{

    LwU32 pqpop_offset;
    LwBool tr_status;
    GblDdrMode = DdrMode;

    // Use a QPOP_OFFSET of 2 for address training.  Bug 810795
    pqpop_offset = 2;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG1,
        FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG1, _QPOP_OFFSET, pqpop_offset, new_reg->pfb_fbpa_config1)
    );

    gddr_setup_addr_tr();

    gddr_start_addr_training();

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {       // run mclk sw on PA Falcon
        seq_pa_write(LW_PFB_FBPA_FALCON_MONITOR,0xD1D1ADD2);
        seq_pa_write(LW_CFBFALCON_SYNC_STATUS, 0x2E02);    // PA updates status to step 46

        SEQ_REG_TRNCHK(LW_PFB_FBPA_TRAINING_STATUS);

        seq_pa_write(LW_PFB_FBPA_CONFIG1, new_reg->pfb_fbpa_config1);  // restore val
        seq_pa_write(LW_PFB_FBPA_FALCON_MONITOR,0xD0D0ADD2);
// poll already done above, FB sync should catch the error status
        tr_status = LW_TRUE;
    }
    else
#endif
    {
        REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,0xD1D1ADD2);

        gddr_wait_for_training_done();

        REG_WR32(LOG, LW_PFB_FBPA_CONFIG1, new_reg->pfb_fbpa_config1);  // restore val

        REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,0xD0D0ADD2);
        tr_status = gddr_check_training_passed();
    }
#else
    gddr_training_qpop_offset(&pqpop_offset);
    gddr_setup_addr_tr();

    // func_run_training(addr, wck, rd, wr, periodic,char);
    //func_run_training(1, 0, 0, 0, 0, 0);
    gddr_start_addr_training();
    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR,0xD1D1ADD2);

    gddr_wait_for_training_done();
    // FORCE_MA_TO_SDR_ALIGNMENT = 0 at end of address training
    //gddr_pgm_ma2sdr(0);

    // Restore QPOP_OFFSET after address training.  Bug 810795
    gddr_training_qpop_offset(&pqpop_offset);

    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR,0xD0D0ADD2);
    //FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DEBUG_HALT);

    tr_status = gddr_check_training_passed();
#endif

    if (tr_status)
    {
        enableEvent(EVENT_SAVE_TRAINING_DATA);
    }
    return tr_status;
}

GCC_ATTRIB_SECTION("memory", "func_program_vref_bkv_values")
void
func_program_vref_bkv_values
(
        LwU32 vref_code
)
{

    LwU32 vref_code3;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    LwU32 shiftmask = DRF_SHIFTMASK(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD9);
    
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3,   _PAD9,   vref_code,         0);
  
    // subp0 byte0
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3, vref_code3, shiftmask);

    // subp0 byte1
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_VREF_CODE3, vref_code3, shiftmask);

    // subp0 byte2
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_VREF_CODE3, vref_code3, shiftmask);

    // subp0 byte3
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_VREF_CODE3, vref_code3, shiftmask);

    // subp1 byte0
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_VREF_CODE3, vref_code3, shiftmask);

    // subp1 byte1
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_VREF_CODE3, vref_code3, shiftmask);

    // subp1 byte2
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_VREF_CODE3, vref_code3, shiftmask);

    //subp1 byte3
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_VREF_CODE3, vref_code3, shiftmask);
#else
    // subp0 byte0
    vref_code3= REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3,   _PAD9,   vref_code,         vref_code3);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3,     vref_code3);

    // subp0 byte1
    vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_VREF_CODE3);
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE1_VREF_CODE3,   _PAD9,   vref_code,         vref_code3);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_VREF_CODE3,     vref_code3);

    // subp0 byte2
    vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_VREF_CODE3);
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE2_VREF_CODE3,   _PAD9,   vref_code,         vref_code3);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_VREF_CODE3,     vref_code3);

    // subp0 byte3
    vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_VREF_CODE3);
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE3_VREF_CODE3,   _PAD9,   vref_code,         vref_code3);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_VREF_CODE3,     vref_code3);

    // subp1 byte0
    vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_VREF_CODE3);
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP1BYTE0_VREF_CODE3,   _PAD9,   vref_code,         vref_code3);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_VREF_CODE3,     vref_code3);

    // subp1 byte1
    vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_VREF_CODE3);
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP1BYTE1_VREF_CODE3,   _PAD9,   vref_code,         vref_code3);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_VREF_CODE3,     vref_code3);

    // subp1 byte2
    vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_VREF_CODE3);
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP1BYTE2_VREF_CODE3,   _PAD9,   vref_code,         vref_code3);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_VREF_CODE3,     vref_code3);

    //subp1 byte3
    vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_VREF_CODE3);
    vref_code3 =FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP1BYTE3_VREF_CODE3,   _PAD9,   vref_code,         vref_code3);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_VREF_CODE3,     vref_code3);
#endif

}

GCC_ATTRIB_SECTION("memory", "func_reset_aclwmulator_edge_offset_bkv")
void
func_reset_aclwmulator_edge_offset_bkv
(
        LwU32 bkv,
        LwU32 offset_bkv
)
{
    LwU32 vref_tracking1;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    LwU32 vref_tracking1_mask;
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           0);
    if (offset_bkv > 127) 
    {
      vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,   _EDGE_OFFSET_BKV_MSB,        1,    vref_tracking1); 
    } 
    else
    {
      vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,   _EDGE_OFFSET_BKV_MSB,        0,    vref_tracking1);
    }
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1); //add MSB
    vref_tracking1_mask = DRF_SHIFTMASK(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1_EDGE_OFFSET_BKV_MSB) | 
	                  DRF_SHIFTMASK(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1_EDGE_OFFSET_BKV) |  
                          DRF_SHIFTMASK(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1_ACLWMULATOR_RESET_VAL);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,   vref_tracking1, vref_tracking1_mask);

    //new_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1 = vref_tracking1;    // not updated...

    // assume mask & field positions are the same and we can simplifiy this...
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE1_VREF_TRACKING1,   vref_tracking1, vref_tracking1_mask);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE2_VREF_TRACKING1,   vref_tracking1, vref_tracking1_mask);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE3_VREF_TRACKING1,   vref_tracking1, vref_tracking1_mask);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE0_VREF_TRACKING1,   vref_tracking1, vref_tracking1_mask);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE1_VREF_TRACKING1,   vref_tracking1, vref_tracking1_mask);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE2_VREF_TRACKING1,   vref_tracking1, vref_tracking1_mask);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE3_VREF_TRACKING1,   vref_tracking1, vref_tracking1_mask);
#else
    vref_tracking1= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           vref_tracking1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,   vref_tracking1);

    new_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1 = vref_tracking1;

    vref_tracking1= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE1_VREF_TRACKING1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE1_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           vref_tracking1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE1_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE1_VREF_TRACKING1,   vref_tracking1);

    vref_tracking1= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE2_VREF_TRACKING1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE2_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           vref_tracking1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE2_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE2_VREF_TRACKING1,   vref_tracking1);

    vref_tracking1= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE3_VREF_TRACKING1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE3_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           vref_tracking1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP0BYTE3_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE3_VREF_TRACKING1,   vref_tracking1);

    vref_tracking1= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE0_VREF_TRACKING1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP1BYTE0_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           vref_tracking1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP1BYTE0_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE0_VREF_TRACKING1,   vref_tracking1);

    vref_tracking1= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE1_VREF_TRACKING1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP1BYTE1_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           vref_tracking1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP1BYTE1_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE1_VREF_TRACKING1,   vref_tracking1);

    vref_tracking1= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE2_VREF_TRACKING1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP1BYTE2_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           vref_tracking1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP1BYTE2_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE2_VREF_TRACKING1,   vref_tracking1);

    vref_tracking1= REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE3_VREF_TRACKING1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP1BYTE3_VREF_TRACKING1,   _ACLWMULATOR_RESET_VAL,  bkv,           vref_tracking1);
    vref_tracking1 = FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIO_SUBP1BYTE3_VREF_TRACKING1,   _EDGE_OFFSET_BKV,        offset_bkv,    vref_tracking1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE3_VREF_TRACKING1,   vref_tracking1);
#endif

}

GCC_ATTRIB_SECTION("memory", "func_fbio_intrpltr_offsets")
void func_fbio_intrpltr_offsets
(
        LwU32 Ib,
        LwU32 Ob
)
{
    LwU32 fbpa_fbio_intrpltr_offset;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    fbpa_fbio_intrpltr_offset= lwr_reg->pfb_fbpa_fbio_intrpltr_offset;
    fbpa_fbio_intrpltr_offset = FLD_SET_DRF_NUM(_PFB,       _FBPA_FBIO_INTRPLTR_OFFSET,     _IB,     Ib,         fbpa_fbio_intrpltr_offset);
    fbpa_fbio_intrpltr_offset = FLD_SET_DRF_NUM(_PFB,       _FBPA_FBIO_INTRPLTR_OFFSET,     _OB,     Ob,         fbpa_fbio_intrpltr_offset);

//    LwU32 fbpa_fbio_intrpltr_mask;
//    fbpa_fbio_intrpltr_mask = DRF_SHIFTMASK(LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET_OB) | DRF_SHIFTMASK(LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET_IB);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET,     fbpa_fbio_intrpltr_offset);
    lwr_reg->pfb_fbpa_fbio_intrpltr_offset = fbpa_fbio_intrpltr_offset;
#else
    fbpa_fbio_intrpltr_offset= REG_RD32(LOG, LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET);
    fbpa_fbio_intrpltr_offset = FLD_SET_DRF_NUM(_PFB,       _FBPA_FBIO_INTRPLTR_OFFSET,     _IB,     Ib,         fbpa_fbio_intrpltr_offset);
    fbpa_fbio_intrpltr_offset = FLD_SET_DRF_NUM(_PFB,       _FBPA_FBIO_INTRPLTR_OFFSET,     _OB,     Ob,         fbpa_fbio_intrpltr_offset);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET,     fbpa_fbio_intrpltr_offset);
#endif
}


GCC_ATTRIB_SECTION("memory", "gddr_flush_mrs_reg")
void
gddr_flush_mrs_reg
(
        void
)
{
    LwU32 fbpa_cfg0;
    LwU32 fbpa_nop;


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    //setting dram ack
    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED,fbpa_cfg0);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

    //Writing NoP command
    fbpa_nop = lwr_reg->pfb_fbpa_nop;
    fbpa_nop = FLD_SET_DRF(_PFB, _FBPA_NOP, _CMD, _NOP_1, fbpa_nop);
    lwr_reg->pfb_fbpa_nop = fbpa_nop;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_NOP,fbpa_nop);

    //clearing dram ack
    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _DISABLED,fbpa_cfg0);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
#else
    //setting dram ack
    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED,fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

    //Writing NoP command
    fbpa_nop = lwr_reg->pfb_fbpa_nop;
    fbpa_nop = FLD_SET_DRF(_PFB, _FBPA_NOP, _CMD, _NOP_1, fbpa_nop);
    lwr_reg->pfb_fbpa_nop = fbpa_nop;
    REG_WR32(LOG, LW_PFB_FBPA_NOP,fbpa_nop);

    //clearing dram ack
    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _DISABLED,fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
#endif
    return;

}

GCC_ATTRIB_SECTION("memory", "gddr_setup_edc_tracking")
void
gddr_setup_edc_tracking
(
        LwBool bMclkSwitch,
        EdcTrackingGainTable* pEdcTrackingGainTable
)
{

    gblpedctrgain_setup = pEdcTrackingGainTable;
    //asatish time optimization : making dfe ctrl to be one time programable
    //clearing the pad9 DFE value
    if (GblDfeInitVrefTrAclwmEdgeSet == LW_FALSE) {
      LwU32 DFE_CTRL3;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
      DFE_CTRL3 = FLD_SET_DRF(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL3, _PAD9, _INIT, 0);

      LwU32 DFE_CTRL3_MASK = DRF_SHIFTMASK(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL3_PAD9);

      SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL3,DFE_CTRL3,DFE_CTRL3_MASK);
      SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE1_VREF_DFE_CTRL3,DFE_CTRL3,DFE_CTRL3_MASK);
      SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE2_VREF_DFE_CTRL3,DFE_CTRL3,DFE_CTRL3_MASK);
      SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE3_VREF_DFE_CTRL3,DFE_CTRL3,DFE_CTRL3_MASK);
      SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE0_VREF_DFE_CTRL3,DFE_CTRL3,DFE_CTRL3_MASK);
      SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE1_VREF_DFE_CTRL3,DFE_CTRL3,DFE_CTRL3_MASK);
      SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE2_VREF_DFE_CTRL3,DFE_CTRL3,DFE_CTRL3_MASK);
      SEQ_REG_RMW32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE3_VREF_DFE_CTRL3,DFE_CTRL3,DFE_CTRL3_MASK);
#else
      DFE_CTRL3 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL3);
      DFE_CTRL3 = FLD_SET_DRF(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL3, _PAD9, _INIT,DFE_CTRL3);
      REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL3,DFE_CTRL3);
      REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE1_VREF_DFE_CTRL3,DFE_CTRL3);
      REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE2_VREF_DFE_CTRL3,DFE_CTRL3);
      REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE3_VREF_DFE_CTRL3,DFE_CTRL3);
      REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE0_VREF_DFE_CTRL3,DFE_CTRL3);
      REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE1_VREF_DFE_CTRL3,DFE_CTRL3);
      REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE2_VREF_DFE_CTRL3,DFE_CTRL3);
      REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1BYTE3_VREF_DFE_CTRL3,DFE_CTRL3);
#endif
    }

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    //    FOLLOWUP to add spare register field to control
        if (bMclkSwitch) {
           // Step a. TRAINING_EDC_CTRL_EDC_TRACKING_MODE=1
           // VBIOS will have edc_tracking_mode=1 and edc_relock_wait_time=10
           lwr_reg->pfb_fbpa_training_edc_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_EDC_CTRL, _EDC_TRACKING_MODE, 1, lwr_reg->pfb_fbpa_training_edc_ctrl);
           SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_EDC_CTRL,lwr_reg->pfb_fbpa_training_edc_ctrl);
        }
#endif


    // Step b. FBIO_EDC_TRACKING_DEBUG_DISABLE_WDQS_PI_UPDATES = 1
    LwU32 disable_wdqs_pi_updates = 1;
    LwU32 fbpa_fbio_edc_tracking_debug;
    fbpa_fbio_edc_tracking_debug = lwr_reg->pfb_fbpa_fbio_edc_tracking_debug;
    fbpa_fbio_edc_tracking_debug = FLD_SET_DRF_NUM(_PFB,    _FBPA_FBIO_EDC_TRACKING_DEBUG,  _DISABLE_WDQS_PI_UPDATES,        disable_wdqs_pi_updates,    fbpa_fbio_edc_tracking_debug);
    lwr_reg->pfb_fbpa_fbio_edc_tracking_debug = fbpa_fbio_edc_tracking_debug;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,  fbpa_fbio_edc_tracking_debug);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,  fbpa_fbio_edc_tracking_debug);
#endif

    // Step c. FBIO_VREF_TRACKING_DISABLE_TRAINING_UPDATES = 1
    ////   dis_tr_update = 1;
    LwU32 rd_flip_direction = 0;
    LwU32 wr_flip_direction = 0;
    LwU32 rd_gain = pEdcTrackingGainTable->EDCTrackingRdGainInitial;
    LwU32 wr_gain = pEdcTrackingGainTable->EDCTrackingWrGainInitial;
    LwU32 half_rate_edc;
    LwU32 edge_intrpltr_offset;
    LwU32 use_center_only;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if (REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast) == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
    {
      edge_intrpltr_offset = 32;
      use_center_only = 0;
      half_rate_edc = 0;
    } else {
      edge_intrpltr_offset = 64;
      use_center_only = 1;
      half_rate_edc = 1;
    }
#else
      edge_intrpltr_offset = 64;
      use_center_only = 1;
      half_rate_edc = 1;
#endif


    LwU32 edge_brlshft = 0;
    LwU32 center_brlshft = 0;
    LwU32 disable_training_updates = 0;
    LwU32 block_count = 4;
    LwU32 en = 0;
    LwU32 hunt_mode = 1;
    LwU32 aclwm_bkv = 0;
    LwU32 edge_offset_bkv = 0;
    LwU32 fbpa_fbio_edc_tracking;

    fbpa_fbio_edc_tracking = lwr_reg->pfb_fbpa_fbio_edc_tracking;
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _RD_FLIP_DIRECTION,      rd_flip_direction,  fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _WR_FLIP_DIRECTION,      wr_flip_direction,  fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _RD_GAIN,        rd_gain,    fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _WR_GAIN,        wr_gain,    fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _EDGE_INTRPLTR_OFFSET,   edge_intrpltr_offset,       fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _EDGE_BRLSHFT,   edge_brlshft,       fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _CENTER_BRLSHFT,         center_brlshft,     fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _DISABLE_TRAINING_UPDATES,       disable_training_updates,   fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _BLOCK_COUNT,    block_count,        fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _EN,     en,         fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _HUNT_MODE,      hunt_mode,  fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _USE_CENTER_ONLY,        use_center_only,    fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _HALF_RATE_EDC,  half_rate_edc,      fbpa_fbio_edc_tracking);
    lwr_reg->pfb_fbpa_fbio_edc_tracking = fbpa_fbio_edc_tracking;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
#endif


    // Step d. LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG_DISABLE_REFRESH_RECENTER = 1;
    ////   disable_refresh_recenter = 1;
    LwU32 disable_refresh_recenter = 1;
    fbpa_fbio_edc_tracking_debug = lwr_reg->pfb_fbpa_fbio_edc_tracking_debug;
    fbpa_fbio_edc_tracking_debug = FLD_SET_DRF_NUM(_PFB,    _FBPA_FBIO_EDC_TRACKING_DEBUG,  _DISABLE_REFRESH_RECENTER,       disable_refresh_recenter,   fbpa_fbio_edc_tracking_debug);
    lwr_reg->pfb_fbpa_fbio_edc_tracking_debug = fbpa_fbio_edc_tracking_debug;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,  fbpa_fbio_edc_tracking_debug);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,  fbpa_fbio_edc_tracking_debug);
#endif

    // Step e. FBIO_EDC_TRACKING_DEBUG_ZERO_OUT_OFFSET_CAL = 1;
    ////   zero_out_offset_cal = 1;
    LwU32 zero_out_offset_cal = 1;
    fbpa_fbio_edc_tracking_debug = lwr_reg->pfb_fbpa_fbio_edc_tracking_debug;
    fbpa_fbio_edc_tracking_debug = FLD_SET_DRF_NUM(_PFB,    _FBPA_FBIO_EDC_TRACKING_DEBUG,  _ZERO_OUT_OFFSET_CAL,        zero_out_offset_cal,    fbpa_fbio_edc_tracking_debug);
    lwr_reg->pfb_fbpa_fbio_edc_tracking_debug = fbpa_fbio_edc_tracking_debug;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,  fbpa_fbio_edc_tracking_debug);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,  fbpa_fbio_edc_tracking_debug);
#endif

    // asatish  : combining step g and h
    // Step g. FBIO_VREF_TRACKING1_EDGE_OFFSET_BKV =64  // NOTE this is part of VBIOS devinit or part of unit level testbench routine (FOLLOWUP check if it is part of unit level tb code)
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if (REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast) == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
    {
       edge_offset_bkv = 197;
    }
    else 
    {
       edge_offset_bkv = 144;
    }
#else
    edge_offset_bkv = 64;
#endif

    // Step h. LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1_ACLWMULATOR_RESET_VAL = 0
    aclwm_bkv = 0;
    if (GblDfeInitVrefTrAclwmEdgeSet == LW_FALSE) {
      func_reset_aclwmulator_edge_offset_bkv(aclwm_bkv, edge_offset_bkv);
    }
    GblDfeInitVrefTrAclwmEdgeSet = LW_TRUE;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    if (REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast) == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
    {
      lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,   _VREF_EDGE_PWRD,      0,   lwr_reg->pfb_fbpa_fbio_vref_tracking);
      lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,   _EDGE_HSSA_PWRD,      0,   lwr_reg->pfb_fbpa_fbio_vref_tracking);
      lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,   _EDGE_INTERP_PWRD,    0,   lwr_reg->pfb_fbpa_fbio_vref_tracking);
      SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,       lwr_reg->pfb_fbpa_fbio_vref_tracking);

    }

    if ((gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6VrefTrackEn) && (GblVrefTrackPerformedOnce == LW_TRUE) &&
        (DRF_VAL(_PFB,_FBPA_SW_CONFIG,_EDC_TR_VREF_SAVE_AND_RESTORE_OPT,lwr_reg->pfb_fbpa_sw_config) != LW_PFB_FBPA_SW_CONFIG_EDC_TR_VREF_SAVE_AND_RESTORE_OPT_INIT)) {
      lwr_reg->pfb_fbpa_fbio_vref_tracking2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING2, _SHADOW_SELECT, 0x1, lwr_reg->pfb_fbpa_fbio_vref_tracking2);
      SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING2,  lwr_reg->pfb_fbpa_fbio_vref_tracking2);
    }
#endif

}


GCC_ATTRIB_SECTION("memory", "func_program_vref_from_vref_tracking")
void func_program_vref_from_vref_tracking
(
        LwS32 vref_offset[TOTAL_DBI_BITS]
)
{
    LwU32 vref_code3;
    LwU32 byte_vref_pad9;
    LwS32 pad9;
    LwU16 idx, subp, byte, offset;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++) {
                if ((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                {
                    for( byte = 0 ; byte < 4; byte++) {
                        offset = (subp * 48) + (byte * 12);
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
                        byte_vref_pad9 = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD8,new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3);
#else
                        byte_vref_pad9 = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD9,new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3);
#endif			
                        vref_code3 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3;
                        pad9 = byte_vref_pad9 + vref_offset[(idx * 8) + (subp * 4) + byte];
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
                        if ((pad9 <= 0 ) || (pad9 > 127)) {
#else
                        if ((pad9 <= 0 ) || (pad9 > 255)) {
#endif
                            FW_MBOX_WR32 (6,pad9);
                            FW_MBOX_WR32 (7,byte_vref_pad9);
                            FW_MBOX_WR32 (8,vref_offset[(idx * 8) + (subp * 4) + byte]);
                            FW_MBOX_WR32 (9,byte);
                            FW_MBOX_WR32 (10,subp);
                            FW_MBOX_WR32 (11,idx);
                            FW_MBOX_WR32 (12,vref_code3);
                            FW_MBOX_WR32 (13,(int)vref_offset);
                            // Falcon to error out if vref is -ve
                            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_VREF_TRACKING_ERROR);
                        }
                        vref_code3 =FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3, _PAD9, pad9, vref_code3);
                        REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3), idx, subp, byte),vref_code3);
                    }
                }
            }
        }
    }
  memoryProgramEdgeOffsetBkv_HAL();
}


GCC_ATTRIB_SECTION("memory", "func_fbio_edc_tracking_debug")
void
func_fbio_edc_tracking_debug
(
        LwU32 byte_sel
)
{
    LwU32 fbpa_fbio_edc_tracking_debug;
    fbpa_fbio_edc_tracking_debug = lwr_reg->pfb_fbpa_fbio_edc_tracking_debug;
    fbpa_fbio_edc_tracking_debug = FLD_SET_DRF_NUM(_PFB,    _FBPA_FBIO_EDC_TRACKING_DEBUG,  _BYTE_SEL,                       byte_sel,                   fbpa_fbio_edc_tracking_debug);
    lwr_reg->pfb_fbpa_fbio_edc_tracking_debug = fbpa_fbio_edc_tracking_debug;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,  fbpa_fbio_edc_tracking_debug);
}

GCC_ATTRIB_SECTION("memory", "func_track_vref")
void
func_track_vref
(
        LwU32 return_vref[TOTAL_DBI_BITS]
)
{
    LwU8 idx;
    LwU8 byte;
    LwU32 fbpa_fbio_vref_tracking;
    for( byte = 0 ; byte < 8; byte++) {
        func_fbio_edc_tracking_debug(byte);
        for (idx = 0; idx < MAX_PARTS; idx++)
        {
            if (isPartitionEnabled(idx))
            {
#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
                fbpa_fbio_vref_tracking = REG_RD32(BAR0, unicast_rd(LW_PFB_FBPA_FBIO_VREF_TRACKING,idx));
                return_vref[(idx * 8) + byte] = REF_VAL(LW_PFB_FBPA_FBIO_VREF_TRACKING_DEBUG_VREF_EDGE_VREF,fbpa_fbio_vref_tracking);
#else
                fbpa_fbio_vref_tracking = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_VREF_TRACKING_READ2);
                return_vref[(idx * 8) + byte] = REF_VAL(LW_PFB_FBPA_FBIO_VREF_TRACKING_READ2_VREF_EDGE_EVEN,fbpa_fbio_vref_tracking);
#endif
            }
        }
    }
    return;


}

GCC_ATTRIB_SECTION("memory", "func_vref_un_swizzled")
void func_vref_un_swizzled
(
        LwU32 vref_to_track[TOTAL_DBI_BITS],
        LwU32 return_vref_unswizzled[TOTAL_DBI_BITS]
)
{
    LwU32 fbpa_swizzle_byte;
    LwU32 swizzle_info[TOTAL_DBI_BITS];
    LwU32 swizzle_byte;
    LwU8 idx;
    LwU8 byte;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            fbpa_swizzle_byte = REG_RD32(BAR0, unicast_rd(LW_PFB_FBPA_SWIZZLE_BYTE,idx));
            if (isLowerHalfPartitionEnabled(idx))
            {
                swizzle_info[(idx * 8) + 0] = REF_VAL(LW_PFB_FBPA_SWIZZLE_BYTE_SUBP0BYTE0,fbpa_swizzle_byte) + (idx * 8);
                swizzle_info[(idx * 8) + 1] = REF_VAL(LW_PFB_FBPA_SWIZZLE_BYTE_SUBP0BYTE1,fbpa_swizzle_byte) + (idx * 8);
                swizzle_info[(idx * 8) + 2] = REF_VAL(LW_PFB_FBPA_SWIZZLE_BYTE_SUBP0BYTE2,fbpa_swizzle_byte) + (idx * 8);
                swizzle_info[(idx * 8) + 3] = REF_VAL(LW_PFB_FBPA_SWIZZLE_BYTE_SUBP0BYTE3,fbpa_swizzle_byte) + (idx * 8);
            }
            swizzle_info[(idx * 8) + 4] = REF_VAL(LW_PFB_FBPA_SWIZZLE_BYTE_SUBP1BYTE0,fbpa_swizzle_byte) + (idx * 8) +4 ;
            swizzle_info[(idx * 8) + 5] = REF_VAL(LW_PFB_FBPA_SWIZZLE_BYTE_SUBP1BYTE1,fbpa_swizzle_byte) + (idx * 8) +4 ;
            swizzle_info[(idx * 8) + 6] = REF_VAL(LW_PFB_FBPA_SWIZZLE_BYTE_SUBP1BYTE2,fbpa_swizzle_byte) + (idx * 8) +4 ;
            swizzle_info[(idx * 8) + 7] = REF_VAL(LW_PFB_FBPA_SWIZZLE_BYTE_SUBP1BYTE3,fbpa_swizzle_byte) + (idx * 8) +4 ;
        }
    }

    for(byte = 0; byte < TOTAL_DBI_BITS; byte++) {
        swizzle_byte = swizzle_info[byte];
        return_vref_unswizzled[byte] = vref_to_track[swizzle_byte];
        //FW_MBOX_WR32(1, return_vref_unswizzled[byte]);
    }

    return;
}

GCC_ATTRIB_SECTION("memory", "func_vref_offset_bkv")
void func_vref_offset_bkv
(
        LwU32 return_vref_offset[TOTAL_DBI_BITS]
)
{
    //LwU8 idx, subp, byte, offset;
    LwU8 idx;
    LwU8 subp;
    LwU8 byte;
    //LwU32 vref_tracking_byte;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                {
                    for( byte = 0 ; byte < 4; byte++)
                    {
                        //offset = (subp * 16) + (byte * 4);
                        //vref_tracking_byte    = REG_RD32(BAR0, unicast_rd_offset(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,idx,offset));
                        return_vref_offset[(idx * 8) + (subp * 4) + byte] = REF_VAL(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1_EDGE_OFFSET_BKV, new_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1);
                        //FW_MBOX_WR32(1, return_vref_offset[(idx * 8) + (subp * 4) + byte]);
                    }
                }
            }
        }
    }
}


GCC_ATTRIB_SECTION("memory", "func_save_vref_values")
void func_save_vref_values
(
        void
)
{
    LwU32 fbio_vref_tracking_sub_byte[TOTAL_DBI_BITS]={0};
    LwU32 fbio_vref_tracking_sub_byte_unswizzle[TOTAL_DBI_BITS]={0};
    //LwU32 fbio_vref_offset_bkv[TOTAL_DBI_BITS];
    LwU32 fbio_vref_offset_bkv;
    LwU32 byte;
    LwS32 fbio_vref_offset[TOTAL_DBI_BITS];
    LwU32 disable_training_updates;
    LwU32 fbpa_fbio_vref_tracking;

    func_track_vref(fbio_vref_tracking_sub_byte);
    func_vref_un_swizzled(fbio_vref_tracking_sub_byte, fbio_vref_tracking_sub_byte_unswizzle);
    //func_vref_offset_bkv(fbio_vref_offset_bkv);
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    new_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1);
    fbio_vref_offset_bkv = (REF_VAL(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1_EDGE_OFFSET_BKV_MSB,new_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1) << 7)  |  REF_VAL(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1_EDGE_OFFSET_BKV, new_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1); 
#else
    fbio_vref_offset_bkv = REF_VAL(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1_EDGE_OFFSET_BKV, new_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1); 
#endif
    for (byte = 0; byte < TOTAL_DBI_BITS; byte ++) {
        fbio_vref_offset[byte] = fbio_vref_tracking_sub_byte_unswizzle[byte] - fbio_vref_offset_bkv;
    }

    // Disable training updates before writing vref values
    //  disable_training_updates = 0;
    disable_training_updates = 0;
    fbpa_fbio_vref_tracking= lwr_reg->pfb_fbpa_fbio_vref_tracking;//REG_RD32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _DISABLE_TRAINING_UPDATES,       disable_training_updates,   fbpa_fbio_vref_tracking);
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _EN, 0,fbpa_fbio_vref_tracking);
#endif
    lwr_reg->pfb_fbpa_fbio_vref_tracking = fbpa_fbio_vref_tracking;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,       fbpa_fbio_vref_tracking);

    func_program_vref_from_vref_tracking(fbio_vref_offset);

    // Restore VREF_TRACKING_DISABLE_TRAINING_UPDATES
    //  disable_training_updates = 1;
    disable_training_updates = 1;
    fbpa_fbio_vref_tracking= lwr_reg->pfb_fbpa_fbio_vref_tracking;//REG_RD32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _DISABLE_TRAINING_UPDATES,       disable_training_updates,   fbpa_fbio_vref_tracking);
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _EN, 1,fbpa_fbio_vref_tracking);
#endif
    lwr_reg->pfb_fbpa_fbio_vref_tracking = fbpa_fbio_vref_tracking;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,       fbpa_fbio_vref_tracking);

}


GCC_ATTRIB_SECTION("memory", "gddr_start_edc_tracking")
void
gddr_start_edc_tracking
(
        LwBool bMclkSwitch,
        LwU32 bFlagG6EdcTrackingEn,
        LwU32 bFlagG6VrefTrackingEn,
        EdcTrackingGainTable* pEdcTrackingGainTable,
	FLCN_TIMESTAMP  startFbStopTimeNs

)
{

    gblpedctrgain_start = pEdcTrackingGainTable;
    //     FW_MBOX_WR32(1, 0x000EDC00);

    // Step 0: Setup EDC hold pattern
    LwU32 fbpa_generic_mrs4;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    LwU32 edc_hold_pattern;
    LwBool EnPrbsPattern;

    LwU8 VbiosEdcVrefVbiosCtrl = gTT.perfMemClkBaseEntry.Param0.PMC11EP0EdcVrefVbiosCtrl;
    LwU8 VbiosPrbsPattern = gTT.perfMemClkBaseEntry.Param0.PMC11EP0PrbsPattern;
    LwU8 VbiosEdcHoldPattern = gTT.perfMemClkBaseEntry.Param0.PMC11EP0EdcHoldPattern;

    LwU8 VbiosEdcTrInitialGainDelay  = gTT.perfMemClkBaseEntry.Param1.PMC11EP1EdcTrInitialGainDelay;
    LwU8 VbiosEdcTrFinalGainDelay    = gTT.perfMemClkBaseEntry.Param1.PMC11EP1EdcTrFinalGainDelay;
    LwU8 VbiosVrefTrInitialGainDelay = gTT.perfMemClkBaseEntry.Param1.PMC11EP1VrefTrInitialGainDelay;
    LwU8 VbiosVrefTrFinalGainDelay   = gTT.perfMemClkBaseEntry.Param1.PMC11EP1VrefTrFinalGainDelay;
#else
    LwU32 spare_field4 = gTT.perfMemClkBaseEntry.spare_field4;
    LwU32 spare_field5 = gTT.perfMemClkBaseEntry.spare_field5;

    LwU32 edc_hold_pattern;
    LwBool EnPrbsPattern;

    LwU8 VbiosEdcVrefVbiosCtrl = spare_field4 & 0x1;
    LwU8 VbiosPrbsPattern = spare_field4 & 0x2;
    LwU8 VbiosEdcHoldPattern = (spare_field4 >>4 ) & 0xF;
    LwU8 VbiosEdcTrInitialGainDelay  = (spare_field4 >> 8 ) & 0xFF;
    LwU8 VbiosEdcTrFinalGainDelay    = (spare_field4 >> 16 ) & 0xFF;
    LwU8 VbiosVrefTrInitialGainDelay = (spare_field4 >> 24 ) & 0xFF;
    LwU8 VbiosVrefTrFinalGainDelay   =  spare_field5 & 0xFF;
#endif 

    if ((VbiosEdcVrefVbiosCtrl != 0) && (bMclkSwitch)) {
        edc_hold_pattern = VbiosEdcHoldPattern;
    } else {	//default
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        if (REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast) == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
        {
            edc_hold_pattern = 0xa;
        } 
        else
        {
            edc_hold_pattern = 0x3;
        }
#else
        edc_hold_pattern = 0x3;
#endif
    }

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
/*    
    fbpa_generic_mrs4 = DRF_NUM(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_EDC_HOLD_PATTERN, edc_hold_pattern);
    SEQ_REG_RMW32(LOG, LW_PFB_FBPA_GENERIC_MRS4,     fbpa_generic_mrs4,
        DRF_SHIFTMASK(LW_PFB_FBPA_GENERIC_MRS4_ADR_GDDR5_EDC_HOLD_PATTERN));
*/      
    fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_EDC_HOLD_PATTERN, edc_hold_pattern, new_reg->pfb_fbpa_generic_mrs4);
    new_reg->pfb_fbpa_generic_mrs4 = fbpa_generic_mrs4;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4, fbpa_generic_mrs4);
#else
    fbpa_generic_mrs4= REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS4);
    fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB,       _FBPA_GENERIC_MRS4,   _ADR_GDDR5_EDC_HOLD_PATTERN ,    edc_hold_pattern,        fbpa_generic_mrs4);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS4,     fbpa_generic_mrs4);
    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
#endif
 
    //Ish: PRBS pattern : should be VBIOS flag
    if ((VbiosEdcVrefVbiosCtrl != 0 ) && (bMclkSwitch)) {
        EnPrbsPattern =  VbiosPrbsPattern;
    } else {	 //default
        EnPrbsPattern = 0;
    }

    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x04);


    if (EnPrbsPattern) {
        LwU32 fbpa_generic_mrs12;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        fbpa_generic_mrs12 = DRF_NUM(_PFB,       _FBPA_GENERIC_MRS12,   _ADR,    0x4);
        SEQ_REG_RMW32(LOG, LW_PFB_FBPA_GENERIC_MRS12,     fbpa_generic_mrs12,
            DRF_SHIFTMASK(LW_PFB_FBPA_GENERIC_MRS12_ADR));
#else
        fbpa_generic_mrs12 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS12);
        fbpa_generic_mrs12 = FLD_SET_DRF_NUM(_PFB,       _FBPA_GENERIC_MRS12,   _ADR,    0x4,        fbpa_generic_mrs12);
        REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS12,     fbpa_generic_mrs12);
#endif
    }
    //
    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(2),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    //     FW_MBOX_WR32(3, 0x000EDC03);
    // Step 1: Enable EDC tracking in hunt mode
    //  hunt_mode = 1;
    //  en        = 1;

    LwU32 hunt_mode = 1;
    LwU32 edc_tracking_en =  bFlagG6EdcTrackingEn;
    LwU32 VrefTrackingEn = bFlagG6VrefTrackingEn;
    LwU32 fbpa_fbio_edc_tracking;
    ////Asatish - was missing from confluence seq.
    //LwU32 edc_disable_training_updates = 0;
    fbpa_fbio_edc_tracking = lwr_reg->pfb_fbpa_fbio_edc_tracking;
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _HUNT_MODE,      hunt_mode,  fbpa_fbio_edc_tracking);
    //    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _DISABLE_TRAINING_UPDATES,       edc_disable_training_updates,   fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _EN,     edc_tracking_en,         fbpa_fbio_edc_tracking);
    lwr_reg->pfb_fbpa_fbio_edc_tracking = fbpa_fbio_edc_tracking;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(3),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
#endif

    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x05);

    //    FW_MBOX_WR32(5, 0x000EDC05);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    LwU8 ib_intrpltr_offset_base = REF_VAL(LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET_IB, lwr_reg->pfb_fbpa_fbio_intrpltr_offset);
    LwU8 ob_intrpltr_offset_base = REF_VAL(LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET_OB, lwr_reg->pfb_fbpa_fbio_intrpltr_offset);

    if (DRF_VAL(_PFB,_FBPA_SW_CONFIG,_EDC_TR_INTRPLTR_OFFSET_OPT,lwr_reg->pfb_fbpa_sw_config) == LW_PFB_FBPA_SW_CONFIG_EDC_TR_INTRPLTR_OFFSET_OPT_INIT)
    {
      func_fbio_intrpltr_offsets(ib_intrpltr_offset_base + 12,ob_intrpltr_offset_base);
      SEQ_WAIT_US(1);

      func_fbio_intrpltr_offsets(ib_intrpltr_offset_base - 12,ob_intrpltr_offset_base);
      SEQ_WAIT_US(1);

      func_fbio_intrpltr_offsets(ib_intrpltr_offset_base + 12,ob_intrpltr_offset_base);
      SEQ_WAIT_US(1);

      func_fbio_intrpltr_offsets(ib_intrpltr_offset_base,ob_intrpltr_offset_base);
    } 
    else
    {
      //Enable the lock support - refer fbio uarch section 12.2.1
       lwr_reg->pfb_fbpa_fbio_edc_tracking1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING1, _LOCK_SUPPORT, 1, lwr_reg->pfb_fbpa_fbio_edc_tracking1);
       lwr_reg->pfb_fbpa_fbio_edc_tracking1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING1, _LOCK_CENTER_TRANSITIONS, 2, lwr_reg->pfb_fbpa_fbio_edc_tracking1);
       lwr_reg->pfb_fbpa_fbio_edc_tracking1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING1, _LOCK_EDGE_TRANSITIONS, 2, lwr_reg->pfb_fbpa_fbio_edc_tracking1);
       SEQ_REG_WR32(LOG,LW_PFB_FBPA_FBIO_EDC_TRACKING1, lwr_reg->pfb_fbpa_fbio_edc_tracking1);
    }
#else
    func_fbio_intrpltr_offsets(50,60);
    OS_PTIMER_SPIN_WAIT_US(1);

    func_fbio_intrpltr_offsets(26,60);
    OS_PTIMER_SPIN_WAIT_US(1);

    func_fbio_intrpltr_offsets(50,60);
    OS_PTIMER_SPIN_WAIT_US(1);

    func_fbio_intrpltr_offsets(38,60);
#endif


    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(3),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    // Step 2: wait_initial_gain 30 us (mclk switch 1 us)
    if (!bMclkSwitch) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(30);
#else
        OS_PTIMER_SPIN_WAIT_US(30);
#endif
    } else if ((VrefTrackingEn) && (GblVrefTrackPerformedOnce == 0))  {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(30);
#else
        OS_PTIMER_SPIN_WAIT_US(30);
#endif
    } else if (VbiosEdcVrefVbiosCtrl) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(VbiosEdcTrInitialGainDelay);
#else
        OS_PTIMER_SPIN_WAIT_US(VbiosEdcTrInitialGainDelay);
#endif
    } else { //default
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(4);
#else
        OS_PTIMER_SPIN_WAIT_US(4);
#endif
    }

    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x06);

    // Step 3: RMW d[`LW_PFB_FBPA_FBIO_EDC_TRACKING_RD_GAIN] = 8;
    LwU32 rd_gain = pEdcTrackingGainTable->EDCTrackingRdGainFinal;
    LwU32 wr_gain = pEdcTrackingGainTable->EDCTrackingWrGainFinal;
    fbpa_fbio_edc_tracking = lwr_reg->pfb_fbpa_fbio_edc_tracking;
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _RD_GAIN,        rd_gain,    fbpa_fbio_edc_tracking);
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _WR_GAIN,        wr_gain,    fbpa_fbio_edc_tracking);
    lwr_reg->pfb_fbpa_fbio_edc_tracking = fbpa_fbio_edc_tracking;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
    //    FW_MBOX_WR32(7, 0x000EDC07);

    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x07);

    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
#endif

    //   // Step 4: wait_final_gain  5 us (mclk switch 1 us)
    if (!bMclkSwitch) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(20);
#else
        OS_PTIMER_SPIN_WAIT_US(20);
#endif
    } else if ((VrefTrackingEn) && (GblVrefTrackPerformedOnce == 0))  {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(20);
#else
        OS_PTIMER_SPIN_WAIT_US(20);
#endif
    } else if (VbiosEdcVrefVbiosCtrl) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(VbiosEdcTrFinalGainDelay);
#else
        OS_PTIMER_SPIN_WAIT_US(VbiosEdcTrFinalGainDelay);
#endif
    } else { //default
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(1);
#else
        OS_PTIMER_SPIN_WAIT_US(1);
#endif
    }

    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x08);

    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(2),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    // Step 5: Enable Vref tracking
    LwU32 flip_direction = 0;
    LwU32 gain = pEdcTrackingGainTable->VrefTrackingGainInitial;
    LwU32 disable_training_updates = 1;
    LwU32 aclwm = 0;
    LwU32 debug_vref_movement_en = 0;
    LwU32 vref_edge_pwrd = 0;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    //edge hssa pwrd and interp pwrd is made 0 for g6  as use_center only is 1 and 
    //is pwrd by use center only
#endif
    LwU32 edge_hssa_pwrd = 0;
    LwU32 edge_interp_pwrd = 0;
    LwU32 fbpa_fbio_vref_tracking;
    fbpa_fbio_vref_tracking= lwr_reg->pfb_fbpa_fbio_vref_tracking;//REG_RD32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _FLIP_DIRECTION,         flip_direction,     fbpa_fbio_vref_tracking);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _GAIN,   gain,       fbpa_fbio_vref_tracking);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _DISABLE_TRAINING_UPDATES,       disable_training_updates,   fbpa_fbio_vref_tracking);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _ACLWM,  aclwm,      fbpa_fbio_vref_tracking);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _EN,     VrefTrackingEn,         fbpa_fbio_vref_tracking);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _DEBUG_VREF_MOVEMENT_EN,         debug_vref_movement_en,     fbpa_fbio_vref_tracking);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _VREF_EDGE_PWRD,         vref_edge_pwrd,     fbpa_fbio_vref_tracking);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _EDGE_HSSA_PWRD,         edge_hssa_pwrd,     fbpa_fbio_vref_tracking);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _EDGE_INTERP_PWRD,       edge_interp_pwrd,   fbpa_fbio_vref_tracking);
    lwr_reg->pfb_fbpa_fbio_vref_tracking = fbpa_fbio_vref_tracking;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
/*
//TODO Fix this once tb shim is in for PA!
// force gain to 1
if (!gbl_en_fb_mclk_sw) {
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _GAIN,   1,       fbpa_fbio_vref_tracking);
}
*/
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,       fbpa_fbio_vref_tracking);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,       fbpa_fbio_vref_tracking);
#endif

    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x09);

    //    FW_MBOX_WR32(9, 0x000EDC09);
    // Step 6: wait_initial_vref_gain 100 us (mclk switch 1 us)
    if (!bMclkSwitch) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(100);
#else
        OS_PTIMER_SPIN_WAIT_US(100);
#endif
    } else if ((VrefTrackingEn) && (GblVrefTrackPerformedOnce == 0)) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(100);
#else
        OS_PTIMER_SPIN_WAIT_US(100);
#endif
    } else if (VbiosEdcVrefVbiosCtrl) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(VbiosVrefTrInitialGainDelay);
#else
        OS_PTIMER_SPIN_WAIT_US(VbiosVrefTrInitialGainDelay);
#endif
    } else { //default
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(4);
#else
        OS_PTIMER_SPIN_WAIT_US(4);
#endif
    }

    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(3),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x0b);

    // Step 7: RMW        d[`LW_PFB_FBPA_FBIO_VREF_TRACKING_GAIN] = 1;
    //  gain = 1;
    gain = pEdcTrackingGainTable->VrefTrackingGainFinal;
    fbpa_fbio_vref_tracking= lwr_reg->pfb_fbpa_fbio_vref_tracking;//REG_RD32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING);
    fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,       _GAIN,   gain,       fbpa_fbio_vref_tracking);
    lwr_reg->pfb_fbpa_fbio_vref_tracking = fbpa_fbio_vref_tracking;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,       fbpa_fbio_vref_tracking);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,       fbpa_fbio_vref_tracking);
#endif
    //     FW_MBOX_WR32(11, 0x000EDC0B);

    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x0c);

    // Step 8: wait_final_vref_gain 10us (mclk switch 1 us)
    if (!bMclkSwitch) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(10);
#else
        OS_PTIMER_SPIN_WAIT_US(10);
#endif
    } else if ((VrefTrackingEn) && (GblVrefTrackPerformedOnce == 0)) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(10);
#else
        OS_PTIMER_SPIN_WAIT_US(10);
#endif
    } else if (VbiosEdcVrefVbiosCtrl) {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(VbiosVrefTrFinalGainDelay);
#else
        OS_PTIMER_SPIN_WAIT_US(VbiosVrefTrFinalGainDelay);
#endif
    } else { //default
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
        SEQ_WAIT_US(1);
#else
        OS_PTIMER_SPIN_WAIT_US(1);
#endif
    }

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    //release the lock support if enabled - refer fbio uarch section 12.2.1
    if (DRF_VAL(_PFB,_FBPA_SW_CONFIG,_EDC_TR_INTRPLTR_OFFSET_OPT,gSwConfig) == LW_PFB_FBPA_SW_CONFIG_EDC_TR_INTRPLTR_OFFSET_OPT_SET)
    {
       lwr_reg->pfb_fbpa_fbio_edc_tracking1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING1, _LOCK_SUPPORT, 0, lwr_reg->pfb_fbpa_fbio_edc_tracking1);
       SEQ_REG_WR32(LOG,LW_PFB_FBPA_FBIO_EDC_TRACKING1, lwr_reg->pfb_fbpa_fbio_edc_tracking1);
       SEQ_WAIT_US(2);
    }
#endif

    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x0d);

    // Step 9: Disable EDC tracking hunt_mode;
    // RMW                d[`LW_PFB_FBPA_FBIO_EDC_TRACKING_HUNT_MODE] = 0;
    // RMW                 LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG_DISABLE_REFRESH_RECENTER = 0;
    hunt_mode = 0;
    fbpa_fbio_edc_tracking = lwr_reg->pfb_fbpa_fbio_edc_tracking;
    fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _HUNT_MODE,      hunt_mode,  fbpa_fbio_edc_tracking);
    lwr_reg->pfb_fbpa_fbio_edc_tracking = fbpa_fbio_edc_tracking;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
#else
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
#endif
    //    FW_MBOX_WR32(13, 0x000EDC0E);

    ////asatish FOLLOWUP - zero out offset cal - required for mclk switch???
    //    LwU32 disable_refresh_recenter = 1;
    //    LwU32 zero_out_offset_cal = 1;
    //    LwU32 fbpa_fbio_edc_tracking_debug;
    //    fbpa_fbio_edc_tracking_debug= REG_RD32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG);
    //    fbpa_fbio_edc_tracking_debug = FLD_SET_DRF_NUM(_PFB,    _FBPA_FBIO_EDC_TRACKING_DEBUG,  _DISABLE_REFRESH_RECENTER,       disable_refresh_recenter,   fbpa_fbio_edc_tracking_debug);
    //    fbpa_fbio_edc_tracking_debug = FLD_SET_DRF_NUM(_PFB,    _FBPA_FBIO_EDC_TRACKING_DEBUG,  _ZERO_OUT_OFFSET_CAL,            zero_out_offset_cal,        fbpa_fbio_edc_tracking_debug);
    //    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,  fbpa_fbio_edc_tracking_debug);


    //    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR,0x0e);

#if (!FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    // Step 10: save vref values
    if ((VrefTrackingEn) && (GblVrefTrackPerformedOnce == 0)) {
         func_save_vref_values();
    }
#endif
}
