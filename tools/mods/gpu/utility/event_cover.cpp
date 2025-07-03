
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "event_cover.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include <algorithm>
#include <ctype.h>

#include <boost/algorithm/string.hpp>

const string g_OnlyCheckChip("ONLY_CHECK_CHIP:");

static EventCover::Status ParseParaNameAndValue(const string& paraStr,
                                                string& paraName,
                                                vector<string>& paraValues);

EventCover::EventCover(const string& testChipName, const string& fileName)
    :m_LineNum(0), m_FileName(fileName), m_TestChipName(testChipName), m_MatchesTestChip(false),
     m_EventIlw(false), m_CounterCheck(false) {};
EventCover::~EventCover() {};

//
// Typical format: chain-link[,substr...] [!] event-name [sub-event ...]
// Two format supported:
// 1. <ClassName>:<ClassName>:...,<ParamList> ...
// 2. ONLY_CHECK_CHIP:<ChipName>,...
EventCover::Status EventCover::ParseOneLine(const string& lwr_line, int line_num)
{
    if (lwr_line.empty())
    {
        return EventCover::Status::False;
    }

    m_EventAssert = lwr_line;

    string::size_type last_char = m_EventAssert.size() - 1;
    if (m_EventAssert[last_char] == '\n')
    {
        m_EventAssert.erase(last_char, 1);
    }

    Tokenizer ec_tokenizer(lwr_line, SPACE_DELIMITER, "\"");
    string lwr_tok;
    EventCover::Status tokenStatus;
    if ((tokenStatus = ec_tokenizer.GetNextToken(lwr_tok)) == EventCover::Status::Error)
    {
        return EventCover::Status::Error;
    }
    else if (tokenStatus == EventCover::Status::False || (lwr_tok[0] == '#'))
    {
        // skip if empty or comment line
        return EventCover::Status::False;
    }

    string classNameStr;
    string::size_type para_pos = lwr_tok.find(",");
    if (para_pos != string::npos)
    {
        m_ParamList = lwr_tok.substr(para_pos+1);
        classNameStr = lwr_tok.substr(0, para_pos);
        Tokenizer::StripQuotes(classNameStr);
    }
    else
    {
        classNameStr = lwr_tok;  // no paramater list
    }

    if (classNameStr.find(g_OnlyCheckChip) == 0)
    {
        classNameStr = classNameStr.substr(g_OnlyCheckChip.length());
        if (!m_TestChipName.empty() && (classNameStr != m_TestChipName))
        {
            Printf(Tee::PriDebug, "Chip %s is not current test chip %s, skipped\n", classNameStr.c_str(), m_TestChipName.c_str());
            return EventCover::Status::False;
        }
        else
        {
            m_MatchesTestChip = true;
        }
    }

    RC rc = Utility::Tokenizer(classNameStr, ":", &m_ClassNames);
    if (OK != rc)
    {
        Printf(Tee::PriError, "Parsing ecov file (line %d): failed to parse class names for %s", line_num, lwr_line.c_str());
        return EventCover::Status::Error;
    }

    m_LineNum = line_num;

    if (ec_tokenizer.GetNextToken(lwr_tok) != EventCover::Status::True)
    {
        Printf(Tee::PriError, "Parsing ecov file (line %d): missing event (%s)",
            line_num, lwr_line.c_str() );
        return EventCover::Status::Error;
    }

    if (lwr_tok[0] == '!')
    {
        m_EventIlw = true;
        lwr_tok.erase(0, 1);
        if (lwr_tok.empty() && ec_tokenizer.GetNextToken(lwr_tok) != EventCover::Status::True)
        {
            Printf(Tee::PriError, "Parsing ecov file (line %d): missing event (%s)",
                line_num, lwr_line.c_str() );
            return EventCover::Status::Error;
        }
    }
    m_EventName = Tokenizer::StripQuotes(lwr_tok);

    // Get all the sub events
    while ((tokenStatus = ec_tokenizer.GetNextToken(lwr_tok)) == EventCover::Status::True)
    {
        if (lwr_tok[0] == '=')
        {
            if (m_SubEvents.empty())
            {
                break;
            }

            lwr_tok.erase(0, 1);
            if (!lwr_tok.empty())
            {
                m_CounterCheck = true;
            }
            else
            {
                continue;
            }
        }

        m_SubEvents.push_back(lwr_tok);
    }

    if (tokenStatus == EventCover::Status::Error)
    {
        return EventCover::Status::Error;
    }

    return EventCover::Status::True;
}

//!-----------------------------------------------------------------------------
//! \brief Parse a param string into a param name and vector of values.
//!
//! The following formats are supported:
//!   * param_name
//!   * param_name=value1
//!   * param_name="value1 value2 [...]"
//!
//! \param paraStr    : Input parameter string.
//! \param paraName   : Output parsed parameter name.
//! \param paraValues : Output parsed vector of parameter values. This may
//!                     contain zero or more elements.
//!
//! \return true if the string could be parsed, false otherwise.
static EventCover::Status ParseParaNameAndValue
(
    const string& paraStr,
    string& paraName,
    vector<string>& paraValues
)
{
    MASSERT(paraName.empty());
    MASSERT(paraValues.empty());

    if (paraStr.empty())
    {
        return EventCover::Status::False;
    }

    string lwrPara(paraStr);
    const string::size_type valuePos = lwrPara.find("=");
    if (valuePos < string::npos)
    {
        paraName = lwrPara.substr(0, valuePos);
        lwrPara.erase(0, valuePos+1);
    }
    else
    {
        // No "=" found. Assume the whole string is the parameter name.
        paraName = paraStr;
        return EventCover::Status::True;
    }

    Tokenizer::StripQuotes(lwrPara);

    // Tokenize the param values without any escape string. Nested quotes are
    // not expected here.
    Tokenizer paraValTokenizer(lwrPara, SPACE_DELIMITER, "");
    string nextValue;

    EventCover::Status tokenStatus;
    while ((tokenStatus = paraValTokenizer.GetNextToken(nextValue)) == EventCover::Status::True)
    {
        paraValues.push_back(nextValue);
    }

    if (tokenStatus == EventCover::Status::Error)
    {
        return EventCover::Status::Error;
    }

    return EventCover::Status::True;
}

//!-----------------------------------------------------------------------------
//! \brief Check for matching conditions for this event
//!
//! This function is for determining whether this event will be used for this
//! test.
//!
//! \param tparams  : Test parameters
//! \param rparams  : GPU Resource parameters
//! \param jparams  : JavaScript parameters
//! \param crcChain : Crc chain
//! \param classNameIgnoreList : Class names ignore list
//!
//! \return true
//! if the chip or any of class names associated with the event is in the crc chain and
//! the specified parameters match those used by the test.
//! or all class names are ignored and parameters matches those by the test.
//! \return false otherwise.
EventCover::Status EventCover::MatchCondition
(
    const ArgReader *tparams,
    const ArgReader *rparams,
    const ArgReader *jparams,
    const CrcChain &crcChain,
    const vector<string>& classNameIgnoreList
) const
{
    if (!m_MatchesTestChip)
    {
        // Some classes (eg compute) does not have class chain. As a
        // workaroud, any class without assoicated chain will always
        // get matched.
        if (crcChain.empty())
        {
            Printf(Tee::PriAlways, "EventCover: Class %s has no associated CRCchain. Skip matching CRCChain.\n",
                boost::algorithm::join(m_ClassNames, ":").c_str());
        }
        else
        {
            Status status = EventCover::Status::True;
            for (auto & className : m_ClassNames)
            {
                if (classNameIgnoreList.end() != find(classNameIgnoreList.begin(), classNameIgnoreList.end(), className))
                {
                    // Not override status if one of previous classes hit CRCchain mismatch.
                    Printf(Tee::PriDebug, "EventCover: Class name %s is ignored due to -ignore_ecov_tags.\n", className.c_str());
                    status = (status == EventCover::Status::Error) ? status : EventCover::Status::False;
                }
                else if (crcChain.end() != find(crcChain.begin(), crcChain.end(), className))
                {
                    // CRCchain matching passes as long as one of classes matches current crc chain.
                    Printf(Tee::PriDebug, "EventCover: Class name %s matches the CRCchain\n", className.c_str());
                    status = EventCover::Status::True;
                    break;
                }
                else
                {
                    // CRCchain matching would fail if none of rest classes matches current crc chain.
                    Printf(Tee::PriDebug, "EventCover: Class %s is not in the CRCchain\n", className.c_str());
                    status = EventCover::Status::Error;
                }
            }

            if (status == EventCover::Status::Error)
            {
                Printf(Tee::PriError, "EventCover: None of class %s matches current CRCchain.\n",
                    boost::algorithm::join(m_ClassNames, ":").c_str()); 
                return EventCover::Status::Error;
            }
            else if (status == EventCover::Status::False)
            {
                Printf(Tee::PriAlways, "EventCover: All classes %s are ignored.\n",
                    boost::algorithm::join(m_ClassNames, ":").c_str());
                return EventCover::Status::False;
            }
            else
            {
                Printf(Tee::PriDebug, "EventCover: one of classes %s passes current CRCchain matching.\n",
                    boost::algorithm::join(m_ClassNames, ":").c_str());
            }
        }
    }

    // Now Match the parameters
    if (m_ParamList.empty())
    {
        return EventCover::Status::True;
    }

    Tokenizer paraTokenizer(m_ParamList, ",", "\"");
    string nextPara;

    EventCover::Status tokenStatus;
    while ((tokenStatus = paraTokenizer.GetNextToken(nextPara)) == EventCover::Status::True)
    {
        int ilwPara = 0;
        if (nextPara[0] == '!')
        {
            ilwPara = 1;
            nextPara.erase(0, 1);
        }

        string paraName;
        vector<string> paraValues;
        EventCover::Status status;
        if ((status = ParseParaNameAndValue(nextPara, paraName, paraValues)) !=
            EventCover::Status::True)
        {
            return status;
        }

        int isMatched = tparams->MatchParam(paraName, paraValues);
        if (isMatched == -1)  // also try resource parameters
        {
            if (rparams != nullptr) //for JS test, rparams = NULL
                isMatched = rparams->MatchParam(paraName, paraValues);
            if (isMatched == -1)
            {
                if (jparams != nullptr)
                    isMatched = jparams->MatchParam(paraName, paraValues);
                if (isMatched == -1)
                {
                    Printf(Tee::PriError, "Unknown parameter %s\n", nextPara.c_str());
                    return EventCover::Status::Error;
                }
            }
        }

        if (!(isMatched ^ ilwPara)) // failed to match
        {
            Printf(Tee::PriError, "Parameter %s%s not match\n", ilwPara?"!":"",
                nextPara.c_str());
            return EventCover::Status::False;
        }
    }

    if (tokenStatus == EventCover::Status::Error)
    {
        return EventCover::Status::Error;
    }

    return EventCover::Status::True;
}

bool EventCover::MatchEvent(const SimEventListType& sim_events) const
{
    const auto sim_iter = sim_events.find(m_EventName);

    if (sim_iter == sim_events.end())
    {
        Printf(Tee::PriError, "Failed to find event \"%s\"\n", m_EventName.c_str());
        return false;
    }
    else if (!sim_iter->second->GetSubEventVecDim())
    {
        // No sub-events checking
        bool e_matched = false;
        if (!m_SubEvents.empty())
        {
            Printf(Tee::PriError, "Event %s has no subevent!\n", m_EventName.c_str());
        }
        else if (m_EventIlw)
        {
            e_matched = (sim_iter == sim_events.end()) ||
                (sim_iter->second->GetEventCount() == 0);
        }
        else
        {
            e_matched = (sim_iter != sim_events.end()) &&
                (sim_iter->second->GetEventCount());
        }
        Printf(Tee::PriDebug, "(%d) Event %s'%s': %s\n", m_LineNum, m_EventIlw?"!":"",
            m_EventName.c_str(), e_matched?"true":"false");
        return e_matched;
    }

    return (sim_iter->second->MatchSubEvents(m_SubEvents, m_EventName,
        m_LineNum, m_EventIlw, m_CounterCheck));
}

const string& Tokenizer::StripQuotes(string& str)
{
    if ((str[0] == '"') && (*(str.rbegin()) == '"'))
    {
        str.erase(0, 1);
        str.erase(str.size()-1, 1);
    }
    return str;
}

SimEvent::SimEvent(const string& name, int count): m_EventName(name),
                                                   m_EventCount(count) {};

SimEvent::~SimEvent()
{
    m_SubEventCounts.clear();
    for (auto subEventDim : m_SubEventDims)
        delete subEventDim;

    m_SubEventDims.clear();
}

int SimEvent::GetSubEventCount(const SubEvtAddrType& idx) const
{
    int idxnum = GetSubEvtIdx(idx);

    if (idxnum >= 0)
    {
        return m_SubEventCounts[idxnum];
    }

    // not found
    return -1;
}

// Callwlate the offset for the given vector
int SimEvent::GetSubEvtIdx(const SubEvtAddrType& idx) const
{
    if (idx.size() != m_SubEventDims.size())
    {
        return -1;
    }

    size_t offset = 0;
    size_t prev_dim = 1;
    for (size_t i=0; i<idx.size(); ++i)
    {
        if (idx[i] < 0)
            return -1;

        offset += (idx[i]*prev_dim);
        prev_dim *= (GetSubEventDimSize( i ) + 1);
    }

    return int(offset);
}

void SimEvent::SetSubEventCount(int val, const SubEvtAddrType& idx)
{
    int idxnum = GetSubEvtIdx(idx);

    if (idxnum >= 0)
        m_SubEventCounts[idxnum] = val;
}

bool SimEvent::MatchSubEvents
(
    const vector<string>& subevt,
    const string& evt_name,
    int line_num,
    int ilw_evt,
    bool check_count
) const
{
    const size_t subevt_dim = GetSubEventVecDim();
    SubEvtAddrType subevt_vec(subevt_dim, ECOV_CONDITION_ANY);
    size_t i = 0;

    // Colwert subevent string to a vector of integers (as address)
    for (; (i < subevt.size()) && (i < subevt_dim); ++i)
    {
        int idx = -1;
        string lwr_tok = subevt[i];

        if (isdigit(lwr_tok[0]))
        {
            idx = strtol(lwr_tok.c_str(), 0, 0);
            if (idx < (int)GetSubEventDimSize(i))
            {
                subevt_vec[i] = idx;
                continue;
            }
            else
            {
                Printf(Tee::PriError, "(%d) Event '%s': failed to parse subevent '%s': index %d(%d) exceeded the demension size %d\n",
                    line_num, evt_name.c_str(), lwr_tok.c_str(), idx, (int)i,
                    (int)GetSubEventDimSize(i));
                return false;
            }
        }

        switch (lwr_tok[0])
        {
            case '*':
                subevt_vec[i] = ECOV_CONDITION_ALL;
                break;
            case '?':
                subevt_vec[i] = ECOV_CONDITION_ANY;
                break;
            case '-':
                subevt_vec[i] = GetSubEventDimSize(i);
                break;
            default:                // should be a name
                idx = GetSubEvtIdxByName(i, Tokenizer::StripQuotes(lwr_tok));
                if (idx < 0)       // could not find the name
                {
                    Printf(Tee::PriError, "(%d) Event '%s': Could not find subevent name %s\n",
                        line_num, evt_name.c_str(), lwr_tok.c_str());
                    return false;
                }
                else
                {
                    subevt_vec[i] = idx;
                }
        }
    }

    SubEvtAddrType failed_vec(subevt_dim, 0);
    vector<SubEvtAddrType> matched_vecs;
    int expected_count = 0;
    if (check_count)
    {
         string lwr_tok = subevt[subevt.size() - 1];
         expected_count = strtol(lwr_tok.c_str(), 0, 0);
    }
    int subevt_matched = CheckSubEvent(subevt_vec, matched_vecs, failed_vec, expected_count) ? 1 : 0;

    // print the matched vector (for debugging/error)
    if (Tee::WillPrint(Tee::PriAlways))
    {
        Printf(Tee::PriAlways, "(%d) Event '%s': subevent (", line_num, evt_name.c_str());
        for (size_t i = 0; i < subevt.size(); ++i)
        {
            Printf(Tee::PriAlways, "%s ", subevt[i].c_str());
        }
        Printf(Tee::PriAlways, ") %s:", subevt_matched ? "match" : "not match");

        if (subevt_matched)    // dump the matched vectors for '*' combinations
        {
            for (size_t i = 0; i < matched_vecs.size(); ++i)
            {
                PrintVector(matched_vecs[i]);
            }
        }
        else
        {
            PrintVector(failed_vec);
        }
        Printf(Tee::PriAlways, "\n");
    }

    return (subevt_matched ^ ilw_evt) != 0;
}

void SimEvent::PrintVector(const SubEvtAddrType& vec) const
{
    size_t i;
    bool first_time = true;
    const size_t vec_dim = GetSubEventVecDim();

    Printf(Tee::PriAlways, "(");
    for (i = 0; (i < vec.size()) && (i < vec_dim); ++i)
    {
        if (first_time)
        {
            first_time = false;
        }
        else
        {
            Printf(Tee::PriAlways, " ");
        }

        switch (vec[i])
        {
            case ECOV_CONDITION_ANY:
                Printf(Tee::PriAlways, "?");
                break;
            case ECOV_CONDITION_ALL:
                Printf(Tee::PriAlways, "*");
                break;
            default:
                if (vec[i] == (int)GetSubEventDimSize(i))
                {
                    Printf(Tee::PriAlways, "-");
                }
                else
                {
                    Printf(Tee::PriAlways, "%0d", vec[i]);
                }
        }
    }
    Printf(Tee::PriAlways, ") = %0d ", (i < vec.size()) ? vec[i] : 0);
}

//
// CheckSubEvent tries to match the given subevent assertion with simulation events.
// The assertion for subevent allows ? (any) * (all) <name> and <number> as subevent
// indices.
//
// Any (?) should be interpreted as 'don't care'. Therefore any match, including the
// 'invalid bit', can be used here.
//
// For example, to match assertion (1 ? * ? *) where all dimensions have size 2,
// we search all (* *) combinations (0 0), (0 1), (1 0), (1 1), ie to check if
// all of the following four is true:
// (1 ? 0 ? 0)
// (1 ? 0 ? 1)
// (1 ? 1 ? 0)
// (1 ? 1 ? 1)
// where ? can be any number.

bool SimEvent::CheckSubEvent
(
    const SubEvtAddrType& subevt_dim,
    vector<SubEvtAddrType>& matched_dims,
    SubEvtAddrType& failed_dim,
    int expected_subevt_count
) const
{
    const size_t dim_num = subevt_dim.size();
    SubEvtAddrType lwr_subevt(subevt_dim);
    SubEvtAddrType subevt_all(dim_num, 0);
    SubEvtAddrType subevt_any(dim_num, 0);

    for (size_t i = 0; i < dim_num; ++i)
    {
        switch (subevt_dim[i])
        {
            case ECOV_CONDITION_ALL:
                subevt_all[i] = GetSubEventDimSize(i);
                if (subevt_all[i] == 0)
                {
                    // '*' to match 0-size dimension. Current decision is 'PASS'
                    matched_dims.push_back(subevt_dim);
                    return true;
                }
                break;
            case ECOV_CONDITION_ANY:
                subevt_any[i] = GetSubEventDimSize(i) + 1; // allow '-'
                break;
            default:
                break;
        }
    }

    ResetSubEventVector(subevt_all, lwr_subevt);
    for (bool next_vec = true; next_vec; next_vec = GetNextVector(subevt_all, lwr_subevt))
    {
        SubEvtAddrType full_subevt(lwr_subevt);
        bool find_match = false;

        int subevt_count = 0;
        ResetSubEventVector(subevt_any, full_subevt);
        for (bool next_vec2 = true; next_vec2;
            next_vec2=GetNextVector(subevt_any, full_subevt))
        {
            subevt_count = GetSubEventCount(full_subevt);
            if (subevt_count)
            {
                if ((expected_subevt_count > 0) && (expected_subevt_count != subevt_count))
                {
                    break;
                }
                find_match = true;
                full_subevt.push_back(subevt_count);
                matched_dims.push_back(full_subevt);
                break;
            }
        }

        if (!find_match)
        {
            // Failed to find a match
            failed_dim = lwr_subevt;
            failed_dim.push_back(subevt_count);
            return false;
        }
    }
    // All 'all' fields matched
    return true;
}

void SimEvent::ResetSubEventVector(const SubEvtAddrType& dim_vec, SubEvtAddrType& vec)
{
    size_t dim_num = vec.size();
    for (size_t i = 0; i < dim_num; ++i)
    {
        if (dim_vec[i] || (vec[i]==ECOV_CONDITION_ALL))
        {
            vec[i] = 0;
        }
    }
}

bool SimEvent::GetNextVector(const SubEvtAddrType& dim_vec, SubEvtAddrType& vec)
{
    size_t dim_num = vec.size();
    for (size_t i = 0; i < dim_num; ++i)
    {
        if (dim_vec[i])
        {
            if (++vec[i] == dim_vec[i])
            {
                vec[i] = 0;     // reset to 0; try to increase next dimension
            }
            else
            {
                return true;
            }
        }
    }
    return false;
}

void SimEvent::AddSubEvtDim(int size)
{
    MASSERT(size >= 0);

    m_SubEventDims.push_back(new SubEventDim(size));
}

void SimEvent::SetSubEvtIdxName(size_t v1, size_t v2, const string& name)
{
    MASSERT(v1 < m_SubEventDims.size());
    SubEventDim* lwr_dim = m_SubEventDims[v1];

    lwr_dim->m_DimNames[v2] = name;
}

int SimEvent::GetSubEvtIdxByName(size_t v1, const string& name) const
{
    MASSERT(v1 < m_SubEventDims.size());
    SubEventDim* lwr_dim = m_SubEventDims[v1];
    MASSERT(lwr_dim);

    for (size_t i = 0; i < lwr_dim->m_DimNames.size(); ++i)
    {
        if (lwr_dim->m_DimNames[i] == name)
        {
            return int(i);
        }
    }
    // no match found
    return -1;
}

UINT32 SimEvent::GetSubEventDimSize(size_t idx) const
{
    if (idx < m_SubEventDims.size())
    {
        return m_SubEventDims[idx]->m_DimSize;
    }
    return 0;
}

void SimEvent::ResizeSubEvtCountVec()
{
    // This just need once
    size_t total_size = 1;
    for (size_t i = 0; i < m_SubEventDims.size(); ++i)
    {
        total_size *= (m_SubEventDims[i]->m_DimSize + 1);
    }

    if (m_SubEventCounts.size() < total_size)
    {
        m_SubEventCounts.resize(total_size, 0);
    }
}

Tokenizer::Tokenizer(const string& str, const string& delim, const string& esc):
    m_Str(str), m_Delimiter(delim), m_EscStr(esc), m_LwrPos(0) {};

EventCover::Status Tokenizer::GetNextToken(string& token)
{
    if (m_Str.empty() || m_Delimiter.empty() || (m_LwrPos== string::npos))
    {
        return EventCover::Status::False;
    }

    string::size_type startPos = m_Str.find_first_not_of(m_Delimiter, m_LwrPos);
    if (startPos == string::npos)
    {
        m_LwrPos = string::npos;
        return EventCover::Status::False;           // No more token left
    }
    m_LwrPos = m_Str.find_first_of(m_Delimiter, startPos);

    if (m_EscStr.empty() || IsEscStrPaired(startPos, m_LwrPos))
    {
        token = m_Str.substr(startPos, m_LwrPos - startPos);
        return EventCover::Status::True;
    }

   //Adjust m_LwrPos to make sure ESC string oclwres pairly in token
    while (m_LwrPos != string::npos)
    {
        // Find the counterpart ESC string for last one in m_str[startPos, m_LwrPos)
        string::size_type escStrPos = m_Str.find(m_EscStr, m_LwrPos);
        if (escStrPos == string::npos) // no match at the ending
        {
            Printf(Tee::PriError, "No matching for ESC string '%s'\n", m_EscStr.c_str());
            return EventCover::Status::Error;
        }

        // Find the delimiter after previous ESC string
        m_LwrPos = m_Str.find_first_of(m_Delimiter, escStrPos);

        //There are even ESC string in [startPos, escStrPos+size) already
        //Make sure ESC string in [escStrPos+size, m_LwrPos) is paired also.
        if (IsEscStrPaired(escStrPos+m_EscStr.size(), m_LwrPos))
        {
            token = m_Str.substr(startPos, m_LwrPos - startPos);
            return EventCover::Status::True;
        }
    }

    Printf(Tee::PriError, "No matching for ESC string '%s'\n", m_EscStr.c_str());
    return EventCover::Status::Error;
}

void Tokenizer::ResetString(const string& str)
{
    m_Str = str;
    m_LwrPos = 0;
}

bool Tokenizer::IsEscStrPaired(string::size_type begin, string::size_type end)
{
    if (m_EscStr.empty() || begin >= end)
        return true;

    int count = 0;
    string str = m_Str.substr(begin, end-begin);
    for (size_t offset = str.find(m_EscStr); offset != string::npos;
        offset = str.find(m_EscStr, offset + m_EscStr.size()))
    {
        ++count;
    }

    return (count % 2 == 0) ? true : false;
}

void SimEvent::DumpSimEvent() const
{
    Printf(Tee::PriAlways, "Event \"%s\"", m_EventName.c_str());
    for (size_t i = 0; i < m_SubEventDims.size(); ++i)
    {
        Printf(Tee::PriAlways, " %d", m_SubEventDims[i]->m_DimSize);
    }
    Printf(Tee::PriAlways, "\n");
}
