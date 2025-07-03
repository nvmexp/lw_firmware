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

#include "../kernel/riscv-isa.h"
#include "sdk/lwpu/inc/lwmisc.h"
#include <assert.h>
#include <fstream>
#include <memory.h>
#include <sdk/lwpu/inc/lwtypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <random>

#include "../debug/elf.h"

extern "C" {
#include "../debug/lines.h"
}

// HAL layer
#include "hal.h"

// load
enum opcode_t
{
    OP_UNIMP                  = 0,
    OP_LD                     = 3,
    OP_SD                     = 35,
    OP_FENCE                  = 15,
    OP_AUIPC                  = 23,
    OP_ARITHMETIC_IMMEDIATE   = 19,
    OP_ARITHMETIC_IMMEDIATE32 = 27,
    OP_ARITHMETIC             = 51,
    OP_ARITHMETIC32           = 59,
    OP_BRANCH                 = 99,
    OP_JAL                    = 111,
    OP_JALR                   = 103,
    OP_LUI                    = 55,
    OP_ELWIRONMENT            = 115
};

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
    LwU64 additional_heap;
    std::vector<LwU8> elf;
    libosDebugResolver resolver;

    image(const std::string &basename);
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
    std::vector<LwU8> block;
    uint32_t kind;

    trivial_memory(uint32_t kind, LwU64 base, const std::vector<LwU8> &block)
        : base(base), block(block), kind(kind)
    {
    }

    virtual LwBool test_address(LwU64 address) { return address >= base && (address - base) < block.size(); }

    virtual LwU8 read8(LwU64 addr) { return block[addr - base]; }
    virtual void write8(LwU64 addr, LwU8 value) { block[addr - base] = value; }
    virtual const uint32_t get_memory_kind() { return kind; }
};

enum csr_kind
{
    CSRRW,
    CSRRS,
    CSRRC,
    CSRRWI,
    CSRRSI,
    CSRRCI
};

enum branch_kind
{
    BEQ,
    BNE,
    BGE,
    BLT,
    BGEU,
    BLTU
};

enum load_kind
{
    LD,
    LW,
    LH,
    LB,
    LWU,
    LHU,
    LBU
};

enum store_kind
{
    SD,
    SW,
    SH,
    SB,
};

enum arithmetic_imm_kind
{
    ADDI,
    SLLI,
    SLTI,
    SLTIU,
    XORI,
    SRLI,
    SRAI,
    ORI,
    ANDI,
    ADDIW,
    SLLIW,
    SRLIW,
    SRAIW
};

enum arithmetic_kind
{
    ADDW,
    SUBW,
    MULW,
    SLLW,
    DIVW,
    SRLW,
    SRAW,
    DIVUW,
    REMW,
    REMUW,
    ADD,
    SUB,
    MUL,
    SLL,
    SLT,
    SLTU,
    DIV,
    XOR,
    SRL,
    SRA,
    DIVU,
    REM,
    OR,
    AND,
    REMU,
};

struct riscv_core_state
{
    bool  mpu_translation_enabled = false;
    LwU64 regs[32]            = {0};
    LwU64 lwrrent_instruction = 0, pc = 0;
    LwU64 mstatus = 0;
    LwU64 satp    = 0;
    LwU64 xie = 0, xip = 0;
    LwU64 xtvec       = 0;
    LwU64 xepc        = 0;
    LwU64 xcause      = 0;
    LwU64 xtval    = 0;
    LwU64 xscratch    = 0;
    LwU64 mucounteren = 0;
    LwU64 cycle = 0, mtimecmp = 0;
    LwU64 resolve_pc        = 0;
    LwBool stopped          = LW_FALSE;
    LwBool single_step_once = LW_FALSE;
    privilege_t privilege   = privilege_machine;
};

struct riscv_core : public riscv_core_state
{
    bool  has_supervisor_mode = false;
    LwU64 elf_address;
    image *firmware;
    
    // Used for simulating register thrashing from SBI
    std::mt19937_64 rand_sbi;    

    std::vector<lwstom_memory *> memory_regions;

    event<void(LwU64 cycle)> event_fast_forward;
    event<void()> event_mret;
    event<void()> event_instruction_retired, event_instruction_start, event_instruction_prestart;
    event<void()> event_ebreak;
    event<void()> event_unimp;
    event<void()> event_wfi;
    event<void()> event_ecall;
    event<void(csr_kind kind, LwU64 csr, LwU64 rd, LwU64 rs1)> event_csr;
    event<void(csr_kind kind, LwU64 csr, LwU64 rd, LwU64 imm)> event_csri;
    event<void(LwU64 rd, LwU64 imm)> event_lui, event_auipc;
    event<void(LwU64 rd, LwU64 rs1, LwS64 imm)> event_jalr;
    event<void(LwU64 rd, LwU64 imm)> event_jal;
    event<void(branch_kind kind, LwU64 rs1, LwU64 rs2, LwU64 target)> event_branch;
    event<void(LwU64 new_xcause, LwU64 new_xtval)> event_trap;

    event<void(arithmetic_imm_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm)> event_arith_immediate;
    event<void(arithmetic_kind kind, LwU64 rd, LwU64 rs1, LwU64 rs2)> event_arithmetic;
    event<void(load_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm)> event_load;
    event<void(store_kind kind, LwU64 rd, LwU64 rs1, LwS64 imm)> event_store;
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

    riscv_core(image *firmware)
        : firmware(firmware)
    {
    }

    void fast_forward();
    void trap(LwU64 new_xcause, LwU64 new_xtval);

    void execute(LwU32 t);

    LwU64 mtime() { return cycle >> 5; }

    virtual void step();
    virtual void update_interrupts() = 0;

    //
    // HALs
    //
    std::function<void(LwU32 addr, LwU64 value)> csr_write_HAL;
    std::function<LwU64(LwU32 addr)> csr_read_HAL;
    std::function<LwBool(LwU32 addr)> csr_validate_access_HAL;

    std::function<LwBool(LwU64 va, LwU64 &physical, LwBool write, LwBool execute, LwBool override_selwrity)>
        translate_address_HAL;
};
#endif
