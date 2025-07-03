/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "repair_util.h"

#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/xp.h"

#include <fstream>
#include <regex>
#include <sstream>

using namespace Memory;
using namespace Repair;

namespace
{
    //!
    //! \brief Gets the HwFbpa property from a JS object by looking for either
    //! "HwFbpa" or the mislabelled "HwFbio".
    //!
    RC GetJsPropertyHwFbpa(JSObject* pJso, HwFbpa* pHwFbpa)
    {
        MASSERT(pJso);
        MASSERT(pHwFbpa);
        RC rc;

        // NOTE: There is a misnomer, "HwFbio" is actually HwFbpa.
        // Accept both 'HwFbio' and 'HwFbpa' to mean the same thing.
        UINT32 hwFbpaVal, hwFbpaMisnomerVal;
        RC hwFbpaRc = GetJsPropertyValue(pJso, "HwFpba", &hwFbpaVal, false);
        RC hwFbpaMisnomerRc = GetJsPropertyValue(pJso, "HwFbio", &hwFbpaMisnomerVal, false);

        if (hwFbpaMisnomerRc == RC::OK && hwFbpaRc == RC::OK)
        {
            if (hwFbpaMisnomerVal != hwFbpaVal)
            {
                Printf(Tee::PriError, "Both 'HwFbio' and 'HwFbpa' fields are "
                       "present but their values do not match\n");
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (hwFbpaMisnomerRc == RC::OK)
        {
            hwFbpaVal = hwFbpaMisnomerVal;
        }
        else if (hwFbpaRc != RC::OK)
        {
            Printf(Tee::PriError, "Unable to get property 'HwFbio' (or it's "
                   "correctly named alternative 'HwFbpa')\n");
            return hwFbpaMisnomerRc;
        }

        *pHwFbpa = HwFbpa(hwFbpaVal);
        return rc;
    }
}

//!
//! \brief Parse the given string and return the FBPA lane it represents.
//!
//! \param s String to colwert. Assumed to be trimmed.
//!
//! \return Return true if the lane was extracted, false o/w
//!
bool Repair::GetLaneFromString(const string& s, FbpaLane* pLane)
{
    MASSERT(pLane);

    const string strToMatch = Utility::ToUpperCase(s);

    const std::regex re("^([A-Z])([0-9]+)_(DATA|DQ|DBI|DM|ECC|ADDR|CMD)$");
    std::smatch match;
    if (!regex_match(strToMatch, match, re))
    {
        return false;
    }

    const string& fbpaLetter  = match[1];
    const string& fbpaLane    = match[2];
    const string& laneTypeStr = match[3];

    *pLane = FbpaLane(HwFbpa::FromLetter(fbpaLetter[0]),
                      static_cast<UINT32>(std::atoi(fbpaLane.c_str())),
                      Lane::GetLaneTypeFromString(laneTypeStr));

    MASSERT(pLane->hwFbpa.Letter() == fbpaLetter[0]
            && "FBPA letter does not match expected value");

    return true;
}

//!
//! \brief Parse the given file content and extract the lane data.
//!
//! \param fileContent File content. Assumed to be JavaScript.
//! \param[out] pLanes Extracted lanes.
//!
RC Repair::GetLanesFromFileFormat(const string& fileContent, vector<FbpaLane>* pLanes)
{
    MASSERT(pLanes);

    /*
     * File format:
     *
     * // Repeat for each lane with unique associative array key.
     * g_FailingLanes[<str>] = {
     *     "LaneName": <str>,
     *     "HwFbio": <int>,
     *     "Subpartition": <int>
     *     "LaneBit": <int>
     *     "RepairType": <str>
     * };
     */

    RC rc;
    JavaScriptPtr pJs;
    JSObject* pJsGlobalObj = pJs->GetGlobalObject();

    CHECK_RC(pJs->Eval(fileContent));

    // NOTE: A JS associative array is represented as an object where the
    // keys are properties associated with the value.
    JSObject* pFailingLanes;
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_FailingLanes", &pFailingLanes));
    vector<string> keys;
    CHECK_RC(pJs->GetProperties(pFailingLanes, &keys));

    for (const string& key : keys)
    {
        // Get lane object
        //
        JSObject* pFailingLane;
        CHECK_RC(pJs->GetProperty(pFailingLanes, key.c_str(), &pFailingLane));

        // Extract the properties from the lane object
        //
        StickyRC firstRc;

        HwFbpa hwFbpa;
        firstRc = GetJsPropertyHwFbpa(pFailingLane, &hwFbpa);
        UINT32 laneBit;
        firstRc = GetJsPropertyValue(pFailingLane, "LaneBit", &laneBit);
        string repairType;
        firstRc = GetJsPropertyValue(pFailingLane, "RepairType", &repairType);

        if (firstRc != RC::OK)
        {
            Printf(Tee::PriError, "Failed to extract lane with key '%s'", key.c_str());
            return firstRc;
        }

        // Record the lane
        //
        pLanes->emplace_back(hwFbpa, laneBit, Lane::GetLaneTypeFromString(repairType));
    }

    return rc;
}

//!
//! \brief Return lanes extracted from the given lane description.
//!
//! \param laneDesc Either a path to a file that contains a FBPA lanes
//! descriptions, or a string that describes a single FBPA lane.
//! \param[out] pLanes Extracted FBPA lanes.
//!
RC Repair::GetLanes(const string& laneDesc, vector<FbpaLane>* pLanes)
{
    MASSERT(pLanes);
    RC rc;

    if (Xp::DoesFileExist(laneDesc))
    {
        // Have a file containing lanes
        std::ifstream file;
        file.open(laneDesc);
        if (file.fail())
        {
            Printf(Tee::PriError, "Unable to open file: '%s'\n", laneDesc.c_str());
            return RC::CANNOT_OPEN_FILE;
        }

        std::stringstream fileContent;
        fileContent << file.rdbuf();
        if (file.fail())
        {
            Printf(Tee::PriError, "Unable to read file: '%s'\n", laneDesc.c_str());
            return RC::FILE_READ_ERROR;
        }

        CHECK_RC(GetLanesFromFileFormat(fileContent.str(), pLanes));
     }
    else
    {
        // Have a single lane description
        FbpaLane lane;
        if (GetLaneFromString(laneDesc, &lane))
        {
            pLanes->push_back(lane);
        }
        else
        {
            Printf(Tee::PriError, "Unable to interpret lane description: '%s'\n", laneDesc.c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return rc;
}

//!
//! \brief Parse the given file content and extract the row data.
//!
//! \param fileContent File contents. Assumed to be JavaScript.
//!
RC Repair::GetRowErrorsFromFileFormat(const string& fileContent, vector<RowError>* pRowErrors)
{
    MASSERT(pRowErrors);

    /*
     * File format:
     *
     * // Repeat for each page with unique associative array key.
     * g_FailingPages["ECC=0xd37 NECC=0xd37"] = {
     *     "EccLow": 3383,
     *     "EccHigh": 0,
     *     "NonEccLow": 3383,
     *     "NonEccHigh": 0,
     *     "FailingRows": {
     *         // One entry for each failing row within the page.
     *         "1-0-0-0-10-3": {
     *             "HwFbp": 1,
     *             "HwFbio": 0,
     *             "Subpartition": 0,
     *             "Rank": 0,
     *             "Bank": 10,
     *             "Row": 3,
     *             "OriginTestId": 242 // Optional for backwards compatibility
     *         }
     *     }
     * };
     */

    RC rc;
    JavaScriptPtr pJs;
    JSObject* pJsGlobalObj = pJs->GetGlobalObject();

    CHECK_RC(pJs->Eval(fileContent));

    // NOTE: A JS associative array is represented as an object where the
    // keys are properties associated with the value.
    JSObject* pFailingPages;
    CHECK_RC(pJs->GetProperty(pJsGlobalObj, "g_FailingPages", &pFailingPages));
    vector<string> pageKeys;
    CHECK_RC(pJs->GetProperties(pFailingPages, &pageKeys));

    for (const string& pageKey : pageKeys)
    {
        // Get page object
        //
        JSObject* pFailingPage;
        CHECK_RC(pJs->GetProperty(pFailingPages, pageKey.c_str(), &pFailingPage));

        JSObject* pFailingRows;
        CHECK_RC(pJs->GetProperty(pFailingPage, "FailingRows", &pFailingRows));
        vector<string> rowKeys;
        CHECK_RC(pJs->GetProperties(pFailingRows, &rowKeys));

        for (const string& rowKey : rowKeys)
        {
            // Extract the properties from the row object
            //
            StickyRC firstRc;

            JSObject* pFailingRow;
            CHECK_RC(pJs->GetProperty(pFailingRows, rowKey.c_str(), &pFailingRow));

            HwFbpa hwFbpa;
            firstRc = GetJsPropertyHwFbpa(pFailingRow, &hwFbpa);
            UINT32 subp;
            firstRc = GetJsPropertyValue(pFailingRow, "Subpartition", &subp);
            UINT32 rank;
            firstRc = GetJsPropertyValue(pFailingRow, "Rank", &rank);
            UINT32 bank;
            firstRc = GetJsPropertyValue(pFailingRow, "Bank", &bank);
            UINT32 row;
            firstRc = GetJsPropertyValue(pFailingRow, "Row", &row);

            INT32 originTestId;
            rc = GetJsPropertyValue(pFailingRow, "OriginTestId", &originTestId);
            if (rc != RC::OK)
            {
                // OriginTestId field not present, record as unknown origin test
                originTestId = s_UnknownTestId;
                rc.Clear();
            }

            UINT32 pc;
            rc = GetJsPropertyValue(pFailingRow, "PseudoChannel", &pc);
            if (rc != RC::OK)
            {
                // PseudoChannel field not present, record as unknown pc
                pc = s_UnknownPseudoChannel;
                rc.Clear();
            }

            if (firstRc != RC::OK)
            {
                Printf(Tee::PriError, "Failed to extract row with key '%s' in page with key '%s'",
                       rowKey.c_str(), pageKey.c_str());
                return firstRc;
            }

            // Record the row
            //
            RowError rowError = {};
            rowError.row = Row(rowKey, hwFbpa, FbpaSubp(subp), PseudoChannel(pc),
                               Rank(rank), Bank(bank), row);
            rowError.originTestId = originTestId;
            pRowErrors->push_back(rowError);
        }
    }

    return rc;
}

//!
//! \brief Return rows extracted from the given row description.
//!
//! \param rowDesc A path to a file that contains row descriptions.
//! \param[out] pRows Extracted rows.
//!
RC Repair::GetRowErrors(const string& rowDesc, vector<RowError>* pRowErrors)
{
    MASSERT(pRowErrors);
    RC rc;

    if (Xp::DoesFileExist(rowDesc))
    {
        // Have a file containing lanes
        std::ifstream file;
        file.open(rowDesc);
        if (file.fail())
        {
            Printf(Tee::PriError, "Unable to open file: '%s'\n", rowDesc.c_str());
            return RC::CANNOT_OPEN_FILE;
        }

        std::stringstream fileContent;
        fileContent << file.rdbuf();
        if (file.fail())
        {
            Printf(Tee::PriError, "Unable to read file: '%s'\n", rowDesc.c_str());
            return RC::FILE_READ_ERROR;
        }

        CHECK_RC(GetRowErrorsFromFileFormat(fileContent.str(), pRowErrors));
    }

    return rc;
}

//!
//! \brief Create supported HBM models that are vendor independent. Only depends
//! on the HBM specification version.
//!
//! \param specs HBM specifications to support.
//!
Hbm::SupportedHbmModels
Repair::VendorIndepSupportedHbmModels
(
    std::initializer_list<Hbm::SpecVersion> specs
)
{
    return Hbm::SupportedHbmModels(specs, {Hbm::Vendor::All}, {Hbm::Die::All},
                                   {Hbm::StackHeight::All},
                                   {Hbm::Revision::All});
}

bool Repair::CmpRowErrorByRow(const RowError& a, const RowError& b)
{
    return a.row < b.row;
};

bool Repair::IsEqualRowErrorByRow(const RowError& a, const RowError& b)
{
    return a.row == b.row;
};

void Repair::Write32
(
    const Settings &settings,
    RegHal &regs,
    ModsGpuRegAddress address,
    UINT32 regIndex,
    UINT32 value
)
{
    if (settings.modifiers.printRegSeq)
    {
        Printf(Tee::PriNormal, "Repair RegWrite32(0x%x, 0x%x)\n",
               regs.LookupAddress(address, regIndex), value);
    }
    else
    {
        Printf(Tee::PriDebug, "Repair RegWrite32(0x%x, 0x%x)\n",
               regs.LookupAddress(address, regIndex), value);
        regs.Write32(address, regIndex, value);
    }
}

void Repair::Write32
(
    const Settings &settings,
    RegHal &regs,
    ModsGpuRegAddress address,
    UINT32 value
)
{
    if (settings.modifiers.printRegSeq)
    {
        Printf(Tee::PriNormal, "Repair RegWrite32(0x%x, 0x%x)\n",
               regs.LookupAddress(address), value);
    }
    else
    {
        Printf(Tee::PriDebug, "Repair RegWrite32(0x%x, 0x%x)\n",
               regs.LookupAddress(address), value);
        regs.Write32(address, value);
    }
}

void Repair::Write32
(
    const Settings &settings,
    RegHal &regs,
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT32 value
)
{
    UINT32 regVal = 0;
    regs.SetField(&regVal, field, regIndex, value);

    Repair::Write32(settings,
                    regs,
                    regs.ColwertToAddress(field),
                    regIndex,
                    regVal);
}

void Repair::Write32
(
    const Settings &settings,
    RegHal &regs,
    ModsGpuRegField field,
    UINT32 value
)
{
    UINT32 regVal = 0;
    regs.SetField(&regVal, field, value);

    Repair::Write32(settings,
                    regs,
                    regs.ColwertToAddress(field),
                    regVal);
}

void Repair::Write32
(
    const Settings &settings,
    RegHal &regs,
    ModsGpuRegValue value,
    UINT32 regIndex
)
{
    Repair::Write32(settings,
                    regs,
                    regs.ColwertToField(value),
                    regIndex,
                    regs.LookupValue(value));
}

void Repair::Write32
(
    const Settings &settings,
    RegHal &regs,
    ModsGpuRegValue value
)
{
    Repair::Write32(settings,
                    regs,
                    regs.ColwertToField(value),
                    regs.LookupValue(value));
}

UINT32 Repair::Read32
(
    const Settings &settings,
    RegHal &regs,
    ModsGpuRegAddress address,
    UINT32 regIndex
)
{
    UINT32 regVal = 0;

    if (settings.modifiers.printRegSeq)
    {
        Printf(Tee::PriNormal, "Repair RegRead32(0x%x, 0x%x)\n",
               regs.LookupAddress(address, regIndex), regVal);
    }
    else
    {
        Printf(Tee::PriDebug, "Repair RegRead32(0x%x, 0x%x)\n",
               regs.LookupAddress(address, regIndex), regVal);
        regVal = regs.Read32(address, regIndex);
    }

    return regVal;
}
