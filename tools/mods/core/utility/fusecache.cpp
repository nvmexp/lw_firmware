/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/fusecache.h"
#include "core/include/fuseparser.h"
#include "core/include/fuseutil.h"
#include "core/include/massert.h"
#include "core/include/rc.h"

// Free what was allocated by FuseParser.ParseFile()
FuseCache::~FuseCache()
{
    map<string, FuseCacheData>::iterator itr = m_CacheMap.begin();
    for (; itr != m_CacheMap.end(); ++itr)
    {
        delete itr->second.pFuseDefMap;
        delete itr->second.pSkuList;
        delete itr->second.pMiscInfo;
        delete itr->second.pFuseDataSet;
    }
}

// The user of this class is expected to call this, otherwise they wouldn't
// know whether to fetch an entry from the cache or make a new one. The
// cache doesn't call ParseFile() again on a miss!
bool FuseCache::HasEntry(const string& filename) const
{
    map<string, FuseCacheData>::const_iterator itr = m_CacheMap.find(filename);
    return itr != m_CacheMap.end();
}

RC FuseCache::AddEntry(const string& filename,
                       FuseUtil::FuseDefMap *pFuseDefMap,
                       FuseUtil::SkuList *pSkuList,
                       FuseUtil::MiscInfo *pMiscInfo,
                       FuseDataSet *pFuseDataSet)
{
    MASSERT(pFuseDefMap && pSkuList && pMiscInfo && pFuseDataSet);

    if (HasEntry(filename))
    {
        Printf(Tee::PriError,
               "Attempted to add entry to FuseCache which already exists\n");
        return RC::SOFTWARE_ERROR;
    }

    m_CacheMap[filename] = FuseCacheData(pFuseDefMap, pSkuList, pMiscInfo, pFuseDataSet);
    Printf(Tee::PriDebug, "Cached %s fuse data\n", filename.c_str());

    return OK;
}

// Returns a read-only entry in the cache
RC FuseCache::GetEntry(const string &filename,
                       const FuseUtil::FuseDefMap **ppFuseDefMap,
                       const FuseUtil::SkuList **ppSkuList,
                       const FuseUtil::MiscInfo **ppMiscInfo,
                       const FuseDataSet **ppFuseDataSet) const
{
    if (!HasEntry(filename))
    {
        Printf(Tee::PriError,
               "Attempted to get entry from FuseCache that does not exist"
               " - call HasEntry() instead\n");
        return RC::SOFTWARE_ERROR;
    }

    if (ppFuseDefMap != nullptr)
    {
        *ppFuseDefMap = m_CacheMap.find(filename)->second.pFuseDefMap;
    }
    if (ppSkuList != nullptr)
    {
        *ppSkuList = m_CacheMap.find(filename)->second.pSkuList;
    }
    if (ppMiscInfo != nullptr)
    {
        *ppMiscInfo = m_CacheMap.find(filename)->second.pMiscInfo;
    }
    if (ppFuseDataSet != nullptr)
    {
        *ppFuseDataSet = m_CacheMap.find(filename)->second.pFuseDataSet;
    }

    Printf(Tee::PriDebug, "Retrieved fuse data for %s from cache\n",
           filename.c_str());

    return OK;
}
