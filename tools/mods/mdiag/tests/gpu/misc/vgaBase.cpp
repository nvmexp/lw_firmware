/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2018, 2020-2021 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "mdiag/utils/randstrm.h"
#include "vgaBase.h"
#include "fermi/gf100/dev_disp.h"
#include "fermi/gf100/dev_bus.h"
#include "mdiag/tests/test_state_report.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"

#define IO_BEGIN(group,reg) {  \
   Platform::PioWrite08(LW##group##X,LW##group##reg##__INDEX); \
   UINT08 value; Platform::PioRead08(LW##group##reg, &value);  \
 InfoPrintf("IO_BEGIN: LW%s%s  (0x%08x) = %02x\n",#group,#reg,LW##group##reg,value);
#define IO_BEGIN2(group,reg,ext) \
   Platform::PioWrite08(LW##group##X##__ext,LW##group##reg##__INDEX); \
   UINT08 value;  Platform::PioRead08(LW##group##reg, &value);  \
 InfoPrintf("IO_BEGIN: LW%s%s  (0x%08x) = %02x\n",#group,#reg,LW##group##reg,value);

#define IO_SET_DEF(group,reg,field,def)  \
   value = FLD_SET_DRF(group,reg,field,def,value);

#define IO_SET_NUM(group,reg,field,num)  \
   value = FLD_SET_DRF_NUM(group,reg,field,num,value);

#define IO_END(group,reg) \
 Platform::PioWrite08(LW##group##reg,value); \
 UINT08 ret; Platform::PioRead08(LW##group##reg, &ret); \
 }

#define IO_DEF(group,reg,field,def) \
   IO_BEGIN(group,reg)  \
   IO_SET_DEF(group,reg,field,def); \
   IO_END(group,reg);

#define IO_NUM(group,reg,field,num) \
   IO_BEGIN(group,reg)  \
   IO_SET_NUM(group,reg,field,num); \
   IO_END(group,reg);

#define RND_MIN 0
#define RND_MAX 0xffffffff

//#define ERROR()  error++;return 1;
#define ERROR()  error++;

/*
vgaBase test:
    verify that access through a0000-bffff (or b8000-bffff in text mode)
    are the same as through bar1 + (vgaBase or vgaWorkSpaceBase)
    depending on modes

*/

extern const ParamDecl vgaBase_params[] = {
    UNSIGNED_PARAM("-loop", "Number of loops to run (default is 1)"),
    UNSIGNED_PARAM("-seed0",                "Random stream seed"),
    UNSIGNED_PARAM("-seed1",                "Random stream seed"),
    UNSIGNED_PARAM("-seed2",                "Random stream seed"),
    SIMPLE_PARAM("-verbose",                "print a bit more debug info"),
    UNSIGNED_PARAM("-vga_base",             "vga base address"),
    UNSIGNED_PARAM("-vga_ws_base",        "vga workspace base address"),
    { "-vga_target", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                       ParamDecl::GROUP_START),LW_PDISP_VGA_BASE_TARGET_PHYS_LWM, LW_PDISP_VGA_BASE_TARGET_PHYS_PCI_COHERENT, "vga ram base address" },
    { "-vga_lwm",      (const char*)LW_PDISP_VGA_BASE_TARGET_PHYS_LWM, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
                        0, 0, "vga ram in local FB" },
    { "-vga_noc",      (const char*)LW_PDISP_VGA_BASE_TARGET_PHYS_PCI, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
                        0, 0, "vga ram in non coherent memory" },
    { "-vga_coh",      (const char*)LW_PDISP_VGA_BASE_TARGET_PHYS_PCI_COHERENT, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
                        0, 0, "vga ram in coherent memory" },

    { "-vga_ws_target", "u", (ParamDecl::PARAM_ENFORCE_RANGE |
                       ParamDecl::GROUP_START),LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_LWM, LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_PCI_COHERENT, "vga_ws ram base address" },
    { "-vga_ws_lwm",      (const char*)LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_LWM, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
                        0, 0, "vga_ws ram in local FB" },
    { "-vga_ws_noc",      (const char*)LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_PCI, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
                        0, 0, "vga_ws ram in non coherent memory" },
    { "-vga_ws_coh",      (const char*)LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_PCI_COHERENT, ParamDecl::GROUP_MEMBER | ParamDecl::GROUP_MEMBER_UNSIGNED,
                        0, 0, "vga_ws ram in coherent memory" },

    FOREVER_PARAM,
    LAST_PARAM
};

/****************************************************************
 * simplify memory mapping interface
 ****************************************************************/

myMem::myMem(void)
{
    m_valid = 0;
}

myMem::~myMem(void)
{
    this->UnMap();
}

void myMem::UnMap(void)
{
    if ( m_valid ) {
        Platform::UnMapDeviceMemory(reinterpret_cast<void*>(m_vbase));
        InfoPrintf("myMem::Unmap 0x%08x - 0x%08x vbase = %08llx\n",m_base,m_base+m_size-1,(UINT64)m_vbase);
        m_valid = 0;
    }
}

// map the range of memory to use for test
int myMem::Map(UINT64 base,UINT32 size)
{
    RC rc;
    void * v;
    Memory::Attrib MapAttrib = Memory::UC;

    // first, if the previous one is valid, and the same,do nothing

    if ( m_valid  ) {
        if ( (base == m_base) && (size == m_size) ) {
            InfoPrintf("Already Map\n");
            Dump();
            return 0;
        } else {
            this->UnMap();
        }
    }

    // Assume a read/write mapping.
    rc = Platform::MapDeviceMemory(&v, base, size, MapAttrib,
                                   Memory::ReadWrite);

    if (OK != rc)
    {
        InfoPrintf("error mapping %x size %x\n",base,size);
        return 1;
    }

    m_vbase = reinterpret_cast<uintptr_t>(v);
    m_base = base;
    m_size = size;
    m_valid = 1;

    InfoPrintf("Done mapping\n");
    Dump();

    return 0;
}

void myMem::Dump(void)
{
    if ( m_valid ) {
        InfoPrintf("myMem: 0x%08x - 0x%08x vaddr = 0x%08llx\n",m_base,m_base+m_size-1,(UINT64)m_vbase);
    } else {
        InfoPrintf("myMem: invalid mapping\n");
    }

}
/****************************************************************
 * vgaBaseTest class
 ****************************************************************/
vgaBaseTest::vgaBaseTest(ArgReader *reader) :
    LoopingTest(reader)
{

    params = reader;

    seed0 = params->ParamUnsigned("-seed0", 0x1234);
    seed1 = params->ParamUnsigned("-seed1", 0x5678);
    seed2 = params->ParamUnsigned("-seed2", 0x9abc);

    m_test_step = 4096;
    m_mem = 0;
    m_legacy = 0;

    m_max_vga_ram = 0x20000;  // the vga window max is 0x20000(128K)
    m_vbaseorig = 0;
}

vgaBaseTest::~vgaBaseTest(void)
{
    // again, nothing special
    if(params) delete params;
}

STD_TEST_FACTORY(vgaBase, vgaBaseTest)

//#define DUMP_REG(x) InfoPrintf("%s (0x%x) = 0x%x\n",#x,x,lwgpu->GetGpuSubdevice()->RegRd32(x));
#define DUMP_REG(x)
// these two routines setup text or graphic mode,
// which decide the region of interet 0xa00000 or 0xb8000 to be used
int vgaBaseTest::SetMode(vmode v)
{

    // set mode,
    UINT32 value;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    DUMP_REG(LW_PDISP_VGA_GR_REG1);
    // graphic mode, odd_even, and write mode
    value = pSubdev->RegRd32(LW_PDISP_VGA_GR_REG1);
    if ( v == TEXT_MODE ) {
        value = FLD_SET_DRF(_PDISP,_VGA_GR_REG1,_GRAPHICS_MODE,_ALPHANUMERIC,value);
        value = FLD_SET_DRF(_PDISP,_VGA_GR_REG1,_MEMORY_MAP,_B8000_BFFFF,value);

        m_legacy = &m_text;
    } else {
        value = FLD_SET_DRF(_PDISP,_VGA_GR_REG1,_GRAPHICS_MODE,_GRAPHIC,value);
        value = FLD_SET_DRF(_PDISP,_VGA_GR_REG1,_MEMORY_MAP,_A0000_BFFFF,value);

        m_legacy = &m_graphic;
    }
    value = FLD_SET_DRF(_PDISP,_VGA_GR_REG1,_WRITE_MODE,_3,value);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_GR_REG1,_ODD_EVEN,1,value);
    pSubdev->RegWr32(LW_PDISP_VGA_GR_REG1,value);

    DUMP_REG(LW_PDISP_VGA_GR_REG1);

    // enable chain4 and ext memory
    DUMP_REG(LW_PDISP_VGA_SR_REG1);
    value = pSubdev->RegRd32(LW_PDISP_VGA_SR_REG1);
    value = FLD_SET_DRF(_PDISP,_VGA_SR_REG1,_EXT_MEMORY,_256K,value);

    value = FLD_SET_DRF_NUM(_PDISP,_VGA_SR_REG1,_CHAIN_4_ADDR,1,value);
    pSubdev->RegWr32(LW_PDISP_VGA_SR_REG1,value);
    DUMP_REG(LW_PDISP_VGA_SR_REG1);

    // enable all planes
    DUMP_REG(LW_PDISP_VGA_SR_REG0);
    value = pSubdev->RegRd32(LW_PDISP_VGA_SR_REG0);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_SR_REG0,_ENAB_PLANE0,1,value);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_SR_REG0,_ENAB_PLANE1,1,value);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_SR_REG0,_ENAB_PLANE2,1,value);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_SR_REG0,_ENAB_PLANE3,1,value);
    pSubdev->RegWr32(LW_PDISP_VGA_SR_REG0,value);
    DUMP_REG(LW_PDISP_VGA_SR_REG0);

    // turn on all bitmask
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_GR_REG2,_BIT_MASK,0xff,0);
    pSubdev->RegWr32(LW_PDISP_VGA_GR_REG2,value);

    InfoPrintf("vgaBaseTest:: Set mode  %s\n",v == TEXT_MODE?"Text":"Graphic");
    return 0;
}

int vgaBaseTest::SetMode_io(vmode v)
{
//    unsigned char data;

    if ( v == TEXT_MODE ) {
/*****************
  don't use register number and values, use symbolic definitions instead
        WriteGR(0x6, 0x0e);             // 0xB8000 - 0xBFFFF
//        WriteGR(0x5, 0x13);
        WriteGR(0x5, 0x10);
        WriteSR(0x2, 0x0f);             // Write to plane 0-3
        WriteSR(0x4, 0x02);
****************/

        IO_BEGIN(_VIO_GR,_MISC);
        IO_SET_DEF(_VIO_GR,_MISC,_GRAPHICS_MODE,_ALPHANUMERIC);
        IO_SET_NUM(_VIO_GR,_MISC,_CHAIN2,0);
        IO_SET_DEF(_VIO_GR,_MISC,_MEMORY_MAP,_B8000_BFFFF);
        IO_END(_VIO_GR,_MISC);

        // odd_even, and write mode
        IO_BEGIN(_VIO_GR,_MODE);
        IO_SET_DEF(_VIO_GR,_MODE,_WRITE_MODE,_0);
        IO_SET_DEF(_VIO_GR,_MODE,_READ_MODE,_0);
        IO_SET_NUM(_VIO_GR,_MODE,_ODD_EVEN,1);
        IO_END(_VIO_GR,_MODE);

        // enable chain4 and ext memory
        IO_BEGIN(_VIO_SR,_MEM_MODE);
        IO_SET_DEF(_VIO_SR,_MEM_MODE,_EXT_MEMORY,_256K);
        IO_SET_NUM(_VIO_SR,_MEM_MODE,_ODD_EVEN_ADDR,0);
        IO_SET_NUM(_VIO_SR,_MEM_MODE,_CHAIN_4_ADDR,1);
        IO_END(_VIO_SR,_MEM_MODE);

        m_legacy = &m_text;
    }
    else
    {
/*****************
  don't use register number and values, use symbolic definitions instead

        WriteGR(0x6, 0x01);             // 0xA0000 - 0xBFFFF
        WriteGR(0x5, 0x40);
        WriteSR(0x2, 0x0f);             // Write to plane 0-3
        WriteSR(0x4, 0x0e);
*****************/
        // GR6  graphic mode and memory map
        IO_BEGIN(_VIO_GR,_MISC);
        IO_SET_DEF(_VIO_GR,_MISC,_GRAPHICS_MODE,_GRAPHIC);
        IO_SET_NUM(_VIO_GR,_MISC,_CHAIN2,0);
        IO_SET_DEF(_VIO_GR,_MISC,_MEMORY_MAP,_A0000_BFFFF);
        IO_END(_VIO_GR,_MISC);

        // GR5
        IO_BEGIN(_VIO_GR,_MODE);
        IO_SET_DEF(_VIO_GR,_MODE,_WRITE_MODE,_0);
        IO_SET_DEF(_VIO_GR,_MODE,_READ_MODE,_0);
        IO_SET_NUM(_VIO_GR,_MODE,_ODD_EVEN,0);
        IO_SET_DEF(_VIO_GR,_MODE,_SHIFT_MODE,_256COLOR);
        IO_END(_VIO_GR,_MODE);

        // SR4 enable chain4 and ext memory
        IO_BEGIN(_VIO_SR,_MEM_MODE);
        IO_SET_DEF(_VIO_SR,_MEM_MODE,_EXT_MEMORY,_256K);
        IO_SET_NUM(_VIO_SR,_MEM_MODE,_ODD_EVEN_ADDR,0);
        IO_SET_NUM(_VIO_SR,_MEM_MODE,_CHAIN_4_ADDR,1);
        IO_END(_VIO_SR,_MEM_MODE);

        m_legacy = &m_graphic;
    }

    // GR3, host data access Not modified
    IO_BEGIN(_VIO_GR,_ROP);
    IO_SET_NUM(_VIO_GR,_ROP,_GRAPHIC_CNTL_ROTATE_COUNT,0);
    IO_SET_DEF(_VIO_GR,_ROP,_GRAPHIC_CNTL_RASTER_OP,_NOP);
    IO_END(_VIO_GR,_ROP);

    // SR2 enable all planes
    IO_BEGIN(_VIO_SR,_PLANE);
    IO_SET_NUM(_VIO_SR,_PLANE,_ENAB_PLANE0,1);
    IO_SET_NUM(_VIO_SR,_PLANE,_ENAB_PLANE1,1);
    IO_SET_NUM(_VIO_SR,_PLANE,_ENAB_PLANE2,1);
    IO_SET_NUM(_VIO_SR,_PLANE,_ENAB_PLANE3,1);
    IO_END(_VIO_SR,_PLANE);

    // set all plane bit mask to 1
    IO_NUM(_VIO_GR,_BIT_MASK,_BIT_MASK,0xff);

    // set sequencial chain4
    Platform::PioWrite08(LW_CIO_CRX__COLOR,LW_CIO_CRE_ENH__INDEX);
    UINT08 value;
    Platform::PioRead08(LW_CIO_CRE_ENH__COLOR, &value);
    value = FLD_SET_DRF(_CIO_CRE_ENH,_SEQUENTIAL,_CHAIN_4,_ENABLE,value);
    Platform::PioWrite08(LW_CIO_CRE_ENH__COLOR, value );
    InfoPrintf("write 0x%x to CIO_CRE_ENH__COLOR\n",value);

    InfoPrintf("vgaBaseTest:: Set mode (use io)  %s\n",v == TEXT_MODE?"Text":"Graphic");
    return 0;
}

int vgaBaseTest::UseWorkspaceRam(bool use)
{
    UINT32 value;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    value = pSubdev->RegRd32(LW_PDISP_VGA_CR_REG6);
    if ( use ) {
        value = FLD_SET_DRF(_PDISP,_VGA_CR_REG6,_WORKSPACE_RAM_ENAB,_ENABLE,value);
    } else {
        value = FLD_SET_DRF(_PDISP,_VGA_CR_REG6,_WORKSPACE_RAM_ENAB,_DISABLE,value);
    }
    pSubdev->RegWr32(LW_PDISP_VGA_CR_REG6,value);

    InfoPrintf("WorkspaceRam %s: write LW_PDISP_VGA_CR_REG6 (0x%x) = 0x%x\n",
               (use==true) ? "enable":"disable",
               LW_PDISP_VGA_CR_REG6,value);
    m_mem = use ? &m_vga_ws : &m_vga;
    return 0;
}

// use vga_base or vga_workspace_base, using IO register to set
int vgaBaseTest::UseWorkspaceRam_io(bool use)
{
    UINT08 value;

    Platform::PioWrite08(LW_CIO_CRX__COLOR,LW_CIO_CRE_ENH__INDEX);
    Platform::PioRead08(LW_CIO_CRE_ENH__COLOR, &value);
    if (use) 
    {
        value = FLD_SET_DRF(_CIO_CRE_ENH,_WORKSPACE_RAM,_ENAB,_ENABLE,value);
    } 
    else 
    {
        value = FLD_SET_DRF(_CIO_CRE_ENH,_WORKSPACE_RAM,_ENAB,_DISABLE,value);
    }
    Platform::PioWrite08(LW_CIO_CRE_ENH__COLOR, value);

    InfoPrintf("WorkspaceRam %s: write LW_CIO_CRE_ENH__COLOR (0x%x) = 0x%x\n",
               (use==true) ? "enable":"disable",
               LW_CIO_CRE_ENH__COLOR,value);

    m_mem = use ? &m_vga_ws : &m_vga;
    return 0;
}

int vgaBaseTest::SetVgaBase(UINT32 base,UINT32 target)
{
    UINT32 value = 0;
    UINT64 mem_base = base;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    if ( target == LW_PDISP_VGA_BASE_TARGET_PHYS_PCI_COHERENT  ||
         target == LW_PDISP_VGA_BASE_TARGET_PHYS_PCI ) {
        // do nothing
    } else {
        // default to use LWM
        target = LW_PDISP_VGA_BASE_TARGET_PHYS_LWM;
        UINT64 fbBase = pSubdev->GetPhysFbBase();
        MASSERT(fbBase != Gpu::s_IlwalidPhysAddr);
        mem_base  += fbBase;
    }

    value = FLD_SET_DRF_NUM(_PDISP,_VGA_BASE,_TARGET,target,value);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_BASE,_ADDR, (base>>((0?LW_PDISP_VGA_BASE_ADDR)+8)),value);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_BASE,_STATUS, LW_PDISP_VGA_BASE_STATUS_VALID,value);
    pSubdev->RegWr32(LW_PDISP_VGA_BASE,value);

    // need to map the memory to a virtual base
    return m_vga.Map(mem_base,m_max_vga_ram);
}

int vgaBaseTest::SetVgaWorkspaceBase(UINT32 base,UINT32 target)
{
    UINT32 value = 0;
    UINT64 mem_base = base;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    if ( target == LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_PCI_COHERENT  ||
         target == LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_PCI ) {
        // do nothing
    } else {
        // default to use LWM
        target = LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_LWM;
        UINT64 fbBase = pSubdev->GetPhysFbBase();
        MASSERT(fbBase != Gpu::s_IlwalidPhysAddr);
        mem_base  += fbBase;
    }

    value = FLD_SET_DRF_NUM(_PDISP,_VGA_WORKSPACE_BASE,_TARGET,target,value);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_WORKSPACE_BASE,_ADDR, (base>>((0?LW_PDISP_VGA_WORKSPACE_BASE_ADDR)+8)),value);
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_WORKSPACE_BASE,_STATUS, LW_PDISP_VGA_WORKSPACE_BASE_STATUS_VALID,value);
    pSubdev->RegWr32(LW_PDISP_VGA_WORKSPACE_BASE,value);
    InfoPrintf("Loading LW_PDISP_VGA_WORKSPACE_BASE with 0x%08x\n", value);

    // need to map the memory to a virtual base
    return m_vga_ws.Map(mem_base,m_max_vga_ram);
}

// this verify the given mode, if something wrong, report and exit
int vgaBaseTest::Verify(vtype t, int exact)
{
    int error = 0;

    uintptr_t a1,a2;
    UINT32 offset,i;
    uintptr_t a1_base,a2_base;
    UINT32 din,dout;
    UINT32 test_size;

    switch (t) {
    case FB:
        a1_base = m_mem->GetVBase();
        a2_base = a1_base;
        InfoPrintf("verify FB\n");
        m_mem->Dump();
        test_size =  m_mem->GetSize()  / m_test_step;
        break;
    case VGARAM:
        a1_base = m_legacy->GetVBase();
        a2_base = a1_base;
        InfoPrintf("verify VGA_RAM\n");
        m_legacy->Dump();
        test_size =  m_legacy->GetSize()  / m_test_step;
        break;
    case FB_TO_VGARAM:
    default:
        a1_base = m_mem->GetVBase();
        a2_base = m_legacy->GetVBase();
        InfoPrintf("verify FB_TO_VGARAM 0x%08x 0x%08x\n", a1_base, a2_base);
        m_mem->Dump();
        m_legacy->Dump();
//        test_size =  m_mem->GetSize()  / m_test_step;
        test_size =  m_legacy->GetSize()  / m_test_step;
        break;
    }

    InfoPrintf("do %s use exact match\n",exact?"":"not");

    for (i=0,offset=0;i<test_size;i++,offset+= m_test_step) {
        a1 = a1_base + offset;
        a2 = a2_base + offset;
        if ( exact ) {
            // first write through bar1, and read from normal vga space
            din = RndStream->RandomUnsigned(RND_MIN,RND_MAX);
            DebugPrintf("mem write:  0x%08x with 0x%08x\n",a1,din);
            MEM_WR32(a1, din);
            if ( t == FB_TO_VGARAM )
            {
                  static bool haveDisplayedMsg = false;
                  if ( haveDisplayedMsg == false )
                  {
                      InfoPrintf( "see lwbug 319400, which is about igpu diffilwlty with this test.\n");
                      InfoPrintf( "However, discrete gpu's technically have a similar race condition.\n");
                      InfoPrintf( "Here is Dennis Ma's comment:\n");
                      InfoPrintf( "On dGPU, there actually is a slight race condition, because it's technically possible\n");
                      InfoPrintf( "that a write through bar1 might not be ack'd before the following read goes over PRI\n");
                      InfoPrintf( "to display to dniso2fb, but this would require a severly backed-up fb, which isn't likely\n");
                      InfoPrintf( "to happen if you're in vga mode (plus I was under the impression that VESA/bar traffic\n");
                      InfoPrintf( "doesn't mix much w/ vga mem aperture traffic). So you still have a slight race in dGPU,\n");
                      InfoPrintf( "but to hit it in a testbench, you'd probably have to artificially force backpressure to expose it.\n");
                      haveDisplayedMsg = true;
                  }
                  dout = MEM_RD32(a1); // force the write to the point of coherency
            }
            dout = MEM_RD32(a2);
            DebugPrintf("mem read:   0x%08x with 0x%08x\n",a2,dout);
            if (din != dout) {
                ErrPrintf("mismatch at offset 0x%x wrote 0x%x read  0x%x\n",offset,din,dout);
                ERROR();
            }
        } else {
            // if don't want to do exact test,  then test for change
            // first write '0' a1, read some value, then write 0xffffff,
            // read back and make sure it changes
            MEM_WR32(a1,0);
            din = MEM_RD32(a2);
            MEM_WR32(a1,0xffffffff);
            dout = MEM_RD32(a2);
            DebugPrintf("offset 0x%08x write to address 0x%08x read back from 0x%08x write 0 read %x write 0xffffffff read %x\n",offset,a1,a2,din,dout);
            if ( din == dout ) {
                ErrPrintf("offset 0x%x is not changes, that is suspicious\n",offset);
                ERROR();
            }
        }
    }

#if 0
    // not sure I want to do this path, the result seems unpredictable
    if ( t == FB_TO_VGARAM ) {
        // reverse the direction,
        // write through vga space, and read back from bar1
        InfoPrintf("for FB_TO_VGARAM, also need to write through FB and read from VGA\n");
        for (i=0,offset=0;i<test_size;i++,offset+= m_test_step) {
            a1 = a1_base + offset;
            a2 = a2_base + offset;

            if ( exact ) {
                din = RndStream->RandomUnsigned(RND_MIN,RND_MAX);
                MEM_WR32(a2, din);
                dout = MEM_RD32(a1);
                if (din != dout) {
                    ErrPrintf("mismatch at offset 0x%x wrote 0x%x read  0x%x\n",offset,din,dout);
                    ERROR();
                }
            } else {
                // if don't want to do exact test,  then test for change
                // first write '0' a1, read some value, then write 0xffffff,
                // read back and make sure it changes
                MEM_WR32(a1,0);
                din = MEM_RD32(a2);
                MEM_WR32(a1,0xffffffff);
                dout = MEM_RD32(a2);
                DebugPrintf("write to address 0x%08x read back from 0x%08x write 0 read %x write 0xffffffff read %x\n",a1,a2,din,dout);
                if ( din == dout ) {
                    ErrPrintf("offset 0x%x is not changes, that is suspicious\n",offset);
                    ERROR();
                }
            }

        }
    }
#endif

    return error;
}

// use memory access1
int vgaBaseTest::InitVga(void)
{
    UINT32 value;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    // grab vga
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_CR_REG58,_SET_DISP_OWNER,1,0);
    pSubdev->RegWr32(LW_PDISP_VGA_CR_REG58,value);
    // enable vga
    pSubdev->RegWr32(LW_PDISP_VGA_GEN_REGS_VSE2,1);
    UINT08 temp;
    Platform::PioRead08(LW_VIO_VSE2, &temp);
    InfoPrintf("done enable vga %x\n", temp);

    // allow host access to display ram and
    // mapp CRTC reg's to 0x3DX
    value = pSubdev->RegRd32(LW_PDISP_VGA_GEN_REGS_MISC)
;
    value = FLD_SET_DRF(_PDISP,_VGA_GEN_REGS_MISC,_IO_ADR,_03DX,value);
    value = FLD_SET_DRF(_PDISP,_VGA_GEN_REGS_MISC,_EN_RAM,_ENAB,value);
    pSubdev->RegWr32(LW_PDISP_VGA_GEN_REGS_MISC,value);

    // Unlock ext register access
    value = FLD_SET_DRF(_PDISP,_VGA_CR_REG15,_EXT_REG_ACCESS,_UNLOCK_RW,0);
    pSubdev->RegWr32(LW_PDISP_VGA_CR_REG15,value);

    return 0;
}

int vgaBaseTest::InitVga_io(void)
{
    UINT32 value;
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    // grab vga
    value = FLD_SET_DRF_NUM(_PDISP,_VGA_CR_REG58,_SET_DISP_OWNER,1,0);
    pSubdev->RegWr32(LW_PDISP_VGA_CR_REG58,value);
    // enable VGA
    Platform::PioWrite08(LW_VIO_VSE2, 1);
    UINT08 temp;
    Platform::PioRead08(LW_VIO_VSE2, &temp);
    InfoPrintf("done enable io vga %x\n",temp);

    // allow host access to display ram and
    // mapp CRTC reg's to 0x3DX
    Platform::PioWrite08(LW_VIO_MISC__WRITE, 3); // allow host access  to display ram and

    // Unlock ext register access
    Platform::PioWrite08(LW_CIO_CRX__COLOR, LW_CIO_CRE_LOCK__INDEX);
    Platform::PioWrite08(LW_CIO_CRE_LOCK__COLOR,
                         LW_CIO_CRE_LOCK_EXT_REG_ACCESS_UNLOCK_RW);
    UINT08 val;
    Platform::PioRead08(LW_CIO_CR_HDT__COLOR, &val);
    if (val != LW_CIO_CRE_LOCK_EXT_REG_ACCESS_RD_WR) {
        ErrPrintf ("Can't unlock Extended CR registers\n");
        return 1;
    }

    // Read back some CRE register to see if it is at INIT
    // How do locked registers read back? Zero, so testing
    // a register that inits to zero is stupid.  Register
    // reset is tested in other tests, so we will not bother
    return 0;
}

// sanity check cfg space, used only for debugging
// no realy meaning
void vgaBaseTest::DumpBarAndCfg()
{
    WarnPrintf("DumpBarAndCfg() is disabled\n");
#if 0
    InfoPrintf("CfgBase = 0x%x\n",lwgpu->GetCfgBase());
    UINT32 i;
    for (i=0;i<3;i++) {
        InfoPrintf("bar%d base = 0x%08x\n",i,lwgpu->GetBARBase(i));
    }
    for (i=0;i<0x20;i+=4) {
        UINT32 value = lwgpu->CfgRd32(i);
        InfoPrintf("Cfg%02d = 0x%08x\n",i,value);
    }
#endif
}

//****************************************************************
// Standard test routines
//****************************************************************
int vgaBaseTest::Setup(void)
{
    GpuSubdevice *pSubdev;

    lwgpu = LWGpuResource::FindFirstResource();

    getStateReport()->enable();
    InfoPrintf("vgaBaseTest: Setup() starts\n");

    RndStream = new RandomStream(seed0, seed1, seed2);

    if ( params->ParamPresent("-verbose") ) {
        DumpBarAndCfg();
    }

    pSubdev = lwgpu->GetGpuSubdevice();
    DebugPrintf("vgaBaseTest: Setup(): this gpu has 'device id' 0x%.8x (not pci devid)\n", pSubdev->DeviceId());

    if ( InitVga_io() ) {
        InfoPrintf("fail to initialize vga, setup fail\n");
        return 0;
    }

    m_vbaseorig = pSubdev->RegRd32( LW_PDISP_VGA_WORKSPACE_BASE );

  //In gf100, mode is controlled by LW_PBUS_BAR1_BLOCK
  UINT32 data = pSubdev->RegRd32(LW_PBUS_BAR1_BLOCK);
  if (DRF_VAL(_PBUS, _BAR1_BLOCK, _MODE, data) == LW_PBUS_BAR1_BLOCK_MODE_VIRTUAL)
  {
      InfoPrintf("BAR1 in virtual mode, resetting to physical for Gpu>=GF100!\n");
      data = data & 0x7fffffff;
      pSubdev->RegWr32(LW_PBUS_BAR1_BLOCK, data);
   } 
   else
   {
       InfoPrintf("BAR1 in physical mode %x\n", data);
   }

    if ( m_graphic.Map(0xa0000,0x10000) ) {
        InfoPrintf("fail to map vga memory range 0xa0000 - 0xaffff\n");
        return 0;
    }

    if ( m_text.Map(0xb8000,0x8000) ) {
        InfoPrintf("fail to map vga memory range 0xb8000 - 0xbffff\n");
        return 0;
    }

    // default to use graphic mode
    SetMode_io(GRAPHIC_MODE);

    // later should give a command line arguement like:
    // -vga_target xx -vga_ws_target
    UINT32 vga_target = params->ParamUnsigned("-vga_target", LW_PDISP_VGA_BASE_TARGET_PHYS_LWM);
    UINT32 vga_ws_target = params->ParamUnsigned("-vga_ws_target",LW_PDISP_VGA_WORKSPACE_BASE_TARGET_PHYS_LWM);
    UINT32 vga_base = params->ParamUnsigned("-vga_base",0);
    UINT32 vga_ws_base = params->ParamUnsigned("-vga_ws_base",0x40000);

    // base addresses must be aligned to 256K
    if (vga_base & 0x3ffff) {
        ErrPrintf("-vga_base (0x%08x) not 256K aligned\n", vga_base);
        return 0;
    }
    if (vga_ws_base & 0x0ffff) {
        ErrPrintf("-vga_ws_base (0x%08x) not 64K aligned\n", vga_ws_base);
        return 0;
    }

    if ( ((vga_base >= vga_ws_base) && (vga_base-vga_ws_base < m_max_vga_ram)) ||
         ((vga_ws_base > vga_base) && (vga_ws_base -vga_base <m_max_vga_ram))) {
        WarnPrintf("vga_base (0x%08x) must diff vga_ws_base (0x%08x) more than %d to work properly\n",
                   vga_base,vga_ws_base,m_max_vga_ram);
    }

    // make sure vga base and workspace base has a little offset
    // default to use LWM as target
    if ( SetVgaBase(vga_base,vga_target) ) {
        return 0;
    }
    if (SetVgaWorkspaceBase(vga_ws_base,vga_ws_target) ){
        return 0;
    }

    InfoPrintf("vgaBaseTest: Setup() done succesfully\n");
    return(1);
}

// a little extra cleanup to be done
void vgaBaseTest::CleanUp(void)
{
    GpuSubdevice *pSubdev = lwgpu->GetGpuSubdevice();

    DebugPrintf("vgaBaseTest: CleanUp() starts\n");

    // restoring LW_PDISP_VGA_WORKSPACE_BASE value
    pSubdev->RegWr32( LW_PDISP_VGA_WORKSPACE_BASE, m_vbaseorig );

    if (RndStream) delete RndStream;

    DebugPrintf("vgaBaseTest: CleanUp() done\n");
}

// the real test
void vgaBaseTest::Run(void)
{
    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("vgaBaseTest: Run() starts\n");

    do {
        if (SingleRun() == false) {
            SetStatus(Test::TEST_FAILED);
            getStateReport()->runFailed("run failed, see errors printed!\n");
            return;
        }
    } while (KeepLooping());

    InfoPrintf("vgaBaseTest: Run() ends\n");
    SetStatus(Test::TEST_SUCCEEDED);
    getStateReport()->crcCheckPassedGold();
    return;
}

#define _VERIFY(x,y)  if ( Verify(x,y) ) return false; else InfoPrintf("pass.\n");
bool vgaBaseTest::SingleRun()
{
    static const vmode m[2] = { GRAPHIC_MODE ,TEXT_MODE };
    static const bool ws[2] = { true,false };

    for (int i=0;i<2;i++) {
        SetMode_io(m[i]);
        for ( int j =0;j<2;j++ ) {
            UseWorkspaceRam_io(ws[j]);
            _VERIFY(FB,1);              // make sure fb is accessible
            _VERIFY(VGARAM,1);          // make sure vga buffer is accessible
            _VERIFY(FB_TO_VGARAM,1);    // make sure can write to one and read from other

        }
    }

    return true;
}

/**
 * GetCRTCAddr: this function returns crtc base address 0x3b4 or 0x3d4
 */
UINT16    vgaBaseTest::GetCRTCAddr()
{
        UINT16     base;

        UINT08 val;
        Platform::PioRead08(0x3cc, &val);
        if(val&0x01)
            base = 0x3d4;
        else
            base = 0x3b4;

        return base;
}

/**
 * WriteCR: write data to CR register.
 */
void    vgaBaseTest::WriteCR( UINT08 index, UINT08 data )
{
        UINT16     addr;

        addr = GetCRTCAddr();
        Platform::PioWrite08( addr, index);
        Platform::PioWrite08( addr+1, data);
}

/**
 * ReadCR: read a CR register.
 */
UINT08    vgaBaseTest::ReadCR( UINT08 index )
{
        UINT16     addr;
        UINT08     data;

        addr = GetCRTCAddr();
        Platform::PioWrite08( addr, index);

        Platform::PioRead08(addr+1, &data);
        return data;
}

/**
 * WriteSR: write data to SR register.
 */
void    vgaBaseTest::WriteSR( UINT08 index, UINT08 data )
{
    Platform::PioWrite08( 0x3c4, index);
    Platform::PioWrite08( 0x3c5, data);
}

/**
 * ReadCR: read a SR register.
 */
UINT08    vgaBaseTest::ReadSR( UINT08 index )
{
        UINT08     data;

        Platform::PioWrite08( 0x3c4, index);
        Platform::PioRead08( 0x3c5, &data );
        return data;
}

/**
 * WriteGR: write data to GR register.
 */
void    vgaBaseTest::WriteGR( UINT08 index, UINT08 data )
{
    Platform::PioWrite08( 0x3ce, index);
    Platform::PioWrite08( 0x3cf, data);
}

/**
 * ReadCR: read a GR register.
 */
UINT08    vgaBaseTest::ReadGR( UINT08 index )
{
        UINT08     data;

        Platform::PioWrite08( 0x3ce, index);
        Platform::PioRead08( 0x3cf, &data );
        return data;
}

