/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2009,2015,2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// We can have different policies to simulate page directory.
// 1. Page directory storage:
//    a. Use array
//    b. Use map
//    c. Use hash_map
// 2. Page directory bits:
//    a. 1 level: 10bit for va32
//                42bit for va64
//    b. 2 level: 5:5 for va32
//                20:22 for va64
//
// Use the class to hide the implement details from simpg32.cpp
//
class PageDirectoryPolicy
{
public:
    virtual void Initialize(UINT32 vaBitsNum, UINT32 pageDirBitsNum) = 0;
    virtual PageTable* AllocNewPageTable(size_t pdeIdx) = 0;
    virtual PageTable* GetPageTablebyIdx(size_t pdeIdx) = 0;
    virtual void FreePageTable(size_t pdeIdx) = 0;
    virtual size_t GetPdeIndex(uintptr_t virtualAddress) const = 0;
    virtual const PageTableList* EnumerateAllPageTables() = 0;
};

//
// Use an array to look up page table
//
class PageDirectory1LevelArray: public PageDirectoryPolicy
{
public:
    virtual void Initialize(UINT32 vaBitsNum, UINT32 pageDirBitsNum);
    virtual ~PageDirectory1LevelArray();

    virtual PageTable* AllocNewPageTable(size_t pdeIdx);
    virtual PageTable* GetPageTablebyIdx(size_t pdeIdx);
    virtual void FreePageTable(size_t pdeIdx);
    virtual size_t GetPdeIndex(uintptr_t virtualAddress) const;
    virtual const PageTableList* EnumerateAllPageTables();

private:
    UINT32 m_VABitsNum = 0;
    UINT32 m_PageDirBitsNum = 0;
    PPageTable* m_PageDirectory = nullptr;
    size_t m_PageDirectorySize = 0;
    PageTableList m_PageTableList;
};

//
// Use one level map to look up page table list
//
class PageDirectory1LevelMap: public PageDirectoryPolicy
{
public:
    virtual void Initialize(UINT32 vaBitsNum, UINT32 pageDirBitsNum);
    virtual ~PageDirectory1LevelMap();

    virtual PageTable* AllocNewPageTable(size_t pdeIdx);
    virtual PageTable* GetPageTablebyIdx(size_t pdeIdx);
    virtual void FreePageTable(size_t pdeIdx);
    virtual size_t GetPdeIndex(uintptr_t virtualAddress) const;
    virtual const PageTableList* EnumerateAllPageTables();

private:
    typedef map<size_t, PageTable>::iterator PdIter;
    map<size_t, PageTable> m_PageDirectory;
    UINT32 m_VABitsNum = 0;
    UINT32 m_PageDirBitsNum = 0;
    PageTableList m_PageTableList;
    bool m_PageTableUpdated = false;
};

//
// singleton class
// the policy is global
//
class PageDirPolicy
{
public:
    explicit PageDirPolicy() {}

    static PageDirectoryPolicy* s_pPageDirPolicy;

    PageDirectoryPolicy* Instance()   const { return s_pPageDirPolicy; }
    PageDirectoryPolicy* operator->() const { return s_pPageDirPolicy; }
    PageDirectoryPolicy & operator*() const { return *s_pPageDirPolicy; }
};
