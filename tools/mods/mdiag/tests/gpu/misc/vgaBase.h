/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _INSTANCEMEM_H
#define _INSTANCEMEM_H

#include "mdiag/utils/types.h"

class RandomStream;
class Rectangle;

#include "mdiag/tests/gpu/lwgpu_single.h"

class myMem {
public:
    myMem(void);
    virtual ~myMem(void);
    uintptr_t GetVBase() { return m_vbase; }
    UINT64 GetBase() { return m_base; }
    UINT32 GetSize() { return m_size; }
    UINT32 GetValid() { return m_valid;}
    void SetBase(UINT32 m) { m_base = m;}
    void SetVBase(uintptr_t v) { m_vbase = v;}
    void SetSize(UINT32 s) { m_size = s;}
    void SetValid(UINT32 i) { m_valid = i;}
    void Dump(void);
    int  Map(UINT64 base,UINT32 size);
    void UnMap(void);
private:
    uintptr_t m_vbase;
    UINT64 m_base;
    UINT32 m_size;
    UINT32 m_valid;
};

class vgaBaseTest : public LoopingTest
{
public:
    vgaBaseTest(ArgReader *params);

    virtual ~vgaBaseTest(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    UINT32 seed0,seed1,seed2;
    LWGpuResource*    lwgpu;

    RandomStream *RndStream;
    ArgReader *params;

    // legacy refers to 0xa0000 - 0xbffff range vga memory
    myMem m_text, m_graphic, m_vga, m_vga_ws;

    myMem * m_mem;    // mem access interface
    myMem * m_legacy; // legacy access interface

    UINT32 m_max_vga_ram;

    UINT32 m_test_step;

    bool SingleRun(void);

    typedef enum { TEXT_MODE, GRAPHIC_MODE } vmode;
    int SetMode(vmode v);
    int SetMode_io(vmode v);

    int UseWorkspaceRam(bool yes);
    int UseWorkspaceRam_io(bool yes);
    int SetVgaBase(UINT32 base,UINT32 target);
    int SetVgaWorkspaceBase(UINT32 base,UINT32 target);
    int InitVga(void);
    int InitVga_io(void);
    void DumpBarAndCfg(void);
    typedef enum { FB,VGARAM,FB_TO_VGARAM } vtype;
    int Verify(vtype t, int exact = 1);

private:
    UINT16  GetCRTCAddr();
    void    WriteCR( UINT08 index, UINT08 data );
    UINT08  ReadCR( UINT08 index );
    void    WriteSR( UINT08 index, UINT08 data );
    UINT08  ReadSR( UINT08 index );
    void    WriteGR( UINT08 index, UINT08 data );
    UINT08  ReadGR( UINT08 index );
    void    WriteLW( UINT32 reg, UINT32 data );
    UINT32  ReadLW( UINT32 reg );

    UINT32 m_vbaseorig;         // Preserve LW_PDISP_VGA_WORKSPACE_BASE_ADDR
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(vgaBase, vgaBaseTest, "Instance memory test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &vgaBase_testentry
#endif

#endif //_INSTANCEMEM_H
