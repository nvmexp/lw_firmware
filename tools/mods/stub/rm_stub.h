/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2013, 2017, 2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/**
 * @file    rm_stub.h
 * @brief   Stub out some LwRm functions
 */

#ifndef INCLUDED_RM_STUB_H
#define INCLUDED_RM_STUB_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_BASIC_STUB_H
#include "bascstub.h"
#endif

class LwRm
{
public:
    using Handle = UINT32;
};

class LwRmStub
{
public:
    typedef UINT32 Handle;
    void HandleResmanEvent(void *pOsEvent, UINT32 GpuInstance);
    RC WriteRegistryDword(const char *devNode, const char *parmStr, UINT32 data);

protected:
    LwRmStub();

private:
    friend class LwRmPtr;
    BasicStub m_Stub;
};

class LwRmPtr
{
public:
    explicit LwRmPtr() {}

    static LwRmStub s_LwRmStub;

    LwRmStub * Instance()   const { return &s_LwRmStub; }
    LwRmStub * operator->() const { return &s_LwRmStub; }
    LwRmStub & operator*()  const { return s_LwRmStub; }
};

RC RmApiStatusToModsRC(UINT32 Status);

#endif // INCLUDED_RM_STUB_H
