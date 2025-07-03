/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "smmudrv.h"

#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/xp.h"
#include "device/interface/pcie.h"
#include "hwref/g00x/g000/address_map_new.h"
#include "mdiag/sysspec.h"
#include "lwmisc.h"  //REF_VAL

#include <functional>

extern SObject Mdiag_Object;
extern RC LoadDLLVerbose(const char * filename, void **pmodule);

void * SmmuDrv::s_SmmuDrvModule = nullptr;

static SProperty Mdiag_SmmuStage
(
    Mdiag_Object,
    "SmmuStage",
    0,
    1,
    0,
    0,
    0,
    "Set smmu translation stage for ats stage and inline smmu stage: 1=STAGE1_ONLY, 3=STAGE1_STAGE2"
);

static SProperty Mdiag_BypassInlineSmmuStage1Translation
(
    Mdiag_Object,
    "BypassInlineSmmuStage1Translation",
    0,
    false,
    0,
    0,
    0,
    "Bypass inline smmu(device context) stage1 translation"
);

static SProperty Mdiag_SmmuS2PageSize
(
    Mdiag_Object,
    "SmmuS2PageSize",
    0,
    512*1024,
    0,
    0,
    0,
    "Set the page size for smmu stage2 (in KB)"
);

template <typename SmmuDrvApiFn>
class SmmuDrvApi
{
public:
    template <typename ...Args>
    static RC Execute(const char* apiName, Args&&... args)
    {
        SmmuDrvApiFn pSmmuFunc = reinterpret_cast<SmmuDrvApiFn>(
                Xp::GetDLLProc(SmmuDrv::s_SmmuDrvModule, apiName));
        MASSERT(pSmmuFunc);
        int result = pSmmuFunc(std::forward<Args>(args)...);
        if (result == 0)
        {
            return OK;
        }
        else
        {
            ErrPrintf("SmmuDrvApi %s fail!\n", apiName);
            return RC::SOFTWARE_ERROR;
        }
    }
};

SmmuDrv::SmmuDrv()
{
    m_SmmuStage = STAGE1_ONLY;
    m_SmmuS2PageSize = SmmuPageSize_512_MB;
    m_BypassS1TranslationForInline = false;

    RC rc = Init();
    MASSERT((rc == OK) && "Fail to initialize smmu driver. \n");
}

SmmuDrv::~SmmuDrv()
{
    RC rc = Cleanup();
    MASSERT((rc == OK) && "Fail to cleanup smmu driver. \n");

    s_SmmuDrvModule = nullptr;
}

#define ZERO_BASE_ADDR 0
static map<string, UINT64> s_ApertureBase = {
    {"Local",  LW_ADDRESS_MAP_GPU_SOCKET0BASE},
    {"Peer0",  LW_ADDRESS_MAP_GPU_SOCKET1BASE},
    {"Peer1",  LW_ADDRESS_MAP_GPU_SOCKET2BASE},
    {"Peer2",  LW_ADDRESS_MAP_GPU_SOCKET3BASE},
    {"Sysmem", ZERO_BASE_ADDR}
};

RC SmmuDrv::CreateAtsMapping
(
    const string & surfaceName,
    const string & addressSpaceName,
    const string & aperture,
    const Pcie * pcie,
    UINT32 testId,
    UINT32 pasid,
    UINT64 atsPageSize,
    UINT64 virtAddr,
    UINT64 physAddr,
    bool atsReadPermission,
    bool atsWritePermission
)
{
    RC rc;

    SmmuPageSize s1PageSize = static_cast<SmmuPageSize>(atsPageSize);

    AtsPermission atsPermission = ColwertAtsPermission(atsReadPermission, atsWritePermission);

    using SmmuCreateAtsMapping =
        bool (*)(UINT32, UINT32, UINT64, UINT64, SmmuPageSize, AtsPermission);
    if (s_ApertureBase.find(aperture) != s_ApertureBase.end())
    {
        physAddr += s_ApertureBase[aperture];
    }
    else
    {
        MASSERT(!"Invalid aperture.\n");
    }
    CHECK_RC(SmmuDrvApi<SmmuCreateAtsMapping>::Execute(
        "RTSmmuCreateAtsMapping", GetBdf(pcie), pasid, virtAddr, physAddr, s1PageSize, atsPermission));

    return OK;
}

RC SmmuDrv::UpdateAtsMapping
(
    const string & addressSpaceName,
    const string & aperture,
    const Pcie * pcie,
    UINT32 pasid,
    UINT64 virtAddr,
    UINT64 physAddr,
    IommuDrv::MappingType updateType,
    bool atsReadPermission,
    bool atsWritePermission
)
{
    RC rc;

    AtsPermission atsPermission = ColwertAtsPermission(atsReadPermission, atsWritePermission);

    using SmmuUpdateAtsMapping =
        bool (*)(UINT32, UINT32, UINT64, UINT64, MappingType, AtsPermission);
    if (s_ApertureBase.find(aperture) != s_ApertureBase.end())
    {
        physAddr += s_ApertureBase[aperture];
    }
    else
    {
        MASSERT(!"Invalid aperture.\n");
    }
    CHECK_RC(SmmuDrvApi<SmmuUpdateAtsMapping>::Execute(
        "RTSmmuUpdateAtsMapping", GetBdf(pcie), pasid, virtAddr, physAddr, updateType, atsPermission));

    return OK;
}

RC SmmuDrv::UpdateAtsPermission
(
    const string & addressSpaceName,
    const string & permissionType,
    const Pcie * pcie,
    UINT32 pasid,
    UINT64 virtAddr,
    UINT64 physAddr,
    bool permissiolwalue,
    bool atsReadPermission,
    bool atsWritePermission
)
{
    RC rc;

    AtsPermission atsPermission = ReadWrite;
    if (permissionType == "read" && permissiolwalue)
    {
        atsPermission = ColwertAtsPermission(true, atsWritePermission);
    }
    else if (permissionType == "read" && !permissiolwalue)
    {
        atsPermission = ColwertAtsPermission(false, atsWritePermission);
    }
    else if (permissionType == "write" && permissiolwalue)
    {
        atsPermission = ColwertAtsPermission(atsReadPermission, true);
    }
    else
    {
        atsPermission = ColwertAtsPermission(atsReadPermission, false);
    }

    using SmmuUpdateAtsMapping = 
        bool (*)(UINT32, UINT32, UINT64, UINT64, MappingType, AtsPermission);
    CHECK_RC(SmmuDrvApi<SmmuUpdateAtsMapping>::Execute(
        "RTSmmuUpdateAtsMapping", GetBdf(pcie), pasid, virtAddr, physAddr, RemapPage, atsPermission));

    return OK;
}

RC SmmuDrv::AllocDevVaSpace(const Pcie * pcie)
{
    RC rc;

    // The interface defined in smmu driver
    // void RTSmmuInitStream(LwU32 bdf, TRANS_CONFIG stream_config, Smmu_PAGE_SIZE s1_granule_size, bool bypassS1devContxt);
    // s1_granule_size is reserved in smmu driver code, while mods is suggested to hard code with SmmuPageSize_64_KB.

    // Initialize smmu stage2 translation table
    using SmmuInitStream = bool (*)(UINT32, TransConfig, SmmuPageSize, bool);
    CHECK_RC(SmmuDrvApi<SmmuInitStream>::Execute(
        "RTSmmuInitStream", GetBdf(pcie), m_SmmuStage, SmmuPageSize_64_KB, m_BypassS1TranslationForInline));

    // Initialize smmu stage1 translation table for device context
    static const UINT32 pasidForDevCtx = 0;
    CHECK_RC(AllocProcessVaSpace(pcie, pasidForDevCtx));

    return OK;
}

RC SmmuDrv::AllocProcessVaSpace(const Pcie * pcie, UINT32 pasid)
{
    RC rc;

    using SmmuInitSubStream = bool (*)(UINT32, UINT32);
    CHECK_RC(SmmuDrvApi<SmmuInitSubStream>::Execute(
        "RTSmmuInitSubStream", GetBdf(pcie), pasid));

    return OK;
}

RC SmmuDrv::Init()
{
    RC rc;

    // Varify relevent argument
    CHECK_RC(ValidateTranslationConfig());

    // Load Smmu driver
    const char *SmmuDrvName = "runtest_mc.so";
    LoadDLLVerbose(SmmuDrvName, &s_SmmuDrvModule);

    using SmmuInit = bool (*)(SmmuPageSize);
    CHECK_RC(SmmuDrvApi<SmmuInit>::Execute("RTSmmuInit", m_SmmuS2PageSize));

    return OK;
}

RC SmmuDrv::Cleanup()
{
    RC rc;

    using SmmuDeInit = bool (*)();
    CHECK_RC(SmmuDrvApi<SmmuDeInit>::Execute("RTSmmuDeInit"));

    return OK;
}

UINT32 SmmuDrv::GetBdf(const Pcie * pcie)
{
    UINT32 bdf = Pci::GetConfigAddress(0,
                                       pcie->BusNumber(),
                                       pcie->DeviceNumber(),
                                       pcie->FunctionNumber(),
                                       0);

    // Smmu driver only cares about field bus, device, function.
    return REF_VAL(PCI_CONFIG_ADDRESS_BDF, bdf);
}

RC SmmuDrv::ValidateTranslationConfig()
{
    RC rc;
    JavaScriptPtr pJs;

    UINT32 smmuStage;
    UINT32 smmuS2PageSize;

    CHECK_RC(pJs->GetProperty(Mdiag_Object, Mdiag_SmmuStage, &smmuStage));
    CHECK_RC(pJs->GetProperty(Mdiag_Object, Mdiag_BypassInlineSmmuStage1Translation, &m_BypassS1TranslationForInline));
    CHECK_RC(pJs->GetProperty(Mdiag_Object, Mdiag_SmmuS2PageSize, &smmuS2PageSize));

    m_SmmuStage = static_cast<TransConfig>(smmuStage);
    m_SmmuS2PageSize = static_cast<SmmuPageSize>(smmuS2PageSize << 10);

    // The contraints to ats stage and inline smmu stage
    // Reference: https://confluence.lwpu.com/pages/viewpage.action?pageId=337465647#ATS/SMMUSupportProposal-SMMUDriverAPI
    MASSERT (m_SmmuStage == STAGE1_ONLY || m_SmmuStage == STAGE1_STAGE2);

    return OK;
}

SmmuDrv::AtsPermission SmmuDrv::ColwertAtsPermission
(
    bool atsReadPermission,
    bool atsWritePermission
)
{
    AtsPermission atsPermission = ReadWrite;
    if (atsReadPermission && atsWritePermission)
    {
        atsPermission = ReadWrite;
    }
    else if (atsReadPermission && !atsWritePermission)
    {
        atsPermission = ReadOnly;
    }
    else if(!atsReadPermission && atsWritePermission)
    {
        atsPermission = WriteOnly;
    }
    else
    {
        MASSERT(!"Invalid ATS permission setting.\n");
    }    
    return atsPermission;
}

