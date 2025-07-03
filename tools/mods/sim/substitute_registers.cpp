/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2018, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <errno.h>
#include <fstream>
#include <map>
#include <vector>
#include "core/include/massert.h"
#include "substitute_registers.h"
#include "core/include/utility.h"

SubstituteRegisters::SubstituteRegisters(const std::string& file) :
    m_SubstituteRegFileName(file),
    m_SubstituteRegister(true),
    m_Initialized(false)
{
}

RC SubstituteRegisters::RegRd(PHYSADDR Offset, void *Data, UINT32 Count)
{
    RC rc = RC::SOFTWARE_ERROR;

    if (m_SubstituteRegister &&
        InSubstituteRegRange(Offset, Count))
    {
        if (m_SubstitutedAddrMap.count(Offset) > 0)
        {
            auto addressMapIterator = m_SubstitutedAddrMap.find(Offset);
            // Using separate map "m_RegAddrAccessed" to store access index.
            // Increment the index for every access. Stop incrementing index 
            // after it reaches end of vector. Return values in order of index.
            // When the number of read requests exceeds the number of values stored, return the last value.
            if (m_RegAddrAccessed.find(Offset) == m_RegAddrAccessed.end())
            {
                *(UINT32*)Data = addressMapIterator->second.front();
                m_RegAddrAccessed.insert(pair<UINT64, UINT32>(Offset,1));
            }
            else
            {
                auto tempRegAccessData = m_RegAddrAccessed.find(Offset);

                if (tempRegAccessData->second < addressMapIterator->second.size())
                {
                    *(UINT32*)Data = addressMapIterator->second[tempRegAccessData->second];
                    tempRegAccessData->second++;
                }
                else
                {
                    *(UINT32*)Data = addressMapIterator->second[(tempRegAccessData->second) -1];
                }
            }

            Printf(Tee::PriDebug, "%s: Substituting PhysRd(0x%llx) = 0x%x\n",
                __FUNCTION__, Offset, *(UINT32*)Data);
        }
        else
        {
            Printf(Tee::PriError, "Error: Offset(0x%llx) is not recorded"
                " in substituted file %s\n",
                Offset,
                m_SubstituteRegFileName.c_str());
            MASSERT(0);
        }

        return OK;
    }

    return rc;
}

RC SubstituteRegisters::RegWr(PHYSADDR Offset, const void *Data, UINT32 Count)
{
    RC rc = RC::SOFTWARE_ERROR;

    if (m_SubstituteRegister &&
        InSubstituteRegRange(Offset, Count))
    {
        Printf(Tee::PriDebug, "%s: Skipping PhysWr(0x%llx) = 0x%x\n",
            __FUNCTION__, Offset, *(const UINT32*)Data);

        return OK;
    }

    return rc;
}

bool SubstituteRegisters::ParseSubstitutedRegisterFile()
{
    if (m_Initialized)
    {
        return m_SubstituteRegister;
    }

    std::ifstream fs(m_SubstituteRegFileName.c_str());
    if (!fs.is_open())
    {
        Printf(Tee::PriWarn, "WARNING: File \"%s\" doesn't exist. skip substituted\n",
            m_SubstituteRegFileName.c_str());

        return false;
    }

    std::string line;
    UINT32 lineNum = 0;
    while (fs.good())
    {
        getline(fs, line);
        lineNum++;

        std::vector<std::string> tokens;
        Utility::Tokenizer(line, " ", &tokens);
        if (tokens.size() == 0)
            continue;

        char* p = NULL;
        errno = 0;
        if (tokens[0] == "BAR0_BASE")
        {
            MASSERT(tokens.size() == 2);

            Printf(Tee::PriWarn, "WARNING: %s: BAR0_BASE no longer needed!\n", __FUNCTION__);

            Utility::Strtoull(tokens[1].c_str(), &p, 0);
            if ((p == tokens[1].c_str()) ||
                (p && *p != 0) ||
                (ERANGE == errno)) // Catching out of range
            {
                MASSERT(!"Invalid Bar0 base in file\n");
            }
        }
        else if (tokens[0] == "SKIP_RANGE")
        {
            bool validRange = true;
            MASSERT(tokens.size() == 3);

            UINT64 rangeStart = Utility::Strtoull(tokens[1].c_str(), &p, 0);
            if ((p == tokens[1].c_str()) ||
                (p && *p != 0) ||
                (ERANGE == errno)) // Catching out of range
            {
                validRange = false;
                Printf(Tee::PriWarn,
                       "WARNING: invalid skip range lower bound (line=%d): \"%s\".\n",
                       lineNum, line.c_str());
            }

            UINT64 rangeEnd = Utility::Strtoull(tokens[2].c_str(), &p, 0);
            if ((p == tokens[2].c_str()) ||
                (p && *p != 0) ||
                (ERANGE == errno)) // Catching out of range
            {
                validRange = false;
                Printf(Tee::PriWarn,
                       "WARNING: invalid skip range upper bound (line=%d): \"%s\".\n",
                       lineNum, line.c_str());
            }

            if (validRange)
            {
                if (rangeStart < rangeEnd)
                {
                    m_SubstituteRegRangeVectors.push_back(std::make_pair(rangeStart, rangeEnd));
                    Printf(Tee::PriDebug,
                           "%s: skip range (0x%llx, 0x%llx)\n",
                          __FUNCTION__, rangeStart, rangeEnd);
                }
                else
                {
                    Printf(Tee::PriWarn,
                       "WARNING: %s: invalid skip range (line=%d) upper rangeStart (0x%llx) > "
                       "rangeEnd (0x%llx) in skip reg file: \"%s\".\n",
                       __FUNCTION__, lineNum, rangeStart, rangeEnd, line.c_str());
                }
            }
        }
        else if (tokens[0] == "SKIP_REG")
        {
            bool validRegEntry = true;

            UINT64 regOffset = Utility::Strtoull(tokens[1].c_str(), &p, 0);
            if ((p == tokens[1].c_str()) ||
                (p && *p != 0) ||
                (ERANGE == errno)) // Catching out of range
            {
                validRegEntry = false;
                Printf(Tee::PriWarn,
                       "%s: WARNING: invalid register offset (line %d): \"%s\".\n", 
                       __FUNCTION__, lineNum, line.c_str());
            }

            vector<UINT32> regData;
            for (UINT32 dataIter = 2; dataIter < tokens.size(); dataIter++)
            {
                regData.push_back(UNSIGNED_CAST(UINT32, Utility::Strtoull(tokens[dataIter].c_str(), &p, 0)));
                if ((p == tokens[dataIter].c_str()) ||
                    (p && *p != 0) ||
                    (ERANGE == errno)) // Catching out of range
                {
                    validRegEntry = false;
                    Printf(Tee::PriWarn,
                           "%s: WARNING: invalid register value (line %d): \"%s\".\n", 
                           __FUNCTION__, lineNum, line.c_str());
                }
            }

            if (validRegEntry)
            {
                m_SubstitutedAddrMap[regOffset] = regData;
                auto iter = m_SubstitutedAddrMap.find(regOffset);
                if (iter != m_SubstitutedAddrMap.end())
                {
                    for (auto regDataValue : iter->second)
                    {
                        Printf(Tee::PriDebug, "%s: regOffset[0x%llx] = 0x%x\n",
                            __FUNCTION__, regOffset, regDataValue);
                    }
                }
            }
        }
        else if (tokens[0][0] != '#')
        {
            // Do no print warning message for comments
            Printf(Tee::PriWarn,
                "WARNING: unknown entry in skip reg file (line %d): \"%s\".\n",
                lineNum, line.c_str());
        }
    }

    return true;
}

bool SubstituteRegisters::InSubstituteRegRange(PHYSADDR Offset, UINT32 Count)
{
    if (!m_Initialized)
    {
        m_SubstituteRegister = ParseSubstitutedRegisterFile();

        m_Initialized = true;
    }

    if (m_SubstituteRegister && (Count == 4))
    {
        vector< std::pair<UINT64, UINT64> >::iterator iter = m_SubstituteRegRangeVectors.begin();
        for (; iter != m_SubstituteRegRangeVectors.end(); iter++)
        {
            if ((Offset <= iter->second) &&
                (Offset >= iter->first))
            {
                return true;
            }
        }

        if (m_SubstitutedAddrMap.count(Offset) == 1)
        {
            return true;
        }
    }

    return false;
}
