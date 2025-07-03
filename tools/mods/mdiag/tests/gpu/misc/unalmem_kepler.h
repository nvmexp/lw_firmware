/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _UNALIGNED_SYSMEM_KEPLER_H_
#define _UNALIGNED_SYSMEM_KEPLER_H_

// very simple unaligned system memory diag - reads data from unaligned pointers in
// system memory

#include "notpll_test_kepler.h"

class NotPllUnalignedSysmemKepler: public NotPllTestKepler
{
public:
    NotPllUnalignedSysmemKepler(ArgReader *params);

    virtual ~NotPllUnalignedSysmemKepler();

    static Test *Factory(ArgDatabase *args);

    virtual SOCV_RC RunInternal();

protected:
    virtual SOCV_RC NotPllUnalignedSysmemTest() = 0;
    Surface m_SysMem;
    UINT32 sysmem_width;
    UINT32 sysmem_pitch;
    UINT32 sysmem_height;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(unal_sysmem_kepler, NotPllUnalignedSysmemKepler, "Accesses system memory at non-aligned addresses");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &unal_sysmem_kepler_testentry
#endif

// XXX collapse all of this code back into unalmem_kepler.cpp and get rid of the derived class
class NotPllUnalignedSysmemKepler_impl: public NotPllUnalignedSysmemKepler
{
public:
    NotPllUnalignedSysmemKepler_impl(ArgReader *params);
    virtual ~NotPllUnalignedSysmemKepler_impl();
protected:
    virtual SOCV_RC NotPllUnalignedSysmemTest();
};

#endif  // _UNALIGNED_SYSMEM_KEPLER_H_
