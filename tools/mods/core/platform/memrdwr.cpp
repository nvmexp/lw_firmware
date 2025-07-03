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
#ifndef INCLUDED_MEMRDWR_H
#include "core/include/memrdwr.h"
#endif
#include "core/include/platform.h"
#include "core/include/massert.h"

#define NULL 0

MemRdWr::MemRdWr()
{
    m_hChannel = NULL;
}
//-----------------------------------------------------------------------------
MemRdWr::~MemRdWr()
{
}
//-----------------------------------------------------------------------------
UINT32 MemRdWr::Rd32(void* addr)
{
    return MEM_RD32(addr);
}
//-----------------------------------------------------------------------------
UINT16 MemRdWr::Rd16(void* addr)
{
    return MEM_RD16(addr);
}
//-----------------------------------------------------------------------------
UINT08 MemRdWr::Rd08(void* addr)
{
    return MEM_RD08(addr);
}
//-----------------------------------------------------------------------------
void MemRdWr::Wr32(void* addr, UINT32 data)
{
    MEM_WR32(addr, data);
}
//-----------------------------------------------------------------------------
void MemRdWr::Wr16(void* addr, UINT16 data)
{
    MEM_WR16(addr, data);
}
//-----------------------------------------------------------------------------
void MemRdWr::Wr08(void* addr, UINT08 data)
{
    MEM_WR08(addr, data);
}
//-----------------------------------------------------------------------------
RC MemRdWr::SetIndMemMode(bool dummy)
{
    MASSERT(dummy == false);   //true only for WMP
    return OK;  //dummy function
}
//-----------------------------------------------------------------------------
bool MemRdWr::IsIndMemMode()
{
    return false;
}

//
