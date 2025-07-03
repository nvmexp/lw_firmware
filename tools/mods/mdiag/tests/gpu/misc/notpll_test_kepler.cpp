/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "notpll_test_kepler.h"

NotPllTestKepler::NotPllTestKepler
(
    ArgReader *params
)
: LWGpuSingleChannelTest(params), m_Name("NoName"), m_Chip(0),
  m_lwgpu(0), m_RegMap(0)
{
    m_lwgpu  = LWGpuResource::FindFirstResource();
    m_Chip   = m_lwgpu->GetArchitecture();
    m_RegMap = m_lwgpu->GetRegisterMap();
}

NotPllTestKepler::~NotPllTestKepler()
{
    // CleanUp() handles most of the cleaning job
    // Nothing need to be done here
}

int NotPllTestKepler::Setup()
{
    if (SetupInternal() == SOCV_OK)
    {
        return 1; // Success
    }
    return 0;
}

SOCV_RC NotPllTestKepler::SetupInternal()
{
    // Initialize members of base class
    lwgpu = LWGpuResource::FindFirstResource();
    ch = lwgpu->CreateChannel(); // Member of parent class
    if (!ch)
    {
        ErrPrintf("%s: failed to initialize resources\n", m_Name.c_str());
        return SOFTWARE_ERROR;
    }
    getStateReport()->init(m_Name.c_str());
    getStateReport()->enable();

    // Own members
    m_RegMap = lwgpu->GetRegisterMap();
    m_Chip   = lwgpu->GetArchitecture();

    return SOCV_OK; // Success
}

void NotPllTestKepler::Run()
{
    if (this->RunInternal() == SOCV_OK)
    {
        InfoPrintf("Test %s has passed\n", m_Name.c_str());
    }
    else
    {
        InfoPrintf("Test %s failed\n", m_Name.c_str());
    }
}

void NotPllTestKepler::CleanUp()
{
    if (ch)
    {
        delete ch;
        ch = 0;
    }
    /*if (m_NotPllCommon)
    {
        delete m_NotPllCommon;
        m_NotPllCommon = 0;
    }*/
    if (lwgpu)
    {
        InfoPrintf("%s: cleaning up to release GPU\n", m_Name.c_str());
        lwgpu = 0;
    }
    LWGpuSingleChannelTest::CleanUp();
}

