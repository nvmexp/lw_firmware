/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2013, 2017 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "rm_stub.h"

LwRmStub LwRmPtr::s_LwRmStub;

LwRmStub::LwRmStub()
{
    m_Stub.SetStubMode(BasicStub::AllFail);
}

void LwRmStub::HandleResmanEvent(void *pOsEvent, UINT32 GpuInstance)
{
    m_Stub.LogCallVoid("LwRmStub::HandleResmanEvent");
}

RC LwRmStub::WriteRegistryDword(const char *devNode, const char *parmStr, UINT32 data)
{
    return OK;
}

RC RmApiStatusToModsRC(UINT32 Status)
{
    BasicStub stub;
    stub.SetStubMode(BasicStub::AllFail);
    return stub.LogCallRC("RmApiStatusToModsRC");
}
