/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include "manifest.h"
#define TRUE            0xAA
#define FALSE           0x55
const Pkc_Verification_Manifest_Mem_Overlay manifest = {
.Version                    = 0x07,
.Ucode_Id                   = 0x04,
.Ucode_Version              = 0x01,
.Is_Relaxed_Version_check   = TRUE,
.Engine_Id_Mask             = 0x4f5b161d,
.Code_Size_In_256_Bytes     = 0x0012,
.Data_Size_In_256_Bytes     = 0x0005,
.fmc_hash_pad_info_bit_mask = 0x00000000,
.Cbc_Iv                     = {0x27d0ef69, 0xc4bbca8a, 0x3437c8cb, 0x8cb8fc04},
.Digest                     = {0x976f1e8d, 0x44ae096d, 0xd341588c, 0x4b9b7b07, 0x5678bc85, 0xa921c19b, 0x9fbb1e3d, 0x1682c882},
.Secret_Mask                = {0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF},
.Debug_Control              = {
                                0x1ffff,  // Debug_Ctrl
                                0x0,  // Debug_Ctrl_lock
                              },
.Io_Pmp_Mode                = 0xffffffffffffffff,
.Is_Dec_Fuse_Keys           = FALSE,
.Mspm                       = {0xf, 0x01},
.number_of_valid_pairs      = 0x0,
.Device_Map                 = {0x8BBBBBBB, 0xBBB9BBBB, 0xBBBBBBBB, 0xB},

.Core_Pmp                   = {{0x1F, 0x1B, 0x1B, 0x1B, 0x0F, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                               {0x000000000004FFFF, 0x000000000006FFFF, 0x000000000048FFFF, 0x00000000004AFFFF, 0x7FFFFFFFFFFFFFFF, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
                                0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
                                0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
                                0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000}},

.Io_Pmp                     = {{0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                               {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
                               {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

.Register_Pair              = {},
};

