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

#pragma once

#ifndef INCLUDED_XMLPARSETEST_H
#define INCLUDED_XMLPARSETEST_H

#include "core/include/fusexml_parser.h"

class XmlParseTest
{
    public:
        static XmlParseTest& GetInstance() { return s_Instance; }

        RC TestParseXml(string chipName);
        RC VerifyXml(string chipName);
        RC VerifyFullXml(string chipName);
        RC ExportGoldensForChip(string chipName);

        void SetXmlFilename(string filename);

    private:
        static XmlParseTest s_Instance;

        string m_GldFilename;

        string m_XmlFilenameOverride;

        XmlParseTest();

        string GetXmlFilename(string chipName);

        void CategorizeFuses
        (
            const map<string, FuseUtil::FuseDef>& xmlFuses,
            map<string, FuseUtil::FuseDef> expectedFuses,
            vector<string>* pAddedFuseNames,
            vector<string>* pChangedFuseNames,
            vector<string>* pRemovedFuseNames,
            vector<string>* pFlaggedFuses
        );

        void CheckFlaggedFusesForOverlap
        (
            const vector<string>& pFlaggedFuses,
            const map<string, FuseUtil::FuseDef>& pXmlFuses,
            vector<pair<string, string> >* pOverlappingFuses
        );

        bool CheckFuseDefsOverlap
        (
            const FuseUtil::FuseDef& lhs,
            const FuseUtil::FuseDef& rhs
        );

        bool CheckRawFusesOverlap
        (
            const FuseUtil::FuseLoc& lhs,
            const FuseUtil::FuseLoc& rhs
        );

        bool CheckFuselessFusesOverlap
        (
            const FuseUtil::FuseLoc& lhs,
            const FuseUtil::FuseLoc& rhs
        );

        bool SigBitsOverlap(UINT32 lLsb, UINT32 lMsb, UINT32 rLsb, UINT32 rMsb);

        RC HandleFlaggedFuseResults
        (
            const vector<string>& pAddedFuseNames,
            const vector<string>& pRemovedFuseNames,
            const vector<string>& pChangedFuseNames
        );

        RC HandleOverlappingFuseResults(const vector<pair<string, string> >& pOverlappingFuses);

        RC ParseXml
        (
            string chipName,
            map<string, FuseUtil::FuseDef>* pXmlFuses
        );
};

#endif
