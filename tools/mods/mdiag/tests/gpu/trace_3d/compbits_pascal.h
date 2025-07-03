/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _COMPBITS_PASCAL_H_
#define _COMPBITS_PASCAL_H_

// Dependency eliminated:
// This header file shall be included by only 1 file: compbits_pascal.cpp

#include "compbits.h"
#include "../amaplib/include/amap_v1.h" // struct CompTagInfoV1

namespace CompBits_Pascal
{

// CBC tests for Pascal & later.

// Calling AMAP lib swizzleCompbitsTRC().
class SurfCopyTest : public CompBitsTest
{
public:
    SurfCopyTest(TraceSubChannel* pSubChan)
        : CompBitsTest(pSubChan)
    {
    }

private:
    virtual ~SurfCopyTest() {};

    virtual RC SetupTest();
    virtual RC DoPostRender();
    virtual void CleanupTest();
    virtual const char* TestName() const { return "SurfaceCopyTest"; }

private:
    void ClearBuffers();
    RC Run(const vector<UINT08>& oldDecompData, bool bReverse);

    // Do something like Eviction but may not be exactly because we
    // don't touch real cacheline.
    RC CompTagSnapshot();
    RC PerformSwizzleCompBits(CompTagInfoV1 *pComptagInfo,
        size_t *pBackingStoreChunkOff, size_t *pBackingStoreChunkSize);
    RC CompTagPromotion();

    vector<TileInfoV1> m_TileInfos;
    vector<UINT08> m_Chunk;
    // Binary bits representing effective 128B chunks of overfetching buffer.
    vector<UINT08> m_CacheLineFetchBits;
    vector<UINT08> m_CompCacheLineTemp;
    CompBits::SurfCopier m_SurfCopier;
};

} // namespace

#endif

