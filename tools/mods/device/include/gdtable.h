/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GDTABLE_H
#define INCLUDED_GDTABLE_H

#include "core/include/memfrag.h"
#include "bufmgr.h"

class Controller;

class GDTable
{

public:
    enum TableType {SEQ, RING};

    GDTable();
    virtual ~GDTable();

    RC       ResetTable();
    RC       Init(UINT32 NumEntries, UINT32 ExtraSize = 0, Controller *pCtrl = nullptr);
    PHYSADDR GetBasePhys();
    void*    GetBaseVirt();
    PHYSADDR GetBaseExtraPhys();
    void*    GetBaseExtraVirt();
    UINT32   GetNumEntries();
    UINT32   GetMaxEntries();
    UINT32   GetMinEntries();
    RC       GetNextFreeEntry(void** ppNextEntry, UINT32* pNextEntryIndex);
    RC       GetEntryAddrPhys(UINT32 Index, PHYSADDR* pPhysAddr);
    RC       GetEntryAddrVirt(UINT32 Index, void** ppVirtAddr);

    RC       GetIndex(void * pVirtAddr,UINT32 * Index);
    void     SetBufferMgr(BufferMgr* BuffMgr);

    RC       WriteToBuffer(vector<UINT08>* pPattern);
    RC       WriteToBuffer(vector<UINT16>* pPattern);
    RC       WriteToBuffer(vector<UINT32>* pPattern);

    /* Function will fill each entry of the descriptor table based on
       the fragments passed in.
       This has to be implemented by the child classes as only they have
       knlowledge of the table structure
    */
    virtual RC FillTableEntries(MemoryFragment::FRAGLIST *pFragList) = 0;
    virtual bool IsEntryFree(void* pEntry) = 0;
    virtual RC   Print() = 0;
    virtual RC   PrintEntry(UINT32 Entry) = 0;
    virtual RC   PrintBuffer(UINT32 Entry = 0xffffffff) = 0;

protected:
    bool     m_IsInit;
    UINT32   m_NumEntries;
    UINT32   m_NextEntry;
    void*    m_BaseVirt;
    BufferMgr* m_pBuffMgr;

    void*    m_BaseExtraVirt;
    // These fields MUST be set in the child class contructors
    UINT32   m_MaxEntries;
    UINT32   m_MinEntries;
    UINT32   m_EntrySize;
    TableType m_TableType;
    UINT32   m_AddrBits;
    UINT32   m_PhysAlign;
    Memory::Attrib m_MemAttr;
    Controller * m_pCtrl;
};

#endif
