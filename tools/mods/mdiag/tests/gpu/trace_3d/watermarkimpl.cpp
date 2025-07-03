/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "watermarkimpl.h"
#include "core/include/argread.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "stdlib.h"
#include "errno.h"
#include "mdiag/sysspec.h"
#include "mdiag/subctx.h"
#include "core/include/regexp.h"
#include "tracetsg.h"
#include "trace_3d.h"

map<WaterMarkScopeGetInfoKey, BasePtr> WaterMarkImpl::m_GlobalWaterMarkScopeGetInfos;

WaterMarkImpl::WaterMarkImpl
(
    const ArgReader * reader,
    Trace3DTest * test
):
    m_Test(test),
    m_Params(reader)
{
}

//--------------------------------------------------
//! \brief Free all resource
WaterMarkImpl::~WaterMarkImpl()
{
   for(vector<WaterMark *>::iterator it = m_WaterMarkTsgs.begin();
        it != m_WaterMarkTsgs.end(); ++it)
    {
        delete (*it);
    }
}

//-----------------------------------------------------
//! \brief The main parse loop
RC WaterMarkImpl::ParseWaterMark()
{
    RC rc;
    UINT32 count = 0;
    const char * keyword = NULL;

    if(m_Params->ParamPresent("-subctx_veid_cwdslot_watermark"))
    {
        count = m_Params->ParamPresent("-subctx_veid_cwdslot_watermark");
        keyword = "-subctx_veid_cwdslot_watermark";
    }
    else if(m_Params->ParamPresent("-subctx_cwdslot_watermark"))
    {
        count = m_Params->ParamPresent("-subctx_cwdslot_watermark");
        keyword = "-subctx_cwdslot_watermark";
    }
    else
    {
        ErrPrintf("%s: must have -subctx_veid_cwdslot_watermark or -subctx_cwdslot_watermark.\n", __FUNCTION__);
        return RC::ILWALID_FILE_FORMAT;
    }

    for(UINT32 i = 0; i < count; ++i)
    {
        string tsgName;

        tsgName = m_Params->ParamNStr(keyword, i, 0);
        m_WaterMarkString = m_Params->ParamNStr(keyword, i, 1);

        if(AddWaterMarkTsg(tsgName))
        {
            CHECK_RC(DoParseWaterMark());
        }
    }

    return rc;
}

//----------------------------------------------------------
//! \brief Really parse the wartermark command line to fetch
//! the water mark value per subctx
RC WaterMarkImpl::DoParseWaterMark()
{
    RC rc;

    vector<string> subctx_watermarks;
    size_t pos;
    WaterMark * pWaterMark = m_WaterMarkTsgs.back();

    while((pos = m_WaterMarkString.find(",", 0)) != string::npos)
    {
        string subctx_watermark = m_WaterMarkString.substr(0, pos);

        if(subctx_watermark.empty())
        {
            ErrPrintf("GpuTrace::TokenzieWaterMark: command line -subctx_cwdslot_watermark error. Need a specified <subctx name>:<water mark>.\n");
            return RC::ILWALID_FILE_FORMAT;
        }
        subctx_watermarks.push_back(subctx_watermark);
        m_WaterMarkString = m_WaterMarkString.substr(pos + 1);
    }

    if(!m_WaterMarkString.empty())
        subctx_watermarks.push_back(m_WaterMarkString);

    if(subctx_watermarks.empty())
    {
        ErrPrintf("GpuTrace::TokenzieWaterMark: command line -subctx_cwdslot_watermark error. Need at least one <subctx name>:<water mark>.\n");
        return RC::ILWALID_FILE_FORMAT;
    }

    if(pWaterMark->GetType() == WaterMark::GLOBAL)
    {
        ErrPrintf("%s: Invalid water mark synax.\n", __FUNCTION__);
        return RC::ILWALID_FILE_FORMAT;
    }

    for(vector<string>::const_iterator it = subctx_watermarks.begin();
        it != subctx_watermarks.end(); ++it)
    {
        if(pWaterMark->GetType() == WaterMark::WATER_MARK_VEID)
        {
            if((pos = (*it).find(":", 0)) != string::npos)
            {
                string veidStr = (*it).substr(0, pos);
                UINT32 veid = static_cast<UINT32>(strtol(veidStr.c_str(), NULL, 0));
                string waterMarkStr = (*it).substr(pos + 1);
                UINT32 waterMark = static_cast<UINT32>(strtol(waterMarkStr.c_str(), NULL, 0));

                pWaterMark->GetWaterMarks()->insert( WaterMark::WaterMarkObject(veid, waterMark) );
            }
            else
            {
                ErrPrintf("%s: command line -subctx_veid_cwdslot_watermark error. Need valid <subctx name>:<water mark>.\n",
                          __FUNCTION__);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if(pWaterMark->GetType() == WaterMark::WATER_MARK_SEQUENCE)
        {
            string waterMarkStr = *it;
            if((pos = waterMarkStr.find(":", 0)) != string::npos)
            {
                ErrPrintf("%s: command line -subctx_cwdslot_watermark error. Please check the paramter. The Synax is "
                          "-subctx_cwdslot_watermark <tsg name> <watermark_hex_32bitdata>.\n",
                          __FUNCTION__);
                return RC::ILWALID_FILE_FORMAT;
            }

            UINT32 waterMark = static_cast<UINT32>(strtol(waterMarkStr.c_str(), NULL, 0));
            UINT32 subctxId = pWaterMark->GetSubCtxId();

            pWaterMark->GetWaterMarks()->insert( WaterMark::WaterMarkObject(subctxId,  waterMark) );
        }
    }

    return rc;
}

//---------------------------------------------------------------------------
//! \brief check whether new watermark scope has been added.
//! If it has been added, skipped it.
bool WaterMarkImpl::AddWaterMarkTsg(const string & tsgName)
{
    RC rc;
    for(vector<WaterMark *>::iterator ppWaterMarkTsg = m_WaterMarkTsgs.begin();
        ppWaterMarkTsg != m_WaterMarkTsgs.end(); ++ppWaterMarkTsg)
    {
        if((*ppWaterMarkTsg)->GetTsgName() == tsgName)
            return false;
    }

    WaterMark * pWaterMarkTsg = new WaterMark();

    pWaterMarkTsg->SetTsgName(tsgName);
    if(m_Params->ParamPresent("-subctx_cwdslot_watermark"))
    {
        pWaterMarkTsg->SetType(WaterMark::WATER_MARK_SEQUENCE);
        m_WaterMarkTsgs.push_back(pWaterMarkTsg);
    }
    else if(m_Params->ParamPresent("-subctx_veid_cwdslot_watermark"))
    {
        pWaterMarkTsg->SetType(WaterMark::WATER_MARK_VEID);
        m_WaterMarkTsgs.push_back(pWaterMarkTsg);
    }
    else
        return false;

    return true;
}

WaterMarkImpl::WaterMark::WaterMark()
{
    m_Type = WaterMark::GLOBAL;
    m_SubCtxId = 0;
    m_BasePtr = ~0x0;
    m_WaterMarkGetIndex = 0;
}

RC WaterMarkImpl::GetWaterMark
(
    shared_ptr<SubCtx> pSubCtx,
    UINT32 * pWaterMark
) const
{
    RC rc;
    WaterMark * pWaterMarkTsg;
    UINT32 getIndex;

    CHECK_RC(QueryWaterMarkInfo(pSubCtx->GetTestId(),
                pSubCtx->GetTraceTsg()->GetName(),
                &pWaterMarkTsg,
                &getIndex));

    if (NULL == pWaterMarkTsg)
    {
        *pWaterMark = UNINITIALIZED_WATERMARK_VALUE;
        return rc;
    }

    *pWaterMark = pWaterMarkTsg->GetWaterMarkValue(pSubCtx, getIndex);

    // get index add 1
    pWaterMarkTsg->UpdateWaterMarkGetIndex();

    return rc;
}

UINT32 WaterMarkImpl::WaterMark::GetWaterMarkValue
(
    shared_ptr<SubCtx> pSubCtx,
    UINT32 getIndex
)
{
    MASSERT(pSubCtx.get());

    if(m_Type == WaterMark::WATER_MARK_VEID)
    {
        if(!pSubCtx->IsSpecifiedVeid())
        {
            ErrPrintf("%s: You need to specify the veid per subctx when use -subctx_veid_cwdslot_watermark.\n",
                      __FUNCTION__);
            return RC::ILWALID_FILE_FORMAT;
        }

        for(map<UINT32, UINT32>::iterator it = m_WaterMarks.begin();
                it != m_WaterMarks.end(); ++it)
        {
            if((*it).first == pSubCtx->GetSpecifiedVeid())
            {
                return  (*it).second;
            }
        }
    }
    else if(m_Type == WaterMark::WATER_MARK_SEQUENCE)
    {
        for(map<UINT32, UINT32>::iterator it = m_WaterMarks.begin();
                it != m_WaterMarks.end(); ++it)
        {
            if((*it).first == getIndex)
            {
                return (*it).second;
            }
        }
    }
    else
    {
        ErrPrintf("%s: No validate subctx sequence id in -subctx_cwdslot_watermark. or "
                  "No validate subctx veid in -subctx_veid_cwdslot_watermark",
                  __FUNCTION__);
        return ~0x0;
    }

    // Get the default value ~0x0 if doesn't found the getIndex in this scope
    return ~0x0;
}
//---------------------------------------------------------------
//! \brief use the regex to match all available tsg
//! Note: * match all tsg
WaterMarkImpl::WaterMark *  WaterMarkImpl::GetWaterMarkTsg
(
    const string & tsgName
)const
{
    unique_ptr<RegExp> regExp(new RegExp());

    for(vector<WaterMark *>::const_iterator it = m_WaterMarkTsgs.begin();
            it != m_WaterMarkTsgs.end(); ++it)
    {
        WaterMark * pWaterMarkTsg = *it;
        regExp->Set(pWaterMarkTsg->GetTsgName());
        if(regExp->Matches(tsgName))
        {
            return pWaterMarkTsg;
        }
    }

    return NULL;
}

RC WaterMarkImpl::QueryWaterMarkInfo
(
    UINT32 testId,
    const string tsgName,
    WaterMark ** ppWaterMarkTsg,
    UINT32 * pWaterMarkGetIndex
) const
{
    RC rc;

    // 0. Get the WaterMarkScope base index
    const WaterMarkScopeGetInfoKey key = std::make_pair(testId, tsgName);
    map<WaterMarkScopeGetInfoKey, BasePtr>::iterator ppWaterMarkScope = m_GlobalWaterMarkScopeGetInfos.find(key);

    if(ppWaterMarkScope == m_GlobalWaterMarkScopeGetInfos.end())
    {
        //can't find the specified testId sequence id vector, initialize it firstly
        m_GlobalWaterMarkScopeGetInfos[key] = 0;
    }
    else
    {
        m_GlobalWaterMarkScopeGetInfos[key]++;
    }

    // 1. Get WarkMark tsg object
    WaterMark * pWaterMarkTsg = GetWaterMarkTsg(tsgName);
    if(pWaterMarkTsg == NULL)
    {
        DebugPrintf("%s: Can't get the available waterMark Scope. Please check the watermark configure and tsgName %s if needed.\n",
                    __FUNCTION__, tsgName.c_str());
        *ppWaterMarkTsg = NULL;
        return rc;
    }

    // 2. Get the BaseIndex in this scope
    // Make sure just set m_BasePtr per Scope one time
    LWGpuResource * pGpu = m_Test->GetGpuResource();
    if(pWaterMarkTsg->GetBasePtr() == static_cast<UINT32>(~0x0))
    {
        if(!pGpu->GetLwgpuArgs()->ParamPresent("-share_subctx_cfg"))
            pWaterMarkTsg->SetBasePtr(0);
        else
            pWaterMarkTsg->SetBasePtr(m_GlobalWaterMarkScopeGetInfos[key]);
    }

    *ppWaterMarkTsg = pWaterMarkTsg;
    *pWaterMarkGetIndex = pWaterMarkTsg->GetWaterMarkGetIndex();

    return rc;
}
