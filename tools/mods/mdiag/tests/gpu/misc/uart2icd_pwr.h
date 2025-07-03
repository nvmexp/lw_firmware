/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _UART2ICD_PWR_H_
#define _UART2ICD_PWR_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

class uart2icd_pwr : public LWGpuSingleChannelTest {
public:
    uart2icd_pwr(ArgReader *params);

    virtual ~uart2icd_pwr(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    virtual void DelayNs(UINT32);

protected:
    int uart2icd_sanityTest();

private:
    //UINT32 m_arch;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(uart2icd_pwr, uart2icd_pwr, "uart2icd_pwr  Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &uart2icd_pwr_testentry
#endif

#endif  // _UART2ICD_PWR_H_
