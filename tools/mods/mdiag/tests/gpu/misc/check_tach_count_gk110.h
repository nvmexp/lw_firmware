/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TACHCOUNT_H_
#define _TACHCOUNT_H_

// test the connnection about tach count for GK110

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class TachCount : public LWGpuSingleChannelTest
{

public:

    TachCount(ArgReader *params);

    virtual ~TachCount(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual INT32 Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    UINT32 check_connect_tach_count();
private:
    IRegisterMap* m_regMap;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(check_tach_count_gk110, TachCount, "GPU TachCount Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &check_tach_count_gk110_testentry
#endif

#endif  // _TACHCOUNT_H_
