/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/fuse.h"
#include "gpu/include/testdevice.h"

#include <map>
#include <memory>

//------------------------------------------------------------------------------
// Factory methods for creating Fuse objects
//------------------------------------------------------------------------------
#if defined(INCLUDE_DGPU)

#include "device/interface/lwlink.h"
#include "gpu/fuse/gpufuse.h"
#include "gpu/fuse/gpufuseadapter.h"
#include "gpu/fuse/newfuse.h"

#if defined(INCLUDE_LWLINK)
    #include "gpu/fuse/lwlink_fuse.h"
    #include "gpu/fuse/lwlinkfuseadapter.h"
#endif

// SkuHandler Implementers
#include "gpu/fuse/skuhandling/basicskuhandler.h"
#include "gpu/fuse/skuhandling/stubskuhandler.h"

// FuseEncoder Implementers
#include "gpu/fuse/encoding/columnfuseencoder.h"
#include "gpu/fuse/encoding/maxwellffencoder.h"
#include "gpu/fuse/encoding/ga10xffencoder.h"
#include "gpu/fuse/encoding/hopperffencoder.h"
#include "gpu/fuse/encoding/iffencoder.h"
#if LWCFG(GLOBAL_ARCH_ADA)
    #include "gpu/fuse/encoding/ad10xiffencoder.h"
#endif
#include "gpu/fuse/encoding/hopperiffencoder.h"
#include "gpu/fuse/encoding/rirencoder.h"
#include "gpu/fuse/encoding/splitmacroencoder.h"
#include "gpu/fuse/encoding/stubfuseencoder.h"

// FuseHwAccess Implementers
#if LWCFG(GLOBAL_ARCH_ADA)
    #include "gpu/fuse/hwaccess/ad10xfuseaccessor.h"
#endif
#include "gpu/fuse/hwaccess/amperefuseaccessor.h"
#include "gpu/fuse/hwaccess/dummyfuseaccessor.h"
#include "gpu/fuse/hwaccess/fpfaccessor.h"
#include "gpu/fuse/hwaccess/ga10xfuseaccessor.h"
#include "gpu/fuse/hwaccess/gpufuseaccessor.h"
#include "gpu/fuse/hwaccess/hopperfuseaccessor.h"
#include "gpu/fuse/hwaccess/riraccessor.h"
#include "gpu/fuse/hwaccess/stubhwaccessor.h"
#if defined(INCLUDE_LWLINK)
    #include "gpu/fuse/hwaccess/lwswitchfuseaccessor.h"
    #include "gpu/fuse/hwaccess/lrfuseaccessor.h"
    #include "gpu/fuse/hwaccess/lsfuseaccessor.h"
#endif

// Downbin Implementers
#include "gpu/fuse/downbin/downbinimpl.h"
#include "gpu/fuse/downbin/ga10xdownbinimpl.h"
#include "gpu/fuse/downbin/hopperdownbinimpl.h"

//-----------------------------------------------------------------------------
// TODO: Migrate all the logic from GpuFuse/LwLinkFuse
// into the new framework and delete these two functions
unique_ptr<Fuse> CreateGpuFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);
    return make_unique<GpuFuseAdapter>(reinterpret_cast<GpuSubdevice*>(pDev));
}

//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateLwSwitchFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);

#if defined(INCLUDE_LWLINK)
    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors;
    if (FuseUtils::GetUseDummyFuses())
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<DummyFuseAccessor>(pDev, 256);
    }
    else
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<LwSwitchFuseAccessor>(pDev);
    }
    auto pFuseEncoder = make_shared<SplitMacroEncoder>
    (
        pDev,
        hwAccessors,
        make_unique<MaxwellFuselessFuseEncoder>(),
        make_unique<IffEncoder>(),
        nullptr,
        nullptr
    );
    pFuseEncoder->ForceOrderedFFRecords(true);
    auto pSkuHandler = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);

    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors), nullptr);
#else
    return nullptr;
#endif
}

//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateLimerockFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);

#if defined(INCLUDE_LWLINK)
    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors;
    if (FuseUtils::GetUseDummyFuses())
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<DummyFuseAccessor>(pDev, 512);
    }
    else
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<LrFuseAccessor>(pDev);
    }
    auto pFuseEncoder = make_shared<SplitMacroEncoder>
    (
        pDev,
        hwAccessors,
        make_unique<MaxwellFuselessFuseEncoder>(),
        make_unique<IffEncoder>(),
        nullptr,
        nullptr
    );
    pFuseEncoder->ForceOrderedFFRecords(true);
    auto pSkuHandler = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);

    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors), nullptr);
#else
    return nullptr;
#endif
}

//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateLagunaFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);

#if defined(INCLUDE_LWLINK)
    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors;
    if (FuseUtils::GetUseDummyFuses())
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<DummyFuseAccessor>(pDev, 512);
    }
    else
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<LsFuseAccessor>(pDev);
    }
    auto pFuseEncoder = make_shared<SplitMacroEncoder>
    (
        pDev,
        hwAccessors,
        make_unique<HopperFuselessFuseEncoder>(),
        make_unique<HopperIffEncoder>(),
        nullptr,
        nullptr
    );
    pFuseEncoder->ForceOrderedFFRecords(true);
    auto pSkuHandler = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);

    // Ignore fuses during SKU matching
    // The FSP section in the fuse macro has read protections and always all Fs
    pSkuHandler->AddReadProtectedFuse("FSP_LW_STATIC");
    pSkuHandler->AddReadProtectedFuse("FSP_ODM_FIELD");
    pSkuHandler->AddReadProtectedFuse("FSP_ODM_STATIC");

    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors), nullptr);
#else
    return nullptr;
#endif
}

//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateNullFuse(TestDevice* pDev)
{
    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors =
    {
        { FuseMacroType::Fuse, make_shared<StubFuseHwAccessor>() }
    };
    auto pFuseEncoder = make_unique<StubFuseEncoder>();
    auto pSkuHandler = make_unique<StubSkuHandler>();
    return make_unique<NewFuse>(pDev,
        move(pSkuHandler), move(pFuseEncoder), move(hwAccessors), nullptr);
}
//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateMaxwellFuse(TestDevice* pDev)
{
    return CreateGpuFuse(pDev);

/*    MASSERT(pDev != nullptr);
    auto devId = pDev->GetDeviceId();

    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors =
    {
        { FuseMacroType::Fuse, make_shared<GpuFuseAccessor>(pDev) }
    };
    auto pFuseEncoder = make_unique<StubFuseEncoder>();
    auto pSkuHandler = make_unique<StubSkuHandler>();
    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors));
*/
}
//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreatePascalFuse(TestDevice* pDev)
{
    return CreateGpuFuse(pDev);

    // MASSERT(pDev != nullptr);
    // auto devId = pDev->GetDeviceId();

    // map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors =
    // {
       // { FuseMacroType::Fuse, make_shared<GpuFuseAccessor>(pDev) }
    // };

    // auto pFuseEncoder = make_shared<SplitMacroEncoder>
    // (
        // devId,
        // hwAccessors,
        // make_unique<MaxwellFuselessFuseEncoder>(),
        // make_unique<IffEncoder>(),
        // nullptr,
        // nullptr
    // );
    // auto pSkuHandler = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);
    // return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                // move(pFuseEncoder), move(hwAccessors));
}
//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateVoltaFuse(TestDevice* pDev)
{
    return CreateGpuFuse(pDev);

    // MASSERT(pDev != nullptr);
    // auto devId = pDev->GetDeviceId();

    // map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors =
    // {
       // { FuseMacroType::Fuse, make_shared<GpuFuseAccessor>(pDev) }
    // };

    // auto pFuseEncoder = make_shared<SplitMacroEncoder>
    // (
        // devId,
        // hwAccessors,
        // make_unique<MaxwellFuselessFuseEncoder>(),
        // make_unique<IffEncoder>(),
        // nullptr,
        // nullptr
    // );
    // auto pSkuHandler = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);
    // return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                // move(pFuseEncoder), move(hwAccessors));
}
//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateTuringFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);

    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors;
    if (FuseUtils::GetUseDummyFuses())
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<DummyFuseAccessor>(pDev, 256);
        hwAccessors[FuseMacroType::Fpf]  = make_shared<DummyFuseAccessor>(pDev, 128);
        hwAccessors[FuseMacroType::Rir]  = make_shared<DummyFuseAccessor>(pDev, 4);
    }
    else
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<GpuFuseAccessor>(pDev);
        hwAccessors[FuseMacroType::Fpf]  = make_shared<FpfAccessor>(pDev);
        hwAccessors[FuseMacroType::Rir]  = make_shared<RirAccessor>(pDev);
    }
    auto pFuseEncoder = make_shared<SplitMacroEncoder>
    (
        pDev,
        hwAccessors,
        make_unique<MaxwellFuselessFuseEncoder>(),
        make_unique<IffEncoder>(),
        make_unique<ColumnFuseEncoder>(),
        make_unique<RirEncoder>()
    );
    auto pSkuHandler = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);

    // Turing permanent ignore fuses
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ");
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ_CP");
    pFuseEncoder->PostSltIgnoreFuse("OPT_KAPPA_INFO");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO0");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO1");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO2");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SRAM_VMIN_BIN");

    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors), nullptr);
}

//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateAmpereFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);

    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors;
    if (FuseUtils::GetUseDummyFuses())
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<DummyFuseAccessor>(pDev, 512);
        hwAccessors[FuseMacroType::Rir]  = make_shared<DummyFuseAccessor>(pDev, 8);
    }
    else
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<AmpereFuseAccessor>(pDev);
        hwAccessors[FuseMacroType::Rir]  = make_shared<RirAccessor>(pDev);
    }
    auto pFuseEncoder = make_shared<SplitMacroEncoder>
    (
        pDev,
        hwAccessors,
        make_unique<MaxwellFuselessFuseEncoder>(),
        make_unique<IffEncoder>(),
        nullptr,
        nullptr   // RIR is not to be supported on Ampere. See http://lwbugs/2697050
    );
    auto pSkuHandler  = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);

    // Permanent ignore fuses
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ");
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ_CP");
    pFuseEncoder->PostSltIgnoreFuse("OPT_KAPPA_INFO");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO0");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO1");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO2");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SRAM_VMIN_BIN");

    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors), nullptr);
}

//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateGA10xFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);

    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors;
    if (FuseUtils::GetUseDummyFuses())
    {
        // Number of fuse rows = 768 (Ref : GA10x fuse IAS)
        hwAccessors[FuseMacroType::Fuse] = make_shared<DummyFuseAccessor>(pDev, 768);
    }
    else
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<GA10xFuseAccessor>(pDev);
    }
    auto pFuseEncoder = make_shared<SplitMacroEncoder>
    (
        pDev,
        hwAccessors,
        make_unique<GA10xFuselessFuseEncoder>(),
        make_unique<IffEncoder>(),
        nullptr,
        nullptr   // No RIR on GA10x
    );
    auto pSkuHandler  = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);

    // Permanent ignore fuses
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ");
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ_CP");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO0");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO1");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO2");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SRAM_VMIN_BIN");

    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors), nullptr);
}

//-----------------------------------------------------------------------------
#if LWCFG(GLOBAL_ARCH_ADA)
unique_ptr<Fuse> CreateAD10xFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);

    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors;
    if (FuseUtils::GetUseDummyFuses())
    {
        // Number of fuse rows = 1024 (Same as GH100)
        hwAccessors[FuseMacroType::Fuse] = make_shared<DummyFuseAccessor>(pDev, 1024);
    }
    else
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<AD10xFuseAccessor>(pDev);
    }
    auto pFuseEncoder = make_shared<SplitMacroEncoder>
    (
        pDev,
        hwAccessors,
        make_unique<GA10xFuselessFuseEncoder>(),
        make_unique<AD10xIffEncoder>(),
        nullptr,
        nullptr   // No RIR on AD10x
    );
    auto pSkuHandler  = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);
    auto pDownbinImpl = make_unique<GA10xDownbinImpl>(pDev);

    // Permanent ignore fuses
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ");
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ_CP");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO0");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO1");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO2");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SRAM_VMIN_BIN");

    // Ignore fuses during SW fusing
    pSkuHandler->AddSwFusingIgnoreFuse("OPT_SLT_DONE");
    pSkuHandler->AddSwFusingIgnoreFuse("OPT_XTAL16_FSM_DISABLE");
    pSkuHandler->AddSwFusingIgnoreFuse("OPT_SELWRE_XVE_RESET_CTRL_WR_SELWRE");
    pSkuHandler->AddSwFusingIgnoreFuse("DISABLE_SW_OVERRIDE");
    pSkuHandler->AddSwFusingIgnoreFuse("OPT_ROW_REMAPPER_EN");

    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors), move(pDownbinImpl));
}
#endif

//-----------------------------------------------------------------------------
unique_ptr<Fuse> CreateHopperFuse(TestDevice* pDev)
{
    MASSERT(pDev != nullptr);

    map<FuseMacroType, shared_ptr<FuseHwAccessor>> hwAccessors;
    if (FuseUtils::GetUseDummyFuses())
    {
        // Number of fuse rows = 1024 (Ref : GH100 fuse IAS)
        hwAccessors[FuseMacroType::Fuse] = make_shared<DummyFuseAccessor>(pDev, 1024);
    }
    else
    {
        hwAccessors[FuseMacroType::Fuse] = make_shared<HopperFuseAccessor>(pDev);
    }
    auto pFuseEncoder = make_shared<SplitMacroEncoder>
    (
        pDev,
        hwAccessors,
        make_unique<HopperFuselessFuseEncoder>(),
        make_unique<HopperIffEncoder>(),
        nullptr,
        nullptr   // No RIR on GH100
    );
    auto pSkuHandler  = make_unique<BasicSkuHandler>(pDev, pFuseEncoder);
    auto pDownbinImpl = make_unique<GH100DownbinImpl>(pDev);

    // Permanent ignore fuses
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ");
    pFuseEncoder->PostSltIgnoreFuse("OPT_IDDQ_CP");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO0");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO1");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SPEEDO2");
    pFuseEncoder->PostSltIgnoreFuse("OPT_SRAM_VMIN_BIN");

    // Ignore fuses during SW fusing
    pSkuHandler->AddSwFusingIgnoreFuse("OPT_SLT_DONE");
    pSkuHandler->AddSwFusingIgnoreFuse("OPT_XTAL16_FSM_DISABLE");
    pSkuHandler->AddSwFusingIgnoreFuse("OPT_SELWRE_XVE_RESET_CTRL_WR_SELWRE");
    pSkuHandler->AddSwFusingIgnoreFuse("DISABLE_SW_OVERRIDE");
    pSkuHandler->AddSwFusingIgnoreFuse("OPT_ROW_REMAPPER_EN");

    // The FSP section in the fuse macro has read protections and always reads all Fs
    pSkuHandler->AddReadProtectedFuse("FSP_LW_STATIC");
    pSkuHandler->AddReadProtectedFuse("FSP_ODM_FIELD");
    pSkuHandler->AddReadProtectedFuse("FSP_ODM_STATIC");

    return make_unique<NewFuse>(pDev, move(pSkuHandler),
                                move(pFuseEncoder), move(hwAccessors), move(pDownbinImpl));
}

//-----------------------------------------------------------------------------
#else

// Stub functions for non dGPU builds
unique_ptr<Fuse> CreateGpuFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateLwSwitchFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateLimerockFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateLagunaFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateNullFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateFermiKeplerFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateMaxwellFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreatePascalFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateVoltaFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateTuringFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateAmpereFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateGA10xFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateAD10xFuse(TestDevice* pDev) { return nullptr; }
unique_ptr<Fuse> CreateHopperFuse(TestDevice* pDev) { return nullptr; }

#endif
