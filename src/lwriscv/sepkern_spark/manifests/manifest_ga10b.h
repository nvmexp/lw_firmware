/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef __MANIFEST_GH100_H_
#define __MANIFEST_GH100_H_

#include "manifest.h"

//
// Synced on 2020-08-11 to
// https://confluence.lwpu.com/display/Peregrine/T234+GA10b+LWRISCV+BootROM+Design+Spec#T234GA10bLWRISCVBootROMDesignSpec-PKCBOOTParameters-Manifest
//

typedef struct
{
    // 384 bytes of RSA signature is prepended to this structure by siggen
    LwU8 version;                    // [MISC] version of the manifest; it needs to be 0x1 in GA10x/T234
    LwU8 ucodeId;                    // [ucode revocation] ucode ID
    LwU8 ucodeVersion;               // [ucode revocation] ucode version
    LwU8 bRelaxedVersionCheck;       // [ucode revocation] if true, allows a greater-or-equal-to check on ucodeVersion (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    LwU32 engineIdMask;              // [ucode revocation] engine ID mask
    LwU16 itcmSizeIn256Bytes;        // [FMC related] FMC's code starts at offset 0 inside IMEM; the sizes need to be a multiple of 256 bytes; the blobs need to be 0-padded.
    LwU16 dtcmSizeIn256Bytes;        // [FMC related] FMC's data starts at offset 0 inside DMEM; the sizes need to be a multiple of 256 bytes; the blobs need to be 0-padded.
    LwU32 fmcHashPadInfoBitMask;     // [FMC related] in GA10x the LSb indicates whether the 64-bit PDI is prepended to FMC code/data when callwlating the digest; all other bits must be 0s
    LwU32 CbcIv[4];                  // [FMC related] initial vector for AES128-CBC decryption of both the manifest and FMC's code and data
    LwU32 digest[8];                 // [FMC related] hash of the FMC's code and data

    MANIFEST_SECRET secretMask;         // [exelwtion environment] SCP HW secret mask
    MANIFEST_DEBUG debugAccessControl;  // [exelwtion environment] ICD and debug control
    LwU64 ioPmpMode;                    // [exelwtion environment] IO-PMP configuration, part I
    LwU8 bDICE;                         // [DICE flag] if true, DECFUSEKEY and DICE subroutine is exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    LwU8 bKDF;                          // [KDF flag] if true,  DECFUSEKEY and KDF  subroutine is exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    MANIFEST_MSPM mspm;                 // [exelwtion environment] max PRIV level and SCP secure indicator
    LwU8 numberOfValidPairs;            // [exelwtion environment] first numberOfValidPairs of registerPair entries get configured by BR
    LwU8 reservedBytes0[11];            // [reserved space] all 0s; 1 byte for bHashSaving, 4 bytes for MAX_DELAY_USEC in the furture
    LwU32 kdfConstant[8];               // [KDF related] 256-bit constant used in KDF
    MANIFEST_DEIVCEMAP deviceMap;       // [exelwtion environment] device map
    MANIFEST_COREPMP corePmp;           // [exelwtion environment] core-PMP
    MANIFEST_IOPMP ioPmp;               // [exelwtion environment] IO-PMP configuration, part II
    MANIFEST_REGISTERPAIR registerPair; // [exelwtion environment] register address/data pairs
    LwU8 reservedBytesAlignTo2k[320];   // [reserved space] all 0s;
} PKC_VERIFICATION_MANIFEST;
ct_assert(sizeof(PKC_VERIFICATION_MANIFEST) == MANIFEST_SIZE);

#endif // __MANIFEST_GH100_H_
