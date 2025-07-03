/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
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

#include "elf.h"
#include "lwtypes.h"

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
libosElfFindSectionByName(elf64_header *elf, const char *targetName, LwU8 **start, LwU8 **end, LwU64 *va_base)
{
    elf64_shdr *shdr        = (elf64_shdr *)(((char *)elf) + elf->shoff);
    const char *string_base = ((char *)elf) + shdr[elf->shstrndx].offset;
    LwU32 i;

    for (i = 0; i < elf->shnum; i++, shdr++)
    {
        const char *name = string_base + shdr->name;
        if (strcmp(name, targetName) == 0)
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
libosElfFindSectionByAddress(elf64_header *elf, LwU64 address, LwU8 **start, LwU8 **end, LwU64 *va_base)
{
    elf64_shdr *shdr = (elf64_shdr *)(((char *)elf) + elf->shoff);
    LwU32 i;

    for (i = 0; i < elf->shnum; i++, shdr++)
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
void *libosElfReadVirtual(elf64_header *elf, LwU64 address, LwU64 size)
{
    LwU8 *start, *end;
    LwU64 va_base;
    LwU64 section_offset;
    LwU64 section_offset_tail;

    // @todo This really should be using the PHDR as theoretically section headers
    //       might be stripped
    if (!libosElfFindSectionByAddress(elf, address, &start, &end, &va_base))
        return 0;

    section_offset = address - va_base;

    // Compute section offset (overflow check)
    section_offset_tail = section_offset + size;
    if (section_offset_tail < section_offset)
        return 0;

    // Bounds check the tail
    if (section_offset_tail > (LwLength)(end - start))
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
const char *libosElfReadStringVirtual(elf64_header *elf, LwU64 address)
{
    LwU8 *start, *end;
    LwU64 base;
    LwU8 *region;
    LwU8 *i;

    // @todo This really should be using the PHDR as theoretically section headers
    //       might be stripped
    if (!libosElfFindSectionByAddress(elf, address, &start, &end, &base))
        return 0;

    region = (address - base) + start;
    i      = region;

    while (i >= start && i < end)
    {
        if (!*i)
            return (const char *)region;
        i++;
    }

    return 0;
}
