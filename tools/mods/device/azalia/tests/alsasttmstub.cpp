/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "alsasttm.h"

AlsaStreamTestModule::AlsaStreamTestModule()
: m_pStream(nullptr)
, m_CaptureHandle(nullptr)
{
}

AlsaStreamTestModule::~AlsaStreamTestModule()
{
    // Dummy code to avoid compiler warnings
    if (m_pStream)
        m_pStream = nullptr;
    if (m_CaptureHandle)
        m_CaptureHandle = nullptr;
}

RC AlsaStreamTestModule::Start()
{
    return RC::SOFTWARE_ERROR;
}

RC AlsaStreamTestModule::Continue()
{
    return RC::SOFTWARE_ERROR;
}

RC AlsaStreamTestModule::Stop()
{
    return RC::SOFTWARE_ERROR;
}

RC AlsaStreamTestModule::GetStream(UINT32 index, AzaStream **pStrm)
{
    return RC::SOFTWARE_ERROR;
}
