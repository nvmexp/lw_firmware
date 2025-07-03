/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * @file  notpll_test_kepler.h
 * @brief A Wrapper to encapsulate more common operations of NotPll tests
 */

#ifndef _NOTPLL_TEST_KEPLER_H_
#define _NOTPLL_TEST_KEPLER_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
//#include "notpll_common_kepler.h"
#include "mdiag/lwgpu.h"
#include "gpu_socv.h"

#define IGNORE_ERROR    false

// A new wrapper used for kepler, all new shared features should be
// implemented for each chip
// Wrapper class of NotPll tests, all tests should derive from this,
// and each chip should has its own implementation and instance
class NotPllTestKepler : public LWGpuSingleChannelTest
{
public:
    NotPllTestKepler(ArgReader *params);
    virtual ~NotPllTestKepler();

    //! Common interface to setup the test
    //! It's better to return RC but base class implemented as such
    //! Derived classes need to implement SetupInternal()
    int Setup();
    virtual SOCV_RC SetupInternal();
    //! Common interface to launch the real test logic
    //! It's better to return RC but base class implemented as such
    //! Derived classes need to implement RunInternal()
    void Run();
    virtual SOCV_RC RunInternal() = 0;

    //! Common interface to release resources
    virtual void CleanUp();
    //! Chip ID is used to instance the real object
    UINT32 GetChip() const { return m_Chip; }
    //! Return test name
    string GetName() const { return m_Name; }

protected:
    string          m_Name;
    UINT32          m_Chip;
    LWGpuResource*  m_lwgpu;
    IRegisterMap*   m_RegMap;
    //NotPllCommonKepler* m_NotPllCommon;
}; // End of NotPllTestKepler

#define CHECK_NOTPLL_TEST_RC(notpll_rc) \
    if (SOCV_OK != (rc = IGNORE_ERROR ? SOCV_OK : (notpll_rc))) \
    { \
        ErrPrintf("Debug Info:Function:%s :Line %d :RC = %d\n",__FUNCTION__,__LINE__,rc); \
        return rc; \
    }

#define CHECK_NOTPLL_TEST_RC_ZERO(notpll_rc_zero) \
    CHECK_NOTPLL_TEST_RC( 0 != (notpll_rc_zero) ? SOFTWARE_ERROR : SOCV_OK )

#endif  // _NOTPLL_TEST_KEPLER_H_
