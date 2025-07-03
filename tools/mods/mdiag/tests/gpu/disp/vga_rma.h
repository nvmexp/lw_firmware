/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2013, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _VGA_RMA_H
#define _VGA_RMA_H

#include "mdiag/utils/types.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "core/include/cmdline.h"
#include "mdiag/utils/buf.h"
#include "mdiag/utils/crc.h"

class vga_rma : public LWGpuSingleChannelTest {

public:

    vga_rma(ArgReader *params);
    virtual ~vga_rma(void);
    static Test *Factory(ArgDatabase *args);
    virtual int Setup(void);
    void Run(void);
    void CleanUp(void);

private:
    int CheckRegister(unsigned int addr, unsigned int value, unsigned int mask);
    int CheckCRRegister(unsigned int addr, unsigned int value, unsigned int mask);
    int CheckRegister_RMA(unsigned int addr, unsigned int value, unsigned int mask);
    int test_palette();
    int check_iord_ack (unsigned adr, unsigned ack);
    unsigned int RMA_READ(unsigned int addr);
    void RMA_WRITE(unsigned int addr, unsigned int value);

    int RMA_SIZE;

    int testStatus;                     // Assume test succeed

};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(vga_rma, vga_rma, "test misc rom features");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &vga_rma_testentry
#endif

#endif // _VGA_RMA_H
