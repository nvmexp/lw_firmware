/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/include/rc.h"
#include "core/include/types.h"

class LwLink;

//------------------------------------------------------------------------------
//! \brief Abstraction for a Throughput Counter
//!
struct LwLinkThroughputCount
{
    //! Enum describing the TL counter IDs
    enum CounterId
    {
         CI_TX_COUNT_0
        ,CI_TX_COUNT_1
        ,CI_TX_COUNT_2
        ,CI_TX_COUNT_3
        ,CI_RX_COUNT_0
        ,CI_RX_COUNT_1
        ,CI_RX_COUNT_2
        ,CI_RX_COUNT_3

        // Add new counter defines above this line
        ,CI_NUM_COUNTERS
    };

    // Defines what unit of traffic this counter will count if enabled
    enum UnitType
    {
        UT_CYCLES
       ,UT_PACKETS
       ,UT_FLITS
       ,UT_BYTES
    };

    // Identifies the type(s) of flits to count when Unit = Flits.
    // More than one filter bit may be set, in which case flits of all matching
    // types are counted.
    enum FlitFilter
    {
        FF_HEAD  = (1 << 0)
       ,FF_AE    = (1 << 1)
       ,FF_BE    = (1 << 2)
       ,FF_DATA  = (1 << 3)
       ,FF_ALL   = (FF_DATA << 1) - 1
    };

    // Qualifies the counting to particular packet types when the counting unit is
    // Packets, Flits, or Data Bytes. More than one filter bit may be set, in which
    // case the contents of all matching packets are counted.
    enum PacketFilter
    {
        PF_ONLY_NOP
       ,PF_ALL_EXCEPT_NOP
       ,PF_DATA_ONLY
       ,PF_WR_RSP_ONLY
       ,PF_REQ_ONLY
       ,PF_RSP_ONLY
       ,PF_ALL
    };

    // Indicates how many events are needed to cause a pulse on the appropriate
    // pm_<tx, rx>_cnt<0, 1> signal to the perfmon logic. This allows event frequency
    // to be divided down when sysclkSysClk is slow.
    enum PmSize
    {
        PS_1
       ,PS_2
       ,PS_4
       ,PS_8
       ,PS_16
       ,PS_32
       ,PS_64
       ,PS_128
    };

    struct Config
    {
        CounterId m_Cid    = CI_NUM_COUNTERS;
        UnitType  m_Ut     = UT_BYTES;
        UINT32    m_Ff     = FF_ALL;
        UINT32    m_Pf     = PF_ALL;
        PmSize    m_PmSize = PS_1;
        Config() = default;
        Config(CounterId cid, UnitType ut, UINT32 ff, UINT32 pf, PmSize ps)
            : m_Cid(cid), m_Ut(ut), m_Ff(ff), m_Pf(pf), m_PmSize(ps) {}
    };

    LwLinkThroughputCount() = default;
    UINT64    countValue = 0;      //!< Value of the counter
    bool      bValid = false;      //!< True if the count is valid
    bool      bOverflow = false;   //!< True if the count is valid
    Config    config;              //!< Configuration for the counter

    //! \brief Get the string for a specified counter
    //!
    //! \param tc     : Counter config to get the string for
    //! \param prefix : Prefix for the string
    //!
    //! \return String representing the counter
    static string GetString(const Config &tc, string prefix);

    //! \brief Get the detailed string for a specified throughput count
    //!
    //! \param tc     : Counter config to get the string for
    //! \param prefix : Prefix for the string
    //!
    //! \return String representing the entire counter config
    static string GetDetailedString(const Config &tc, string prefix);
    static UINT32 CounterIdToIndex(CounterId counterId);
};

class LwLinkThroughputCounters
{
public:
    INTERFACE_HEADER(LwLinkThroughputCounters);

    LwLinkThroughputCounters() = default;
    virtual ~LwLinkThroughputCounters() { }

    bool AreThroughputCountersSetup(UINT64 linkMask) const;
    RC ClearThroughputCounters(UINT64 linkMask);
    RC GetThroughputCounters
    (
        UINT64 linkMask
      , map<UINT32, vector<LwLinkThroughputCount>> *pCounts
    );
    bool IsSupported() const
        { return DoIsSupported(); }
    RC SetupThroughputCounters
    (
        UINT64 linkMask
      , const vector<LwLinkThroughputCount::Config> &configs
    );
    RC StartThroughputCounters(UINT64 linkMask);
    RC StopThroughputCounters(UINT64 linkMask);
    bool IsCounterSupported(LwLinkThroughputCount::CounterId cid) const
        { return DoIsCounterSupported(cid); }
protected:
    RC CheckLinkMask(UINT64 linkMask, const char * reason) const
        { return DoCheckLinkMask(linkMask, reason); }

private:

    // Throughput counter helper functions
    virtual bool DoIsCounterSupported(LwLinkThroughputCount::CounterId cid) const = 0;
    virtual bool DoIsSupported() const = 0;
    virtual RC DoCheckLinkMask(UINT64 linkMask, const char * reason) const = 0;
    virtual RC ReadThroughputCounts
    (
        UINT32 linkId
       ,UINT32 counterMask
       ,vector<LwLinkThroughputCount> *pCounts
    ) = 0;
    virtual RC WriteThroughputClearRegs(UINT32 linkId, UINT32 counterMask) = 0;
    virtual RC WriteThroughputStartRegs(UINT32 linkId, UINT32 counterMask, bool bStart) = 0;
    virtual RC WriteThroughputSetupRegs
    (
        UINT32 linkId,
        const vector<LwLinkThroughputCount::Config> &configs
    ) = 0;

    map<UINT32, pair<UINT32, bool>> m_ThroughputSetup;//!< Mask of throughput counters on a link
                                                      //!< and whether or not they are running
};
