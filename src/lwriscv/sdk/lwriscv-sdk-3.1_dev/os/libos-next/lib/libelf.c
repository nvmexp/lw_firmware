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
#    define strcmp(str1, str2) portStringCompare(str1, str2, 0x1000)
#else // LWRM
#    include <memory.h>
#    include <stdio.h>
#    include <string.h>
#endif // LWRM

#include "libelf.h"
#include <lwtypes.h>

static void * elfSimpleAccess(LibosElfImage * image, LwU64 offset, LwU64 size)
{
    if ((offset + size) <= image->size)
        return (LwU8 *)image->elf + offset;
    return 0;
}

/**
 *
 * @brief Initialize an elf image to point to an unloaded elf.
 *
 * @param[in] image
 *  The image structure to initialize with the provided elf.
 * @param[in] elf
 * @param[in] size
 */
LibosStatus LibosElfImageConstruct(LibosElfImage * image, void * elf, LwU64 size)
{
    if (size < sizeof(LibosElf64Header))
        return LibosErrorIncomplete;
        

#ifdef LIBOS_TOOL_BAKE
    image->elf = malloc(size);
    memcpy(image->elf, elf, size);
#else
    image->elf = elf;
#endif

    image->size = size;
    image->map = elfSimpleAccess;

    return LibosOk;
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
LwBool LibosTinyElfGetBootEntry(LibosElfImage * image, LwU64 * physicalEntry)
{
    LwU64 entryVa = image->elf->entry;

    // Find the phdr containing the entry point
    LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderForRange(image, entryVa, entryVa); 
    if (phdr)
    {
        *physicalEntry = phdr->paddr + entryVa - phdr->vaddr;
        return LW_TRUE;
    }

    return LW_FALSE;
}

/**
 *
 * @brief Iterates Program Headers
 * 
 *  Program headers are loadable regions of the ELF.
 *  Each header may contain any number of sections.
 *
 * @param[in] image
 * @param[in] previous
 *   Previous returned program header. Pass null to get first header.
 */
LibosElf64ProgramHeader * LibosElfProgramHeaderNext(LibosElfImage * image, LibosElf64ProgramHeader * previous)
{
    LibosElf64ProgramHeader * phdrTable = image->map(image, image->elf->phoff, sizeof(LibosElf64ProgramHeader) * image->elf->phnum);
    LibosElf64ProgramHeader * phdrTableEnd = phdrTable + image->elf->phnum;
    
    LibosElf64ProgramHeader * next;
    if (!previous) 
        next = phdrTable;
    else
        next = previous + 1;

    if (next == phdrTableEnd)
        return 0;

    return next;
}

/**
 *
 * @brief Iterates Sections
 * 
 *
 * @param[in] image
 * @param[in] previous
 *   Previous returned section header. Pass null to get first header.
 */
LibosElf64SectionHeader * LibosElfSectionHeaderNext(LibosElfImage * image, LibosElf64SectionHeader * previous) 
{
    LibosElf64SectionHeader * shdrTable = image->map(image, image->elf->shoff, sizeof(LibosElf64SectionHeader) * image->elf->shnum);
    LibosElf64SectionHeader * shdrTableEnd = shdrTable + image->elf->shnum;

    LibosElf64SectionHeader * next;
    if (!previous) 
        next = shdrTable;
    else
        next = previous + 1;

    if (next == shdrTableEnd)
        return 0;
    
    return next;
}


/**
 *
 * @brief Locates the dynamic loader section
 *  
 *  This section contains dependency information, import/export symbols,
 *  and relocations.
 *
 * @param[in] image
 * @param[in] previous
 *   Previous returned section header. Pass null to get first header.
 */
LibosElf64ProgramHeader * LibosElfHeaderDynamic(LibosElfImage * image)
{
    for (LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderNext(image, 0); phdr; phdr = LibosElfProgramHeaderNext(image, phdr))
        if (phdr->type == PT_DYNAMIC)
            return phdr;
    return 0;
}

/**
 *
 * @brief Iterates resources in dynamic loader section
 *  
 *  LibosElf64Dynamic::tag must contain
 *      DT_HASH 
 *      DT_STRTAB 
 *      DT_SYMTAB
 *      DT_RELA
 *      DT_RELASZ
 *      DT_RELAENT
 *      DT_STRSZ
 *      DT_SYMENT
 *      DT_REL
 *      DT_RELSZ
 *      DT_RELENT
 *  It may also contain
 *      DT_NEEDED - List of dynamic libraries this image depends on
 *      ptr is an offset in DT_STRTAB table for image name
 * 
 * @param[in] dynamicTableHeader
 *   @see LibosElfHeaderDynamic
 * @param[in] previous
 *   Previous returned dynamic table entry. Pass null to get first entry.
 */
LibosElf64Dynamic * LibosElfDynamicEntryNext(LibosElfImage * image, LibosElf64ProgramHeader * dynamicTableHeader, LibosElf64Dynamic * previous) 
{
    LibosElf64Dynamic * dynamicTable = image->map(image, dynamicTableHeader->offset, dynamicTableHeader->filesz);
    LibosElf64Dynamic * dynamicTableEnd = dynamicTable + (dynamicTableHeader->filesz / sizeof(LibosElf64Dynamic));

    LibosElf64Dynamic * next;
    if (!previous)
        next = dynamicTable;
    else
        next = previous + 1;

    if (previous == dynamicTableEnd)
        return 0;

    return next;
}

/**
 *
 * @brief Locates the program header containing this region of virtual addresses.
 *  
 * @param[in] image
 * @param[in] va
 * @param[in] vaLastByte
 *   The program header must contain all bytes in the range [va, vaLastByte] inclusive.
 */
LibosElf64ProgramHeader * LibosElfProgramHeaderForRange(LibosElfImage * image, LwU64 va, LwU64 vaLastByte) 
{
    for (LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderNext(image, 0);
         phdr;
         phdr = LibosElfProgramHeaderNext(image, phdr))
    {
        // Is this memory region within this PHDR?
        if (va >= phdr->vaddr && (vaLastByte - phdr->vaddr) < phdr->memsz) 
            return phdr;
    }

    return 0;
}

/**
 *
 * @brief Find the start and end of the ELF section in memory
 *        from the section name.
 *
 * @param[in] image
 * @param[in] sectionName
 *   The section to find such as .text or .data
 * @param[out] start, end
 *   The start and end of the section in the loaded ELF file
 *   e.g   validPtr >= start && validPtr < end
 */
LibosElf64SectionHeader * LibosElfFindSectionByName(LibosElfImage * image, const char *targetName)
{
    // @todo: Enumerate, and introduce function for kth section header
    LibosElf64SectionHeader * shdr = image->map(image, image->elf->shoff, image->elf->shnum * sizeof(LibosElf64SectionHeader));
    
    if (!shdr || image->elf->shstrndx >= image->elf->shnum)
        return 0;

    const char *shstr = image->map(image, shdr[image->elf->shstrndx].offset, shdr[image->elf->shstrndx].size);
    size_t      shstrSize = shdr[image->elf->shstrndx].size;

    for (int i = 0; i < image->elf->shnum; i++, shdr++)
    {
        if (shdr->name >= shstrSize)
            return 0;

        const char *name = shstr + shdr->name;
      
        if (strncmp(name, targetName, shstrSize - shdr->name) == 0)
        {
            LwU64 offset = shdr->offset;

            return shdr;
        }
    }

    return 0;
}

/**
 *
 * @brief Find the start and end of the loaded section.
 *        
 * @returns 
 *  LibosErrorIncomplete if there are no bits assigned to the section
 * @param[in] image
 * @param[in] shdr
 *   The section header to find the start and end of.
 * @param[out] start, end
 *   The start and end of the section in the loaded ELF file
 *   e.g   validPtr >= start && validPtr < end
 */
LibosStatus LibosElfMapSection(LibosElfImage * image, LibosElf64SectionHeader * shdr, LwU8 ** start, LwU8 ** end)
{
    LwU8 * mapping;
    
    if (shdr->type == SHT_NOBITS) 
        // Try mapping through the PHDR as a last ditch approach
        mapping = LibosElfMapVirtual(image, shdr->addr, shdr->size);
    else
        mapping = image->map(image, shdr->offset, shdr->size);
    *start   = mapping;
    *end     = mapping + shdr->size;

    return LibosOk;
}

/**
 *
 * @brief Find the start and end of the ELF section in memory
 *        from the section name.
 *
 * @param[in] image
 * @param[in] address
 *   Query the section containing the requested byte
 */
LibosElf64SectionHeader * LibosElfFindSectionByAddress(LibosElfImage * image, LwU64 address)
{
    for (LibosElf64SectionHeader * shdr = LibosElfSectionHeaderNext(image, 0); shdr; shdr = LibosElfSectionHeaderNext(image, shdr))
        if (address >= shdr->addr && address < (shdr->addr + shdr->size))
            return shdr;

    return 0;
}

/**
 *
 * @brief Reads an arbitrary sized block of memory by loaded VA through
 *        the ELF. This can be used to read data from the perspective
 *        of a processor who has loaded the ELF.
 *
 * @param[in] image
 * @param[in] address
 *   The absolute virtual address to read
 * @param[out] size
 *   The number of bytes that must be valid.
 * @returns
 *   The pointer to the data in question, or NULL if the operation failed)
 */
void *LibosElfMapVirtual(LibosElfImage * image, LwU64 address, LwU64 size)
{
    LwU8 *start, *end;
    LwU64 vaBase;

    LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderForRange(image, address, address + size - 1);
    if (!phdr)
        return 0;

    return image->map(image, address - phdr->vaddr + phdr->offset, size);
}

/**
 *
 * @brief Reads an arbitrary length stringby loaded VA through
 *        the ELF. This can be used to read data from the perspective
 *        of a processor who has loaded the ELF.
 *
 * @param[in] image
 * @param[in] address
 *   The virtual address of the string constant.
 * @returns
 *   The string constant. Returns null if the string isn't entirely contained
 *   in the image.
 */
const char *LibosElfMapVirtualString(LibosElfImage * image, LwU64 address)
{
    LwU64 stringOffset = 0;
    const char * string = 0;

    do
    {
        // Ensure the entire string is in the same PHDR
        LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderForRange(image, address, address + stringOffset);
        if (!phdr)
            return 0;   // No null terminator, invalid string

        // Update the mapping
        string = (LwU8 *)image->map(image, phdr->offset + (address - phdr->vaddr), stringOffset+1);        
        if (!string)
            return 0;   

        stringOffset++;
    } while(string[stringOffset]);

    return string;
}

#ifdef LIBOS_TOOL_BAKE

LwBool LibosElfCommitData(LibosElfImage *elf, LwU64 commitVirtualAddress, LwU64 commitSize) 

{
    if (!commitSize)
        return LW_FALSE;

    // Ensure the commit size doesn't wrap
    LwU64 lastByte;
    if (__builtin_add_overflow(commitVirtualAddress, commitSize - 1, &lastByte))
        return LW_FALSE;

    // Ensure that the commit region doesn't partially overlap any sections
    for (LibosElf64SectionHeader * psec = LibosElfSectionHeaderNext(elf, 0); psec; psec = LibosElfSectionHeaderNext(elf, psec))
        if (lastByte >= psec->addr && (lastByte - psec->addr) < psec->size)
            lastByte = psec->addr + psec->size - 1;

    // Find the containing PHDR
    LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderForRange(elf, commitVirtualAddress, lastByte);
    
    // Is the area already committed?
    if (lastByte < (phdr->vaddr + phdr->filesz))
        return LW_TRUE;

    // Find insertion point in the ELF image
    LwU64 insertionOffset = phdr->offset + phdr->filesz;
    LwU64 insertionCount  = (lastByte + 1) - (phdr->vaddr + phdr->filesz);   

    // Update the file backed size
    phdr->filesz = lastByte - phdr->vaddr + 1;

    // Update all PHDR offsets
    for (LibosElf64ProgramHeader * phdr = LibosElfProgramHeaderNext(elf, 0); phdr; phdr = LibosElfProgramHeaderNext(elf, phdr))
        if (phdr->offset > insertionOffset)
            phdr->offset += insertionCount;

    // Update all section offsets
    for (LibosElf64SectionHeader * psec = LibosElfSectionHeaderNext(elf, 0); psec; psec = LibosElfSectionHeaderNext(elf, psec))
    {
        if (psec->offset > insertionOffset)
            psec->offset += insertionCount;

        // Did we just commit a section? Colwert from nobits to progbits
        if (psec->addr >= phdr->vaddr && (psec->addr+psec->size) <= (phdr->vaddr + phdr->filesz)) 
            if (psec->type == SHT_NOBITS) {
                // @bug: Set the offset field
                psec->type = SHT_PROGBITS;
            }
    }

    if (elf->elf->phoff > insertionOffset)
        elf->elf->phoff += insertionCount;

    if (elf->elf->shoff > insertionOffset)
        elf->elf->shoff += insertionCount;

    // Reallocate the file and create the hole
    elf->elf = realloc(elf->elf, elf->size + insertionCount);
    memmove(((LwU8*)elf->elf) + insertionOffset + insertionCount, ((LwU8*)elf->elf) + insertionOffset, elf->size - insertionOffset);

    // Zero the hole
    memset(((LwU8*)elf->elf) + insertionOffset, 0, insertionCount);

    elf->size += insertionCount;
    
    return LW_TRUE;
}

#endif
