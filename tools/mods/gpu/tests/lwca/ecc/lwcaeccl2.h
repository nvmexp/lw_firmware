/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef LWDAECCL2_H_INCLUDED
#define LWDAECCL2_H_INCLUDED

//
// When we disable checkbits using CHECKBIT_WR_SUPPRESS_DISABLED communication between
// the host and LWCA kernel becomes much more diffilwlt.
//
// We must ensure that the bytes written to memory surface results in the same L2 checkbits being generated,
// otherwise ECC errors will be detected.
//
// Inspection of the ECC error syndrome code at
// //hw/hw/lwgpu_ltc/vmod/ecc/gvlit1/LW_ECC_GENERATE_SYNDROME_64x8.v reveals that for the lower 32 bits, as long
// as we flip the same offset bit of each 4-bit nibble the syndrome bit will not change. In other words, any
// dword with 8 of the same hex values (e.g. 0x44444444) will generate the same syndrome as long as the checkbits
// don't change.
//
// Note that this doesn't hold true for bits [63:32]. If we use the second dword for communication errors will
// be detected.
//
// Example syndrome bit callwlation from code. c is checksum, d is data:
//
// s[0] = c[0] ^ d[0] ^ d[6] ^ d[7] ^ d[12] ^ d[13] ^ d[15] ^ d[16]
// ^ d[19] ^ d[20] ^ d[26] ^ d[29] ^ d[31] ^ d[34] ^ d[35] ^ d[38]
// ^ d[43] ^ d[45] ^ d[47] ^ d[48] ^ d[50] ^ d[51] ^ d[56] ^ d[60]
// ^ d[61] ^ d[62] ^ d[63] ;
//
#define SM_INIT                 0x00000000
#define HOST_PROCEED_64x8       0xAAAAAAAA
#define SM_READY_64x8           0xBBBBBBBB
#define SM_DISABLE_CHKBITS_64x8 0xCCCCCCCC
#define SM_ENABLE_CHKBITS_64x8  0xDDDDDDDD
#define SM_RESET_ERRS_64x8      0xEEEEEEEE
#define SM_DONE_64x8            0xFFFFFFFF

// On Turing+ L2 ECC was changed to use the 256x16 syndrome callwlation which doesnt
// have a simple pattern that will not cause syndrome to change.  Instead values were
// found that hash to identical syndrome values by writing a program to generate syndrome
// using the new encoding scheme.  The following values all callwlate to a syndrome of 0
#define HOST_PROCEED_256x16       0x00020016
#define SM_READY_256x16           0x0004002c
#define SM_DISABLE_CHKBITS_256x16 0x0006003a
#define SM_ENABLE_CHKBITS_256x16  0x00080058
#define SM_RESET_ERRS_256x16      0x000a004e
#define SM_DONE_256x16            0x000c0074

#endif // LWDAECCL2_H_INCLUDED
