/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    symbols.c
 * @brief   Symbol resolver
 *
 * If enabled / initialized, should be able to resolve pointers.
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include <limits.h>
#include <stdarg.h>

/* ------------------------ LW Includes ------------------------------------ */
#include <flcnretval.h>
#include <lwtypes.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
#include <lwrtos.h>
#include <shared.h>
#include <sections.h>

/* ------------------------ Module Includes -------------------------------- */
#include <lwriscv/print.h>
#include <riscv_csr.h>
#include <lwriscv-mpu.h>
#include <drivers/drivers.h>
#include <drivers/mpu.h>
#include <drivers/vm.h>

#include "elf_defs.h"

#define RISCV_ALIGNMENT_ASSUME(ptr) __builtin_assume_aligned((void*)(ptr), 8)

//
// Configuration, should be passed via symbolInit()
//
static LwBool bInitialized = LW_FALSE;
static LwUPtr elfBase = 0;
static LwU64 elfSize = 0;

//
// Pointers to (pieces) of our ELF image
//
static const Elf_Ehdr *pEhdr;               // elf header
static const Elf_Shdr *pShdr;               // section header
static LwU64          shdrCount;            // number of sections
static const char     *pSecStrTable;        // string table section
static const char     *pStrTable = NULL;    // string table section
static const Elf_Sym  *pSymTable;           // symbol table
static LwU64          symCount;             // number of symbols

/**
 * @brief symbolInit    Initialize symbol resolver
 * @param elfBasePhys   (Physical) address where ELF image is stored
 * @param elfSize       Size of ELF image
 * @param wprId         Write-Protected Region id
 */
void symbolInit(LwU64 elfBasePhys, LwU64 elfFileSize, LwU64 wprId)
{
    LwU64 baseAligned = LW_ALIGN_DOWN64(elfBasePhys, LW_RISCV_CSR_MPU_PAGE_SIZE);
    LwU64 baseOffset  = elfBasePhys - baseAligned;
    LwUPtr baseVa;
    LwU64 idx;

    elfSize = elfFileSize; // real elf size (not mapping)

    // Mapping size
    elfSize = LW_ALIGN_UP64(baseOffset + elfSize, LW_RISCV_CSR_MPU_PAGE_SIZE);

    // Allocate VA
    baseVa = vmAllocateVaSpace(elfSize / LW_RISCV_CSR_MPU_PAGE_SIZE);

    // MAP
    elfBase = (LwUPtr)mpuKernelEntryCreate(baseAligned, baseVa, elfSize,
                                           DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                                           DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 1) |
                                           DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, wprId));
    if (elfBase == 0U)
    {
        dbgPrintf(LEVEL_ERROR, "Failed to map ELF image.\n");
        return;
    }

    elfBase = baseVa + baseOffset;

    dbgPrintf(LEVEL_DEBUG, "ELF mapped at 0x%llx\n", elfBase);

    //
    // WARNING
    // We trust ELF image under assumption that it is stored in memory that
    // can't be written by "untrusted" parties.
    // That should be always true for LS/HS.
    //

    // Pre-process elf
    pEhdr = (const Elf_Ehdr*)RISCV_ALIGNMENT_ASSUME(elfBase);
    pShdr = (const Elf_Shdr*)RISCV_ALIGNMENT_ASSUME(elfBase + pEhdr->e_shoff);
    shdrCount = pEhdr->e_shnum;

    // string table
    pSecStrTable = (const char*)(elfBase + pShdr[pEhdr->e_shstrndx].sh_offset);

    // symbol table
    pSymTable = NULL;
    symCount = 0;

    for (idx=0; idx < shdrCount; ++idx)
    {
        const Elf_Shdr *pSec = &pShdr[idx];

        if (pSec->sh_type == SHT_SYMTAB)
        {
            pSymTable = (const Elf_Sym *)RISCV_ALIGNMENT_ASSUME(elfBase + pSec->sh_offset);

            if (pSec->sh_entsize > 0)
            {
                symCount = pSec->sh_size / pSec->sh_entsize;
            }
        }

        if ((pSec->sh_type == SHT_STRTAB) && (pStrTable == NULL))
        {
            pStrTable = (const char *)(elfBase + pSec->sh_offset);
        }
    }
    dbgPrintf(LEVEL_DEBUG, "Found %lld symbols.\n", symCount);

    bInitialized = LW_TRUE;
}

/**
 * @brief symResolveSection Resolve section name/offset by address
 * @param addr              Address
 * @param ppName            Section name
 * @param pOffset           Offset within section
 * @return                  FLCN_OK on success
 */
FLCN_STATUS symResolveSection(LwUPtr addr, const char **ppName, LwU64 *pOffset)
{
    unsigned i;

    if (fbhubGatedGet())
    {
        // The symbols are mapped in FB, so don't try to access them if in MSCG
        return FLCN_ERR_OBJECT_NOT_FOUND;
    }

    if (ppName != NULL)
    {
        *ppName = ".unknown";
    }

    if (pOffset != NULL)
    {
        *pOffset = 0;
    }

    for (i=0; i < shdrCount; ++i)
    {
        const Elf_Shdr *pSec = &pShdr[i];
        LwU64 val = pSec->sh_addr;
        LwU64 size = pSec->sh_size;

        // We care only about sections that have data/code
        if ((pSec->sh_type != SHT_PROGBITS) && (pSec->sh_type != SHT_NOBITS))
        {
            continue;
        }

        // Check if address hits in the range
        if ((addr >= val) && (addr < (val + size)))
        {
            if (pOffset != NULL)
            {
                *pOffset = addr - val;
            }

            // Lookup name
            if (ppName != NULL)
            {
                *ppName = &pSecStrTable[pSec->sh_name];
            }
            return FLCN_OK;
        }
    }
    return FLCN_ERR_OBJECT_NOT_FOUND;
}

//
// Helper function to lookup "objects" inside ELF
//
// WARNING: This is *expensive* function. Especially for large images.
//
static FLCN_STATUS symResolveType(LwU32 type,
                                  LwUPtr addr,
                                  const char **ppName,
                                  LwU64 *pOffset)
{
    unsigned i;

    if (fbhubGatedGet())
    {
        // The symbols are mapped in FB, so don't try to access them if in MSCG
        return FLCN_ERR_OBJECT_NOT_FOUND;
    }

    if (ppName != NULL)
    {
        *ppName = "Unknown";
    }

    if (pOffset != NULL)
    {
        *pOffset = 0;
    }

    for (i=0; i < symCount; ++i)
    {
        const Elf_Sym *pSym = &pSymTable[i];
        LwU64 val;
        LwU64 size;

        if (ELF_ST_TYPE(pSym->st_info) != type)
        {
            continue;
        }

        val = pSym->st_value;
        size = pSym->st_size;

        // Skip empty objects
        if (size == 0)
        {
            continue;
        }

        // Does address overlap with symbol?
        if ((addr >= val) && (addr < (val + size)))
        {
            if (pOffset != NULL)
            {
                *pOffset = addr - val;
            }

            if (ppName != NULL)
            {
                *ppName = &pStrTable[pSym->st_name];
            }
            return FLCN_OK;
        }
    }
    return FLCN_ERR_OBJECT_NOT_FOUND;
}

/**
 * @brief symResolveFunction    Resolve function name/offset
 * @param addr                  Address (code pointer)
 * @param ppName                Function name
 * @param pOffset               Offset inside function
 * @return                      FLCN_OK on success
 */
FLCN_STATUS symResolveFunction(LwUPtr addr, const char **ppName, LwU64 *pOffset)
{
    return symResolveType(STT_FUNC, addr, ppName, pOffset);
}

/**
 * @brief symResolveData    Resolve data object name/offset
 * @param addr              Address of object (data pointer)
 * @param ppName            Object name
 * @param pOffset           Offset inside object (for arrays etc.)
 * @return                  FLCN_OK on success
 */
FLCN_STATUS symResolveData(LwUPtr addr, const char **ppName, LwU64 *pOffset)
{
    return symResolveType(STT_OBJECT, addr, ppName, pOffset);
}

/**
 * @brief symResolveFunctionSmart   Resolve function name/offset with fallback
 *
 * Works like @sa symResolveFunction, but if no function is found, it tries
 * to lookup section address belongs to.
 */
FLCN_STATUS symResolveFunctionSmart(LwU64 addr,
                                    const char **ppName,
                                    LwU64 *pOffset)
{
    FLCN_STATUS ret;

    ret = symResolveType(STT_FUNC, addr, ppName, pOffset);
    if (ret != FLCN_OK)
    {
        ret = symResolveSection(addr, ppName, pOffset);
    }
    return ret;
}

/**
 * @brief symResolveDataSmart   Resolve object name/offset with fallback
 *
 * Works like @sa symResolveData, but if no object is found, it tries
 * to lookup section address belongs to.
 */
FLCN_STATUS symResolveDataSmart(LwU64 addr, const char **ppName, LwU64 *pOffset)
{
    FLCN_STATUS ret;

    ret = symResolveType(STT_OBJECT, addr, ppName, pOffset);
    if (ret != FLCN_OK)
    {
        ret = symResolveSection(addr, ppName, pOffset);
    }
    return ret;
}
