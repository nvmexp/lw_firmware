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

#define PT_LOAD 1

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

#define SHT_NOBITS 8

typedef struct
{
    LwU32 name;
    LwU8 info;
    LwU8 other;
    LwU16 shndx;
    LwU64 value;
    LwU64 size;
} LibosElf64Symbol;

LwBool LibosElfFindSectionByName(LibosElf64Header *elf, const char *sectionName, LwU8 **start, LwU8 **end, LwU64 *va_base);
LwBool LibosElfFindSectionByAddress(LibosElf64Header *elf, LwU64 address, LwU8 **start, LwU8 **end, LwU64 *va_base); 
void * LibosElfReadVirtual(LibosElf64Header *elf, LwU64 address, LwU64 size);
const char *LibosElfReadStringVirtual(LibosElf64Header *elf, LwU64 address);
LwBool      LibosElfGetBootEntry(LibosElf64Header *elf, LwU64 * physicalEntry);
LibosElf64ProgramHeader * LibosElfNextPhdr(LibosElf64Header *elf, LibosElf64ProgramHeader * previous);

#ifdef __cplusplus
}
#endif

#endif