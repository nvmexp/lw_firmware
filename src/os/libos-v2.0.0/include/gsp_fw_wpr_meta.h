/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

// TODO: This file is is not libos specific, so it really does not belong under
//       uproc/os/libos-v2.0.0.  It is used by booter, bootloader, libos, GSP-RM,
//       and kernel RM.  An appropriate location might be uproc/common/inc, but
//       that directory is not lwrrently synced by DVS for driver builds.

#pragma once

#ifndef GSP_FW_WPR_META_H_
#define GSP_FW_WPR_META_H_

/*!
 * GSP firmware WPR metadata
 *
 * Initialized by CPU-RM and DMA'd to FB, at the end of what will be WPR2.
 * Verified, and locked in WPR2 by Booter.
 *
 * Firmware scrubs the last 256mb of FB, no memory outside of this region
 * may be used until the FW RM has scrubbed the remainder of memory.
 *
 *   ---------------------------- <- fbSize (end of FB, 1M aligned)
 *   | VGA WORKSPACE            |
 *   ---------------------------- <- vbiosReservedOffset  (64K? aligned)
 *   | (potential align. gap)   |
 *   ---------------------------- <- gspFwWprEnd (128K aligned)
 *   | FRTS data                |    (frtsSize is 0 on GA100)
 *   | ------------------------ | <- frtsOffset
 *   | BOOT BIN (e.g. SK + BL)  |
 *   ---------------------------- <- bootBinOffset
 *   | GSP FW ELF               |
 *   ---------------------------- <- gspFwOffset
 *   | GSP FW (WPR) HEAP        |
 *   ---------------------------- <- gspFwHeapOffset
 *   | Booter-placed metadata   |
 *   | (struct GspFwWprMeta)    |
 *   ---------------------------- <- gspFwWprStart (128K aligned)
 *   | GSP FW (non-WPR) HEAP    |
 *   ---------------------------- <- nonWprHeapOffset, gspFwRsvdStart
 *                                   (GSP_CARVEOUT_SIZE bytes from end of FB)
 */
typedef struct
{
    // Magic
    // Booter to use as sanity check (i.e. got the right struct from CPU)
    // BL to use for verification (i.e. Booter locked it in WPR2)
    LwU64 magic; // = 0xdc3aae21371a60b3;

    // Revision number of Booter-BL-Sequencer handoff interface
    // Bumped up when we change this interface so it is not backward compatible.
    // Bumped up when we revoke GSP-RM ucode
    LwU64 revision; // = 1;

    // ---- Members regarding data in SYSMEM ----------------------------
    // Consumed by Booter for DMA

    LwU64 sysmemAddrOfRadix3Elf;
    LwU64 sizeOfRadix3Elf;

    LwU64 sysmemAddrOfBootloader;
    LwU64 sizeOfBootloader;

    // Offsets inside bootloader image needed by Booter
    LwU64 bootloaderCodeOffset;
    LwU64 bootloaderDataOffset;
    LwU64 bootloaderManifestOffset;

    LwU64 sysmemAddrOfSignature;
    LwU64 sizeOfSignature;

    // ---- Members describing FB layout --------------------------------
    // Booter to verify all start/offset fields monotonically increasing

    // Booter to verify within VBIOS-scrubbed 256MB
    LwU64 gspFwRsvdStart;

    // Booter to verify within gspFwRsvdStart and gspFwWprStart.
    LwU64 nonWprHeapOffset;
    LwU64 nonWprHeapSize;

    LwU64 gspFwWprStart;

    // GSP-RM to use to setup heap.
    LwU64 gspFwHeapOffset;
    LwU64 gspFwHeapSize;

    // Booter to verify enough space to fit ELF
    // Booter to DMA ELF
    // BL to use to find ELF for jump
    LwU64 gspFwOffset;
    // Size is sizeOfRadix3Elf above.

    // Booter to verify enough space to fit BL
    // Booter to DMA BL
    LwU64 bootBinOffset;
    // Size is sizeOfBootloader above.

    // Booter to verify agrees with WPR2-lo and hi registers from FRTS
    LwU64 frtsOffset;
    LwU64 frtsSize;

    LwU64 gspFwWprEnd;

    // GSP-RM to use for fbRegionInfo?
    LwU64 fbSize;

    // ---- Other members -----------------------------------------------

    // GSP-RM to use for fbRegionInfo?
    LwU64 vgaWorkspaceOffset;
    LwU64 vgaWorkspaceSize;

    // Boot count.  Used to determine whether to load the firmware image.
    LwU64 bootCount;

    // TODO: the partitionRpc* fields below do not really belong in this
    //       structure. The values are patched in by the partition bootstrapper
    //       when GSP-RM is booted in a partition, and this structure was a
    //       colwenient place for the bootstrapper to access them. These should
    //       be moved to a different comm. mechanism between the bootstrapper
    //       and the GSP-RM tasks.

    // Shared partition RPC memory (physical address)
    LwU64 partitionRpcAddr;

    // Offsets relative to partitionRpcAddr
    LwU16 partitionRpcRequestOffset;
    LwU16 partitionRpcReplyOffset;

    // Pad structure to exactly 256 bytes.  Can replace padding with additional
    // fields without incrementing revision.  Padding initialized to 0.
    LwU32 padding[7];

    // Booter to clear, lock in WPR2 (PL3 only), DMA/authenticate/etc.,
    //        write this field, then sub-WPR to allow GSP
    //        MUST NOT allow GSP until authentication is done
    // BL to use for verification (i.e. Booter says OK to boot)
    LwU64 verified;  // 0x0 -> ulwerified, 0xa0a0a0a0a0a0a0a0 -> verified

} GspFwWprMeta;

#define GSP_FW_WPR_META_VERIFIED  0xa0a0a0a0a0a0a0a0ULL
#define GSP_FW_WPR_META_REVISION  1
#define GSP_FW_WPR_META_MAGIC     0xdc3aae21371a60b3ULL

#endif // GSP_FW_WPR_META_H_
