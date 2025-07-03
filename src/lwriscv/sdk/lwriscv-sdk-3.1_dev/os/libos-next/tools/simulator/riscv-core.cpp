/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "libriscv.h"
#include "libos-config.h"
#include "peregrine-headers.h"
#include "lwmisc.h"
#include "lwtypes.h"
#include <assert.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <memory>

extern "C" {
#include "libelf.h"
#include "libdwarf.h"
}
#include "riscv-core.h"

LwU64 sext32(LwS32 t) { return (LwU64)((LwS64)t); }

LwU64 sext32(LwU32 t) { return sext32((LwS32)t); }

LwS32 sext32(LwS32 t, LwU32 width) { return (t << (32 - width)) >> (32 - width); }

image::image(const std::string &basename)
{
    // Load the ELF
    std::ifstream filestream((basename).c_str() /* gcc-5 workaround */, std::ios::binary);

    if (filestream.fail())
    {
        fprintf(stderr, "Unable to open input %s.elf\n", basename.c_str());
        throw std::exception();
    }

    std::copy(
        std::istreambuf_iterator<char>(filestream), std::istreambuf_iterator<char>(),
        std::back_inserter(elf));

#if LIBOS_TINY
    if (LibosOk != LibosElfImageConstruct(&elfImage,  &elf[0], elf.size()) || 
        !LibosTinyElfGetBootEntry(&elfImage, &entry_offset))
    {
        fprintf(stderr, "%s.elf: Not a LIBOS ELF. (corrupt LIBOS header or entry point)\n", basename.c_str());
        throw std::exception();
    }

    // Initialize the symbol / line decoder
    LibosDebugResolverConstruct(&resolver, &elfImage);

#endif
}

void image::load_symbols(const std::string & name)
{
    std::ifstream filestream(name.c_str() /* gcc-5 workaround */, std::ios::binary);

    if (filestream.fail())
    {
        fprintf(stderr, "Unable to open input %s\n", name.c_str());
        throw std::exception();
    }

    std::shared_ptr<std::vector<LwU8> > elf = std::make_shared<std::vector<LwU8> >();

    std::copy(
        std::istreambuf_iterator<char>(filestream), std::istreambuf_iterator<char>(),
        std::back_inserter(*elf));    

    LibosElfImage image;
    if (LibosOk != LibosElfImageConstruct(&image, &(*elf)[0], elf->size()))
    {
        fprintf(stderr, "%s.elf: Not a LIBOS ELF. \n", name.c_str());
        throw std::exception();
    }
    symbols.push_back(elf);

    LibosDebugResolverConstruct(&resolver, &image);
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

    if (!xtvec)
    {
        fflush(stdout);
        fprintf(stderr, "\nNo trap handler, stopping core.\n");
        stopped = true;
    }
    xepc                = programCounter;
    programCounter = pc = xtvec;
    xcause                   = new_xcause;

    if (has_supervisor_mode)
    {
        mstatus   = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _SPP, privilege, mstatus);
        privilege = privilege_supervisor;
        LwU32 sie = DRF_VAL(_RISCV, _CSR_MSTATUS, _SIE, mstatus);
        mstatus   = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _SPIE, sie, mstatus);
        mstatus   = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _SIE, 0, mstatus);
    }
    else
    {
        mstatus   = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _MPP, privilege, mstatus);
        privilege = privilege_machine;
        LwU32 xie = DRF_VAL(_RISCV, _CSR_MSTATUS, _MIE, mstatus);
        mstatus   = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _MPIE, xie, mstatus);
        mstatus   = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _MIE, 0, mstatus);
    }

    xtval = new_xtval;
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

bool riscv_core::tlb_populate(TlbEntry & entry, LwU64 address)
{
    LwU64 physicalAddress;
    TranslationKind allowedMask;
    lwstom_memory *memory;

    address = address &~ (TlbLineSize-1);

    // Perform VA to PA translation
    if (!translate_address(address, physicalAddress, allowedMask))
        return false;

    // We ensure we don't map a line with more permissions than the PMP allows
    // real hardware is more conservative
    pmp_assert_partition_has_access(physicalAddress, TlbLineSize, allowedMask);


    // Find the backing memory object for the physical address
    if (!(memory = memory_for_address(physicalAddress)))
        allowedMask = translateNoMemoryBehindAllocation;

    entry.virtualAddress = address;
    entry.physicalAddress = physicalAddress;
    entry.memory = memory;
    entry.allowedMask = allowedMask;
    entry.tlbVersion = tlbVersion;
    return true;
}

lwstom_memory *riscv_core::translate_or_fault(LwU64 address, TranslationKind kind, LwU64 &physical)
{
    TlbEntry & entry = tlb_line_for_address(address);
    if (entry.virtualAddress != (address &~ (TlbLineSize-1)) || entry.tlbVersion != tlbVersion) 
        if (!tlb_populate(entry, address))
            goto fault;
    
    if (entry.allowedMask & kind) 
    {
        physical = entry.physicalAddress + (address & (TlbLineSize - 1));
        return entry.memory;
    }
    else
    {
fault:
        if (entry.allowedMask == translateNoMemoryBehindAllocation)
        {
            if (kind == translateUserLoad || kind == translateSupervisorLoad)
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _LACC_FAULT), address);
            else if (kind == translateUserStore || kind == translateSupervisorStore)
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _SACC_FAULT), address);
            else
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _IACC_FAULT), address);

            return 0;
        }
        switch(kind)
        {
            case translateUserLoad:
            case translateSupervisorLoad:
                if (has_supervisor_mode)
                    trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _LPAGE_FAULT), address);
                else
                    trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _LACC_FAULT), address);
                break;

            case translateUserExelwte:
            case translateSupervisorExelwte:
                if (has_supervisor_mode)
                    trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _IPAGE_FAULT), address);
                else
                    trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _IACC_FAULT), address);
                break;

            case translateUserStore:
            case translateSupervisorStore:
                if (has_supervisor_mode)
                    trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _SPAGE_FAULT), address);
                else
                    trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _SACC_FAULT), address);
                break;
        }
    }

    return 0;
}

void riscv_core::write_register(LwU64 registerIndex, LwU64 newValue)
{
    if (regs[registerIndex] != newValue) {
        regs[registerIndex] = newValue;
        event_register_written(registerIndex);
    }
}

void riscv_core::execute(LwU32 opcode)
{
    LwU32 mpie;
    LwS64 imm;
    LwU64 target, scratch;
    lwstom_memory *memory;

    //
    // Decode the instruction
    //
    LibosRiscvInstruction i;
    LwU64 nextPc = libosRiscvDecode(opcode, programCounter, &i);

    if (!nextPc) 
    {
        assert(0 && "Illegal instruction");
        trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
        return;        
    }

    pc = nextPc;

    event_instruction_decoded(programCounter, &i);

    switch (i.opcode)
    {
        case RISCV_LB: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorLoad : translateUserLoad, scratch)))
                break;            
            write_register(i.rd, (int8_t)memory->read8(scratch));            break;

        case RISCV_LH: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorLoad : translateUserLoad, scratch)))
                break;     
            write_register(i.rd, (int16_t)memory->read16(scratch));            break;

        case RISCV_LW: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorLoad : translateUserLoad, scratch)))
                break;     
            write_register(i.rd, (LwS32)memory->read32(scratch));            break;

        case RISCV_LD: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorLoad : translateUserLoad, scratch)))
                break;     
            write_register(i.rd, (LwS64)memory->read64(scratch));            break;

        case RISCV_LBU: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorLoad : translateUserLoad, scratch)))
                break;     
            write_register(i.rd, (LwU8)memory->read8(scratch));            break;

        case RISCV_LHU: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorLoad : translateUserLoad, scratch)))
                break;     
            write_register(i.rd, (LwU16)memory->read16(scratch));            break;

        case RISCV_LWU:
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorLoad : translateUserLoad, scratch)))
                break;     
            write_register(i.rd, (LwU32)memory->read32(scratch));            break;

        case RISCV_SB: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorStore : translateUserStore, scratch)))
                break;
            memory->write8(scratch, regs[i.rs2]);            break;

        case RISCV_SH: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorStore : translateUserStore, scratch)))
                break;
            memory->write16(scratch, regs[i.rs2]);            break;

        case RISCV_SW: 
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorStore : translateUserStore, scratch)))
                break;
            memory->write32(scratch, regs[i.rs2]);            break;

        case RISCV_SD:
            if (!(memory = translate_or_fault(i.imm + regs[i.rs1], privilege == privilege_supervisor ? translateSupervisorStore : translateUserStore, scratch)))
                break;
            memory->write64(scratch, regs[i.rs2]);            break;

        case RISCV_LI: 
        case RISCV_LUI: 
            write_register(i.rd, i.imm);            break;

        case RISCV_AUIPC:
            write_register(i.rd, i.imm);            break;

        case RISCV_SRLI: 
            write_register(i.rd, (LwU64)regs[i.rs1] >> i.imm);            break;
        
        case RISCV_SRLIW: 
            write_register(i.rd, sext32((LwU32)regs[i.rs1] >> i.imm));            break;

        case RISCV_SRAI: 
            write_register(i.rd, (LwS64)regs[i.rs1] >> i.imm);            break;

        case RISCV_SRAIW:
            write_register(i.rd, sext32((LwS32)regs[i.rs1] >> i.imm));            break;

        case RISCV_SLLI:
            write_register(i.rd, regs[i.rs1] << i.imm);
            break;

        case RISCV_SLLIW:
            write_register(i.rd, sext32((LwU32)regs[i.rs1] << i.imm));
            break;

        case RISCV_SLT:
            write_register(i.rd, (LwS64)regs[i.rs1] < (LwS64)regs[i.rs2]);
            break;

        case RISCV_SLTI: 
            write_register(i.rd, (LwS64)regs[i.rs1] < i.imm);
            break;
        
        case RISCV_SLTU:
            write_register(i.rd, (LwU64)regs[i.rs1] < (LwU64)regs[i.rs2]);
            break;

        case RISCV_SLTIU: 
            write_register(i.rd, (LwU64)regs[i.rs1] < (LwU64)i.imm);
            break;
        
        case RISCV_XORI:
            write_register(i.rd, regs[i.rs1] ^ i.imm);
            break;

        case RISCV_ORI:  
            write_register(i.rd, regs[i.rs1] | i.imm);
            break;
        
        case RISCV_ADDI: 
            write_register(i.rd, regs[i.rs1] + i.imm);
            break;
        
        case RISCV_ADDIW: 
            write_register(i.rd, (LwS32)(regs[i.rs1] + i.imm));
            break;

        case RISCV_ANDI: 
            write_register(i.rd, regs[i.rs1] & i.imm);
              break;

        case RISCV_SLLW: 
            write_register(i.rd, sext32((LwU32)(regs[i.rs1] << (regs[i.rs2] & 63))));
              break;        
        
        case RISCV_DIV: 
            if (!regs[i.rs2])
                write_register(i.rd, -1);
            else if (regs[i.rs1] == 0x8000000000000000ULL && regs[i.rs2] == 0xFFFFFFFF)
                write_register(i.rd, 0x8000000000000000ULL);
            else
                write_register(i.rd, (LwS64)regs[i.rs1] / (LwS64)regs[i.rs2]);
              break;        
            
        
        case RISCV_DIVW: 
            if (!(LwU32)regs[i.rs2])
                write_register(i.rd, -1);
            else if ((LwU32)regs[i.rs1] == 0x80000000 && (LwU32)regs[i.rs2] == 0xFFFFFFFF)
                write_register(i.rd, sext32(0x80000000));
            else
                write_register(i.rd, sext32((LwS32)regs[i.rs1] / (LwS32)regs[i.rs2]));
              break;
        
        case RISCV_DIVU:
            if (regs[i.rs2])
                write_register(i.rd, regs[i.rs1] / regs[i.rs2]);
            else
                write_register(i.rd, -1);        
              break;

        case RISCV_DIVUW:
            if ((LwU32)regs[i.rs2])
                write_register(i.rd, sext32((LwU32)regs[i.rs1] / (LwU32)regs[i.rs2]));
            else
                write_register(i.rd, -1);     
              break;

        case RISCV_REM: 
            if (!regs[i.rs2])
                write_register(i.rd, regs[i.rs1]);
            else if (regs[i.rs1] == 0x8000000000000000ULL && regs[i.rs2] == 0xFFFFFFFF)
                write_register(i.rd, 0);
            else
                write_register(i.rd, (LwS64)regs[i.rs1] % (LwS64)regs[i.rs2]);
              break;
        
        case RISCV_REMU: 
            if (regs[i.rs2])
                write_register(i.rd, regs[i.rs1] % regs[i.rs2]);
            else
                write_register(i.rd, regs[i.rs1]);    
              break;

        case RISCV_SRLW: 
            write_register(i.rd, sext32((LwU32)((LwU64)regs[i.rs1] >> (regs[i.rs2] & 63))));
              break;

        case RISCV_SRAW: 
            write_register(i.rd, sext32((LwS32)((LwS64)regs[i.rs1] >> (regs[i.rs2] & 63))));
              break;

        case RISCV_SLL:
            write_register(i.rd, regs[i.rs1] << (regs[i.rs2] & 63));
              break;

        case RISCV_SRL: 
            write_register(i.rd, (LwU64)regs[i.rs1] >> (regs[i.rs2] & 63));            
              break;

        case RISCV_SRA:  
            write_register(i.rd, (LwS64)regs[i.rs1] >> (regs[i.rs2] & 63));        
              break;

        case RISCV_SUB:  
            write_register(i.rd, regs[i.rs1] - regs[i.rs2]); 
              break;

        case RISCV_SUBW: 
            write_register(i.rd, sext32((LwU32)regs[i.rs1] - (LwU32)regs[i.rs2]));
              break;

        case RISCV_ADDW:
            write_register(i.rd, sext32((LwU32)regs[i.rs1] + (LwU32)regs[i.rs2]));
              break;

        case RISCV_MULW: 
            write_register(i.rd, sext32((LwU32)regs[i.rs1] * (LwU32)regs[i.rs2]));
              break;
        
        case RISCV_MUL: 
            write_register(i.rd, regs[i.rs1] * regs[i.rs2]);
              break;

        case RISCV_XOR:  
            write_register(i.rd, regs[i.rs1] ^ regs[i.rs2]);
              break;

        case RISCV_OR:   
            write_register(i.rd, regs[i.rs1] | regs[i.rs2]);
              break;

        case RISCV_AND:
            write_register(i.rd, regs[i.rs1] & regs[i.rs2]);
              break;

        case RISCV_ADD: 
            write_register(i.rd, regs[i.rs1] + regs[i.rs2]);
              break;        

        case RISCV_REMW: 
            if (!(LwU32)regs[i.rs2])
                write_register(i.rd, sext32((LwU32)regs[i.rs1]));
            else if ((LwU32)regs[i.rs1] == 0x80000000 && (LwU32)regs[i.rs2] == 0xFFFFFFFF)
                write_register(i.rd, 0);
            else
                write_register(i.rd, sext32((LwS32)regs[i.rs1] % (LwS32)regs[i.rs2]));
              break;        
        
        case RISCV_REMUW:
            if ((LwU32)regs[i.rs2])
                write_register(i.rd, sext32((LwU32)regs[i.rs1] % (LwU32)regs[i.rs2]));
            else
                write_register(i.rd, sext32((LwU32)regs[i.rs1])); 
              break;        
        
        case RISCV_EBREAK:
            trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _BKPT), 0);            break;        
        
        case RISCV_MRET: 
            assert(privilege == privilege_machine);
            pc        = xepc;
            privilege = (privilege_t)DRF_VAL64(_RISCV, _CSR_MSTATUS, _MPP, mstatus);
            assert(privilege <= privilege_machine);
            if (privilege == privilege_supervisor)
                assert(has_supervisor_mode);
            mpie    = DRF_VAL64(_RISCV, _CSR_MSTATUS, _MPIE, mstatus);
            mstatus = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _MIE, mpie, mstatus);            break;
                    
        case RISCV_SRET: 
            assert(privilege == privilege_supervisor && has_supervisor_mode);
            pc        = xepc;
            privilege = (privilege_t)DRF_VAL64(_RISCV, _CSR_MSTATUS, _SPP, mstatus);
            assert(privilege <= privilege_supervisor);
            mpie    = DRF_VAL64(_RISCV, _CSR_MSTATUS, _SPIE, mstatus);
            mstatus = FLD_SET_DRF_NUM64(_RISCV, _CSR_MSTATUS, _SIE, mpie, mstatus);            break;        
        
        case RISCV_WFI:
            assert(privilege > privilege_user);
            fast_forward();            break;  

        case RISCV_ECALL:
            if (privilege == privilege_user)
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _UCALL), 0);
            else if (privilege == privilege_supervisor)
                sbi();            break;      

        case RISCV_CSRRW:
            if (csr_validate_access_HAL(i.csr))
            {
                scratch = csr_read_HAL(i.csr);
                csr_write_HAL(i.csr, regs[i.rs1]);
                write_register(i.rd, scratch);
    
            }
            else
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);            break;

        case RISCV_CSRRS:
            if (csr_validate_access_HAL(i.csr))
            {
                scratch = csr_read_HAL(i.csr);
                if (regs[i.rs1])
                    csr_write_HAL(i.csr, scratch | regs[i.rs1]);
                write_register(i.rd, scratch);
    
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }            break;

        case RISCV_CSRRC:
            if (csr_validate_access_HAL(i.csr))
            {
                scratch = csr_read_HAL(i.csr);
                if (regs[i.rs1])
                    csr_write_HAL(i.csr, scratch & ~regs[i.rs1]);
                write_register(i.rd, scratch);
    
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }            break;

        case RISCV_CSRRWI:
            if (csr_validate_access_HAL(i.csr))
            {
                scratch = csr_read_HAL(i.csr);
                csr_write_HAL(i.csr, i.imm);
                write_register(i.rd, scratch);
    
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }            break;

        case RISCV_CSRRSI:
            if (csr_validate_access_HAL(i.csr))
            {
                scratch = csr_read_HAL(i.csr);
                csr_write_HAL(i.csr, scratch | (LwU64)i.imm);
                write_register(i.rd, scratch);
    
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }            break;

        case RISCV_CSRRCI:
            if (csr_validate_access_HAL(i.csr))
            {
                scratch = csr_read_HAL(i.csr);
                csr_write_HAL(i.csr, scratch & ~(LwU64)i.imm);
                write_register(i.rd, scratch);
    
            }
            else
            {
                trap(DRF_DEF(_RISCV_CSR, _MCAUSE, _EXCODE, _ILL), 0);
            }            break;        

        case RISCV_JALR:
            target   = regs[i.rs1] + i.imm;
            write_register(i.rd, pc);

            pc = target;            break;

        case RISCV_JAL:
            write_register(i.rd, pc);


            // Is this an infinite loop? If so, fast forward until the next trap
            // @note This reduces testing time drastically when the idle task is hit (or we panic).
            if (programCounter == i.imm)
                fast_forward();

            pc = i.imm;            break;        

        case RISCV_BEQ: // BEQ
            if (regs[i.rs1] == regs[i.rs2])
                pc = i.imm;            break;

        case RISCV_BNE: // BNE
            if (regs[i.rs1] != regs[i.rs2])
                pc = i.imm;            break;

        case RISCV_BLT: // BLT
            if ((LwS64)regs[i.rs1] < (LwS64)regs[i.rs2])
                pc = i.imm;            break;

        case RISCV_BGE: // BGE
            if ((LwS64)regs[i.rs1] >= (LwS64)regs[i.rs2])
                pc = i.imm;            break;

        case RISCV_BLTU: // BLTU
            if ((LwU64)regs[i.rs1] < (LwU64)regs[i.rs2])
                pc = i.imm;            break;

        case RISCV_BGEU: // BGEU
            if ((LwU64)regs[i.rs1] >= (LwU64)regs[i.rs2])
                pc = i.imm;            break;            

        case RISCV_FENCE:            break;

        default:              
            printf("Unsupported opcode %s\n", opcode_names[i.opcode]);
            assert(0);            break;
    }

    event_instruction_retired();
}

void riscv_core::step()
{
    LwU64 scratch;
    programCounter = pc;

    event_instruction_prestart();

    if (stopped)
        return;

    if (single_step_once)
        stopped = LW_TRUE, single_step_once = LW_FALSE;

    update_interrupts();

    // Check for timer
#if LIBOS_CONFIG_RISCV_S_MODE
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
#endif
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

    // Issue two translations since the opcode may straddle a page
    LwU64 first_word_pa, second_word_pa;
    lwstom_memory * first_word, * second_word;

    if (!(first_word = translate_or_fault(programCounter, privilege == privilege_supervisor ? translateSupervisorExelwte : translateUserExelwte, first_word_pa)))
      return;

    if (!(second_word = translate_or_fault(programCounter+2, privilege == privilege_supervisor ? translateSupervisorExelwte : translateUserExelwte, second_word_pa)))
      return;

    LwU32 op = first_word->read16(first_word_pa) | (((LwU32)second_word->read16(second_word_pa)) << 16);
    execute(op);

    regs[RISCV_X0] = 0;
    cycle++;
}
