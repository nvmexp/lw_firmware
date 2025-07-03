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

#include "core/include/utility.h"
#include "core/include/massert.h"
#include "expat.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/fusexml_parser.h"
#include "core/include/fusecache.h"
#include "core/include/version.h"
#include "core/include/xp.h"
#include "fuse.h"
#include "core/include/fileholder.h"
#include <string.h>
#include <ctype.h>
#include <memory>

using namespace FuseUtil;

JS_CLASS(FuseXml);

static SObject FuseXml_Object
(
    "FuseXml",
    FuseXmlClass,
    0,
    0,
    "Fuse Xml class"
);

// this is like fgets except the source buffer is char* instead of FILE*
// Maybe there already is a function for this
static UINT32 BufferGets(char* DstBuff,
                         UINT32 MaxSize,
                         const vector<char> &SrcBuff,
                         UINT32 *pPos)
{
    MASSERT(DstBuff);
    MASSERT(pPos);

    UINT32 Idx = 0;
    while ((Idx + *pPos) < SrcBuff.size() &&
           (SrcBuff[Idx + *pPos] != '\n') &&
           (Idx < MaxSize - 1))
    {
        DstBuff[Idx] = SrcBuff[Idx + *pPos];
        Idx++;
    }
    // increment Position pointer + '\n'
    *pPos += Idx + 1;
    // put in the Null character
    DstBuff[Idx] = '\0';
    // return Num bytes copied
    return Idx;
}

FuseXmlParser::FuseXmlParser(){}

//-----------------------------------------------------------------------------
//                          ----- Parser Handlers-------

/*
This part handles:
Getting 'the fuse name', and select the EndHandler

            <key>BYPASS_FUSES</key>
             <ref>                 |called here: depth = 5|
              <hash>
*/
void FuseXmlParser::ChipsOptStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 5)
    {
        Printf(Tee::PriNormal, "bad depth in ChipsOptEndHandler @line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }

    map<string, FuseDef>::iterator pFuseDef;
    string Buffer = pParam->TextList.front();
    pFuseDef      = pParam->pFuseDefList->find(Utility::ToUpperCase(Buffer));
    if (pFuseDef != pParam->pFuseDefList->end())
    {
        Printf(Tee::PriNormal, "Definition already found. XML error @line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_ELEMENT;
        return;
    }
    pParam->NextEnd = &FuseXmlParser::ChipsOptEndHandler;
}

void FuseXmlParser::FuseOptStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    pParam->NextEnd = &FuseXmlParser::FuseOptEndHandler;
}

/*
This part handles:
Getting 'the fuse option'

        <key>All_Fuse_options</key>
          <ref>
           <hash>
            <key>base_address</key>
             <str>4096</str>
*/

void FuseXmlParser::FuseOptEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);

    if (pParam->Depth == 5 || pParam->Depth == 4)
    {
        MASSERT(!pParam->TextList.empty());
        MASSERT(pParam->pMiscInfo);

        while (pParam->TextList.size())
        {
            string mapkey = Utility::ToUpperCase(PopString(pParam));
            if (mapkey == "RAM_REPAIR_CHAIN_SIZE")
            {
                (*pParam->pMiscInfo).McpInfo[mapkey+"_1"] = PopString(pParam);
                mapkey += "_2";
            }
            (*pParam->pMiscInfo).McpInfo[mapkey] = PopString(pParam);
        }

        if (pParam->Depth == 4)
        {
            pParam->NextStart = &FuseXmlParser::OptionStartHandler;
            pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
        }
        else
        {
            pParam->NextStart = &FuseXmlParser::FuseOptStartHandler;
            pParam->NextEnd = &FuseXmlParser::FuseOptEndHandler;
        }
        return;
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in FuseOptEndHandler @line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
}

void FuseXmlParser::FreetextStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);

    if (pParam->Depth == 5)
    {
        pParam->pMiscInfo->McpInfo["SkuName"] = PopString(pParam);
    }

    pParam->NextEnd = &FuseXmlParser::FreetextEndHandler;
}

void FuseXmlParser::FreetextEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);

    if (pParam->Depth == 6)
    {
         MASSERT(!pParam->TextList.empty());
         MASSERT(pParam->pMiscInfo);

         string sku = pParam->pMiscInfo->McpInfo["SkuName"] ;
         string section = PopString(pParam);

         while (!pParam->TextList.empty())
         {
             string key = PopString(pParam);
             if (!pParam->TextList.empty())
             {
                 pParam->pMiscInfo->McpInfo[sku+'_'+key] = PopString(pParam);
             }
             else
             {
                 pParam->pMiscInfo->McpInfo[sku+'_'+key] = "NA";
             }
         }

         pParam->NextStart = &FuseXmlParser::FreetextStartHandler;
         pParam->NextEnd = &FuseXmlParser::FreetextEndHandler;
    }
    else if(pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
    }
    else if(pParam->Depth == 5)
    {
        pParam->NextStart = &FuseXmlParser::FreetextStartHandler;
        pParam->NextEnd = &FuseXmlParser::FreetextEndHandler;
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in FuseOptEndHandler @line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
    }
    return;
}

/*
This part handles:
Given the fuse name obtained in ChipsOptStartHandler, this function saves the
bit, offset and type into the fuse def structure

            <key>BYPASS_FUSES</key>
             <ref>
              <hash>
               <key>bits</key>
                <str>1</str>
               <key>lwstomer_visible</key>
                <str>0</str>
               <key>description</key>
                <undef/>
               <key>offset</key>
                <str>NA</str>
               <key>type</key>
                <str>en</str>
               <key>ecc</key>
                <str>H0</str>
               <key>reload</key>
                <str>1</str>
              </hash>
             </ref>          |Called here - depth = 5|
*/
void FuseXmlParser::ChipsOptEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    bool IsDescriptionSku = false;
    // handle the end of a Fuse definition
    if (pParam->Depth == 5)
    {
        MASSERT(!pParam->TextList.empty());
        string Buffer = Utility::ToUpperCase(PopString(pParam));

        FuseDef NewFuse;
        NewFuse.Name = Buffer;

        Buffer = PopString(pParam);
        if (Buffer != "bits")
        {
            Printf(Tee::PriNormal, "no bits specified\n");
            pParam->ErrorCode = PARSE_UNEXPECTED_STRING;
            return;
        }

        Buffer = PopString(pParam);
        NewFuse.NumBits = strtol(Buffer.c_str(), NULL, 10);
        NewFuse.Visibility = 0;

        if (pParam->TextList.front() == "lwstomer_visible")
        {
            PopString(pParam);  // pops 'lwstomer_visible'
            Buffer = PopString(pParam);
            NewFuse.Visibility = strtol(Buffer.c_str(), NULL, 10);
        }

        Buffer = PopString(pParam);
        if (Buffer != "description")
        {
            Printf(Tee::PriNormal, "missing 'description'\n");
            pParam->ErrorCode = PARSE_UNEXPECTED_STRING;
            return;
        }

        //until we find 'offset'
        while (pParam->TextList.front() != "offset" || pParam->TextList.size() == 0)
        {
            IsDescriptionSku = (PopString(pParam) == "sku") ? true : false ;
        }

        Buffer = PopString(pParam);
        bool parsedPrimaryOffset = false;
        while (Buffer == "offset")
        {
            if(Buffer == "offset")
            {
                ParseOffset(PopString(pParam),
                            NewFuse.PrimaryOffset);
                parsedPrimaryOffset = true;
            }
            else
            {
                Printf(Tee::PriNormal, "unkonwn type of offset\n");
                pParam->ErrorCode = PARSE_UNEXPECTED_STRING;
                return;
            }
            Buffer = PopString(pParam);
        }

        if (!parsedPrimaryOffset)
        {
            Printf(Tee::PriNormal, "no primary offset\n");
            pParam->ErrorCode = PARSE_UNEXPECTED_STRING;
            return;
        }

        if (Buffer != "type")
        {
            Printf(Tee::PriNormal, "not type specified\n");
            pParam->ErrorCode = PARSE_UNEXPECTED_STRING;
            return;
        }
        NewFuse.Type = PopString(pParam);

        if ((pParam->TextList.size() != 0) &&
            (pParam->TextList.front() == "ecc"))
        {
            PopString(pParam);  // pops 'ecc'
            Buffer = PopString(pParam);
            size_t Pos = Buffer.find("H");
            if (Pos == string::npos)
            {
                Printf(Tee::PriNormal, "no ecc type specified\n");
                pParam->ErrorCode = PARSE_UNEXPECTED_STRING;
                return;
            }
            NewFuse.HammingECCType = strtoul(Buffer.substr(Pos+1).c_str(),
                                             nullptr,
                                             10);
        }

        if ((pParam->TextList.size() != 0) &&
            (pParam->TextList.front() == "reload"))
        {
            PopString(pParam);  // pops 'reload'
            Buffer = PopString(pParam);
            NewFuse.Reload = strtoul(Buffer.c_str(), nullptr, 10);
        }

        if (IsDescriptionSku)
        {
            UINT32 numSkus = atoi((*pParam->pMiscInfo).McpInfo["SKU_NUMBER"].c_str());
            string prefix = NewFuse.Name;
            for (UINT32 i = 0; i < numSkus; i++)
            {
                char suffix = '0' + i;
                NewFuse.Name = prefix + "_SKU" + suffix;
                (*pParam->pFuseDefList)[NewFuse.Name] = NewFuse;
            }
        }
        else
        {
            // push the new fuse to the fuse definition list
            (*pParam->pFuseDefList)[NewFuse.Name] = NewFuse;
        }
        pParam->TextList.clear();
    }
    // this means the fuse definition ends: return to option handler
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in ChipsOptEndHandler @line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
    }
}

void FuseXmlParser::MapOptStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if ((pParam->Depth != 5) && (pParam->Depth != 6))
    {
        Printf(Tee::PriNormal, "bad depth in MapOptStartHandler @line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    else
    {
        pParam->NextEnd = &FuseXmlParser::MapOptEndHandler;
    }
}
/*
Format:
            <key>OPT_DAC_DISABLE_CP</key>
             <ref>
              <hash>
               <key>fuse_num</key>
                <ref>
                 <hash>
                  <key>6</key>
                   <str>23:21</str>
                 </hash>
                </ref>
               <key>has_redundancy</key>
                <str>1</str>
               <key>redundant_fuse_num</key>
                <ref>
                 <hash>
                  <key>7</key>
                   <str>23:21</str>
                 </hash>
                </ref>            |Triggered here - depth = 5|
              </hash>
             </ref>               |Triggered here - depth = 4 -> store the definition|
*/
void FuseXmlParser::MapOptEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    // handle the end of a Fuse definition
    if (pParam->Depth == 5)
    {
        string Buffer = Utility::ToUpperCase(PopString(pParam));
        map<string, FuseDef>::iterator pFuseDef;
        pFuseDef = pParam->pFuseDefList->find(Buffer);
        if (pFuseDef == pParam->pFuseDefList->end())
        {
            Printf(Tee::PriNormal, "%s Fuse not found - bad XML @line %d\n",
                   Buffer.c_str(), pParam->LwrLine);
            pParam->TextList.clear();
            pParam->ErrorCode = PARSE_UNEXPECTED_STRING;
            return;
        }

        // there's another section for parsing fuseless info
        // also skip parsing pseudo fuse's useless Map_options info
        if (pFuseDef->second.Type == "fuseless" ||
            pFuseDef->second.Type == TYPE_PSEUDO)
        {
            pParam->TextList.clear();
            return;
        }

        bool HasRedundancy = false;
        bool GetPrimaryFuse = false;
        bool GetRedundantFuse = false;
        while (!pParam->TextList.empty())
        {
            Buffer = PopString(pParam);
            // determine what kind of 'map' we're reading
            if (Buffer == "fuse_num")
            {
                GetPrimaryFuse = true;
                Buffer = PopString(pParam);
            }
            else if (Buffer == "has_redundancy")
            {
                if (PopString(pParam) == "1")
                {
                    HasRedundancy = true;
                }
                GetPrimaryFuse  = false;
            }
            else if ((Buffer == "redundant_fuse_num") && HasRedundancy)
            {
                GetRedundantFuse = true;
                Buffer = PopString(pParam);
            }

            // read in the map values
            if (GetPrimaryFuse || GetRedundantFuse)
            {
                MASSERT(!(GetPrimaryFuse && GetRedundantFuse));
                FuseLoc NewLoc = {0};

                // get row number (note that
                NewLoc.Number = strtol(Buffer.c_str(), NULL, 10);
                size_t Pos = Buffer.find(":");
                if (Pos == string::npos)
                {
                    NewLoc.Number = strtol(Buffer.c_str(), NULL, 10);
                    Buffer = PopString(pParam);
                }

                // get bit location
                char *pEnd;
                NewLoc.Msb = strtol(Buffer.c_str(), &pEnd, 10);
                pEnd++;     //skip over ':'
                NewLoc.Lsb = strtol(pEnd, &pEnd, 10);

                if (GetPrimaryFuse)
                {
                    pFuseDef->second.FuseNum.push_back(NewLoc);
                    /* Create/update EccInfo struct of suitable HammingEccType
                     * matching with that of the current fuse
                     */
                    if (pFuseDef->second.HammingECCType != UNKNOWN_ECC_TYPE)
                    {
                        map<UINT32, EccInfo>::iterator pEccDef =
                                pParam->pMiscInfo->EccList.find(pFuseDef->second.HammingECCType);

                        if (pEccDef == pParam->pMiscInfo->EccList.end())
                        {
                            EccInfo NewEccInfo;
                            NewEccInfo.FirstWord = 0xFFFFFFFF;
                            NewEccInfo.FirstBit  = 0xFFFFFFFF;
                            NewEccInfo.LastWord  = 0;
                            NewEccInfo.LastBit   = 0;

                            NewEccInfo.EccType = pFuseDef->second.HammingECCType;
                            (pParam->pMiscInfo->EccList)[NewEccInfo.EccType] = NewEccInfo;
                            pEccDef = pParam->pMiscInfo->EccList.find(NewEccInfo.EccType);
                        }

                        if (NewLoc.Number < pEccDef->second.FirstWord)
                        {
                            pEccDef->second.FirstWord = NewLoc.Number;
                            pEccDef->second.FirstBit  = NewLoc.Lsb;
                        }
                        else if (NewLoc.Number == pEccDef->second.FirstWord)
                        {
                            pEccDef->second.FirstBit =
                                pEccDef->second.FirstBit < NewLoc.Lsb ?
                                pEccDef->second.FirstBit : NewLoc.Lsb;
                        }
                        else if (NewLoc.Number > pEccDef->second.LastWord)
                        {
                            pEccDef->second.LastWord = NewLoc.Number;
                            pEccDef->second.LastBit  = NewLoc.Msb;
                        }
                        else if (NewLoc.Number == pEccDef->second.LastWord)
                        {
                            pEccDef->second.LastBit =
                                pEccDef->second.LastBit > NewLoc.Msb ?
                                pEccDef->second.LastBit : NewLoc.Msb;
                        }
                    }
                }
                else // GetRedundantFuse = true
                    pFuseDef->second.RedundantFuse.push_back(NewLoc);
            }
        }
    }
    // this means the fuse definition ends: return to option handler
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
    }
    else if (pParam->Depth == 6)
    {
        // do nothing
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in ChipsOptEndHandler @line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
    }
}

void FuseXmlParser::MktOptStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 5)
    {
        Printf(Tee::PriNormal, "bad depth in MktOptStartHandler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    else
    {
        pParam->NextEnd = &FuseXmlParser::MktOptEndHandler;
    }
}

/*
Format:
            <key>100</key>
             <ref>
              <hash>
               <key>BYPASS_FUSES</key>
                <str>Enable</str>
               <key>DISABLE_FUSE_PROGRAM</key>
                <str>Enable</str>
               <key>ENABLE_FUSE_PROGRAM</key>
                <str>Enable</str>
               <key>RAMREPAIR_CHAIN_SIZE</key>
                <str>Enable</str>
               <key>opt_ce_disable</key>
                <str>Enable</str>
                .
                .
               <key>opt_y_coordinate</key>
                <str>ATE</str>
              </hash>
             </ref>    |triggered here|

*/
void FuseXmlParser::MktOptEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    // handle the end of a Fuse definition
    if (pParam->Depth == 5)
    {
        string Buffer = PopString(pParam);
        if (Buffer == "Description")
        {
            // we don't care about fuse description text
            pParam->TextList.clear();
            return;
        }

        FuseUtil::SkuList::iterator SkuConfIter;
        SkuConfIter = pParam->pSkuList->find(Buffer);
        // make sure the SKU has not been defined
        if (SkuConfIter != pParam->pSkuList->end())
        {
            Printf(Tee::PriNormal,
                "Sku %s already known - bad XML\n", Buffer.c_str());
            pParam->ErrorCode = PARSE_DUPLICATED_DEFINITION;
            return;
        }

        SkuConfig NewSku;
        NewSku.SkuName = Buffer;

        // get all the fuse values in the list:
        map<string, FuseDef>::iterator FuseDefIter;

        while (!pParam->TextList.empty())
        {
            FuseInfo FuseVal;
            Buffer = Utility::ToUpperCase(PopString(pParam));
            FuseDefIter = pParam->pFuseDefList->find(Buffer);
            if (FuseDefIter == pParam->pFuseDefList->end())
            {
                Printf(Tee::PriNormal, "Fuse %s not defined previously\n",
                       Buffer.c_str());
                pParam->ErrorCode = PARSE_MISSING_FUSEDEF;
                return;
            }
            FuseVal.StrValue = PopString(pParam);
            FuseVal.pFuseDef = &(FuseDefIter->second);
            NewSku.FuseList.push_back(FuseVal);
        }
        (*pParam->pSkuList)[NewSku.SkuName] = NewSku;
    }
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in MktOptEndHandler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
}

void FuseXmlParser::BRomPatchOptStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 5)
    {
        Printf(Tee::PriNormal, "bad depth in BRomPatchOptStartHandler @ line %d, depth = %d\n",
               pParam->LwrLine, pParam->Depth);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    else
    {
       pParam->NextStart = &FuseXmlParser::BRRevStartHandler;
       pParam->NextEnd = &FuseXmlParser::BRRevEndHandler;
    }
}

/*
Format:
            <key>Bootrom_patches</key>
             <ref>
              <hash>
               <key>T210-INT-DSC-A1</key>
               <ref>
                <hash>
                <key>name</key>
                <str>patch_1_se</str>
                <key>sequence</key>
                <str>1</str>
                <key>rows</key>
                <ref>
                  <hash>
                    <key>address</key>
                    <str>191</str>
                    <key>value</key>
                    <str>0x000141C7</str>
                    .
                    .
                    .
                    .
                  </hash>
                </ref>
                </hash>
               </ref>  |triggered here|
*/
void FuseXmlParser::BRomPatchOptEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);

    if (pParam->Depth == 5)
    {
        pParam->NextStart = &FuseXmlParser::BRomPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::BRomPatchOptEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
        pParam->TextList.clear();
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in BRomPatchOptEndHandler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
}

void FuseXmlParser::BRRevStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 6)
    {
        Printf(Tee::PriNormal, "bad depth in BRRev start handler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    else
    {
        pParam->NextEnd = &FuseXmlParser::BRRevEndHandler;
    }
}

/*
Format:
            <key>rows</key>
                <ref>
                  <hash>
                    <key>address</key>
                    <str>191</str>
                    <key>value</key>
                    <str>0x000141C7</str>
                    .
                    .
                    .
                    .
                  </hash>
                </ref>
*/
void FuseXmlParser::BRRevEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if(pParam->Depth == 6)
    {
        string Buffer = PopString(pParam);

        FuseUtil::SkuList::iterator SkuConfIter;
        SkuConfIter = pParam->pSkuList->find(Buffer);
        if (SkuConfIter != pParam->pSkuList->end())
        {
            Printf(Tee::PriDebug, "Sku %s already known - adding BRrevisions\n",
                    Buffer.c_str());
        }
        else
        {
            Printf(Tee::PriDebug, "Adding a new sku %s with its BR revs\n",
                    Buffer.c_str());
            SkuConfig NewSku;
            NewSku.SkuName = Buffer;
            (*pParam->pSkuList)[NewSku.SkuName] = NewSku;
            //Updating SkuConfIter to now point to the newly added Sku
            SkuConfIter = pParam->pSkuList->find(Buffer);
        }

        BRRevisionInfo NewBRRev;
        while (!pParam->TextList.empty())
        {
            string Field;
            Buffer = PopString(pParam);
            if (!strcmp(Buffer.c_str(), "name"))
            {
                Field = PopString(pParam);
                NewBRRev.VerName = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "sequence"))
            {
                Field = PopString(pParam);
                NewBRRev.VerNo = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "target_section"))
            {
                Field = PopString(pParam);
                NewBRRev.PatchSection = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "target_version"))
            {
                Field = PopString(pParam);
                NewBRRev.PatchVersion = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "rows"))
            {
                while (!pParam->TextList.empty())
                {
                    BRPatchRec NewBRPatRec;

                    //Parse address
                    Buffer = PopString(pParam);
                    Field = PopString(pParam);
                    UINT32 add = strtol(Field.c_str(), NULL, 10);
                    NewBRPatRec.Addr = add;

                    //Parse value
                    Buffer = PopString(pParam);
                    Field = PopString(pParam);
                    UINT32 val = strtoul(Field.c_str(), NULL, 16);
                    NewBRPatRec.Value = val;

                    NewBRRev.BRPatches.push_back(NewBRPatRec);
                }
            }
        }
        SkuConfIter->second.BRRevs.push_back(NewBRRev);
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 5)
    {
        pParam->NextStart = &FuseXmlParser::BRomPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::BRomPatchOptEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 3)
    {
        pParam->NextStart = &FuseXmlParser::XmlStartHandler;
        pParam->NextEnd = &FuseXmlParser::XmlEndHandler;
        pParam->TextList.clear();
    }
}

void FuseXmlParser::PscRomPatchOptStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 5)
    {
        Printf(Tee::PriNormal, "bad depth in PscRomPatchOptStartHandler @ line %d, depth = %d\n",
               pParam->LwrLine, pParam->Depth);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    else
    {
       pParam->NextStart = &FuseXmlParser::PscRomRevStartHandler;
       pParam->NextEnd = &FuseXmlParser::PscRomRevEndHandler;
    }
}

/*
Format:
            <key>Pscrom_patches</key>
            <ref>
              <hash>
                <key>T234-DAY1-F0-A1</key>
                <ref>
                  <hash>
                    <key>name</key>
                    <str>BPMP_ROM_patch_example</str>
                    <key>sequence</key>
                    <str>1</str>
                    <key>target_section</key>
                    <str>bpmp</str>
                    <key>target_version</key>
                    <str>bootROM_patch_version</str>
                    <key>checksum</key>
                    <str>86217aa96b4136eea32986efee3c99284327c4af</str>
                    <key>rows</key>
                    <ref>
                      <hash>
                        <key>address</key>
                        <str>487</str>
                        <key>value</key>
                        <str>0x00205678</str>
                           ...
                      </hash>
                    </ref>
                  </hash>
                </ref>
              </hash>
            </ref>	|triggered here|
*/
void FuseXmlParser::PscRomPatchOptEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);

    if (pParam->Depth == 5)
    {
        pParam->NextStart = &FuseXmlParser::PscRomPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::PscRomPatchOptEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
        pParam->TextList.clear();
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in PscRomPatchOptEndHandler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
}

void FuseXmlParser::PscRomRevStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 6)
    {
        Printf(Tee::PriNormal, "bad depth in PscRomRevs start handler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    else
    {
        pParam->NextEnd = &FuseXmlParser::PscRomRevEndHandler;
    }
}

/*
Format:
            <key>rows</key>
            <ref>
              <hash>
                <key>address</key>
                <str>498</str>
                <key>value</key>
                <str>0xBD390001</str>
                .
                .
                .
              </hash>
            </ref>
*/
void FuseXmlParser::PscRomRevEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if(pParam->Depth == 6)
    {
        string Buffer = PopString(pParam);

        FuseUtil::SkuList::iterator SkuConfIter;
        SkuConfIter = pParam->pSkuList->find(Buffer);
        if (SkuConfIter != pParam->pSkuList->end())
        {
            Printf(Tee::PriDebug, "Sku %s already known - adding PscRom Rrevisions\n",
                    Buffer.c_str());
        }
        else
        {
            Printf(Tee::PriDebug, "Adding a new sku %s with its PscRom revs\n",
                    Buffer.c_str());
            SkuConfig NewSku;
            NewSku.SkuName = Buffer;
            (*pParam->pSkuList)[NewSku.SkuName] = NewSku;
            //Updating SkuConfIter to now point to the newly added Sku
            SkuConfIter = pParam->pSkuList->find(Buffer);
        }

        PscRevisionInfo NewPscRomRev;
        while (!pParam->TextList.empty())
        {
            string Field;
            Buffer = PopString(pParam);
            if (!strcmp(Buffer.c_str(), "name"))
            {
                Field = PopString(pParam);
                NewPscRomRev.VerName = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "sequence"))
            {
                Field = PopString(pParam);
                NewPscRomRev.VerNo = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "target_section"))
            {
                Field = PopString(pParam);
                NewPscRomRev.PatchSection = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "target_version"))
            {
                Field = PopString(pParam);
                NewPscRomRev.PatchVersion = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "rows"))
            {
                while (!pParam->TextList.empty())
                {
                    PscPatchRec NewPscPatRec;

                    //Parse address
                    Buffer = PopString(pParam);
                    Field = PopString(pParam);
                    UINT32 add = strtol(Field.c_str(), NULL, 10);
                    NewPscPatRec.Addr = add;

                    //Parse value
                    Buffer = PopString(pParam);
                    Field = PopString(pParam);
                    UINT32 val = strtoul(Field.c_str(), NULL, 16);
                    NewPscPatRec.Value = val;

                    NewPscRomRev.PscPatches.push_back(NewPscPatRec);
                }
            }
        }
        SkuConfIter->second.PscRomRevs.push_back(NewPscRomRev);
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 5)
    {
        pParam->NextStart = &FuseXmlParser::PscRomPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::PscRomPatchOptEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 3)
    {
        pParam->NextStart = &FuseXmlParser::XmlStartHandler;
        pParam->NextEnd = &FuseXmlParser::XmlEndHandler;
        pParam->TextList.clear();
    }
}

void FuseXmlParser::IffPatchOptStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 5)
    {
        Printf(Tee::PriNormal, "bad depth in IffPatchOptStartHandler @ line %d, depth = %d\n",
               pParam->LwrLine, pParam->Depth);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    else
    {
       pParam->NextStart = &FuseXmlParser::IffPatchStartHandler;
       pParam->NextEnd = &FuseXmlParser::IffPatchEndHandler;
    }
}

/*
Format:
            <key>Iff_patches</key>
             <ref>
              <hash>
               <key>Sku_1</key>
               <ref>
                <hash>
                <key>name</key>
                <str>IFF_Patch_0</str>
                <key>sequence</key>
                <str>1</str>
                <key>rows</key>
                <ref>
                  <hash>
                    <key>value</key>
                    <str>0xccccfcfc</str>
                    <key>value</key>
                    <str>0x100ce7d</str>
                    .
                    .
                    .
                    .
                  </hash>
                </ref>
                </hash>
               </ref>  |triggered here|
*/
void FuseXmlParser::IffPatchOptEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);

    if (pParam->Depth == 5)
    {
        pParam->NextStart = &FuseXmlParser::IffPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::IffPatchOptEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
        pParam->TextList.clear();
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in IffPatchOptEndHandler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
}

void FuseXmlParser::IffPatchStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 6)
    {
        Printf(Tee::PriNormal, "bad depth in IffPatch start handler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    else
    {
        pParam->NextEnd = &FuseXmlParser::IffPatchEndHandler;
    }
}

/*
Format:
            <key>rows</key>
                <ref>
                  <hash>
                    <key>value</key>
                    <str>0x000141C7</str>
                    <key>value</key>
                    <str>0x000283CE</str>
                    .
                    .
                    .
                    .
                  </hash>
                </ref>
*/
void FuseXmlParser::IffPatchEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if(pParam->Depth == 6)
    {
        string Buffer = PopString(pParam);

        FuseUtil::SkuList::iterator SkuConfIter;
        SkuConfIter = pParam->pSkuList->find(Buffer);
        if (SkuConfIter != pParam->pSkuList->end())
        {
            Printf(Tee::PriDebug, "Sku %s already known - adding IFF Rows\n",
                    Buffer.c_str());
        }
        else
        {
            Printf(Tee::PriDebug, "Adding a new sku %s with its IFF Rows\n",
                    Buffer.c_str());
            SkuConfig NewSku;
            NewSku.SkuName = Buffer;
            (*pParam->pSkuList)[NewSku.SkuName] = NewSku;
            //Updating SkuConfIter to now point to the newly added Sku
            SkuConfIter = pParam->pSkuList->find(Buffer);
        }

        IFFInfo iffInfo;
        while (!pParam->TextList.empty())
        {
            string Field;
            Buffer = PopString(pParam);
            if (!strcmp(Buffer.c_str(), "name"))
            {
                Field = PopString(pParam);
                iffInfo.name = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "sequence"))
            {
                Field = PopString(pParam);
                iffInfo.num = Field.c_str();
            }
            else if (!strcmp(Buffer.c_str(), "rows"))
            {
                while (!pParam->TextList.empty())
                {
                    //Parse value
                    Buffer = PopString(pParam);
                    Field = PopString(pParam);
                    UINT32 val = strtoul(Field.c_str(), NULL, 16);
                    iffInfo.rows.push_back(val);
                }
            }
        }
        SkuConfIter->second.iffPatches.push_back(iffInfo);
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 5)
    {
        pParam->NextStart = &FuseXmlParser::IffPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::IffPatchOptEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 3)
    {
        pParam->NextStart = &FuseXmlParser::XmlStartHandler;
        pParam->NextEnd = &FuseXmlParser::XmlEndHandler;
        pParam->TextList.clear();
    }
}

/*
Format:
         <key>Fuse_encoding</key>
          <ref>
           <hash>
            <key>chipName</key>
             <str></str>
            <key>columnWidth</key>
             <str>8</str>
            <key>configFields</key>
             <ref>
*/
void FuseXmlParser::FuseEncodingStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    string TopString;
    while (pParam->TextList.size())
    {
        TopString = PopString(pParam);
        if (!strcmp(TopString.c_str(), "configFields"))
        {
            Printf(Tee::PriLow, "configFields\n");
            pParam->NextStart = &FuseXmlParser::ConfigFieldsStartHandler;
        }
        else if (!strcmp(TopString.c_str(), "rams"))
        {
            Printf(Tee::PriLow, "rams\n");
            pParam->NextStart = &FuseXmlParser::ConfigFieldsStartHandler;
        }
        else if (!strcmp(TopString.c_str(), "jtagChainMap"))
        {
            Printf(Tee::PriLow, "jtagChainMap\n");
            pParam->NextStart = &FuseXmlParser::JtagChainMapStartHandler;
        }
        else if (!strcmp(TopString.c_str(), "fuselessFuse"))
        {
            Printf(Tee::PriLow, "fuselessFuse\n");
            pParam->NextStart = &FuseXmlParser::FuselessStartHandler;
            pParam->TextList.clear();
        }
    }
}
/*
Format:
             </ref>
            <key>numRamRepairChains</key>
             <str>0</str>
            <key>ramRepairFuseBlockBegin</key>
             <str>736</str>
            <key>ramRepairFuseBlockEnd</key>
             <str>2047</str>
            <key>recordsPerRow</key>
             <str>3</str>
            <key>requiredRevNumber</key>
             <str>2.1</str>
           </hash>
          </ref>    // triggered here
*/

void FuseXmlParser::FuseEncodingEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth == 4)
    {
        string TopString;
        while (pParam->TextList.size())
        {
            TopString = PopString(pParam);
            if (!strcmp(TopString.c_str(), "ramRepairFuseBlockBegin"))
            {
                TopString = PopString(pParam);
                pParam->pMiscInfo->FuselessStart = strtol(TopString.c_str(), NULL, 10);
            }
            else if (!strcmp(TopString.c_str(), "ramRepairFuseBlockEnd"))
            {
                TopString = PopString(pParam);
                pParam->pMiscInfo->FuselessEnd = strtol(TopString.c_str(), NULL, 10);
            }
        }
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
    }
}

/*
Format:
     <key>jtagChainMap</key>
      <ref>
        <array>
          <ref>
            <hash>
              <key>chainId</key>
              <str>1</str>
              <key>chainName</key>
              <str>ccplex0_0/d_ccplex0_clstr/.*RAMREPAIR</str>
            </hash>
          </ref>
        </array>
      </ref>
*/

void FuseXmlParser::JtagChainMapStartHandler(ParserArgs* pParam)
{
    pParam->NextEnd = &FuseXmlParser::JtagChainMapEndHandler;
    pParam->TextList.clear();
}

void FuseXmlParser::JtagChainMapEndHandler(ParserArgs* pParam)
{
    if (pParam->Depth == 5)
    {
        pParam->NextStart =  &FuseXmlParser::FuseEncodingStartHandler;
        pParam->NextEnd =  &FuseXmlParser::FuseEncodingEndHandler;
        pParam->TextList.clear();  //?
        return;
    }
    else if (pParam->Depth != 6)
    {
        Printf(Tee::PriNormal, "bad depth in JtagChainMapsEndHandler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        pParam->TextList.clear();
        return;
    }
    // depth must == 6 here
    // do nothing till we understand how to process this information.
}

/*
Format:
            <key>configFields</key>
             <ref>
              <array>
               <ref>   // ConfigFieldsStartHandler trigged here
                <hash>
                 <key>blockIndex</key>
                  <str>0</str>
                 <key>blockOffset</key>
                  <str>0</str>
                 <key>chainId</key>
                  <str>2</str>
                 <key>lsbIndex</key>
                  <str>0</str>
                 <key>msbIndex</key>
                  <str>15</str>
                 <key>name</key>
                  <str>chiplet_logic_length</str>
                </hash>
               </ref>  // ConfigFieldsEndHandler triggered here
*/
void FuseXmlParser::ConfigFieldsStartHandler(ParserArgs* pParam)
{
    pParam->NextEnd = &FuseXmlParser::ConfigFieldsEndHandler;
    pParam->TextList.clear();
}

void FuseXmlParser::ConfigFieldsEndHandler(ParserArgs* pParam)
{
    if (pParam->Depth == 5)
    {
        pParam->NextStart =  &FuseXmlParser::FuseEncodingStartHandler;
        pParam->NextEnd =  &FuseXmlParser::FuseEncodingEndHandler;
        pParam->TextList.clear();  //?
        return;
    }
    else if (pParam->Depth != 6)
    {
        Printf(Tee::PriNormal, "bad depth in ConfigFieldsEndHandler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        pParam->TextList.clear();
        return;
    }
    // depth must == 6 here

    string  Buffer;
    string  Name;
    FuseLoc SubFuse = {0};
    while (pParam->TextList.size())
    {
        Buffer = PopString(pParam);
        if (!strcmp(Buffer.c_str(), "blockIndex"))
        {
            Buffer = PopString(pParam);
            SubFuse.ChainOffset = strtol(Buffer.c_str(), NULL, 10);
        }
        else if (!strcmp(Buffer.c_str(), "blockOffset"))
        {
            Buffer = PopString(pParam);
            SubFuse.DataShift = strtol(Buffer.c_str(), NULL, 10);
        }
        else if (!strcmp(Buffer.c_str(), "chainId"))
        {
            Buffer = PopString(pParam);
            SubFuse.ChainId = strtol(Buffer.c_str(), NULL, 10);
        }
        else if (!strcmp(Buffer.c_str(), "lsbIndex"))
        {
            Buffer = PopString(pParam);
            SubFuse.Lsb = strtol(Buffer.c_str(), NULL, 10);
        }
        else if (!strcmp(Buffer.c_str(), "msbIndex"))
        {
            Buffer = PopString(pParam);
            SubFuse.Msb = strtol(Buffer.c_str(), NULL, 10);
        }
        else if (!strcmp(Buffer.c_str(), "name") ||
                 !strcmp(Buffer.c_str(), "inst"))
        {
            Name = PopString(pParam);
        }
    }

    pParam->SubFuses[Name] = SubFuse;
}

void FuseXmlParser::FuselessStartHandler(ParserArgs *pParam)
{
    pParam->NextEnd = &FuseXmlParser::FuselessEndHandler;
}

void FuseXmlParser::FuselessEndHandler(ParserArgs *pParam)
{
    if (pParam->Depth == 5)
    {
        pParam->NextStart = &FuseXmlParser::FuseEncodingStartHandler;
        pParam->NextEnd = &FuseXmlParser::FuseEncodingEndHandler;
        pParam->TextList.clear();
        return;
    }

    string  Buffer;
    string  FuseName;
    vector<string> SubFuseNames;
    bool StartParsingSubFuseNames = false;
    while (pParam->TextList.size())
    {
        Buffer = PopString(pParam);
        if (StartParsingSubFuseNames)
        {
            SubFuseNames.push_back(Buffer);
        }
        else if (!strcmp(Buffer.c_str(), "fuseName"))
        {
            FuseName = PopString(pParam);
        }
        else if (!strcmp(Buffer.c_str(), "jtagConfigFields"))
        {
            StartParsingSubFuseNames = true;
        }
    }

    if (SubFuseNames.size())
    {
        map<string, FuseDef>::iterator FuseDefIter;
        FuseDefIter = pParam->pFuseDefList->find(Utility::ToUpperCase(FuseName));
        if (FuseDefIter == pParam->pFuseDefList->end())
        {
            Printf(Tee::PriNormal, "%s unknown \n", FuseName.c_str());
            return;
        }
        FuseDefIter->second.FuseNum.clear();
        for (UINT32 i = 0; i < SubFuseNames.size(); i++)
        {
            map<string, FuseLoc>::iterator SubFuseIter;
            SubFuseIter = pParam->SubFuses.find(SubFuseNames[i]);
            if (SubFuseIter == pParam->SubFuses.end())
            {
                Printf(Tee::PriNormal, "subfuse %s unknown\n",
                       SubFuseNames[i].c_str());
                pParam->ErrorCode = PARSE_ERROR;
                return;
            }
            FuseLoc NewLoc = SubFuseIter->second;
            FuseDefIter->second.FuseNum.push_back(NewLoc);
        }
    }
}

/*
This handles this part of the XML script:
 <ref>            |TRIGGERED HERE|
  <hash class="FuseConfig">
   <key>LW_FUSE</key>
    <ref>         |TRIGGERED HERE|
     <hash>
*/
void FuseXmlParser::XmlStartHandler(ParserArgs *pParam)
{
    MASSERT(pParam);
    if (pParam->Depth < 3)
    {
        pParam->TextList.clear();
        Printf(Tee::PriDebug, "XmlStartHandler handler depth < 3\n");
    }
    else if (pParam->Depth == 3)
    {
        string FirstStr = PopString(pParam);
        if (FirstStr == "options")
        {
            Printf(Tee::PriLow, "options\n");
            pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        }
        else if (FirstStr == "XML Header")
        {
            Printf(Tee::PriLow, "XML Header\n");
            pParam->NextEnd = &FuseXmlParser::HeaderHandler;
        }
        else
        {
            Printf(Tee::PriNormal, "Unknown tag\n");
            pParam->ErrorCode = PARSE_UNEXPECTED_STRING;
        }
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth for XmlStartHandler\n");
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
}

void FuseXmlParser::XmlEndHandler(ParserArgs *pParam)
{
    MASSERT(pParam);
    Printf(Tee::PriDebug, "Options end handler\n");

    pParam->NextStart = NULL;
    pParam->NextEnd = NULL;
}

/*
This handles this part of the XML script:
      <key>XML Header</key>
       <ref>         |TRIGGERED HERE|
        <hash>
         <key>A Perforce XML Tag</key>
*/

void FuseXmlParser::HeaderHandler(ParserArgs *pParam)
{
    MASSERT(pParam);
    Printf(Tee::PriDebug, "Header end handler\n");
    if (pParam->Depth == 3)
    {
        while (!pParam->TextList.empty())
        {
            string Buffer = Utility::ToUpperCase(PopString(pParam));
            if (Buffer == "A PERFORCE XML TAG")
            {
                pParam->pMiscInfo->P4Data.P4Str = PopString(pParam);
                // revision string format: '$Id: //sw/mods/chipfuse/xml_v1/gt218_f.xml#2 $'
                size_t Pos = pParam->pMiscInfo->P4Data.P4Str.find("#");
                if (Pos == string::npos)
                {
                    Printf(Tee::PriWarn, "Revision number not found in fuse XML file\n");
                    pParam->pMiscInfo->P4Data.P4Revision = 0xBADBEEF;
                    continue;
                }

                string RevStr = pParam->pMiscInfo->P4Data.P4Str.substr(Pos+1);

                // RevStr is now '2 $'
                // now get '2' and colwer it to UINT32
                Pos = RevStr.find(" ");
                if (Pos == string::npos)
                {
                    Printf(Tee::PriWarn, "Bad revision format in fuse XML file\n");
                    pParam->pMiscInfo->P4Data.P4Revision = 0xBADBEEF;
                    continue;
                }
                RevStr = RevStr.substr(0, Pos);
                pParam->pMiscInfo->P4Data.P4Revision = strtoul(RevStr.c_str(),
                                                               NULL, 10);

            }
            else if (Buffer == "GENERATION TIME")
            {
                pParam->pMiscInfo->P4Data.GeneratedTime = PopString(pParam);
            }
            else if (Buffer == "PROJECT NAME")
            {
                pParam->pMiscInfo->P4Data.ProjectName = PopString(pParam);
            }
            else if (Buffer == "STATUS")
            {
                pParam->pMiscInfo->P4Data.Validity = PopString(pParam);
            }
            else if (Buffer == "VERSION")
            {
                string VersionStr = PopString(pParam);
                pParam->pMiscInfo->P4Data.Version = strtoul(VersionStr.c_str(), NULL, 10);
            }
        }
    }
    pParam->NextStart = NULL;
    pParam->NextEnd = NULL;
}

/*
This handles this part of the XML script:
    <key>Pseudo_fuses</key>
    <ref>
      <hash>
        <key>pseudo_a9_ape_disable</key>
        <ref>           | TRIGGERED HERE : Depth = 5 |
*/
void FuseXmlParser::PseudoFuseStartHandler(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 5)
    {
        Printf(Tee::PriNormal,
               "bad depth in PseudoFuseStartHandler @ line %d, depth = %d\n",
               pParam->LwrLine, pParam->Depth);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }
    MASSERT(!pParam->TextList.empty());

    // pseudo FuseDef should have been defined in Chip_options already
    FuseDefMap *pFuseDefMap = pParam->pFuseDefList;
    string Name = Utility::ToUpperCase(pParam->TextList.front());
    if (pFuseDefMap->find(Name) == pFuseDefMap->end())
    {
        Printf(Tee::PriNormal, "Pseudo fuse %s not defined previously.\n",
               Name.c_str());
        pParam->ErrorCode = PARSE_MISSING_FUSEDEF;
        return;
    }
    pParam->NextEnd = &FuseXmlParser::PseudoFuseEndHandler;
}

/*
This handles this part of the XML script:
    <key>Pseudo_fuses</key>
    <ref>
      <hash>
        <key>pseudo_a9_ape_disable</key>
        <ref>
          <array>
            <str>ip_disable[0]</str>
          </array>
        </ref>          | TRIGGERED HERE : Depth = 5 |

        <key>pseudo_adc_calib_valid</key>
        <ref>
          <array>
            <str>eco_reserve_0[0]</str>
          </array>
        </ref>
      </hash>
    </ref>              | TRIGGERED HERE : Depth = 4 |
*/
void FuseXmlParser::PseudoFuseEndHandler(ParserArgs* pParam)
{
    MASSERT(pParam);

    if (pParam->Depth == 5)
    {
        FuseDefMap *pFuseDefMap = pParam->pFuseDefList;
        string PseudoFuseName = Utility::ToUpperCase(PopString(pParam));

        FuseDef *pPseudoFuseDef = &(*pFuseDefMap)[PseudoFuseName];

        // set 'pseudo' type (it might be configured as 'fuseless' in xml)
        pPseudoFuseDef->Type = FuseUtil::TYPE_PSEUDO;

        Printf(Tee::PriDebug, "[PseudoFuse] Parse pseudo fuse %s\n", pPseudoFuseDef->Name.c_str());
        while (!pParam->TextList.empty())
        {
            // Example:
            //   "ip_disable[1]"
            //   "boot_lw_info[19:14]"
            string MasterFuseStr = Utility::ToUpperCase(PopString(pParam));
            string MasterFuseName = MasterFuseStr;
            // If bit limits aren't specified, use the full fuse
            UINT32 HighBit = 31;
            UINT32 LowBit = 0;
            auto Index = MasterFuseStr.find('[');
            if (Index != string::npos)
            {
                MasterFuseName = MasterFuseName.substr(0, Index);
                string BitField = MasterFuseStr.substr(
                        Index + 1, MasterFuseStr.size() - Index - 2);

                char *pSep = nullptr;
                HighBit = strtoul(BitField.c_str(), &pSep, 10);
                LowBit = HighBit;
                if (*pSep == ':')
                {
                    LowBit = strtoul(pSep + 1, nullptr, 10);
                }
            }

            Printf(Tee::PriDebug, "[PseudoFuse]  map to master fuse %s[%d:%d]\n",
                    MasterFuseName.c_str(), HighBit, LowBit);

            if (pFuseDefMap->find(MasterFuseName) == pFuseDefMap->end())
            {
                Printf(Tee::PriError, "Master fuse %s not found!\n",
                       MasterFuseName.c_str());
                pParam->ErrorCode = PARSE_MISSING_FUSEDEF;
                return;
            }

            // build bi-directional mapping between pseudo and master fuses
            // make it easier to compose/decompose
            FuseDef *pMasterFuseDef = &(*pFuseDefMap)[MasterFuseName];

            pPseudoFuseDef->subFuses.push_back(
                    make_tuple(pMasterFuseDef, HighBit, LowBit));

            pMasterFuseDef->PseudoFuseMap[PseudoFuseName] = \
                make_pair(HighBit, LowBit);
        }

        pParam->TextList.clear();
    }
    else if (pParam->Depth == 4)
    {
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
        pParam->TextList.clear();
    }
    else
    {
        Printf(Tee::PriNormal, "bad depth in PseudoFuseEndHandler @ line %d\n",
               pParam->LwrLine);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
    }
}

/*
This handles this part of the XML script:
      <key>options</key>
       <ref>         |TRIGGERED HERE|
        <hash>
         <key>Chip_options</key>
         or
         <key>Map_options</key>
         or
         <key>Mkt_options</key>
         |TRIGGERED HERE|
*/
void FuseXmlParser::OptionStartHandler(ParserArgs *pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 4)
    {
        Printf(Tee::PriNormal, "bad depth for OptionStartHandler %d\n",pParam->Depth);
        pParam->ErrorCode = PARSE_UNEXPECTED_DEPTH;
        return;
    }

    string FirstStr = PopString(pParam);
    if (!strcmp(FirstStr.c_str(), "Chip_options"))
    {
        Printf(Tee::PriDebug, "Chip_options\n");
        pParam->NextStart = &FuseXmlParser::ChipsOptStartHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "All_Fuse_options"))
    {
        Printf(Tee::PriDebug, "Fuse_options\n");
        pParam->NextStart = &FuseXmlParser::FuseOptStartHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Free_text"))
    {
        Printf(Tee::PriDebug, "Free_text\n");
        pParam->NextStart = &FuseXmlParser::FreetextStartHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Map_options"))
    {
        Printf(Tee::PriDebug, "Map_options\n");
        pParam->NextStart = &FuseXmlParser::MapOptStartHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Mkt_options"))
    {
        Printf(Tee::PriDebug, "Mkt_options\n");
        pParam->NextStart = &FuseXmlParser::MktOptStartHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Bootrom_patches"))
    {
        Printf(Tee::PriDebug, "Bootrom_patches\n");
        pParam->NextStart = &FuseXmlParser::BRomPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::BRomPatchOptEndHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Pscrom_patches"))
    {
        Printf(Tee::PriDebug, "Pscrom_patches\n");
        pParam->NextStart = &FuseXmlParser::PscRomPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::PscRomPatchOptEndHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "IFF_patches"))
    {
        Printf(Tee::PriDebug, "IFF_patches\n");
        pParam->NextStart = &FuseXmlParser::IffPatchOptStartHandler;
        pParam->NextEnd = &FuseXmlParser::IffPatchOptEndHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Fuse_encoding"))
    {
        Printf(Tee::PriDebug, "Fuse_encoding\n");
        pParam->NextStart = &FuseXmlParser::FuseEncodingStartHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Fs_fb_configurations"))
    {
        Printf(Tee::PriDebug, "Fs_fb_configurations\n");
        pParam->NextStart = &FuseXmlParser::FbConfigStartHandler;
        pParam->NextEnd = &FuseXmlParser::FbConfigEndHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Fuse_macro"))
    {
        Printf(Tee::PriDebug, "Fuse_macro\n");
        pParam->NextStart = &FuseXmlParser::FuseMacroStartHandler;
        pParam->NextEnd = &FuseXmlParser::FuseMacroEndHandler;
    }
    else if (!strcmp(FirstStr.c_str(), "Pseudo_fuses"))
    {
        Printf(Tee::PriDebug, "Pseudo_fuses\n");
        pParam->NextStart = &FuseXmlParser::PseudoFuseStartHandler;
    }
    else
    {
        Printf(Tee::PriDebug, "Unknown option - %s\n",FirstStr.c_str());
        pParam->NextStart = &FuseXmlParser::UnknownOptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::UnknownOptionEndHandler;
    }
}

void FuseXmlParser::UnknownOptionStartHandler(ParserArgs *pParam)
{
}

void FuseXmlParser::UnknownOptionEndHandler(ParserArgs *pParam)
{
    if (pParam->Depth == 4)
    {
        pParam->TextList.clear();
        pParam->NextStart = &FuseXmlParser::OptionStartHandler;
        pParam->NextEnd = &FuseXmlParser::OptionEndHandler;
    }
}

void FuseXmlParser::FbConfigStartHandler(FuseXmlParser::ParserArgs* pParam)
{
}

//-----------------------------------------------------------------------------
// Format:
//          <key>Fs_fb_configurations</key>
//          <ref>
//           <hash>
//            <key>fuse_names</key>
//             <ref>
//              <array>
//               <str>INFO_VALID</str>
//               <str>INFO_FBP_COUNT</str>
//               <str>OPT_FBP_DISABLE</str>
//               <str>OPT_FBIO_DISABLE</str>
//              <str>OPT_FBIO_SHIFT</str>
//              </array>
//             </ref>
//            <key>perforce_id</key>
//             <str>$Id: //hw/chpsoln/scripts/fuse_xml/gf110.pm#4 $</str>
//            <key>values</key>   ------> start collecting data here
//             <ref>
//              <array>
//               <ref>
//                <array>
//                 <str>1</str>
//                 <str>6</str>
//                <str>0x00</str>
//                 <str>0x00</str>
//                 <str>0x00</str>
//                </array>
//               </ref>
//               <ref>
void FuseXmlParser::FbConfigEndHandler(FuseXmlParser::ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->Depth != 4)
        return;

    // return back to top
    pParam->NextStart = &FuseXmlParser::OptionStartHandler;
    pParam->NextEnd =   &FuseXmlParser::OptionEndHandler;

    string Buffer;
    bool TableStarts = false;
    FuseDataSet *pDataSet = pParam->pDataSet;
    if (!pDataSet)
    {
        pParam->TextList.clear();
        return;
    }

    pDataSet->ResetFbConfig();

    while (pParam->TextList.size())
    {
        if (!TableStarts)
        {
            Buffer = PopString(pParam);
            if (Buffer == "values")
                TableStarts = true;
        }

        if (TableStarts & (pParam->TextList.size() >= 5))
        {
            Buffer = PopString(pParam);
            UINT32 IsValid = strtol(Buffer.c_str(), NULL, 10);
            Buffer = PopString(pParam);
            UINT32 NumFbp = strtol(Buffer.c_str(), NULL, 10);
            Buffer = PopString(pParam);
            UINT32 FbpDisable = strtol(Buffer.c_str(), NULL, 16);
            Buffer = PopString(pParam);
            UINT32 FbioDisable = strtol(Buffer.c_str(), NULL, 16);
            Buffer = PopString(pParam);
            UINT32 FbioShift = strtol(Buffer.c_str(), NULL, 16);
            pDataSet->AddFbConfig((IsValid == 1)? true:false,
                                  NumFbp,
                                  FbpDisable,
                                  FbioDisable,
                                  FbioShift);
        }
    }
    pParam->TextList.clear();
}

void FuseXmlParser::FuseMacroStartHandler(ParserArgs *pParam)
{
}

//-----------------------------------------------------------------------------
//Format:
//        <key>Fuse_macro</key>
//        <ref>
//          <hash>
//            <key>num_fuse_rows</key>
//            <str>192</str>
//            <key>num_fuse_cols</key>
//            <str>32</str>
//            <key>fuse_record_start</key>
//            <str>105</str>
//          </hash>
//        </ref>  |triggered here depth = 4|
void FuseXmlParser::FuseMacroEndHandler(ParserArgs *pParam)
{
    MASSERT(pParam);
    if (pParam->Depth == 4)
    {
        while (!pParam->TextList.empty())
        {
            string Field;
            string Buffer = PopString(pParam);
            if (!strcmp(Buffer.c_str(), "num_fuse_rows"))
            {
                Field = PopString(pParam);
                pParam->pMiscInfo->MacroInfo.FuseRows =
                    strtoul(Field.c_str(), nullptr, 10);
            }
            else if (!strcmp(Buffer.c_str(), "num_fuse_cols"))
            {
                Field = PopString(pParam);
                pParam->pMiscInfo->MacroInfo.FuseCols =
                    strtoul(Field.c_str(), nullptr, 10);
            }
            else if (!strcmp(Buffer.c_str(), "fuse_record_start"))
            {
                Field = PopString(pParam);
                pParam->pMiscInfo->MacroInfo.FuseRecordStart =
                    strtoul(Field.c_str(), nullptr, 10);
            }
        }
        pParam->TextList.clear();
    }
    else if (pParam->Depth == 3)
    {
        pParam->NextStart = &FuseXmlParser::XmlStartHandler;
        pParam->NextEnd = &FuseXmlParser::XmlEndHandler;
        pParam->TextList.clear();
    }
}

void FuseXmlParser::OptionEndHandler(ParserArgs *pParam)
{
    pParam->NextStart = &FuseXmlParser::XmlStartHandler;
    pParam->NextEnd = &FuseXmlParser::XmlEndHandler;
}

void FuseXmlParser::Start(void *data, const char *el, const char **attr)
{
    MASSERT(data);
    FuseXmlParser::ParserArgs *pParam = (FuseXmlParser::ParserArgs*)data;
    // ignore these tags
    if (!strcmp(el, "hash") || !strcmp(el, "undef"))
        return;

    if (!strcmp(el, "ref"))
    {
        pParam->Depth++;

        if (pParam->NextStart == NULL)
        {
            // begin with 'Xml handler'
            pParam->NextStart = &FuseXmlParser::XmlStartHandler;
            pParam->NextEnd = &FuseXmlParser::XmlEndHandler;
        }
        // execute the appropriate handler
        if (pParam->NextStart)
            pParam->NextStart(pParam);
    }
    if (!strcmp(el, "key") || !strcmp(el, "str"))
    {
        pParam->PushNextString = true;
        return;
    }
}

void FuseXmlParser::End(void *data, const char *el)
{
    MASSERT(data);
    FuseXmlParser::ParserArgs *pParam = (FuseXmlParser::ParserArgs*)data;

    if (pParam->PushNextString)
    {
        pParam->PushNextString = false;
        if (pParam->TextList.empty())
        {
            pParam->TextList.push_back("");
        }
    }

    if (!strcmp(el, "hash") || !strcmp(el, "undef"))
        return;

    if (!strcmp(el, "ref"))
    {
        if (pParam->NextEnd)
            pParam->NextEnd(pParam);

        pParam->Depth--;
    }
}

//! This is the character handler - this is ilwoked each time the
//! parser see a 'character' between tags
//! We will skip the string unless it is specified by other handlers
void FuseXmlParser::CharacterHandler(void *data, const char *txt, int txtlen)
{
    MASSERT(data);
    MASSERT(txt);
    FuseXmlParser::ParserArgs *pParam = (FuseXmlParser::ParserArgs*)data;
    if (!pParam->PushNextString)
    {
        return;
    }

    string TempStr (txt, txtlen);

    // Skip the string if it consists only of whitespace characters
    bool allWhitespace = !TempStr.empty();
    for (size_t i=0; i < TempStr.size(); i++)
    {
        if (!isspace(TempStr[i]))
        {
            allWhitespace = false;
            break;
        }
    }
    if (allWhitespace)
    {
        return;
    }

    pParam->TextList.push_back(TempStr);
    pParam->PushNextString = false;
}
//-----------------------------------------------------------------------------
//! Brief: This is the function that sets up the Expat parser and run through
// the specified XML file. It will report abort the Parser if the handlers
// passes back a bad return code ParserParam
RC FuseXmlParser::ParseFileImpl(const string &FileName,
                                FuseDefMap   *pFuseDefMap,
                                SkuList      *pSkuList,
                                MiscInfo     *pMiscInfo,
                                FuseDataSet  *pFuseDataSet)
{
    MASSERT(pFuseDefMap && pSkuList && pMiscInfo && pFuseDataSet);

    RC         rc;
    ParserArgs ParserParam = {0};
    int        StopParse   = 0;

    ObjectHolder<XML_Parser> Parser(XML_ParserCreate(NULL), XML_ParserFree);

    if (!Parser)
        return RC::SOFTWARE_ERROR;

    // initialize the members:
    pSkuList->clear();
    pMiscInfo->P4Data.GeneratedTime = "";
    pMiscInfo->P4Data.P4Str = "";
    pMiscInfo->P4Data.ProjectName = "";
    pMiscInfo->P4Data.Version = 0xBADBEEF; //invalid XML version
    pMiscInfo->P4Data.Validity = "Not Found";
    pMiscInfo->McpInfo.clear();

    ParserParam.pFuseDefList = pFuseDefMap;
    ParserParam.pDataSet     = pFuseDataSet;
    ParserParam.pSkuList     = pSkuList;
    ParserParam.pMiscInfo    = pMiscInfo;
    ParserParam.pParser      = this;

    // setup the XML handlers
    XML_SetElementHandler(Parser, FuseXmlParser::Start, FuseXmlParser::End);
    XML_SetCharacterDataHandler(Parser, FuseXmlParser::CharacterHandler);
    XML_SetUserData(Parser, (void *) &ParserParam);

    // buffer to store one line of xml
    const UINT32 LINE_LENGTH = 1024;
    char pBuff[LINE_LENGTH];
    vector<char>XmlBuff;

    CHECK_RC(Utility::ReadPossiblyEncryptedFile(FileName, &XmlBuff, NULL));

    UINT32 Pos = 0;
    for (;;)
    {
        BufferGets(pBuff, LINE_LENGTH, XmlBuff, &Pos);
        if ((Pos >= XmlBuff.size()) || (ParserParam.ErrorCode != PARSE_OK))
        {
            Printf(Tee::PriLow, "Parse ends\n");
            StopParse = 1;
            break;
        }
        if (!XML_Parse(Parser, pBuff, (int)strlen(pBuff), StopParse))
        {
            Printf(Tee::PriNormal, "Parse error at line %d:\n%s\n",
                   static_cast<int>(XML_GetLwrrentLineNumber(Parser)),
                   XML_ErrorString(XML_GetErrorCode(Parser)));
            break;
        }
        ParserParam.LwrLine++;
    }

    // Process the information retrieved from XML
    CHECK_RC(InterpretSkuFuses(pSkuList));
    RemoveTraditionalIff(pSkuList); // WAR for RFE 1633076
    CHECK_RC(GetParserStatus(ParserParam.ErrorCode));

    return rc;
}

//-----------------------------------------------------------------------------
// Find the number of fuses that has the specified attribute with the given SKU
//
// MAYBE THIS ONE BELINGS IN FUSE.CPP
UINT32 FuseXmlParser::GetNumFuseType(const string SkuName,
                                INT32 FuseAttrib,
                                FuseUtil::SkuList *pSkuList)
{
    FuseUtil::SkuList::const_iterator SkuIter;
    SkuIter = pSkuList->find(SkuName);
    // make sure the SKU exists:
    if (SkuIter == pSkuList->end())
    {
        Printf(Tee::PriNormal, "SKU %s is unknown\n", SkuName.c_str());
        return 0;
    }

    UINT32 NumFound = 0;
    list<FuseInfo>::const_iterator FuseValIter =
        SkuIter->second.FuseList.begin();
    for (; FuseValIter != SkuIter->second.FuseList.end(); FuseValIter++)
    {
        if (FuseValIter->Attribute & FuseAttrib)
            NumFound++;
    }

    return NumFound;
}

// Pop the top string from the list and colwer to upper case
string FuseXmlParser::PopString(ParserArgs* pParam)
{
    MASSERT(pParam);
    if (pParam->TextList.size() == 0)
    {
        Printf(Tee::PriNormal,
               "trying to pop string when string stack stack is empty!\n");
        return "";
    }
    string Buffer = pParam->TextList.front();
    pParam->TextList.pop_front();
    return Buffer;
}

// ---------------- internal functions ---------------------

RC FuseXmlParser::GetParserStatus(ParserError ErrorCode)
{
    switch(ErrorCode)
    {
        case PARSE_OK:
            return OK;
        case PARSE_UNEXPECTED_ELEMENT:
            Printf(Tee::PriNormal, "Unknown XML format\n");
            return RC::SOFTWARE_ERROR;
        case PARSE_UNEXPECTED_STRING:
            Printf(Tee::PriNormal, "Bad string in XML file\n");
            return RC::SOFTWARE_ERROR;
        case PARSE_MISSING_FUSEDEF:
            Printf(Tee::PriNormal, "Fuse name not defined\n");
            return RC::SOFTWARE_ERROR;
        case PARSE_DUPLICATED_DEFINITION:
            Printf(Tee::PriNormal, "Multiple definition\n");
            return RC::SOFTWARE_ERROR;
        default:
            Printf(Tee::PriNormal, "Unknown parser error.\n");
            return RC::SOFTWARE_ERROR;
    }
}

JS_STEST_LWSTOM(FuseXml, SetVerbose, 1, "dump fuses")
{
    MASSERT(pArguments != 0);

    bool IsEnable;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &IsEnable)))
    {
        JS_ReportError(pContext, "Usage: FuseXml.SetVerbose(1/0)");
        return JS_FALSE;
    }

    RETURN_RC(FuseParser::SetVerbose(IsEnable));
}
