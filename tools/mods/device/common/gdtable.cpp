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

#include "device/include/gdtable.h"
#include "device/include/memtrk.h"
#include "core/include/platform.h"
#include "cheetah/include/logmacros.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "GDTable"

GDTable::GDTable()
{
    m_NumEntries = 0;
    m_MaxEntries = 0;
    m_MinEntries = 0;
    m_EntrySize = 0;
    m_BaseVirt = nullptr;
    m_NextEntry = 0;
    m_TableType = SEQ;
    m_AddrBits = 32;
    m_PhysAlign = 4;
    m_MemAttr = Memory::WB;
    m_IsInit = false;
    m_pBuffMgr = nullptr;
    m_BaseExtraVirt = nullptr;
    m_pCtrl = nullptr;
}

GDTable::~GDTable()
{
    MemoryTracker::FreeBuffer(m_BaseExtraVirt);
}

PHYSADDR GDTable::GetBasePhys()  { return Platform::VirtualToPhysical(m_BaseVirt); }
void*    GDTable::GetBaseVirt()  { return m_BaseVirt; }
void*    GDTable::GetBaseExtraVirt()  { return m_BaseExtraVirt; }
PHYSADDR GDTable::GetBaseExtraPhys()  { return Platform::VirtualToPhysical(m_BaseExtraVirt); }
UINT32   GDTable::GetNumEntries(){ return m_NumEntries; }
UINT32   GDTable::GetMaxEntries(){ return m_MaxEntries; }
UINT32   GDTable::GetMinEntries(){ return m_MinEntries; }

// Mainly for ahci whose'prdt shares the same continuous memory region with other items
RC GDTable::Init(UINT32 NumEntries, UINT32 ExtraSize, Controller *pCtrl)
{

    LOG_ENT();
    RC rc = OK;

    ExtraSize = (ExtraSize + 3) & ~3; //align on dword
    if (!m_IsInit)
    {
        m_pCtrl = pCtrl;
        m_NumEntries = NumEntries;
        CHECK_RC(MemoryTracker::AllocBufferAligned(ExtraSize + m_EntrySize * m_NumEntries,
                                                   &m_BaseExtraVirt, m_PhysAlign,
                                                   m_AddrBits, m_MemAttr, pCtrl));
        m_BaseVirt = (char*)m_BaseExtraVirt + ExtraSize;
        CHECK_RC(ResetTable());
        m_IsInit = true;
    }

    LOG_EXT();
    return rc;
}

RC GDTable::ResetTable()
{
    m_NextEntry = 0;
    Memory::Fill32(m_BaseVirt, 0, m_NumEntries * (m_EntrySize/4));

    return OK;
}

RC GDTable::GetNextFreeEntry(void** ppNextEntryAddr, UINT32* pNextEntryIndex)
{
    LOG_ENT();
    RC rc = OK;

    if (m_TableType == SEQ)
    {
        if (m_NextEntry >= m_NumEntries)
        {
            Printf(Tee::PriError, "All entries in DTable have been filled\n");
            return RC::BAD_PARAMETER;
        }

        for(UINT32 i = 0; i < m_NumEntries; ++i)
        {
            CHECK_RC(GetEntryAddrVirt(m_NextEntry, ppNextEntryAddr));
            *pNextEntryIndex = m_NextEntry++;

            if (IsEntryFree(*ppNextEntryAddr))
                break;
        }
    }
    else if (m_TableType == RING)
    {
        for(UINT32 i = 0; i < m_NumEntries; ++i)
        {
            CHECK_RC(GetEntryAddrVirt(m_NextEntry, ppNextEntryAddr));
            *pNextEntryIndex = m_NextEntry;
            m_NextEntry = (m_NextEntry + 1) % m_NumEntries;

            if (IsEntryFree(*ppNextEntryAddr))
                break;
        }
    }

    LOG_EXT();
    return rc;
}

RC GDTable::GetEntryAddrPhys(UINT32 Index, PHYSADDR* pPhysAddr)
{
    RC rc = OK;

    if (Index >= m_NumEntries)
    {
        Printf(Tee::PriError, "Invalid Entry\n");
        return RC::BAD_PARAMETER;
    }

    void* virtAddr = 0;
    CHECK_RC(GetEntryAddrVirt(Index, &virtAddr));

    if (pPhysAddr)
        *pPhysAddr = Platform::VirtualToPhysical(virtAddr);

    return rc;
}

RC GDTable::GetEntryAddrVirt(UINT32 Index, void** ppVirtAddr)
{
    if (Index >= m_NumEntries)
    {
        Printf(Tee::PriError, "Invalid Entry\n");
        return RC::BAD_PARAMETER;
    }

    if (ppVirtAddr)
        *ppVirtAddr = (void*)(((UINT08*)m_BaseVirt) + (Index * m_EntrySize));

    return OK;
}

RC GDTable::GetIndex(void * pVirtAddr,UINT32 * Index)
{
    LOG_ENT();

    PHYSADDR physAddr = Platform::VirtualToPhysical(pVirtAddr);
    PHYSADDR physBase = Platform::VirtualToPhysical(m_BaseVirt);

    if ((physAddr < physBase) ||
       ((physAddr - physBase) % m_EntrySize))
    {
        Printf(Tee::PriError, "Invalid Virtual Address for DTable Entry\n");
        return RC::BAD_PARAMETER;
    }
    if (!Index)
    {
        Printf(Tee::PriError, "Null Pointer Passed\n");
        return RC::SOFTWARE_ERROR;
    }

    *Index = (physAddr - physBase) / m_EntrySize;

    LOG_EXT();
    return OK;
}

void GDTable::SetBufferMgr(BufferMgr* pBuffMgr)
{
    if (m_pBuffMgr)
    {
        Printf(Tee::PriDebug, "Deallocating old buffer manager\n");
        delete m_pBuffMgr;
    }

    m_pBuffMgr = pBuffMgr;
}

RC GDTable::WriteToBuffer(vector<UINT08>* pPattern)
{
    RC rc = OK;

    if (!m_pBuffMgr)
        Printf(Tee::PriLow, "Buffer Manager not set. Cannot Write to Buffer. "
                            "This is OK when calling the test class functions\n");
    else
        CHECK_RC(m_pBuffMgr->FillPattern(pPattern));

    return rc;
}

RC GDTable::WriteToBuffer(vector<UINT16>* pPattern)
{
    RC rc = OK;

    if (!m_pBuffMgr)
        Printf(Tee::PriLow, "Buffer Manager not set. Cannot Write to Buffer. "
                            "This is OK when calling the test class functions\n");
    else
        CHECK_RC(m_pBuffMgr->FillPattern(pPattern));

    return rc;
}

RC GDTable::WriteToBuffer(vector<UINT32>* pPattern)
{
    RC rc = OK;

    if (!m_pBuffMgr)
        Printf(Tee::PriLow, "Buffer Manager not set. Cannot Write to Buffer. "
                            "This is OK when calling the test class functions\n");
    else
        CHECK_RC(m_pBuffMgr->FillPattern(pPattern));

    return rc;
}
