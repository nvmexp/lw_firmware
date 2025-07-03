/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file FuseJsonParser parses JSON fuse files

#pragma once
#ifndef INCLUDE_FUSEJSONPARSER_H
#define INCLUDE_FUSEJSONPARSER_H

#include "fuseparser.h"
#include "jsonparser.h"

#include "document.h"

using JsonTree = rapidjson::GenericValue<rapidjson::UTF8<char>,
                                         rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator> >;
namespace FuseUtil
{
    struct FuseLoc;
}

class FuseJsonParser : public FuseParser
{
public:
    FuseJsonParser();
    RC GetPorFileId(const string& fusefileName, UINT32 *pId);

protected:
    virtual RC ParseFileImpl(const string         &filename,
                             FuseUtil::FuseDefMap *pFuseDefMap,
                             FuseUtil::SkuList    *pSkuList,
                             FuseUtil::MiscInfo   *pMiscInfo,
                             FuseDataSet          *pFuseDataSet = nullptr);

private:
    string m_FileName;
    FuseUtil::FuseDefMap *m_pFuseDefMap;
    FuseUtil::SkuList *m_pSkuList;
    FuseUtil::MiscInfo *m_pMiscInfo;

    static RC ParseFuseJson(const string& filename, rapidjson::Document* pDoc);

    RC InterpretFuseJson(const rapidjson::Document& doc, bool isFpf);

    RC ParseHeader(const JsonTree& header);
    RC ParseChipOptions(const JsonTree& chipOptions, bool isFpf);
    RC ParseMapOptions(const JsonTree& mapOptions);
    RC ParseMktOptions(const JsonTree& mktOptions, bool isFpf);
    RC ParseFuseEncodingOptions(const JsonTree& fuseOptions);
    RC ParseFsTagMap(const JsonTree& tagsSection);
    RC ParseFloorsweepingOptions(const JsonTree& fsOptions);
    RC ParseConfigFields(const JsonTree& configFields,
                         map<string, FuseUtil::FuseLoc>& subFuses);
    RC ParseFuselessFuse(const JsonTree& fuselessFuse,
                         map<string, FuseUtil::FuseLoc>& subFuses);
    RC ParsePseudoFuses(const JsonTree& sltData);
    RC ParseBootromPatches(const JsonTree& bootromPatches);
    RC ParseIFFPatches(const JsonTree& iffPatches);
    RC ParseFuseMacro(const JsonTree& fuseMacro, bool isFpf);
    RC ParsePorData(const JsonTree& porData);
    RC ParseSltData(const JsonTree& sltData);
    RC ParseSkuIds(const JsonTree& skuIds);

    RC InterpretPorJson(const rapidjson::Document& doc);

    static FuseUtil::FuseLoc ParseFuseLocString(const rapidjson::Value::ConstMemberIterator& fuseLocItr);
    static RC FindUnredactedFilename(const string& name, string *pFilename);
};

#endif
