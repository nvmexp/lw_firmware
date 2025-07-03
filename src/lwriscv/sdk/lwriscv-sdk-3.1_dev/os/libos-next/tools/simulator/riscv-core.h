/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef RISCV_CORE
#define RISCV_CORE

#include "libos-config.h"
#include "libriscv.h"
#include "lwmisc.h"
#include <assert.h>
#include <fstream>
#include <memory.h>
#include <random>
#include <lwtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "libelf.h"

extern "C" {
#include "libdwarf.h"
#include <sys/mman.h>
}

// HAL layer
#include "hal.h"

enum privilege_t
{
    privilege_user       = 0,
    privilege_supervisor = 1,
    privilege_reserved   = 2,
    privilege_machine    = 3,
};

struct image
{
    LwU64 entry_offset;
    std::vector<LwU8> elf;
    LibosElfImage     elfImage;
    std::vector<std::shared_ptr<std::vector<LwU8> > > symbols;
    LibosDebugResolver resolver;

    image(const std::string &basename);
    void load_symbols(const std::string & elf);
};

struct lwstom_memory
{
    // @note reads by definition cannot cross region boundaries.
    //   Read functions validate natural alignment
    //   For example: a 4 byte aligned field can't cross
    //   a 4kb aligned paged.

    virtual LwBool test_address(LwU64 addr) = 0;

    virtual LwU8 read8(LwU64 addr)              = 0;
    virtual void write8(LwU64 addr, LwU8 value) = 0;

    /**
     *  Default implementations of composite reads
     */
    virtual LwU16 read16(LwU64 addr);
    virtual LwU32 read32(LwU64 addr);
    virtual LwU64 read64(LwU64 addr);
    virtual void write16(LwU64 addr, LwU16 value);
    virtual void write32(LwU64 addr, LwU32 value);
    virtual void write64(LwU64 addr, LwU64 value);
    virtual const uint32_t get_memory_kind() = 0;
};

struct trivial_memory : public lwstom_memory
{
    LwU64 base;
    LwU8 * block;
    uint32_t kind;
    LwU64    limit;

    trivial_memory(uint32_t kind, LwU64 base, LwU64 limit)
        : base(base), kind(kind), limit(limit)
    {
        // MAP_NORESERVE ensures we don't commit space until pages are touched
        block = (LwU8*)mmap(NULL, limit, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 0, 0);
    }

    ~trivial_memory() {
        munmap(block, (size_t)limit);
    }

    virtual LwBool test_address(LwU64 address) { return address >= base && (address - base) < limit; }

    virtual LwU8 read8(LwU64 addr) { 
        return block[addr - base]; 
    }

    virtual void write8(LwU64 addr, LwU8 value) 
    { 
        block[addr - base] = value; 
    }

    virtual LwU32 read32(LwU64 addr) { 
        return *(LwU32*)&block[addr - base]; 
    }

    virtual void write32(LwU64 addr, LwU32 value) 
    { 
        *(LwU32 *)&block[addr - base] = value; 
    }

    virtual LwU64 read64(LwU64 addr) { 
        return *(LwU64 *)&block[addr - base]; 
    }

    virtual void write64(LwU64 addr, LwU64 value) 
    { 
        *(LwU64 *)&block[addr - base] = value; 
    }        

    virtual const uint32_t get_memory_kind() { return kind; }
};

struct riscv_core_state
{
    LwU64 regs[32]               = {0};
    LwU64 programCounter = 0, pc = 0;
    LwU64 mstatus = 0;
    LwU64 satp    = 0;
#if LIBOS_CONFIG_MPU_HASHED
    LwU64 smpuhp  = 0;
#endif    
    LwU64 xie = 0, xip = 0;
    LwU64 xtvec       = 0;
    LwU64 xepc        = 0;
    LwU64 xcause      = 0;
    LwU64 xtval       = 0;
    LwU64 xscratch    = 0;
    LwU64 mucounteren = 0;
    LwU64 cycle = 0, mtimecmp = 0;
    LwU64 resolve_pc        = 0;
    LwBool stopped          = LW_FALSE;
    LwBool shutdown         = LW_FALSE;
    LwBool single_step_once = LW_FALSE;
    privilege_t privilege   = privilege_machine;
};

struct riscv_core : public riscv_core_state
{
    bool has_supervisor_mode = false;
    LwU64 elf_address;
    image *firmware;

    // Separation kernel
    LwU64 partitionIndex;

    struct {
        LwU64 entry;
        struct {
            LwU64 begin;
            LwU64 end;
            LwU32 pmpAccess;
        } pmp[16];
    } partition[64] = {0};

    // Used for simulating register thrashing from SBI
    std::mt19937_64 rand_sbi;

    std::vector<lwstom_memory *> memory_regions;

    event<void(LwU64 programCounter, LibosRiscvInstruction *)> event_instruction_decoded;   
    event<void(LwU64 cycle)> event_fast_forward;
    event<void()> event_instruction_retired, event_instruction_start, event_instruction_prestart;
    event<void(LwU64 new_xcause, LwU64 new_xtval)> event_trap;
    event<void(LwU64 reg)> event_register_written;

    void reset()
    {
        // reset the core state
        (riscv_core_state &)*this = riscv_core_state();

        if (has_supervisor_mode)
            privilege = privilege_supervisor;
        else
            privilege = privilege_machine;
    }

    enum TranslationKind {
      translateUserLoad = 1,
      translateUserStore = 2,
      translateUserExelwte = 4,
      
      translateSupervisorLoad = 8,
      translateSupervisorStore = 16,
      translateSupervisorExelwte = 32,

      /* Machine is only supported for turing, and is aliased to supervisor for now */
      translateMachineLoad = 8,
      translateMachineStore = 16,
      translateMachineExelwte = 32,

      translateNoMemoryBehindAllocation = 128
    };

    struct TlbEntry
    {   
        LwU64           virtualAddress;
        TranslationKind allowedMask;
        LwU64           physicalAddress;
        lwstom_memory * memory;
        LwU64           tlbVersion;
    };

    bool tlb_populate(TlbEntry & entry, LwU64 address);
    
    LwU64 tlbVersion = 0;

    enum { TlbLineSize = 128, TlbSize = 1024 };

    TlbEntry tlb[TlbSize];

    TlbEntry & tlb_line_for_address(LwU64 address) {
        return tlb[(address / TlbLineSize) & (TlbSize - 1)];
    }

    lwstom_memory *translate_or_fault(LwU64 address, TranslationKind kind, LwU64 &physical);

    lwstom_memory *memory_for_address(LwU64 address)
    {
        for (auto cm : memory_regions)
        {
            if (cm->test_address(address))
                return cm;
        }
        return NULL;
    }

    void print_address(LwU64 address);
    void print_register(LwU32 register);
    void print_out_register(LwU32 register);

    void set_elf_address(LwU64 elf_address) { this->elf_address = elf_address; }

    riscv_core(image *firmware) : firmware(firmware) {}

    void fast_forward();
    void trap(LwU64 new_xcause, LwU64 new_xtval);

    void write_register(LwU64 registerIndex, LwU64 newValue);
    void execute(LwU32 t);
    void sbi();
    virtual void soft_reset() = 0;

    LwU64 mtime() { return cycle >> 5; }

    virtual void step();
    virtual void update_interrupts() = 0;

    //
    // HALs
    //
    std::function<void(LwU32 addr, LwU64 value)> csr_write_HAL;
    std::function<LwU64(LwU32 addr)> csr_read_HAL;
    std::function<LwBool(LwU32 addr)> csr_validate_access_HAL;

    virtual LwBool translate_address(LwU64 va, LwU64 &pa, TranslationKind & mask) {
        pa = va;
        return LW_TRUE;
    }

    virtual void pmp_assert_partition_has_access(LwU64 pa, LwU64 size, TranslationKind kind) = 0;
};
#endif
