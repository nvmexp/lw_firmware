/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include <stdio.h>
#include <math.h>

#include "mdiag/tests/test_state_report.h"
#include "vga_rma.h"
#include "mdiag/tests/stdtest.h"
#include "fermi/gf100/dev_disp.h"
#include "mdiag/IRegisterMap.h"
#include "lwmisc.h"

#define TEST_NAME     "vga_rma"

extern const ParamDecl vga_rma_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

vga_rma::vga_rma(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
}

vga_rma::~vga_rma(void)
{
}

STD_TEST_FACTORY(vga_rma, vga_rma)

int vga_rma::Setup(void) {
    lwgpu = LWGpuResource::FindFirstResource();
    UINT32 arch = lwgpu->GetArchitecture();
    if(arch < 0x30) {
        return 0;
    }

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("vga_rma::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("vga_rma");
    getStateReport()->enable();

    return 1;

}

void    vga_rma::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu) {
        DebugPrintf("vga_rma::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();

}

// **********************************************************************
void vga_rma::Run(void)
{
    int   shft_val;
    unsigned int i, j = 0;

    InfoPrintf ("%s ----- Begin Test ----- \n", TEST_NAME);

    testStatus = 1;                     // Assume test succeed

    Platform::PioWrite08(LW_VIO_VSE2, DRF_VAL(_VIO, _VSE2_VIDSYS, _EN, 0x1)); // enable vga
    Platform::PioWrite08(LW_VIO_MISC__WRITE, DRF_DEF(_VIO, _MISC, _IO_ADR, _03DX) | DRF_DEF(_VIO, _MISC, _EN_RAM, _ENAB)); // enable color (0x3dX), and ram
    Platform::PioWrite08(LW_VIO_GRX, LW_VIO_GR_MISC__INDEX);
    Platform::PioWrite08(LW_VIO_GR_MISC, DRF_DEF(_VIO, _GR_MISC, _GRAPHICS_MODE, _GRAPHIC));       // 128K A0000-BFFFF
    Platform::PioWrite08(0x3d4, 0x3f); // unlock extended CR registers
    Platform::PioWrite08(0x3d5, 0x57); // unlock extended CR registers

    UINT32 rdVal32 = 0, wrVal32 = 0, addr = 0x3d4;
    UINT16 rdVal16 = 0, wrVal16 = 0;
    UINT08 rdVal08 = 0, wrVal08 = 0x3c;

    // test some CR REGS
    Platform::PioWrite08(addr, wrVal08);
    Platform::PioRead08(addr, &rdVal08);
    if (rdVal08 != wrVal08) 
    {
        ErrPrintf("ERROR: I/o reg failure 0x%08x, rd_val = 0x%08x expected 0x%08x\n",
            addr, rdVal08, wrVal08);
        testStatus = 0;
    }

    // test scratch registers CR80 - 9F
    for (i = 0; i < 0x20; i++) 
    {
        wrVal16 = ((0x80 | i) << 8) | (0x80 + i);
        Platform::PioWrite16(addr, wrVal16); // write 80 - 9f
        Platform::PioRead16(addr, &rdVal16);
        if (rdVal16 != wrVal16) 
        {
            ErrPrintf("ERROR: 0x3d4 rd_val = 0x%08x expected 0x%08x\n", 
                rdVal16, wrVal16);
            testStatus = 0;
        }
    }
    // now do readback testing
    for (i = 0; i < 0x20; i++) 
    {
        wrVal16 = ((0x80|i) << 8) | (0x80+i);
        Platform::PioWrite08(addr, wrVal16);
        Platform::PioRead16(addr, &rdVal16);
        if (rdVal16 != wrVal16) 
        {
            ErrPrintf("ERROR 0x3d4 rd_val = 0x%08x expected 0x%08x\n", 
                rdVal16, wrVal16);
            testStatus = 0;
        }
    }

    testStatus &= CheckCRRegister(0x80, 0xba, 0xff);
    testStatus &= CheckCRRegister(0x0, 0x55, 0xff);
    testStatus &= CheckCRRegister(0x0, 0x2, 0xff);

    Platform::PioRead08(0x3cc, &rdVal08);

    testStatus &= CheckCRRegister(0x0, 0x2, 0xff);
    testStatus &= CheckCRRegister(0x0, 0x54, 0xff);

    testStatus &= test_palette(); // test a palette register

    testStatus &= CheckCRRegister(0x0, 0x42, 0xff);

    testStatus &= test_palette();

    InfoPrintf("Now testing CR regs using priv i/o\n");

    // test scratch registers CR80 - 9f using priv i/o
    for (i = 0; i < 0x20; i++) 
    {
        shft_val = (i % 4) * 8;
        wrVal32 = (0xc0 | i) << shft_val;
        addr = LW_PDISP_VGA_CR_REG0 + 0x80 + (i / 4) * 4;
        lwgpu->RegWr32(addr, wrVal32);
        rdVal32 = lwgpu->RegRd32(addr);
        if (rdVal32 != wrVal32) 
        {
            ErrPrintf("ERROR: 0x3d4 rd_val = 0x%08x expected 0x%08x\n", 
                rdVal32, wrVal32);
            testStatus = 0;
        }
    }

    // test a palette register
    addr = LW_PDISP_VGA_LUT(0);
    wrVal08 = 0x23;
    lwgpu->RegWr08(addr, wrVal08);
    rdVal08 = lwgpu->RegRd08(addr);
    if (rdVal08 != wrVal08) 
    {
        ErrPrintf("ERROR: I/o reg failure 0x%08x, rd_val = 0x%08x expected 0x%08x\n",
            addr, rdVal08, wrVal08);
        testStatus = 0;
    }

    //----------RMA testing
    RMA_SIZE = 1; // start w/ 8 bit i/o
    for (i = 0; i < 3; i++) // test 3 time, at 8/16/32 bit i/o
    {
        for (j = 0; j < 8; j++)
        {
            addr = LW_PDISP_VGA_CR_REG0+0x80+j*4;
            testStatus &= CheckRegister(addr, 0xdeadbeef, 0xffffffff);
            testStatus &= CheckRegister(addr, 0xcafecafe, 0xffffffff);
            wrVal32 = 0xdeadbeef;
            RMA_WRITE(addr, wrVal32);
            rdVal32 = RMA_READ(addr);
            if (rdVal32 != wrVal32) 
            {
                ErrPrintf("ERROR: RMA reg failure 0x%08x, rd_val = 0x%08x expected 0x%08x\n",
                   addr, rdVal32, wrVal32);
                testStatus = 0;
            }
        }
        RMA_SIZE = RMA_SIZE*2;
    }

    if (testStatus) 
    {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } 
    else 
    {
        SetStatus(Test::TEST_FAILED_CRC);
        getStateReport()->crcCheckFailed("run failed, Interrupt oclwred while polling notify buffer\n");
    }
}

unsigned int vga_rma::RMA_READ(unsigned int addr)
{
    UINT08 saveIndex = 0, value08 = 0;
    UINT16 value16 = 0;
    UINT32 value32 = 0;

    switch(RMA_SIZE)
    {
        case 1: // 8 bit i/o
            Platform::PioRead08(0x3d4, &saveIndex);
            Platform::PioWrite08(0x3d4, 0xf0);
            Platform::PioWrite08(0x3d5, 0x03);

            Platform::PioWrite08(0x3d0, addr);
            Platform::PioWrite08(0x3d1, addr >> 8);
            Platform::PioWrite08(0x3d2, addr >> 16);
            Platform::PioWrite08(0x3d3, addr >> 24);

            Platform::PioWrite08(0x3d4, 0xf0);
            Platform::PioWrite08(0x3d5, 0x05);

            Platform::PioRead08(0x3d3, &value08);
            value32 = value08;
            Platform::PioRead08(0x3d2, &value08);
            value32 = (value32 << 8) | value08;
            Platform::PioRead08(0x3d1, &value08);
            value32 = (value32 << 8) | value08;
            Platform::PioRead08(0x3d0, &value08);
            value32 = (value32 << 8) | value08;

            Platform::PioWrite08(0x3d4, saveIndex);
            break;
        
        case 2 : // 16 bit i/o
            Platform::PioRead08(0x3d4, &saveIndex);
            Platform::PioWrite16(0x3d4, 0x03f0);

            Platform::PioWrite16(0x3d0, addr);
            Platform::PioWrite16(0x3d2, addr>>16);

            Platform::PioWrite16(0x3d4, 0x05f0);

            Platform::PioRead16(0x3d0, &value16);
            value32 = value16;
            Platform::PioRead16(0x3d2, &value16);
            value32 = value32 | (value16 << 16);

            Platform::PioWrite08(0x3d4, saveIndex);
            break;

        case 4 : // 32 bit i/o
            Platform::PioRead08(0x3d4, &saveIndex);
            Platform::PioWrite16(0x3d4, 0x03f0);

            Platform::PioWrite32(0x3d0, addr);

            Platform::PioWrite16(0x3d4, 0x05f0);

            Platform::PioRead32(0x3d0, &value32);
            Platform::PioWrite08(0x3d4, saveIndex);

            break;
        default :
            ErrPrintf("TEST ERROR: RMA_SIZE must be 1, 2, or 4\n");
            value32 = 0;
    }

    return value32;
}

// assumes cr1f is unlocked
void vga_rma::RMA_WRITE(unsigned int addr, unsigned int value)
{
    UINT08 saveIndex = 0;

    switch(RMA_SIZE)
    {
        case 1: // 8 bit i/o
            Platform::PioRead08(0x3d4, &saveIndex);
            Platform::PioWrite08(0x3d4, 0xf0);
            Platform::PioWrite08(0x3d5, 0x03);

            Platform::PioWrite08(0x3d0, addr);
            Platform::PioWrite08(0x3d1, addr >> 8);
            Platform::PioWrite08(0x3d2, addr >> 16);
            Platform::PioWrite08(0x3d3, addr >> 24);

            Platform::PioWrite08(0x3d4, 0xf0);
            Platform::PioWrite08(0x3d5, 0x07);

            Platform::PioWrite08(0x3d0, value);
            Platform::PioWrite08(0x3d1, value >> 8);
            Platform::PioWrite08(0x3d2, value >> 16);
            Platform::PioWrite08(0x3d3, value >> 24);

            Platform::PioWrite08(0x3d4, saveIndex);
            break;
        
        case 2: // 16 bit i/o
            Platform::PioRead08(0x3d4, &saveIndex);
            Platform::PioWrite16(0x3d4, 0x03f0);

            Platform::PioWrite16(0x3d0, addr);
            Platform::PioWrite16(0x3d2, addr >> 16);

            Platform::PioWrite16(0x3d4, 0x07f0);

            Platform::PioWrite16(0x3d0, value);
            Platform::PioWrite16(0x3d2, value >> 16);

            Platform::PioWrite08(0x3d4, saveIndex);
            break;
    
        case 4: // 32 bit i/o
            Platform::PioRead08(0x3d4, &saveIndex);
            Platform::PioWrite16(0x3d4, 0x03f0);

            Platform::PioWrite32(0x3d0, addr);

            Platform::PioWrite16(0x3d4, 0x07f0);

            Platform::PioWrite32(0x3d0, value);

            Platform::PioWrite08(0x3d4, saveIndex);
            break;
        default :
            ErrPrintf("TEST ERROR: RMA_SIZE must be 1, 2, or 4\n");
    }
}

int vga_rma::check_iord_ack (unsigned adr, unsigned ack)
{
    //This routine wants to know if the GPU claimed the transaction.
    // Can mdiag determine this correctly?
    // For now, let return of ZERO be a non-ack
    // Any non-zero value is considered a successful ack
    UINT16 stat = 0;
    Platform::PioRead16(adr, &stat);
    if (ack ? (stat == 0) : (stat != 0))
    {
        ErrPrintf ("%%t Error: IO 0x%03x should %shave acknowledged\n", 
            adr, ack ? "" : "not ");
        return 0;
    }
    return 1;
}

int vga_rma::test_palette() 
{
    UINT32 addr = 0x3c6;
    UINT08 rdVal08 = 0, wrVal08 = 0xff;

    // test a palette register
    Platform::PioWrite08(addr, wrVal08); // switch to CR00
    Platform::PioRead08(addr, &rdVal08);
    if (rdVal08 != wrVal08) 
    {
        ErrPrintf("ERROR: I/o reg failure 0x%08x, rd_val = 0x%08x expected 0x%08x\n",
            addr, rdVal08, wrVal08);
        return 0;
    }
    return 1;
}

// writes value into addr, and does readback -- checking against mask; return 0 if err
int vga_rma::CheckRegister_RMA(unsigned int addr, unsigned int value, unsigned int mask)
{
  unsigned int rd_val;

  RMA_WRITE(addr, value);
  rd_val = RMA_READ(addr);

  if(rd_val != (value&mask)) {
    ErrPrintf("ERROR: RMA reg failure 0x%08x, rd_val = 0x%08x expected 0x%08x\n",
                 addr, rd_val, value&mask);
    return(0);
  }
  return(1);
}

// writes value into addr, and does readback -- checking against mask; return 0 if err
int vga_rma::CheckRegister(unsigned int addr, unsigned int value, unsigned int mask)
{
  unsigned int rd_val;

  lwgpu->RegWr32(addr, value);
  rd_val = lwgpu->RegRd32(addr);

  if(rd_val != (value&mask)) {
    ErrPrintf("ERROR: Mem reg failure 0x%08x, rd_val = 0x%08x expected 0x%08x\n",
                 addr, rd_val, value&mask);
    return(0);
  }
  return(1);
}

// writes value into addr, and does readback -- checking against mask; return 0 if err
int vga_rma::CheckCRRegister(unsigned int addr, unsigned int value, unsigned int mask)
{
    UINT16 wrVal16 = (value << 8) | (addr);
    UINT16 rdVal16 = 0;

    Platform::PioWrite16(0x3d4, wrVal16); // write 3a - 3c
    Platform::PioRead16(0x3d4, &rdVal16);
    if ((rdVal16 & (mask << 8 | 0xff)) != wrVal16) 
    {
        ErrPrintf("ERROR: 0x3d4 rd_val = 0x%08x expected 0x%08x\n", 
            rdVal16, wrVal16 & mask);
        return(0);
    }
    return(1);
}

