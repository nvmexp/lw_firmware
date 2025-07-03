/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/xp.h"
#include "core/include/utility.h"
#include <stdlib.h>
#include <string.h>

RC Xp::InteractiveFile::Read(double* pValue)
{
    MASSERT(pValue);

    string value;
    RC rc;
    CHECK_RC(Read(&value));
    if (value.empty())
    {
        Printf(Tee::PriError, "Empty string returned\n");
        return RC::ILWALID_INPUT;
    }

    char* endptr = nullptr;
    const double lval = strtod(value.c_str(), &endptr);

    //Note that strtod will return HUGE_VAL or -HUGE_VAL if string value representation
    //is out of range
    if (!endptr || (*endptr != 0 && *endptr != '\n' && *endptr != ' ') ||
        lval < numeric_limits<double>::lowest() || lval > numeric_limits<double>::max() ||
        lval == HUGE_VAL || lval == -HUGE_VAL)
    {
        Printf(Tee::PriError, "Value \"%s\" cannot be colwerted to a double\n",
                            value.c_str());
        return RC::ILWALID_INPUT;
    }

    *pValue = static_cast<double> (lval);
    return OK;
}

RC Xp::InteractiveFile::Write(double value)
{
    return Write(Utility::ToString(value));
}

RC Xp::InteractiveFile::ReadBinary(vector<UINT08>* pValue)
{
    MASSERT(pValue);
    RC rc;

    string file;
    CHECK_RC(Read(&file));

    for (char c : file)
    {
        pValue->push_back(static_cast<UINT08>(c));
    }

    return RC::OK;
}

RC Xp::InteractiveFile::Read(int* pValue)
{
    MASSERT(pValue);

    string value;
    RC rc;
    CHECK_RC(Read(&value));
    if (value.empty())
    {
        Printf(Tee::PriError, "Empty string returned\n");
        return RC::ILWALID_INPUT;
    }

    char* endptr = nullptr;
    const long lval = strtol(value.c_str(), &endptr, 10);

    if (!endptr || (*endptr != 0 && *endptr != '\n' && *endptr != ' ') ||
        lval < numeric_limits<int>::min() || lval > numeric_limits<int>::max() ||
        lval == LONG_MIN || lval == LONG_MAX)
    {
        Printf(Tee::PriError, "Value \"%s\" cannot be colwerted to an int\n",
                            value.c_str());
        return RC::ILWALID_INPUT;
    }

    *pValue = static_cast<int>(lval);
    return OK;
}

RC Xp::InteractiveFile::ReadHex(int* pValue)
{
    MASSERT(pValue);

    string value;
    RC rc;
    CHECK_RC(Read(&value));
    if (value.empty())
    {
        Printf(Tee::PriError, "Empty string returned\n");
        return RC::FILE_READ_ERROR;
    }

    const bool negative = !value.empty() && value[0] == '-';
    const string absValue = negative ? value.substr(1) : value;
    const UINT64 uval = Utility::Strtoull(absValue.c_str(), nullptr, 16);

    // FYI: max unsigned long long is returned on error.
    // Detect if the value will fit in int in general.
    if (uval > 0x7FFFFFFFU)
    {
        Printf(Tee::PriError, "Value \"%s\" cannot be colwerted to an int\n",
                            value.c_str());
        return RC::SOFTWARE_ERROR;
    }

    *pValue = (negative ? -1 : 1) * static_cast<int>(uval);
    return OK;
}

RC Xp::InteractiveFile::Write(int value)
{
    return Write(Utility::ToString(value));
}

RC Xp::InteractiveFile::Read(UINT64* pValue)
{
    MASSERT(pValue);

    string value;
    RC rc;
    CHECK_RC(Read(&value));
    if (value.empty())
    {
        Printf(Tee::PriError, "Empty string returned\n");
        return RC::ILWALID_INPUT;
    }

    if (value[0] == '-')
    {
        Printf(Tee::PriError, "Value \"%s\" cannot be colwerted to an unsigned integer\n",
                            value.c_str());
        return RC::ILWALID_INPUT;
    }

    const UINT64 uval = Utility::Strtoull(value.c_str(), nullptr, 10);

    // Max unsigned long long is returned on error.
    if (uval == numeric_limits<UINT64>::max())
    {
        Printf(Tee::PriError, "Value \"%s\" cannot be colwerted to an unsigned integer\n",
                            value.c_str());
        return RC::ILWALID_INPUT;
    }

    *pValue = uval;
    return OK;
}

RC Xp::InteractiveFile::Write(UINT64 value)
{
    return Write(Utility::ToString(value));
}

RC Xp::InteractiveFile::Write(const char* value)
{
    MASSERT(value);
    return Write(value, strlen(value));
}
