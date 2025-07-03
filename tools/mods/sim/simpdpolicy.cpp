/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2009,2019-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include <map>
#include <vector>
#include "core/include/massert.h"
#include "simpage.h"
#include "simpdpolicy.h"

static
#ifdef LW_64_BITS
// map + 42bit pd(42:10:12)
PageDirectory1LevelMap
#else
// array + 10bit pd(10:10:12)
PageDirectory1LevelArray
//PageDirectory1LevelMap
#endif
s_PageDirPolicy;

PageDirectoryPolicy* PageDirPolicy::s_pPageDirPolicy = &s_PageDirPolicy;

// class PageDirectory1LevelArray
///////////////////////////////////////////////////////////////////////////////
void PageDirectory1LevelArray::Initialize
(
    UINT32 vaBitsNum,
    UINT32 pageDirBitsNum
)
{
    m_VABitsNum = vaBitsNum;
    m_PageDirBitsNum = pageDirBitsNum;
    m_PageDirectory = 0;
    m_PageDirectorySize = 0;
}

/*virtual*/ PageDirectory1LevelArray::~PageDirectory1LevelArray()
{
    if (m_PageDirectory != 0)
    {
        for (size_t i = 0; i < m_PageDirectorySize; i++)
        {
            if (m_PageDirectory[i])
            {
                delete m_PageDirectory[i];
            }
        }

        delete[] m_PageDirectory;
        m_PageDirectorySize = 0;
    }
}

/*virtual*/ PageTable* PageDirectory1LevelArray::AllocNewPageTable
(
    size_t pdeIdx
)
{
    if (m_PageDirectory == 0)
    {
        MASSERT(m_PageDirectorySize == 0);
        m_PageDirectorySize = static_cast<size_t>(1) << m_PageDirBitsNum;
        m_PageDirectory = new PPageTable[m_PageDirectorySize];
        MASSERT(m_PageDirectory != 0);

        memset(m_PageDirectory, 0, sizeof(PPageTable)*m_PageDirectorySize);
    }

    if (pdeIdx >= m_PageDirectorySize)
    {
        return 0;
    }

    m_PageDirectory[pdeIdx] = new PageTable;
    return m_PageDirectory[pdeIdx];
}

/*virtual*/ PageTable* PageDirectory1LevelArray::GetPageTablebyIdx
(
    size_t pdeIdx
)
{
    if ((m_PageDirectory == 0) || (pdeIdx >= m_PageDirectorySize))
    {
        return 0;
    }

    return m_PageDirectory[pdeIdx];
}

/*virtual*/ void PageDirectory1LevelArray::FreePageTable(size_t pdeIdx)
{
    if ((m_PageDirectory == 0) || (pdeIdx >= m_PageDirectorySize))
    {
       return;
    }

    if (m_PageDirectory[pdeIdx])
    {
        delete m_PageDirectory[pdeIdx];
        m_PageDirectory[pdeIdx] = 0;
    }
}

/*virtual*/ size_t PageDirectory1LevelArray::GetPdeIndex
(
    uintptr_t virtualAddress
) const
{
    return virtualAddress >> (m_VABitsNum - m_PageDirBitsNum);
}

/*virtual*/ const PageTableList* PageDirectory1LevelArray::EnumerateAllPageTables()
{
    m_PageTableList.clear();

    for(size_t i = 0; i < m_PageDirectorySize; i++)
    {
        if (m_PageDirectory[i])
        {
            m_PageTableList.push_back(make_pair(i, m_PageDirectory[i]));
        }
    }

    return &m_PageTableList;
}

// Class PageDirectory1LevelMap
///////////////////////////////////////////////////////////////////////////////
void PageDirectory1LevelMap::Initialize
(
    UINT32 vaBitsNum,
    UINT32 pageDirBitsNum
)
{
    m_VABitsNum = vaBitsNum;
    m_PageDirBitsNum = pageDirBitsNum;
}

/*virtual*/ PageDirectory1LevelMap::~PageDirectory1LevelMap()
{
    m_PageDirectory.clear();
}

/*virtual*/ PageTable* PageDirectory1LevelMap::AllocNewPageTable
(
    size_t pdeIdx
)
{
    MASSERT(m_PageDirectory.find(pdeIdx) == m_PageDirectory.end());
    m_PageTableUpdated = true;
    return &m_PageDirectory[pdeIdx];
}

/*virtual*/ PageTable* PageDirectory1LevelMap::GetPageTablebyIdx
(
    size_t pdeIdx
)
{
    if (m_PageDirectory.empty())
    {
        return 0;
    }

    PdIter it = m_PageDirectory.find(pdeIdx);

    if (it != m_PageDirectory.end())
    {
        return &it->second;
    }
    else
    {
        return 0;
    }
}

/*virtual*/ void PageDirectory1LevelMap::FreePageTable(size_t pdeIdx)
{
    if (m_PageDirectory.empty())
    {
       return;
    }

    PdIter it = m_PageDirectory.find(pdeIdx);
    if (it != m_PageDirectory.end())
    {
        m_PageDirectory.erase(it);
        m_PageTableUpdated = true;
    }
}

/*virtual*/ size_t PageDirectory1LevelMap::GetPdeIndex
(
    uintptr_t virtualAddress
) const
{
    return virtualAddress >> (m_VABitsNum - m_PageDirBitsNum);
}


/*virutal*/ const PageTableList* PageDirectory1LevelMap::EnumerateAllPageTables()
{
    // it could be time consuming to enumerate every time, so save a copy and 
    // only setup it when the page table is updated.
    if (m_PageTableUpdated)
    {
        m_PageTableList.clear();

        PdIter it = m_PageDirectory.begin();
        for(; it != m_PageDirectory.end(); it++)
        {
            m_PageTableList.push_back(make_pair(it->first, &it->second));
        }
        m_PageTableUpdated = false;
    }
    return &m_PageTableList;
}
