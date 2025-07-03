/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ELF_DEFS_H
#define ELF_DEFS_H

/*!
 * @file   elf_defs.h
 * @brief  ELF structures and defines
 */

// These structures are all ELF standard definitions, including the naming.
// Please do not rename any of these structs nor their members.
// For information on these fields, see http://www.skyfree.org/linux/references/ELF_Format.pdf

// AM-TODO: These should be split into 32 and 64bit versions when we want to support loading
//          both 32bit and 64bit ELF files.

typedef struct
{
    LwU8  e_ident[16];
    LwU16 e_type;
    LwU16 e_machine;
    LwU32 e_version;
#if __riscv_xlen == 32
    LwU32 e_entry;
    LwU32 e_phoff;
    LwU32 e_shoff;
#elif __riscv_xlen == 64
    LwU64 e_entry;
    LwU64 e_phoff;
    LwU64 e_shoff;
#endif
    LwU32 e_flags;
    LwU16 e_ehsize;
    LwU16 e_phentsize;
    LwU16 e_phnum;
    LwU16 e_shentsize;
    LwU16 e_shnum;
    LwU16 e_shstrndx;
} Elf_Ehdr;

typedef struct
{
    LwU32 sh_name;
    LwU32 sh_type;
#if __riscv_xlen == 32
    LwU32 sh_flags;
    LwU32 sh_addr;
    LwU32 sh_offset;
    LwU32 sh_size;
#elif __riscv_xlen == 64
    LwU64 sh_flags;
    LwU64 sh_addr;
    LwU64 sh_offset;
    LwU64 sh_size;
#endif
    LwU32 sh_link;
    LwU32 sh_info;
#if __riscv_xlen == 32
    LwU32 sh_addralign;
    LwU32 sh_entsize;
#elif __riscv_xlen == 64
    LwU64 sh_addralign;
    LwU64 sh_entsize;
#endif
} Elf_Shdr;


typedef struct
{
    LwU32 p_type;
#if __riscv_xlen == 32
    LwU32 p_offset;
    LwU32 p_vaddr;
    LwU32 p_paddr;
    LwU32 p_filesz;
    LwU32 p_memsz;
    LwU32 p_flags;
    LwU32 p_align;
#elif __riscv_xlen == 64
    LwU32 p_flags;
    LwU64 p_offset;
    LwU64 p_vaddr;
    LwU64 p_paddr;
    LwU64 p_filesz;
    LwU64 p_memsz;
    LwU64 p_align;
#endif
} Elf_Phdr;

typedef struct
{
    LwU32 st_name;
#if __riscv_xlen == 32
    LwU32 st_value;
    LwU32 st_size;
    LwU8  st_info;
    LwU8  st_other;
    LwU16 st_shndx;
#elif __riscv_xlen == 64
    LwU8  st_info;
    LwU8  st_other;
    LwU16 st_shndx;
    LwU64 st_value;
    LwU64 st_size;
#endif
} Elf_Sym;

#define IS_ELF(hdr) \
    ((hdr).e_ident[0] == 0x7f && (hdr).e_ident[1] == 'E' && \
     (hdr).e_ident[2] == 'L'  && (hdr).e_ident[3] == 'F')

#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATA2LSB 1

#define ET_EXEC 2
#define ET_CORE 4

#define EM_RISCV 243
#define EM_X86_64 62

#define PT_LOAD 1
#define PT_NOTE 4

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

//
// Section types
//
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11
#define SHT_NUM         12

//
// Symbol types
//
#define STT_NOTYPE      0
#define STT_OBJECT      1
#define STT_FUNC        2
#define STT_SECTION     3
#define STT_FILE        4
#define STT_COMMON      5
#define STT_TLS         6

#define ELF_ST_TYPE(x)  (((unsigned int) x) & 0xf)

#endif
