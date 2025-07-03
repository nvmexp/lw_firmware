/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ELF_H
#define ELF_H

#include "bootloader.h"
#include "elf_defs.h"
#include "mpu.h"

/*!
 * @file   elf.h
 * @brief  ELF loader.
 */

// Supported load-bases for ELF files.
typedef enum LW_ELF_LOAD_BASE
{
    LW_ELF_LOAD_BASE_INDEX_RESERVED_RW,
    LW_ELF_LOAD_BASE_INDEX_IN_PLACE_ELF,
    LW_ELF_LOAD_BASE_INDEX_ODP_COW,
    LW_ELF_LOAD_BASE_INDEX_COUNT,
} LW_ELF_LOAD_BASE;

typedef struct LW_ELF_STATE
{
    const LwU8 *pElf;
    LwUPtr elfSize;

    const Elf_Ehdr *pEhdr;
    const Elf_Phdr *pPhdrs;
    const Elf_Shdr *pShdrs;

    // Section string table section header.
    const Elf_Shdr *pShstrtabShdr;

    // Section name string table.
    const char *pShstrtab;
    LwUPtr shstrtabSize;

    // MPU configuration.
    const LW_RISCV_MPU_INFO *pMpuInfo;

    // MPU-aligned Loader physical address base.
    LwUPtr loaderMpuPaBase;
    // MPU-aligned Loader size.
    LwUPtr loaderMpuSize;
    // MPU-aligned Stack physical address base.
    LwUPtr stackMpuPaBase;
    // MPU-aligned Stack size.
    LwUPtr stackMpuSize;
    // MPU-aligned Bootargs virtual address base.
    LwUPtr bootargsMpuVaBase;
    // MPU-aligned Bootargs physical address base.
    LwUPtr bootargsMpuPaBase;
    // MPU-aligned Bootargs size.
    LwUPtr bootargsMpuSize;
    // Target entrypoint.
    LwUPtr entryPoint;

    // First address of the loader.
    LwUPtr loaderBase;
    // Size of the loader (including ELF file) in bytes.
    LwUPtr loaderSize;
    // Start of reserved RW space for loading.
    LwUPtr reservedBase;
    // Size of reserved RW space in bytes.
    LwUPtr reservedSize;

    // Lowest VA in use by target program.
    LwUPtr lowestVa;
    // Highest VA in use by target program.
    LwUPtr highestVa;

    // Load base array for remapping physical addresses.
    LwUPtr loadBases[LW_ELF_LOAD_BASE_INDEX_COUNT];
    LwU32  loadBaseCount;

    // WPR/GSC ID, saved from BCR_DMACFG_SEC.
    LwU32 wprId;

#if ELF_LOAD_USE_DMA
    // DMA index to be used for copying memory.
    LwU8   dmaIdx;

    // Aperture base for addrs being DMA-d.
    LwUPtr apertureBase;

    // Aperture size for addrs being DMA-d.
    LwUPtr apertureSize;
#endif // ELF_LOAD_USE_DMA
} LW_ELF_STATE,
*PLW_ELF_STATE;

/*!
 * @brief Validates an ELF and sets up the state necessary for further use.
 *
 * @param[out]  pState          State structure.
 * @param[in]   pElf            Pointer to the ELF file in memory.
 * @param[in]   elfSize         Size in bytes of the ELF file.
 * @param[in]   pParams         Pointer to the boot arguments.
 * @param[in]   paramSize       Size of the boot arguments.
 * @param[in]   pLoaderBase     Base address of the loader.
 * @param[in]   loaderSize      Size in bytes of the loader image (this program + params + ELF + padding).
 * @param[in]   pReservedBase   Base address of the reserved RW space for loading.
 * @param[in]   reservedSize    Size in bytes of the reserved RW space for loading.
 * @param[in]   wprId           WPR ID of region we booted from.
 *
 * @return Zero on failure, one on success.
 */
int elfBegin(LW_ELF_STATE *pState, const void *pElf, LwUPtr elfSize,
             const LW_RISCV_BOOTLDR_PARAMS *pParams, LwUPtr paramSize,
             const void *pLoaderBase, LwUPtr loaderSize,
             const void *pReservedBase, LwUPtr reservedSize, LwU32 wprId);

/*!
 * @brief Loads a prepared ELF file into memory and exelwtes it.
 *
 * @param[inout] pState         State structure.
 */
void elfLoad(LW_ELF_STATE *pState)
    __attribute__((noreturn));

/*!
 * @brief Sets up identity entries in the MPU to work around WPR-ID requirements (see COREUCODES-531).
 *
 * @param[in] pLoaderBase   Base address of the bootloader in memory.
 * @param[in] loaderExtent  Total footprint of loader image and pre-reserved load region.
 * @param[in] wprId         WPR ID of region we booted from.
 */
void elfSetIdentityRegions(const void *pLoaderBase, LwUPtr loaderExtent, LwU32 wprId);

#endif // ELF_H
