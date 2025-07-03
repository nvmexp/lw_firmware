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

// UnalignedMemTest interface tests.

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "core/include/tee.h"
#include "gpu/utility/surf2d.h"
#include "core/include/platform.h"

#include "unalmem_kepler.h"

extern const ParamDecl unal_sysmem_kepler_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    LAST_PARAM
};

NotPllUnalignedSysmemKepler::NotPllUnalignedSysmemKepler
(
    ArgReader *params
)
: NotPllTestKepler(params)
{
}

NotPllUnalignedSysmemKepler::~NotPllUnalignedSysmemKepler()
{
}

Test* NotPllUnalignedSysmemKepler::Factory(ArgDatabase *args)
{
    ArgReader *reader = new ArgReader(unal_sysmem_kepler_params);
    if (!reader || !reader->ParseArgs(args))
    {
        ErrPrintf("unal_sysmem_kepler: Factory() error reading parameters!\n");
        return 0;
    }

    // All chips share one version of this test now (XXX collapse the _impl code back into this file)
    return new NotPllUnalignedSysmemKepler_impl(reader);
}

//------------------------------------------------------------------------------
// Run the test.
//------------------------------------------------------------------------------
SOCV_RC NotPllUnalignedSysmemKepler::RunInternal()
{
    SOCV_RC rc = SOCV_OK;
    getStateReport()->init("UnalignedSysmemTest");
    getStateReport()->enable();
    SetStatus(TEST_INCOMPLETE);
    rc = this->NotPllUnalignedSysmemTest();
    if (SOCV_OK == rc)
    {
        SetStatus(TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
        InfoPrintf("Test passed\n");
    }
    else
    {
        SetStatus(TEST_FAILED);
        getStateReport()->runFailed("Test failed\n");
        InfoPrintf("Test failed\n");
    }
    return rc;
}
