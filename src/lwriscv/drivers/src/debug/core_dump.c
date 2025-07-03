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
 * @file    core_dump.c
 * @brief   Generate core dump file
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <engine.h>
#include <lwriscv-mpu.h>
#include <elf_defs.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
// MK TODO: this is required to link with SafeRTOS core (that we must link with).
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>

#include <portfeatures.h>
#include <task.h>

/* ------------------------ Module Includes -------------------------------- */
#include <lwriscv/print.h>
#include <shlib/string.h>
#include "drivers/drivers.h"
#include "drivers/mm.h"
#include "drivers/mpu.h"
#include "drivers/vm.h"
#include "core_dump_pvt.h"

sysKERNEL_DATA static void *coreDumpBuffer;
sysKERNEL_DATA static LwU64 coreDumpBufferSize;
sysKERNEL_DATA static LwU64 *pCoreDumpFinalSize;

sysKERNEL_DATA static LwU8 taskCount = 0;
sysKERNEL_DATA LwrtosTaskHandle taskHandles[MAX_DEBUGGED_TASK_COUNT] = {0};
sysKERNEL_DATA char taskNames[MAX_DEBUGGED_TASK_COUNT][MAX_DEBUGGED_TASK_NAME_SIZE] = {0};

sysKERNEL_DATA static LwU32 falcolwersion;

sysKERNEL_CODE void
coreDumpRegisterTask(LwrtosTaskHandle handle, const char *name)
{
    if (taskCount >= MAX_DEBUGGED_TASK_COUNT)
    {
        dbgPrintf(LEVEL_ERROR,
                  "core_dump: Error registering task, too many tasks\n");
        return;
    }

    size_t len = strnlen(name, MAX_DEBUGGED_TASK_NAME_SIZE);
    if (len >= MAX_DEBUGGED_TASK_NAME_SIZE)
    {
        dbgPrintf(LEVEL_ERROR,
                  "core_dump: Error registering task, name too long: %s\n",
                  name);
        return;
    }

    taskHandles[taskCount] = handle;
    memcpy(taskNames[taskCount], name, len + 1);
    taskCount++;
}

sysKERNEL_CODE void
coreDumpInit(LwU64 coreDumpPhysAddr, LwU32 coreDumpSize)
{
    // Check whether PA is in SYSMEM
    if ((coreDumpPhysAddr < LW_RISCV_AMAP_SYSGPA_START) ||
       (coreDumpPhysAddr >= (LW_RISCV_AMAP_SYSGPA_START + LW_RISCV_AMAP_SYSGPA_SIZE)) ||
       (coreDumpSize > (LW_RISCV_AMAP_SYSGPA_START + LW_RISCV_AMAP_SYSGPA_SIZE - coreDumpPhysAddr)))
    {
        dbgPrintf(LEVEL_ERROR, "core_dump: Failed to init core dump buffer\n");
        return;
    }

    // Check whether size is at least 1 page
    if (coreDumpSize < LW_RISCV_CSR_MPU_PAGE_SIZE)
    {
        dbgPrintf(LEVEL_ERROR,
                  "core_dump: Failed to init core dump buffer, size is too small (%d B)\n",
                  coreDumpSize);
        return;
    }

    // Get a sufficient number of pages if the number of bytes isn't divisible by page size
    LwU64 pageCount = (coreDumpSize + LW_RISCV_CSR_MPU_PAGE_SIZE - 1) /
                      LW_RISCV_CSR_MPU_PAGE_SIZE;

    LwUPtr virtAddr = vmAllocateVaSpace(pageCount);
    if (virtAddr == 0)
    {
        dbgPrintf(LEVEL_ERROR, "core_dump: Failed to init core dump buffer\n");
        return;
    }

    void *buf = mpuKernelEntryCreate(coreDumpPhysAddr, virtAddr,
                                     pageCount * LW_RISCV_CSR_MPU_PAGE_SIZE,
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _COHERENT, 1) |
                                     0);

    if (buf == NULL)
    {
        dbgPrintf(LEVEL_ERROR, "core_dump: Failed to init core dump buffer\n");
        return;
    }

    // Reserve start of the buffer for final core dump size
    pCoreDumpFinalSize = buf;
    coreDumpBuffer = buf + sizeof(*pCoreDumpFinalSize);
    *pCoreDumpFinalSize = 0;

    coreDumpBufferSize = coreDumpSize - sizeof(*pCoreDumpFinalSize);

    dbgPrintf(LEVEL_DEBUG, "core_dump: Initialized buffer to %p (%llu B)\n", coreDumpBuffer,
           coreDumpBufferSize);

    falcolwersion = intioRead(LW_PRGNLCL_FALCON_OS);
}

/* ELF header for the core file. Values are standard for a RISCV ELF, except the ET_CORE type */
sysKERNEL_CODE static void
_fillEhdr(Elf_Ehdr *ehdr, LwU64 phoff, LwU16 phnum)
{
    ehdr->e_ident[0]    = 0x7f;
    ehdr->e_ident[1]    = 'E';
    ehdr->e_ident[2]    = 'L';
    ehdr->e_ident[3]    = 'F';
    ehdr->e_ident[4]    = ELFCLASS64;
    ehdr->e_ident[5]    = ELFDATA2LSB;
    ehdr->e_ident[6]    = 1;                // EI_VERSION
    ehdr->e_ident[7]    = 0;                // EI_OSABI
    ehdr->e_ident[8]    = 0;                // EI_ABIVERSION

    ehdr->e_type        = ET_CORE;
    ehdr->e_machine     = EM_RISCV;
    ehdr->e_version     = 1;
    ehdr->e_entry       = 0;
    ehdr->e_phoff       = phoff;
    ehdr->e_shoff       = 0;                // no section headers for core files
    ehdr->e_flags       = 0;
    ehdr->e_ehsize      = sizeof(Elf_Ehdr);
    ehdr->e_phentsize   = sizeof(Elf_Phdr);
    ehdr->e_phnum       = phnum;
    ehdr->e_shentsize   = 0;                //
    ehdr->e_shnum       = 0;                // no section headers for core files
    ehdr->e_shstrndx    = 0;                //
}

/* Program header that points to the location of note entries inside the core file */
sysKERNEL_CODE static void
_fillNotehdr(Elf_Phdr *notehdr, LwU64 nentriesOffset, LwU64 nentriesSize)
{
    notehdr->p_type     = PT_NOTE;
    notehdr->p_offset   = nentriesOffset;
    notehdr->p_vaddr    = 0;
    notehdr->p_paddr    = 0;
    notehdr->p_filesz   = nentriesSize;
    notehdr->p_memsz    = 0;
    notehdr->p_flags    = 0;
    notehdr->p_align    = 0;
}

/* Program header that maps a virtual address range to an offset inside the core file. */
sysKERNEL_CODE static void
_fillVMAhdr(Elf_Phdr *vmahdr, LwU64 offset, LwU8 *vaddr, LwU64 vmasz)
{
    vmahdr->p_type      = PT_LOAD;
    vmahdr->p_offset    = offset;
    vmahdr->p_vaddr     = (LwU64)vaddr;
    vmahdr->p_paddr     = 0;
    vmahdr->p_filesz    = vmasz;
    vmahdr->p_memsz     = vmasz;
    vmahdr->p_flags     = PF_R | PF_W;
    vmahdr->p_align     = 0x10;
}

sysKERNEL_CODE static void
_fillNprstatus(Elf_Nprstatus *nprstatus, LwU32 lwpid, portUnsignedBaseType pc, portUnsignedBaseType *gpregset)
{
    LwU16 signal = 0;   // GDB shows the linux signal if non-zero

    nprstatus->namesz  = 5;
    nprstatus->descsz  = sizeof(nprstatus->desc);
    nprstatus->type    = NT_PRSTATUS;
    nprstatus->name[0] = 'C';
    nprstatus->name[1] = 'O';
    nprstatus->name[2] = 'R';
    nprstatus->name[3] = 'E';
    nprstatus->name[4] = '\0';

    memcpy(&nprstatus->desc[PRSTATUS_OFFSET_PR_LWRSIG], &signal, sizeof(signal));
    memcpy(&nprstatus->desc[PRSTATUS_OFFSET_PR_PID], &lwpid, sizeof(lwpid));
    /* pr_reg = [ pc | x1 .. x31 ] */
    memcpy(&nprstatus->desc[PRSTATUS_OFFSET_PR_REG], &pc, sizeof(pc));
    memcpy(&nprstatus->desc[PRSTATUS_OFFSET_PR_REG] + sizeof(pc), gpregset, ELF_GREGSET_T_SIZE - sizeof(pc));
}

sysKERNEL_CODE static void
_fillNprpsinfo(Elf_Nprpsinfo *nprpsinfo, char *fname, LwU64 cause)
{
    nprpsinfo->namesz  = 5;
    nprpsinfo->descsz  = sizeof(nprpsinfo->desc);
    nprpsinfo->type    = NT_PRPSINFO;
    nprpsinfo->name[0] = 'C';
    nprpsinfo->name[1] = 'O';
    nprpsinfo->name[2] = 'R';
    nprpsinfo->name[3] = 'E';
    nprpsinfo->name[4] = '\0';

    LwU32 pid = 0;  // GDB shows this as the thread ID if lwpid is 0
    memcpy(&nprpsinfo->desc[PRPSINFO_OFFSET_PR_PID], &pid, sizeof(pid));
    // TODO: This should match the ucode elf name, but PRPSINFO_PR_FNAME_LEN
    // isn't large enough to hold the whole string. Also should use strncpy.
    memcpy(&nprpsinfo->desc[PRPSINFO_OFFSET_PR_FNAME], fname, PRPSINFO_PR_FNAME_LEN);

    // TODO: We could put some exception info here.
    char *pr_psargs = (char *)&nprpsinfo->desc[PRPSINFO_OFFSET_PR_PSARGS];
    if (cause < 8)
    {
        pr_psargs[0] = '0' + cause;
        pr_psargs[1] = '\0';
    }
    else
    {
        pr_psargs[0] = '\0';
    }
}

/* Dump the address range [vma_start, vma_end). */
sysKERNEL_CODE static LwBool
_dumpVMA(LwU64 *memdmpOffset, Elf_Phdr *phdr, LwU8 *vma_start, LwU8 *vma_end)
{
    // Align the start of the address range to GDB's dcache line.
    // If we don't do this GDB may read data from the ELF file being debugged
    // instead of the core dump.
    vma_start = (LwU8 *)LW_ALIGN_DOWN((LwU64)vma_start, DCACHE_DEFAULT_LINE_SIZE);

    LwU64 vmasz = vma_end - vma_start;
    dbgPrintf(LEVEL_TRACE,
              "core_dump: Dumping VMA (vmasz=%llu, vma_start=%p, vma_end=%p, offset=%llu, bufsz=%llu)\n",
              vmasz, vma_start, vma_end, *memdmpOffset, coreDumpBufferSize);

    if ((vma_start > vma_end) || (*memdmpOffset + vmasz) > coreDumpBufferSize)
    {
        dbgPrintf(LEVEL_ERROR, "core_dump: Error dumping VMA\n");
        return LW_FALSE;
    }

    _fillVMAhdr(phdr, *memdmpOffset, vma_start, vmasz);
    memcpy(coreDumpBuffer + *memdmpOffset, vma_start, vmasz);
    *memdmpOffset += vmasz;

    return LW_TRUE;
}


/*
 * Writes a core dump to coreDumpBuffer. The core dump is an ELF file in a
 * similar format to Linux core dumps.
 */
sysKERNEL_CODE void
coreDump(LwrtosTaskHandle handle,
         LwU64 cause)
{
    ct_assert(LW_OFFSETOF(xTCB, xCtx) == 0);
    xPortTaskContext *pCtx = &(taskGET_TCB_FROM_HANDLE(handle)->xCtx);

    const LwBool bExcFromKernel = (xPortGetExceptionNesting() > 1);

    if (fbhubGatedGet())
    {
        // Coredump goes to sysmem, which we can't access if in MSCG
        dbgPrintf(LEVEL_ERROR, "core_dump: Aborted due to Fbhub gating\n");
        return;
    }

    dbgPrintf(LEVEL_INFO,
              "core_dump: Dumping core to %p (%lluB), bExcFromKernel=%d\n",
              coreDumpBuffer, coreDumpBufferSize, bExcFromKernel);

    //
    // Layout:
    //  -- Elf file header
    //  -- Program headers
    //      Note Header
    //      VMA Headers (1 per context)
    //  -- Note entries
    //      prpsinfo note
    //      prstatus notes (1 per context)
    //  -- Memory dump
    //

    const LwU16 ctxnum = taskCount + (bExcFromKernel ? 1 : 0);      // Also dump kernel context if that's the source of the exception
    const LwU16 phnum = ctxnum + 2;                                 // One program header per context + one for NOTES section + one for falcolwersion

    const LwU64 phdrsOffset    = sizeof(Elf_Ehdr);
    const LwU64 nentriesOffset = phdrsOffset + phnum * sizeof(Elf_Phdr);
    const LwU64 nentriesSize   = sizeof(Elf_Nprpsinfo) + ctxnum * sizeof(Elf_Nprstatus);
    LwU64 memdmpOffset         = nentriesOffset + nentriesSize;

    if (memdmpOffset > coreDumpBufferSize)
    {
        dbgPrintf(LEVEL_ERROR, "core_dump: Error, buffer too small.\n");
        return;
    }

    Elf_Phdr *phdrs             = coreDumpBuffer + phdrsOffset;
    Elf_Nprpsinfo *nprpsinfo    = coreDumpBuffer + nentriesOffset;
    Elf_Nprstatus *nprstatuses  = coreDumpBuffer + nentriesOffset + sizeof(Elf_Nprpsinfo);

    LwU16 pidx = 0;
    LwU16 nidx = 0;

    _fillEhdr(coreDumpBuffer, phdrsOffset, phnum);
    _fillNotehdr(&phdrs[pidx++], nentriesOffset, nentriesSize);
    _fillNprpsinfo(nprpsinfo, /* fname */ "g_gsp_tu10x_riscv.elf", cause);

    // Dump falcolwersion
    LwU8 *start = (LwU8 *) &falcolwersion;
    if (LW_TRUE != _dumpVMA(&memdmpOffset, &phdrs[pidx++], start, start + sizeof(falcolwersion)))
    {
        goto cleanup;
    }

    // First context dumped should be the source of the exception
    LwU8 *stackStart = (LwU8 *)pCtx->uxGpr[LWRISCV_GPR_SP];
    if (bExcFromKernel)
    {
        // We don't have a TCB for kernel exceptions
        extern LwU8 __stack_end[];
        _fillNprstatus(&nprstatuses[nidx++], /* lwpid */ 1, pCtx->uxPc, pCtx->uxGpr);
        if (LW_TRUE != _dumpVMA(&memdmpOffset, &phdrs[pidx++], stackStart, __stack_end))
        {
            goto cleanup;
        }
    }
    else
    {
        LwU32 id       = (LwU32)(LwU64)pCtx;
        LwU8 *stackEnd = (LwU8 *)((xTCB *)pCtx)->pcStackLastAddress;
        _fillNprstatus(&nprstatuses[nidx++], id, pCtx->uxPc, pCtx->uxGpr);
        if (LW_TRUE != _dumpVMA(&memdmpOffset, &phdrs[pidx++], stackStart, stackEnd))
        {
            goto cleanup;
        }
        if (!mpuTaskUnmap(pCtx))
        {
            goto cleanup;
        }
    }

    // Dump the other tasks
    for (LwU8 i = 0; i < MAX_DEBUGGED_TASK_COUNT; i++)
    {
        if ((taskHandles[i] != NULL) && (taskHandles[i] != handle))
        {
            xTCB *pxTcb = taskGET_TCB_FROM_HANDLE(taskHandles[i]);
            LwU32 id         = (LwU32)(LwU64)pxTcb;
            LwU8 *stackStart = (LwU8 *)pxTcb->xCtx.uxGpr[LWRISCV_GPR_SP];
            LwU8 *stackEnd   = (LwU8 *)pxTcb->pcStackLastAddress;

            if (!mpuTaskMap(taskHandles[i]))
            {
                goto cleanup;
            }
            if (LW_TRUE != _dumpVMA(&memdmpOffset, &phdrs[pidx++], stackStart, stackEnd))
            {
                mpuTaskUnmap(taskHandles[i]);
                goto cleanup;
            }
            _fillNprstatus(&nprstatuses[nidx++], id, pxTcb->xCtx.uxPc, pxTcb->xCtx.uxGpr);
            if (!mpuTaskUnmap(taskHandles[i]))
            {
                goto cleanup;
            }
        }
    }

cleanup:
    if (!bExcFromKernel)
    {
        mpuTaskMap(pCtx);
    }

    if (nidx != ctxnum)
    {
        dbgPrintf(LEVEL_ERROR,
                  "core_dump: Error, wrote %u prstatus notes (expected %u)\n",
                  nidx, ctxnum);
    }
    if (pidx != phnum)
    {
        dbgPrintf(LEVEL_ERROR,
                  "core_dump: Error, wrote %u program headers (expected %u)\n",
                  pidx, phnum);
    }

    *pCoreDumpFinalSize = memdmpOffset;
    dbgPrintf(LEVEL_INFO, "core_dump: Dumped core to %p, size is %llu B\n",
              coreDumpBuffer, memdmpOffset);
}
