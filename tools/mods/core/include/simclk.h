/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SIMCLK_H
#define INCLUDED_SIMCLK_H

#include <vector>
#include <map>

#include <stdio.h>
#include <time.h>
#include "massert.h"
#include "platform.h"
#include "tee.h"

namespace SimClk
{
    /* @class Single oclwrence of an SimClk tracking objects
     */
    class Inst
    {
    public:
        Inst() : Inst(true) {}
        explicit Inst(bool startNow)
        {
            if (startNow)
            {
                Start();
            }
        }

        void Start();
        void Stop();
        bool HasStarted() const {return m_StartClk > 0;}
        UINT64 Clks() const {return m_Clks;}
        double WallClks() const {return m_WallClks;}
        void IncRef() {++m_ref;}
        void DecRef() {--m_ref;}
        UINT32 GetRef() const {return m_ref;}
    private:
        UINT64 m_Clks         = 0;
        UINT64 m_StartClk     = 0;
        double m_WallClks     = 0.0;
        time_t m_StartWallClk = 0;
        UINT32 m_ref          = 0;
    };

    /* @class A group of a same kind of Insts
    */
    class InstGroup
    {
    public:
        enum Type{RM_ALLOC, RM_FREE, RM_CTRL, TYPE_NUM};
        static const char* s_InstNames[];
        explicit InstGroup(Type type):m_Name(s_InstNames[type]),m_Clks(0),m_wallClks(0){}
        ~InstGroup()
        {
            vector<Inst*>::const_iterator it = m_InstList.begin();
            for (; it != m_InstList.end(); ++it)
            {
                (*it)->DecRef();
                if ((*it)->GetRef() == 0) delete *it;
            }
        }

        void AddInst(Inst* pInst)
        {
            pInst->IncRef();
            m_InstList.push_back(pInst);
        }
        void Aggregate();
        //void Print()const;
        UINT64 Clks()const {return m_Clks;}
        double WallClks() const {return m_wallClks;}
        UINT32 Times()const {return static_cast<UINT32>(m_InstList.size());}
        const string& Name() const {return m_Name;}
    private:
        string m_Name;
        UINT64 m_Clks;
        double m_wallClks;
        vector<Inst*> m_InstList;
    };

    class ClkVerif
    {
    public:
        RC Load(const char* path);
        RC Verify(const string& key, UINT64 value, UINT32 threshold)const;
    private:
        map<string, UINT64> m_ClkRepo;
    };

    /* @class Class of event instances worth profiling.
     */
    class Event
    {
    public:
        enum Type{
                     MODS_MAIN,
                     PF_INIT,
                     GPU_INIT,
                     MDIAG_RUN,
                     TEST_SETUP,
                     TEST_RUN,
                     TEST_CLEAN,
                     T3D_ALLOC_CH,
                     T3D_SETUP_SURF,
                     T3D_FINISH_SETUP,
                     T3D_TRACEOP,
                     T3D_PASSFAIL,
                     T3D_TRACE_PROFILE,
                     TYPE_NUM
                 };

        static const char* s_EventNames[];
        explicit Event(const string &name):m_Name(name),m_Indentation(0),m_Inst(true)
        {
            Printf(Tee::PriHigh, "SIMCLK: Event %s start\n", m_Name.c_str());
            for (unsigned int i = 0; i < InstGroup::TYPE_NUM; ++i)
            {
                 m_InstSet.push_back(new InstGroup(static_cast<InstGroup::Type>(i)));
            }
        }

        ~Event()
        {
            for (unsigned int i = 0; i < m_InstSet.size(); ++i)
            {
                 delete m_InstSet[i];
            }
        }

        void Stop()
        {
            Printf(Tee::PriHigh, "SIMCLK: Event %s stop\n", m_Name.c_str());
            m_Inst.Stop();
            Aggregate();
        }

        bool InEvent()const
        {
            return m_Inst.HasStarted();
        }

        void AddInst(InstGroup::Type type, Inst *pInst)
        {
            m_InstSet[type]->AddInst(pInst);
        }

        const string&  Name() const {return m_Name;}

        void Aggregate()
        {
            for (unsigned int i = 0; i < m_InstSet.size(); ++i)
            {
                 m_InstSet[i]->Aggregate();
            }
        }

        UINT64 Clks() const {return m_Inst.Clks();}
        void Print();
        void Dump(FILE* fp) const;
        RC Verify(const ClkVerif& verif, UINT32 threshold) const;

        void SetIndent(UINT32 indent){m_Indentation = indent;}
        UINT32 GetIndent() const {return m_Indentation;}

    private:
        Event(const Event&);
        string m_Name;
        UINT32 m_Indentation;
        Inst   m_Inst;
        vector<InstGroup*> m_InstSet;
    };

    /* @class RAII class for Event
    */
    class EventWrapper
    {
    public:
        explicit EventWrapper(Event::Type type);
        EventWrapper(Event::Type type, const string &name);
        ~EventWrapper();
    private:
        void InitEvent(const string &name);
        Event *m_pEvent;
    };

    /*@class manage simclk events, Singleton
    */
    class SimManager{
    public:
        static void Init(bool gild, const string &path, UINT32 threshold);
        static SimManager* GetInstance();
        ~SimManager();
        void StoreAndVerify();
        void AddEvent(Event *pEvent);
        Inst* Track(InstGroup::Type type);
        RC Verify() const;

        void SetDevID(UINT32 id) {m_DevID = id;}
        UINT32 GetDevID()const {return m_DevID;}
        //skip profile event by default
        void SkipTraceProfile(bool flag = true) {m_SkipTraceProfile = flag;}
        bool IsSkipTraceProfile() const {return m_SkipTraceProfile;}
        bool IsStoredAndVerified() const {return m_StoredAndVerified;}
        int GetEventCount(const string& s) {return m_EventCount[s];}
        void IncreaseEventCount(const string& s) { m_EventCount[s]++;}
    private:
        static SimManager *pInstance;
        SimManager(bool check, const string &path, UINT32 threshold);
        void Store() const;
        bool m_IsGild;
        bool m_StoredAndVerified;
        bool m_SkipTraceProfile;
        string m_FileName;
        UINT32 m_Threshold;
        UINT32 m_DevID;
        ClkVerif m_Verif;
        vector<Event*> m_Events;
        map<string, UINT32> m_EventCount;
    };

    /* @class RAII class for Inst
     */
    class InstWrapper
    {
    public:
        explicit InstWrapper(InstGroup::Type type):m_pInst(0)
        {
            if (SimClk::SimManager *pM = SimClk::SimManager::GetInstance())
            {
                m_pInst =  pM->Track(type);
            }
        }

        ~InstWrapper()
        {
            if (m_pInst)
            {
                m_pInst->Stop();
            }

            m_pInst = NULL;
        }

    private:
        Inst* m_pInst;
    };
}; // SimClk

template<class F,class A1>
UINT32 rm_forward(SimClk::InstGroup::Type type, F f, A1 arg1)
{
    SimClk::InstWrapper temp(type);
    return f(arg1);
}

template<class F,class A1>
void void_rm_forward(SimClk::InstGroup::Type type, F f, A1 arg1)
{
    SimClk::InstWrapper temp(type);
    f(arg1);
}

#endif
