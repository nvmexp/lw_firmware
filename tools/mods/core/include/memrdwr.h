/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2002-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#define INCLUDED_MEMRDWR_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

class MemRdWr
{
public:
    MemRdWr();
    ~MemRdWr();
    UINT32 Rd32(void* addr);
    void Wr32(void* addr, UINT32 data);
    UINT16 Rd16(void* addr);
    void Wr16(void* addr, UINT16 data);
    UINT08 Rd08(void* addr);
    void Wr08(void* addr, UINT08 data);
    RC SetIndMemMode(bool UseIndirect);
    bool IsIndMemMode();
private:
    void * m_hChannel;
};

