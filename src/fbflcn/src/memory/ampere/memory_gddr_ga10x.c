/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
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
#include "priv.h"

#include "config/fbfalcon-config.h"
#include "memory.h"
#include "config/g_memory_hal.h"
#include "fbflcn_objfbflcn.h"

extern REGISTER_DATA gRD;

void
memoryGetByteSwizzle_GA10X
(
        LwU8 partition,
        LwU8* byteSwizzle
)
{
    LwU32 byteSwizzleReg = REG_RD32(BAR0, unicast(LW_PFB_FBPA_SWIZZLE_BYTE,partition));
    LwU32 byte_idx;

    for(byte_idx=0; byte_idx < (SUBPS*BYTE_PER_SUBP); byte_idx++) {
        byteSwizzle[(4*(byte_idx/BYTE_PER_SUBP)) + (((3 << (byte_idx*2)) & byteSwizzleReg) >> (byte_idx*2))] = byte_idx;
    }
}


void
memoryProgramEdgeOffsetBkv_GA10X
(
        void
)
{

    //programming pad9 value to edge offset bkv
    LwU32 pad9 = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD9,
            REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3));
    LwU16 idx, subp, byte, offset;
    LwU32 vref_tracking1;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                if ((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                {
                    for( byte = 0 ; byte < 4; byte++) {
                        offset =  (subp * 16) + (byte * 4);
                        vref_tracking1 = REG_RD32(BAR0, unicast_rd_offset(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1,idx,offset));
                        vref_tracking1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1, _EDGE_OFFSET_BKV, pad9, vref_tracking1);
                        if (pad9 > 127)
                        {
                            vref_tracking1 = FLD_SET_DRF_NUM(_PFB_, FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1, _EDGE_OFFSET_BKV_MSB, 1, vref_tracking1);
                        }
                        else
                        {
                            vref_tracking1 = FLD_SET_DRF_NUM(_PFB_, FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1, _EDGE_OFFSET_BKV_MSB, 0, vref_tracking1);
                        }
                        REG_WR32(LOG, unicast_rd_offset((LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1), idx,offset), vref_tracking1);
                    }
                }
            }
        }
    }
}


void
memoryMoveVrefTrackingShadowValues_GA10X
(
        LwU32* pBuffer,
        REGISTER_ACCESS_TYPE cmd
)
{

    if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    {
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
                        LwU32 byte_idx;
                        for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                        {
                            LwU32 baseAddr =  LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING_SHADOW1;

                            LwU32 addr;
                            addr = unicast_rd_offset(baseAddr, partition, (subp*4 + byte_idx) * 4 * 2);

                            *(pBuffer++) = REG_RD32(BAR0, addr);
                            *(pBuffer++) = REG_RD32(BAR0, addr+4);

                        }  // for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                    }
                    else  // ! if ((subp == 1) || (isLowerHalfPartitionEnabled(partition)))
                    {
                        pBuffer += (BYTE_PER_SUBP * 2);
                    }
                }  // for (subp=0; subp<SUBPS; subp++)
            }
            else  // ! if (isPartitionEnabled(partition))
            {
                pBuffer += (SUBPS * BYTE_PER_SUBP * 2);
            }
        }  // for (partition=0; partition<MAX_PARTS; partition++)
    }  // if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    else
    {
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
                        LwU8 byte_idx;    // inx loops through bytes w/ 3 entries per byte
                        for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                        {
                            LwU32 baseAddr =  LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING_SHADOW1;

                            LwU32 addr;
                            addr = unicast_rd_offset(baseAddr, partition, (subp*4 + byte_idx) * 4 * 2);

                            REG_WR32(BAR0, addr, *(pBuffer++));
                            REG_WR32(BAR0, addr+4, *(pBuffer++));

                        }
                    }
                    else
                    {
                        pBuffer += (BYTE_PER_SUBP * 2);
                    }
                }
            }
            else
            {
                pBuffer += (SUBPS * BYTE_PER_SUBP * 2);
            }
        }
    }

}


void
memoryMoveVrefCodeTrainingValues_GA10X
(
        LwU32* pBuffer,
        REGISTER_ACCESS_TYPE cmd
)
{
    static LwU8  byte_swizzle_array[SUBPS*BYTE_PER_SUBP];
    static LwU8  swizzle_array[8];

    static LwU8 final_vref_array[8];

    /*
    LwU32* pData;
    if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    {
        pData = &gRD.vrefCodeCtrl2[0];
    }
    else
    {
        pData = &gRD.vrefCodeCtrl[0];
    }
    */

    // VREF codes are swizzled while reading from the design
    if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    {
        LwU8 partition;
        for (partition=0; partition<MAX_PARTS; partition++)
        {
            if (isPartitionEnabled(partition))
            {
                memoryGetByteSwizzle_HAL(&Fbflcn,partition,byte_swizzle_array);

                LwU8 subp;
                for (subp=0; subp<SUBPS; subp++)
                {
                    if ((subp == 1) || (isLowerHalfPartitionEnabled(partition)))
                    {

                        //LwU32 regAdr = unicast_wr_vref_dfe(startReg,partition,subp,inx);
                        LwU32 byte_idx;
                        for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                        {
                            LwU32 bit_swizzle_addr = (LW_PFB_FBPA_SWIZZLE_BIT_SUBP0BYTE0 +
                                    (subp*16) +
                                    ((byte_swizzle_array[subp*BYTE_PER_SUBP + byte_idx] - subp*4)*4));
                            LwU32 bit_swizzle = REG_RD32(BAR0, unicast(bit_swizzle_addr ,partition));

                            LwU8 bit_idx;
                            for(bit_idx = 0 ; bit_idx < 8; bit_idx++) {
                                LwU32 swizzle_mask = 15 << (bit_idx * 4);
                                LwU32 swizzle_data = (bit_swizzle & swizzle_mask) >> (bit_idx * 4);
                                if(swizzle_data > 4) {
                                    swizzle_data = swizzle_data - 1;
                                }
                                swizzle_array[bit_idx] = swizzle_data;
                            }

                            // loop the 3 vref types: low,mid,high
                            enum VREF_CODE_FIELD vrefType;
                            for(vrefType=EYE_LOW; vrefType<=EYE_HIGH; vrefType++)
                            {
                                LwU32 vrefBaseAddr = 0;
                                switch (vrefType) {
                                case EYE_LOW:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1;
                                    break;
                                case EYE_MID:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_MID_CODE1;
                                    break;
                                case EYE_HIGH:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_UPPER_CODE1;
                                    break;
                                default:
                                    FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_ILWALID_EYE);
                                }
                                LwU32 vrefAddr;
                                vrefAddr = unicast_rd_offset(vrefBaseAddr, partition, (subp*4 + byte_idx) * 4 * 3);

                                // loop through the 3 vref code registers in each set and extract the codes
                                // for all dq bit
                                LwU8 vref_idx;
                                LwU8  vref_read_array[8];
                                LwU8  vref_straight[3] = {0,0,0};
                                LwU32 vrefPrivAddr = vrefAddr;

                                for(vref_idx = 0; vref_idx < 3; vref_idx++) {
                                    LwU32 pad_vref_read = REG_RD32(BAR0, vrefPrivAddr);
                                    vrefPrivAddr = vrefPrivAddr + 4; //Move to the next address for the next loop iteration

                                    LwU8 pad_vref0 = DRF_VAL(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE1 ,_PAD0, pad_vref_read);
                                    LwU8 pad_vref1 = DRF_VAL(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE1 ,_PAD1, pad_vref_read);
                                    LwU8 pad_vref2 = DRF_VAL(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE1 ,_PAD2, pad_vref_read);
                                    LwU8 pad_vref3 = DRF_VAL(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE1 ,_PAD3, pad_vref_read);
                                    if(vref_idx == 0) {
                                        vref_read_array[0] = pad_vref0;
                                        vref_read_array[1] = pad_vref1;
                                        vref_read_array[2] = pad_vref2;
                                        vref_read_array[3] = pad_vref3;
                                    }
                                    if(vref_idx == 1) {
                                        vref_straight[0]   = pad_vref0;   // dbi
                                        vref_read_array[4] = pad_vref1;
                                        vref_read_array[5] = pad_vref2;
                                        vref_read_array[6] = pad_vref3;
                                    }
                                    if(vref_idx == 2) {
                                        vref_straight[2]   = pad_vref2;   // edc
                                        vref_straight[1]   = pad_vref1;   // edc
                                        vref_read_array[7] = pad_vref0;
                                    }
                                }

                                // use the swizzle data to create logical dq array of codes
                                LwU8 logical_dq;
                                for(bit_idx = 0; bit_idx < 8 ; bit_idx = bit_idx +1) {
                                    logical_dq = swizzle_array[bit_idx];
                                    final_vref_array[logical_dq] = vref_read_array[bit_idx];
                                }

                                LwU32 vrefCode;

                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE1, _PAD0, final_vref_array[0], 0x0);
                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE1, _PAD1, final_vref_array[1], vrefCode);
                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE1, _PAD2, final_vref_array[2], vrefCode);
                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE1, _PAD3, final_vref_array[3], vrefCode);
                                *(pBuffer++) = vrefCode;

                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE2, _PAD4, vref_straight[0], 0x0);
                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE2, _PAD5, final_vref_array[4], vrefCode);
                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE2, _PAD6, final_vref_array[5], vrefCode);
                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE2, _PAD7, final_vref_array[6], vrefCode);
                                *(pBuffer++) = vrefCode;

                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE3, _PAD8, final_vref_array[7], 0x0);
                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE3, _PAD9, vref_straight[1], vrefCode);
                                vrefCode = FLD_SET_DRF_NUM(_PFB_FBPA_FBIOTRNG,_SUBP0BYTE0_VREF_CODE3, _PAD10, vref_straight[2], vrefCode);
                                *(pBuffer++) = vrefCode;

                            }  // for(vrefType=EYE_LOW; vrefType<=EYE_HIGH; vrefType++)
                        }  // for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                    }
                    else  // ! if ((subp == 1) || (isLowerHalfPartitionEnabled(partition)))
                    {
                        pBuffer += (BYTE_PER_SUBP * 3 * VREF_CODE_CTRL_SETS);
                    }
                }  // for (subp=0; subp<SUBPS; subp++)
            }
            else  // ! if (isPartitionEnabled(partition))
            {
                pBuffer += (SUBPS * BYTE_PER_SUBP* 3* VREF_CODE_CTRL_SETS);
            }
        }  // for (partition=0; partition<MAX_PARTS; partition++)
    }  // if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    else
    {
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
                        LwU8 byte_idx;    // inx loops through bytes w/ 3 entries per byte
                        for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                        {
                            enum VREF_CODE_FIELD vrefType;
                            for(vrefType=EYE_LOW; vrefType<=EYE_HIGH; vrefType++)
                            {
                                LwU32 vrefBaseAddr = 0;
                                switch (vrefType) {
                                case EYE_LOW:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1;
                                    break;
                                case EYE_MID:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_MID_CODE1;
                                    break;
                                case EYE_HIGH:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_UPPER_CODE1;
                                    break;
                                default:
                                    FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_ILWALID_EYE);
                                }
                                LwU32 vrefAddr;
                                vrefAddr = unicast_rd_offset(vrefBaseAddr, partition, (subp*4 + byte_idx) * 4 * 3);
                                LwU32 vrefPrivAddr = vrefAddr;

                                LwU8 vref_idx;
                                for(vref_idx = 0; vref_idx < 3; vref_idx++) {
                                    REG_WR32(BAR0, vrefPrivAddr, *(pBuffer++));
                                    vrefPrivAddr = vrefPrivAddr + 4;
                                }
                            }
                        }
                    }
                    else
                    {
                        pBuffer += (BYTE_PER_SUBP * 3* VREF_CODE_CTRL_SETS);
                    }
                }
            }
            else
            {
                pBuffer += (SUBPS * BYTE_PER_SUBP* 3* VREF_CODE_CTRL_SETS);
            }
        }
    }

}

/*
 * memoryMoveVrefDfeTrainingValues
 *
 * memoryMoveVrefDfeTrainingVaues copies the vref dfe values from the register to the memory or from memory to register
 * This set supports legacy dram up to g6 and consiste of the same register count and structure as the vref codes.
 * first register: LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1
 *
 */
void
memoryMoveVrefDfeTrainingValues_GA10X
(
        LwU32* pBuffer,
        REGISTER_ACCESS_TYPE cmd
)
{
    static LwU8  byte_swizzle_array[SUBPS*BYTE_PER_SUBP];
    static LwU8  swizzle_array[8];

    static LwU8 final_vref_array[8];

    /*
    LwU32* pData;
    if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    {
        pData = &gRD.vrefDfeCtrl2[0];
    }
    else
    {
        pData = &gRD.vrefDfeCtrl[0];
    }
    */

    // VREF codes are swizzled while reading from the design
    if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    {
        LwU8 partition;
        for (partition=0; partition<MAX_PARTS; partition++)
        {
            if (isPartitionEnabled(partition))
            {
                memoryGetByteSwizzle_HAL(&Fbflcn,partition,byte_swizzle_array);

                LwU8 subp;
                for (subp=0; subp<SUBPS; subp++)
                {
                    if ((subp == 1) || (isLowerHalfPartitionEnabled(partition)))
                    {
                        LwU32 byte_idx;
                        for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                        {
                            LwU32 bit_swizzle = REG_RD32(BAR0, unicast(
                                    (LW_PFB_FBPA_SWIZZLE_BIT_SUBP0BYTE0 +
                                            (subp*16) +
                                            ((byte_swizzle_array[subp*BYTE_PER_SUBP + byte_idx] -subp *4) * 4))
                                            ,partition));
                            // create an array of this swizzle value
                            LwU8 bit_idx;
                            for(bit_idx = 0 ; bit_idx < 8; bit_idx++) {
                                LwU32 swizzle_mask = 15 << (bit_idx * 4);
                                LwU32 swizzle_data = (bit_swizzle & swizzle_mask) >> (bit_idx * 4);
                                if(swizzle_data > 4) {
                                    swizzle_data = swizzle_data - 1;
                                }
                                swizzle_array[bit_idx] = swizzle_data;
                            }


                            LwU32 vrefBaseAddr = LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1;

                            LwU32 vrefAddr;
                            vrefAddr = unicast_rd_offset(vrefBaseAddr, partition, (subp*4 + byte_idx) * 4 * 3);

                            // loop through the 3 ctrl code registers and extract the codes
                            // for all dq bit
                            LwU8 vref_idx;
                            LwU8  vref_read_array[8];
                            LwU8  vref_straight[3] = {0,0,0};
                            LwU32 vrefPrivAddr = vrefAddr;

                            for(vref_idx = 0; vref_idx < 3; vref_idx++) {
                                LwU32 pad_dfe_read = REG_RD32(BAR0, vrefPrivAddr);
                                vrefPrivAddr = vrefPrivAddr + 4; //Move to the next address for the next loop iteration

                                if(vref_idx == 0) {
                                    vref_read_array[0] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL1 ,_PAD0, pad_dfe_read);
                                    vref_read_array[1] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL1 ,_PAD1, pad_dfe_read);
                                    vref_read_array[2] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL1 ,_PAD2, pad_dfe_read);
                                    vref_read_array[3] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL1 ,_PAD3, pad_dfe_read);
                                }
                                if(vref_idx == 1) {
                                    vref_straight[0]   = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL2 ,_PAD4, pad_dfe_read);
                                    vref_read_array[4] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL2 ,_PAD5, pad_dfe_read);
                                    vref_read_array[5] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL2 ,_PAD6, pad_dfe_read);
                                    vref_read_array[6] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL2 ,_PAD7, pad_dfe_read);
                                }
                                if(vref_idx == 2) {
                                    vref_straight[2]   = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL3 ,_PAD8, pad_dfe_read);
                                    vref_straight[1]   = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL3 ,_PAD9, pad_dfe_read);
                                    vref_read_array[7] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL3 ,_PAD10, pad_dfe_read);
                                }
                            }

                            // use the swizzle data to create logical dq array of codes
                            LwU8 logical_dq;
                            for(bit_idx = 0; bit_idx < 8 ; bit_idx = bit_idx +1) {
                                logical_dq = swizzle_array[bit_idx];
                                final_vref_array[logical_dq] = vref_read_array[bit_idx];
                            }

                            // recreate code sets to match vref codes register format
                            LwU32 dfeCtrl;

                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD0, final_vref_array[0], 0x0);
                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD1, final_vref_array[1], dfeCtrl);
                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD2, final_vref_array[2], dfeCtrl);
                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD3, final_vref_array[3], dfeCtrl);
                            *(pBuffer++) = dfeCtrl;

                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL2, _PAD4, vref_straight[0], 0x0);
                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL2, _PAD5, final_vref_array[4], dfeCtrl);
                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL2, _PAD6, final_vref_array[5], dfeCtrl);
                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL2, _PAD7, final_vref_array[6], dfeCtrl);
                            *(pBuffer++) = dfeCtrl;

                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL3, _PAD8, final_vref_array[7], 0x0);
                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL3, _PAD9, vref_straight[1], dfeCtrl);
                            dfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_DFE_CTRL3, _PAD10, vref_straight[2], dfeCtrl);
                            *(pBuffer++) = dfeCtrl;

                        }
                    }
                    else   // ! if ((subp == 1) || (isLowerHalfPartitionEnabled(partition)))
                    {
                        pBuffer += (BYTE_PER_SUBP * 3);
                    }
                }  // for (subp=0; subp<SUBPS; subp++)
            }
            else // ! if(isPartitionEnabled(partition))
            {
                pBuffer += (SUBPS * BYTE_PER_SUBP * 3);
            }
        }  // for (partition=0; partition<MAX_PARTS; partition++)
    }
    else
    {
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
                        LwU8 byte_idx;    // inx loops through bytes w/ 3 entries per byte
                        for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                        {
                            LwU32 vrefAddr = unicast_rd_offset(LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, partition, (subp*4 + byte_idx) * 12);
                            LwU8 ctrl;
                            for(ctrl=0; ctrl<3; ctrl++)
                            {
                                REG_WR32(BAR0, vrefAddr, *(pBuffer++));
                                vrefAddr = vrefAddr + 4;
                            }
                        }
                    }
                    else
                    {
                        pBuffer += (BYTE_PER_SUBP * 3);
                    }
                }
            }
            else
            {
                pBuffer += (SUBPS * BYTE_PER_SUBP * 3);
            }
        }
    }

}


/*
 * memoryMoveVrefDfeG6xTrainingValues
 *
 * memoryMoveVrefDfeTrainingVaues copies the G6X vref dfe values from the register to the memory or from memory to register
 * the G6X set of the dfe ctrl values has only 1 register per byte and tighter packaging. But there are 3 sets for
 * lower, mid and upper dfe
 * first register: LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1
 *
 */
void
memoryMoveVrefDfeG6xTrainingValues_GA10X
(
        LwU32* pBuffer,
        REGISTER_ACCESS_TYPE cmd
)
{
    static LwU8  byte_swizzle_array[SUBPS*BYTE_PER_SUBP];
    static LwU8  swizzle_array[8];

    /*
    LwU32* pData;
    if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    {
        pData = &gRD.vrefDfeG6xCtrl2[0];
    }
    else
    {
        pData = &gRD.vrefDfeG6xCtrl[0];
    }
    */

    // VREF codes are swizzled while reading from the design
    if ((enum REGISTER_ACCESS_ENUM)cmd == REGISTER_READ)
    {
        LwU8 partition;
        for (partition=0; partition<MAX_PARTS; partition++)
        {
            if (isPartitionEnabled(partition))
            {
                memoryGetByteSwizzle_HAL(&Fbflcn,partition,byte_swizzle_array);

                LwU8 subp;
                for (subp=0; subp<SUBPS; subp++)
                {
                    if ((subp == 1) || (isLowerHalfPartitionEnabled(partition)))
                    {
                        LwU32 byte_idx;
                        for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                        {
                            LwU32 bit_swizzle = REG_RD32(BAR0, unicast(
                                    (LW_PFB_FBPA_SWIZZLE_BIT_SUBP0BYTE0 +
                                            (subp*16) +
                                            ((byte_swizzle_array[subp*BYTE_PER_SUBP + byte_idx] -subp *4) * 4))
                                            ,partition));
                            // create an array of this swizzle value
                            LwU8 bit_idx;
                            for(bit_idx = 0 ; bit_idx < 8; bit_idx++) {
                                LwU32 swizzle_mask = 15 << (bit_idx * 4);
                                LwU32 swizzle_data = (bit_swizzle & swizzle_mask) >> (bit_idx * 4);
                                if(swizzle_data > 4) {
                                    swizzle_data = swizzle_data - 1;
                                }
                                swizzle_array[bit_idx] = swizzle_data;
                            }

                            // swizzle values are now in swizzle_array for this byte
                            // Each byte has three VREF registers
                            // loop the 3 vref types: low,mid,high
                            enum VREF_CODE_FIELD vrefType;
                            for(vrefType=EYE_LOW; vrefType<=EYE_HIGH; vrefType++)
                            {
                                LwU32 vrefBaseAddr = 0;
                                switch (vrefType) {
                                case EYE_LOW:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1;
                                    break;
                                case EYE_MID:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_MID_DFE_CTRL1;
                                    break;
                                case EYE_HIGH:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_UPPER_DFE_CTRL1;
                                    break;
                                default:
                                    FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_ILWALID_EYE);
                                }
                                LwU32 vrefAddr = unicast_rd_offset(vrefBaseAddr, partition, (subp*4 + byte_idx) * 4);

                                LwU32 pad_dfe_read = REG_RD32(BAR0, vrefAddr);

                                LwU8 pad_dfe[9];
                                pad_dfe[0] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD0, pad_dfe_read);
                                pad_dfe[1] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD1, pad_dfe_read);
                                pad_dfe[2] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD2, pad_dfe_read);
                                pad_dfe[3] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD3, pad_dfe_read);
                                LwU8 pad_dfe_dbi = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD4, pad_dfe_read);
                                pad_dfe[4] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD5, pad_dfe_read);
                                pad_dfe[5] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD6, pad_dfe_read);
                                pad_dfe[6] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD7, pad_dfe_read);
                                pad_dfe[7] = DRF_VAL(_PFB_FBPA_FBIO,_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1 ,_PAD8, pad_dfe_read);

                                // use the swizzle data to create logical dq array of codes
                                LwU8 logical_dq;
                                LwU8 final_dfe_array[8];
                                for(bit_idx = 0; bit_idx < 8 ; bit_idx = bit_idx +1) {
                                    logical_dq = swizzle_array[bit_idx];
                                    final_dfe_array[logical_dq] = pad_dfe[bit_idx];
                                }

                                LwU32 vrefDfeCtrl = 0;
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD0,final_dfe_array[0],vrefDfeCtrl);
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD1,final_dfe_array[1],vrefDfeCtrl);
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD2,final_dfe_array[2],vrefDfeCtrl);
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD3,final_dfe_array[3],vrefDfeCtrl);
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD4,pad_dfe_dbi,vrefDfeCtrl);
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD5,final_dfe_array[4],vrefDfeCtrl);
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD6,final_dfe_array[5],vrefDfeCtrl);
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD7,final_dfe_array[6],vrefDfeCtrl);
                                vrefDfeCtrl = FLD_SET_DRF_NUM(_PFB_FBPA_FBIO,_SUBP1BYTE2_VREF_LOWER_DFE_CTRL1,_PAD8,final_dfe_array[7],vrefDfeCtrl);

                                *(pBuffer++) = vrefDfeCtrl;

                            }  // for(vrefType=EYE_LOW; vrefType<=EYE_HIGH; vrefType++)
                        }  // for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                    }
                    else  // ! if ((subp == 1) || (isLowerHalfPartitionEnabled(partition)))
                    {
                        pBuffer += (BYTE_PER_SUBP * VREF_DFE_REGS_IN_BYTE);
                    }
                }  // for (subp=0; subp<SUBPS; subp++)
            }
            else  // ! if (isPartitionEnabled(partition))
            {
                pBuffer += (SUBPS * BYTE_PER_SUBP * VREF_DFE_REGS_IN_BYTE);
            }
        }  // for (partition=0; partition<MAX_PARTS; partition++)
    }
    else
    {
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
                        LwU8 byte_idx;    // inx loops through bytes w/ 3 entries per byte
                        for(byte_idx=0; byte_idx<BYTE_PER_SUBP; byte_idx++)
                        {
                            enum VREF_CODE_FIELD vrefType;
                            for(vrefType=EYE_LOW; vrefType<=EYE_HIGH; vrefType++)
                            {
                                LwU32 vrefBaseAddr = 0;
                                switch (vrefType) {
                                case EYE_LOW:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1;
                                    break;
                                case EYE_MID:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_MID_DFE_CTRL1;
                                    break;
                                case EYE_HIGH:
                                    vrefBaseAddr = LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_UPPER_DFE_CTRL1;
                                    break;
                                default:
                                    FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_ILWALID_EYE);
                                }
                                LwU32 vrefAddr = unicast_rd_offset(vrefBaseAddr, partition, (subp*4 + byte_idx) * 4);

                                REG_WR32(BAR0, vrefAddr, *(pBuffer++));
                            }
                        }
                    }
                    else
                    {
                        pBuffer += (BYTE_PER_SUBP * VREF_DFE_REGS_IN_BYTE);
                    }
                }
            }
            else
            {
                pBuffer += (SUBPS * BYTE_PER_SUBP * VREF_DFE_REGS_IN_BYTE);
            }
        }

    }
}


