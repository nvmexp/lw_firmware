/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pe_configurefile.h"
#include "parsefile.h"
#include "mdiag/sysspec.h"
#include "mdiag/subctx.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "tracetsg.h"
#include "stdlib.h"
#include "errno.h"
#include "trace_3d.h"
#include "gpu/include/floorsweepimpl.h"
#include "ctrl/ctrl9067.h"
#include <algorithm>
#include "gputrace.h"
using namespace std;

UINT32 PEConfigureFile::m_GlobalTsgIndex = 0;
map<SmcEngine *, PEConfigureFile::SmcInfo> PEConfigureFile::m_SmcTsgInfos;
map<PEScopeGetInfoKey, PEConfigureFile::ScopeGetInfo> PEConfigureFile::m_GlobalPEScopeGetInfos;

PEConfigureFile::PEConfigureFile
(
    const ArgReader * reader,
    Trace3DTest * test
):
    m_Test(test),
    m_params(reader),
    m_LocalTsgIndex(0)
{
    m_LineNumber = 1;
}

PEConfigureFile::~PEConfigureFile()
{
    for(deque<Scope *>::iterator iter = m_PEValue.begin();
        iter != m_PEValue.end(); ++iter)
    {
        delete (*iter);
    }
}

RC PEConfigureFile::ReadHeader(const char * fileName, UINT32 timeoutMs)
{
    RC rc;

    // Initialize the floorSweeping tpc value
    m_FloorSweepingTpcCount = GetFloorSweepingTpcCount();
    CHECK_RC(SetFloorSweepingTpcMask());

    // Sanity check for -share_subctx_tsg_oer_smc.
    // Just allow same -partition_table for the same SMC
    if (m_params->ParamPresent("-partition_table") && 
            m_params->ParamPresent("-share_subctx_cfg_per_smc"))
    {
        SmcEngine * pSmcEngine = m_Test->GetSmcEngine();
        if (pSmcEngine == nullptr)
        {
            ErrPrintf("When you enable -share_subctx_cfg_per_smc, you need under the SMC mode\n");
            return RC::SOFTWARE_ERROR;
        }

        if (m_SmcTsgInfos.find(pSmcEngine) == m_SmcTsgInfos.end())
        {
            SmcInfo smcInfo;
            smcInfo.fileName = fileName;
            smcInfo.sharedTsgIndexPerSmc = 0;
            m_SmcTsgInfos[pSmcEngine] = smcInfo;
        }
        else
        {
            SmcInfo smcInfo = m_SmcTsgInfos[pSmcEngine];
            if (smcInfo.fileName != fileName)
            {
                ErrPrintf("You need to use the same partition table when those two trace are sharing the -partition_table resource "
                          "under the -share_subctx_cfg_per_smc.\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }
    // Read file into Buffer or clean up it
    m_pParseFile.reset(new ParseFile(m_Test, m_params));

    CHECK_RC(m_pParseFile->ReadHeader(fileName, timeoutMs));

    swap(m_Tokens, m_pParseFile->GetTokens());

    rc = ParsePE();

    return rc;
}

RC PEConfigureFile::ParsePE()
{
    RC rc;

    while (m_Tokens.size())
    {
        string tok = m_Tokens.front();
        m_Tokens.pop_front();

        if (ParseFile::s_NewLine == tok)
        {
            m_LineNumber++;
            continue;
        }

        if (0 == strcmp("TSG_PARTITION_BEGIN", tok.c_str()))      rc = ParseTSG_PARTITION_BEGIN();
        else if (0 == strcmp("TSG_PARTITION_END", tok.c_str()))         rc = ParseTSG_PARTITION_END();
        else if (m_pScope->GetType() != Scope::GLOBAL)
        {
              rc = ParseTSG_PARTITION_INDEX(tok);
        }
        else
        {
            ErrPrintf("Unexcepted token \"%s\" at beginning of line.\n", tok.c_str());
            return RC::ILWALID_FILE_FORMAT;
        }

        if(OK != rc)
            break;

        // Every statement should be followed by a newline.
        tok = const_cast<char *>(m_Tokens.front());
        m_Tokens.pop_front();

        while (tok != ParseFile::s_NewLine)
        {
            WarnPrintf("Unexpected token \"%s\" at the end of line %d.\n", tok.c_str(), m_LineNumber);
            tok = const_cast<char *>(m_Tokens.front());
            m_Tokens.pop_front();
        }

        m_LineNumber++;
    }

    return rc;
}

RC PEConfigureFile::ParseTSG_PARTITION_BEGIN()
{
    RC rc;
    string tok;
    PARTITION_MODE mode = PEConfigureFile::NOT_SET;
    UINT32 tsgIndex = ~0x0;
    string label;
    string tok1 = m_Tokens.front();
    bool isSubCtxVeid = false;

    m_pScope = new Scope();

    while (OK == ParseString(&tok))
    {
        if (tok == "STATIC_PARTITION")
        {
            mode = PEConfigureFile::STATIC;
        }
        else if (tok == "DYNAMIC_PARTITION")
        {
            mode = PEConfigureFile::DYNAMIC;
        }
        else if (tok == "NONE_PARTITION")
        {
            mode = PEConfigureFile::NONE;
        }
        else if (tok == "TSG_DESCLARED_SEQ")
        {
            if(OK != ParseUINT32(&tsgIndex))
            {
                ErrPrintf("PE need a TSG id at line %d.\n", m_LineNumber);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (tok == "LABEL")
        {
            if(OK != ParseString(&label))
            {
                ErrPrintf("%s: PE need a label name at line %d.\n", __FUNCTION__, m_LineNumber);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (tok == "SUBCTX_VEID")
        {
            isSubCtxVeid = true;
        }
    }

    if(mode == PEConfigureFile::NOT_SET)
    {
        ErrPrintf("PE need to specify STATIC_PARTITION, DYNAMIC_PARTITION or NONE_PARTITION "
            "at line %d.\n", m_LineNumber);
        return RC::ILWALID_FILE_FORMAT;
    }

    if(m_pScope->GetType() != Scope::GLOBAL)
    {
        ErrPrintf("PE table doesn't support nest at line %d.\n", m_LineNumber);
        return RC::ILWALID_FILE_FORMAT;
    }

    if(label != m_params->ParamStr("-pe_label", ""))
    {
        m_IsValidLabel = false;
        WarnPrintf("label %s at partittion table doesn't match the command line %s.\n",
                   label.c_str(), m_params->ParamStr("-pe_label"));
    }
    else
        m_IsValidLabel = true;

    if(isSubCtxVeid)
        m_pScope->SetType(Scope::SUBCTX_VEID);
    else
        m_pScope->SetType(Scope::SUBCTX_DECLARED_SEQ);
    m_pScope->SetTsgIndex(tsgIndex);
    m_pScope->SetPartitionMode(mode);
    m_pScope->SetLabel(label);

    return rc;
}

RC PEConfigureFile::ParseTSG_PARTITION_END()
{
    RC rc;

    if(m_pScope->GetType() == Scope::GLOBAL)
    {
        ErrPrintf("PE table error at line %d.\n", m_LineNumber);
        return RC::ILWALID_FILE_FORMAT;
    }

    // bypass those scope which lable doesn't match the command line -pe_label
    if(!m_IsValidLabel)
        return rc;

    m_PEValue.push_back(m_pScope);

    return rc;
}

RC PEConfigureFile::ParseTSG_PARTITION_INDEX(string tok)
{
    RC rc;
    vector<UINT32> tpcMask;
    UINT32 maxTpcCount = 0;
    UINT32 maxSingletonTpcGpcCount = 0;
    UINT32 maxPluralTpcGpcCount = 0;
    UINT32 index;
    size_t pos;

    if((pos = tok.find(":", 0)) != string::npos)
    {
        if(m_pScope->GetType() != Scope::SUBCTX_VEID)
        {
            ErrPrintf("%s You may lost the key word SUBCTX_VEID at the partition talbe configure.\n", __FUNCTION__);
            return RC::ILWALID_FILE_FORMAT;
        }
        string veid = tok.substr(0,pos);
        tok = tok.substr(pos + 1);
        index = std::atoi(veid.c_str());
    }
    else
    {
        if(m_pScope->GetType() != Scope::SUBCTX_DECLARED_SEQ)
        {
            ErrPrintf("%s The partition mode %s you use is wrong.\n",
                      __FUNCTION__, GetPartitionTypeModuleName(m_pScope->GetType()));
            return RC::ILWALID_FILE_FORMAT;
        }
        index = m_pScope->GetPEPutIndex();
    }

    CHECK_RC(ParseIndexValue(tok, tpcMask));

    while (OK == ParseString(&tok))
    {
        if(tok == "MAX_TPC_COUNT")
        {
            if(OK != ParseUINT32(&maxTpcCount))
            {
                ErrPrintf("MAX_TPC_COUNT needs one specified number at line %d.\n",
                    m_LineNumber);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (tok == "MAX_SINGLETON_TPC_GPC_COUNT")
        {
            if(OK != ParseUINT32(&maxSingletonTpcGpcCount))
            {
                ErrPrintf("MAX_SINGLETON_TPC_GPC_COUNT needs one specified number at line %d.\n",
                    m_LineNumber);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
        else if (tok == "MAX_PLURAL_TPC_GPC_COUNT")
        {
            if(OK != ParseUINT32(&maxPluralTpcGpcCount))
            {
                ErrPrintf("MAX_PLURAL_TPC_GPC_COUNT needs one specified number at line %d.\n",
                    m_LineNumber);
                return RC::ILWALID_FILE_FORMAT;
            }
        }
    }

    // check scope type
    if(m_pScope->GetType() == Scope::GLOBAL)
    {
        ErrPrintf("PE table error at line %d.\n", m_LineNumber);
        return RC::ILWALID_FILE_FORMAT;
    }

    CHECK_RC(PostFloorSweepingTpcMask(tpcMask));

    m_pScope->GetTPCMasks()[index] = tpcMask;
    m_pScope->SetResourceMaxCount(Scope::Resource::TPC, index, maxTpcCount);
    m_pScope->SetResourceMaxCount(Scope::Resource::SINGLETON_TPC_GPC, index, maxSingletonTpcGpcCount);
    m_pScope->SetResourceMaxCount(Scope::Resource::PLURAL_TPC_GPC, index, maxPluralTpcGpcCount);

    return rc;
}

RC PEConfigureFile::ParseIndexValue(string tok, vector<UINT32> & tpcMask)
{
    RC rc;
    vector<string> tpcMaskTokens;
    UINT32 base = 16;

    if (tok.find("0x", 0) != string::npos)
    {
        tok.erase(0, 2);
    }
    else
    {
        base = 10;
    }

    if (Utility::Tokenizer(tok, "_" , &tpcMaskTokens) != OK)
    {
        ErrPrintf("Invalid partition table format in specifying TPC mask(%s)\n",
            tok.c_str());
        return RC::ILWALID_FILE_FORMAT;
    }

    for (auto & tpcMaskStr : tpcMaskTokens)
    {
        tpcMask.push_back(strtol(tpcMaskStr.c_str(), NULL, base));
        DebugPrintf("PEConfig: TpcMask %u- 0x%x\n", tpcMask.size()-1, tpcMask.back()); 
    }

    reverse(tpcMask.begin(), tpcMask.end());

    return rc;
}

RC PEConfigureFile::PostFloorSweepingTpcMask(vector<UINT32> & tpcMask)
{
    RC rc;

    for(UINT32 i = 0; i < tpcMask.size(); ++i)
    {
        tpcMask[i] &= m_FloorSweepingTpcMask[i];
    }

    return rc;
}

UINT32 PEConfigureFile::GetFloorSweepingTpcCount()
{
    GpuDevice * pGpudev = m_Test->GetGpuResource()->GetGpuDevice();
    UINT32 numSubDev = pGpudev->GetNumSubdevices();
    InfoPrintf("SRIOV: %s support subdevice count %d.\n", __FUNCTION__, numSubDev);
    MASSERT(numSubDev == 1);

    return m_Test->GetTPCCount(0);
}

UINT32 PEConfigureFile::GetTpcMaskCount(const vector<UINT32> * pTpcMask)
{
    UINT32 tpcCount = 0;

    for(UINT32 i = 0; i < pTpcMask->size(); ++i)
    {
        UINT32 tpcMask = (*pTpcMask)[i];
        while(tpcMask != 0)
        {
            if((tpcMask & 0x1) == 0x1)
            {
                tpcCount += 1;
            }

            tpcMask = tpcMask >> 1;
        }
    }

    return tpcCount;
}

RC PEConfigureFile::SetFloorSweepingTpcMask()
{
    RC rc;

    // callwlate the tpcMask after floor sweeping has been taken into consideration
    // take the floorsweep into the vector m_FloorSweepingTpcMask
    UINT32 paddingMask = m_FloorSweepingTpcCount % 16;
    UINT32 noPaddingIndex = m_FloorSweepingTpcCount / 16;
    for(UINT32 i = 0; i < LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX / 16; ++i)
    {
        if(i < noPaddingIndex)
            m_FloorSweepingTpcMask.push_back(~0x0);
        else if (i == noPaddingIndex)
            m_FloorSweepingTpcMask.push_back((1 << paddingMask) - 1);
        else
            m_FloorSweepingTpcMask.push_back(0x0);
    }

    // check whether the partition table after floor sweeping is still valid
    for(vector<UINT32>::const_iterator it = m_FloorSweepingTpcMask.begin();
            it != m_FloorSweepingTpcMask.end(); ++it)
    {
        if(*it != 0) return rc;
    }

    ErrPrintf("%s: After floor sweeping the partition table setting value is 0. "
              "Please check the partition table.\n", __FUNCTION__);
    return RC::ILWALID_FILE_FORMAT;
}

const vector<UINT32> * PEConfigureFile::Scope::GetTPCMasksValue
(
    shared_ptr<SubCtx> pSubCtx,
    UINT32 PEGetIndex
)
{
    MASSERT(pSubCtx.get());

    // find the matched subctx id in the partition table and return the TPC value
    // Note: if partition table entry less than the subctx define in the hdr,
    //       automatically set post floor sweeping value(all 1)
    if (m_Type == Scope::SUBCTX_DECLARED_SEQ)
    {
        if(PEGetIndex < m_TPCMasks.size())
            return &(m_TPCMasks[PEGetIndex]);
    }
    else if (m_Type == Scope::SUBCTX_VEID)
    {
        if(!pSubCtx->IsSpecifiedVeid())
            MASSERT(!"PEConfigureFile::GetTPCMasksValue: You need to specify the veid"
                "for the subctx when you are using partition talbe SUBCTX_VEID mods.\n");

        TPCMasks::iterator it = m_TPCMasks.find(pSubCtx->GetSpecifiedVeid());
        if (it != m_TPCMasks.end())
            return &(it->second);
    }
    else
    {
        MASSERT(!"PEConfigureFile::Scope::GetTPCMasksValue: Can't found the available"
                 " value in the configure file.");
    }

    return nullptr;
}

UINT32 PEConfigureFile::Scope::GetResourceMaxCount
(
    shared_ptr<SubCtx> pSubCtx,
    UINT32 PEGetIndex,
    Resource resource
)
{
    UINT32 index = 0;

    if (m_Type == Scope::SUBCTX_DECLARED_SEQ)
    {
        index = PEGetIndex;
    }
    else if(m_Type == Scope::SUBCTX_VEID)
    {
        if (!pSubCtx->IsSpecifiedVeid())
        {
            ErrPrintf("VEID needs to be specified for the subctx %s when SUBCTX_VEID is used in the partition table\n",
                pSubCtx->GetName().c_str());
            MASSERT(0);
        }
        index = pSubCtx->GetSpecifiedVeid();
    }
    else
    {
        ErrPrintf("Illegal partiton table type %s. Please double check the partition table.\n",
            PEConfigureFile::GetPartitionTypeModuleName(m_Type));
        MASSERT(!"Unsupported PE configuration file mode.");
        return 0;
    }

    return GetResourceMaxCount(resource, index);
}

void PEConfigureFile::Scope::SetResourceMaxCount
(
    Resource resource, 
    UINT32 index, 
    UINT32 count
)
{
    if (resource == Scope::Resource::TPC)
    {
        m_MaxTpcCounts[index] = count;
    }
    else if (resource == Scope::Resource::SINGLETON_TPC_GPC)
    {
        m_MaxSingletonTpcGpcCounts[index] = count;
    }
    else if (resource == Scope::Resource::PLURAL_TPC_GPC)
    {
        m_MaxPluralTpcGpcCounts[index] = count;
    }
    else
    {
        MASSERT(!"Unknown resource passed to SetResourceMaxCount\n");
    }
}

UINT32 PEConfigureFile::Scope::GetResourceMaxCount
(
    Resource resource, 
    UINT32 index
)
{
    string resourceStr;

    if (resource == Scope::Resource::TPC)
    {
        resourceStr = "TPC";
        if (m_MaxTpcCounts.find(index) != m_MaxTpcCounts.end())
        {
            return m_MaxTpcCounts[index];
        }
    }
    else if (resource == Scope::Resource::SINGLETON_TPC_GPC)
    {
        resourceStr = "SINGLETON_TPC_GPC";
        if (m_MaxSingletonTpcGpcCounts.find(index) != m_MaxSingletonTpcGpcCounts.end())
        {
            return m_MaxSingletonTpcGpcCounts[index];
        }
    }
    else if (resource == Scope::Resource::PLURAL_TPC_GPC)
    {
        resourceStr = "PLURAL_TPC_GPC";
        if (m_MaxPluralTpcGpcCounts.find(index) != m_MaxPluralTpcGpcCounts.end())
        {
            return m_MaxPluralTpcGpcCounts[index];
        }
    }
    else
    {
        MASSERT(!"Unknown resource passed to GetResourceMaxCount\n");
    }

    WarnPrintf("Unable to find the max count for the resource %s with index %d in the configure file.\n", 
        resourceStr.c_str(), index);
    return 0;
}

RC PEConfigureFile::GetTpcInfoPerSubCtx
(
    shared_ptr<SubCtx> pSubCtx,
    vector<UINT32> * pTpcMaskValues,
    UINT32 * pMaxTpcCount,
    UINT32 * pMaxSingletonTpcGpcCount,
    UINT32 * pMaxPluralTpcGpcCount,
    PEConfigureFile::PARTITION_MODE * pPartitionMode
)
{
    RC rc;

    MASSERT(pSubCtx.get() && pTpcMaskValues && pMaxTpcCount);

    Scope * pTsgScope;
    UINT32 PEGetIndex;

    // 1. Query the PE table get the scope information
    CHECK_RC(QuerryPETableInfo(pSubCtx->GetTestId(), pSubCtx->GetTraceTsg()->GetName(),
                      &pTsgScope, &PEGetIndex));

    // 2. Read the tpc mask values
    const vector<UINT32> * pTemp = pTsgScope->GetTPCMasksValue(pSubCtx, PEGetIndex);
    // Note: If can't found the subctx in the partition table set the post floorSweeping
    //      mask value(all 1)
    if (!pTemp)
        *pTpcMaskValues = m_FloorSweepingTpcMask;
    else
        *pTpcMaskValues = *pTemp;

    // 3. Read the max tpc count if it is dyanmic mode
    PEConfigureFile::PARTITION_MODE mode = pTsgScope->GetPartitionMode();
    if (mode == PEConfigureFile::DYNAMIC)
    {
        *pMaxTpcCount = pTsgScope->GetResourceMaxCount(pSubCtx, PEGetIndex, Scope::Resource::TPC);
        *pMaxSingletonTpcGpcCount = pTsgScope->GetResourceMaxCount(pSubCtx, PEGetIndex, Scope::Resource::SINGLETON_TPC_GPC);
        *pMaxPluralTpcGpcCount = pTsgScope->GetResourceMaxCount(pSubCtx, PEGetIndex, Scope::Resource::PLURAL_TPC_GPC);
        // Not found the valid nonthrottledC value, 0 always to be a illegal value
        // 1. Inset the after floorsweeping value count as the nonthrottledC value if
        // this entry doesn't exist
        // 2. Inset the tpcMaskCount value as the nonthrottledC value if this this entry
        // exist but the max tpc count doesn't set
        if(*pMaxTpcCount == 0)
        {
            if(!pTemp)
                *pMaxTpcCount = GetFloorSweepingTpcCount();
            else
                *pMaxTpcCount = GetTpcMaskCount(pTemp);
        }
    }
    else if (mode == PEConfigureFile::STATIC ||
             mode == PEConfigureFile::NONE)
    {
        *pMaxTpcCount = GetFloorSweepingTpcCount();
    }
    else
    {
       ErrPrintf("%s: Not support mods %s.\n", __FUNCTION__, GetPartitionModeName(mode));
       MASSERT(0);
    }

    // 4. get the partiton mode
    *pPartitionMode = mode;

    // 5. Update the m_PEGetIndex
    pTsgScope->UpdatePEGetIndex();

    return rc;
}

RC PEConfigureFile::QuerryPETableInfo
(
    UINT32 testId,
    const string tsgName,
    Scope ** ppTsgScope,
    UINT32 * pPEGetIndex
)
{
    RC rc;
    LWGpuResource * pGpu = m_Test->GetGpuResource();

    // 0. Get the tsgScpoe struct
    const PEScopeGetInfoKey key = std::make_pair(testId, tsgName);

    if (m_params->ParamPresent("-share_subctx_cfg_per_smc"))
    {
        SmcEngine * pSmcEngine = m_Test->GetSmcEngine();
        MASSERT(m_SmcTsgInfos.find(pSmcEngine) != m_SmcTsgInfos.end());
        // The first time to init the tsgIndex and peGetIndex
        if (m_SmcTsgInfos[pSmcEngine].scopeGetInfo.find(key) == 
                m_SmcTsgInfos[pSmcEngine].scopeGetInfo.end())
        {
            ScopeGetInfo scopeGetInfo;
            scopeGetInfo.m_TsgIndex = m_SmcTsgInfos[pSmcEngine].sharedTsgIndexPerSmc++;
            scopeGetInfo.m_PEGetIndex = 0;
            m_SmcTsgInfos[pSmcEngine].scopeGetInfo[key] = scopeGetInfo;
        }
        else
        {
            m_SmcTsgInfos[pSmcEngine].scopeGetInfo[key].m_PEGetIndex++;
        }
    }
    else if(m_GlobalPEScopeGetInfos.find(key) == m_GlobalPEScopeGetInfos.end())
    {
        //can't find the specified testId sequence id vector, initialize it firstly
        ScopeGetInfo scopeGetInfo;
        if(pGpu->GetLwgpuArgs()->ParamPresent("-share_subctx_cfg"))
            scopeGetInfo.m_TsgIndex = m_GlobalTsgIndex++;
        else
            scopeGetInfo.m_TsgIndex = m_LocalTsgIndex++;
        
        scopeGetInfo.m_PEGetIndex = 0;
        m_GlobalPEScopeGetInfos[key] = scopeGetInfo;
    }
    else
    {
        m_GlobalPEScopeGetInfos[key].m_PEGetIndex ++;
    }

    // 1. Get the tsg scope accordint to the tsgid inforamtion
    Scope * pScope = GetTsgScope(m_GlobalPEScopeGetInfos[key].m_TsgIndex);
    if(!pScope)
    {
        ErrPrintf("%s: Can't get the available tsgScope via tsg index 0x%x.\n",
                  __FUNCTION__, m_GlobalPEScopeGetInfos[key].m_TsgIndex);
        MASSERT(0);
    }

    // 2. Get PEIndex in this scope
    // Make sure just set m_BasePtr per Scope one time
    if(pScope->GetBasePtr() == static_cast<UINT32>(~0x0))
    {
        if(pGpu->GetLwgpuArgs()->ParamPresent("-share_subctx_cfg"))
            pScope->SetBasePtr(m_GlobalPEScopeGetInfos[key].m_PEGetIndex);
        else
            pScope->SetBasePtr(0);
    }

    *ppTsgScope = pScope;
    *pPEGetIndex = pScope->GetPEGetIndex();

    return rc;
}

PEConfigureFile::Scope * PEConfigureFile::GetTsgScope
(
    UINT32 tsgIndex
)
{
    for(deque<Scope *>::iterator iter = m_PEValue.begin(); iter != m_PEValue.end(); ++iter)
    {
        UINT32 scopeTsgIndex = (*iter)->GetTsgIndex();

        if(tsgIndex == scopeTsgIndex)
        {
            // 4. find the matched subctx id and matched label in the partition table and return the TPC value
            if(m_params->ParamPresent("-pe_label"))
            {
                string label = m_params->ParamStr("-pe_label", "");
                if(label == (*iter)->GetLabel())
                {
                    return (*iter);
                }
            }
            else
            {
                // 4. find the matched subctx id in the partition table and return the TPC value
                return (*iter);
            }
        }
    }

    InfoPrintf("%s: Can't found the available TSG in the configure file.\n", __FUNCTION__);
    return nullptr;
}

RC PEConfigureFile::ParseUINT32(UINT32 * pUINT32)
{
    // Someday, we could allow arithmetic expressions here.
    const char * tok = m_Tokens.front();
    char * p;
    errno = 0;
    *pUINT32 = strtoul(tok, &p, 0);
    // Either 'did not move forward' or 'did not hit the end' will be treated as failure.
    if ((p == tok) ||
            (p && *p != 0) ||
            (ERANGE == errno)) // Catching out of range
    {
        // First character wasn't recognized by strtoul as part of an integer.
        return RC::ILWALID_FILE_FORMAT;
    }

    // Only consume the token if we got a valid integer.
    m_Tokens.pop_front();
    return OK;
}

RC PEConfigureFile::ParseString(string * pstr)
{
    if (ParseFile::s_NewLine == m_Tokens.front())
        return RC::ILWALID_FILE_FORMAT;

    *pstr = m_Tokens.front();
    m_Tokens.pop_front();
    return OK;
}

PEConfigureFile::Scope::Scope()
{
    m_Type = Scope::GLOBAL;
    m_TsgIndex = ~0x0;
    m_TPCMasks.clear();
    m_PEPutIndex = 0;
    m_PEGetIndex = 0;
    m_BasePtr = ~0x0;
}

const char * PEConfigureFile::GetPartitionModeName
(
    PEConfigureFile::PARTITION_MODE partitionMode
)
{
    const char * str = "";

    switch(partitionMode)
    {
        case PEConfigureFile::NONE:
            str = "NONE";
            break;
        case  PEConfigureFile::STATIC:
            str = "STATIC";
            break;
        case  PEConfigureFile::DYNAMIC:
            str = "DYNAMIC";
            break;
        case  PEConfigureFile::NOT_SET:
            str = "NOT_SET";
            break;
        default:
            InfoPrintf("%s: Ilwalidate Partition mode.\n", __FUNCTION__);
            break;
    }

    return str;
}

const char * PEConfigureFile::GetPartitionTypeModuleName
(
    Scope::Type type
)
{
    const char * str = "Unknown Partition Type";

    switch(type)
    {
        case Scope::SUBCTX_VEID:
            str = "SUBCTX_VEID";
            break;
        case Scope::SUBCTX_DECLARED_SEQ:
            str = "SUBCTX_DECLARED_SEQ";
            break;
        default:
            break;
    }

    return str;
}
