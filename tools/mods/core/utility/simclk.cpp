/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2013,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
using namespace std;

#include "core/include/xp.h"
#include "core/include/cmdline.h"
#include "core/include/simclk.h"
#include "core/include/fileholder.h"

const char* SimClk::InstGroup::s_InstNames[] = {"RM_ALLOC", "RM_FREE", "RM_CTRL"};

void SimClk::InstGroup::Aggregate()
{
    m_Clks = 0;
    vector<Inst*>::const_iterator it = m_InstList.begin();
    for (; it != m_InstList.end(); ++it)
    {
        m_Clks += (*it)->Clks();
        m_wallClks += (*it)->WallClks();
    }
}

const char* SimClk::Event::s_EventNames[] =
{
    "MODS_MAIN",
    "PLATFORM_INIT",
    "GPU_INIT",
    "MDIAG_RUN",
    "TEST_SETUP",
    "TEST_RUN",
    "TEST_CLEAN",
    "T3D_ALLOC_CH",
    "T3D_SETUP_SURF",
    "T3D_FINISH_SETUP",
    "T3D_RUN_TRACEOP",
    "T3D_CHECK_PASSFAIL",
    "[TRACE_PROFILE]"
};

SimClk::SimManager* SimClk::SimManager::pInstance = 0;

void SimClk::SimManager::Init(bool check, const string &path, UINT32 threshold)
{
    if (pInstance != 0)
    {
        Printf(Tee::PriWarn, "SimClk::SimManager::Init found nonzero pInstance!\n");
    }
    static SimManager s_Instance(check, path, threshold);
    pInstance = &s_Instance;
}

SimClk::SimManager* SimClk::SimManager::GetInstance()
{
    return pInstance;
}

SimClk::SimManager::SimManager(bool gild, const string &path, UINT32 threshold):
                              m_IsGild(gild), m_StoredAndVerified(false), m_SkipTraceProfile(true),
                              m_FileName(path), m_Threshold(threshold),
                              m_DevID(0)
{
}

SimClk::SimManager::~SimManager()
{
    StoreAndVerify();

    vector<Event*>::iterator it = m_Events.begin();
    for (; it != m_Events.end(); ++it)
    {
        delete *it;
    }

}

void SimClk::SimManager::StoreAndVerify()
{
    if (!m_StoredAndVerified)
    {
        if (m_IsGild)
        {
            Store();
        }
        else
        {
           if (OK != m_Verif.Load(m_FileName.c_str()))
           {
               Printf(Tee::PriError, "cannot load profile file %s\n",m_FileName.c_str());
           }

           if (OK != Verify())
           {
               assert(!"SimClk:Verificaiton failed!");
           }
           Printf(Tee::PriNormal, "SimClk: Verification passed!\n");
        }
        m_StoredAndVerified = true;
    }
}

void SimClk::SimManager::AddEvent(Event *pEvent)
{
    UINT32 indent = 0;
    vector<Event*>::iterator it = m_Events.begin();
    for (; it != m_Events.end(); ++it)
    {
        if ((*it)->InEvent())
        {
            ++indent;
        }
    }
    pEvent->SetIndent(indent);
    m_Events.push_back(pEvent);
}

SimClk::Inst* SimClk::SimManager::Track(SimClk::InstGroup::Type type)
{
    Inst *pInst = new Inst;
    bool bInstAdded = false;
    for (unsigned int i = 0; i < m_Events.size(); ++i)
    {
        if (m_Events[i]->InEvent())
        {
            m_Events[i]->AddInst(type, pInst);
            bInstAdded = true;
        }
    }
    if (!bInstAdded)
    {
        delete pInst;
        return nullptr;
    }
    return pInst;
}

void SimClk::SimManager::Store() const
{
    FileHolder file;
    file.Open(m_FileName.c_str(), "w");
    FILE *fp = file.GetFile();
    if (fp == NULL)
    {
        Printf(Tee::PriError, "Failed to open %s to write\n", m_FileName.c_str());
        return;
    }

    vector<Event*>::const_iterator it = m_Events.begin();
    for (; it != m_Events.end(); ++it)
    {
        if ((*it)->Clks() != 0)
        {
            (*it)->Dump(fp);
        }
    }
}

RC SimClk::SimManager::Verify() const
{
    RC rc;
    vector<Event*>::const_iterator it = m_Events.begin();
    for (; it != m_Events.end(); ++it)
    {
        if ((*it)->Clks() != 0)
        {
            CHECK_RC((*it)->Verify(m_Verif, m_Threshold));
        }
    }
    return OK;
}

RC SimClk::ClkVerif::Load(const char* path)
{
    if (path == nullptr)
    {
        Printf(Tee::PriError, "SimClk::ClkVerif::Load failed with empty path!\n");
        return RC::CANNOT_OPEN_FILE;
    }
    FileHolder file;
    RC rc;
    CHECK_RC(file.Open(path, "r"));
    FILE *fp = file.GetFile();
    char key[200];
    char bufLine[1000];
    UINT64 value;
    while(fgets(bufLine, 1000, fp))
    {
        if (!sscanf(bufLine, "%s = %llu", key, &value))
        {
             return RC::CANNOT_PARSE_FILE;
        }
        m_ClkRepo.insert(make_pair((char*)key, value));
    }
    return OK;
}

RC SimClk::ClkVerif::Verify(const string& key, UINT64 value, UINT32 threshold) const
{
    map<string, UINT64>::const_iterator it = m_ClkRepo.find(key);
    if (it != m_ClkRepo.end())
    {
        UINT64 threshold64 = it->second * threshold/100;
        if (threshold64 < 200)
        {
            threshold64 = 200;
        }
        if (it->second + threshold64 < value)
        {
            Printf(Tee::PriError, "SimClk: Event:%s check failed,"
                   "expect = %llu,actually = %llu, threshhold = %llu\n",
                    key.c_str(), it->second, value, threshold64);
            return RC::GOLDEN_VALUE_MISCOMPARE;
        }
        return OK;
    }
    Printf(Tee::PriError, "SimClk: Event:%s not found in profile\n",key.c_str());
    return RC::GOLDEN_VALUE_NOT_FOUND;
}

void SimClk::Event::Print( )
{
    Printf(Tee::PriNormal, "%s, simclk = %llu ns, wallclk = %f s\n",
        m_Name.c_str(), m_Inst.Clks(), m_Inst.WallClks());
    Printf(Tee::PriNormal, "%s,", m_Name.c_str());
    for (int i = 0; i < InstGroup::TYPE_NUM; ++i)
    {
         Printf(Tee::PriNormal, " %s[%u](simclk = %llu ns, wallclk = %f s)",
            (m_InstSet[i]->Name()).c_str(),m_InstSet[i]->Times(), m_InstSet[i]->Clks(),
            m_InstSet[i]->WallClks());
         if (i != InstGroup::TYPE_NUM -1)
         {
             Printf(Tee::PriNormal, ", ");
         }
    }
    Printf(Tee::PriNormal, "\n\n");
}

void SimClk::Event::Dump(FILE *fp) const
{
    string indentStr = "";
    for (UINT32 i = 0; i < GetIndent(); ++i)
    {
        indentStr += "    ";
    }
    fprintf(fp,"%s%s = %llu\n",indentStr.c_str(), m_Name.c_str(),m_Inst.Clks());
    for (int i = 0; i < InstGroup::TYPE_NUM; ++i)
    {
        fprintf(fp, "%s%s:%s = %llu\n",
        indentStr.c_str(),m_Name.c_str(), (m_InstSet[i]->Name()).c_str(), m_InstSet[i]->Clks());
    }
}

RC SimClk::Event::Verify(const SimClk::ClkVerif& verif, UINT32 threshold) const
{
    RC rc;
    CHECK_RC(verif.Verify(m_Name, m_Inst.Clks(), threshold));
    for (int i = 0; i < InstGroup::TYPE_NUM; ++i)
    {
        CHECK_RC(verif.Verify(m_Name +':'+ m_InstSet[i]->Name(), m_InstSet[i]->Clks(), threshold));
    }
    return OK;
}

void SimClk::Inst::Start()
{
    if (m_StartClk != 0)
    {
        Printf(Tee::PriNormal, "SimClk::Inst::Start starting with nonzero start clock=%llu!\n", m_StartClk);
    }
    if (Platform::IsInitialized())
        m_StartClk = Platform::GetTimeNS();
    time(&m_StartWallClk);
}

void SimClk::Inst::Stop()
{
    UINT64 nowClks = Platform::GetTimeNS();
    if (nowClks < m_StartClk)
    {
        Printf(Tee::PriWarn, "SimClk::Inst::Stop current time (= %llu) is earlier than start time (= %llu)!\n", nowClks, m_StartClk);
    }

    m_Clks = nowClks - m_StartClk;
    m_StartClk = 0;

    time_t stopWallClk;
    time(&stopWallClk);
    m_WallClks = difftime(stopWallClk, m_StartWallClk);
}

SimClk::EventWrapper::EventWrapper(SimClk::Event::Type type): m_pEvent(0)
{
    if (SimManager *pM = SimManager::GetInstance())
    {
        if ((type != SimClk::Event::T3D_TRACE_PROFILE) || (pM->IsSkipTraceProfile() == false))
        {
            string name = Event::s_EventNames[type];
            name = name + ':' + Utility::ToString(pM->GetEventCount(Event::s_EventNames[type]));
            InitEvent(name);
            pM->IncreaseEventCount(Event::s_EventNames[type]);
        }
    }
}

SimClk::EventWrapper::EventWrapper(SimClk::Event::Type type, const string &subname): m_pEvent(0)
{
    if (SimManager *pM = SimManager::GetInstance())
    {
        if ((type != SimClk::Event::T3D_TRACE_PROFILE) || (pM->IsSkipTraceProfile() == false))
        {
            string name = Event::s_EventNames[type];
            name = name + ':' + Utility::ToString(pM->GetEventCount(Event::s_EventNames[type])) + ':' + subname;
            InitEvent(name);
            pM->IncreaseEventCount(Event::s_EventNames[type]);
        }
    }
}

void SimClk::EventWrapper::InitEvent(const string &name)
{
    if (m_pEvent)
    {
        Printf(Tee::PriWarn, "SimClk::EventWrapper::InitEvent(name=%s) called with non-null pEvent=%s!\n", name.c_str(), m_pEvent->Name().c_str());
    }
    m_pEvent = new Event(name);
    SimManager::GetInstance()->AddEvent(m_pEvent);
}

SimClk::EventWrapper::~EventWrapper()
{
    if (m_pEvent)
    {
        m_pEvent->Stop();
        m_pEvent->Print();
    }
}
