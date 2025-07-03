/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    mem.c
 * @brief   Memory management main code
 *
 * Main memory subsystem file (everything that not fits other files)
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <lwtypes.h>
#include <lwuproc.h>

// Register headers
#include <lwriscv-mpu.h>
#include <engine.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
// MK TODO: this is required to link with SafeRTOS core (that we must link with).
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>

#include <portfeatures.h>
#include <task.h>
#include <lwrtos.h>
#include <sections.h>
#include <shared.h>
#include <lwriscv/print.h>

#include <config/g_sections_riscv.h>

/* ------------------------ Module Includes -------------------------------- */
#include "drivers/drivers.h"
#include "drivers/mm.h"
#include "drivers/mpu.h"
#include "drivers/vm.h"
#include "config/g_sections_riscv.h"
#include "elf_defs.h"

sysKERNEL_DATA LwUPtr mmFbgpaPhysStart = 0;
sysKERNEL_DATA LwU64  mmWprId          = 0;
sysKERNEL_DATA LwBool bFbhubGated      = LW_FALSE;

sysKERNEL_DATA static LwUPtr   mmElfpaPhysStart = 0;
sysKERNEL_DATA static LwU32    numElfInPlaceSections = 0;

//
// Pointers to (pieces) of our ELF image
//
sysKERNEL_DATA static const Elf_Phdr *pPhdrs;              // phdrs
sysKERNEL_DATA static LwU64           phdrCount;           // number of sections

//------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------

#define RISCV_ALIGNMENT_ASSUME(ptr) __builtin_assume_aligned((void*)(ptr), 8)

#define REMAPPED_PHYS_SHIFT 52
#define REMAPPED_PHYS_MASK 0xF
#define REMAPPED_PHYS_BASE_RW 0xFF0
#define REMAPPED_PHYS_BASE_ELF 0XFF1
#define REMAPPED_PHYS_BASE_ELF_ODP_COW 0XFF2

//
// See bootloader mpu.h, that's the bootloader's reserved index #0.
// MMINTS-TODO: this is a mess, so this will have to be unified with the
// bootloader's headers.
//
#define BOOTLOADER_MPUIDX_IDENTITY_CARVEOUT  (LW_RISCV_CSR_MPU_ENTRY_COUNT - \
                                              LW_RISCV_MPU_BOOTLOADER_RSVD + 1)

// This has to be called BEFORE mpuInit and BEFORE s_physAddrToRunInPlaceElfAddr!
sysKERNEL_CODE static void s_mmInitElfData(void)
{
    if (ELF_IN_PLACE)
    {
        const Elf_Ehdr *pEhdr; // ELF header

        //
        // Check that the identity mapping set up by the bootloader is still alive at this point
        // (the bootloader guarantees that the application has identity mappings to the TCM and
        // the backing area in FB/WPR/sysmem etc. as it boots). We clear these mappings later
        // in mpuInit().
        //
        csr_write(LW_RISCV_CSR_SMPUIDX, BOOTLOADER_MPUIDX_IDENTITY_CARVEOUT);
        if (FLD_TEST_DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 0, csr_read(LW_RISCV_CSR_SMPUVA)))
        {
            dbgPrintf(LEVEL_ALWAYS, "ERROR: identity MPU mapping set up by the bootloader not present, cannot process ELF!\n");
        }

        // Pre-process elf
        pEhdr  = (const Elf_Ehdr*)RISCV_ALIGNMENT_ASSUME(mmElfpaPhysStart);
        pPhdrs = (const Elf_Phdr*)RISCV_ALIGNMENT_ASSUME(mmElfpaPhysStart + pEhdr->e_phoff);
        phdrCount = pEhdr->e_phnum;
    }
}

//
// This has to be called BEFORE mpuInit! Otherwise subsequent calls to
// s_physAddrToRunInPlaceElfAddr may try to access the ELF once the identity
// mapping will have already been deleted.
//
sysKERNEL_CODE static void s_mmClearElfData(void)
{
    if (ELF_IN_PLACE)
    {
        pPhdrs = NULL;
        phdrCount = 0;
    }
}

sysKERNEL_CODE static LwUPtr s_physAddrToRunInPlaceElfAddr(LwUPtr physAddr)
{
    if (ELF_IN_PLACE && (((physAddr) >> REMAPPED_PHYS_SHIFT) >= REMAPPED_PHYS_BASE_RW))
    {
        for (LwU32 i = 0; i < phdrCount; i++)
        {
            if ((pPhdrs[i].p_type == PT_LOAD) && (pPhdrs[i].p_filesz > 0) && // check that section is valid
                (pPhdrs[i].p_memsz == pPhdrs[i].p_filesz) && // section must be entirely contained in file to be run-in-place
                (physAddr >= pPhdrs[i].p_paddr) && (physAddr < pPhdrs[i].p_paddr + pPhdrs[i].p_filesz))
            {
                // If the phys addr fits in this section, offset it by the section's offset
                numElfInPlaceSections++;
                return mmElfpaPhysStart + pPhdrs[i].p_offset + (physAddr - pPhdrs[i].p_paddr);
            }
        }
    }

    return (LwUPtr)NULL;
}

sysKERNEL_CODE static LwUPtr s_physAddrToFbAddr(LwUPtr physAddr)
{
    return mmFbgpaPhysStart + (physAddr & ((1ULL << REMAPPED_PHYS_SHIFT) - 1U));
}

sysKERNEL_CODE static LwUPtr
s_mmPhysNormalize(LwUPtr physAddr)
{
    if (ELF_IN_PLACE && ELF_IN_PLACE_FULL_IF_NOWPR && (mmWprId == 0)) // No WPR, use run-in-place ELF
    {
        LwUPtr result = s_physAddrToRunInPlaceElfAddr(physAddr);

        if (result != (LwUPtr)NULL)
        {
            return result;
        }

        // Even if !result, it could still be a heap-only section (which are NOBITS), so try with non-run-in-place
    }

    if ((physAddr >> REMAPPED_PHYS_SHIFT) == REMAPPED_PHYS_BASE_RW)
    {
        return s_physAddrToFbAddr(physAddr);
    }
    else if (ELF_IN_PLACE && ((physAddr >> REMAPPED_PHYS_SHIFT) == REMAPPED_PHYS_BASE_ELF))
    {
        LwUPtr result = s_physAddrToRunInPlaceElfAddr(physAddr);

        return result;
    }
    else
    {
        return physAddr;
    }
}

//
// Translate FB pointer to physical offset within FB.
// MK TODO: we may generalize that function at some point
// This function works only for mapped region
//
sysKERNEL_CODE FLCN_STATUS
mmVirtToPhysFb(LwU64 gspPtr, LwU64 *pFbOffset)
{
    LwU64 physAddr = 0;
    if (!mpuVirtToPhys((const void*)gspPtr, &physAddr, NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Resulting physical address must be inside FB
    if ((physAddr < LW_RISCV_AMAP_FBGPA_START) ||
        (physAddr >= (LW_RISCV_AMAP_FBGPA_START + LW_RISCV_AMAP_FBGPA_SIZE)))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (pFbOffset != NULL)
    {
        *pFbOffset = physAddr;
    }

    return FLCN_OK;
}

/*
 * This function must initialize memory subsystem. We need it to:
 * - Cleanup bootloader mappings
 * - Initialize variables for memory translation
 *
 * @param fbPhysBase    Physical address base of FB carveout.
 * @param wprId         Write-Protected Region id.
 */
sysKERNEL_CODE FLCN_STATUS
mmInit(LwUPtr fbPhysBase, LwUPtr elfPhysBase, LwU64 wprId)
{
    mmFbgpaPhysStart = fbPhysBase;
    mmElfpaPhysStart = elfPhysBase;

    dbgPrintf(LEVEL_INFO, "Physical base of framebuffer carveout is 0x%llx\n",
              mmFbgpaPhysStart);
    dbgPrintf(LEVEL_INFO, "Physical base of elf file is 0x%llx\n",
              mmElfpaPhysStart);

    // Save WPR ID.
    mmWprId = wprId;

    if (ELF_IN_PLACE)
    {
        dbgPrintf(LEVEL_INFO, "ELF-in-place enabled, mapping all eligible sections as elf-in-place!\n");

        if (ELF_IN_PLACE_FULL_ODP_COW)
        {
            dbgPrintf(LEVEL_INFO, "ELF ODP copy-on-write enabled, double-mapping paged RW sections as copy-on-write run-in-place ELF\n");
        }

        if (ELF_IN_PLACE_FULL_IF_NOWPR && (mmWprId == 0))
        {
            dbgPrintf(LEVEL_INFO, "Read-only WPR not enabled, mapping all memsize == filesize FB-backed sections as run-in-place ELF\n");
        }
    }

    s_mmInitElfData();

    // Normalize the physical address table up front, so we won't need to do it again
    for (LwU32 i = 0; i < UPROC_SECTION_COUNT; i++)
    {
        if (SectionDescMaxSize[i] == 0U)
        {
            dbgPrintf(LEVEL_ERROR, "Warning: couldn't map optimized-out section PA 0x%llx (size == 0)\n", SectionDescStartPhysical[i]);
            SectionDescStartPhysical[i] = (LwUPtr)NULL;
        }
        else
        {
#if ELF_IN_PLACE_FULL_ODP_COW
            LwUPtr physAddr = SectionDescStartPhysical[i];

            if ((i < UPROC_DATA_SECTION_COUNT) && // Data sections always go first!
                ((physAddr >> REMAPPED_PHYS_SHIFT) == REMAPPED_PHYS_BASE_ELF_ODP_COW))
            {
                // These are later used by ODP, which chooses between the two as backing storage
                SectionDescStartPhysical[i] = s_physAddrToFbAddr(physAddr);
                SectionDescStartDataInElf[i] = s_physAddrToRunInPlaceElfAddr(physAddr);

                if (SectionDescStartDataInElf[i] == (LwUPtr)NULL)
                {
                    // sometimes we just can't map a section into ELF, like with an all-heap section.
                    SectionDescStartDataInElf[i] = SectionDescStartPhysical[i];
                }
            }
            else
#endif // ELF_IN_PLACE_FULL_ODP_COW
            {
                SectionDescStartPhysical[i] = s_mmPhysNormalize(SectionDescStartPhysical[i]);
            }

            if (SectionDescStartPhysical[i] == (LwUPtr)NULL)
            {
                return FLCN_ERROR;
            }
        }
    }

    s_mmClearElfData();

    if (ELF_IN_PLACE && (numElfInPlaceSections > 0))
    {
        dbgPrintf(LEVEL_INFO, "Normalized section addrs for %d run-in-place ELF sections!\n", numElfInPlaceSections);
    }

    // Initialize MPU management
    if (!mpuInit())
    {
        return FLCN_ERROR;
    }

    // Initialize vm
    vmInit(engineDYNAMIC_VA_BASE, engineDYNAMIC_VA_SIZE);

    return FLCN_OK;
}


sysKERNEL_CODE FLCN_STATUS
mmTaskStackSetup(LwU8 sectionIdxStack, LwU16 *pStackDepth, LwUPtr **pStack)
{
    extern LwUPtr SectionDescStartVirtual[UPROC_SECTION_COUNT];
    extern LwUPtr SectionDescHeapSize[UPROC_DATA_SECTION_COUNT];
    extern LwLength SectionDescMaxSize[UPROC_SECTION_COUNT];

    FLCN_STATUS status = FLCN_OK;

    // Extract stack info from linker-script initialized data.
    *pStack = (LwUPtr*)SectionDescStartVirtual[sectionIdxStack];
    *pStackDepth = SectionDescMaxSize[sectionIdxStack] / sizeof(LwUPtr);

    // Set section to 100% used
    SectionDescHeapSize[sectionIdxStack] = SectionDescMaxSize[sectionIdxStack];

    return status;
}

sysKERNEL_CODE FLCN_STATUS
mmInitTaskMemory(const char *pName,
                 TaskMpuInfo *pTaskMpu,
                 MpuHandle *pMpuHandles,
                 LwLength mpuEntryCount,
                 LwU64 codeSectionIndex,
                 LwU64 dataSectionIndex)
{
    dbgPrintf(LEVEL_DEBUG, "Initializing memory for task %s...\n", pName);

    mpuTaskInit(pTaskMpu, pMpuHandles, mpuEntryCount);

    MpuHandle codeMpuHnd = MpuHandleIlwalid;
    MpuHandle dataMpuHnd = MpuHandleIlwalid;

    // Create single mapping for whole code used by task.
    if (!UPROC_SECTION_LOCATION_IS_ODP(SectionDescLocation[codeSectionIndex]))
    {
        //
        // Don't set the "cacheable" attribute for resident sections as it will
        // be absent on those mapped during boot and has no effect anyway.
        //
        LwU64 cacheable = (SectionDescLocation[codeSectionIndex] !=
            UPROC_SECTION_LOCATION_IMEM_RES);

        codeMpuHnd = mpuEntryNew(SectionDescStartPhysical[codeSectionIndex],
                                 SectionDescStartVirtual[codeSectionIndex],
                                 SectionDescMaxSize[codeSectionIndex],
                                 SectionDescPermission[codeSectionIndex] |
                                 DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, cacheable) |
                                 DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, mmWprId));
        if (codeMpuHnd == MpuHandleIlwalid)
        {
            return FLCN_ERROR;
        }
    }

    //
    // Create single mapping for whole data used by task (+stack).
    // This doesn't include heap that should be dynamically assigned.
    //
    if (!UPROC_SECTION_LOCATION_IS_ODP(SectionDescLocation[dataSectionIndex]))
    {
        //
        // Don't set the "cacheable" attribute for resident sections as it will
        // be absent on those mapped during boot and has no effect anyway.
        //
        LwU64 cacheable = (SectionDescLocation[dataSectionIndex] !=
            UPROC_SECTION_LOCATION_DMEM_RES);
        
        dataMpuHnd = mpuEntryNew(SectionDescStartPhysical[dataSectionIndex],
                                 SectionDescStartVirtual[dataSectionIndex],
                                 SectionDescMaxSize[dataSectionIndex],
                                 SectionDescPermission[dataSectionIndex] |
                                 DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, cacheable) |
                                 DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, mmWprId));
        if (dataMpuHnd == MpuHandleIlwalid)
        {
            if (codeMpuHnd != MpuHandleIlwalid)
            {
                mpuEntryRelease(codeMpuHnd);
            }
            return FLCN_ERROR;
        }
    }

    if (codeMpuHnd != MpuHandleIlwalid)
    {
        mpuTaskEntryAddPreInit(pTaskMpu, codeMpuHnd);
        mpuEntryRelease(codeMpuHnd);
    }
    if (dataMpuHnd != MpuHandleIlwalid)
    {
        mpuTaskEntryAddPreInit(pTaskMpu, dataMpuHnd);
        mpuEntryRelease(dataMpuHnd);
    }

    return FLCN_OK;
}
