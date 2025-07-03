/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file FuseParser defines an interface for parsing fuse files (XML or JSON).
//!       It uses a FuseCache to save the information it reads from the fuse
//!       file. This way each fuse file only needs to be parsed once, and all
//!       subsequent reads can be made from the cache.

// What is fuseless fuse?
// The name 'fuseless' fuse is confusing itself. HW architects came up
// with this concept because there had been too many fuse blowing
// mistakes in operations. Even with 'undo' fuses, these mistakes
// are creating lots of waste.
// Fuseless fuse uses a chunck of 'blowable' fuse data to store fuse
// 'instructions'. The instructions don't need to be in any specific
// location; and most importantly, the instructions can be ilwalidated.
// That means we can keep undo'ing until we run out of 'fuse space'

// What is a Subfuse?
// A fuseless fuse is made up of one or more 'subfuses'. In fuseless
// type, the actual fuse values live in a jtag chain structure for
// floorsweeping and ramrepair. One fuse can have its value span
// multiple locations. We call each small chunk of a fuse a 'subfuse'

#pragma once
#ifndef INCLUDED_FUSEPARSER_H
#define INCLUDED_FUSEPARSER_H

#include "tee.h"
#include "types.h"
#include <map>
#include <string>
#include <vector>

struct JSObject;
class RC;
class FuseCache;

namespace FuseUtil
{
    struct MiscInfo;
    struct SkuConfig;
    struct FuseDef;
    struct FuseInfo;
    struct OffsetInfo;
    typedef map<string, FuseDef> FuseDefMap;
    typedef map<string, SkuConfig> SkuList;
}

// Helper base class which stores chip specific data from the parser.
// Only used by the XML parser through GM10x. GM20x and up does not
// use this.
class FuseDataSet
{
public:
    FuseDataSet(){};
    ~FuseDataSet(){};

    void ResetFbConfig();
    void AddFbConfig(bool Valid,
                     UINT32 FbpCount,
                     UINT32 FbpDisable,
                     UINT32 FbioDisable,
                     UINT32 FbioShift);

    RC ExportFbConfigToJs(JSObject **ppObject) const;

private:
    struct FbConfigInfo
    {
        bool   Valid;
        UINT32 FbpCount;
        UINT32 FbpDisable;
        UINT32 FbioDisable;
        UINT32 FbioShift;
    };
    vector<FbConfigInfo> m_FbConfigList;
};

class FuseParser
{
public:
    FuseParser();
    virtual ~FuseParser() {}

    // Factory method to create a FuseParser based on the filename
    static FuseParser* CreateFuseParser(const string& filename);

    // Wraps ParseFileImpl() with the FuseCache logic
    RC ParseFile(const string&                filename,
                 const FuseUtil::FuseDefMap **ppFuseDefMap,
                 const FuseUtil::SkuList    **ppSkuList,
                 const FuseUtil::MiscInfo   **ppMiscInfo,
                 const FuseDataSet          **ppFuseDataSet = nullptr);

    static RC SetVerbose(bool Enable);
    static bool GetVerbose() { return m_Verbose; }

protected:

    // Actual parsing happens here
    virtual RC ParseFileImpl(const string         &filename,
                             FuseUtil::FuseDefMap *pFuseDefMap,
                             FuseUtil::SkuList    *pSkuList,
                             FuseUtil::MiscInfo   *pMiscInfo,
                             FuseDataSet          *pFuseDataSet) = 0;

    static void ParseOffset(const string& offsetVal, FuseUtil::OffsetInfo& offsetInfo);
    static RC InterpretSkuFuses(FuseUtil::SkuList *pSkuList);
    static void RemoveTraditionalIff(FuseUtil::SkuList *pSkuList);
    static RC CheckXmlSanity(const FuseUtil::FuseInfo &FuseInfo);

    FuseCache &m_FuseCache;

private:
    static bool m_Verbose;
};

#endif
