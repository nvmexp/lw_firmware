/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
//------------------------------------------------------------------------------
//
//  object that knows how to set PRI PM multiplex controls to route desired
//  signals to the PM.
//
//  If you need more than one set of multiplex signals, make more than one
//  of these.
//------------------------------------------------------------------------------

#include "pm_mux_mgr.h"

// IMPORTANT: since this file is used outside of MODS, please avoid adding extra
// includes on MODS stuff if possible
#include <string>
#include <map>
#include <algorithm>
#include <stdio.h>
#include <assert.h>
#include <string.h>

bool PMMuxMgr::s_pm_mappings_loaded = false;

std::map<std::string, PMMuxMgr::MuxInfo> PMMuxMgr::s_muxSettings;
std::map<std::string, PMMuxMgr::AMV> PMMuxMgr::s_instanceSettings;
std::vector<PMMuxMgr::AMV> PMMuxMgr::s_muxChoices;
std::vector<PMMuxMgr::InstanceSetting> PMMuxMgr::s_instanceChoices;

#define INFO DebugPrintf

bool PMMuxMgr::AMV::ConflictsWith(const AMV& amv) const
{
    if( this->m_addressBase == amv.m_addressBase )
    {
        if( (this->m_maskHi == amv.m_maskHi) && (this->m_maskLo == amv.m_maskLo) )
        {
            return this->m_value != amv.m_value;
        }
    }

    return false;
}

bool PMMuxMgr::AMV::operator == (const AMV& amv) const
{
    return  (m_addressBase == amv.m_addressBase) &&
            (m_addressInc == amv.m_addressInc ) &&
            (m_addressNum == amv.m_addressNum ) &&
            (m_maskHi == amv.m_maskHi ) &&
            (m_maskLo == amv.m_maskLo ) &&
            (m_value == amv.m_value );
}

bool PMMuxMgr::AMV::CanMergeWith(const AMV& amv) const
{
    if( (m_addressBase == amv.m_addressBase) &&
        (m_addressInc == amv.m_addressInc ) &&
        (m_addressNum == amv.m_addressNum ) )
    {
        if( (m_maskHi == (amv.m_maskLo-1)) ||
            (m_maskLo == (amv.m_maskHi+1)) )
            return true;
    }

    return false;
}

void PMMuxMgr::AMV::MergeWith(const AMV& amv)
{
    if( m_maskHi == (amv.m_maskLo-1) )
    {
        m_value |= amv.m_value << (m_maskHi - m_maskLo + 1);
        m_maskHi = amv.m_maskHi;
    }
    else
    {
        assert(m_maskLo == (amv.m_maskHi+1));
        m_value = amv.m_value | (m_value << (amv.m_maskHi - amv.m_maskLo + 1));
        m_maskLo = amv.m_maskLo;
    }
}

/*std::ostream& operator << (std::ostream &os, const PMMuxMgr::AMV& amv)
{
    os << hex
        << "0x" << amv.m_addressBase << " + 0x" << amv.m_addressInc << " * [0," << dec << amv.m_addressNum << ") : "
        << "0x" << hex << amv.m_value
        << " [" << dec << amv.m_maskLo << "," << amv.m_maskHi << "]";
    return os;
}*/

bool PMMuxMgr::RequestedSignal::operator < (const RequestedSignal& sig) const
{
    if( this->m_name == sig.m_name )
        return this->m_instance < sig.m_instance;
    else
        return this->m_name < sig.m_name;
}

bool PMMuxMgr::RequestedSignal::operator == (const RequestedSignal& sig) const
{
    return (this->m_name == sig.m_name) && (this->m_instance == sig.m_instance);
}

bool PMMuxMgr::InstanceSetting::operator == (const InstanceSetting& setting) const
{
    return (this->m_instance == setting.m_instance) && (this->m_setting == setting.m_setting);
}

PMMuxMgr::PMMuxMgr(RegWriteFunctor& regWriteFunctor)
    :    m_writeFunction(regWriteFunctor), m_computed(false),  m_RegOffset(0)
{
    // Load PM mappings was moved to child class
}

PMMuxMgr::~PMMuxMgr()
{
}

void PMMuxMgr::AddNonMuxedSignalInfo(const char* sigName, int index)
{
    m_nonMuxedSignals[sigName] = index;
}

bool PMMuxMgr::RequestSignal(const char* sigName, int instance)
{
//     INFO("PMMuxMgr::RequestSignal - %s[%d]\n", sigName, instance);
//     INFO("m_requestedSignals.size() = %d\n", m_requestedSignals.size());
    RequestedSignal sig(sigName, instance);
    if( std::find(m_requestedSignals.begin(), m_requestedSignals.end(), sig) == m_requestedSignals.end() )
        m_requestedSignals.push_back(sig);
    return true;
}

template <class T>
bool increment_vector(T* vals, T* num_vals, int vec_len)
{
    int to_inc = 0;
    if( vec_len <= 0 ) return false;

    if( vals[to_inc] < (num_vals[to_inc]-1) )
    {
        vals[to_inc]++;
    }
    else
    {
        do
        {
            to_inc++;
            if( to_inc >= vec_len ) return false;
        }
        while( vals[to_inc] >= (num_vals[to_inc]-1) );

        vals[to_inc]++;
    }

    while( to_inc > 0 )
    {
        to_inc--;
        vals[to_inc] = 0;
    }

    return true;
}

bool PMMuxMgr::incremental_find_mux_setting(int* settings, int* num_settings, MuxInfo** mux_infos)
{
    int num_requested = m_requestedSignals.size();

    //  for each signal, check against previous signals; if there's a conflict, increment it
    //  if the conflict doesn't go away, error out, to signal that this simple algorithm
    //  couldn't find a solution
    for( int lwr_signal_index = 1 ; lwr_signal_index < num_requested ; ++lwr_signal_index )
    {
        //  test

        //printf("lwr_signal_index = %d : ", lwr_signal_index);
        //for( int i = 0 ; i < num_requested ; ++i )
        //    printf("%d ", settings[i]);
        //printf("\n");

        bool conflict = this->has_conflict(settings, mux_infos, lwr_signal_index+1);

        if( conflict )
        {
            int lwr_num_settings = num_settings[lwr_signal_index];
            for( int lwr_setting_index = 1 ; lwr_setting_index < lwr_num_settings ; ++lwr_setting_index )
            {
                settings[lwr_signal_index] = lwr_setting_index;
                conflict = this->has_conflict(settings, mux_infos, lwr_signal_index+1);
                if( !conflict )
                    break;
            }

            if( conflict )
                return false;   //  couldn't resolve by incrementing this signal
        }
    }
    return true;
}

bool PMMuxMgr::brute_force_find_mux_setting(int* settings, int* num_settings, MuxInfo** mux_infos)
{
    int num_requested = m_requestedSignals.size();

    //  iterate over all settings in vector
    //  to find a valid solution
    //  this is not smart but guaranteed to complete and find a solution if there is one
    while( true )
    {
        //  test
        bool conflict = this->has_conflict(settings, mux_infos);

        if( conflict )
        {
            if( !increment_vector(settings, num_settings, num_requested) )
                return false;
        }
        else
            break;
    }
    return true;
}

bool PMMuxMgr::Compute(std::string& error, bool useComprehensiveSearch)
{
    if( !this->compute_instance_settings(error) )
        return false;

    int num_requested = m_requestedSignals.size();

    //  For the mux settings, the possible settings
    //  that can reproduce a given set of signals can be described as a vector
    //  of integers.  Each integer corresponds to a acceptable setting that will mux the requested
    //  signal.  For instance, if we had 3 signals, each with 2 settings that could mux the signals
    //  in the way we want, we could describe the variations with the following vectors of integers:
    //  0 0 0 ; 0 0 1 ; 0 1 0 ; 0 1 1 ; 1 0 0 ; 1 0 1 ; 1 1 0 ; 1 1 1
    //  So for instance, 1 0 0 means the second listed mux setting for the first signal, and the
    //  first listed mux setting for the second and third signals.
    //  Some combinations will not be allowable, for instance if the first listed mux setting
    //  for the second and third mux signals muxes both to the same pm watchbus index, any configuration
    //  like x 0 0 will not be allowed.  So we could still pick x 0 1, x 1 0, or x 1 1.

    std::vector<int> lwr_settings(num_requested, 0);
    std::vector<MuxInfo*> mux_infos(num_requested, (MuxInfo*)NULL);
    std::vector<int> num_settings(num_requested, 0);

    //  record information about existing mux settings for each signal
    for( int i = 0 ; i < num_requested ; ++i )
    {
        MuxInfoMap::iterator it = s_muxSettings.find(m_requestedSignals[i].m_name);
        if( it != s_muxSettings.end() )
        {
            MuxInfo& mux_info = it->second;
            mux_infos[i] = &mux_info;
            num_settings[i] = mux_info.m_settings.size();
        }
    }

    if( !this->incremental_find_mux_setting(&lwr_settings[0], &num_settings[0], &mux_infos[0]) )
    {
        if( !useComprehensiveSearch )
        {
            error = "Couldn't find non-conflicting set of mux settings (incremental)";
            return false;
        }

        memset(&lwr_settings[0], 0, sizeof(int) * lwr_settings.size());
        if( !this->brute_force_find_mux_setting(&lwr_settings[0], &num_settings[0], &mux_infos[0]) )
        {
            error = "Couldn't find non-conflicting set of mux settings (comprehensive)";
            return false;
        }
    }

    //  now we have our set of mux settings which should be valid.
    //  build up list of AMVs we need to write to implement them,
    //  and record watchbus indices for the signals.
    for( int i = 0 ; i < num_requested ; ++i )
    {
        RequestedSignal& reqSig = m_requestedSignals[i];
        if( mux_infos[i] )
        {
            MuxSetting& setting = mux_infos[i]->m_settings[lwr_settings[i]];
            m_muxSettings.push_back(setting.m_setting);
            m_resultMap[reqSig] = setting.m_watchbusIndex;
            s_muxChoices.push_back(setting.m_setting);
        }
        else
        {
            NonMuxedSignalMap::iterator it = m_nonMuxedSignals.find(reqSig.m_name);

            if( it != m_nonMuxedSignals.end() )
            {
                m_resultMap[reqSig] = it->second;
            }
            else
            {
                char temp[1024];
                sprintf(temp, "signal %s[%d] is not defined\n", reqSig.m_name.c_str(), reqSig.m_instance);
                error = temp;
                return false;
            }
        }
    }

    this->optimize_settings();
    m_computed = true;

    return true;
}

//  call Set whenever you need the signals to be muxed, this will
//  do a lot of PRI register writes to the various controls
//  specified in the .spec files for this purpose.
void PMMuxMgr::Set()
{
    if( !m_computed )
    {
        return;
    }

    //  for the instance settings, we write a 1 to the enable field of the instance we want and
    //  0 to all the others
    int num_settings = m_instanceSettings.size();
    for( int i = 0 ; i < num_settings ; ++i )
    {
        const InstanceSetting& instance_setting = m_instanceSettings[i];
        const AMV& amv = instance_setting.m_setting;

        for( unsigned int instance_index = 0 ; instance_index < instance_setting.m_setting.m_addressNum ; ++instance_index )
        {
            //  We or the address with 0x00400000 because the TPC addresses in the lw_ref_dev_tpc are written without the 4.
            m_writeFunction.RegWriteFn( (amv.m_addressBase | m_RegOffset) + (amv.m_addressInc * instance_index),
                amv.m_maskLo,
                amv.m_maskHi,
                (instance_index == instance_setting.m_instance) ? 1 : 0,
                amv.m_signame.c_str(),
                instance_index);
        }
    }

    //  for the mux settings, we write identical values for each instance
    num_settings = m_muxSettings.size();
    for( int i = 0 ; i < num_settings ; ++i )
    {
        const AMV& amv = m_muxSettings[i];
        for( UINT32 instance_index = 0 ; instance_index < amv.m_addressNum ; ++instance_index )
        {
            m_writeFunction.RegWriteFn( (amv.m_addressBase | m_RegOffset) + (amv.m_addressInc * instance_index),
                amv.m_maskLo,
                amv.m_maskHi,
                amv.m_value,
                amv.m_signame.c_str(),
                instance_index );
        }
    }
}

INT32 PMMuxMgr::GetWatchbusIndex(const char* sigName, int instance)
{
    if( !m_computed )
    {
        return -1;
    }

    SignalMap::iterator it = m_resultMap.find(RequestedSignal(sigName, instance));
    if( it == m_resultMap.end() )
    {
        NonMuxedSignalMap::iterator nmsm_it = m_nonMuxedSignals.find(sigName);
        if( nmsm_it == m_nonMuxedSignals.end() )
        {
            //INFO("Couldn't find signal %s\n", sigName);
            return -1;
        }
        else
        {
            //INFO("Found non-muxed signal %s\n", sigName);
            return nmsm_it->second;
        }
    }
    else
    {
        //INFO("Found muxed signal %s\n", sigName);
        return it->second;
    }
}

void PMMuxMgr::optimize_settings()
{
    //cout << "\nOld number of mux settings: " << m_muxSettings.size() << endl;
    //copy(m_muxSettings.begin(), m_muxSettings.end(), ostream_iterator<AMV>(cout, "\n"));

    std::vector<AMV>::iterator new_mux_end = std::unique(m_muxSettings.begin(), m_muxSettings.end());
    m_muxSettings.erase(new_mux_end, m_muxSettings.end());

    bool found = true;
    while( found )
    {
        found = false;
        //  search for possible merge
        int num_settings = m_muxSettings.size();
        for( int m1 = 0 ; m1 < (num_settings-1) ; ++m1 )
        {
            for( int m2 = m1+1 ; m2 < num_settings ; ++m2 )
            {
                if( m_muxSettings[m1].CanMergeWith(m_muxSettings[m2]) )
                {
                    m_muxSettings[m1].MergeWith(m_muxSettings[m2]);

                    m_muxSettings.erase(m_muxSettings.begin()+m2);
                    found = true;
                    break;
                }
            }
            if( found ) break;
        }
    }

    //cout << "\nNew number of mux settings: " << m_muxSettings.size() << endl;
    //copy(m_muxSettings.begin(), m_muxSettings.end(), ostream_iterator<AMV>(cout, "\n"));

    //cout << "\nOld number of instance settings: " << m_instanceSettings.size() << endl;
    std::vector<InstanceSetting>::iterator new_instance_end = std::unique(m_instanceSettings.begin(), m_instanceSettings.end());
    m_instanceSettings.erase(new_instance_end , m_instanceSettings.end());
    //cout << "New number of instance settings: " << m_instanceSettings.size() << endl << endl;
}

void PMMuxMgr::instance_mapping(const char* signal, UINT32 addressBase, UINT32 addressInc, UINT32 numInstances, UINT32 maskHi, UINT32 maskLo)
{
    AMV temp;
    temp.m_addressBase = addressBase;
    temp.m_addressInc = addressInc;
    temp.m_addressNum = numInstances;
    temp.m_maskHi = maskHi;
    temp.m_maskLo = maskLo;
    temp.m_value = 1;
    temp.m_signame = signal;
    s_instanceSettings[signal] = temp;
}

void PMMuxMgr::instanced_mux_mapping(const char* signal, UINT32 pmWatch, UINT32 value, UINT32 addressBase, UINT32 addressInc, UINT32 addressNum, UINT32 maskHi, UINT32 maskLo)
{
    MuxSetting temp;
    temp.m_watchbusIndex = pmWatch;
    temp.m_setting.m_addressBase = addressBase;
    temp.m_setting.m_addressInc = addressInc;
    temp.m_setting.m_addressNum = addressNum;
    temp.m_setting.m_maskHi = maskHi;
    temp.m_setting.m_maskLo = maskLo;
    temp.m_setting.m_value = value;
    temp.m_setting.m_signame = signal;

    MuxInfoMap::iterator it = s_muxSettings.find(signal);
    if( it != s_muxSettings.end() )
    {
        it->second.m_settings.push_back(temp);
    }
    else
    {
        std::pair<MuxInfoMap::iterator, bool> pairib = s_muxSettings.insert(std::make_pair(signal, MuxInfo()));
        assert(pairib.second == true);  //  didn't find it before so it should be inserted new now
        MuxInfo &info = pairib.first->second;
        info.m_settings.push_back(temp);
    }
}

bool PMMuxMgr::has_conflict(int* settings, MuxInfo** mux_infos, int upToSignal)
{
    for( int check1 = 0 ; check1 < (upToSignal-1) ; ++check1 )
    {
        if( mux_infos[check1] == NULL ) continue;
        MuxSetting& mux_setting1 = mux_infos[check1]->m_settings[settings[check1]];
        for( int check2 = check1 + 1 ; check2 < upToSignal ; ++check2 )
        {
            if( mux_infos[check2] == NULL ) continue;
            MuxSetting& mux_setting2 = mux_infos[check2]->m_settings[settings[check2]];
            if( mux_setting1.m_watchbusIndex == mux_setting2.m_watchbusIndex )
                return true;

            if( mux_setting1.m_setting.ConflictsWith(mux_setting2.m_setting) )
                return true;
        }

        // Also check the conflict against previous domain's choices
        std::vector<AMV>::const_iterator mux_iter = s_muxChoices.begin();
        std::vector<AMV>::const_iterator mux_end = s_muxChoices.end();
        for( ; mux_iter != mux_end; ++mux_iter )
        {
            if( mux_setting1.m_setting.ConflictsWith(*mux_iter) )
                return true;
        }
    }

    return false;
}

bool PMMuxMgr::compute_instance_settings(std::string& error)
{
    //  For the instance settings, there's only one way to get each instance we need, we can just iterate through
    //  and record the settings we need.
    int num_requested = m_requestedSignals.size();
    std::vector<RequestedSignal> instanced_signals;    //  we use this to record signal information in case of errors
    for( int i = 0 ; i < num_requested ; ++i )
    {
        RequestedSignal& sig = m_requestedSignals[i];

        if( sig.m_instance >= 0 )
        {
            InstanceInfoMap::iterator it = s_instanceSettings.find(sig.m_name);
            if( it == s_instanceSettings.end() )
                continue;       //  no instance setting for signal, but this is not necessary an error

            const AMV& amv = it->second;

            //  check previous signals for conflict
            for( size_t check_index = 0 ; check_index < m_instanceSettings.size() ; ++check_index )
            {
                RequestedSignal& check_sig = instanced_signals[check_index];
                if( (sig.m_instance != check_sig.m_instance) && (amv == m_instanceSettings[check_index].m_setting) )
                {
                    char temp[1024];
                    sprintf(temp, "Request for instanced signal %s[%d] conflicts with %s[%d]", sig.m_name.c_str(), sig.m_instance, check_sig.m_name.c_str(), check_sig.m_instance);
                    error = temp;
                    return false;
                }
            }

            // Also check with previous domains' settings for conflicts
            for( std::vector<InstanceSetting>::const_iterator iter = s_instanceChoices.begin();
                 iter != s_instanceChoices.end(); ++iter )
            {
                if( (sig.m_instance != (int) iter->m_instance) && (amv == iter->m_setting) )
                {
                    char temp[1024];
                    sprintf(temp, "Request for instanced signal %s[%d] conflicts with signal in previous domain",
                        sig.m_name.c_str(), sig.m_instance);
                    error = temp;
                    return false;
                }
            }

            instanced_signals.push_back(sig);
            m_instanceSettings.push_back(InstanceSetting(amv, sig.m_instance));
            s_instanceChoices.push_back(InstanceSetting(amv, sig.m_instance));
        }
    }

    return true;
}

//  removes all settings (in case of an error) so that nothing will be
//  written to PRI registers for a broken experiment
void PMMuxMgr::ClearSettings()
{
    m_instanceSettings.clear();
    m_muxSettings.clear();
}

void PMMuxMgr::ClearChoices()
{
    s_muxChoices.clear();
    s_instanceChoices.clear();
}
