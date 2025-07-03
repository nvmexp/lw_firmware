/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/argread.h"
#include "gpu/include/gpusbdev.h"
#include "crcchain.h"
#include "mdiag/utils/surf_types.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "mdiag/sysspec.h"

#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "class/cl5080.h" // LW50_DEFERRED_API_CLASS
#include "class/cl9097.h" // FERMI_A
#include "class/cl9197.h" // FERMI_B
#include "class/cl9297.h" // FERMI_C
#include "class/cl86b6.h" // LW86B6_VIDEO_COMPOSITOR
#include "class/cl902d.h" // FERMI_TWOD_A
#include "class/cl90b8.h" // GF106_DMA_DECOMPRESS
#include "class/cl95a1.h" // LW95A1_TSEC
#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "class/cla140.h" // KEPLER_INLINE_TO_MEMORY_B
#include "class/cla06c.h" // KEPLER_CHANNEL_GROUP_A
#include "class/clb097.h" // MAXWELL_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc197.h" // PASCAL_B
#include "class/clc397.h" // VOLTA_A
#include "class/clc497.h" // VOLTA_B
#include "class/clc597.h" // TURING_A
#include "class/clc697.h" // AMPERE_A
#include "class/clc797.h" // AMPERE_B
#include "class/clc997.h" // ADA_A
#include "class/clcb97.h" // HOPPER_A
#include "class/clcc97.h" // HOPPER_B
#include "class/clcd97.h" // BLACKWELL_A

/* 
 * CRC chain is based on the smallest chip in the family.
 * For example: GA100 should be compatible with GA10x class.
*/
const char* GF100_CRC_CHAIN[]  = {"gf100"};
const char* GF108_CRC_CHAIN[]  = {"gf108", "gf100"};
const char* GF118_CRC_CHAIN[]  = {"gf118", "gf108","gf100"};
const char* SEC_CRC_CHAIN[]    = {"lw95a1_tsec", "g98f", "g98a", "g82f", "lw50a"};
const char* GK104_CRC_CHAIN[]  = {"gk104", "kepler_a"};
const char* GK20A_CRC_CHAIN[]  = {"gk20a", "gk110", "gk104", "kepler_a"};
const char* GK20X_CRC_CHAIN[]  = {"gk20x", "gk20a", "gk110", "gk104", "kepler_a"};
const char* GM108_CRC_CHAIN[]  = {"gm108", "maxwell_a", "gk20x", "gk20a", "gk110", "gk104", "kepler_a"};
const char* GM208_CRC_CHAIN[]  = {"gm208", "gm108", "maxwell_a", "gk20x", "gk20a", "gk110", "gk104", "kepler_a"};
const char* GP108_CRC_CHAIN[]  = {"gp108", "pascal_a","gm208","gm108"};
const char* GV100_CRC_CHAIN[]  = {"gv100", "volta_a","gp108"};
const char* GV208_CRC_CHAIN[]  = {"volta_b", "gv100", "volta_a", "gp108"};
const char* TU108_CRC_CHAIN[]  = {"tu108", "turing_a","volta_b", "gv100", "volta_a"};
const char* GA108_CRC_CHAIN[]  = {"ga108", "ampere_a", "tu108", "turing_a"};
const char* AD108_CRC_CHAIN[]  = {"ad108", "ada_a", "ga108", "ampere_a"};
const char* GH100_CRC_CHAIN[]  = {"gh100", "hopper_a", "ga108", "ampere_a"};
// Since hopper_next (merge of hopper_a and ada_a) isn't defined yet, 
// ad108/ada_a has been added for hopper_b here.
const char* GH20X_CRC_CHAIN[]  = {"gh100", "hopper_a", "ad108", "ada_a"};
const char* GB100_CRC_CHAIN[]  = {"gb100", "blackwell_a", "gh208", "gh100", "hopper_a"};
// When adding a new crcChain here, you will need to add crcChains in 
// SetupPerfsimCrcChain and SetupAmodelCrcChain

CrcChainManager::CrcChainManager
(
    UINT32 defaultClass
):
    m_DefaultCrcChainClass(defaultClass)
{
}

Trace3DCrcChain::Trace3DCrcChain
(
    const ArgReader* params,
    GpuSubdevice* pSubdev,
    UINT32 defaultClass
):
    CrcChainManager(defaultClass),
    m_Params(params),
    m_Subdev(pSubdev)
{
}

CrcChain& Trace3DCrcChain::GetCrcChain(UINT32 classnum)
{
    // Use default class if classnum is not specified
    if (classnum == UNSPECIFIED_CLASSNUM)
    {
        classnum = m_DefaultCrcChainClass;
    }

    // Find the crc chain of the class
    map<UINT32, CrcChain>::iterator it = m_Class2CrcChain.find(classnum);
    if (it != m_Class2CrcChain.end() && it->second.size() > 0)
    {
        return it->second;
    }

    // Setup crc chain if not found
    RC rc = SetupCrcChain(classnum);
    if (rc == OK)
    {
        return m_Class2CrcChain[classnum];
    }
    rc.Clear();

    // Use crc chain of default class if setup failed
    if (m_Class2CrcChain[m_DefaultCrcChainClass].size() == 0)
    {
        rc = SetupCrcChain(m_DefaultCrcChainClass);
        if (rc != OK)
        {
            WarnPrintf("%s: Setup crc chain of default class(0x%x) failed. "
                "An empty chain is returned.\n",
                __FUNCTION__, m_DefaultCrcChainClass);
        }
    }
    return m_Class2CrcChain[m_DefaultCrcChainClass];
}

RC Trace3DCrcChain::SetupCrcChain(UINT32 classnum)
{
    RC rc = OK;
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        switch(*Platform::ModelIdentifier())
        {
            case 'a': // Amodel
                CHECK_RC(SetupAmodelCrcChain(classnum));
                break;
            case 'p': // Perfsim
                CHECK_RC(SetupPerfsimCrcChain(classnum));
                break;
            default:
                rc = RC::SOFTWARE_ERROR;
                break;
        }
    }
    else if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        switch(*Platform::ModelIdentifier())
        {
            case 'a': // FSA
                InfoPrintf("%s: Overriding with Amodel CRC chain for FSA mode.\n", __FUNCTION__);
                CHECK_RC(SetupAmodelCrcChain(classnum));
                break;
            case 'p': // FSP
                InfoPrintf("%s: Overriding with Perfsim CRC chain for FSP mode.\n", __FUNCTION__);
                CHECK_RC(SetupPerfsimCrcChain(classnum));
                break;
            default: // Fmodel
                CHECK_RC(SetupNonAmodelCrcChain(classnum));
                break;
        }
    }
    else
    {
        CHECK_RC(SetupNonAmodelCrcChain(classnum));
    }

    return rc;
}

RC Trace3DCrcChain::SetupAmodelCrcChain(UINT32 classnum)
{
    CrcChain& crcChain = m_Class2CrcChain[classnum];

    if (EngineClasses::IsClassType("Lwdec", classnum) ||
        EngineClasses::IsClassType("Lwenc", classnum) ||
        EngineClasses::IsClassType("Lwjpg", classnum) ||
        EngineClasses::IsClassType("Ofa", classnum))
    {
        crcChain.push_back(EngineClasses::GetClassName(classnum));
    }
    else if (EngineClasses::IsClassType("Ce", classnum))
    {
        crcChain.push_back("gt212a_copy_engine");
    }
    else if (EngineClasses::IsClassType("Compute", classnum))
    {
        crcChain.push_back("");
    }
    else
    {
        switch(classnum)
        {
        case FERMI_A:
            crcChain.push_back("gf100a");
            break;
        case FERMI_B:
            crcChain.push_back("gf108a");
            crcChain.push_back("gf100a");
            break;
        case FERMI_C:
            crcChain.push_back("gf118a");
            crcChain.push_back("gf108a");
            crcChain.push_back("gf100a");
            break;
        case KEPLER_A:
            crcChain.push_back("gk104a");
            crcChain.push_back("kepler_a_amod");
            break;
        case KEPLER_B:
            crcChain.push_back("gk110a");
            crcChain.push_back("gk104a");
            crcChain.push_back("kepler_a_amod");
            break;
        case KEPLER_C:
            crcChain.push_back("gk20aa");
            crcChain.push_back("gk110a");
            crcChain.push_back("gk104a");
            crcChain.push_back("kepler_a_amod");
            break;
        case MAXWELL_A:
            crcChain.push_back("gm108a");
            crcChain.push_back("maxwell_a_amod");
            //Temporarily chain to kepler, but remove once traces are properly gilded
            crcChain.push_back("gk20aa");
            crcChain.push_back("gk110a");
            crcChain.push_back("gk104a");
            crcChain.push_back("kepler_a_amod");
            break;
        case MAXWELL_B:
            crcChain.push_back("gm208a");
            crcChain.push_back("gm108a");
            crcChain.push_back("maxwell_a_amod");
            //Temporarily chain to kepler, but remove once traces are properly gilded
            crcChain.push_back("gk20aa");
            crcChain.push_back("gk110a");
            crcChain.push_back("gk104a");
            crcChain.push_back("kepler_a_amod");
            break;
        case PASCAL_A:
        case PASCAL_B:
            crcChain.push_back("gp108a");
            crcChain.push_back("pascal_a_amod");
            crcChain.push_back("gm208a");
            crcChain.push_back("gm108a");
            break;
        case VOLTA_A:
            crcChain.push_back("gv100a");
            crcChain.push_back("gp108a");
            break;
        case VOLTA_B:
            crcChain.push_back("gv000a");
            crcChain.push_back("gv100a");
            crcChain.push_back("gp108a");
            break;
        case TURING_A:
            crcChain.push_back("tu108a");
            crcChain.push_back("gv000a");
            crcChain.push_back("gv100a");
            crcChain.push_back("gp108a");
            break;
        case AMPERE_A:
        case AMPERE_B:
            crcChain.push_back("ga108a");
            crcChain.push_back("tu108a");
            break;
        case ADA_A:
            crcChain.push_back("ad108a");
            crcChain.push_back("ga108a");
            break;
        case HOPPER_A:
        case HOPPER_B:
            crcChain.push_back("gh100a");
            crcChain.push_back("ad108a");
            crcChain.push_back("ga108a");
            break;
        case BLACKWELL_A:
            crcChain.push_back("gb100a");
            crcChain.push_back("gh208a");
            crcChain.push_back("gh100a");
            crcChain.push_back("ad108a");
            break;            
        case LW95A1_TSEC:
            crcChain.push_back("lw95a1_tsec");
            crcChain.push_back("g98a");
            crcChain.push_back("lw50a");
            break;
        case FERMI_TWOD_A:
        case KEPLER_INLINE_TO_MEMORY_B:
            crcChain.push_back("");
            break;
        case GF106_DMA_DECOMPRESS:
            crcChain.push_back("gf106_dma_decompress");
            break;
        case LW86B6_VIDEO_COMPOSITOR:
            crcChain.push_back("lw86b6_video_compositor");
            break;
        default:
            return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}

RC Trace3DCrcChain::SetupPerfsimCrcChain(UINT32 classnum)
{
    CrcChain& crcChain = m_Class2CrcChain[classnum];

    crcChain.push_back(Utility::StrPrintf("gb100%s", Platform::ModelIdentifier()));
    crcChain.push_back(Utility::StrPrintf("gh208%s", Platform::ModelIdentifier()));
    crcChain.push_back(Utility::StrPrintf("gh100%s", Platform::ModelIdentifier()));
    crcChain.push_back(Utility::StrPrintf("ad108%s", Platform::ModelIdentifier()));
    crcChain.push_back(Utility::StrPrintf("ga108%s", Platform::ModelIdentifier()));
    crcChain.push_back(Utility::StrPrintf("ga100%s", Platform::ModelIdentifier()));
    crcChain.push_back(Utility::StrPrintf("tu108%s", Platform::ModelIdentifier()));  

    return SetupAmodelCrcChain(classnum);
}

RC Trace3DCrcChain::SetupNonAmodelCrcChain(UINT32 classnum)
{
    bool set_crc_head = false;
    const char* crc_head = "";

    // bug# 857294, crc chaining for gk20x
    // Unfortunately gk20x uses the KEPLER_B class, but as of gk20x, chains
    // differently from gk110.  gk20x will therefore use the full
    // kepler_b chain (gk20x -> gk20a -> gk110 -> ... ) while gk110 must
    // start at gk110
    if (m_Subdev && m_Subdev->HasBug(857294) && classnum == KEPLER_B)
    {
        set_crc_head = true;
        crc_head = "gk110";
    }

    if (set_crc_head)
    {
        InfoPrintf("Setting crc chain head to: %s\n", crc_head);
    }

    // assign proper crc chain, callwlate size
    CrcChain& crcChain = m_Class2CrcChain[classnum];
    UINT32 size = 0;
    const char** keys = NULL;

    if (EngineClasses::IsClassType("Lwdec", classnum) ||
        EngineClasses::IsClassType("Lwenc", classnum) ||
        EngineClasses::IsClassType("Lwjpg", classnum) ||
        EngineClasses::IsClassType("Ofa", classnum))
    {
        crcChain.push_back(EngineClasses::GetClassName(classnum));
    }
    else if (EngineClasses::IsClassType("Ce", classnum))
    {
        crcChain.push_back("gt212_copy_engine");
        crcChain.push_back("gt212a_copy_engine");
    }
    else if (EngineClasses::IsClassType("Compute", classnum))
    {
        crcChain.push_back("");
    }
    else
    {
        switch (classnum)
        {
        case FERMI_A:
            keys = GF100_CRC_CHAIN;
            size = sizeof(GF100_CRC_CHAIN)/sizeof(char*);
            break;
        case FERMI_B:
            keys = GF108_CRC_CHAIN;
            size = sizeof(GF108_CRC_CHAIN)/sizeof(char*);
            break;
        case FERMI_C:
            keys = GF118_CRC_CHAIN;
            size = sizeof(GF118_CRC_CHAIN)/sizeof(char*);
            break;
        case KEPLER_A:
            keys = GK104_CRC_CHAIN;
            size = sizeof(GK104_CRC_CHAIN)/sizeof(char*);
            break;
        case KEPLER_B:
            keys = GK20X_CRC_CHAIN;
            size = sizeof(GK20X_CRC_CHAIN)/sizeof(char*);
            break;
        case KEPLER_C:
            keys = GK20A_CRC_CHAIN;
            size = sizeof(GK20A_CRC_CHAIN)/sizeof(char*);

            if (Platform::IsTegra())
            {
                //This is a terrible strategy for gilding rawImages since rawImages exposes
                //the underlying architecture of the chip and CLASS chaining, by definition,
                //should be independent of architecture.  We provide this solution only for
                //immediate colwenience to test owners and we should ultimately seek a more robust
                //solution that provides hints in the test itself (like -append_soc_crc_key "t124_raw")
                if (RawImageMode())
                {
                    crcChain.push_back("t124");
                }
            }
            break;
        case MAXWELL_A:
            keys = GM108_CRC_CHAIN;
            size = sizeof(GM108_CRC_CHAIN)/sizeof(char*);
            break;
        case MAXWELL_B:
            keys = GM208_CRC_CHAIN;
            size = sizeof(GM208_CRC_CHAIN)/sizeof(char*);

            if (Platform::IsTegra())
            {
                // Same hack for T210 as GK20A for T124.
                if (RawImageMode())
                {
                    crcChain.push_back("t210");
                }
            }
            break;
        case PASCAL_A:
        case PASCAL_B:
            keys = GP108_CRC_CHAIN;
            size = sizeof(GP108_CRC_CHAIN)/sizeof(char*);
            break;
        case VOLTA_A:
            keys = GV100_CRC_CHAIN;
            size = sizeof(GV100_CRC_CHAIN)/sizeof(char*);

            if (m_Subdev->HasFeature(Device::GPUSUB_IS_STANDALONE_TEGRA_GPU) || m_Subdev->IsSOC())
            {
                // Similar hack to the T124 and T210 ones above. Due to raw image
                // changes, GV11B and T194 also needs its own crc key. Bug 1930577 has some
                // dislwssions about what strategy to use for raw images going
                // forward to avoid adding more such hacks in the future.
                if (RawImageMode())
                {
                    crcChain.push_back("gv11b");
                }
            }
            break;
        case VOLTA_B:
            keys = GV208_CRC_CHAIN;
            size = sizeof(GV208_CRC_CHAIN)/sizeof(char*);
            break;
        case TURING_A:
            keys = TU108_CRC_CHAIN;
            size = sizeof(TU108_CRC_CHAIN)/sizeof(char*);
            break;
        case AMPERE_A:
        case AMPERE_B:
            keys = GA108_CRC_CHAIN;
            size = sizeof(GA108_CRC_CHAIN) / sizeof(char*);
            break;
        case ADA_A:
            keys = AD108_CRC_CHAIN;
            size = sizeof(AD108_CRC_CHAIN) / sizeof(char*);
            break;
        case HOPPER_A:
            keys = GH100_CRC_CHAIN;
            size = sizeof(GH100_CRC_CHAIN) / sizeof(char*);
            break;
        case HOPPER_B:
            keys = GH20X_CRC_CHAIN;
            size = sizeof(GH20X_CRC_CHAIN) / sizeof(char*);
            break;
        case BLACKWELL_A:
            keys = GB100_CRC_CHAIN;
            size = sizeof(GB100_CRC_CHAIN) / sizeof(char*);
            break;
        case LW95A1_TSEC:
            keys = SEC_CRC_CHAIN;
            size = sizeof(SEC_CRC_CHAIN)/sizeof(char*);
            break;
        case FERMI_TWOD_A:
            crcChain.push_back("fermi_twod_a");
            break;
        case KEPLER_INLINE_TO_MEMORY_B:
            crcChain.push_back("");
            break;
        case GF106_DMA_DECOMPRESS:
            crcChain.push_back("gf106_dma_decompress");
            break;
        case LW04_SOFTWARE_TEST:
            crcChain.push_back("lw04_software_test");
            break;
        case LW86B6_VIDEO_COMPOSITOR:
            crcChain.push_back("lw86b6_video_compositor");
            break;
        default:
            return RC::SOFTWARE_ERROR;
        }
    }

    // build crc chain
    for(UINT32 i = 0; i < size; ++i)
    {
        if(set_crc_head)
        {
            if(strcmp(crc_head, keys[i]) == 0)
                set_crc_head = false;
            else
                continue;
        }

        crcChain.push_back(keys[i]);
    }

    if(set_crc_head)
    {
        ErrPrintf("Illegal crc head: %s  detected for chip: %s\n",
            crc_head, m_Subdev->DeviceName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

bool Trace3DCrcChain::RawImageMode()
{
    return m_Params && m_Params->ParamPresent("-RawImageMode") &&
        (_RAWMODE)m_Params->ParamUnsigned("-RawImageMode") == RAWON;
}
