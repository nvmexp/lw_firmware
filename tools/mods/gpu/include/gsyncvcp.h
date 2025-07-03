/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

//! GSync device definitions
#define GSYNC_MAGIC0_OPCODE     0xE0 //!< Opcode for magic register 0
#define GSYNC_MAGIC0_UNLOCK_SH   'N' //!< Magic0 SH value to unlock reg page
#define GSYNC_MAGIC0_UNLOCK_SL   'V' //!< Magic0 SL value to unlock reg page
#define GSYNC_MAGIC1_OPCODE     0xE1 //!< Opcode for magic register 1

#define GSYNC_MAGIC1_UNLOCK_NORMAL_SH 'V' //!< Magic1 SH value to unlock "normal" usage
#define GSYNC_MAGIC1_UNLOCK_NORMAL_SL 'R' //!< Magic1 SL value to unlock "normal" usage
#define GSYNC_MAGIC0_NORMAL_EN_SL       1 //!< Magci0 SL value when "normal" mode unlocked
#define GSYNC_MAGIC1_NORMAL_EN_SL       1 //!< Magic1 SL value when "normal" mode unlocked

#define GSYNC_MAGIC1_UNLOCK_CRC_SH   'C' //!< Magic1 SH value to unlock CRC page
#define GSYNC_MAGIC1_UNLOCK_CRC_SL   'R' //!< Magic1 SL value to unlock CRC page
#define GSYNC_MAGIC0_CRC_EN_SL         1 //!< Magic0 SL value when CRC mode unlocked
#define GSYNC_MAGIC1_CRC_EN_SL         3 //!< Magic1 SL value when CRC mode unlocked

#define GSYNC_CRC_STAT_OPCODE      0xE2 //!< Opcode for CRC status register
#define GSYNC_CRC_STAT_SL_WR_START    1 //!< SL write value to start CRCs
#define GSYNC_CRC_STAT_SL_WR_RESET    2 //!< SL write value to reset CRCs
#define GSYNC_CRC_STAT_SL_RD_IDLE     0 //!< SL read value when CRC capture idle
#define GSYNC_CRC_STAT_SL_RD_COLLECT  1 //!< SL read value when collecting CRCs
#define GSYNC_CRC_STAT_SL_RD_RESET    2 //!< SL read value when reseting CRCs

#define GSYNC_CRC_FRAMES_OPCODE    0xE3 //!< Opcode for CRC frames register

#define GSYNC_CRC_MODE_OPCODE      0xE4 //!< Opcode for CRC mode register
#define GSYNC_CRC_MODE_SU             0 //!< SL write value for SU (pre memory)
                                        //!< CRCs
#define GSYNC_CRC_MODE_SU_PPU         1 //!< SL write value for SU (pre memory)
                                        //!< and PPU (post memory) CRCs
#define GSYNC_CRC_PACKER_UNPACKER_CRC 1 //!< SL write value for Packer and Unpacker
                                        //!< CRCs (Gsync R3)

#define GSYNC_CRC_BUFF_OPCODE      0xE5 //!< Opcode for CRC buffer access

#define GSYNC_BYTES_PER_CRC           4 //!< Number of bytes per GSync CRC
#define GSYNC_SU_CRCS_PER_FRAME       4 //!< Number of GSync SU CRCs per frame
#define GSYNC_PPU_CRCS_PER_FRAME      4 //!< Number of GSync PPU CRCs per frame
#define GSYNC_CRC_SPOOLUP_FRAMES      2 //!< Number of frames of CRC spoolup
                                        //!< after CRC mode enable
#define GSYNC_PACKER_CRCS_PER_FRAME   4 //!< Number of Gsync packer CRCs per frame (R3)
#define GSYNC_UNPACKER_CRCS_PER_FRAME 4 //!< Number of Gsync unpacker CRCs per frame (R3)
#define GSYNC_CRC_MEMSIZE          4096 //!< Maximum size of GSync CRC memory in
                                        //!< bytes

//! VCP packet definitions
#define DP_WRITE_ADDR            0x6E //!< DP I2C write device address
#define DP_READ_ADDR             0x6F //!< DP I2C read device address
#define I2C_SLAVE_ADDR           0x37 //!< drop last bit of DP_READ/WRITE_ADDR.
                                      //! I2c Rd-Wr over TMDS
#define VCP_REG_ADDR             0x51 //!< DP VCP register address
#define VCP_PKT_OVERHEAD         0x03 //!< Number of bytes of packet overhead

#define DP_LENGTH_MASK           0x80 //!< Mask to add to DP packet length

//!Defines for VCP GetFeature packet
#define VCP_GET_FEATURE_OPCODE          0x01
#define VCP_GET_FEATURE_DATA_LENGTH     0x02
#define VCP_GET_FEATURE_PKT_LENGTH      \
    (VCP_GET_FEATURE_DATA_LENGTH + VCP_PKT_OVERHEAD)
#define VCP_GET_FEATURE_READ_PKT_LENGTH 0x0b //!< Length of data returned from
                                             //!< GetFeature
#define VCP_GET_FEATURE_RC_IDX          0x03 //!< Return code data index
#define VCP_GET_FEATURE_MH_IDX          0x06 //!< Max value high byte data index
#define VCP_GET_FEATURE_ML_IDX          0x07 //!< Max value low byte data index
#define VCP_GET_FEATURE_SH_IDX          0x08 //!< Lwr value high byte data index
#define VCP_GET_FEATURE_SL_IDX          0x09 //!< Lwr value low byte data index

//!Defines for VCP SetFeature packet
#define VCP_SET_FEATURE_OPCODE          0x03
#define VCP_SET_FEATURE_DATA_LENGTH     0x04
#define VCP_SET_FEATURE_PKT_LENGTH      \
    (VCP_SET_FEATURE_DATA_LENGTH + VCP_PKT_OVERHEAD)

//!Defines for VCP ReadTable packet
#define VCP_READ_TABLE_OPCODE           0xE2
#define VCP_READ_TABLE_DATA_LENGTH      0x04
#define VCP_READ_TABLE_PKT_LENGTH       \
    (VCP_READ_TABLE_DATA_LENGTH + VCP_PKT_OVERHEAD)
#define VCP_READ_TABLE_READ_PKT_LENGTH  0x26 //!< Length of data returned from
                                             //!< GetTable
#define VCP_READ_TABLE_LEN_IDX          0x01 //!< Length data index
#define VCP_READ_TABLE_OFF_H_IDX        0x03 //!< Offset high byte data index
#define VCP_READ_TABLE_OFF_L_IDX        0x04 //!< Offset low byte data index
#define VCP_READ_TABLE_DATA_START_IDX   0x05 //!< Table data start index

#define FACTORY_RESET_OPCODE            0x04 //!< Set feature opcode for factory
                                             //!< reset
#define INPUT_SEL_OPCODE                0xF7 //!< Set feature opcode for setting
                                             //!< input port
#define GSYNC_DIE_TEMP_OPCODE           0xF9 //!< Set feature opcode for reading
                                             //!< Gsync silicon die temperature
#define GSYNC_BOARD_ID                  0xFA //!< Set feature opcode for reading
                                             //!< board ID
#define MONITOR_MODE_OPCODE             0xE2 //!< Set feature opcode for reading
                                             //!< current monitor mode
