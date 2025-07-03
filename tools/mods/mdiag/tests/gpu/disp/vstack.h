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
#ifndef _VSTACK_H
#define _VSTACK_H

#include "mdiag/utils/types.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "core/include/cmdline.h"
#include "mdiag/utils/buf.h"
#include "mdiag/utils/crc.h"

class vstack : public LWGpuSingleChannelTest {

public:
    IRegisterMap*  m_regMap;
    vstack(ArgReader *params);
    virtual ~vstack(void);
    static Test *Factory(ArgDatabase *args);
    virtual int Setup(void);
    void Run(void);
    void CleanUp(void);

private:
    void test_sanity(int mode);
    void test_manual_mode(int mode);
    void test_backpressure(int mode);
    void test_auto_mode(int mode);
    UINT16  GetCRTCAddr();
    void    WriteCR( UINT08 index, UINT08 data );
    UINT08  ReadCR( UINT08 index );
    bool    TestCR( UINT08 index, UINT08 wr_data, UINT08 exp_val, UINT08 mask );
    void    WriteLW( UINT32 reg, UINT32 data );
    UINT32  ReadLW( UINT32 reg );
    bool    TestLW( UINT32 reg, UINT32 wr_data, UINT32 exp_val, UINT32 mask);

private:
    UINT32 IgnoreFailures;            // Set to 1 to let test continue with failures

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(vstack, vstack, "Test host vstack");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &vstack_testentry
#endif

#endif // _VSTACK_H

