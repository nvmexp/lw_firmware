/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "../kernel/riscv-isa.h"
#include "sdk/lwpu/inc/lwmisc.h"
#include "sdk/lwpu/inc/lwtypes.h"
#include <assert.h>
#include <cstring>
#include <drivers/common/inc/hwref/ampere/ga102/dev_gsp_riscv_csr_64.h>
#include <drivers/common/inc/swref/ampere/ga102/dev_riscv_csr_64_addendum.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory.h>
#include <sdk/lwpu/inc/lwtypes.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "../debug/elf.h"
extern "C" {
#include "../debug/lines.h"
#include "../include/libos_loader.h"
}
#include "riscv-core.h"

LwU64 sext32(LwS32 t) { return (LwU64)((LwS64)t); }
LwU64 sext32(LwU32 t) { return sext32((LwS32)t); }

image::image(const std::string &basename)
{
    // Load the ELF
    std::ifstream filestream((basename + ".elf").c_str() /* gcc-5 workaround */, std::ios::binary);
    if (filestream.fail())
    {
        fprintf(stderr, "Unable to open input %s.elf\n", basename.c_str());
        throw std::exception();
    }

    std::copy(
        std::istreambuf_iterator<char>(filestream), std::istreambuf_iterator<char>(),
        std::back_inserter(elf));

    // Verify the LIBOS header and callwlate heap footprint
    if (!libosElfGetStaticHeapSize((elf64_header *)(&elf[0]), &additional_heap) ||
        !libosElfGetBootEntry((elf64_header *)(&elf[0]), &entry_offset))
    {
        fprintf(stderr, "%s.elf: Not a LIBOS ELF. (corrupt LIBOS header or entry point)\n", basename.c_str());
        throw std::exception();
    }

    // Initialize the symbol / line decoder
    libosDebugResolverConstruct(&resolver, (elf64_header *)(&elf[0]));
}

LwU16 lwstom_memory::read16(LwU64 addr)
{
    assert((addr & 1) == 0);
    return read8(addr) | (read8(addr + 1) << 8);
}
LwU32 lwstom_memory::read32(LwU64 addr)
{
    assert((addr & 3) == 0);
    return read16(addr) | (read16(addr + 2) << 16);
}
LwU64 lwstom_memory::read64(LwU64 addr)
{
    assert((addr & 7) == 0);
    return read32(addr) | (((LwU64)read32(addr + 4)) << 32);
}

void lwstom_memory::write16(LwU64 addr, LwU16 value)
{
    assert((addr & 1) == 0);
    write8(addr, value & 0xFF);
    write8(addr + 1, (value >> 8) & 0xFF);
}

void lwstom_memory::write32(LwU64 addr, LwU32 value)
{
    assert((addr & 1) == 0);
    write16(addr, value & 0xFFFF);
    write16(addr + 2, (value >> 16) & 0xFFFF);
}

void lwstom_memory::write64(LwU64 addr, LwU64 value)
{
    assert((addr & 1) == 0);
    write32(addr, value & 0xFFFFFFFF);
    write32(addr + 4, (value >> 32) & 0xFFFFFFFF);
}

void riscv_core::trap(LwU64 new_xcause, LwU64 new_xtval)
{
    event_trap(new_xcause, new_xtval);

    if (!xtvec) {
        fprintf(stderr, "No trap handler, stopping core.\n");
        stopped = true;
    }
    xepc                = lwrrent_instruction;
    lwrrent_instruction = pc = xtvec;
    xcause                   = new_xcause;

    if (has_supervisor_mode)
    {
        mstatus                  = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _SPP, privilege, mstatus);
        privilege                = privilege_supervisor;
        LwU32 sie                = DRF_VAL(_RISCV, _CSR_MSTATUS, _SIE, mstatus);
        mstatus                  = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _SPIE, sie, mstatus);
        mstatus                  = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _SIE, 0, mstatus);
    }
    else
    {
        mstatus                  = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _MPP, privilege, mstatus);
        privilege                = privilege_machine;
        LwU32 xie                = DRF_VAL(_RISCV, _CSR_MSTATUS, _MIE, mstatus);
        mstatus                  = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _MPIE, xie, mstatus);
        mstatus                  = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _MIE, 0, mstatus);
    }

    xtval                 = new_xtval;
}

// The core has signaled it's about to enter an infinite wait loop or a WFI
// We should advance the core until the next interrupt/trap (in this case timer)
void riscv_core::fast_forward()
{
    update_interrupts();
    
    if (DRF_VAL(_RISCV, _CSR_SIP, _SEIP, xip) ||
        DRF_VAL(_RISCV, _CSR_MIP, _MEIP, xip))
        return;

    if (mtime() < mtimecmp)
    {
        event_fast_forward(mtimecmp << 5);
        cycle = mtimecmp << 5;
        assert(mtime() == mtimecmp);
    }
}

void riscv_core::execute(LwU32 opcode)
{
    LwU32 mpie;
    LwS64 imm;
    LwU64 rs1, rs2, rd, top6, top7, shamt, target, csr, scratch;
    lwstom_memory *memory;

    switch (opcode & 0x7F)
    {
    case OP_ELWIRONMENT:
        csr = opcode >> 20, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
        imm = rs1;
        switch ((opcode >> 12) & 7)
        {
        case 0: // ECALL/EBREAK/MRET/...
            switch (opcode)
            {
            case 0x30200073: // MRET
                assert(privilege == privilege_machine);
                event_mret();
                pc        = xepc;
                privilege = (privilege_t)DRF_VAL64(_RISCV, _CSR_MSTATUS, _MPP, mstatus);
                assert(privilege <= privilege_machine);
                if (privilege == privilege_supervisor)
                    assert(has_supervisor_mode);
                mpie      = DRF_VAL64(_RISCV, _CSR_MSTATUS, _MPIE, mstatus);
                mstatus   = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _MIE, mpie, mstatus);
                break;

            case 0x10200073: // SRET
                assert(privilege == privilege_supervisor && has_supervisor_mode);
                event_mret();
                pc        = xepc;
                privilege = (privilege_t)DRF_VAL64(_RISCV, _CSR_MSTATUS, _SPP, mstatus);
                assert(privilege <= privilege_supervisor);
                mpie      = DRF_VAL64(_RISCV, _CSR_MSTATUS, _SPIE, mstatus);
                mstatus   = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _SIE, mpie, mstatus);
                break;

            case 0x00100073:
                event_ebreak();
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _BKPT), 0);
                break;

            case 0x10500073:
                event_wfi();
                fast_forward();
                break;

            case 0x00000073:
                event_ecall();
                if (privilege == privilege_user)
                    trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _UCALL), 0);
                else if (privilege == privilege_supervisor) 
                {
                    /*
                        RISC-V SBI for supervisor ecall
                        https://github.com/riscv/riscv-sbi-doc/blob/master/riscv-sbi.adoc
                    */
                   uint64_t sbi_extension_id = regs[RISCV_A7];
                   uint64_t sbi_function_id = regs[RISCV_A6];

                   // sbi_set_timer
                   if (sbi_extension_id == 0 && sbi_function_id == 0) 
                   {
                        mtimecmp = regs[RISCV_A0];

                        if (has_supervisor_mode)
                            xip &= ~REF_NUM64(LW_RISCV_CSR_SIP_STIP, 1);
                        else
                            xip &= ~REF_NUM64(LW_RISCV_CSR_MIP_MTIP, 1);      

                        regs[RISCV_A0] = 0 /* SBI_SUCCESS */;                 
                   }
                   else
                   {
                       assert(0 && "Unsupported SBI");
                       regs[RISCV_A0] = -2 /* SBI_NOT_SUPPORTED */;                 
                   }

                   // Scrub temporary registers that SBI is allowed to overwrite
                   regs[RISCV_T0] = rand_sbi();
                   regs[RISCV_T1] = rand_sbi();
                   regs[RISCV_T2] = rand_sbi();
                   regs[RISCV_T3] = rand_sbi();
                   regs[RISCV_T4] = rand_sbi();
                   regs[RISCV_T5] = rand_sbi();
                   regs[RISCV_T6] = rand_sbi();
                   regs[RISCV_A1] = rand_sbi();
                   regs[RISCV_A2] = rand_sbi();
                   regs[RISCV_A3] = rand_sbi();
                   regs[RISCV_A4] = rand_sbi();
                   regs[RISCV_A5] = rand_sbi();
                   regs[RISCV_A6] = rand_sbi();                   
                   regs[RISCV_A7] = rand_sbi();                   
                   regs[RISCV_RA] = rand_sbi();                                     
                }
                break;

            default:
                assert(0);
            }
            break;

        case 1: // CSRRW
            event_csr(CSRRW, csr, rd, rs1);
            if (csr_validate_access_HAL(csr))
            {
                scratch = csr_read_HAL(csr);
                csr_write_HAL(csr, regs[rs1]);
                regs[rd] = scratch;
                event_register_written(rd);
            }
            else
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            break;

        case 2: // CSRRS
            event_csr(CSRRS, csr, rd, rs1);
            if (csr_validate_access_HAL(csr))
            {
                scratch = csr_read_HAL(csr);
                if (regs[rs1])
                    csr_write_HAL(csr, scratch | regs[rs1]);
                regs[rd] = scratch;
                event_register_written(rd);
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }
            break;

        case 3: // CSRRC
            event_csr(CSRRC, csr, rd, rs1);
            if (csr_validate_access_HAL(csr))
            {
                scratch = csr_read_HAL(csr);
                if (regs[rs1])
                    csr_write_HAL(csr, scratch & ~regs[rs1]);
                regs[rd] = scratch;
                event_register_written(rd);
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }
            break;

        case 5: // CSRRWI
            event_csri(CSRRWI, csr, rd, imm);
            if (csr_validate_access_HAL(csr))
            {
                scratch = csr_read_HAL(csr);
                csr_write_HAL(csr, imm);
                regs[rd] = scratch;
                event_register_written(rd);
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }
            break;

        case 6: // CSRRSI
            event_csri(CSRRSI, csr, rd, imm);
            if (csr_validate_access_HAL(csr))
            {
                scratch = csr_read_HAL(csr);
                csr_write_HAL(csr, scratch | (LwU64)imm);
                regs[rd] = scratch;
                event_register_written(rd);
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }
            break;

        case 7: // CSRRCI
            event_csri(CSRRCI, csr, rd, imm);
            if (csr_validate_access_HAL(csr))
            {
                scratch = csr_read_HAL(csr);
                csr_write_HAL(csr, scratch & ~(LwU64)imm);
                regs[rd] = scratch;
                event_register_written(rd);
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }
            break;

        default:
            assert(0);
        }
        break;

    case OP_LUI:
        rd = (opcode >> 7) & 31, imm = (((LwS32)opcode) >> 12) << 12;
        event_lui(rd, (LwU64)imm);
        regs[rd] = imm;
        event_register_written(rd);
        break;

    case OP_JALR:
        assert(((opcode >> 12) & 7) == 0);
        rd = (opcode >> 7) & 31, rs1 = (opcode >> 15) & 31, imm = ((LwS32)opcode) >> 20;
        event_jalr(rd, rs1, imm);
        target   = regs[rs1] + imm;
        regs[rd] = pc;
        event_register_written(rd);
        pc = target;
        break;

    case OP_AUIPC:
        rd     = (opcode >> 7) & 31;
        target = lwrrent_instruction + ((((LwS32)opcode) >> 12) << 12);
        event_auipc(rd, target);
        regs[rd] = target;
        event_register_written(rd);
        break;

    case OP_JAL:
        rd = (opcode >> 7) & 31;
        // imm[20|10:1|11|19:12
        imm = ((LwS32)(
                  (opcode & 0x80000000) | ((opcode & 0xFF000) << 11) | ((opcode & 0x00100000) << 2) |
                  ((opcode & 0x7FE00000) >> 9))) >>
              12;
        target = lwrrent_instruction + imm * 2;
        event_jal(rd, target);
        regs[rd] = pc;
        event_register_written(rd);

        // Is this an infinite loop? If so, fast forward until the next trap
        // @note This reduces testing time drastically when the idle task is hit (or we panic).
        if (lwrrent_instruction == target)
            fast_forward();

        pc = target;
        break;

    case OP_BRANCH:
        // Genius: The immediate is swizzled to high heaven

        imm = ((LwS32)(
                  ((opcode & 0x7E000000) >> 1) | (opcode & 0x80000000) | ((opcode & 0x80) << (30 - 7)) |
                  ((opcode & 0xF00) << 12))) >>
              19;

        rs1 = (opcode >> 15) & 31, rs2 = (opcode >> 20) & 31;
        target = lwrrent_instruction + imm;
        switch ((opcode >> 12) & 7)
        {
        case 0: // BEQ
            event_branch(BEQ, rs1, rs2, target);
            if (regs[rs1] == regs[rs2])
                pc = target;
            break;

        case 1: // BNE
            event_branch(BNE, rs1, rs2, target);
            if (regs[rs1] != regs[rs2])
                pc = target;
            break;

        case 4: // BLT
            event_branch(BLT, rs1, rs2, target);
            if ((LwS64)regs[rs1] < (LwS64)regs[rs2])
                pc = target;
            break;

        case 5: // BGE
            event_branch(BGE, rs1, rs2, target);
            if ((LwS64)regs[rs1] >= (LwS64)regs[rs2])
                pc = target;
            break;

        case 6: // BLTU
            event_branch(BLTU, rs1, rs2, target);
            if ((LwU64)regs[rs1] < (LwU64)regs[rs2])
                pc = target;
            break;

        case 7: // BGEU
            event_branch(BGEU, rs1, rs2, target);
            if ((LwU64)regs[rs1] >= (LwU64)regs[rs2])
                pc = target;
            break;
        }
        break;

    case OP_LD:
        imm = ((LwS32)opcode) >> 20, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
        switch ((opcode >> 12) & 7)
        {
        case 0: // LB
            event_load(LB, rd, rs1, imm);
            break;
        case 1: // LH
            event_load(LH, rd, rs1, imm);
            break;
        case 2: // LW
            event_load(LW, rd, rs1, imm);
            break;
        case 3: // LD
            event_load(LD, rd, rs1, imm);
            break;
        case 4: // LBU
            event_load(LBU, rd, rs1, imm);
            break;
        case 5: // LHU
            event_load(LHU, rd, rs1, imm);
            break;
        case 6: // LWU
            event_load(LWU, rd, rs1, imm);
            break;

        default:
            assert(0);
        }

        if (!translate_address_HAL(imm + regs[rs1], scratch, LW_FALSE, LW_FALSE, LW_FALSE))
        {
            // Newer hardware raises LPAGE_FAULT on MPU translation failure
            if (has_supervisor_mode)
              trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _LPAGE_FAULT), imm + regs[rs1]);
            else
              trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _LACC_FAULT), imm + regs[rs1]);
            break;
        }
        if (!(memory = memory_for_address(scratch)))
        {
            // @todo: We raise a LACC_FAULT when the MPU translation suceeds but the physical address is bad
            trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _LACC_FAULT), imm + regs[rs1]);
            break;
        }
        switch ((opcode >> 12) & 7)
        {
        case 0: // LB
            regs[rd] = (int8_t)memory->read8(scratch);
            break;
        case 1: // LH
            regs[rd] = (int16_t)memory->read16(scratch);
            break;
        case 2: // LW
            regs[rd] = (LwS32)memory->read32(scratch);
            break;
        case 3: // LD
            regs[rd] = (LwS64)memory->read64(scratch);
            break;
        case 4: // LBU
            regs[rd] = (LwU8)memory->read8(scratch);
            break;
        case 5: // LHU
            regs[rd] = (LwU16)memory->read16(scratch);
            break;
        case 6: // LWU
            regs[rd] = (LwU32)memory->read32(scratch);
            break;

        default:
            assert(0);
        }
        event_register_written(rd);
        break;

    case OP_SD:
        imm = ((LwS32)(((opcode & 0xF80) << 13) | (opcode & 0xFE000000))) >> 20;
        rs1 = (opcode >> 15) & 31, rs2 = (opcode >> 20) & 31;

        switch ((opcode >> 12) & 7)
        {
        case 0: // SB
            event_store(SB, rs1, rs2, imm);
            break;
        case 1: // SH
            event_store(SH, rs1, rs2, imm);
            break;
        case 2: // SW
            event_store(SW, rs1, rs2, imm);
            break;
        case 3: // SD
            event_store(SD, rs1, rs2, imm);
            break;
        default:
            assert(0);
        }

        if (!translate_address_HAL(imm + regs[rs1], scratch, LW_TRUE, LW_FALSE, LW_FALSE))
        {
            if (has_supervisor_mode)
              trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _SPAGE_FAULT), imm + regs[rs1]);
            else
              trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _SACC_FAULT), imm + regs[rs1]);
            break;
        }
        if (!(memory = memory_for_address(scratch)))
        {
            // Physical error
            trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _SACC_FAULT), imm + regs[rs1]);
            break;
        }
        switch ((opcode >> 12) & 7)
        {
        case 0: // SB
            memory->write8(scratch, regs[rs2]);
            break;
        case 1: // SH
            memory->write16(scratch, regs[rs2]);
            break;
        case 2: // SW
            memory->write32(scratch, regs[rs2]);
            break;
        case 3: // SD
            memory->write64(scratch, regs[rs2]);
            break;
        default:
            assert(0);
        }
        break;

    case OP_ARITHMETIC_IMMEDIATE:
        imm = ((LwS32)opcode) >> 20, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
        shamt = (opcode >> 20) & 63;
        top7  = opcode >> (32 - 7);
        top6  = opcode >> (32 - 6);
        switch ((opcode >> 12) & 7)
        {
        case 0: // ADDI
            event_arith_immediate(ADDI, rd, rs1, imm);
            regs[rd] = regs[rs1] + imm;
            break;

        case 1: // SLLI
            assert(top6 == 0);
            event_arith_immediate(SLLI, rd, rs1, imm);
            regs[rd] = regs[rs1] << shamt;
            break;

        case 2: // SLTI
            event_arith_immediate(SLTI, rd, rs1, imm);
            regs[rd] = (LwS64)regs[rs1] < imm;
            break;

        case 3: // SLTIU
            event_arith_immediate(SLTIU, rd, rs1, imm);
            regs[rd] = (LwU64)regs[rs1] < (LwU64)imm;
            break;

        case 4: // XORI
            event_arith_immediate(XORI, rd, rs1, imm);
            regs[rd] = regs[rs1] ^ imm;
            break;

        case 5:
            if (top6 == 0) // SRLI
            {
                event_arith_immediate(SRLI, rd, rs1, imm);
                regs[rd] = (LwU64)regs[rs1] >> shamt;
            }
            else if (top6 == 0x10) // SRAI
            {
                event_arith_immediate(SRAI, rd, rs1, imm);
                regs[rd] = (LwS64)regs[rs1] >> shamt;
            }
            else
                assert(0);
            break;

        case 6: // ORI
            event_arith_immediate(ORI, rd, rs1, imm);
            regs[rd] = regs[rs1] | imm;
            break;

        case 7: // ANDI
            event_arith_immediate(ANDI, rd, rs1, imm);
            regs[rd] = regs[rs1] & imm;
            break;
        }
        event_register_written(rd);
        break;

    case OP_ARITHMETIC_IMMEDIATE32:
        imm = ((LwS32)opcode) >> 20, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
        shamt = (opcode >> 20) & 63;
        top7  = opcode >> (32 - 7);
        top6  = opcode >> (32 - 6);
        switch ((opcode >> 12) & 7)
        {
        case 0: // ADDIW
            event_arith_immediate(ADDIW, rd, rs1, imm);
            regs[rd] = sext32((LwU32)regs[rs1] + (LwS32)imm);
            break;

        case 1: // SLLIW
            assert(top7 == 0);
            event_arith_immediate(SLLIW, rd, rs1, imm);
            regs[rd] = sext32((LwU32)regs[rs1] << shamt);
            break;

        case 5:
            if (top7 == 0) // SRLIW
            {
                event_arith_immediate(SRLIW, rd, rs1, imm);
                regs[rd] = sext32((LwU32)regs[rs1] >> shamt);
            }
            else if (top7 == 0x20) // SRAIW
            {
                event_arith_immediate(SRAIW, rd, rs1, imm);
                regs[rd] = sext32((LwS32)regs[rs1] >> shamt);
            }
            else
                assert(0);
            break;

        default:
            assert(0);
        }
        event_register_written(rd);
        break;

    /*
          RV64I extension adds dedicated 32-bit operand instructions
    */
    case OP_ARITHMETIC32:
        top7 = opcode >> (32 - 7);
        rs2 = (opcode >> 20) & 31, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
        shamt = (opcode >> 20) & 63;
        switch ((opcode >> 12) & 7)
        {
        case 0:
            if (top7 == 0) // ADDW
            {
                event_arithmetic(ADDW, rd, rs1, rs2);
                regs[rd] = sext32((LwU32)regs[rs1] + (LwU32)regs[rs2]);
                ;
            }
            else if (top7 == 0x20) // SUBW
            {
                event_arithmetic(SUBW, rd, rs1, rs2);
                regs[rd] = sext32((LwU32)regs[rs1] - (LwU32)regs[rs2]);
                ;
            }
            else if (top7 == 0x01) // MULW
            {
                event_arithmetic(MULW, rd, rs1, rs2);
                regs[rd] = sext32((LwU32)regs[rs1] * (LwU32)regs[rs2]);
            }
            else
            {
                assert(0);
            }
            break;

        case 1: // SLLW
            assert(top7 == 0);
            event_arithmetic(SLLW, rd, rs1, rs2);
            regs[rd] = sext32((LwU32)(regs[rs1] << (regs[rs2] & 63)));
            break;

        case 4:
            if (top7 == 0x01) // DIVW
            {
                event_arithmetic(DIVW, rd, rs1, rs2);
                if (!(LwU32)regs[rs2])
                    regs[rd] = -1;
                else if ((LwU32)regs[rs1] == 0x80000000 && (LwU32)regs[rs2] == 0xFFFFFFFF)
                    regs[rd] = sext32(0x80000000);
                else
                    regs[rd] = sext32((LwS32)regs[rs1] / (LwS32)regs[rs2]);
            }
            else
                assert(0);
            break;

        case 5:
            if (top7 == 0) // SRLW
            {
                event_arithmetic(SRLW, rd, rs1, rs2);
                regs[rd] = sext32((LwU32)((LwU64)regs[rs1] >> (regs[rs2] & 63)));
            }
            else if (top7 == 0x20) // SRAW
            {
                event_arithmetic(SRAW, rd, rs1, rs2);
                regs[rd] = sext32((LwS32)((LwS64)regs[rs1] >> (regs[rs2] & 63)));
            }
            else if (top7 == 0x01) // DIVUW
            {
                event_arithmetic(DIVUW, rd, rs1, rs2);
                if ((LwU32)regs[rs2])
                    regs[rd] = sext32((LwU32)regs[rs1] / (LwU32)regs[rs2]);
                else
                    regs[rd] = -1;
            }
            else
                assert(0);
            break;

        case 6:
            if (top7 == 0x01) // REMW
            {
                event_arithmetic(REMW, rd, rs1, rs2);
                if (!(LwU32)regs[rs2])
                    regs[rd] = sext32((LwU32)regs[rs1]);
                else if ((LwU32)regs[rs1] == 0x80000000 && (LwU32)regs[rs2] == 0xFFFFFFFF)
                    regs[rd] = 0;
                else
                    regs[rd] = sext32((LwS32)regs[rs1] % (LwS32)regs[rs2]);
            }
            else
                assert(0);
            break;

        case 7:
            if (top7 == 0x01) // REMUW
            {
                event_arithmetic(REMUW, rd, rs1, rs2);
                if ((LwU32)regs[rs2])
                    regs[rd] = sext32((LwU32)regs[rs1] % (LwU32)regs[rs2]);
                else
                    regs[rd] = sext32((LwU32)regs[rs1]);
            }
            else
                assert(0);
            break;

        default:
            assert(0);
        }
        event_register_written(rd);
        break;

    case OP_ARITHMETIC:
        top7 = opcode >> (32 - 7);
        rs2 = (opcode >> 20) & 31, rs1 = (opcode >> 15) & 31, rd = (opcode >> 7) & 31;
        shamt = (opcode >> 20) & 63;
        switch ((opcode >> 12) & 7)
        {
        case 0:
            if (top7 == 0) // ADD
            {
                event_arithmetic(ADD, rd, rs1, rs2);
                regs[rd] = regs[rs1] + regs[rs2];
            }
            else if (top7 == 0x20) // SUB
            {
                event_arithmetic(SUB, rd, rs1, rs2);
                regs[rd] = regs[rs1] - regs[rs2];
            }
            else if (top7 == 0x01) // MUL
            {
                event_arithmetic(MUL, rd, rs1, rs2);
                regs[rd] = regs[rs1] * regs[rs2];
            }
            else
            {
                assert(0);
            }
            break;

        case 1: // SLL
            assert(top7 == 0);
            event_arithmetic(SLL, rd, rs1, rs2);
            regs[rd] = regs[rs1] << (regs[rs2] & 63);
            break;

        case 2: // SLT
            assert(top7 == 0);
            event_arithmetic(SLT, rd, rs1, rs2);
            regs[rd] = (LwS64)regs[rs1] < (LwS64)regs[rs2];
            break;

        case 3: // SLTU
            assert(top7 == 0);
            event_arithmetic(SLTU, rd, rs1, rs2);
            regs[rd] = (LwU64)regs[rs1] < (LwU64)regs[rs2];
            break;

        case 4:
            if (top7 == 0x01) // DIV
            {
                event_arithmetic(DIV, rd, rs1, rs2);
                if (!regs[rs2])
                    regs[rd] = -1;
                else if (regs[rs1] == 0x8000000000000000ULL && regs[rs2] == 0xFFFFFFFF)
                    regs[rd] = 0x8000000000000000ULL;
                else
                    regs[rd] = (LwS64)regs[rs1] / (LwS64)regs[rs2];
            }
            else if (top7 == 0) // XOR
            {
                event_arithmetic(XOR, rd, rs1, rs2);
                regs[rd] = regs[rs1] ^ regs[rs2];
            }
            else
                assert(0);
            break;

        case 5:
            if (top7 == 0) // SRL
            {
                event_arithmetic(SRL, rd, rs1, rs2);
                regs[rd] = (LwU64)regs[rs1] >> (regs[rs2] & 63);
            }
            else if (top7 == 0x20) // SRA
            {
                event_arithmetic(SRA, rd, rs1, rs2);
                regs[rd] = (LwS64)regs[rs1] >> (regs[rs2] & 63);
            }
            else if (top7 == 0x01) // DIVU
            {
                event_arithmetic(DIVU, rd, rs1, rs2);
                if (regs[rs2])
                    regs[rd] = regs[rs1] / regs[rs2];
                else
                    regs[rd] = -1;
            }
            else
                assert(0);
            break;

        case 6:
            if (top7 == 0x01) // REM
            {
                event_arithmetic(REM, rd, rs1, rs2);
                if (!regs[rs2])
                    regs[rd] = regs[rs1];
                else if (regs[rs1] == 0x8000000000000000ULL && regs[rs2] == 0xFFFFFFFF)
                    regs[rd] = 0;
                else
                    regs[rd] = (LwS64)regs[rs1] % (LwS64)regs[rs2];
            }
            else if (top7 == 0)
            { // OR
                event_arithmetic(OR, rd, rs1, rs2);
                regs[rd] = regs[rs1] | regs[rs2];
            }
            else
                assert(0);
            break;

        case 7:
            if (top7 == 0) // AND
            {
                event_arithmetic(AND, rd, rs1, rs2);
                regs[rd] = regs[rs1] & regs[rs2];
            }
            else if (top7 == 1) // REMU
            {
                if (regs[rs2])
                    regs[rd] = regs[rs1] % regs[rs2];
                else
                    regs[rd] = regs[rs1];
            }
            break;
        }
        event_register_written(rd);
        break;

    case OP_FENCE:
        // TODO: Verification strategy for fences in general
        assert(opcode == 0x0330000F); // fence rw,rw
        break;

    case OP_UNIMP:
        event_unimp();
        trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
        break;

    default:
        event_unimp();
        trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
        assert(0 && "Missing instruction emulation");
        break;
    }
    event_instruction_retired();
}

void riscv_core::step()
{
    LwU64 scratch;
    lwrrent_instruction = pc;

    event_instruction_prestart();

    if (stopped)
        return;

    if (single_step_once)
        stopped = LW_TRUE, single_step_once = LW_FALSE;

    update_interrupts();

    // Check for timer
    if (has_supervisor_mode)
    {
      if (mtime() >= mtimecmp && !DRF_VAL(_RISCV, _CSR_SIP, _STIP, xip))
      {
          xip |= REF_NUM64(LW_RISCV_CSR_SIP_STIP, 1);
      }

      if (DRF_VAL(_RISCV, _CSR_MIP, _STIP, xip) && DRF_VAL(_RISCV, _CSR_SIE, _STIE, xie) &&
          DRF_VAL(_RISCV, _CSR_MSTATUS, _SIE, mstatus))
      {
          // Set the MTIP bit (cleared on writed to mtimecmp)
          // If MTIE and MIE are enabled, then issue a trap
          trap(REF_NUM64(LW_RISCV_CSR_MCAUSE_INT, 1) | LW_RISCV_CSR_MCAUSE_EXCODE_S_TINT, 0);

          return;
      }

      // External interrupt pending
      if (DRF_VAL(_RISCV, _CSR_MIP, _SEIP, xip) && DRF_VAL(_RISCV, _CSR_SIE, _SEIE, xie) &&
          DRF_VAL(_RISCV, _CSR_MSTATUS, _SIE, mstatus))
      {
          trap(REF_NUM64(LW_RISCV_CSR_MCAUSE_INT, 1) | LW_RISCV_CSR_MCAUSE_EXCODE_S_EINT, 0);

          return;
      }

    }
    else
    {
      if (mtime() >= mtimecmp && !DRF_VAL(_RISCV, _CSR_MIP, _MTIP, xip))
      {
          xip |= REF_NUM64(LW_RISCV_CSR_MIP_MTIP, 1);
      }

      if (DRF_VAL(_RISCV, _CSR_MIP, _MTIP, xip) && DRF_VAL(_RISCV, _CSR_MIE, _MTIE, xie) &&
          DRF_VAL(_RISCV, _CSR_MSTATUS, _MIE, mstatus))
      {
          // Set the MTIP bit (cleared on writed to mtimecmp)
          // If MTIE and MIE are enabled, then issue a trap
          trap(REF_NUM64(LW_RISCV_CSR_MCAUSE_INT, 1) | LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT, 0);

          return;
      }

      // External interrupt pending
      if (DRF_VAL(_RISCV, _CSR_MIP, _MEIP, xip) && DRF_VAL(_RISCV, _CSR_MIE, _MEIE, xie) &&
          DRF_VAL(_RISCV, _CSR_MSTATUS, _MIE, mstatus))
      {
          trap(REF_NUM64(LW_RISCV_CSR_MCAUSE_INT, 1) | LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT, 0);

          return;
      }      
    }


    // Print a line comment if this is an exact match for a new line
    // or we just took a branch
    event_instruction_start();

    lwstom_memory *memory;
    if (!translate_address_HAL(lwrrent_instruction, scratch, LW_FALSE, LW_TRUE, LW_FALSE) ||
        !(memory = memory_for_address(scratch)))
    {
        if (has_supervisor_mode)
          trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _IPAGE_FAULT), lwrrent_instruction);
        else
          trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _IACC_FAULT), lwrrent_instruction);

        if (!translate_address_HAL(pc, scratch, LW_FALSE, LW_TRUE, LW_FALSE))
            assert(0 && "double fault");
        return;
    }

    LwU32 op = memory->read32(scratch);

    pc = pc + 4;
    execute(op);
    regs[0] = 0;
    cycle++;
}
