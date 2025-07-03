/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwriscv/manifest_t23x.h>

const PKC_VERIFICATION_MANIFEST manifest = {
.version                    = 0x07,
.ucodeId                    = 0x04,
.ucodeVersion               = 0x07,
.bRelaxedVersionCheck       = TRUE,
.engineIdMask               = 0x4f5b161d,
.itcmSizeIn256Bytes         = 0x0012,
.dtcmSizeIn256Bytes         = 0x0005,
.fmcHashPadInfoBitMask      = 0x00000000,
.CbcIv                      = {0x27d0ef69, 0xc4bbca8a, 0x3437c8cb, 0x8cb8fc04},
.digest                     = {0x976f1e8d, 0x44ae096d, 0xd341588c, 0x4b9b7b07, 0x5678bc85, 0xa921c19b, 0x9fbb1e3d, 0x1682c882},
.secretMask                 = {0x0000000000000000LLU, 0xFFFFFFFFFFFFFFFFLLU},
.debugAccessControl         = {
                                0x1ffff,  // Debug_Ctrl
                                0x0,  // Debug_Ctrl_lock
                              },
.ioPmpMode                  = 0xffffffffffffffff,
.bDICE           = FALSE,
.bKDF = FALSE,
.mspm                       = {0x0a, 0x01},
.deviceMap.deviceMap        = {0x8BBBBBBB, 0xBBB9BBBB, 0xBBBBBBBB, 0xB},
.corePmp                    = {
    .pmpcfg                 =  {0x1F, 0x1B, 0x1B, 0x1B, 0x0F, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .pmpaddr                =  {0x000000000004FFFF, 0x000000000006FFFF, 0x000000000048FFFF, 0x00000000004AFFFF, 0x7FFFFFFFFFFFFFFF, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
                                0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
                                0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
                                0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000}},

.ioPmp                      = {
    .iopmpcfg               =  {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    .iopmpaddrlo            =  {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    .iopmpaddrhi            =  {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},

.numberOfValidPairs = 0x0,
.registerPair               = {
    .addr                   =  { },
    .andMask                =  { },
    .orMask                 =  { },
                                },
};

