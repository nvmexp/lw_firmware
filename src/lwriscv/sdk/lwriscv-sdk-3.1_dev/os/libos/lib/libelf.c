/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifdef LWRM
#    include <lwport/lwport.h>
#    define memcpy(d, s, l)    portMemCopy(d, l, s, l)
#    define strcmp(str1, str2) portStringCompare(str1, str2, 0x1000)
#else // LWRM
#    include <memory.h>
#    include <stdio.h>
#    include <string.h>
#endif // LWRM

#include "libelf.h"
#include <lwtypes.h>
#define LIBOS_PHDR_BOOT 0

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

LwBool LibosElfGetBootEntry(LibosElf64Header *elf,LwU64 * physicalEntry)
{
    LwU64 entry_va = elf->entry;
    LwU32 i;
    for (i = 0; i < elf->phnum; i++)
    {
        LibosElf64ProgramHeader *phdr = (LibosElf64ProgramHeader *)((LwU8 *)elf + elf->phoff + elf->phentsize * i);
        if (entry_va >= phdr->vaddr && entry_va < (phdr->vaddr + phdr->memsz))
        {
            if (phdr->paddr) {
                // We're inside a section intended to be copied into IMEM.
                *physicalEntry = phdr->paddr + entry_va - phdr->vaddr;
                return LW_TRUE;
            }
            else 
                return LW_FALSE;
        }
    }
    return LW_FALSE;
}

LibosElf64ProgramHeader * LibosElfNextPhdr(LibosElf64Header *elf, LibosElf64ProgramHeader * previous)
{
    LibosElf64ProgramHeader * first = (LibosElf64ProgramHeader *)((LwU8 *)elf + elf->phoff);
    LibosElf64ProgramHeader * end = (LibosElf64ProgramHeader *)((LwU8 *)elf + elf->phoff + elf->phentsize * elf->phnum);
    
    LibosElf64ProgramHeader * next;
    if (!previous) 
        next = first;
    else
        next = previous + 1;

    if (next == end)
        return 0;

    return next;
}

LibosElf64ProgramHeader * LibosElfContainingProgramHeader(LibosElf64Header * elf, LwU64 va)
{
    for (LibosElf64ProgramHeader * i = LibosElfNextPhdr(elf, 0); i != 0; i = LibosElfNextPhdr(elf, i))
    {
        if (va >= i->vaddr && va < (i->vaddr + i->memsz))
            return i;
    }    
    return 0;
}

/**
 *
 * @brief Find the start and end of the ELF section in memory
 *        from the section name.
 *
 * @param[in] elf
 * @param[in] sectionName
 *   The section to find such as .text or .data
 * @param[out] start, end
 *   The start and end of the section in the loaded ELF file
 *   e.g   validPtr >= start && validPtr < end
 * @param[out] va_baase
 *   The virtual address this section is loaded at
 */
LwBool
LibosElfFindSectionByName(LibosElf64Header *elf, const char *targetName, LwU8 **start, LwU8 **end, LwU64 *va_base)
{
    LibosElf64SectionHeader *shdr        = (LibosElf64SectionHeader *)(((char *)elf) + elf->shoff);
    const char *string_base = ((char *)elf) + shdr[elf->shstrndx].offset;
    for (int i = 0; i < elf->shnum; i++, shdr++)
    {
        const char *name = string_base + shdr->name;
      
        if (strcmp(name, targetName) == 0)
        {
            LwU64 offset = shdr->offset;

            // @note: Sections will have incorrect file offsets if their type is SHT_NOBITS
            //        In this case, the backing store exists in the PHDR and can be looked up
            //        from there.
            if (shdr->type == SHT_NOBITS) 
            {
               LibosElf64ProgramHeader * phdr = LibosElfContainingProgramHeader(elf, shdr->addr);
               offset = phdr->offset + (shdr->addr - phdr->vaddr);               

               // Does this section extend past the end of the PHDR? Fail.
               if ((shdr->addr + shdr->size) > (phdr->vaddr + phdr->filesz))
                return LW_FALSE;
            }
            *start   = (LwU8 *)elf + offset;
            *end     = (LwU8 *)elf + offset + shdr->size;
            *va_base = shdr->addr;
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/**
 *
 * @brief Find the start and end of the ELF section in memory
 *        from the section name.
 *
 * @param[in] elf
 * @param[in] sectionName
 *   The section to find such as .text or .data
 * @param[out] start, end
 *   The start and end of the section in the loaded ELF file
 *   e.g   validPtr >= start && validPtr < end
 * @param[out] va_base
 *   The virtual address this section is loaded at
 */
LwBool
LibosElfFindSectionByAddress(LibosElf64Header *elf, LwU64 address, LwU8 **start, LwU8 **end, LwU64 *va_base)
{
    LibosElf64SectionHeader *shdr = (LibosElf64SectionHeader *)(((char *)elf) + elf->shoff);
    for (int i = 0; i < elf->shnum; i++, shdr++)
    {
        if (address >= shdr->addr && address < (shdr->addr + shdr->size))
        {
            *start   = (LwU8 *)elf + shdr->offset;
            *end     = (LwU8 *)elf + shdr->offset + shdr->size;
            *va_base = shdr->addr;
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/**
 *
 * @brief Reads an arbitrary sized block of memory by loaded VA through
 *        the ELF. This can be used to read data from the perspective
 *        of a processor who has loaded the ELF.
 *
 * @param[in] elf
 * @param[in] address
 *   The absolute virtual address to read
 * @param[out] size
 *   The number of bytes that must be valid.
 * @returns
 *   The pointer to the data in question, or NULL if the operation failed)
 */
void *LibosElfReadVirtual(LibosElf64Header *elf, LwU64 address, LwU64 size)
{
    LwU8 *start, *end;
    LwU64 va_base;

    // @todo This really should be using the PHDR as theoretically section headers
    //       might be stripped
    if (!LibosElfFindSectionByAddress(elf, address, &start, &end, &va_base))
        return 0;

    LwU64 section_offset = address - va_base;

    // Compute section offset (overflow check)
    LwU64 section_offset_tail = section_offset + size;
    if (section_offset_tail < section_offset)
        return 0;

    // Bounds check the tail
    if (section_offset_tail > (LwU64) (end - start))
        return 0;

    return (address - va_base) + start;
}

/**
 *
 * @brief Reads an arbitrary length stringby loaded VA through
 *        the ELF. This can be used to read data from the perspective
 *        of a processor who has loaded the ELF.
 *
 * @param[in] elf
 * @param[in] address
 *   The absolute virtual address to read
 * @returns
 *   The pointer to the data in question, or NULL if the operation failed)
 *   Ensures that all bytes of the string lie within the ELF same section.
 */
const char *LibosElfReadStringVirtual(LibosElf64Header *elf, LwU64 address)
{
    LwU8 *start, *end;
    LwU64 base;

    // @todo This really should be using the PHDR as theoretically section headers
    //       might be stripped
    if (!LibosElfFindSectionByAddress(elf, address, &start, &end, &base))
        return 0;

    LwU8 *region = (address - base) + start;
    LwU8 *i      = region;

    while (i >= start && i < end)
    {
        if (!*i)
            return (const char *)region;
        i++;
    }

    return 0;
}
