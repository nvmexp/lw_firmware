/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/ism.h"
#include "core/include/massert.h"
#include "lwmisc.h"     // DRF_NUM64
#include "core/include/script.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/jscript.h"
#include <algorithm>

namespace
{
    void DisableIsms(Ism *pIsm) { pIsm->EnableISMs(false); }
}

namespace IsmSpeedo
{
    const char * SpeedoPartTypeToString(IsmSpeedo::PART_TYPES speedoType)
    {
        #define PART_TYPE_ENUM_STRING(x)  case x: return #x
        switch (speedoType)
        { 
            PART_TYPE_ENUM_STRING(COMP);
            PART_TYPE_ENUM_STRING(BIN);
            PART_TYPE_ENUM_STRING(METAL);
            PART_TYPE_ENUM_STRING(MINI);
            PART_TYPE_ENUM_STRING(TSOSC);
            PART_TYPE_ENUM_STRING(VNSENSE);
            PART_TYPE_ENUM_STRING(NMEAS);
            PART_TYPE_ENUM_STRING(HOLD);
            PART_TYPE_ENUM_STRING(AGING);
            PART_TYPE_ENUM_STRING(BIN_AGING);
            PART_TYPE_ENUM_STRING(AGING2);
            PART_TYPE_ENUM_STRING(CPM);
            PART_TYPE_ENUM_STRING(IMON);
            PART_TYPE_ENUM_STRING(ALL);
            PART_TYPE_ENUM_STRING(CTRL);
            PART_TYPE_ENUM_STRING(ISINK);
        }
        return "unknown";
    }
}

/* static */ const char * Ism::IsmEnumToString(IsmTypes ism)
{
    #define ISM_ENUM_STRING(x)  case x: return #x
    switch (ism)
    { 
        ISM_ENUM_STRING(LW_ISM_none);
        ISM_ENUM_STRING(LW_ISM_ctrl);
        ISM_ENUM_STRING(LW_ISM_MINI_1clk);
        ISM_ENUM_STRING(LW_ISM_MINI_2clk);
        ISM_ENUM_STRING(LW_ISM_MINI_1clk_dbg);
        ISM_ENUM_STRING(LW_ISM_ROSC_comp);
        ISM_ENUM_STRING(LW_ISM_ROSC_bin);
        ISM_ENUM_STRING(LW_ISM_ROSC_bin_metal);
        ISM_ENUM_STRING(LW_ISM_VNSENSE_ddly);
        ISM_ENUM_STRING(LW_ISM_TSOSC_a);
        ISM_ENUM_STRING(LW_ISM_MINI_1clk_no_ext_ro);
        ISM_ENUM_STRING(LW_ISM_MINI_2clk_dbg);
        ISM_ENUM_STRING(LW_ISM_MINI_1clk_no_ext_ro_dbg);
        ISM_ENUM_STRING(LW_ISM_MINI_2clk_no_ext_ro);
        ISM_ENUM_STRING(LW_ISM_NMEAS_v2);
        ISM_ENUM_STRING(LW_ISM_HOLD);
        ISM_ENUM_STRING(LW_ISM_ctrl_v2);
        ISM_ENUM_STRING(LW_ISM_NMEAS_v3);
        ISM_ENUM_STRING(LW_ISM_ROSC_bin_aging);
        ISM_ENUM_STRING(LW_ISM_TSOSC_a2);
        ISM_ENUM_STRING(LW_ISM_ctrl_v3);
        ISM_ENUM_STRING(LW_ISM_aging);
        ISM_ENUM_STRING(LW_ISM_MINI_2clk_v2);
        ISM_ENUM_STRING(LW_ISM_MINI_2clk_dbg_v2);
        ISM_ENUM_STRING(LW_ISM_TSOSC_a3);
        ISM_ENUM_STRING(LW_ISM_ROSC_bin_aging_v1);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite);
        ISM_ENUM_STRING(LW_ISM_ctrl_v4);
        ISM_ENUM_STRING(LW_ISM_aging_v1);
        ISM_ENUM_STRING(LW_ISM_cpm);
        ISM_ENUM_STRING(LW_ISM_ROSC_bin_v1);
        ISM_ENUM_STRING(LW_ISM_MINI_2clk_dbg_v3);
        ISM_ENUM_STRING(LW_ISM_NMEAS_v4);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite_v2);
        ISM_ENUM_STRING(LW_ISM_aging_v2);
        ISM_ENUM_STRING(LW_ISM_cpm_v1);
        ISM_ENUM_STRING(LW_ISM_aging_v3);
        ISM_ENUM_STRING(LW_ISM_aging_v4);
        ISM_ENUM_STRING(LW_ISM_TSOSC_a4);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite_v3);
        ISM_ENUM_STRING(LW_ISM_ROSC_bin_v2);
        ISM_ENUM_STRING(LW_ISM_ROSC_comp_v1);
        ISM_ENUM_STRING(LW_ISM_cpm_v2);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite_v4);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite_v5);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite_v6);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite_v7);
        ISM_ENUM_STRING(LW_ISM_cpm_v3);
        ISM_ENUM_STRING(LW_ISM_ROSC_bin_v3);
        ISM_ENUM_STRING(LW_ISM_cpm_v4);
        ISM_ENUM_STRING(LW_ISM_cpm_v5);
        ISM_ENUM_STRING(LW_ISM_cpm_v6);
        ISM_ENUM_STRING(LW_ISM_ROSC_bin_v4);
        ISM_ENUM_STRING(LW_ISM_ROSC_comp_v2);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite_v8);
        ISM_ENUM_STRING(LW_ISM_NMEAS_lite_v9);
        ISM_ENUM_STRING(LW_ISM_imon);
        ISM_ENUM_STRING(LW_ISM_imon_v2);
        ISM_ENUM_STRING(LW_ISM_lwrrent_sink);
        ISM_ENUM_STRING(LW_ISM_unknown);
    }
    return "unknown";
}

Ism::Ism(vector<IsmChain> *pTable)
    :m_Table(*pTable), m_TableIsValid(false),
     m_IsmLocDurClkCycles(0xffffffff)
{
}

//--------------------------------------------------------------------------------------------------
// Copy the src bits into the destArray for each ISM that is enabled via the ismMask.
// Note: The assumption is that the ismMask is only referencing ISMs of the same type.
// ie if the ismMask has enabled both MINI & NMEAS then that would be an error.
// This assumes that the scrArray contains the bits for a single macro type starting at bit 0.
// and destArray is a collection of bits for a JTag chain.
RC Ism::CopyBits
(
    const IsmChain *pChain      // the chain configuration
    ,vector<UINT32> &destArray  // Vector of raw bits for this chain.
    ,const vector<UINT64> &srcArray   // Vector of raw bits for this ISM type
    ,const bitset<Ism::maxISMsPerChain>& ismMask             // a mask of the ISMs you want to powerup.
)
{
    for (size_t idx = 0; idx < pChain->ISMs.size(); idx++)
    {
        if (ismMask[idx])
        {
            // Copy the bits to the chain.
            Utility::CopyBits(srcArray,
                     destArray,
                     0,  //srcBitOffset
                     pChain->ISMs[idx].Offset, // dstBitOffset
                     GetISMSize(pChain->ISMs[idx].Type)); //numBits
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
// Configure the chain of ISMs using the IsmMask(bit 0 = first ISM at offset 0,
// bit 1 = next ism, etc.)
RC Ism::ConfigureISMChain
(
    const IsmChain *pChain      // the chain configuration
    ,vector<UINT32> &jtagArray  // Vector of raw bits for this chain.
    ,bool bEnable               // Enable any independant ISM in the chain.
    ,vector<IsmInfo>*pSettings  //
    ,const bitset<Ism::maxISMsPerChain>& ismMask // a mask of the ISMs you want to powerup.
    ,UINT32 durClkCycles        // how long you want the experiment to run
    ,UINT32 flags               // Special handling flags
)
{
    RC rc;
    UINT32 idx = 0;
    UINT32 bits = 0;

    // initialize the array
    jtagArray.assign(jtagArray.size(),0);

    MASSERT(pSettings && !pSettings->empty());

    while ( (bits < pChain->Hdr.Size) && (idx < pChain->ISMs.size()))
    {
        // we don't want to simply increment bits by size just incase there
        // are missing ISMs in the chain.
        bits = pChain->ISMs[idx].Offset +
               GetISMSize(pChain->ISMs[idx].Type);

        // For each ISM in this chain
        // If we want to read this speedo enable IDDQ (0=powerUp, 1=powerDn)
        const UINT32 iddq = !ismMask.test(idx);
        const UINT32 enable = bEnable ? 1 : 0;

        IsmInfo info;
        CHECK_RC(GetISMSettings(pSettings, pChain, idx, &info));

        vector<UINT64> jtagArgs;
        CHECK_RC(ConfigureISMBits(jtagArgs,
                                info,
                                pChain->ISMs[idx].Type,
                                iddq,
                                enable,
                                durClkCycles,
                                flags));

        // Add the bits to the chain.
        Utility::CopyBits(jtagArgs,  //src vector
                 jtagArray, //dst vector
                 0,         //srcBitOffset
                 pChain->ISMs[idx].Offset, //dstBitOffset
                 GetISMSize(pChain->ISMs[idx].Type)); // numBits

        idx++; // next ISM
    }
    return OK;
}

IsmSpeedo::PART_TYPES Ism::GetPartType(IsmTypes ismType)
{
    MASSERT(ismType < LW_ISM_unknown);
    switch (ismType)
    { 
        case LW_ISM_none                   :
            MASSERT(!"No PART_TYPE for LW_ISM_none");
            return IsmSpeedo::ALL;  // its better than nothing.
            
        case LW_ISM_ctrl:
        case LW_ISM_ctrl_v2:
        case LW_ISM_ctrl_v3:
        case LW_ISM_ctrl_v4:
            return IsmSpeedo::CTRL;

        case LW_ISM_MINI_1clk:
        case LW_ISM_MINI_2clk:
        case LW_ISM_MINI_1clk_no_ext_ro:
        case LW_ISM_MINI_2clk_dbg:
        case LW_ISM_MINI_1clk_no_ext_ro_dbg:
        case LW_ISM_MINI_2clk_no_ext_ro:
        case LW_ISM_MINI_1clk_dbg:
        case LW_ISM_MINI_2clk_v2:
        case LW_ISM_MINI_2clk_dbg_v2:
        case LW_ISM_MINI_2clk_dbg_v3:
            return IsmSpeedo::MINI;
            
        case LW_ISM_ROSC_comp:
        case LW_ISM_ROSC_comp_v1:
        case LW_ISM_ROSC_comp_v2:
            return IsmSpeedo::COMP;
            
        case LW_ISM_ROSC_bin:
        case LW_ISM_ROSC_bin_v1:
        case LW_ISM_ROSC_bin_v2:
        case LW_ISM_ROSC_bin_v3:
        case LW_ISM_ROSC_bin_v4:
            return IsmSpeedo::BIN;
            
        case LW_ISM_ROSC_bin_metal:
            return IsmSpeedo::METAL;
            
        case LW_ISM_VNSENSE_ddly:
            return IsmSpeedo::VNSENSE;
            
        case LW_ISM_TSOSC_a:
        case LW_ISM_TSOSC_a2:
        case LW_ISM_TSOSC_a3:
        case LW_ISM_TSOSC_a4:
            return IsmSpeedo::TSOSC;
            
        case LW_ISM_NMEAS_v2:
        case LW_ISM_NMEAS_v3:
        case LW_ISM_NMEAS_v4:
        case LW_ISM_NMEAS_lite:
        case LW_ISM_NMEAS_lite_v2 :
        case LW_ISM_NMEAS_lite_v3 :
        case LW_ISM_NMEAS_lite_v4 :
        case LW_ISM_NMEAS_lite_v5 :
        case LW_ISM_NMEAS_lite_v6 :
        case LW_ISM_NMEAS_lite_v7 :
        case LW_ISM_NMEAS_lite_v8 :
        case LW_ISM_NMEAS_lite_v9 :
            return IsmSpeedo::NMEAS;
            
        case LW_ISM_HOLD:
            return IsmSpeedo::HOLD;

        case LW_ISM_ROSC_bin_aging:
        case LW_ISM_ROSC_bin_aging_v1:
            return IsmSpeedo::BIN_AGING;
                
        case LW_ISM_aging:
        case LW_ISM_aging_v1:
        case LW_ISM_aging_v2:
        case LW_ISM_aging_v3:
        case LW_ISM_aging_v4:
            return IsmSpeedo::AGING;
                
        case LW_ISM_cpm:
        case LW_ISM_cpm_v1:
        case LW_ISM_cpm_v2:
        case LW_ISM_cpm_v3:
        case LW_ISM_cpm_v4:
        case LW_ISM_cpm_v5:
        case LW_ISM_cpm_v6:
            return IsmSpeedo::CPM;

        case LW_ISM_imon:
            return IsmSpeedo::IMON;

        default:
            MASSERT(!"Unknown NN_Ism_type");
            return IsmSpeedo::ALL;  // its better than nothing.
    }
}
//------------------------------------------------------------------------------
// Return the chains with ISMs that have a given SpeedoType. Each GPU
// may have a different set of ISMs that fall into a give speedo type,
// but the mapping from IsmTypes to SpeedoType is expected to stay the
// same.
//
RC Ism::GetChains
(
    vector<IsmChainFilter> *pIsmFilter, //!< (out) list of chains
    PART_TYPES speedoType,         //!< (in) SpeedoType to search for
    INT32  chainId,                //!< (in) chain id to search for (-1 for all)
    INT32  chiplet,                //!< (in) chiplet to search for (-1 for all)
    UINT32 chainFlags              //!< (in) flags to use with the chain
)
{
    RC rc;
    switch (speedoType)
    {
        case BIN:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_v1, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_v2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_v3, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_v4, chainId, chiplet, chainFlags));
            break;

        case AGING:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_aging, chainId, chiplet, chainFlags)); //$
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_aging_v1, chainId, chiplet, chainFlags)); //$
            break;

        case BIN_AGING:// user wants both bin & aging at the same sample rate.
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_v1, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_v2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_v3, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_v4, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_aging, chainId, chiplet, chainFlags)); //$
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_aging_v1, chainId, chiplet, chainFlags)); //$
            break;

        case COMP:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_comp, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_comp_v1, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_comp_v2, chainId, chiplet, chainFlags));
            break;

        case METAL:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ROSC_bin_metal, chainId, chiplet, chainFlags)); //$
            break;

        case MINI:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_1clk, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_2clk, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_2clk_v2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_2clk_no_ext_ro, chainId, chiplet, chainFlags)); //$
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_1clk_dbg, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_2clk_dbg, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_2clk_dbg_v2, chainId, chiplet, chainFlags)); //$
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_2clk_dbg_v3, chainId, chiplet, chainFlags)); //$
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_1clk_no_ext_ro, chainId, chiplet, chainFlags)); //$
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_MINI_1clk_no_ext_ro_dbg, chainId, chiplet, chainFlags)); //$
            break;

        case VNSENSE:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_VNSENSE_ddly, chainId, chiplet, chainFlags));
            break;

        case TSOSC:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_TSOSC_a,  chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_TSOSC_a2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_TSOSC_a3, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_TSOSC_a4, chainId, chiplet, chainFlags));
            break;

        case NMEAS: // NMEAS & NMEAS_lite are mutually exclusive
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_v2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_v3, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_v4, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite_v2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite_v3, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite_v4, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite_v5, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite_v6, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite_v7, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite_v8, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_NMEAS_lite_v9, chainId, chiplet, chainFlags));
            break;

        case HOLD:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_HOLD, chainId, chiplet, chainFlags));
            break;

        case AGING2:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_aging, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_aging_v1, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_aging_v2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_aging_v3, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_aging_v4, chainId, chiplet, chainFlags));
            break;

        case CPM:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_cpm, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_cpm_v1, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_cpm_v2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_cpm_v3, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_cpm_v4, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_cpm_v5, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_cpm_v6, chainId, chiplet, chainFlags));
            
        case IMON:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_imon, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_imon_v2, chainId, chiplet, chainFlags));
            break;

        case CTRL:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ctrl, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ctrl_v2, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ctrl_v3, chainId, chiplet, chainFlags));
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_ctrl_v4, chainId, chiplet, chainFlags));
            break;
            
        case ISINK:
            CHECK_RC(GetISMChains(pIsmFilter, LW_ISM_lwrrent_sink, chainId, chiplet, chainFlags));
            break;

        default:
            MASSERT(!"GetChains() invalid speedoType!");
            rc = RC::SOFTWARE_ERROR;
            break;
    }
    return rc;
}

//------------------------------------------------------------------------------
// Return a pointer to a pointer to the requested ISM chain.
const Ism::IsmChain * Ism::GetISMChain(UINT32 chainId)
{
    for (size_t i=0; i < m_Table.size(); i++)
    {
        if (m_Table[i].Hdr.InstrId == chainId)
            return &m_Table[i];
    }
    return NULL;    // error can't find this chain
}

//------------------------------------------------------------------------------
RC Ism::GetISMChainCounts
(
    const IsmChain *pChain     // the chain configuration
    ,vector<UINT32> &jtagArray // Vector of raw bits for this chain.
    ,vector<UINT32> *pValues   // Where to put the counts
    ,vector<IsmInfo>*pInfo     // unique info to identify this ISM.
    ,vector<IsmInfo>*pSettings //
    ,const bitset<Ism::maxISMsPerChain>& ismMask // a mask of the ISMs count you want.
    ,UINT32     durClkCycles   // duration in Jtag Clk cycles
)
{
    UINT32 idx = 0;
    UINT32 bits = 0;
    RC rc;
    // protect agains floating point exception.
    if (durClkCycles == 0)
        return RC::BAD_PARAMETER;

    while ( (bits < pChain->Hdr.Size) && (idx < pChain->ISMs.size()))
    {
        // we dont want to simply increment bits by size just incase there
        // are missing ISMs in the chain.
        bits = pChain->ISMs[idx].Offset +
               GetISMSize(pChain->ISMs[idx].Type);

        IsmInfo info(1, 
                     pChain->Hdr.InstrId,
                     pChain->Hdr.Chiplet,
                     pChain->ISMs[idx].Offset,
                     IsmSpeedo::NO_OSCILLATOR,
                     IsmSpeedo::NO_OUTPUT_DIVIDER,
                     IsmSpeedo::NO_MODE,
                     0, 0, 0, 0, 0,
                     pChain->ISMs[idx].Flags);

        CHECK_RC(GetISMSettings(pSettings, pChain, idx, &info));

        vector<UINT64> ismBits;
        ismBits.resize(CallwlateVectorSize(pChain->ISMs[idx].Type, 64), 0);
        // extract this macro's bits into a separate 64bit vector for processing.
        Utility::CopyBits(jtagArray,
                 ismBits,
                 pChain->ISMs[idx].Offset,
                 0,
                 GetISMSize(pChain->ISMs[idx].Type));
        // extract the value
        UINT64 rawCount = ExtractISMRawCount(pChain->ISMs[idx].Type,ismBits);
        // Colwert to frequency
        info.count = (UINT32)rawCount;
        if( info.clkKHz != 0)
        {
            IsmSpeedo::PART_TYPES partType = GetPartType(pChain->ISMs[idx].Type);
            INT32 outDiv = GetISMOutDivForFreqCalc(partType, info.outDiv);

            // make the units in 100Hz
            rawCount = (rawCount * ((1 << outDiv) * info.clkKHz) * 10) / durClkCycles;
        }

        if (pChain->ISMs[idx].Type == LW_ISM_aging_v3 ||
            pChain->ISMs[idx].Type == LW_ISM_aging_v4)
        {
            info.dcdCount = ExtractISMRawDcdCount(pChain->ISMs[idx].Type, ismBits);
        }

        if (ismMask[idx])
        {
            pValues->push_back((UINT32)rawCount);
            pInfo->push_back(info);
        }

        idx++; // next ISM
    }
    return OK;
}

//------------------------------------------------------------------------------
// Populate the IsmChainFilter with pointers to the requested class of ISM.
//
RC Ism::GetISMChains
(
    vector<IsmChainFilter> *pIsmFilters,
    UINT32                  ismType,
    INT32                   chainId,
    INT32                   chiplet,
    UINT32                  chainFlags)
{
    if (m_Table.empty())
    {
        return OK;
    }
    for (size_t i=0; i < m_Table.size(); i++)
    {
        UINT32 bits = 0;
        UINT32 idx = 0;
        bitset<Ism::maxISMsPerChain> ismMask;
        // If specified the chain/chiplet must match
        if ((chainId != -1) && (m_Table[i].Hdr.InstrId != (UINT32)chainId))
            continue;

        if ((chiplet != -1) && (m_Table[i].Hdr.Chiplet != (UINT32)chiplet))
            continue;

        // If this chain isnt a selected chain sub type, skip it
        const UINT32 chainTypeFlags = m_Table[i].Hdr.Flags & CHAIN_SUBTYPE_MASK;
        if (!(chainTypeFlags & chainFlags))
            continue;

        //WAR BUGxxx The chain sizes are not correct in Kepler GPUs.
        // So test for ISMs[] size too to prevent asserts.
        while ((bits < m_Table[i].Hdr.Size) && (idx < m_Table[i].ISMs.size()))
        {
            // we dont want to simply increment bits by size just incase there
            // are missing ISMs in the chain.
            bits = m_Table[i].ISMs[idx].Offset + GetISMSize(m_Table[i].ISMs[idx].Type);

            if (m_Table[i].ISMs[idx].Type == ismType)
            {
                ismMask[idx] = 1;
            }

            idx++; // next ISM
        }
        if (ismMask.count())
        {
            bool bFound = false;
            for (UINT32 j=0; j < pIsmFilters->size(); j++)
            {
                if((*pIsmFilters)[j].pChain == &m_Table[i])
                {
                    (*pIsmFilters)[j].ismMask |= ismMask;
                    bFound = true;
                    break;
                }
            }

            if(!bFound)
            {
                IsmChainFilter filter(&m_Table[i],ismType,ismMask);
                pIsmFilters->push_back(filter);
            }
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
// Return the number of ISMs for a given general type
UINT32 Ism::GetNumISMs(PART_TYPES partType)
{
    UINT32 count = 0;
    vector<IsmChainFilter> ChainFilter;
    RC rc = GetChains(&ChainFilter, partType, -1, -1, ~0);

    if (OK == rc)
    {
        for (UINT32 idx = 0; idx < ChainFilter.size(); idx++)
        {
            count += static_cast<UINT32>(ChainFilter[idx].ismMask.count());
        }
    }
    return count;
}

//-------------------------------------------------------------------------------------------------
// GetISMCtrlChain(): Default implementation
// Search through the ISMTable and return the 1st (and only) oclwrance of the chain that has the
// PRIMARY bit set in the Flags field.
// Note: On GH100+ there are several ISM controllers where each controller manages a subset of the
//       NMEAS_LITE_* chains. However there is still a single PRIMARY controller that will control
//       all of the sub-controllers. We need to find the PRIMARY controller here.
const Ism::IsmChain* Ism::GetISMCtrlChain()
{
    if(!m_Table.empty())
    {
        for (size_t i = 0; i< m_Table.size(); i++)
        {
            for (size_t idx=0; idx < m_Table[i].ISMs.size(); idx++)
            {
                if (m_Table[i].ISMs[idx].Offset < (INT32)m_Table[i].Hdr.Size &&
                    m_Table[i].ISMs[idx].Flags & IsmFlags::PRIMARY)
                {
                    return &m_Table[i];
                }
            }
        }
        MASSERT(!"ISM table is missing a PRIMARY LW_ISM_ctrl chain!");
    }
    return NULL;
}

// Return the correct format version that should be used in the JS Object.
// This version number indicates what JS object properties will get populated by the C++ code.
UINT32 Ism::GetIsmJsObjectVersion(IsmTypes ismType)
{
    static UINT32 ismVersion[] =
    {
       0, //LW_ISM_none                                
       1, //LW_ISM_ctrl                                
       1, //LW_ISM_MINI_1clk                           
       1, //LW_ISM_MINI_2clk                           
       1, //LW_ISM_MINI_1clk_dbg                       
       1, //LW_ISM_ROSC_comp                           
       1, //LW_ISM_ROSC_bin                            
       1, //LW_ISM_ROSC_bin_metal                      
       0, //LW_ISM_VNSENSE_ddly                        
       1, //LW_ISM_TSOSC_a                             
       1, //LW_ISM_MINI_1clk_no_ext_ro                 
       1, //LW_ISM_MINI_2clk_dbg                       
       1, //LW_ISM_MINI_1clk_no_ext_ro_dbg             
       1, //LW_ISM_MINI_2clk_no_ext_ro                 
       2, //LW_ISM_NMEAS_v2                            
       1, //LW_ISM_HOLD                                
       1, //LW_ISM_ctrl_v2                             
       3, //LW_ISM_NMEAS_v3                            
       1, //LW_ISM_ROSC_bin_aging                      
       1, //LW_ISM_TSOSC_a2                            
       1, //LW_ISM_ctrl_v3                             
       1, //LW_ISM_aging                               
       1, //LW_ISM_MINI_2clk_v2                        
       1, //LW_ISM_MINI_2clk_dbg_v2                    
       1, //LW_ISM_TSOSC_a3                            
       1, //LW_ISM_ROSC_bin_aging_v1                   
       4, //LW_ISM_NMEAS_lite                          
       1, //LW_ISM_ctrl_v4                             
       1, //LW_ISM_aging_v1                            
       0, //LW_ISM_cpm                                 
       1, //LW_ISM_ROSC_bin_v1                         
       1, //LW_ISM_MINI_2clk_dbg_v3                    
       0, //LW_ISM_NMEAS_v4 (we don't know how to program this one)
       5, //LW_ISM_NMEAS_lite_v2                       
       1, //LW_ISM_aging_v2                            
       1, //LW_ISM_cpm_v1                              
       1, //LW_ISM_aging_v3                            
       1, //LW_ISM_aging_v4                            
       1, //LW_ISM_TSOSC_a4                            
       6, //LW_ISM_NMEAS_lite_v3                       
       1, //LW_ISM_ROSC_bin_v2                         
       1, //LW_ISM_ROSC_comp_v1                        
       2, //LW_ISM_cpm_v2                              
       7, //LW_ISM_NMEAS_lite_v4                       
       7, //LW_ISM_NMEAS_lite_v5                       
       7, //LW_ISM_NMEAS_lite_v6                       
       7, //LW_ISM_NMEAS_lite_v7                       
       3, //LW_ISM_cpm_v3                              
       1, //LW_ISM_ROSC_bin_v3                         
       4, //LW_ISM_cpm_v4                              
       5, //LW_ISM_cpm_v5                              
       2, //LW_ISM_ROSC_bin_v4                         
       1, //LW_ISM_ROSC_comp_v2                        
       8, //LW_ISM_NMEAS_lite_v8                       
       8, //LW_ISM_NMEAS_lite_v9                       
       2, //LW_ISM_imon                                
       6, //LW_ISM_cpm_v6                              
       3, //LW_ISM_imon_v2                             
       1, //LW_ISM_lwrrent_sink                        
    };
    if (ismType >= IsmTypes::LW_ISM_unknown)
    {
        MASSERT(!"GetIsmJsObjectVersion() unknown ISM type");
    }
    return (ismVersion[ismType]);
}

//------------------------------------------------------------------------------
// Search through the array of IsmInfo settings to find one that matches the
// startBit of this ISM. If not found use the first entry as the default.
RC Ism::GetISMSettings(
    vector<IsmInfo>*pSettings// (in) array of ISM specific settings
    ,const IsmChain *pChain  // (in) point the the specific chain data
    ,UINT32 IsmIdx           // (in) index into ISMs[] for ISM to check
    ,IsmInfo *pFinalVals)    // (out) location to store the final values.
{
    MASSERT(pSettings && pFinalVals && pChain && (IsmIdx < pChain->ISMs.size()));

    const IsmRef *pIsm = &pChain->ISMs[IsmIdx];

    // start with default values for all chains and update any field that may use defaults.
    *pFinalVals = (*pSettings)[0];
    pFinalVals->chainId = pChain->Hdr.InstrId;
    pFinalVals->chiplet = pChain->Hdr.Chiplet;
    pFinalVals->startBit= pChain->ISMs[IsmIdx].Offset;
    pFinalVals->flags   = pChain->ISMs[IsmIdx].Flags;
    RC rc;
    pFinalVals->version = GetIsmJsObjectVersion(pChain->ISMs[IsmIdx].Type);
    CHECK_RC(GetISMClkFreqKhz(&pFinalVals->clkKHz));

    // check to see if an alternate setting has been specified for any of the MINI ISMs.
    if( pIsm->Type == LW_ISM_MINI_1clk ||
        pIsm->Type == LW_ISM_MINI_1clk_no_ext_ro ||
        pIsm->Type == LW_ISM_MINI_1clk_no_ext_ro_dbg ||
        pIsm->Type == LW_ISM_MINI_2clk ||
        pIsm->Type == LW_ISM_MINI_2clk_v2 ||
        pIsm->Type == LW_ISM_MINI_2clk_no_ext_ro ||
        pIsm->Type == LW_ISM_MINI_1clk_dbg  ||
        pIsm->Type == LW_ISM_MINI_2clk_dbg ||
        pIsm->Type == LW_ISM_MINI_2clk_dbg_v2 ||
        pIsm->Type == LW_ISM_MINI_2clk_dbg_v3 ||
        pIsm->Type == LW_ISM_NMEAS_lite ||
        pIsm->Type == LW_ISM_NMEAS_lite_v2)
    {
        for ( UINT32 i = 1; i < pSettings->size(); i++)
        {
            if(pIsm->Offset == (*pSettings)[i].startBit)
            {
                pFinalVals->oscIdx = (*pSettings)[i].oscIdx;
                pFinalVals->outDiv = (*pSettings)[i].outDiv;
                pFinalVals->mode   = (*pSettings)[i].mode;
                break;
            }
        }
    }
    return OK;
}
//------------------------------------------------------------------------------
//! \brief Read speedometers from an ISM Jtag chain.
//!
//! Reads all the specified oscillators of a given type from the ISM jtag chains.
//! GPUs have multiple ISM cirlwits connected throughout the part using jtag chains. 
//! Each chain may have 1 or more ISMs. This method will read all of the chains that 
//! have the specified SpeedoType of oscillator and return their values.
//!
//! \param [out] pSpeedos Array that will hold the return values.
//! \param [in] pOscIdx Index of the oscillator we want to test.
//!                 putting NO_OSCILLATOR as the parameter selects
//!                 the default oscillator which may be different from each
//!                 SpeedoType and GPU class.
//! \param [in] SpeedoType determines what type of speedo to read. It can be one of the 
//!             IsmSpeedo::PART_TYPES.
RC Ism::ReadISMSpeedometers
(
    vector<UINT32>* pSpeedos    //!< (out) measured ring cycles
    ,vector<IsmInfo>* pInfo     //!< (out) uniquely identifies each ISM.
    ,PART_TYPES speedoType      //!< (in) what type of speedo to read
    ,vector<IsmInfo>*pSettings  //!< (in) Index of ring oscillators.
    ,UINT32 chainFlags          //!< (in) flags for chains to process
    ,UINT32 durationClkCycles   //!< (in)  sample len in cl cycles.
)
{
    RC rc;
    UINT32 idx = 0;
    MASSERT(pSpeedos);
    MASSERT(pSettings && !pSettings->empty());

    if (m_Table.empty() || speedoType == NMEAS || speedoType == HOLD)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (!m_TableIsValid){
        CHECK_RC(ValidateISMTable());
    }

    // The first entry are the global values for most ISMs
    // Make a copy so we don't disturb the callers original settings.
    vector<IsmInfo> locSettings(*pSettings);

    // Get the list of Jtag chains for each ISM class. Each chip may have
    // a different set of ISM that fall into a given class.
    // Note: The user can specify a single chain by setting the chainId & chiplet in the 1st
    //       entry in pSettings vector.
    vector<IsmChainFilter> chainFilter;
    CHECK_RC(GetChains(&chainFilter, speedoType, locSettings[0].chainId, locSettings[0].chiplet,
                       chainFlags));

    // Return failure if there are no ISMs of the specified type
    if (chainFilter.empty())
    {
        Printf(Tee::PriError, 
               "There are no ISMs with (Type:%s chainId:0x%x chiplet:%d) detected on this GPU.\n",
               SpeedoPartTypeToString(speedoType), locSettings[0].chainId, locSettings[0].chiplet);
        return RC::BAD_PARAMETER;
    }

    // If we have some chains to program, lets get their default values if not supplied.
    for (idx = 0; idx < locSettings.size(); idx++)
    {
        if (IsmSpeedo::NO_OSCILLATOR == locSettings[idx].oscIdx &&
            (VNSENSE != speedoType && TSOSC != speedoType))
        {
            locSettings[idx].oscIdx = GetOscIdx(speedoType);
        }

        if (IsmSpeedo::NO_OUTPUT_DIVIDER == locSettings[idx].outDiv)
        {
            locSettings[idx].outDiv = GetISMOutputDivider(speedoType);
        }

        if (IsmSpeedo::NO_MODE == locSettings[idx].mode)
        {
            locSettings[idx].mode = GetISMMode(speedoType);
        }
    }

    // This reading is self contained and ISMs should be disabled on exit
    // regardless of pass/failure
    Utility::CleanupOnReturnArg<void, void, Ism *> disableIsms(&DisableIsms, this);
    CHECK_RC(EnableISMs(true));

    // Mini ISM are special because they require a global controller to start
    // the sampling process.
    if( speedoType == MINI)
    {
        CHECK_RC(ReadMiniISMChainsViaJtag(pSpeedos,
                pInfo,
                &chainFilter,
                &locSettings,
                durationClkCycles));
    }
    else
    {
        for (idx = 0; idx < chainFilter.size(); idx++)
        {
            CHECK_RC(ReadISMChailwiaJtag(pSpeedos,
                                pInfo,
                                chainFilter[idx].pChain,
                                &locSettings,
                                chainFilter[idx].ismMask,
                                durationClkCycles));

        }
    }
    // dump out some debugging info.
    static std::map<int, std::string> speedoNames;
    if (speedoNames.empty())
    {
        speedoNames[COMP    ]= "Comp";
        speedoNames[BIN     ]= "Bin";
        speedoNames[METAL   ]= "Metal";
        speedoNames[MINI    ]= "Mini";
        speedoNames[TSOSC   ]= "Tsosc";
        speedoNames[VNSENSE ]= "Vnsense";
        speedoNames[IMON    ]= "Imon";
        speedoNames[BIN_AGING]= "Bin_Aging";
    }
    std::map<int, std::string>::iterator itr  = speedoNames.find(speedoType);
    if (itr != speedoNames.end())
    {
        vector<UINT32>::const_iterator iter;
        for (iter = pSpeedos->begin(), idx=0; iter != pSpeedos->end(); iter++,idx++)
        {
            Printf(Tee::PriDebug, "%s speedo:%d Value=%d\n",
                   speedoNames[speedoType].c_str(), idx, *iter);
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC Ism::ReadISMSpeedometers
(
    vector<UINT32>* pSpeedos    //!< (out) measured ring cycles
    ,vector<IsmInfo>* pInfo     //!< (out) uniquely identifies each ISM.
    ,PART_TYPES speedoType      //!< (in) what type of speedo to read
    ,vector<IsmInfo>*pSettings  //!< (in) ISM specific settings to use
    ,UINT32 durationClkCycles    //!< (in)  sample len in cl cycles.
)
{
    // Ignore the flags in the default implementation
    return ReadISMSpeedometers(pSpeedos,
                               pInfo,
                               speedoType,
                               pSettings,
                               Ism::NORMAL_CHAIN,
                               durationClkCycles);
}

//------------------------------------------------------------------------------
// Return the amount of time to wait for the experiment to complete based on the
// default Jtag clock that is running at 25Mhz frequency.
UINT32 Ism::GetDelayTimeUs(const IsmChain * pChain, UINT32 durClkCycles)
{
    //Ref-clk is about 25mhz, _DURATION is 32 bits,
    // 0xfffffffff * 1/25e6 = 171.79 sec. max.
    //1.0 milliseconds (minimum Sleep) is plenty in most cases.
    // min sleep time = 1ms
    // Note: On RTL sleeping for 1ms takes about 3 days! So we need to round to a smaller unit.
    // Some special case ISMs (TSOSC_a4) require a longer delay than what we callwlate from the 
    // JTag clock. 
    return durClkCycles / 25;
}

//------------------------------------------------------------------------------
// Run a set of speedo experiments via Jtag when multiple In-Silicon Measurement
// cirlwits (ISM)s are grouped together into a single JTag chain.
// Each ISM could have upto 255 Ring Oscillators(ROSCs) aka "speedos".
// This API does not account for the special LW_MINI_ class of ISMs that are
// controlled by a primary LW_ISM_CTRL. To read that type of ISM see
// ReadMiniISMChainsViaJtag()
// This API allows you to read up to maxISMsPerChain in a single chain.
//
RC Ism::ReadISMChailwiaJtag
(
    vector<UINT32> * pValues    // Vector of ROSC values to update.
    ,vector<IsmInfo>* pInfo     // Unique info to identify this ISM.
    ,const IsmChain * pChain    // What chain you want to run the experiment on
    ,vector<IsmInfo>* pSettings
    ,const bitset<Ism::maxISMsPerChain>& ismMask // a mask of the ISMs you want to run.
    ,UINT32     durClkCycles    // how long you want the experiment to run
)
{
    const bool DisableISM = false;
    const bool EnableISM = true;

    RC rc;

    MASSERT(pChain);
    vector <UINT32> jtagArray;
    vector <UINT32> jtagArray2;
    jtagArray.resize( (pChain->Hdr.Size+31)/32, 0);
    jtagArray2.resize( (pChain->Hdr.Size+31)/32, 0);

    if (NO_CLK_CYCLES == durClkCycles)
    {
        durClkCycles = GetISMDurClkCycles();
    }

    // We need to change the sequence as follows:
    // 1. program all chains with iddq=0 (power on)
    // 2. program the rest of the configuration information (enable = 0)
    // 3. program the enable = 1 bit.

    // Power up the ISM's in the disabled state, & duration = 0.
    CHECK_RC(ConfigureISMChain(pChain,
                                jtagArray,
                                DisableISM,
                                pSettings,
                                ismMask,
                                0,
                                STATE_1));

    // Send the bits down to JTag
    CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                            pChain->Hdr.InstrId,
                            pChain->Hdr.Size,
                            jtagArray));

    // We have to give the hardware time to power up before setting the duration.
    // On ISMs that don't have an enable bit, once the duration bits are set to non-zero
    // values the ISM will begin counting.
    Platform::Delay(1);
    CHECK_RC(ConfigureISMChain(pChain,
                                jtagArray2,
                                DisableISM,
                                pSettings,
                                ismMask,
                                durClkCycles,
                                STATE_2));

    if (jtagArray2 != jtagArray)
    {
        // Send the bits down to JTag
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray2));
    }

    // we need to wait atleast 1us after deasserting iddq before
    // asserting enable
    Platform::Delay(1);
    CHECK_RC(ConfigureISMChain(pChain,
                                jtagArray,
                                EnableISM,
                                pSettings,
                                ismMask,
                                durClkCycles,
                                STATE_3));

    // Most chains don't require a 2nd write to enable the ISMs so only send down
    // the 2nd set of bits if they are differnt.
    if (jtagArray2 != jtagArray)
    {
        // Enable the independant ISMs
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
    }

    UINT32 delayTimeUs = GetDelayTimeUs(pChain, durClkCycles);

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        delayTimeUs += 10; // add a small margin to allow the hardware to complete.
    }
    // make sure we don't have a zero sleep time.
    delayTimeUs = max(delayTimeUs, (UINT32)1);
    Platform::SleepUS(delayTimeUs);

    // Read the speedos
    CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
                            pChain->Hdr.InstrId,
                            pChain->Hdr.Size,
                            &jtagArray));

    // Extract the counts
    CHECK_RC(GetISMChainCounts(pChain,
                               jtagArray,
                               pValues,
                               pInfo,
                               pSettings,
                               ismMask,
                               durClkCycles));

    bitset<Ism::maxISMsPerChain> keepActiveMask;
    GetKeepActiveMask(&keepActiveMask,pChain);

    // power down the ISMs configure them to stop the measurement
    CHECK_RC(ConfigureISMChain(pChain,
                                jtagArray2,
                                DisableISM,
                                pSettings,
                                keepActiveMask, // this is what powers down the ISMs.
                                durClkCycles,
                                (STATE_4 | KEEP_ACTIVE)));  // Using keepActiveMask instead of standard ismMask

    if (jtagArray2 != jtagArray)
    {
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray2));
    }

    return OK;
}

//------------------------------------------------------------------------------
// Run a set of speedo experiments via Jtag when multiple In-Silicon Measurement
// cirlwits (ISM)s are grouped together into a single JTag chain.
// Each ISM could have upto 255 Ring Oscillators(ROSCs) aka "speedos".
// There is a special LW_MINI_ class of ISMs that are controlled by a primary
// LW_ISM_CTRL. To run an experiment you need to configure each ISM in the chain
// then configure LW_ISM_CTRL to start the experiment.
// This API allows you to read up ROSC x (aka speedo) from upto 32 ISMs in each
// chain.
//
RC Ism::ReadMiniISMChainsViaJtag
(
    vector<UINT32> * pValues   // Vector of ROSC values to update.
    ,vector<IsmInfo>* pInfo    // Unique info to identify this ISM.
    ,vector<IsmChainFilter>*pChainFilter //
    ,vector<IsmInfo>* pSettings//
    ,UINT32 durClkCycles  // how long you want the experiment to run
)
{
    const bool DisableISM = false;
    const bool EnableISM = true;
    // number of clocks after the trigger to wait before starting the experiment
    const UINT32 DelayClkCycles = 0;

    if( durClkCycles == 0)
    {
        Printf(Tee::PriError,
            "%s : durClkCycles parameter can't be zero!\n",
            __FUNCTION__);
        return RC::BAD_PARAMETER;
    }

    vector <UINT32> jtagArray;
    UINT32 idx = 0;
    RC rc;

    if (NO_CLK_CYCLES == durClkCycles)
    {
        durClkCycles = GetISMDurClkCycles();
    }

    // Program up all of the chains first
    for (idx = 0; idx < pChainFilter->size(); idx++)
    {
        const IsmChain * pChain = (*pChainFilter)[idx].pChain;
        jtagArray.resize((pChain->Hdr.Size+31)/32, 0);
        CHECK_RC(ConfigureISMChain(pChain,
                                    jtagArray,
                                    DisableISM,
                                    pSettings,
                                    (*pChainFilter)[idx].ismMask,
                                    durClkCycles,
                                    FLAGS_NONE));

        // Send the bits down to JTag
        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));
    }
    // Now program the primary controller to start the experiment
    // Get the chain with the primary control ISM. Its needed to control all of
    // the MINI_ISMs
    const IsmChain * pCtrl = GetISMCtrlChain();
    if (pCtrl == 0)
    {
        Printf(Tee::PriError,
            "%s : No control chain found!\n",
            __FUNCTION__);
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(ConfigureISMCtrl(pCtrl,
        jtagArray,
        EnableISM,   // enable & trigger
        durClkCycles,
        DelayClkCycles,
        LW_ISM_CTRL_TRIGGER_now,
        0, /*loopMode*/
        0, /*haltMode*/
        1  /*ctrlSrcSel=jtag*/));

    CHECK_RC(WriteHost2Jtag(pCtrl->Hdr.Chiplet,
                            pCtrl->Hdr.InstrId,
                            pCtrl->Hdr.Size,
                            jtagArray));

    //Ref-clk is about 25mhz, _DURATION is 32 bits,
    // 0xfffffffff * 1/25e6 = 171.79 sec. max.
    // add 10% margin to account for the really long durations.
    UINT32 delayMargin = max((UINT32)((float)durClkCycles * 0.1 * (1000.0 / 25.0e+6)), (UINT32)1);
    UINT32 delayTimeMs = (UINT32)((float)durClkCycles * (1000.0 / 25.0e+6));
    Platform::SleepUS((delayTimeMs+delayMargin)*1000);

    UINT32 complete = 0;
    CHECK_RC(PollISMCtrlComplete(&complete));

    if (!complete)
    {
        Printf(Tee::PriError,
               "%s : WARNING! Timed out waiting for MINI ISMs to complete!\n"
               "DurClkCycles = %d\n", __FUNCTION__, durClkCycles);
        return(RC::ISM_TIMEOUT);
    }
    // Read the chains
    for (idx = 0; idx < pChainFilter->size(); idx++)
    {
        const IsmChain * pChain = (*pChainFilter)[idx].pChain;
        jtagArray.resize( (pChain->Hdr.Size+31)/32, 0);

        CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                &jtagArray));

        // Extract the counts
        CHECK_RC(GetISMChainCounts(pChain,
                                   jtagArray,
                                   pValues,
                                   pInfo,
                                   pSettings,
                                   (*pChainFilter)[idx].ismMask,
                                   durClkCycles));
    }

    // Disable the primary controller
    CHECK_RC(ConfigureISMCtrl(pCtrl,
                              jtagArray,
                              DisableISM,
                              durClkCycles,
                              DelayClkCycles,
                              LW_ISM_CTRL_TRIGGER_now,
                              0, /*loopMode*/
                              0, /*haltMode*/
                              0  /*ctrlSrcSel*/));

    CHECK_RC(WriteHost2Jtag(pCtrl->Hdr.Chiplet,
                            pCtrl->Hdr.InstrId,
                            pCtrl->Hdr.Size,
                            jtagArray));

    return OK;
}

//------------------------------------------------------------------------------
// Reads the current MINI ISM counts on all of the chains.
// Note: The primary controller is not programmed to start any experiements.
RC Ism::ReadMiniISMs
(
    vector<UINT32> *pSpeedos
    ,vector<IsmInfo> *pInfo
    ,UINT32 chainFlags
    ,UINT32 expflags
)
{
    if(m_Table.empty())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (!GetISMsEnabled())
    {
        Printf(Tee::PriError,
              "%s : ISMs not enabled! Was SetupMiniISMs[OnChain] successful?\n",
              __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    vector<IsmChainFilter> chainFilter;
    CHECK_RC(GetChains(&chainFilter, MINI, -1, -1, chainFlags));

    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        CHECK_RC(ReadMiniISMsOnChain(
            chainFilter[idx].pChain->Hdr.InstrId,
            chainFilter[idx].pChain->Hdr.Chiplet,
            pSpeedos, pInfo, expflags));
    }
    return rc;
}

//------------------------------------------------------------------------------
// Reads the current MINI ISM counts on a specific chain.
// Note: The primary controller is not programmed to start any experiements.
RC Ism::ReadMiniISMsOnChain
(
    INT32 chainId
    ,INT32 chiplet
    ,vector<UINT32> *pSpeedos
    ,vector<IsmInfo> *pInfo
    ,UINT32 expFlags
)
{
    RC rc;
    const bool DisableISM = false;
    vector<IsmChainFilter> chainFilter;
    vector <UINT32> jtagArray;

    if (!GetISMsEnabled())
    {
        Printf(Tee::PriHigh,
              "%s : ISMs not enabled! Was SetupMiniISMs[OnChain] successful?\n",
              __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    UINT32 complete = 0;
    CHECK_RC(PollISMCtrlComplete(&complete));
    if (!complete)
        return RC::ISM_EXPERIMENT_INCOMPLETE;

    // Get the list of Jtag chains for a given ISM class. Each chip may have
    // a diffent set of ISMs that fall into a given class.
    CHECK_RC(GetChains(&chainFilter, MINI, chainId, chiplet, ~0));

    if (chainFilter.empty())
        return RC::BAD_PARAMETER;

    // ChainId/Chiplet should be unique
    MASSERT(chainFilter.size() == 1);
    const IsmChain * pChain = chainFilter[0].pChain;

    if (!m_IsmLocSettings.count(pChain->Hdr))
        return RC::BAD_PARAMETER;

    vector<IsmInfo> *pSettings = &m_IsmLocSettings[pChain->Hdr];

    jtagArray.resize((pChain->Hdr.Size+31)/32, 0);

    CHECK_RC(ReadHost2Jtag(pChain->Hdr.Chiplet,
                            pChain->Hdr.InstrId,
                            pChain->Hdr.Size,
                            &jtagArray));

    // Extract the counts
    CHECK_RC(GetISMChainCounts(pChain,
                               jtagArray,
                               pSpeedos,
                               pInfo,
                               pSettings,
                               chainFilter[0].ismMask,
                               m_IsmLocDurClkCycles)); // this needs to be saved off during setup

    // power down the ISMs configure them to stop the measurement
    if (!(expFlags & KEEP_ACTIVE))
    {
        bitset<Ism::maxISMsPerChain> keepActiveMask;
        GetKeepActiveMask(&keepActiveMask,pChain);

        CHECK_RC(ConfigureISMChain(pChain,
                                    jtagArray,
                                    DisableISM,
                                    pSettings,
                                    keepActiveMask,
                                    m_IsmLocDurClkCycles,
                                    FLAGS_NONE));

        CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                                pChain->Hdr.InstrId,
                                pChain->Hdr.Size,
                                jtagArray));

        m_IsmLocSettings.erase(pChain->Hdr);
    }

    // power off the primary controller if the experiment is complete
    if (m_IsmLocSettings.empty())
    {
        const IsmChain *pCtrl = GetISMCtrlChain();
        CHECK_RC(ConfigureISMCtrl(pCtrl,
            jtagArray,
            DisableISM,
            1,          // duration clock cycles
            0,          // delay
            LW_ISM_CTRL_TRIGGER_off,
            0,          // loopMode
            0,          // haltMode
            0));        // ctrlSrcSel
    
        CHECK_RC(WriteHost2Jtag(pCtrl->Hdr.Chiplet,
                                pCtrl->Hdr.InstrId,
                                pCtrl->Hdr.Size,
                                jtagArray));
    
        // If the experiment is over, power off the ISMs
        CHECK_RC(EnableISMs(false));
    }

    return rc;
}

//------------------------------------------------------------------------------
// This is only supported on GV100+
// Reads the current NMEAS ISM counts on all of the chains.
/*virtual*/
RC Ism::ReadNoiseMeasLite
(
    vector<UINT32> *pSpeedos
    ,vector<NoiseInfo> *pInfo
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// This is only supported on GV100+
// Reads the current NMEAS ISM counts on one chain.
/*virtual*/
RC Ism::ReadNoiseMeasLiteOnChain
(
    INT32 chainId
    ,INT32 chiplet
    ,vector<UINT32> *pSpeedos
    ,vector<NoiseInfo> *pInfo
    ,bool bPollExperiment
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// This API will setup the PRIMARY ISM controller. To setup a sub-controller use
// SetupISMSubControllerOnChain()
RC Ism::SetupISMController
(
    UINT32 dur        // number of clock cycles to run the experiment
    ,UINT32 delay     // number of clock cycles to delay after the trigger
                      // before the ROs start counting.
    ,INT32 trigger    // trigger sourc to use
                      // (0=off, 1=priv write, 2=perf mon trigger, 3=now)
    ,INT32 loopMode   // 1= continue looping the experiment until the haltMode
                      //    has been set to 1.
                      // 0= perform a single experiment
    ,INT32 haltMode   // 1= halt the lwrrently running experiment. Used in
                      //    conjunction with loopMode to stop the repeating the
                      //    same experiment.
                      // 0= don't halt the current experiment.
    ,INT32 ctrlSrcSel // 1 = jtag, 0 = priv access
)
{
    RC rc;
    const bool EnableISM = true;

    if (m_Table.empty())
    {
        Printf(Tee::PriWarn, "%s : ISM table is empty!\n", __FUNCTION__);
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (dur == NO_CLK_CYCLES)
    {
        dur = GetISMDurClkCycles();
    }

    if (delay == NO_DELAY)
    {
        delay = 0;
    }

    if (trigger == NO_TRIGGER)
    {
        trigger = LW_ISM_CTRL_TRIGGER_now;
    }
    vector <UINT32> jtagArray;
    const IsmChain * pCtrl = GetISMCtrlChain();

    // Save off this controller chain for subsequent calls to ReadNoiseMeasLiteOnChain()
    m_pCachedISMControlChain = pCtrl;
    
    Utility::CleanupOnReturnArg<void, void, Ism *> disableIsms(&DisableIsms, this);
    CHECK_RC(EnableISMs(true));

    // when haltMode is true then the experiment is already running and more than
    // likely the loopMode has been set to continually run the same experiment.

    if (haltMode == 0)
    {
        // Reset the primary controller trigger
        CHECK_RC(ConfigureISMCtrl(pCtrl,
            jtagArray,
            EnableISM,
            dur,         // duration clock cycles
            delay,
            LW_ISM_CTRL_TRIGGER_off,
            loopMode,
            haltMode,
            ctrlSrcSel));

        CHECK_RC(WriteHost2Jtag(pCtrl->Hdr.Chiplet,
                                pCtrl->Hdr.InstrId,
                                pCtrl->Hdr.Size,
                                jtagArray));

        Platform::Delay(10); // usec
    }

    CHECK_RC(ConfigureISMCtrl(pCtrl,
        jtagArray,
        EnableISM,   // enable & trigger
        dur,         // duration clock cycles
        delay,
        trigger,
        loopMode,
        haltMode,
        ctrlSrcSel));

    CHECK_RC(WriteHost2Jtag(pCtrl->Hdr.Chiplet,
                            pCtrl->Hdr.InstrId,
                            pCtrl->Hdr.Size,
                            jtagArray));

    // Save off the dur/delay for later use.
    m_IsmLocDurClkCycles = dur;

    // Leave ISMs enabled.  The corresponding disable will either happen in the
    // read or will need to be done manually if the user elects to keep the
    // experiment going after the read
    disableIsms.Cancel();

    return rc;
}

//------------------------------------------------------------------------------
// This API will setup all the ISM sub-controllers on a specific chain to run an experiment.
RC Ism::SetupISMSubController
(
    UINT32 dur        // number of clock cycles to run the experiment
    ,UINT32 delay     // number of clock cycles to delay after the trigger
                      // before the ROs start counting.
    ,INT32 trigger    // trigger sourc to use
                      // (0=off, 1=priv write, 2=perf mon trigger, 3=now)
    ,INT32 loopMode   // 1= continue looping the experiment until the haltMode
                      //    has been set to 1.
                      // 0= perform a single experiment
    ,INT32 haltMode   // 1= halt the lwrrently running experiment. Used in
                      //    conjunction with loopMode to stop the repeating the
                      //    same experiment.
                      // 0= don't halt the current experiment.
    ,INT32 ctrlSrcSel // 1 = jtag 0 = priv access

)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// This API will setup all the ISM sub-controllers on a specific chain to run an experiment.
RC Ism::SetupISMSubControllerOnChain
(
    INT32 chainId          // The Jtag chain
    ,INT32 chiplet         // The chiplet for this jtag chain.
    ,UINT32 dur            // number of clock cycles to run the experiment
    ,UINT32 delay          // number of clock cycles to delay after the trigger
                           // before the ROs start counting.
    ,INT32 trigger         // trigger sourc to use
                           // (0=off, 1=priv write, 2=perf mon trigger, 3=now)
    ,INT32 loopMode        // 1= continue looping the experiment until the haltMode
                           //    has been set to 1.
                           // 0= perform a single experiment
    ,INT32 haltMode        // 1= halt the lwrrently running experiment. Used in
                           //    conjunction with loopMode to stop the repeating the
                           //    same experiment.
                           // 0= don't halt the current experiment.
    ,INT32 ctrlSrcSel      // 1 = jtag 0 = priv access
    ,bool bCacheController // true = save off the pChain info for subsequent use.

)
{
    return RC::UNSUPPORTED_FUNCTION;
}


//------------------------------------------------------------------------------
// Setup all the MINI ISMs to run an experiment.
RC Ism::SetupMiniISMs(
    INT32 oscIdx
  , INT32 outDiv
  , INT32 mode
  , INT32 init
  , INT32 finit
  , INT32 refClkSel
  , UINT32 chainFlags
)
{
    RC rc;
    vector<IsmChainFilter> chainFilter;
    CHECK_RC(GetChains(&chainFilter, MINI, -1, -1, chainFlags));

    Utility::CleanupOnReturnArg<void, void, Ism *> disableIsms(&DisableIsms, this);
    CHECK_RC(EnableISMs(true));

    for (UINT32 idx = 0; idx < chainFilter.size(); idx++)
    {
        CHECK_RC(SetupMiniISMsOnChain(
            chainFilter[idx].pChain->Hdr.InstrId,
            chainFilter[idx].pChain->Hdr.Chiplet,
            oscIdx, outDiv, mode, init, finit, refClkSel));
    }

    // Leave ISMs enabled.  The corresponding disable will either happen in the
    // read or will need to be done manually if the user elects to keep the
    // experiment going after the read
    disableIsms.Cancel();
    return rc;
}

//------------------------------------------------------------------------------
// Setup all the MINI ISMs on a specific chain to run an experiment.
RC Ism::SetupMiniISMsOnChain(
    INT32 chainId
  , INT32 chiplet
  , INT32 oscIdx
  , INT32 outDiv
  , INT32 mode
  , INT32 init
  , INT32 finit
  , INT32 refClkSel
)
{
    RC rc;
    vector<IsmChainFilter> chainFilter;
    vector <UINT32> jtagArray;
    const bool EnableISM = true;

    if (oscIdx == -1)
        oscIdx = GetOscIdx(MINI);
    if (outDiv == -1)
        outDiv = GetISMOutputDivider(MINI);
    if (refClkSel == -1)
        refClkSel = GetISMRefClkSel(MINI);

    // Get the list of Jtag chains for the MINI ISM class. Each chip may have
    // a diffent set of ISMs that fall into this class.
    CHECK_RC(GetChains(&chainFilter, MINI, chainId, chiplet, ~0));

    // if not found the chainId is probably wrong.
    if (chainFilter.empty())
        return RC::BAD_PARAMETER;

    // ChainId/Chiplet should be unique
    MASSERT(chainFilter.size() == 1);
    const IsmChain * pChain = chainFilter[0].pChain;

    jtagArray.resize((pChain->Hdr.Size + 31) / 32, 0);
    IsmInfo info(1, 
                 chainId, 
                 pChain->Hdr.Chiplet,
                 -1, 
                 oscIdx,
                 outDiv, 
                 mode, 
                 0, 
                 0, 
                 0, 
                 init, 
                 finit,
                 FLAGS_NONE, 
                 0, 
                 refClkSel);

    Utility::CleanupOnReturnArg<void, void, Ism *> disableIsms(&DisableIsms, this);
    CHECK_RC(EnableISMs(true));

    // TODO: save off the current settings for this chain. We will need them
    // when we try to read back the counts and colwert to frequency.
    vector<IsmInfo> locSettings;
    locSettings.push_back(info);
    // Power up and configure the MINI ISM's
    CHECK_RC(ConfigureISMChain(pChain,
                                jtagArray,
                                EnableISM,
                                &locSettings,
                                chainFilter[0].ismMask,
                                GetISMDurClkCycles(),
                                FLAGS_NONE));

    // Send the bits down to JTag
    CHECK_RC(WriteHost2Jtag(pChain->Hdr.Chiplet,
                            pChain->Hdr.InstrId,
                            pChain->Hdr.Size,
                            jtagArray));

    // save off these setting for when we read back the values
    m_IsmLocSettings[pChain->Hdr] = locSettings;

    // Leave ISMs enabled.  The corresponding disable will either happen in the
    // read or will need to be done manually if the user elects to keep the
    // experiment going after the read
    disableIsms.Cancel();

    return rc;
}

//------------------------------------------------------------------------------
// Get the current iddq mask for this requested chain. By default there are no
// iddq masks created. This only oclwrs in JS scripts when the user calls the
// gpuSubdev.SetKeepActive(instrId,chiplet,offset,bKeepActive)
void Ism::GetKeepActiveMask(bitset<Ism::maxISMsPerChain>* pActiveMask, const IsmChain * pChain)
{
    for (size_t i = 0; i < m_Table.size(); i++)
    {
        if ((m_Table[i].Hdr.Chiplet == pChain->Hdr.Chiplet) &&
            (m_Table[i].Hdr.InstrId == pChain->Hdr.InstrId))
        {
            for (size_t j = 0; j < m_Table[i].ISMs.size(); j++)
            {
                // 0 = PowerUp, 1 = PowerDn
                (*pActiveMask)[j] = (m_Table[i].ISMs[j].Flags & KEEP_ACTIVE) ? 1:0;
            }
        }
    }
}

//------------------------------------------------------------------------------
// Specify if this ISM should remain active (powered up) after reading its counts
RC Ism::SetKeepActive(UINT32 instrId, UINT32 chiplet, INT32 offset, bool bKeepActive)
{
    bool instrIdOK = false;
    bool chipletOK = false;

    for (size_t i = 0; i < m_Table.size(); i++)
    {
        if (m_Table[i].Hdr.InstrId == instrId)
        {
            instrIdOK = true;
        }
        if (m_Table[i].Hdr.Chiplet == chiplet)
        {
            chipletOK = true;
        }
        if ((m_Table[i].Hdr.Chiplet == chiplet) && (m_Table[i].Hdr.InstrId == instrId))
        {
            for (size_t j = 0; j < m_Table[i].ISMs.size(); j++)
            {
                if (m_Table[i].ISMs[j].Offset == offset)
                {
                    if (bKeepActive)
                    {
                        m_Table[i].ISMs[j].Flags |= KEEP_ACTIVE;
                    }
                    else
                    {
                        m_Table[i].ISMs[j].Flags &= ~KEEP_ACTIVE;
                    }
                    return OK;
                }
            }
        }
    }

    if (!instrIdOK)
    {
        Printf(Tee::PriError, "Invalid instrId 0x%x\n", instrId);
    }
    if (!chipletOK)
    {
        Printf(Tee::PriError, "Invalid chiplet %u\n", chiplet);
    }
    if (chipletOK && instrIdOK)
    {
        Printf(Tee::PriError, "Invalid offset %d\n", offset);
    }
    return RC::BAD_PARAMETER;
}

//------------------------------------------------------------------------------
// Validate the ISM table meets the following requirements:
// 1. There is exactly one chain that has a LW_ISM_ctrl_* macro. This is the primary ISM used to
//    control all of the MINI_xxx ISMs.
// 2. No two chains have the same instructionID/chiplet tuple.
// 3. None of the ISMs in a chain overlap or exceed the chain size.
// Assumption here is that we have a valid m_Table and m_Size > 0
RC Ism::ValidateISMTable()
{
    StickyRC rc;
    ChainHdrSet hdrs;
    // Check for duplicate chains
    for (size_t i=0; i < m_Table.size(); i++)
    {
        if (hdrs.find(m_Table[i].Hdr) == hdrs.end())
        {
            hdrs.insert(m_Table[i].Hdr);
        }
        else
        {
            // find the duplicate(s)
            for (size_t ii=0; ii < i; ii++)
            {
                if(m_Table[ii].Hdr == m_Table[i].Hdr)
                {
                    Printf(Tee::PriHigh,"Chain[%d]={0x%x,%d,%d} is duplicate of"
                        " Chain[%d]\n",
                        (int)i,
                        m_Table[ii].Hdr.InstrId,
                        m_Table[ii].Hdr.Size,
                        m_Table[ii].Hdr.Chiplet,
                        (int)ii);
                    rc = RC::SOFTWARE_ERROR;
                }
            }
        }
    }
    hdrs.clear();
    // check for overlapping or out of range ISMs.
    for (size_t i=0; i < m_Table.size(); i++)
    {
        INT32 offset = -1;
        for (size_t j=0; j < m_Table[i].ISMs.size(); j++)
        {
            if (m_Table[i].ISMs[j].Offset > offset)
            {
                offset = m_Table[i].ISMs[j].Offset + GetISMSize(m_Table[i].ISMs[j].Type) -1;
            }
            else
            {
                Printf(Tee::PriHigh,"Chain:%d (InstrId:0x%x,Size:%d,Chiplet:%d)"
                    " ISM offset:%d is misconfigured\n",
                    (int)i,
                    m_Table[i].Hdr.InstrId,
                    m_Table[i].Hdr.Size,
                    m_Table[i].Hdr.Chiplet,
                    m_Table[i].ISMs[j].Offset);
                rc = RC::SOFTWARE_ERROR;
            }
        }
        if ((UINT32)offset >= m_Table[i].Hdr.Size)
        {
            Printf(Tee::PriHigh,"Chain:%d (InstrId:0x%x,Size:%d,Chiplet:%d)"
                " ISMs are out of bounds\n",
                (int)i,
                m_Table[i].Hdr.InstrId,
                m_Table[i].Hdr.Size,
                m_Table[i].Hdr.Chiplet);
            rc = RC::SOFTWARE_ERROR;
        }
        else if ((!AllowIsmUnusedBits() && !AllowIsmTrailingBit() && ((UINT32)offset < (m_Table[i].Hdr.Size-1))) ||
                 (!AllowIsmUnusedBits() && AllowIsmTrailingBit()  && ((UINT32)offset < (m_Table[i].Hdr.Size-2))))
        {
            Printf(Tee::PriHigh, "Chain:%d (InstrId:0x%x,Size:%d,Chiplet:%d)"
            " is %d bit(s) larger then total size of ISMs.\n",
            (int)i,
            m_Table[i].Hdr.InstrId,
            m_Table[i].Hdr.Size,
            m_Table[i].Hdr.Chiplet,
            m_Table[i].Hdr.Size - offset - 1);
            rc = RC::SOFTWARE_ERROR;
        }
    }

    // validate only one primary control ISM exists.
    bool found = false;
    for (size_t i=0; i < m_Table.size(); i++)
    {
        for (size_t j=0; j < m_Table[i].ISMs.size(); j++)
        {
            if (m_Table[i].ISMs[j].Flags & IsmFlags::PRIMARY)
            {
                if (found)
                {
                    Printf(Tee::PriError,
                        "Chain:%d (InstrId:0x%x, Size:%d, Chiplet:%d) "
                        "ISM offset:%d found another primary control ISM.\n",
                        (int)i,
                        m_Table[i].Hdr.InstrId,
                        m_Table[i].Hdr.Size,
                        m_Table[i].Hdr.Chiplet,
                        m_Table[i].ISMs[j].Offset);
                    rc = RC::SOFTWARE_ERROR;
                }
                found = true;
            }
        }
    }
    if (!found)
    {
        Printf(Tee::PriError, "ISM Table is  missing a primary ISM Controller\n");
        rc = RC::SOFTWARE_ERROR;
    }

    m_TableIsValid = (OK == rc);

    return rc;
}

//--------------------------------------------------------------------------------------------------
// Noise measurement experiment APIs by default are not supported.
// The first implementation of this is on GP10x GPUs
/*virtual*/
RC Ism::SetupNoiseMeas //!< Setup internal data in prepration for a noise experiment
(
    Ism::NoiseMeasParam param     //!< structure with setup information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------------------------------------
// The first implementation of this is on GV10X GPUs
/*virtual*/
RC Ism::SetupNoiseMeasLite //!< Setup internal data in prepration for a noise experiment
(
    const NoiseInfo& param     //!< structure with setup information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
//--------------------------------------------------------------------------------------------------
// The first implementation of this is on GV10X GPUs
/*virtual*/
RC Ism::SetupNoiseMeasLiteOnChain
(
    INT32 chainId
    , INT32 chiplet
    , const NoiseInfo& param  // structure with setup information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-------------------------------------------------------------------------------------------------
// The first implementation of this is on GV10X GPUs
// Write all of the Critical Path Measurement (CPM) cirlwits on this GPU
/*virtual*/
RC Ism::WriteCPMs
(
    vector<CpmInfo> &params     // structure with setup information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-------------------------------------------------------------------------------------------------
// The first implementation of this is on GV10X GPUs
// Read all of the Critical Path Measurement (CPM) cirlwits on this GPU
/*virtual*/
RC Ism::ReadCPMs
(
    vector<CpmInfo> *pInfo
    , vector<CpmInfo> &cpmParams      // structure with setup information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
//-------------------------------------------------------------------------------------------------
// The first implementation of this is on AD10X GPUs
// Write all of the Current Sink (ISINK) cirlwits on this GPU
/*virtual*/
RC Ism::WriteISinks
(
    vector<ISinkInfo> &params     // structure with setup information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//-------------------------------------------------------------------------------------------------
// The first implementation of this is on AD10x GPUs
// Read all of the Current Sink cirlwits on this GPU
/*virtual*/
RC Ism::ReadISinks
(
    vector<ISinkInfo> *pInfo
    , vector<ISinkInfo> &cpmParams      // structure with setup information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/*virtual*/
RC Ism::RunNoiseMeas //!< run the noise measurement experiment on all chains
(
    NoiseMeasStage stage //!< bitmask of the experiment stages to perform.
    ,vector<UINT32>*pValues //!< vector to hold the raw counts
    ,vector<NoiseInfo>*pInfo //!< vector to hold the macro specific information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/*virtual*/
RC Ism::RunNoiseMeasOnChain //!< run the noise measurement experiment on a specific chain
(
    INT32 chainId       //!< JTag instruction Id to use
    ,INT32 chiplet       //!< JTag chiplet to use
    ,NoiseMeasStage stage //!< bitmask of the experiment statges to perform
    ,vector<UINT32>*pValues //!< vector to hold the raw counts
    ,vector<NoiseInfo>*pInfo //!< vector to hold the macro specific information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------------------------------------
// HOLD measurement APIs by default are not supported.
// The first implementation of this is on T210 CheetAh SOCs
/*virtual*/
RC Ism::ReadHoldISMs //!< Read all Hold ISMs
(
    const HoldInfo   & settings  //!< settings for reading the hold ISMs
    ,vector<HoldInfo>* pInfo     //!< vector to hold the macro specific information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/*virtual*/
RC Ism::ReadHoldISMsOnChain //!< Read Hold ISMs on a specific chain
(
    INT32               chainId   //!< JTag instruction Id to use
    ,INT32              chiplet   //!< JTag chiplet to use
    ,const HoldInfo   & settings  //!< settings for reading the hold ISMs
    ,vector<HoldInfo> * pInfo     //!< vector to hold the macro specific information
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

