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

#include "mdiag/tests/stdtest.h"
#include "mdiag/tests/test_state_report.h"
#include "vstack.h"
#include "mdiag/IRegisterMap.h"

#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_disp.h"

//
// Useful macros
//
//#define LW_MIN(a, b) (((a) < (b)) ? (a) : (b))
//#define LW_MAX(a, b) (((a) > (b)) ? (a) : (b))
//#define LW_CEIL(a,b) (((a)+(b)-1)/(b))

/*
  test vstack autopush/autopop
  test overflow/underflow
  test clearing of overflow/underflow
  test full/empty detection
  test manual push/pop mode
  test under backpressure

  same from i/o space
  test that reads/writes to 3d4 have no side effects
*/

#define TEST_NAME     "vstack"
static  unsigned int  stack[512];

int     testStatus = 1;                     // Assume test succeed

extern const ParamDecl vstack_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

vstack::vstack(ArgReader *params) :
    LWGpuSingleChannelTest(params)
{
}

vstack::~vstack(void)
{
}

STD_TEST_FACTORY(vstack, vstack)

int vstack::Setup(void) {
    lwgpu = LWGpuResource::FindFirstResource();
    UINT32 arch = lwgpu->GetArchitecture();
    if(arch < 0x30) {
        return 0;
    }

    ch = lwgpu->CreateChannel();
    if(!ch)
    {
        ErrPrintf("vstack::Setup failed to create channel\n");
        return 0;
    }

    getStateReport()->init("vstack");
    getStateReport()->enable();

    return 1;

}

void    vstack::CleanUp(void)
{
    if(ch)
    {
        delete ch;
        ch = 0;
    }

    if (lwgpu) {
        DebugPrintf("vstack::CleanUp(): Releasing GPU.\n");
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();

}

// **********************************************************************
void
vstack::Run(void)
{
    int         k;

    InfoPrintf("vstack::Run: start\n");

    IgnoreFailures = 0;                         // Set to 1 to let test continue with failures

    // Enable access to vga registers, unlock vga registers
    Platform::PioWrite08(LW_VIO_VSE2, 1);              // turn on i/o accesses
    Platform::PioWrite08(LW_VIO_MISC__WRITE, 0x03);    // use color space (3d4)
    Platform::PioWrite16(LW_CIO_CRX__COLOR, 0x573F);   // unlock cr registers

    // generate data
    for (k=0; k<512; k++) {
        stack[k] = (k+1)&0xff;
    }

    InfoPrintf("vstack::Run: start using pri_cr register testing\n");
    // First pass using priv_cr register.
    test_sanity(1);
    test_manual_mode(1);
    test_backpressure(1);
    test_auto_mode(1);

    InfoPrintf("vstack::Run: start using io cr register testing\n");
    // Second pass using io cr register.
    test_sanity(0);
    test_manual_mode(0);
    test_backpressure(0);
    test_auto_mode(0);

    if (testStatus) {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(TEST_FAILED_CRC);
        getStateReport()->crcCheckFailed("vstack::test_auto_mode: run failed, Interrupt oclwred while polling notify buffer\n");
    }

    InfoPrintf("vstack::Run: end\n");

}

void vstack::test_sanity(int mode)
{
    unsigned    int    returlwal;

    // push then pop
    mode?lwgpu->RegWr32(LW_PDISP_VGA_STACK_DATA, 0x55):WriteCR(LW_CIO_CRE_STACK_DATA__INDEX, 0x55);
    returlwal = mode?lwgpu->RegRd32(LW_PDISP_VGA_STACK_DATA):ReadCR(LW_CIO_CRE_STACK_DATA__INDEX);
    if(returlwal != 0x55) {
        ErrPrintf("vstack::test_sanity: stack value is %x, expected 0x55\n", returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    // push then pop
    mode?lwgpu->RegWr32(LW_PDISP_VGA_STACK_DATA, 0xaa):WriteCR(LW_CIO_CRE_STACK_DATA__INDEX, 0xaa);
    returlwal = mode?lwgpu->RegRd32(LW_PDISP_VGA_STACK_DATA):ReadCR(LW_CIO_CRE_STACK_DATA__INDEX);
    if(returlwal != 0xaa) {
        ErrPrintf("vstack::test_sanity: stack value is %x, expected 0xaa\n", returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }
}

void vstack::test_manual_mode(int mode)
{
    int val;
    int returlwal, expectedval;

    InfoPrintf("vstack::test_manual_mode: test manual push/pop mode\n");

    // put into manual push/pop mode
    val = DRF_DEF( _PDISP, _VGA_STACK_MISC, _PUSH, _MAN_PUSH ) |
            DRF_DEF( _PDISP, _VGA_STACK_MISC, _POP, _MAN_POP ) |
            DRF_DEF( _PDISP, _VGA_STACK_MISC, _READS, _PREPOP );
    lwgpu->RegWr32 ( LW_PDISP_VGA_STACK_MISC, val);

    mode?lwgpu->RegWr32(LW_PDISP_VGA_STACK_DATA, 0xa):WriteCR(LW_CIO_CRE_STACK_DATA__INDEX, 0xa);
    mode?lwgpu->RegWr32(LW_PDISP_VGA_STACK_DATA, 0xb):WriteCR(LW_CIO_CRE_STACK_DATA__INDEX, 0xb);
    mode?lwgpu->RegWr32(LW_PDISP_VGA_STACK_DATA, 0xc):WriteCR(LW_CIO_CRE_STACK_DATA__INDEX, 0xc);

//    if(!mode) lwgpu->RegWr32 (LW_PDISP_VGA_STACK_POINTER, 0);  // reset to '0'
    returlwal = lwgpu->RegRd32(LW_PDISP_VGA_STACK_POINTER);
    expectedval = 0x0;
    if(expectedval != returlwal)
    {
        ErrPrintf("vstack::test_manual_mode: stack ptr val is %d, expected %d\n", returlwal, expectedval);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    // push it down
    mode?lwgpu->RegWr32 (LW_PDISP_VGA_STACK_CTRL, DRF_DEF( _PDISP, _VGA_STACK, _CTRL_CMD, _PUSH )):WriteCR(LW_CIO_CRE_STACK_CTRL__INDEX, DRF_DEF( _CIO_CRE, _STACK, _CTRL_CMD, _PUSH ));

    // read twice to make sure it doesn't go away
    returlwal = mode?lwgpu->RegRd32(LW_PDISP_VGA_STACK_DATA):ReadCR(LW_CIO_CRE_STACK_DATA__INDEX);
    expectedval = 0xc;
    if(expectedval != returlwal) {
        ErrPrintf("vstack::test_manual_mode: stack val is %d, expected %d\n", returlwal, expectedval);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }
    returlwal = mode?lwgpu->RegRd32(LW_PDISP_VGA_STACK_DATA):ReadCR(LW_CIO_CRE_STACK_DATA__INDEX);
    expectedval = 0xc;
    if(expectedval != returlwal) {
        ErrPrintf("vstack::test_manual_mode: stack val is %d, expected %d\n", returlwal, expectedval);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }
    // check that ptr is higher now
    returlwal = lwgpu->RegRd32(LW_PDISP_VGA_STACK_POINTER);
    expectedval = 0x1;
    if(expectedval != returlwal) {
        ErrPrintf("vstack::test_manual_mode: stack ptr val is %d, expected %d\n", returlwal, expectedval);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    // pop
    mode?lwgpu->RegWr32 (LW_PDISP_VGA_STACK_CTRL, DRF_DEF( _PDISP, _VGA_STACK, _CTRL_CMD, _POP )):WriteCR(LW_CIO_CRE_STACK_CTRL__INDEX, DRF_DEF( _CIO_CRE, _STACK, _CTRL_CMD, _POP ));

    // check that ptr is lower now
    returlwal = lwgpu->RegRd32(LW_PDISP_VGA_STACK_POINTER);
    expectedval = 0x0;
    if(expectedval != returlwal) {
        ErrPrintf("vstack::test_manual_mode: stack ptr val is %d, expected %d\n", returlwal, expectedval);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    // go back to autopush/pop mode
    val = DRF_DEF( _PDISP, _VGA_STACK_MISC, _PUSH, _AUTOPUSH ) |
            DRF_DEF( _PDISP, _VGA_STACK_MISC, _POP, _AUTOPOP ) |
            DRF_DEF( _PDISP, _VGA_STACK_MISC, _READS, _PREPOP );
    lwgpu->RegWr32 (LW_PDISP_VGA_STACK_MISC, val);

}

void vstack::test_backpressure(int mode)
{

    int k;
    int addr;
    int returlwal;
    int mystack[512];

    addr = LW_PDISP_VGA_STACK_DATA;

    // generate data
    for (k=0; k<512; k++) {
        mystack[k] = (1 + 3*k)&0xff;
    }

    // generate PRI traffic
    for (k=0; k<5; k++) {
        lwgpu->RegWr32 (LW_PBUS_DEBUG_0, k);
    }
    for (k = 0; k<5; k++) {
        mode?lwgpu->RegWr32 (addr, mystack[k]):WriteCR(LW_CIO_CRE_STACK_DATA__INDEX, mystack[k]);
    }
    InfoPrintf("vstack::test_backpressure: read after writing to PRI for backpressure\n");

    // generate more PRI traffic
    for (k=0; k<8; k++) {
        lwgpu->RegWr32 (LW_PBUS_DEBUG_0, k+5);
    }

    for (k=4; k>=0; k--) {
        returlwal = mode?lwgpu->RegRd32(addr):ReadCR(LW_CIO_CRE_STACK_DATA__INDEX);
        if(returlwal != mystack[k]) {
            ErrPrintf("vstack::test_backpressure: popping entry %d from stack, expected %x, but rcvd %x\n", k, mystack[k], returlwal);
            testStatus = 0;
            if (!IgnoreFailures) return;
        }
    }
}

void vstack::test_auto_mode(int mode)
{
    unsigned int addr, returlwal, tmp1;
    int k;

    // put in 511 entries, stack should still be nonfull
    for (k = 0; k<511; k++) {
        mode?lwgpu->RegWr32(LW_PDISP_VGA_STACK_DATA, stack[k]):WriteCR(LW_CIO_CRE_STACK_DATA__INDEX, stack[k]);
    }

    returlwal = lwgpu->RegRd32(LW_PDISP_VGA_STACK_POINTER);
    tmp1 = DRF_VAL(_PDISP, _VGA_STACK, _POINTER_VALUE, returlwal);
    if(tmp1 != 511) {
        ErrPrintf("vstack::test_auto_mode: pointer is %d, expected 511\n", returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    returlwal = mode?lwgpu->RegRd32(LW_PDISP_VGA_STACK_CTRL):ReadCR(LW_CIO_CRE_STACK_CTRL__INDEX);
    tmp1 = DRF_VAL(_PDISP, _VGA_STACK_CTRL, _OVERFLOWED, returlwal) |
            DRF_VAL(_PDISP, _VGA_STACK_CTRL, _UNDERFLOWED, returlwal);
    if (tmp1 != 0) {
        ErrPrintf("vstack::test_auto_mode: detected unexpected stack overflow/underflow LW_PDISP_VGA_STACK_CTRL = 0x%x\n", returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    addr = mode?LW_PDISP_VGA_STACK_DATA:LW_CIO_CRE_STACK_DATA__INDEX;

    // put in 512th entry
    mode?lwgpu->RegWr32 (addr, stack[k]):WriteCR(addr, stack[k]);

    returlwal = lwgpu->RegRd32(LW_PDISP_VGA_STACK_POINTER);
    tmp1 = DRF_VAL(_PDISP, _VGA_STACK, _POINTER_VALUE, returlwal);
    if(tmp1 != 512) {
        ErrPrintf("vstack::test_auto_mode: pointer is %d, expected 512\n", returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    returlwal = mode?lwgpu->RegRd32(LW_PDISP_VGA_STACK_CTRL):ReadCR(LW_CIO_CRE_STACK_CTRL__INDEX);
    tmp1 = DRF_VAL(_PDISP, _VGA_STACK_CTRL, _OVERFLOWED, returlwal) |
            DRF_VAL(_PDISP, _VGA_STACK_CTRL, _UNDERFLOWED, returlwal);
    if (tmp1 != 0) {
        ErrPrintf("vstack::test_auto_mode: detected unexpected stack overflow/underflow LW_PDISP_VGA_STACK_CTRL = 0x%x\n", returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    // check that stack is now full
    tmp1 = DRF_VAL(_PDISP, _VGA_STACK, _CTRL_FULL, returlwal);

    if (tmp1 != LW_PDISP_VGA_STACK_CTRL_FULL_TRUE) {
        ErrPrintf("vstack::test_auto_mode: stack didn't report that it was full\n", returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    for (k=511; k>=0; k--) {
        returlwal = mode?lwgpu->RegRd32(addr):ReadCR(addr);
        if(returlwal != stack[k]) {
            ErrPrintf("vstack::test_auto_mode: popping entry %d from stack, expected %x, but rcvd %x\n", k, stack[k], returlwal);
            testStatus = 0;
            if (!IgnoreFailures) return;
        }
    }

    //check empty detection
    returlwal = mode?lwgpu->RegRd32(LW_PDISP_VGA_STACK_CTRL):ReadCR(LW_CIO_CRE_STACK_CTRL__INDEX);
    tmp1 = DRF_VAL(_PDISP, _VGA_STACK, _CTRL_EMPTY, returlwal);
    if (tmp1 != LW_PDISP_VGA_STACK_CTRL_EMPTY_TRUE) {
        ErrPrintf("vstack::test_auto_mode: stack didn't report that it was empty\n",  returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }

    // cause underflow
    returlwal = mode?lwgpu->RegRd32(addr):ReadCR(addr);
    returlwal = mode?lwgpu->RegRd32(LW_PDISP_VGA_STACK_CTRL):ReadCR(LW_CIO_CRE_STACK_CTRL__INDEX);
    tmp1 = DRF_VAL(_PDISP, _VGA_STACK_CTRL, _UNDERFLOWED, returlwal);
    if ( tmp1 != LW_PDISP_VGA_STACK_CTRL_UNDERFLOWED_TRUE ) {
        ErrPrintf("vstack::test_auto_mode: failed to detect stack underflow LW_PDISP_VGA_STACK_CTRL = 0x%x\n", returlwal);
        testStatus = 0;
        if (!IgnoreFailures) return;
    }
}

/**
 * GetCRTCAddr: this function returns crtc base address 0x3b4 or 0x3d4
 */
UINT16 vstack::GetCRTCAddr()
{
    UINT16 base = 0;
    UINT08 data = 0;

    Platform::PioRead08(0x3cc, &data);

    if (data & 0x01)  
        base = 0x3d4;
    else 
        base = 0x3b4;

    return base;
}

/**
 * WriteCR: write data to CR register.
 */
void vstack::WriteCR(UINT08 index, UINT08 data)
{
    UINT16 addr = GetCRTCAddr();
    Platform::PioWrite08(addr, index);
    Platform::PioWrite08(addr+1, data);
}

/**
 * ReadCR: read a CR register.
 */
UINT08 vstack::ReadCR(UINT08 index)
{
    UINT16 addr = GetCRTCAddr();
    UINT08 data = 0;

    Platform::PioWrite08(addr, index);
    Platform::PioRead08(addr+1, &data);
    return data;
}

/**
 * Test CR register, write a data to CR register, then read it back to see if it matches expectation.
 * Return: 0 - success
 *         1 - fail
 * */
bool vstack::TestCR(UINT08 index, UINT08 wr_data, UINT08 exp_val, UINT08 mask)
{
    UINT16 addr = GetCRTCAddr();
    UINT08 readVal = 0;

    Platform::PioWrite08(addr, index);
    Platform::PioWrite08(addr+1, wr_data);

    Platform::PioRead08(addr+1, &readVal);

    if ((readVal & mask) != (exp_val & mask))
    {
        ErrPrintf("read_val&mask = %08x, exp_val = %08x\n", readVal & mask , exp_val);
        ErrPrintf("Error [TestCR]: CR Register mismatch... Index = %8x mask =%8x Read_Val=%8x Exp_Val=%8x\n", 
            index, mask, readVal, exp_val);
        return 1;
    }
    else // read value matched the expected value
    { 
        return 0;
    } 
}

/**
 * WriteLW: write data to LW register.
 */
void vstack::WriteLW(UINT32 reg, UINT32 data)
{
    WriteCR(0xF0, 0x1);             // Select rma ptr
    Platform::PioWrite32(0x3d0, reg);
    WriteCR(0xF0, 0x7);             // Select rma write data
    Platform::PioWrite32(0x3d0, data);
}

/**
 * ReadLW: read LW register.
 */
UINT32 vstack::ReadLW(UINT32 reg)
{
    UINT32 data = 0;

    WriteCR(0xF0, 0x1);             // Select rma ptr
    Platform::PioWrite32(0x3d0, reg);
    WriteCR(0xF0, 0x5);             // Select rma read data
    Platform::PioRead32(0x3d0, &data);

    return data;
}

/**
 * TestLW: test LW register.
 * Return: 0 - success
 *         1 - fail
 */
bool vstack::TestLW(UINT32 reg, UINT32 wr_data, UINT32 exp_val, UINT32 mask)
{
    UINT32 readVal = 0;

    WriteCR(0xF0, 0x1);             // Select rma ptr
    Platform::PioWrite32(0x3d0, reg);
    WriteCR(0xF0, 0x7);             // Select rma write data
    Platform::PioWrite32(0x3d0, wr_data);
    WriteCR(0xF0, 0x5);             // Select rma read data
    Platform::PioRead32(0x3d0, &readVal);

    if ((readVal & mask) != (exp_val & mask))
    {
        ErrPrintf("read_val&mask = %08x, exp_val = %08x\n", readVal & mask , exp_val);
        ErrPrintf("Error [TestCR]: CR Register mismatch... Index = %8x mask =%8x Read_Val=%8x Exp_Val=%8x\n", 
            reg, mask, readVal, exp_val);
        return 1;
    }
    else // read value matched the expected value
    { 
        return 0;
    } 
}

