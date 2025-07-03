/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef __PM_MUX_MGR_H__
#define __PM_MUX_MGR_H__

//------------------------------------------------------------------------------
//
//  object that knows how to set PRI PM multiplex controls to route desired
//  signals to the PM.
//
//  Generally you will have one of these for each (hardware) experiment you
//  are configuring.  The PMMuxMgr doesn't know about multiple watchbuses.
//
//  You may pass in a list of vpmSignalNames, from vpmSigNames.h, to
//  describe any fixed mappings.
//------------------------------------------------------------------------------

// IMPORTANT: since this file is used outside of MODS, please avoid adding extra
// includes on MODS stuff if possible
#include "types.h"
#include <vector>
#include <map>
#include <string>
#include <iostream>

//  The client is responsible for supplying this callback object
//  which will perform masked 32-bit register writes.  For instance, the client
//  supplied function might call lwgpu->RegRd32 and lwgpu->RegWr32
//  to implement this function.
//  This is done to make the component more reusable and remove
//  dependencies.
//  The callback object allows the client to store his own data in a typesafe
//  way to be used by the function.

struct RegWriteFunctor
{
    virtual void RegWriteFn(UINT32 address, UINT32 maskLo, UINT32 maskHi, UINT32 value, const char* name, UINT32 idx) = 0;
};

class PMMuxMgr
{

public:

    PMMuxMgr(RegWriteFunctor& regWriteFunctor);
    virtual ~PMMuxMgr();

    //  AddNonMuxedSignalInfo is used to add signal names (for instance
    //  from vpmSigNames.h) that are not muxed but still need to be
    //  resolved to the correct watchbus index.
    //  Typically you would call this for each signal in the watchbus
    //  you are interested in before using RequestSignal and Compute.
    void AddNonMuxedSignalInfo(const char* sigName, int index);

    //  call request signal once for each unit signal that needs routing
    //  (the input signal names from the .spec file, also the strings
    //  in vpmSigNames.h)
    //  will return false if the sigName couldn't be found.
    //  for units that have multiple intances (ie TPCs), you can supply
    //  an instance number to pick the instance of interest.
    //  Do this before calling Compute.
    bool RequestSignal(const char* sigName, int instance = 0);

    //  call Compute to determine a mux/instance setting for the signals
    //  which were specified with RequestSignal earlier.
    //  useComprehensiveSearch is potentially very slow, but is the default
    //  to match legacy behavior.
    //  returns false and the error string will be filled in if there's
    //  a failure to mux the signals.
    bool Compute(std::string& error, bool useComprehensiveSearch = true);

    //  call Set whenever you need the signals to be muxed, this will
    //  do a lot of PRI register writes to the various controls
    //  specified in the .spec files for this purpose.
    void Set();

    //  GetWatchbusIndex returns the index of the signal requested on the
    //  PM watchbus, so that signal names can be mapped to their hardware
    //  input locations.
    //  returns -1 for an unknown signal name
    INT32 GetWatchbusIndex(const char* sigName, int instance = 0);

    //  removes all settings (in case of an error) so that nothing will be
    //  written to PRI registers for a broken experiment
    void ClearSettings();
    static void ClearChoices();

    // Set register offset: Specify the register address offset if it is not
    // fully coded in the header file. For example: TPC registers could be
    // 0x400000.
    // Note, this is mostly for LW50, where the header file cannot be update
    // any more from HW tree. All g8x and later chips should have the offset
    // correctly coded in the header file (therefore offset is 0).
    void SetRegOffset( UINT32 offset ) { m_RegOffset = offset; }
    UINT32 GetRegOffset() const { return m_RegOffset; }

    // chip specific function, moved to derived class
//     static void load_pm_mappings() = 0;
    static void instance_mapping(const char* signal, UINT32 addressBase, UINT32 addressInc, UINT32 numInstances, UINT32 maskHi, UINT32 maskLo);
    static void instanced_mux_mapping(const char* signal, UINT32 pmWatch, UINT32 value, UINT32 addressBase, UINT32 addressInc, UINT32 addressNum, UINT32 maskHi, UINT32 maskLo);

protected:
    static bool s_pm_mappings_loaded;

private:

    //  address, mask, value
    //  actually specifies an array of addresses, using the base, inc, and num fields
    struct AMV
    {
        UINT32  m_addressBase;
        UINT32  m_addressInc;
        UINT32  m_addressNum;
        UINT32  m_maskHi;
        UINT32  m_maskLo;
        UINT32  m_value;
        std::string  m_signame;

        bool operator == (const AMV& amv) const;
        bool ConflictsWith(const AMV& amv) const;
        bool CanMergeWith(const AMV& amv) const;
        void MergeWith(const AMV& amv);
    };

    struct InstanceSetting
    {
        InstanceSetting() {}
        InstanceSetting(const AMV& amv, int instance) : m_instance(instance), m_setting(amv) {}
        bool operator == (const InstanceSetting& setting) const;
        UINT32 m_instance;
        AMV m_setting;
    };

    struct MuxSetting
    {
        UINT32  m_watchbusIndex;
        AMV     m_setting;
    };

    struct MuxInfo
    {
        std::vector<MuxSetting> m_settings;
    };

    struct RequestedSignal
    {
        RequestedSignal() {}
        RequestedSignal(const char* name, int instance) : m_name(name), m_instance(instance) {}
        RequestedSignal(const char* name) : m_name(name), m_instance(-1) {}
        bool operator < (const RequestedSignal& sig) const;
        bool operator == (const RequestedSignal& sig) const;
        std::string m_name;
        int m_instance; // -1 for no instance needed or don't care
    };

    bool has_conflict(int* settings, MuxInfo** mux_infos) { return has_conflict(settings, mux_infos, (int)m_requestedSignals.size()); }
    bool has_conflict(int* settings, MuxInfo** mux_infos, int upToSignal);
    bool brute_force_find_mux_setting(int* settings, int* num_settings, MuxInfo** mux_infos);
    bool incremental_find_mux_setting(int* settings, int* num_settings, MuxInfo** mux_infos);
    bool compute_instance_settings(std::string& error);
    void optimize_settings();

    RegWriteFunctor& m_writeFunction;

    typedef std::map<std::string, MuxInfo> MuxInfoMap;
    static MuxInfoMap s_muxSettings;
    typedef std::map<std::string, AMV> InstanceInfoMap;
    static InstanceInfoMap s_instanceSettings;

    typedef std::map<std::string, int> NonMuxedSignalMap;
    NonMuxedSignalMap m_nonMuxedSignals;
    std::vector<AMV> m_muxSettings;
    std::vector<InstanceSetting> m_instanceSettings;
    std::vector<RequestedSignal> m_requestedSignals;
    typedef std::map<RequestedSignal, UINT32> SignalMap;
    SignalMap m_resultMap;
    bool m_computed;
    UINT32 m_RegOffset;         // Register Offset
    static std::vector<AMV> s_muxChoices;
    static std::vector<InstanceSetting> s_instanceChoices;
};

#endif // __PM_MUX_MGR_H__

