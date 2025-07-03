/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBELF_H_
#define LIBELF_H_
#include <lwtypes.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "libos_status.h"

// Structures for Loader
typedef struct
{
    LwU8 ident[16];
    LwU16 type;
    LwU16 machine;
    LwU32 version;
    LwU64 entry;
    LwU64 phoff;
    LwU64 shoff;
    LwU32 flags;
    LwU16 ehsize;
    LwU16 phentsize;
    LwU16 phnum;
    LwU16 shentsize;
    LwU16 shnum;
    LwU16 shstrndx;
} LibosElf64Header;

typedef struct
{
    LwU32 type;
    LwU32 flags;
    LwU64 offset;
    LwU64 vaddr;
    LwU64 paddr;
    LwU64 filesz;
    LwU64 memsz;
    LwU64 align;
} LibosElf64ProgramHeader;

#define PF_X 1
#define PF_W 2
#define PF_R 4

#define PT_LOAD    1
#define PT_NOTE    4
#define PT_PHDR    6
#define PT_SHLIB   5
#define PT_DYNAMIC 2
// Structures for finding Symbols
typedef struct
{
    LwU32 name;
    LwU32 type;
    LwU64 flags;
    LwU64 addr;
    LwU64 offset;
    LwU64 size;
    LwU32 link;
    LwU32 info;
    LwU64 addralign;
    LwU64 entsize;
} LibosElf64SectionHeader;

#define SHT_NOBITS   8
#define SHT_PROGBITS 1

typedef struct
{
    LwU32 name;
    LwU8 info;
    LwU8 other;
    LwU16 shndx;
    LwU64 value;
    LwU64 size;
} LibosElf64Symbol;

typedef struct {
  LwS64 tag; 
  LwU64 ptr;
} LibosElf64Dynamic;

typedef struct LibosElfImage{
    LibosElf64Header * elf;
    LwU64              size;
    void *            (*map)(struct LibosElfImage *image, LwU64 offset, LwU64 size);
} LibosElfImage;

LibosStatus               LibosElfImageConstruct(LibosElfImage * image, void * elf, LwU64 size);

// Program headers
LibosElf64ProgramHeader * LibosElfProgramHeaderForRange(LibosElfImage * image, LwU64 va, LwU64 vaLastByte);
LibosElf64ProgramHeader * LibosElfProgramHeaderNext(LibosElfImage * image, LibosElf64ProgramHeader * previous);

// Section headers
LibosElf64SectionHeader * LibosElfFindSectionByName(LibosElfImage * image, const char *targetName);
LibosStatus               LibosElfMapSection(LibosElfImage * image, LibosElf64SectionHeader * shdr, LwU8 ** start, LwU8 ** end);
LibosElf64SectionHeader * LibosElfFindSectionByAddress(LibosElfImage * image, LwU64 address);
LibosElf64SectionHeader * LibosElfSectionHeaderNext(LibosElfImage * image, LibosElf64SectionHeader * previous);

// Dynamic linker
LibosElf64ProgramHeader * LibosElfHeaderDynamic(LibosElfImage * image);
LibosElf64Dynamic * LibosElfDynamicEntryNext(LibosElfImage * image, LibosElf64ProgramHeader * dynamicTableHeader, LibosElf64Dynamic * previous);

// VA based access
void *                    LibosElfMapVirtual(LibosElfImage * image, LwU64 address, LwU64 size);
const char *              LibosElfMapVirtualString(LibosElfImage * image, LwU64 address);

// Libos Tiny entry
LwBool                    LibosTinyElfGetBootEntry(LibosElfImage * image, LwU64 * physicalEntry);

LwBool                    LibosElfCommitData(LibosElfImage * image, LwU64 commitVirtualAddress, LwU64 commitSize);

#ifdef __cplusplus
}
#endif

#endif