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
#ifndef _OVERTPIN_H_
#define _OVERTPIN_H_

// test dedicated overtpin for GK110

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/IRegisterMap.h"

class OvertPin : public LWGpuSingleChannelTest
{

public:

    OvertPin(ArgReader *params);

    virtual ~OvertPin(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual INT32 Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    UINT32 checkOvertDedicatedPin();
private:
    IRegisterMap* m_regMap;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(check_dedicated_overt_gk110, OvertPin, "GPU OvertPin Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &check_dedicated_overt_gk110_testentry
#endif

#endif  // _OVERTPIN_H_
