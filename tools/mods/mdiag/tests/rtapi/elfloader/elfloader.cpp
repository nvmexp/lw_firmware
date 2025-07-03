/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <unistd.h>
#include <fcntl.h>

#include "elfloader.h"
#include "core/include/platform.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/utils.h"

#define PN_XNUM 0xffff

ElfLoader::ElfLoader(const char* fileName)
{
    m_lastSegment = 0;
    m_is32Bit = false;
    RC rc = ReadElf(fileName);
    MASSERT(rc == OK);
}

ElfLoader::~ElfLoader()
{
    if (m_pElf != nullptr)
    {
        if (elf_end(m_pElf) > 0)
        {
            ErrPrintf("Error: Elf handle is not released\n");
        }
    }

    if (m_fd > 0)
    {
        close(m_fd);
    }
}

template <typename EhdrType, typename PhdrType, typename ShdrType>
RC ElfLoader::LoadElfData(Elf64_Word segIndex, ElfLoader::ElfData &elfData)
{
    Elf *elf = m_pElf;
    elfData.data = nullptr;

    EhdrType *ehdr = m_is32Bit ? reinterpret_cast<EhdrType*>(elf32_getehdr(elf)) :
                                 reinterpret_cast<EhdrType*>(elf64_getehdr(elf));
    PhdrType *phdr = m_is32Bit ? reinterpret_cast<PhdrType*>(elf32_getphdr(elf)) :
                                 reinterpret_cast<PhdrType*>(elf64_getphdr(elf));

    Elf64_Word numPhdrs = ehdr->e_phnum;
    InfoPrintf("LoadElfData numPhdrs:%d, m_is32Bit %d \n", numPhdrs, m_is32Bit);

    if (numPhdrs == PN_XNUM)
    {
        /* In case of overflow, the number of program headers can be
           found in the sh_info field of the first section header.  */
        Elf_Scn *section = elf_getscn(elf, 0);
        ShdrType *shdr = m_is32Bit ? reinterpret_cast<ShdrType*>(elf32_getshdr(section)) : reinterpret_cast<ShdrType*>(elf64_getshdr(section));
        if (shdr)
        {
            numPhdrs = shdr->sh_info;
        }
    }

    /* Run through the program headers and look for the next loadable segment.  */
    while (segIndex < numPhdrs)
    {
        if (phdr[segIndex].p_type == PT_LOAD)
        {
            elfData.addr = phdr[segIndex].p_paddr;
            elfData.vAddr = phdr[segIndex].p_vaddr;
            elfData.memSize = phdr[segIndex].p_memsz;
            elfData.diskSize = phdr[segIndex].p_filesz;
            elfData.data = elf_rawfile(elf, nullptr) + phdr[segIndex].p_offset;
            ++segIndex;
            break;
        }
        else
        {
            ++segIndex;
        }
    }

    return OK;
}

RC ElfLoader::LoadData(Elf64_Word segIndex, ElfData &elfData)
{
    RC rc = OK;
    char *elfIdent = nullptr;
    char  elfClass;

    elfIdent = elf_getident(m_pElf, NULL);
    MASSERT(elfIdent != nullptr);

    elfClass = elfIdent[EI_CLASS];
    m_is32Bit = (elfClass == ELFCLASS32);
    if (m_is32Bit)
    {
        CHECK_RC((LoadElfData<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>(segIndex, elfData)));
    }
    else
    {
        CHECK_RC((LoadElfData<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>(segIndex, elfData)));
    }

    return rc;
}

RC ElfLoader::ReadElf(const char * fileName)
{
    m_fd = open(fileName, O_RDONLY);
    if (m_fd < 0)
    {
        ErrPrintf("Failed to open filei: %s!\n", fileName);
        return RC::CANNOT_OPEN_FILE;
    }

    if (elf_version(EV_LWRRENT) == EV_NONE)
    {
        ErrPrintf("ELF library initialization failed! - It's out of date\n");
        return RC::SOFTWARE_ERROR;
    }

    m_pElf = elf_begin(m_fd, ELF_C_READ, NULL);
    if (m_pElf == nullptr)
    {
        ErrPrintf("Can't read ELF file!");
        return RC::FILE_READ_ERROR;
    }

    return OK;
}

RC ElfLoader::LoadElf(UINT64 offset)
{
    RC rc = OK;
    ElfData elfData;
    do
    {
        CHECK_RC(LoadData(m_lastSegment, elfData));

        if (elfData.data != nullptr)
        {
            UINT64 start = elfData.addr + offset;
            UINT64 end = elfData.addr + offset + elfData.diskSize;

            DebugPrintf("Start to write sysmem range from 0x%11x to 0x%11x, size:0x%x!\n",
                    start, end - 1, elfData.diskSize);

            CHECK_RC(Platform::PhysWr(start, elfData.data, elfData.diskSize));

            DebugPrintf("Finish writing sysmem range from 0x%11x to 0x%11x!\n",
                    start, end - 1);

            CHECK_RC(MDiagUtils::ZeroMem(end, elfData.memSize - elfData.diskSize));
        }

        m_lastSegment++;
    } while (elfData.data != nullptr);

    return rc;
}
