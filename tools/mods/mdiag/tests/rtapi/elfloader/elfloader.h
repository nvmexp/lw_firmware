/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "core/include/rc.h"
#include "libelf/libelf.h"

class ElfLoader {
public:

    ElfLoader(const char* fileName);
    ~ElfLoader();

    struct ElfData{
        UINT64   addr;      /* Address.         */
        UINT64   vAddr;     /* Virtual address. */
        UINT64   memSize;   /* Size in memory.  */
        UINT64   diskSize;  /* Size on disk.    */
        char     *data;     /* The bits.        */
    };

    RC LoadElf(UINT64 offset = 0);

private:
    RC ReadElf(const char * fileName);
    RC LoadData(Elf64_Word segIndex, ElfData &elfData);

    template <typename EhdrType, typename PhdrType, typename ShdrType>
    RC LoadElfData(Elf64_Word segIndex, ElfLoader::ElfData &elfData);

    Elf64_Word m_lastSegment;
    Elf* m_pElf;
    bool m_is32Bit;
    int  m_fd;
};
