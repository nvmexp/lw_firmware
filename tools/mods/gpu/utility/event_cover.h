
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

#ifndef _EVENT_COVER_H_
#define _EVENT_COVER_H_

#include <map>
#include <vector>
#include <string>
#include <list>
#include "core/include/types.h"
#include "core/include/argread.h"

class SimEvent;
typedef map<string, SimEvent*> SimEventListType;
typedef list<string>           CrcChain;

//
// Class EventCover defines the event cover assertion expected for a specific
// test. It should be saved in test.ecov file together with the test trace.
// A typical format looks like:
//
// chain-link[,substr...] [!] event-name [sub-event ...]
//
// Detail description can be found here:
//
// https://fermi/wiki/index.php/Infrastructure/CoverageAssertions
//
class EventCover
{
public:
    // This enum includes an additional error return status for various
    // EventCover functions.
    enum class Status
    {
        False = 0,
        True = 1,
        Error = 2,
    };

    EventCover(const string& testChipName, const string& fileName);
    virtual ~EventCover();

    EventCover::Status MatchCondition(const ArgReader* tparams,
        const ArgReader* rparams, const ArgReader* jparams,
        const CrcChain& crc_chain, const vector<string>& classNameIgnoreList) const;
    bool MatchEvent(const SimEventListType& sim_events) const;
    EventCover::Status ParseOneLine(const string& linecon, int line_num);
    int  GetLineNumber() const { return m_LineNum; }
    const string& GetFileName() const { return m_FileName; }
    const string& GetEvent() const { return m_EventAssert; }
    const string& GetEventName() const { return m_EventName; }

private:
    string m_EventName;
    int    m_LineNum;           // line number defined in ecov file
    string m_FileName;          // name of ecov file
    vector<string> m_ClassNames;// class names;
    string m_TestChipName;      // Current test chip name
    bool   m_MatchesTestChip;   // If true, match test chip, will check this event
    string m_ParamList;         // list of parameters
    vector<string> m_SubEvents; // Sub-event string list
    bool   m_EventIlw;          // Event ilwerter (!Event)
    string m_EventAssert;
    bool   m_CounterCheck;      //If true, check event counter.
};

typedef vector<EventCover*> ECovListType;

//
// Class SimEvent defines the events happened in a simulation. MODS picks up
// the event database captured by FMODEL and constructs this data structure
// for later validating.
//
// SubEvents are multi-demension vector associated with one event.
//

typedef vector<int> SubEvtAddrType;

#define ECOV_CONDITION_ALL    (-1)
#define ECOV_CONDITION_ANY    (-2)

#define SPACE_DELIMITER    (" \t\n")
#define DOUBLE_QUOTES '"'

class SimEvent
{
public:
    SimEvent(const string& name, int count);
    virtual ~SimEvent();

    int  GetEventCount() const { return m_EventCount; }
    void SetEventCount(int val) { m_EventCount = val; }
    UINT32 GetSubEventDimSize(size_t idx) const;
    size_t GetSubEventVecDim() const { return m_SubEventDims.size(); }
    void SetSubEventCount(int val, const SubEvtAddrType& idx);
    int  GetSubEventCount(const SubEvtAddrType& idx) const;

    void AddSubEvtDim(int size);
    void SetSubEvtIdxName(size_t v1, size_t v2, const string& name);

    bool MatchSubEvents(const vector<string>& subevents, const string& evt_name,
        int line_number, int ilw_evt, bool count_check) const;
    bool CheckSubEvent(const SubEvtAddrType& subevt_dim, vector<SubEvtAddrType>&
        matched_dims, SubEvtAddrType& failed_dim, int expected_subevt_count) const;
    void ResizeSubEvtCountVec();
    void DumpSimEvent() const;
    void PrintVector(const SubEvtAddrType& vec) const;

    static void ResetSubEventVector(const SubEvtAddrType& dim_vec, SubEvtAddrType& vec);
    static bool GetNextVector(const SubEvtAddrType& dim_vec, SubEvtAddrType& vec);

private:
    // SubEventDim defines one dimension of subevent.
    struct SubEventDim
    {
        SubEventDim(UINT32 sz): m_DimSize(sz), m_DimNames(sz+1){};

        UINT32 m_DimSize;
        vector<string> m_DimNames;
    };

    // Though subevent is a multi-dimension table, the event counts are stored
    // in a one-dimension vector. GetSubEvtIdx() is used to translate the vector
    // address <v1, v2, v3...> into a real index value, as follows:
    // (((((v1*d1_size)+v2)*d2_size)+v3)*d3_size...)
    int GetSubEvtIdx(const SubEvtAddrType& idx) const;
    int GetSubEvtIdxByName(size_t v1, const string& name) const;

    string m_EventName;
    int    m_EventCount;

    vector<UINT32> m_SubEventCounts;
    vector<SubEventDim*> m_SubEventDims;
};

// Tokenizer tries to tokenize the given str with the delimiter.
//
// If Escape str defined, any delimiters quoted inside a pair of esc char
// will be ignored. Eg 'test 12' is a token if esc char is "'".

class Tokenizer
{
public:
    Tokenizer(const string& str, const string& delim, const string& esc);
    ~Tokenizer() {};

    EventCover::Status GetNextToken(string& token);
    void ResetString(const string& str);
    void ResetDelimiter(const string& str) { m_Delimiter = str; }

    static const string& StripQuotes(string& str);
    bool IsEscStrPaired(string::size_type begin, string::size_type end);

private:
    string m_Str;
    string m_Delimiter;
    string m_EscStr;
    string::size_type m_LwrPos;
};

#endif
