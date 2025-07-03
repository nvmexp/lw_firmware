/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/jsonparser.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"

// RapidJson includes
#include "document.h"
#include "filereadstream.h"
#include "error/en.h"

using namespace rapidjson;

RC JsonParser::ParseJson(const string& filename, Document* pDoc)
{
    // Exit early if file doesn't exist
    const auto fileStatus = Utility::FindPkgFile(filename, nullptr);
    if (fileStatus == Utility::NoSuchFile)
    {
        Printf(Tee::PriWarn, "JSON file %s not found\n", filename.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    RC rc;
    vector<char> JsonBuffer;
    CHECK_RC(Utility::ReadPossiblyEncryptedFile(filename, &JsonBuffer, nullptr));

    string bufferString(JsonBuffer.begin(), JsonBuffer.end());
    bufferString = Utility::RedactString(bufferString);
    JsonBuffer.assign(bufferString.begin(), bufferString.end());

    // This is necessary to avoid RapidJson accessing beyond JsonBuffer's bounds
    JsonBuffer.push_back('\0');

    pDoc->Parse(&JsonBuffer[0]);

    if (pDoc->HasParseError())
    {
        size_t line, column;

        CHECK_RC(Utility::GetLineAndColumnFromFileOffset(
            JsonBuffer, pDoc->GetErrorOffset(), &line, &column));

        Printf(Tee::PriError,
               "JSON syntax error in %s at line %zd column %zd\n",
               filename.c_str(), line, column);
        Printf(Tee::PriError, "%s\n", GetParseError_En(pDoc->GetParseError()));
        return RC::CANNOT_PARSE_FILE;
    }
    return rc;
}

RC JsonParser::RetrieveJsolwal(const JsonTree& tree, const char* valName, UINT32& retVal)
{
    RC rc;

    if (!tree[valName].IsUint())
    {
        Printf(Tee::PriError, "Found wrong type for \"%s\" in JSON file\n", valName);
        return RC::BAD_FORMAT;
    }

    retVal = tree[valName].GetUint();

    return rc;
}

RC JsonParser::RetrieveJsolwal(const JsonTree& tree, const char* valName, INT32& retVal)
{
    RC rc;

    if (!tree[valName].IsInt())
    {
        Printf(Tee::PriError, "Found wrong type for \"%s\" in JSON file\n", valName);
        return RC::BAD_FORMAT;
    }

    retVal = tree[valName].GetInt();

    return rc;
}

RC JsonParser::RetrieveJsolwal(const JsonTree& tree, const char* valName, FLOAT64& retVal)
{
    RC rc;

    if (tree[valName].IsString())
    {
        string str = tree[valName].GetString();
        char* rtn = nullptr;
        const char* temp = str.c_str();
        retVal = strtod(temp, &rtn);

        // if strtod fails, retVal will be 0 and rtn will stay the same
        if (retVal != 0.0 || rtn != temp)
        {
            return rc;
        }
        else
        {
            Printf(Tee::PriError, "Found bad string for \"%s\" in JSON file\n", valName);
            return RC::BAD_FORMAT;
        }
    }
    if (!tree[valName].IsDouble() && !tree[valName].IsUint() && !tree[valName].IsInt())
    {
        Printf(Tee::PriError, "Found wrong type for \"%s\" in JSON file\n", valName);
        return RC::BAD_FORMAT;
    }

    retVal = tree[valName].GetDouble();

    return rc;
}

RC JsonParser::RetrieveJsolwal(const JsonTree& tree, const char* valName, string& retVal)
{
    RC rc;

    if (!tree[valName].IsString())
    {
        Printf(Tee::PriError, "Found wrong type for \"%s\" in JSON file\n", valName);
        return RC::BAD_FORMAT;
    }

    retVal = tree[valName].GetString();

    return rc;
}

// Find a string named valName and colwert it to a number
RC JsonParser::FindJsonUintFromString
(
    const JsonTree& tree,
    const string& valName,
    UINT32* pRetVal
)
{
    RC rc;
    string valStr;
    CHECK_RC(FindJsolwal(tree, valName, valStr));
    *pRetVal = strtoul(valStr.c_str(), nullptr, 0);
    return rc;
}

// Try to find a string value named valName and colwert it to a number,
// Return 0 if it doesn't exist
RC JsonParser::TryFindJsonUintFromString
(
    const JsonTree& tree,
    const string& valName,
    UINT32* pRetVal
)
{
    RC rc;
    string valStr;
    CHECK_RC(TryFindJsolwal(tree, valName, &valStr));
    *pRetVal = strtoul(valStr.c_str(), nullptr, 0);
    return rc;
}

RC JsonParser::TryFindJsonUintFromString
(
    const JsonTree& tree,
    const string& valName,
    UINT32 defVal,
    UINT32* pRetVal
)
{
    RC rc;
    if (!tree.HasMember(valName.c_str()))
    {
        *pRetVal = defVal;
    }
    else
    {
        CHECK_RC(TryFindJsonUintFromString(tree, valName, pRetVal));
    }
    
    return rc;
}

RC JsonParser::CheckHasSection(const JsonTree& tree, const char* sectionName)
{
    RC rc;

    if (!tree.HasMember(sectionName))
    {
        Printf(Tee::PriError, "Could not find section \"%s\" in JSON file\n",
                               sectionName);
        return RC::CANNOT_PARSE_FILE;
    }

    return rc;
}

RC JsonParser::CheckHasSection(const Document& doc, const char* sectionName)
{
    RC rc;

    if (!doc.HasMember(sectionName))
    {
        Printf(Tee::PriError, "Could not find section \"%s\" in JSON file\n",
                               sectionName);
        return RC::CANNOT_PARSE_FILE;
    }

    return rc;
}
