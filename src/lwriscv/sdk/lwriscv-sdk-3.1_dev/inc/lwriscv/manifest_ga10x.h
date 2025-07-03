/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_MANIFEST_GA10X_H
#define LWRISCV_MANIFEST_GA10X_H

#include "lwriscv/manifest.h"

//
// Synced on 2020-01-30 to
// https://confluence.lwpu.com/display/Peregrine/GA10x+LWRISCV+BootROM+Design+Spec
//

typedef struct
{
    // 384 bytes of RSA signature is prepended to this structure by siggen
    uint8_t version;                    // [MISC] version of the manifest; it needs to be 0x1 in GA10x/T234
    uint8_t ucodeId;                    // [ucode revocation] ucode ID
    uint8_t ucodeVersion;               // [ucode revocation] ucode version
    uint8_t bRelaxedVersionCheck;       // [ucode revocation] if true, allows a greater-or-equal-to check on ucodeVersion (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    uint32_t engineIdMask;              // [ucode revocation] engine ID mask
    uint16_t itcmSizeIn256Bytes;        // [FMC related] FMC's code starts at offset 0 inside IMEM; the sizes need to be a multiple of 256 bytes; the blobs need to be 0-padded.
    uint16_t dtcmSizeIn256Bytes;        // [FMC related] FMC's data starts at offset 0 inside DMEM; the sizes need to be a multiple of 256 bytes; the blobs need to be 0-padded.
    uint32_t fmcHashPadInfoBitMask;     // [FMC related] in GA10x the LSb indicates whether the 64-bit PDI is prepended to FMC code/data when callwlating the digest; all other bits must be 0s
    uint32_t CbcIv[4];                  // [FMC related] initial vector for AES128-CBC decryption of both the manifest and FMC's code and data
    uint32_t digest[8];                 // [FMC related] hash of the FMC's code and data

    MANIFEST_SECRET secretMask;         // [exelwtion environment] SCP HW secret mask
    MANIFEST_DEBUG debugAccessControl;  // [exelwtion environment] ICD and debug control
    uint64_t ioPmpMode;                    // [exelwtion environment] IO-PMP configuration, part I
    uint8_t reservedByte;                  // [reserved space] all 0s, freed up due to deprecated fields
    uint8_t bDecFuseKeys;                  // [DECFUSEKEY flag] if true, DECFUSEKEY subroutine is exelwted (false is defined as 0x55; true is defined as 0xAA; other values will be reported as an error)
    MANIFEST_MSPM mspm;                 // [exelwtion environment] max PRIV level and SCP secure indicator
    uint8_t numberOfValidPairs;            // [exelwtion environment] first numberOfValidPairs of registerPair entries get configured by BR
    uint8_t reservedBytes0[11];            // [reserved space] all 0s; 1 byte for bHashSaving, 4 bytes for MAX_DELAY_USEC in the furture
    uint32_t kdfConstant[8];               // [KDF related] 256-bit constant used in KDF
    MANIFEST_DEVICEMAP deviceMap;       // [exelwtion environment] device map
    MANIFEST_COREPMP corePmp;           // [exelwtion environment] core-PMP
    MANIFEST_IOPMP ioPmp;               // [exelwtion environment] IO-PMP configuration, part II
    MANIFEST_REGISTERPAIR registerPair; // [exelwtion environment] register address/data pairs
    uint8_t reservedBytesAlignTo2k[320];   // [reserved space] all 0s;
} PKC_VERIFICATION_MANIFEST;
_Static_assert(sizeof(PKC_VERIFICATION_MANIFEST) == MANIFEST_SIZE, "Incorrect manifest size.");

#endif // LWRISCV_MANIFEST_GA10X_H
