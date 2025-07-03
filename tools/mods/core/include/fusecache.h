/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file FuseCache is a cache of information parsed from the FuseParser.
//!       Its sole purpose is to prevent parsing fuse files more than once
//!       during a MODS run. Only FuseParsers should interact with this class.
//!       A single cache is needed because ParseFile() can be called multiple
//!       times for a single chip (e.g. SLI). Therefore, it should not
//!       be tied to any one GPU or CheetAh device. Unlike a typical cache which
//!       fetches data on a miss, FuseCache expects FuseParser to peek at the
//!       cache first and add an entry if there is a cache miss. This design
//!       arises because we want Fuse, GpuFuse, etc to call FuseParser, not
//!       FuseCache. Otherwise, end users would be calling FuseCache.GetEntry()
//!       when they want to parse the file, which is obtuse. Or ParseFile()
//!       could call into the cache, and on a cache miss the FuseCache could
//!       call back into FuseParser.ParseFile(). The logic for that would be
//!       very messy and confusing.

#pragma once
#ifndef INCLUDED_FUSECACHE_H
#define INCLUDED_FUSECACHE_H

#include "types.h"
#include <map>
#include <string>

class RC;
class FuseDataSet;

namespace FuseUtil
{
    struct MiscInfo;
    struct SkuConfig;
    struct FuseDef;
    typedef map<string, FuseDef> FuseDefMap;
    typedef map<string, SkuConfig> SkuList;
}

// FuseCacheData holds dynamically allocated information
// obtained from parsing the fuse file. Its contents are read-only
// because we don't want anyone modifying the values read from the
// fuse file.
struct FuseCacheData
{
    const FuseUtil::FuseDefMap* pFuseDefMap;
    const FuseUtil::SkuList*    pSkuList;
    const FuseUtil::MiscInfo*   pMiscInfo;
    const FuseDataSet*          pFuseDataSet;

    FuseCacheData(const FuseUtil::FuseDefMap* pFuseDefMap,
                  const FuseUtil::SkuList*    pSkuList,
                  const FuseUtil::MiscInfo*   pMiscInfo,
                  const FuseDataSet*          pFuseDataSet) :
        pFuseDefMap(pFuseDefMap),
        pSkuList(pSkuList),
        pMiscInfo(pMiscInfo),
        pFuseDataSet(pFuseDataSet)
    {}

    FuseCacheData() :
        pFuseDefMap(nullptr),
        pSkuList(nullptr),
        pMiscInfo(nullptr),
        pFuseDataSet(nullptr)
    {}
};

class FuseCache
{
public:
    static FuseCache& GetInstance()
    {
        static FuseCache instance;
        return instance;
    }

    ~FuseCache();

    // Transfers data ownership to the cache
    RC AddEntry(const string&         filename,
                FuseUtil::FuseDefMap *pFuseDefMap,
                FuseUtil::SkuList    *pSkuList,
                FuseUtil::MiscInfo   *pMiscInfo,
                FuseDataSet          *pFuseDataSet);

    RC GetEntry(const string&                filename,
                const FuseUtil::FuseDefMap **ppFuseDefMap,
                const FuseUtil::SkuList    **ppSkuList,
                const FuseUtil::MiscInfo   **ppMiscInfo,
                const FuseDataSet          **ppFuseDataSet) const;

    bool HasEntry(const string& filename) const;

private:
    FuseCache() {};
    FuseCache(FuseCache const&);      // Don't make a copy constructor
    void operator=(FuseCache const&); // Don't make an assignment operator

    // Map of fuse filenames to their cached data
    map<string, FuseCacheData> m_CacheMap;
};

#endif
