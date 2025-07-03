/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_FUSEXML_PARSER_H
#define INCLUDED_FUSEXML_PARSER_H

#include "fuseutil.h"
#include "fuseparser.h"
#include <list>
#include <string>
#include <vector>
#include <map>

class RC;

class FuseXmlParser : public FuseParser
{
public:

    // Force users to use the CreateFuseParser() factory
    friend FuseParser* FuseParser::CreateFuseParser(const string& filename);
    virtual ~FuseXmlParser() {};

    enum ParserError
    {
        PARSE_OK,
        PARSE_UNEXPECTED_ELEMENT,
        PARSE_UNEXPECTED_DEPTH,
        PARSE_UNEXPECTED_STRING,
        PARSE_MISSING_FUSEDEF,
        PARSE_DUPLICATED_DEFINITION,
        PARSE_ERROR,
    };

    // ParserArgs needs to be public for the expat parser handlers
    struct ParserArgs
    {
        UINT32                            LwrLine;
        UINT32                            Depth;
        ParserError                       ErrorCode;
        list<string>                      TextList;
        UINT32                            Flags;
        bool                              PushNextString;
        void                              (*NextStart)(ParserArgs*);
        void                              (*NextEnd)(ParserArgs*);

        map<string, FuseUtil::FuseLoc>    SubFuses;
        map<string, FuseUtil::FuseDef>   *pFuseDefList; // list of fuses & their locations
        FuseDataSet                      *pDataSet;     // chip specific info

        // list of each marketing SKU and each sku's fuse values
        FuseUtil::SkuList                *pSkuList;
        FuseUtil::MiscInfo               *pMiscInfo;
        FuseXmlParser                    *pParser;
    };

    static string PopString(ParserArgs* pParam);

protected:
    virtual RC ParseFileImpl(const string&         filename,
                             FuseUtil::FuseDefMap *pFuseDefMap,
                             FuseUtil::SkuList    *pSkuList,
                             FuseUtil::MiscInfo   *pMiscInfo,
                             FuseDataSet          *pFuseDataSet = nullptr);

private:

    FuseXmlParser();

    ParserArgs m_ParserArgs;

    static void Start(void *data, const char *el, const char **attr);
    static void End(void *data, const char *el);

    // Expat parser handlers
    static void OptionStartHandler(ParserArgs* pParam);
    static void OptionEndHandler(ParserArgs* pParam);
    static void CharacterHandler(void *data, const char *txt, int txtlen);

    // this is called from Start handler
    static void XmlStartHandler(ParserArgs* pParam);
    static void XmlEndHandler(ParserArgs* pParam);

    // Get the Perforce tag
    static void HeaderHandler(ParserArgs* pParam);

    // Brief: For each of Chip_Option, Map_Option and Mkt_Option, the current
    // implementation will attempt to begin to queue up a list of text for
    // ach fuse or SKU definition during the start handler. The End handler
    // will try to interpret the list of text and store them into the data
    // structures

    static void ChipsOptStartHandler(ParserArgs* pParam);
    static void ChipsOptEndHandler(ParserArgs* pParam);
    static void FuseOptStartHandler(ParserArgs* pParam);
    static void FuseOptEndHandler(ParserArgs* pParam);
    static void MapOptStartHandler(ParserArgs* pParam);
    static void MapOptEndHandler(ParserArgs* pParam);
    static void MktOptStartHandler(ParserArgs* pParam);
    static void MktOptEndHandler(ParserArgs* pParam);
    static void BRomPatchOptStartHandler(ParserArgs* pParam);
    static void BRomPatchOptEndHandler(ParserArgs*  pParam);
    static void BRRevStartHandler(ParserArgs* pParam);
    static void BRRevEndHandler(ParserArgs* pParam);
    static void PscRomPatchOptStartHandler(ParserArgs* pParam);
    static void PscRomPatchOptEndHandler(ParserArgs*  pParam);
    static void PscRomRevStartHandler(ParserArgs* pParam);
    static void PscRomRevEndHandler(ParserArgs* pParam);
    static void IffPatchOptStartHandler(ParserArgs* pParam);
    static void IffPatchOptEndHandler(ParserArgs*  pParam);
    static void IffPatchStartHandler(ParserArgs* pParam);
    static void IffPatchEndHandler(ParserArgs*  pParam);
    static void FuseEncodingStartHandler(ParserArgs* pParam);
    static void FuseEncodingEndHandler(ParserArgs* pParam);
    static void ConfigFieldsStartHandler(ParserArgs* pParam);
    static void ConfigFieldsEndHandler(ParserArgs* pParam);
    static void JtagChainMapStartHandler(ParserArgs* pParam);
    static void JtagChainMapEndHandler(ParserArgs* pParam);
    static void FuselessStartHandler(ParserArgs *pParam);
    static void FuselessEndHandler(ParserArgs *pParam);
    static void FreetextStartHandler(ParserArgs* pParam);
    static void FreetextEndHandler(ParserArgs* pParam);
    static void UnknownOptionStartHandler(ParserArgs *pParam);
    static void UnknownOptionEndHandler(ParserArgs *pParam);
    static void FbConfigStartHandler(FuseXmlParser::ParserArgs* pParam);
    static void FbConfigEndHandler(FuseXmlParser::ParserArgs* pParam);
    static void FuseMacroStartHandler(FuseXmlParser::ParserArgs* pParam);
    static void FuseMacroEndHandler(FuseXmlParser::ParserArgs* pParam);
    static void PseudoFuseStartHandler(FuseXmlParser::ParserArgs* pParam);
    static void PseudoFuseEndHandler(FuseXmlParser::ParserArgs* pParam);

    RC GetParserStatus(ParserError ErrorCode);
    UINT32 GetNumFuseType(const string SkuName,
                          INT32 FuseAttrib,
                          FuseUtil::SkuList *pSkuList);
};

#endif
