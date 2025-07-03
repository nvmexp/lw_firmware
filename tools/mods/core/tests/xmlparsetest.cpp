/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xmlparsetest.h"

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include <memory>

XmlParseTest XmlParseTest::s_Instance = XmlParseTest();

XmlParseTest::XmlParseTest()
:
m_XmlFilenameOverride("")
{
}

RC XmlParseTest::TestParseXml(string chipName)
{
    RC rc;
    // we don't need the results, so nullptr's fine
    CHECK_RC(ParseXml(chipName, nullptr));

    // If we get to here, it parsed without issue
    Printf(Tee::PriNormal, "Fuse XML %s parses.\n",
        GetXmlFilename(chipName).c_str());
    return rc;
}

RC XmlParseTest::VerifyXml(string chipName)
{
    Printf(Tee::PriHigh, "Simple Fuse Verification unsupported for now,\n");
    Printf(Tee::PriHigh, "    please use -verify_full_xml.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

RC XmlParseTest::VerifyFullXml(string chipName)
{
    RC rc;

    map<string, FuseUtil::FuseDef> xmlFuses;

    CHECK_RC(ParseXml(chipName, &xmlFuses));

    vector<string> flaggedFuses;

    // Flag all fuses to make sure there are no overlaps
    map<string, FuseUtil::FuseDef>::iterator fuseIter;
    for (fuseIter = xmlFuses.begin(); fuseIter != xmlFuses.end(); fuseIter++)
    {
        flaggedFuses.push_back(fuseIter->first);
    }

    vector<pair<string, string> > overlappingFuses;
    CheckFlaggedFusesForOverlap(flaggedFuses, xmlFuses, &overlappingFuses);

    rc = HandleOverlappingFuseResults(overlappingFuses);

    if (rc == OK)
    {
        Printf(Tee::PriNormal,
            "Fuse XML %s parses, and the fuse definitions don't overlap\n",
            GetXmlFilename(chipName).c_str());
    }

    return rc;
}

RC XmlParseTest::ExportGoldensForChip(string chipName)
{
    Printf(Tee::PriHigh, "Fuse Golden Export unsupported for now.\n");
    return RC::UNSUPPORTED_FUNCTION;
}

void XmlParseTest::SetXmlFilename(string filename)
{
    m_XmlFilenameOverride = filename;
}

string XmlParseTest::GetXmlFilename(string chipName)
{
    if (m_XmlFilenameOverride != "")
    {
        return m_XmlFilenameOverride;
    }
    return Utility::ToLowerCase(chipName) + "_f.xml";
}

void XmlParseTest::CategorizeFuses
(
    const map<string, FuseUtil::FuseDef>& xmlFuses,
    map<string, FuseUtil::FuseDef> expectedFuses,
    vector<string>* pAddedFuseNames,
    vector<string>* pChangedFuseNames,
    vector<string>* pRemovedFuseNames,
    vector<string>* pFlaggedFuses
)
{
    MASSERT(pAddedFuseNames);
    MASSERT(pChangedFuseNames);
    MASSERT(pRemovedFuseNames);
    MASSERT(pFlaggedFuses);

    map<string, FuseUtil::FuseDef>::const_iterator xmlFuse;
    map<string, FuseUtil::FuseDef>::iterator expectedFuse;

    // Categorize each fuse def differing from the expected as
    // "Added", "Changed", or "Removed" and flag any that are "Added"
    // or "Changed" as they potentially overlap other fuses
    for (xmlFuse = xmlFuses.begin(); xmlFuse != xmlFuses.end(); xmlFuse++)
    {
        expectedFuse = expectedFuses.find(xmlFuse->first);

        // Added
        if (expectedFuse == expectedFuses.end())
        {
            pAddedFuseNames->push_back(xmlFuse->first);
            pFlaggedFuses->push_back(xmlFuse->first);
            continue;
        }

        // Changed
        if (xmlFuse->second != expectedFuse->second)
        {
            pChangedFuseNames->push_back(xmlFuse->first);
            pFlaggedFuses->push_back(xmlFuse->first);
        }

        // Erase fuses from the expected list to leave the
        // fuses that have been Removed from the XML
        expectedFuses.erase(expectedFuse);
    }

    // Removed
    for (expectedFuse = expectedFuses.begin();
         expectedFuse != expectedFuses.end();
         expectedFuse++)
    {
        pRemovedFuseNames->push_back(expectedFuse->first);
    }
}

void XmlParseTest::CheckFlaggedFusesForOverlap
(
    const vector<string>& flaggedFuses,
    const map<string, FuseUtil::FuseDef>& xmlFuses,
    vector<pair<string, string> >* pOverlappingFuses
)
{
    MASSERT(pOverlappingFuses);

    vector<string>::const_iterator flagged, tempStrIter;
    map<string, FuseUtil::FuseDef>::const_iterator xmlFuse;

    for (flagged = flaggedFuses.begin();
         flagged != flaggedFuses.end();
         flagged++)
    {
        for (xmlFuse = xmlFuses.begin();
             xmlFuse != xmlFuses.end();
             xmlFuse++)
        {
            // Avoid self-comparison
            if (xmlFuse->first == *flagged)
            {
                continue;
            }
            bool overlap = CheckFuseDefsOverlap(xmlFuse->second, xmlFuses.find(*flagged)->second);
            if (overlap)
            {
                // If the overlapping fuse was a flagged fuse checked
                // earlier then skip adding a redundant entry
                for (tempStrIter = flaggedFuses.begin();
                     tempStrIter != flagged && overlap;
                     tempStrIter++)
                {
                    if (xmlFuse->first == *tempStrIter)
                    {
                        overlap = false;
                    }
                }
            }
            if (overlap)
            {
                pOverlappingFuses->push_back(
                    pair<string, string>(*flagged, xmlFuse->first));
            }
        }
    }
}

bool XmlParseTest::CheckFuseDefsOverlap
(
    const FuseUtil::FuseDef& lhs,
    const FuseUtil::FuseDef& rhs
)
{
    // Don't bother checking pseudo fuses
    if (lhs.Type == FuseUtil::TYPE_PSEUDO || rhs.Type == FuseUtil::TYPE_PSEUDO)
    {
        return false;
    }

    bool lFuseless = (lhs.Type == "fuseless");
    bool rFuseless = (rhs.Type == "fuseless");

    if (lFuseless != rFuseless)
    {
        return false;
    }

    bool overlap = false;

    // Union the two FuseLoc Lists, since they both
    // potentially overlap another FuseLoc
    list<FuseUtil::FuseLoc> lFuses(lhs.FuseNum);
    lFuses.insert(lFuses.end(),
        lhs.RedundantFuse.begin(), lhs.RedundantFuse.end());

    list<FuseUtil::FuseLoc> rFuses(rhs.FuseNum);
    rFuses.insert(rFuses.end(),
        rhs.RedundantFuse.begin(), rhs.RedundantFuse.end());

    list<FuseUtil::FuseLoc>::const_iterator lIter, rIter;
    for (lIter = lFuses.begin(); lIter != lFuses.end() && !overlap; lIter++)
    {
        for (rIter = rFuses.begin(); rIter != rFuses.end() && !overlap; rIter++)
        {
            if (lFuseless)
            {
                overlap = overlap || CheckFuselessFusesOverlap(*lIter, *rIter);
            }
            else
            {
                overlap = overlap || CheckRawFusesOverlap(*lIter, *rIter);
            }
        }
    }
    return overlap;
}

bool XmlParseTest::CheckRawFusesOverlap
(
    const FuseUtil::FuseLoc& lhs,
    const FuseUtil::FuseLoc& rhs
)
{

    return lhs.Number == rhs.Number &&
        SigBitsOverlap(lhs.Lsb, lhs.Msb, rhs.Lsb, rhs.Msb);
}

bool XmlParseTest::CheckFuselessFusesOverlap
(
    const FuseUtil::FuseLoc& lhs,
    const FuseUtil::FuseLoc& rhs
)
{
    return lhs.ChainId == rhs.ChainId &&
        SigBitsOverlap(lhs.Lsb, lhs.Msb, rhs.Lsb, rhs.Msb);
}

bool XmlParseTest::SigBitsOverlap(UINT32 aLsb, UINT32 aMsb,
                                    UINT32 bLsb, UINT32 bMsb)
{
    // noOverlap = aMsb < bLsb or aLsb > bMsb
    return (aMsb >= bLsb) && (aLsb <= bMsb);
}

RC XmlParseTest::HandleFlaggedFuseResults
(
    const vector<string>& addedFuseNames,
    const vector<string>& removedFuseNames,
    const vector<string>& changedFuseNames
)
{
    RC rc;

    vector<string>::const_iterator tempStrIter;

    if (!addedFuseNames.empty())
    {
        rc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriNormal, "Added Fuses:\n");
        for (tempStrIter = addedFuseNames.begin();
             tempStrIter != addedFuseNames.end();
             tempStrIter++)
        {
            Printf(Tee::PriNormal, "    %s\n", tempStrIter->c_str());
        }
        Printf(Tee::PriNormal, "\n");
    }
    if (!removedFuseNames.empty())
    {
        rc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriNormal, "Removed Fuses:\n");
        for (tempStrIter = removedFuseNames.begin();
             tempStrIter != removedFuseNames.end();
             tempStrIter++)
        {
            Printf(Tee::PriNormal, "    %s\n", tempStrIter->c_str());
        }
        Printf(Tee::PriNormal, "\n");
    }
    if (!changedFuseNames.empty())
    {
        rc = RC::SOFTWARE_ERROR;
        Printf(Tee::PriNormal, "Changed Fuses:\n");
        for (tempStrIter = changedFuseNames.begin();
             tempStrIter != changedFuseNames.end();
             tempStrIter++)
        {
            Printf(Tee::PriNormal, "    %s\n", tempStrIter->c_str());
        }
        Printf(Tee::PriNormal, "\n");
    }

    return rc;
}

RC XmlParseTest::HandleOverlappingFuseResults
(
    const vector<pair<string, string> >& pOverlappingFuses
)
{
    if (!pOverlappingFuses.empty())
    {
        Printf(Tee::PriHigh, "The following fuse pairs overlap:\n");
        vector<pair<string, string> >::const_iterator pair;
        for (pair = pOverlappingFuses.begin();
             pair != pOverlappingFuses.end();
             pair++)
        {
            Printf(Tee::PriHigh, "    %s - %s\n", pair->first.c_str(),
                pair->second.c_str());
        }
        Printf(Tee::PriNormal, "\n");
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC XmlParseTest::ParseXml
(
    string chipName,
    map<string, FuseUtil::FuseDef>* pXmlFuses
)
{
    RC rc;
    MASSERT(chipName != "");

    string filename = GetXmlFilename(chipName);

    const FuseUtil::FuseDefMap* fuseDefMap;
    const FuseUtil::SkuList* skuList;
    const FuseUtil::MiscInfo* Info;

    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(filename));
    CHECK_RC(pParser->ParseFile(filename, &fuseDefMap, &skuList, &Info));
    if (pXmlFuses != nullptr)
    {
        pXmlFuses->insert(fuseDefMap->begin(),
                          fuseDefMap->end());
    }
    return rc;
}

JS_CLASS(XmlParseTest);

static SObject XmlParseTest_Object
(
    "XmlParseTest",
    XmlParseTestClass,
    0,
    0,
    "Fuse Xml Parse Test class"
);

C_(XmlParseTest_ExportGoldensForChip);
static SMethod XmlParseTest_ExportGoldensForChip
(
    XmlParseTest_Object,
    "ExportGoldensForChip",
    C_XmlParseTest_ExportGoldensForChip,
    1,
    "Export golden values based on current FuseXml",
    MODS_INTERNAL_ONLY
);
C_(XmlParseTest_ExportGoldensForChip)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    string        chipName;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &chipName)))
    {
        JS_ReportError(pContext, "Usage: XmlParseTest.ExportGoldensForChip(chipName)");
        return JS_FALSE;
    }

    RETURN_RC(XmlParseTest::GetInstance().ExportGoldensForChip(chipName));
}

C_(XmlParseTest_VerifyXml);
static SMethod XmlParseTest_VerifyXml
(
    XmlParseTest_Object,
    "VerifyXml",
    C_XmlParseTest_VerifyXml,
    1,
    "Verify a chip's XML against last known good values",
    MODS_INTERNAL_ONLY
);
C_(XmlParseTest_VerifyXml)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    string        chipName;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &chipName)))
    {

        JS_ReportError(pContext, "Usage: XmlParseTest.VerifyXml(chipName)");
        return JS_FALSE;

    }

    RETURN_RC(XmlParseTest::GetInstance().VerifyXml(chipName));
}

C_(XmlParseTest_VerifyFullXml);
static SMethod XmlParseTest_VerifyFullXml
(
    XmlParseTest_Object,
    "VerifyFullXml",
    C_XmlParseTest_VerifyFullXml,
    1,
    "Verify a chip's XML from scratch",
    MODS_INTERNAL_ONLY
);
C_(XmlParseTest_VerifyFullXml)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    string        chipName;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &chipName)))
    {
        JS_ReportError(pContext, "Usage: XmlParseTest.VerifyXml(chipName)");
        return JS_FALSE;
    }

    RETURN_RC(XmlParseTest::GetInstance().VerifyFullXml(chipName));
}

C_(XmlParseTest_TestParseXml);
static SMethod XmlParseTest_TestParseXml
(
    XmlParseTest_Object,
    "TestParseXml",
    C_XmlParseTest_TestParseXml,
    1,
    "Run the XML Parser on the chip's XML to see if it's formatted correctly",
    MODS_INTERNAL_ONLY
);
C_(XmlParseTest_TestParseXml)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    string        chipName;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &chipName)))
    {
        JS_ReportError(pContext, "Usage: XmlParseTest.TestParseXml(chipName)");
        return JS_FALSE;
    }

    RETURN_RC(XmlParseTest::GetInstance().TestParseXml(chipName));
}

C_(XmlParseTest_SetXmlFilename);
static SMethod XmlParseTest_SetXmlFilename
(
    XmlParseTest_Object,
    "SetXmlFilename",
    C_XmlParseTest_SetXmlFilename,
    1,
    "Set the name of the xml file to use (default is derived from chip's name",
    MODS_INTERNAL_ONLY
);
C_(XmlParseTest_SetXmlFilename)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJS;
    string        filename;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &filename)))
    {
        JS_ReportError(pContext, "Usage: XmlParseTest.SetXmlFile(xmlFilename)");
        return JS_FALSE;
    }

    XmlParseTest::GetInstance().SetXmlFilename(filename);

    RETURN_RC(OK);
}
