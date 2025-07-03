/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2014,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_SMBSTUB_H
#define INCLUDED_SMBSTUB_H
#include "core/include/types.h"
#include "core/include/rc.h"
#include <vector>

// Dummy declarations
class McpDev;
class SmbPort;
class SmbPort
{
public:
    SmbPort() {}
    void Search(vector<UINT08> *Addrs) {}
    RC RdByteCmd(UINT32 Addr, UINT32 Reg, UINT08 *Data) { return OK; }
    RC WrByteCmd(UINT32 Addr, UINT32 Reg, UINT08 Data) { return OK; }
};

class SmbDevice
{
public:
    SmbDevice() {}
    RC GetSubDev(UINT32 SubDevIndex, SmbPort **ppSubDev){ return OK; }
    RC GetNumSubDev(UINT32 * pNumSubDev);
};

class SmbManager
{
public:
    SmbManager() {}
    RC Find() { return OK; }
    RC InitializeAll() { return OK; }
    RC GetLwrrent(McpDev  **smbdev);
    void Uninitialize() { }
    void UninitializeAll() { }
    void Forget() { }
};

namespace SmbMgr
{
    SmbManager *Mgr();
}

#endif
