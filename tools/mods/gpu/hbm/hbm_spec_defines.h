/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_HBM_SPEC_DEFINES_H
#define INCLUDED_HBM_SPEC_DEFINES_H

// Channel Selection Definition
// 'CHAN' infix corresponds to bits WIR[11:8]
// Source: HBM Draft Specification Rev. 2.02 (Table 78)
#define HBM_WIR_CHAN_A                          0
#define HBM_WIR_CHAN_B                          1
#define HBM_WIR_CHAN_C                          2
#define HBM_WIR_CHAN_D                          3
#define HBM_WIR_CHAN_E                          4
#define HBM_WIR_CHAN_F                          5
#define HBM_WIR_CHAN_G                          6
#define HBM_WIR_CHAN_H                          7
#define HBM_WIR_CHAN_ALL                        0xF

// Instruction Register Encodings
// 'INSTR' infix corresponds to bits WIR[7:0]
// Source: HBM Draft Specification Rev. 2.02 (Table 79)
#define HBM_WIR_INSTR_DEVICE_ID                 0xE
#define HBM_WDR_LENGTH_DEVICE_ID                82

#define HBM_WIR_INSTR_SOFT_LANE_REPAIR          0x12
#define HBM_WIR_INSTR_HARD_LANE_REPAIR          0x13
#define HBM_WDR_LENGTH_LANE_REPAIR              72

// Device ID Wrapper Data Bit Ranges
#define HBM_DEV_ID_BITS_MODEL_PART_NUMBER         6:0
#define HBM_DEV_ID_BITS_HBM_STACK_HEIGHT          7:7
#define HBM_DEV_ID_BITS_CHANNEL_AVAILABLE        15:8
#define HBM_DEV_ID_BITS_ADDRESSING_MODE         17:16
#define HBM_DEV_ID_BITS_SERIAL_NUMBER           51:18
#define HBM_DEV_ID_BITS_MANUFACTURING_WEEK      59:52
#define HBM_DEV_ID_BITS_MANUFACTURING_YEAR      67:60
#define HBM_DEV_ID_BITS_MANUFACTURING_LOCATION  71:68
#define HBM_DEV_ID_BITS_MANUFACTURER_ID         75:72
#define HBM_DEV_ID_BITS_DENSITY                 79:76
#define HBM_DEV_ID_BITS_ECC                     80:80
#define HBM_DEV_ID_BITS_GEN2_TEST               81:81

// Device ID Wrapper Stack Heights
#define HBM_DEV_ID_STACK_HEIGHT_2_OR_4          0
#define HBM_DEV_ID_STACK_HEIGHT_8               1

// Device ID Wrapper Addressing Modes
#define HBM_DEV_ID_ADDR_MODE_UNKNOWN            0
#define HBM_DEV_ID_ADDR_MODE_PSEUDO             1
#define HBM_DEV_ID_ADDR_MODE_LEGACY             2

// Some helper defines
#define HBM_WIR_DEVICE_ID_NUM_DWORDS            ((HBM_WDR_LENGTH_DEVICE_ID + 31) / 32)

// Channel defines
#define HBM_MAX_NUM_CHANNELS                    8
#define HBM_BANKS_PER_CHANNEL                   16

// DWORD Remapping
#define HBM_REMAP_DWORD_BYTE_ENCODING_SIZE           4      //!< Size in bits of a single DWORD byte encoding
#define HBM_REMAP_DWORD_BYTE_PAIR_ENCODING_SIZE      8      //!< Size in bits of a DWORD byte pair encoding
#define HBM_REMAP_DWORD_BYTE_REPAIR_IN_OTHER_BYTE    0xE
#define HBM_REMAP_DWORD_BYTE_NO_REPAIR               0xF
#define HBM_REMAP_DWORD_BYTE_PAIR_NO_REPAIR          0xFF
#define HBM_REMAP_DWORD_NO_REPAIR                    0xFFFF

#endif // INCLUDED_HBM_SPEC_DEFINES_H
