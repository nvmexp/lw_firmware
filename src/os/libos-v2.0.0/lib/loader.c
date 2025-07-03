/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "loader.h"
#include "../debug/elf.h"
#include "../include/libos_init_args.h"
#include "../kernel/libos_defines.h"
#include <lwtypes.h>
#define LIBOS_PHDR_BOOT 0

typedef struct
{
    LwU32 magic;
    LwU32 major_version;
    LwU64 additional_heap;
} libos_header_t;

/**
 *
 * @brief Returns the size of the heap required at the end of the ELF
 *
 *        Once the kernel starts, additional heap
 *        space is required by the kernel to unfurl any copy
 *        on write, or BSS sections.  This memory is expected
 *        to live immediately after the end of the image.
 *
 *        The total size of this region is precise and callwlated
 *        by the linker through the ld-script.
 *
 * @param[in] elf
 * @returns false if not a recognized LIBOS ELF.
 */
LwBool libosElfGetStaticHeapSize(elf64_header *elf, LwU64 *additionalHeapSize)
{
    // Locate the pheader table
    elf64_phdr *phdr = (elf64_phdr *)((LwU8 *)elf + elf->phoff + elf->phentsize * LIBOS_MANIFEST_HEADER);

    // Verify section size
    if (phdr->filesz < sizeof(libos_header_t))
        return LW_FALSE;

    // The leader is at the end of the phdr_boot section
    libos_header_t *header =
        (libos_header_t *)(((LwU8 *)elf) + phdr->offset + phdr->filesz - sizeof(libos_header_t));

    // Verify magic
    if (header->magic != LIBOS_MAGIC || header->major_version != 0)
        return LW_FALSE;

    *additionalHeapSize = header->additional_heap;

    return LW_TRUE;
}

/**
 *
 * @brief Returns the offset from the start of the ELF corresponding
 *        to the entry point.
 *
 *        The ELF should be copied directly into memory without modification.
 *        The packaged LIBOS elf contains a self bootstrapping ELF loader.
 *
 *        During boot:
 *            - PHDRs are copied into memory descriptors
 *            - Copy on write sections are duplicated (into additional heap above)
 *
 * @param[in] elf
 * @returns false if not a recognized LIBOS ELF.
 */

LwBool libosElfGetBootEntry(elf64_header *elf, LwU64 *entry_offset)
{
    LwU64 entry_va = elf->entry;
    LwU32 i;
    for (i = 0; i < elf->phnum; i++)
    {
        elf64_phdr *phdr = (elf64_phdr *)((LwU8 *)elf + elf->phoff + elf->phentsize * i);
        if (entry_va >= phdr->vaddr && entry_va < (phdr->vaddr + phdr->memsz))
        {
            *entry_offset = entry_va - phdr->vaddr + phdr->offset;
            return LW_TRUE;
        }
    }
    return LW_FALSE;
}
