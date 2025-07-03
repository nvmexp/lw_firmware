/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "subdevgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "cheetah/include/tegra_gpu_file.h"
#include "class/cl2080.h" // LW20_SUBDEVICE_0
#include "class/cl90e0.h" // GF100_SUBDEVICE_GRAPHICS
#include "class/cla0e0.h" // GK110_SUBDEVICE_GRAPHICS
#include "class/clc0e0.h" // GP100_SUBDEVICE_GRAPHICS
#include "class/clc3e0.h" // GV100_SUBDEVICE_GRAPHICS
#include "ctrl/ctrl2080.h" // LW2080_CTRL_CMD_ECC*
#include "ctrl/ctrl90e0.h" // GF100_SUBDEVICE_GRAPHICS
#include "ctrl/ctrla0e0.h" // GK110_SUBDEVICE_GRAPHICS
#include "ctrl/ctrlc0e0.h" // GP100_SUBDEVICE_GRAPHICS
#include "ctrl/ctrlc3e0.h" // GV100_SUBDEVICE_GRAPHICS

RC SubdeviceGraphics::GetL1cDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetSHMDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetTexDetailedEccCounts(GetTexDetailedEccCountsParams *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetL1DataDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetL1TagDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetCbuDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetSmIcacheDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetGccL15DetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetGpcMmuDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetHubMmuL2TlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetHubMmuHubTlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetHubMmuFillUnitDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetGpccsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetFecsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetSmRamsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetL1cDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetRfDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetSHMDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetTexDetailedAggregateEccCounts(GetTexDetailedEccCountsParams *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetL1DataDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetL1TagDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetCbuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetSmIcacheDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetGccL15DetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetGpcMmuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetHubMmuL2TlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetHubMmuHubTlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetHubMmuFillUnitDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetGpccsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetFecsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }
RC SubdeviceGraphics::GetSmRamsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
    { return RC::UNSUPPORTED_FUNCTION; }

//--------------------------------------------------------------------
//! \brief Abstraction layer around GF100_SUBDEVICE_GRAPHICS class
//!
class GF100SubdeviceGraphics : public SubdeviceGraphics
{
public:
    GF100SubdeviceGraphics(GpuSubdevice *pGpuSubdevice);
    virtual RC GetL1cDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetSHMDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetTexDetailedEccCounts(GetTexDetailedEccCountsParams *pParams);

    virtual RC GetL1cDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetRfDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetSHMDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetTexDetailedAggregateEccCounts(
        GetTexDetailedEccCountsParams *pParams);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around GK110_SUBDEVICE_GRAPHICS class
//!
class GK110SubdeviceGraphics : public SubdeviceGraphics
{
public:
    GK110SubdeviceGraphics(GpuSubdevice *pGpuSubdevice);
    virtual RC GetL1cDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetTexDetailedEccCounts(GetTexDetailedEccCountsParams *pParams);
    virtual RC GetSHMDetailedEccCounts(Ecc::DetailedErrCounts *pParams);

    virtual RC GetL1cDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetRfDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetSHMDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetTexDetailedAggregateEccCounts(
        GetTexDetailedEccCountsParams *pParams);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around GP100_SUBDEVICE_GRAPHICS class
//!
class GP100SubdeviceGraphics : public SubdeviceGraphics
{
public:
    GP100SubdeviceGraphics(GpuSubdevice *pGpuSubdevice);
    virtual RC GetL1cDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetTexDetailedEccCounts(GetTexDetailedEccCountsParams *pParams);
    virtual RC GetSHMDetailedEccCounts(Ecc::DetailedErrCounts *pParams);

    virtual RC GetL1cDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetRfDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetSHMDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetTexDetailedAggregateEccCounts(
        GetTexDetailedEccCountsParams *pParams);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around GV100_SUBDEVICE_GRAPHICS class
//!
class GV100SubdeviceGraphics : public SubdeviceGraphics
{
public:
    GV100SubdeviceGraphics(GpuSubdevice *pGpuSubdevice);
    virtual RC GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetL1DataDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetL1TagDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetCbuDetailedEccCounts(Ecc::DetailedErrCounts *pParams);

    virtual RC GetRfDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_LRF); }
    virtual RC GetL1DataDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_L1DATA); }
    virtual RC GetL1TagDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_L1TAG); }
    virtual RC GetCbuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_CBU); }

private:
    RC GetDetailedAggregateEccCounts(Ecc::DetailedErrCounts* pParams, LW2080_ECC_UNITS unit);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around LW2080 ECC controls
//!
class GenericSubdeviceGraphics : public SubdeviceGraphics
{
public:
    GenericSubdeviceGraphics(GpuSubdevice *pGpuSubdevice);
    virtual RC GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_LRF); }
    virtual RC GetL1DataDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_L1DATA); }
    virtual RC GetL1TagDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_L1TAG); }
    virtual RC GetCbuDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_CBU); }
    virtual RC GetL1cDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_L1); }
    virtual RC GetSHMDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_SHM); }
    virtual RC GetSmIcacheDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_SM_ICACHE); }
    virtual RC GetGccL15DetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_GCC_L15); }
    virtual RC GetGpcMmuDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_GPCMMU); }
    virtual RC GetHubMmuL2TlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_HUBMMU_L2TLB); }
    virtual RC GetHubMmuHubTlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_HUBMMU_HUBTLB); }
    virtual RC GetHubMmuFillUnitDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_HUBMMU_FILLUNIT); }
    virtual RC GetGpccsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_GPCCS); }
    virtual RC GetFecsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_FECS); }
    virtual RC GetSmRamsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedEccCounts(pParams, ECC_UNIT_SM_RAMS); }

    virtual RC GetRfDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_LRF); }
    virtual RC GetL1DataDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_L1DATA); }
    virtual RC GetL1TagDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_L1TAG); }
    virtual RC GetCbuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_CBU); }
    virtual RC GetL1cDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_L1); }
    virtual RC GetSHMDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_SHM); }
    virtual RC GetSmIcacheDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_SM_ICACHE); }
    virtual RC GetGccL15DetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_GCC_L15); }
    virtual RC GetGpcMmuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_GPCMMU); }
    virtual RC GetHubMmuL2TlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_HUBMMU_L2TLB); }
    virtual RC GetHubMmuHubTlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_HUBMMU_HUBTLB); }
    virtual RC GetHubMmuFillUnitDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_HUBMMU_FILLUNIT); }
    virtual RC GetGpccsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_GPCCS); }
    virtual RC GetFecsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_FECS); }
    virtual RC GetSmRamsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
        { return GetDetailedAggregateEccCounts(pParams, ECC_UNIT_SM_RAMS); }

    virtual RC GetTexDetailedEccCounts(GetTexDetailedEccCountsParams *pParams);
    virtual RC GetTexDetailedAggregateEccCounts(GetTexDetailedEccCountsParams *pParams);

private:
    //! Virtual GPCs that are valid to collect ECC counts from.
    vector<UINT32> m_ValidVirtualGpcs;

    UINT32 GetSublocationCount(UINT32 gpc, LW2080_ECC_UNITS unit);
    RC GetDetailedEccCounts(Ecc::DetailedErrCounts* pParams, LW2080_ECC_UNITS unit);
    RC GetDetailedAggregateEccCounts(Ecc::DetailedErrCounts* pParams, LW2080_ECC_UNITS unit);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around GPU driver sysfs nodes
//!
class TegraSubdeviceGraphics : public SubdeviceGraphics
{
public:
    explicit TegraSubdeviceGraphics(GpuSubdevice* pGpuSubdevice);

    RC GetL1cDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetSHMDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetTexDetailedEccCounts(GetTexDetailedEccCountsParams *pParams) override;
    RC GetL1DataDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetL1TagDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetCbuDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetSmIcacheDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetGccL15DetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetGpcMmuDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetHubMmuL2TlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetHubMmuHubTlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetHubMmuFillUnitDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetGpccsDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetFecsDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetSmRamsDetailedEccCounts(Ecc::DetailedErrCounts *pParams) override;

    RC GetL1cDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetRfDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetSHMDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetTexDetailedAggregateEccCounts(GetTexDetailedEccCountsParams *pParams) override;
    RC GetL1DataDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetL1TagDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetCbuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetSmIcacheDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetGccL15DetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetGpcMmuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetHubMmuL2TlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetHubMmuHubTlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetHubMmuFillUnitDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetGpccsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetFecsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;
    RC GetSmRamsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams) override;

private:
    RC GetEccCounts(const char* corrected, const char* uncorrected, Ecc::DetailedErrCounts* pParams);
    static string GetEccFileName(const char* pattern, UINT32 gpc, UINT32 tpc);
};

//--------------------------------------------------------------------
//! \brief Factory method to alloc the latest supported subclass
//!
/* static */ SubdeviceGraphics *SubdeviceGraphics::Alloc
(
    GpuSubdevice *pGpuSubdevice
)
{
    if (pGpuSubdevice->IsSOC() && Platform::UsesLwgpuDriver())
    {
        return new TegraSubdeviceGraphics(pGpuSubdevice);
    }

    if (pGpuSubdevice->HasFeature(Device::GPUSUB_SUPPORTS_RM_GENERIC_ECC))
    {
        return new GenericSubdeviceGraphics(pGpuSubdevice);
    }

    UINT32 classList[] =
    {
        GV100_SUBDEVICE_GRAPHICS,
        GP100_SUBDEVICE_GRAPHICS,
        GK110_SUBDEVICE_GRAPHICS,
        GF100_SUBDEVICE_GRAPHICS
    };
    UINT32 myClass;

    if (OK == LwRmPtr()->GetFirstSupportedClass(
            static_cast<UINT32>(NUMELEMS(classList)), classList, &myClass,
            pGpuSubdevice->GetParentDevice()))
    {
        switch (myClass)
        {
            case GV100_SUBDEVICE_GRAPHICS:
                return new GV100SubdeviceGraphics(pGpuSubdevice);
            case GP100_SUBDEVICE_GRAPHICS:
                return new GP100SubdeviceGraphics(pGpuSubdevice);
            case GK110_SUBDEVICE_GRAPHICS:
                return new GK110SubdeviceGraphics(pGpuSubdevice);
            case GF100_SUBDEVICE_GRAPHICS:
                return new GF100SubdeviceGraphics(pGpuSubdevice);
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------
// Constructors
//
SubdeviceGraphics::SubdeviceGraphics
(
    GpuSubdevice *pGpuSubdevice,
    UINT32 myClass
) :
    m_pGpuSubdevice(pGpuSubdevice),
    m_Class(myClass)
{
    MASSERT(m_pGpuSubdevice);
    MASSERT(m_Class);
}

GF100SubdeviceGraphics::GF100SubdeviceGraphics(GpuSubdevice *pGpuSubdevice) :
    SubdeviceGraphics(pGpuSubdevice, GF100_SUBDEVICE_GRAPHICS)
{
    MASSERT(LW90E0_CTRL_GR_ECC_GPC_COUNT >= pGpuSubdevice->GetGpcCount());
    for (UINT32 gpc = 0; gpc < pGpuSubdevice->GetGpcCount(); ++gpc)
    {
        MASSERT(LW90E0_CTRL_GR_ECC_TPC_COUNT >=
                pGpuSubdevice->GetTpcCountOnGpc(gpc));
    }
}

GK110SubdeviceGraphics::GK110SubdeviceGraphics(GpuSubdevice *pGpuSubdevice) :
    SubdeviceGraphics(pGpuSubdevice, GK110_SUBDEVICE_GRAPHICS)
{
    MASSERT(LWA0E0_CTRL_GR_L1C_ECC_MAX_COUNT >= pGpuSubdevice->GetTpcCount());
    MASSERT(LWA0E0_CTRL_GR_RF_ECC_MAX_COUNT >= pGpuSubdevice->GetTpcCount());
    MASSERT(LWA0E0_CTRL_GR_TEX_MAX_ERROR_COUNT >=
            pGpuSubdevice->GetTpcCount() * pGpuSubdevice->GetTexCountPerTpc());
}

GP100SubdeviceGraphics::GP100SubdeviceGraphics(GpuSubdevice *pGpuSubdevice) :
    SubdeviceGraphics(pGpuSubdevice, GP100_SUBDEVICE_GRAPHICS)
{
    MASSERT(LWC0E0_CTRL_GR_ECC_GPC_COUNT >= pGpuSubdevice->GetGpcCount());
    for (UINT32 gpc = 0; gpc < pGpuSubdevice->GetGpcCount(); ++gpc)
    {
        MASSERT(LWC0E0_CTRL_GR_ECC_TPC_COUNT >=
                pGpuSubdevice->GetTpcCountOnGpc(gpc));
    }
}

GV100SubdeviceGraphics::GV100SubdeviceGraphics(GpuSubdevice *pGpuSubdevice) :
    SubdeviceGraphics(pGpuSubdevice, GV100_SUBDEVICE_GRAPHICS)
{
    MASSERT(LWC3E0_CTRL_GR_ECC_GPC_COUNT >= pGpuSubdevice->GetGpcCount());
    for (UINT32 gpc = 0; gpc < pGpuSubdevice->GetGpcCount(); ++gpc)
    {
        MASSERT(LWC3E0_CTRL_GR_ECC_TPC_COUNT >=
                pGpuSubdevice->GetTpcCountOnGpc(gpc));
    }
}

GenericSubdeviceGraphics::GenericSubdeviceGraphics(GpuSubdevice *pGpuSubdevice) :
    SubdeviceGraphics(pGpuSubdevice, LW20_SUBDEVICE_0)
{
    m_ValidVirtualGpcs = Ecc::GetValidVirtualGpcs(pGpuSubdevice);
}

//--------------------------------------------------------------------
// Colwenience methods to resize & zero-fill the ECC data vectors.
// Avoid clear(), to save time if the caller allocated the vectors.
//

// For rectangular 2D data
template<class DATA_STRUCT> static void ResizeAndClearEccData
(
    vector<vector<DATA_STRUCT> > *pVector,
    UINT32 size1,
    UINT32 size2
)
{
    static const DATA_STRUCT ZERO_DATA = {0};
    pVector->resize(size1);
    for (UINT32 ii = 0; ii < size1; ++ii)
    {
        (*pVector)[ii].resize(size2);
        fill((*pVector)[ii].begin(), (*pVector)[ii].end(), ZERO_DATA);
    }
}

// For rectangular 3D data
template<class DATA_STRUCT> static void ResizeAndClearEccData
(
    vector<vector<vector<DATA_STRUCT> > > *pVector,
    UINT32 size1,
    UINT32 size2,
    UINT32 size3
)
{
    pVector->resize(size1);
    for (UINT32 ii = 0; ii < size1; ++ii)
        ResizeAndClearEccData(&(*pVector)[ii], size2, size3);
}

// For irregular data with dimensions gpcCount x tpcCountOnGpc(gpc)
template<class DATA_STRUCT> static void ResizeAndClearEccData
(
    vector<vector<DATA_STRUCT> > *pVector,
    const GpuSubdevice *pGpuSubdevice
)
{
    static const DATA_STRUCT ZERO_DATA = {0};
    pVector->resize(pGpuSubdevice->GetGpcCount());
    for (UINT32 gpc = 0; gpc < pVector->size(); ++gpc)
    {
        (*pVector)[gpc].resize(pGpuSubdevice->GetTpcCountOnGpc(gpc));
        fill((*pVector)[gpc].begin(), (*pVector)[gpc].end(), ZERO_DATA);
    }
}

// For irregular data with dimensions gpcCount x tpcCountOnGpc(gpc) x size3
template<class DATA_STRUCT> static void ResizeAndClearEccData
(
    vector<vector<vector<DATA_STRUCT> > > *pVector,
    const GpuSubdevice *pGpuSubdevice,
    UINT32 size3
)
{
    pVector->resize(pGpuSubdevice->GetGpcCount());
    for (UINT32 gpc = 0; gpc < pVector->size(); ++gpc)
    {
        UINT32 tpcCount = pGpuSubdevice->GetTpcCountOnGpc(gpc);
        ResizeAndClearEccData(&(*pVector)[gpc], tpcCount, size3);
    }
}

//--------------------------------------------------------------------
// Wrappers around LW**E0_CTRL_CMD_GR_GET_L1C_ECC_COUNTS
//
/* virtual */ RC GF100SubdeviceGraphics::GetL1cDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E0 doesn't implement GET_L1C_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LW90E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.tpcCount = LW90E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LW90E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(rmParams.gpcCount == pGpuSubdevice->GetGpcCount());
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < pParams->eccCounts.size(); ++gpc)
    {
        MASSERT(rmParams.tpcCount >= pGpuSubdevice->GetTpcCountOnGpc(gpc));
        for (UINT32 tpc = 0; tpc < pParams->eccCounts[gpc].size(); ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].l1Sbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].l1Dbe.count;
        }
        for (size_t tpc = pParams->eccCounts[gpc].size();
             tpc < rmParams.tpcCount; ++tpc)
        {
            MASSERT(rmParams.gpcEcc[gpc][tpc].l1Sbe.count == 0);
            MASSERT(rmParams.gpcEcc[gpc][tpc].l1Dbe.count == 0);
        }
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceGraphics::GetL1cDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    LWA0E0_CTRL_GR_GET_L1C_ECC_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E0_CTRL_CMD_GR_GET_L1C_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E0_CTRL_GR_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E0_CTRL_GR_GET_L1C_ECC_COUNTS &entry = rmParams.l1cEcc[ii];
        MASSERT(entry.gpc < pParams->eccCounts.size());
        MASSERT(entry.tpc < pParams->eccCounts[entry.gpc].size());
        pParams->eccCounts[entry.gpc][entry.tpc].corr = entry.sbe;
        pParams->eccCounts[entry.gpc][entry.tpc].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceGraphics::GetL1cDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    // Pascal does not have L1C ECC
    //
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
// Wrappers around LW**E0_CTRL_CMD_GR_GET_RF_ECC_COUNTS
//
/* virtual */ RC GF100SubdeviceGraphics::GetRfDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E0 doesn't implement GET_RF_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LW90E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.tpcCount = LW90E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LW90E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(rmParams.gpcCount == pGpuSubdevice->GetGpcCount());
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < pParams->eccCounts.size(); ++gpc)
    {
        MASSERT(rmParams.tpcCount >= pGpuSubdevice->GetTpcCountOnGpc(gpc));
        for (UINT32 tpc = 0; tpc < pParams->eccCounts[gpc].size(); ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].rfSbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].rfDbe.count;
        }
        for (size_t tpc = pParams->eccCounts[gpc].size();
             tpc < rmParams.tpcCount; ++tpc)
        {
            MASSERT(rmParams.gpcEcc[gpc][tpc].rfSbe.count == 0);
            MASSERT(rmParams.gpcEcc[gpc][tpc].rfDbe.count == 0);
        }
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceGraphics::GetRfDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    LWA0E0_CTRL_GR_GET_RF_ECC_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E0_CTRL_CMD_GR_GET_RF_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E0_CTRL_GR_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E0_CTRL_GR_GET_RF_ECC_COUNTS &entry = rmParams.rfEcc[ii];
        MASSERT(entry.gpc < pParams->eccCounts.size());
        MASSERT(entry.tpc < pParams->eccCounts[entry.gpc].size());
        pParams->eccCounts[entry.gpc][entry.tpc].corr = entry.sbe;
        pParams->eccCounts[entry.gpc][entry.tpc].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceGraphics::GetRfDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C0E0 doesn't implement GET_RF_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LWC0E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.tpcCount = LWC0E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LWC0E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(rmParams.gpcCount == pGpuSubdevice->GetGpcCount());
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < pParams->eccCounts.size(); ++gpc)
    {
        MASSERT(rmParams.tpcCount >= pGpuSubdevice->GetTpcCountOnGpc(gpc));
        for (UINT32 tpc = 0; tpc < pParams->eccCounts[gpc].size(); ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].rfSbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].rfDbe.count;
        }
        for (size_t tpc = pParams->eccCounts[gpc].size();
             tpc < rmParams.tpcCount; ++tpc)
        {
            MASSERT(rmParams.gpcEcc[gpc][tpc].rfSbe.count == 0);
            MASSERT(rmParams.gpcEcc[gpc][tpc].rfDbe.count == 0);
        }
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceGraphics::GetRfDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C3E0 doesn't implement GET_RF_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LWC3E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = { };
    rmParams.tpcCount = LWC3E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LWC3E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC3E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    UINT32 numGpc = pGpuSubdevice->GetGpcCount();
    MASSERT(rmParams.gpcCount >= numGpc);
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < rmParams.gpcCount; ++gpc)
    {
        UINT32 numTpc = gpc < numGpc ? pGpuSubdevice->GetTpcCountOnGpc(gpc) : 0;
        MASSERT(rmParams.tpcCount >= numTpc);
        for (UINT32 tpc = 0; tpc < rmParams.tpcCount; ++tpc)
        {
            if (gpc < numGpc && tpc < numTpc)
            {
                pParams->eccCounts[gpc][tpc].corr =
                    rmParams.gpcEccCounts[gpc][tpc].rfCorrectedTotal.count;
                pParams->eccCounts[gpc][tpc].uncorr =
                    rmParams.gpcEccCounts[gpc][tpc].rfUncorrectedTotal.count;
            }
            else
            {
                MASSERT(rmParams.gpcEccCounts[gpc][tpc].rfCorrectedTotal.count == 0);
                MASSERT(rmParams.gpcEccCounts[gpc][tpc].rfUncorrectedTotal.count == 0);
            }
        }
    }
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC GF100SubdeviceGraphics::GetSHMDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E0 doesn't implement GET_SHM_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LW90E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.tpcCount = LW90E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LW90E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(rmParams.gpcCount == pGpuSubdevice->GetGpcCount());
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < pParams->eccCounts.size(); ++gpc)
    {
        MASSERT(rmParams.tpcCount >= pGpuSubdevice->GetTpcCountOnGpc(gpc));
        for (UINT32 tpc = 0; tpc < pParams->eccCounts[gpc].size(); ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].shmSbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].shmDbe.count;
        }
        for (size_t tpc = pParams->eccCounts[gpc].size();
             tpc < rmParams.tpcCount; ++tpc)
        {
            MASSERT(rmParams.gpcEcc[gpc][tpc].shmSbe.count == 0);
            MASSERT(rmParams.gpcEcc[gpc][tpc].shmDbe.count == 0);
        }
    }
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC GK110SubdeviceGraphics::GetSHMDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    LWA0E0_CTRL_GR_GET_SHM_ECC_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E0_CTRL_CMD_GR_GET_SHM_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E0_CTRL_GR_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E0_CTRL_GR_GET_SHM_ECC_COUNTS &entry = rmParams.shmEcc[ii];
        MASSERT(entry.gpc < pParams->eccCounts.size());
        MASSERT(entry.tpc < pParams->eccCounts[entry.gpc].size());
        pParams->eccCounts[entry.gpc][entry.tpc].corr = entry.sbe;
        pParams->eccCounts[entry.gpc][entry.tpc].uncorr = entry.dbe;
    }
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC GP100SubdeviceGraphics::GetSHMDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C0E0 doesn't implement GET_SHM_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LWC0E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.tpcCount = LWC0E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LWC0E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(rmParams.gpcCount == pGpuSubdevice->GetGpcCount());
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < pParams->eccCounts.size(); ++gpc)
    {
        MASSERT(rmParams.tpcCount >= pGpuSubdevice->GetTpcCountOnGpc(gpc));
        for (UINT32 tpc = 0; tpc < pParams->eccCounts[gpc].size(); ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].shmSbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].shmDbe.count;
        }
        for (size_t tpc = pParams->eccCounts[gpc].size();
             tpc < rmParams.tpcCount; ++tpc)
        {
            MASSERT(rmParams.gpcEcc[gpc][tpc].shmSbe.count == 0);
            MASSERT(rmParams.gpcEcc[gpc][tpc].shmDbe.count == 0);
        }
    }
    return rc;
}

//--------------------------------------------------------------------
// Wrappers around LW**E0_CTRL_CMD_GR_GET_TEX_ERROR_COUNTS
//
/* virtual */ RC GF100SubdeviceGraphics::GetTexDetailedEccCounts
(
    SubdeviceGraphics::GetTexDetailedEccCountsParams *pParams
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GK110SubdeviceGraphics::GetTexDetailedEccCounts
(
    SubdeviceGraphics::GetTexDetailedEccCountsParams *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    UINT32 numTexPerTpc = pGpuSubdevice->GetTexCountPerTpc();
    RC rc;

    LWA0E0_CTRL_GR_GET_TEX_ERROR_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E0_CTRL_CMD_GR_GET_TEX_ERROR_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E0_CTRL_GR_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice, numTexPerTpc);
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E0_CTRL_GR_GET_TEX_ERROR_COUNTS &entry =
            rmParams.texEcc[ii];
        MASSERT(entry.gpc < pParams->eccCounts.size());
        MASSERT(entry.tpc < pParams->eccCounts[entry.gpc].size());
        MASSERT(entry.tex < numTexPerTpc);
        pParams->eccCounts[entry.gpc][entry.tpc][entry.tex].corr =
            entry.correctable;
        pParams->eccCounts[entry.gpc][entry.tpc][entry.tex].uncorr =
            entry.unCorrectable;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceGraphics::GetTexDetailedEccCounts
(
    SubdeviceGraphics::GetTexDetailedEccCountsParams *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    UINT32 numTexPerTpc = pGpuSubdevice->GetTexCountPerTpc();

    LWC0E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    rmParams.gpcCount = LWC0E0_CTRL_GR_ECC_GPC_COUNT;
    rmParams.tpcCount = LWC0E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.texCount = LWC0E0_CTRL_GR_ECC_TEX_COUNT;

    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice, numTexPerTpc);
    for (UINT32 gpc = 0; gpc < pParams->eccCounts.size(); ++gpc)
    {
        for (UINT32 tpc = 0; tpc < pParams->eccCounts[gpc].size(); ++tpc)
        {
            for (UINT32 tex = 0; tex < pParams->eccCounts[gpc][tpc].size(); ++tex)
            {
                pParams->eccCounts[gpc][tpc][tex].corr =
                    rmParams.gpcEcc[gpc][tpc].tex[tex].correctable.count;
                pParams->eccCounts[gpc][tpc][tex].uncorr =
                    rmParams.gpcEcc[gpc][tpc].tex[tex].unCorrectable.count;
            }
        }
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceGraphics::GetL1DataDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C3E0 doesn't implement GET_RF_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LWC3E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = { };
    rmParams.tpcCount = LWC3E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LWC3E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC3E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    UINT32 numGpc = pGpuSubdevice->GetGpcCount();
    MASSERT(rmParams.gpcCount >= numGpc);
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < rmParams.gpcCount; ++gpc)
    {
        UINT32 numTpc = gpc < numGpc ? pGpuSubdevice->GetTpcCountOnGpc(gpc) : 0;
        MASSERT(rmParams.tpcCount >= numTpc);
        for (UINT32 tpc = 0; tpc < rmParams.tpcCount; ++tpc)
        {
            if (gpc < numGpc && tpc < numTpc)
            {
                pParams->eccCounts[gpc][tpc].corr =
                    rmParams.gpcEccCounts[gpc][tpc].l1DataCorrectedTotal.count;
                pParams->eccCounts[gpc][tpc].uncorr =
                    rmParams.gpcEccCounts[gpc][tpc].l1DataUncorrectedTotal.count;
            }
            else
            {
                MASSERT(rmParams.gpcEccCounts[gpc][tpc].l1DataCorrectedTotal.count == 0);
                MASSERT(rmParams.gpcEccCounts[gpc][tpc].l1DataUncorrectedTotal.count == 0);
            }
        }
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceGraphics::GetL1TagDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C3E0 doesn't implement GET_RF_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LWC3E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = { };
    rmParams.tpcCount = LWC3E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LWC3E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC3E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    UINT32 numGpc = pGpuSubdevice->GetGpcCount();
    MASSERT(rmParams.gpcCount >= numGpc);
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < rmParams.gpcCount; ++gpc)
    {
        UINT32 numTpc = gpc < numGpc ? pGpuSubdevice->GetTpcCountOnGpc(gpc) : 0;
        MASSERT(rmParams.tpcCount >= numTpc);
        for (UINT32 tpc = 0; tpc < rmParams.tpcCount; ++tpc)
        {
            if (gpc < numGpc && tpc < numTpc)
            {
                pParams->eccCounts[gpc][tpc].corr =
                    rmParams.gpcEccCounts[gpc][tpc].l1TagCorrectedTotal.count;
                pParams->eccCounts[gpc][tpc].uncorr =
                    rmParams.gpcEccCounts[gpc][tpc].l1TagUncorrectedTotal.count;
            }
            else
            {
                MASSERT(rmParams.gpcEccCounts[gpc][tpc].l1TagCorrectedTotal.count == 0);
                MASSERT(rmParams.gpcEccCounts[gpc][tpc].l1TagUncorrectedTotal.count == 0);
            }
        }
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceGraphics::GetCbuDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C3E0 doesn't implement GET_RF_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LWC3E0_CTRL_GR_GET_ECC_COUNTS_PARAMS rmParams = { };
    rmParams.tpcCount = LWC3E0_CTRL_GR_ECC_TPC_COUNT;
    rmParams.gpcCount = LWC3E0_CTRL_GR_ECC_GPC_COUNT;
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC3E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    UINT32 numGpc = pGpuSubdevice->GetGpcCount();
    MASSERT(rmParams.gpcCount >= numGpc);
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);
    for (UINT32 gpc = 0; gpc < rmParams.gpcCount; ++gpc)
    {
        UINT32 numTpc = gpc < numGpc ? pGpuSubdevice->GetTpcCountOnGpc(gpc) : 0;
        MASSERT(rmParams.tpcCount >= numTpc);
        for (UINT32 tpc = 0; tpc < rmParams.tpcCount; ++tpc)
        {
            if (gpc < numGpc && tpc < numTpc)
            {
                pParams->eccCounts[gpc][tpc].uncorr =
                    rmParams.gpcEccCounts[gpc][tpc].cbuUncorrectedTotal.count;
            }
            else
            {
                MASSERT(rmParams.gpcEccCounts[gpc][tpc].cbuUncorrectedTotal.count == 0);
            }
        }
    }
    return rc;
}

//--------------------------------------------------------------------
// Wrappers around LW**E0_CTRL_CMD_GR_GET_AGGREGATE_L1C_ECC_COUNTS
//
/* virtual */ RC GF100SubdeviceGraphics::GetL1cDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E0 doesn't implement GET_AGGREGATE_L1C_ECC_COUNTS;
    // use GET_AGGREGATE_ECC_COUNTS instead
    //
    LW90E0_CTRL_GR_GET_AGGREGATE_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E0_CTRL_CMD_GR_GET_AGGREGATE_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts, LW90E0_CTRL_GR_ECC_GPC_COUNT,
                          LW90E0_CTRL_GR_ECC_TPC_COUNT);
    for (UINT32 gpc = 0; gpc < LW90E0_CTRL_GR_ECC_GPC_COUNT; ++gpc)
    {
        for (UINT32 tpc = 0; tpc < LW90E0_CTRL_GR_ECC_TPC_COUNT; ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].l1Sbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].l1Dbe.count;
        }
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceGraphics::GetL1cDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // Get data from RM
    //
    LWA0E0_CTRL_GR_GET_AGGREGATE_L1C_ECC_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E0_CTRL_CMD_GR_GET_AGGREGATE_L1C_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E0_CTRL_GR_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    // Resize the output array to the floorswept GPU size, or the
    // data size returned by the RM, whichever is larger.
    //
    UINT32 numGpcs = pGpuSubdevice->GetGpcCount();
    UINT32 numTpcs = 0;
    for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
    {
        if (numTpcs < pGpuSubdevice->GetTpcCountOnGpc(gpc))
            numTpcs = pGpuSubdevice->GetTpcCountOnGpc(gpc);
    }
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        if (numGpcs < rmParams.l1cEcc[ii].gpc)
            numGpcs = rmParams.l1cEcc[ii].gpc;
        if (numGpcs < rmParams.l1cEcc[ii].gpc)
            numGpcs = rmParams.l1cEcc[ii].gpc;
        if (numTpcs < rmParams.l1cEcc[ii].tpc)
            numTpcs = rmParams.l1cEcc[ii].tpc;
    }
    ResizeAndClearEccData(&pParams->eccCounts, numGpcs, numTpcs);

    // Fill the output array
    //
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E0_CTRL_GR_GET_L1C_ECC_COUNTS &entry = rmParams.l1cEcc[ii];
        pParams->eccCounts[entry.gpc][entry.tpc].corr = entry.sbe;
        pParams->eccCounts[entry.gpc][entry.tpc].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceGraphics::GetL1cDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    // Pascal does not have L1C ECC
    //
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
// Wrappers around LW**E0_CTRL_CMD_GR_GET_AGGREGATE_RF_ECC_COUNTS
//
/* virtual */ RC GF100SubdeviceGraphics::GetRfDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E0 doesn't implement GET_AGGREGATE_RF_ECC_COUNTS;
    // use GET_AGGREGATE_ECC_COUNTS instead
    //
    LW90E0_CTRL_GR_GET_AGGREGATE_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E0_CTRL_CMD_GR_GET_AGGREGATE_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts, LW90E0_CTRL_GR_ECC_GPC_COUNT,
                          LW90E0_CTRL_GR_ECC_TPC_COUNT);
    for (UINT32 gpc = 0; gpc < LW90E0_CTRL_GR_ECC_GPC_COUNT; ++gpc)
    {
        for (UINT32 tpc = 0; tpc < LW90E0_CTRL_GR_ECC_TPC_COUNT; ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].rfSbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].rfDbe.count;
        }
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceGraphics::GetRfDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // Get data from RM
    //
    LWA0E0_CTRL_GR_GET_AGGREGATE_RF_ECC_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E0_CTRL_CMD_GR_GET_AGGREGATE_RF_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E0_CTRL_GR_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    // Resize the output array to the floorswept GPU size, or the
    // data size returned by the RM, whichever is larger.
    //
    UINT32 numGpcs = pGpuSubdevice->GetGpcCount();
    UINT32 numTpcs = 0;
    for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
    {
        if (numTpcs < pGpuSubdevice->GetTpcCountOnGpc(gpc))
            numTpcs = pGpuSubdevice->GetTpcCountOnGpc(gpc);
    }
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        if (numGpcs < rmParams.rfEcc[ii].gpc)
            numGpcs = rmParams.rfEcc[ii].gpc;
        if (numTpcs < rmParams.rfEcc[ii].tpc)
            numTpcs = rmParams.rfEcc[ii].tpc;
    }
    ResizeAndClearEccData(&pParams->eccCounts, numGpcs, numTpcs);

    // Fill the output array
    //
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E0_CTRL_GR_GET_RF_ECC_COUNTS &entry = rmParams.rfEcc[ii];
        pParams->eccCounts[entry.gpc][entry.tpc].corr = entry.sbe;
        pParams->eccCounts[entry.gpc][entry.tpc].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceGraphics::GetRfDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C0E0 doesn't implement GET_AGGREGATE_RF_ECC_COUNTS;
    // use GET_AGGREGATE_ECC_COUNTS instead
    //
    LWC0E0_CTRL_GR_GET_AGGREGATE_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E0_CTRL_CMD_GR_GET_AGGREGATE_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts, LWC0E0_CTRL_GR_ECC_GPC_COUNT,
                          LWC0E0_CTRL_GR_ECC_TPC_COUNT);
    for (UINT32 gpc = 0; gpc < LWC0E0_CTRL_GR_ECC_GPC_COUNT; ++gpc)
    {
        for (UINT32 tpc = 0; tpc < LWC0E0_CTRL_GR_ECC_TPC_COUNT; ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].rfSbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].rfDbe.count;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC GV100SubdeviceGraphics::GetDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts* pParams,
    LW2080_ECC_UNITS unit
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numGpc = pGpuSubdevice->GetGpcCount();
    pParams->eccCounts.clear();

    LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams;

    for (UINT32 gpc = 0; gpc < numGpc; gpc++)
    {
        UINT32 numTpc = pGpuSubdevice->GetTpcCountOnGpc(gpc);
        pParams->eccCounts.emplace_back(numTpc);
        for (UINT32 tpc = 0; tpc < numTpc; tpc++)
        {
            memset(&rmParams, 0, sizeof(rmParams));
            rmParams.unit = unit;
            rmParams.location.lrf.location = gpc;
            rmParams.location.lrf.sublocation = tpc;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
                     &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[gpc][tpc].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[gpc][tpc].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

/* virtual */ RC GenericSubdeviceGraphics::GetTexDetailedEccCounts
(
    SubdeviceGraphics::GetTexDetailedEccCountsParams *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numTexPerTpc = pGpuSubdevice->GetTexCountPerTpc();
    LW2080_CTRL_ECC_GET_DETAILED_ERROR_COUNTS_PARAMS rmParams = { };

    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice, numTexPerTpc);

    // Only populate the entries corresponding to valid GPCs
    for (UINT32 gpc : m_ValidVirtualGpcs)
    {
        UINT32 numTpc = pGpuSubdevice->GetTpcCountOnGpc(gpc);
        for (UINT32 tpc = 0; tpc < numTpc; tpc++)
        {
            for (UINT32 tex = 0; tex < numTexPerTpc; tex++)
            {
                memset(&rmParams, 0, sizeof(rmParams));
                rmParams.unit = ECC_UNIT_TEX;
                rmParams.location.tex.grLocation.location = gpc;
                rmParams.location.tex.grLocation.sublocation = tpc;
                rmParams.location.tex.texId = tex;

                CHECK_RC(LwRmPtr()->ControlBySubdevice(
                    pGpuSubdevice,
                    LW2080_CTRL_CMD_ECC_GET_DETAILED_ERROR_COUNTS,
                    &rmParams, sizeof(rmParams)));

                MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

                pParams->eccCounts[gpc][tpc][tex].corr =
                    rmParams.eccCounts.correctedTotalCounts;
                pParams->eccCounts[gpc][tpc][tex].uncorr =
                    rmParams.eccCounts.uncorrectedTotalCounts;
            }
        }
    }
    return rc;
}

//--------------------------------------------------------------------
RC GenericSubdeviceGraphics::GetTexDetailedAggregateEccCounts
(
    GetTexDetailedEccCountsParams *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numTexPerTpc = pGpuSubdevice->GetTexCountPerTpc();
    LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams = { };

    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice, numTexPerTpc);

    // Only populate the entries corresponding to valid GPCs
    for (UINT32 gpc : m_ValidVirtualGpcs)
    {
        UINT32 numTpc = pGpuSubdevice->GetTpcCountOnGpc(gpc);
        for (UINT32 tpc = 0; tpc < numTpc; tpc++)
        {
            for (UINT32 tex = 0; tex < numTexPerTpc; tex++)
            {
                memset(&rmParams, 0, sizeof(rmParams));
                rmParams.unit = ECC_UNIT_TEX;
                rmParams.location.tex.grLocation.location = gpc;
                rmParams.location.tex.grLocation.sublocation = tpc;
                rmParams.location.tex.texId = tex;

                CHECK_RC(LwRmPtr()->ControlBySubdevice(
                    pGpuSubdevice,
                    LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
                    &rmParams, sizeof(rmParams)));

                MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

                pParams->eccCounts[gpc][tpc][tex].corr =
                    rmParams.eccCounts.correctedTotalCounts;
                pParams->eccCounts[gpc][tpc][tex].uncorr =
                    rmParams.eccCounts.uncorrectedTotalCounts;
            }
        }
    }
    return rc;
}

//--------------------------------------------------------------------
UINT32 GenericSubdeviceGraphics::GetSublocationCount(UINT32 gpc, LW2080_ECC_UNITS unit)
{
    if (unit == ECC_UNIT_GPCMMU)
        return GetGpuSubdevice()->GetMmuCountPerGpc(gpc);
    return GetGpuSubdevice()->GetTpcCountOnGpc(gpc);
}

//--------------------------------------------------------------------
RC GenericSubdeviceGraphics::GetDetailedEccCounts
(
    Ecc::DetailedErrCounts* pParams
   ,LW2080_ECC_UNITS unit
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numGpc = pGpuSubdevice->GetGpcCount();
    pParams->eccCounts.clear();

    LW2080_CTRL_ECC_GET_DETAILED_ERROR_COUNTS_PARAMS rmParams = { };

    for (UINT32 gpc = 0; gpc < numGpc; ++gpc)
    {
        UINT32 numSublocations = GetSublocationCount(gpc, unit);
        pParams->eccCounts.emplace_back(numSublocations);

        if (!Utility::Contains(m_ValidVirtualGpcs, gpc))
        {
            continue; // Not a queryable ECC location, skip
        }

        for (UINT32 sublocation = 0; sublocation < numSublocations; sublocation++)
        {
            memset(&rmParams, 0, sizeof(rmParams));
            rmParams.unit = unit;
            rmParams.location.lrf.location = gpc;
            rmParams.location.lrf.sublocation = sublocation;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                pGpuSubdevice,
                LW2080_CTRL_CMD_ECC_GET_DETAILED_ERROR_COUNTS,
                &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[gpc][sublocation].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[gpc][sublocation].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
RC GenericSubdeviceGraphics::GetDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts* pParams
   ,LW2080_ECC_UNITS unit
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numGpc = pGpuSubdevice->GetGpcCount();
    pParams->eccCounts.clear();

    LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams = { };

    for (UINT32 gpc = 0; gpc < numGpc; ++gpc)
    {
        UINT32 numSublocations = GetSublocationCount(gpc, unit);
        pParams->eccCounts.emplace_back(numSublocations);

        if (!Utility::Contains(m_ValidVirtualGpcs, gpc))
        {
            continue; // Not a queryable ECC location, skip
        }

        for (UINT32 sublocation = 0; sublocation < numSublocations; sublocation++)
        {
            memset(&rmParams, 0, sizeof(rmParams));
            rmParams.unit = unit;
            rmParams.location.lrf.location = gpc;
            rmParams.location.lrf.sublocation = sublocation;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                pGpuSubdevice,
                LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
                &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[gpc][sublocation].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[gpc][sublocation].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC GF100SubdeviceGraphics::GetSHMDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E0 doesn't implement GET_AGGREGATE_SHM_ECC_COUNTS;
    // use GET_AGGREGATE_ECC_COUNTS instead
    //

    LW90E0_CTRL_GR_GET_AGGREGATE_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E0_CTRL_CMD_GR_GET_AGGREGATE_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts, LW90E0_CTRL_GR_ECC_GPC_COUNT,
                          LW90E0_CTRL_GR_ECC_TPC_COUNT);
    for (UINT32 gpc = 0; gpc < LW90E0_CTRL_GR_ECC_GPC_COUNT; ++gpc)
    {
        for (UINT32 tpc = 0; tpc < LW90E0_CTRL_GR_ECC_TPC_COUNT; ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].shmSbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].shmDbe.count;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
/* virtual */ RC GK110SubdeviceGraphics::GetSHMDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // Get data from RM
    //
    LWA0E0_CTRL_GR_GET_AGGREGATE_SHM_ECC_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E0_CTRL_CMD_GR_GET_AGGREGATE_SHM_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E0_CTRL_GR_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    // Resize the output array to the floorswept GPU size, or the
    // data size returned by the RM, whichever is larger.
    //
    UINT32 numGpcs = pGpuSubdevice->GetGpcCount();
    UINT32 numTpcs = 0;
    for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
    {
        if (numTpcs < pGpuSubdevice->GetTpcCountOnGpc(gpc))
            numTpcs = pGpuSubdevice->GetTpcCountOnGpc(gpc);
    }
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        if (numGpcs < rmParams.shmEcc[ii].gpc)
            numGpcs = rmParams.shmEcc[ii].gpc;
        if (numGpcs < rmParams.shmEcc[ii].gpc)
            numGpcs = rmParams.shmEcc[ii].gpc;
        if (numTpcs < rmParams.shmEcc[ii].tpc)
            numTpcs = rmParams.shmEcc[ii].tpc;
    }
    ResizeAndClearEccData(&pParams->eccCounts, numGpcs, numTpcs);

    // Fill the output array
    //
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E0_CTRL_GR_GET_SHM_ECC_COUNTS &entry = rmParams.shmEcc[ii];
        pParams->eccCounts[entry.gpc][entry.tpc].corr = entry.sbe;
        pParams->eccCounts[entry.gpc][entry.tpc].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceGraphics::GetSHMDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C0E0 doesn't implement GET_AGGREGATE_SHM_ECC_COUNTS;
    // use GET_AGGREGATE_ECC_COUNTS instead
    //

    LWC0E0_CTRL_GR_GET_AGGREGATE_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E0_CTRL_CMD_GR_GET_AGGREGATE_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts, LWC0E0_CTRL_GR_ECC_GPC_COUNT,
                          LWC0E0_CTRL_GR_ECC_TPC_COUNT);
    for (UINT32 gpc = 0; gpc < LWC0E0_CTRL_GR_ECC_GPC_COUNT; ++gpc)
    {
        for (UINT32 tpc = 0; tpc < LWC0E0_CTRL_GR_ECC_TPC_COUNT; ++tpc)
        {
            pParams->eccCounts[gpc][tpc].corr =
                rmParams.gpcEcc[gpc][tpc].shmSbe.count;
            pParams->eccCounts[gpc][tpc].uncorr =
                rmParams.gpcEcc[gpc][tpc].shmDbe.count;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
// Wrappers around LW**E0_CTRL_CMD_GR_GET_AGGREGATE_TEX_ERROR_COUNTS
//
/* virtual */ RC GF100SubdeviceGraphics::GetTexDetailedAggregateEccCounts
(
    SubdeviceGraphics::GetTexDetailedEccCountsParams *pParams
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GK110SubdeviceGraphics::GetTexDetailedAggregateEccCounts
(
    SubdeviceGraphics::GetTexDetailedEccCountsParams *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // Get data from RM
    //
    LWA0E0_CTRL_GR_GET_AGGREGATE_TEX_ERROR_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E0_CTRL_CMD_GR_GET_AGGREGATE_TEX_ERROR_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E0_CTRL_GR_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    // Resize the output array to the floorswept GPU size, or the
    // data size returned by the RM, whichever is larger.
    //
    UINT32 numGpcs = pGpuSubdevice->GetGpcCount();
    UINT32 numTpcs = 0;
    UINT32 numTexs = pGpuSubdevice->GetTexCountPerTpc();
    for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
    {
        if (numTpcs < pGpuSubdevice->GetTpcCountOnGpc(gpc))
            numTpcs = pGpuSubdevice->GetTpcCountOnGpc(gpc);
    }
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        if (numGpcs < rmParams.texEcc[ii].gpc)
            numGpcs = rmParams.texEcc[ii].gpc;
        if (numTpcs < rmParams.texEcc[ii].tpc)
            numTpcs = rmParams.texEcc[ii].tpc;
        if (numTexs < rmParams.texEcc[ii].tex)
            numTexs = rmParams.texEcc[ii].tex;
    }
    ResizeAndClearEccData(&pParams->eccCounts, numGpcs, numTpcs, numTexs);

    // Fill the output array
    //
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E0_CTRL_GR_GET_TEX_ERROR_COUNTS &entry =
            rmParams.texEcc[ii];
        pParams->eccCounts[entry.gpc][entry.tpc][entry.tex].corr =
            entry.correctable;
        pParams->eccCounts[entry.gpc][entry.tpc][entry.tex].uncorr =
            entry.unCorrectable;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceGraphics::GetTexDetailedAggregateEccCounts
(
    SubdeviceGraphics::GetTexDetailedEccCountsParams *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // C0E0 doesn't implement GET_AGGREGATE_RF_ECC_COUNTS;
    // use GET_AGGREGATE_ECC_COUNTS instead
    //
    LWC0E0_CTRL_GR_GET_AGGREGATE_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E0_CTRL_CMD_GR_GET_AGGREGATE_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts, LWC0E0_CTRL_GR_ECC_GPC_COUNT,
                          LWC0E0_CTRL_GR_ECC_TPC_COUNT,
                          LWC0E0_CTRL_GR_ECC_TEX_COUNT);
    for (UINT32 gpc = 0; gpc < LWC0E0_CTRL_GR_ECC_GPC_COUNT; ++gpc)
    {
        for (UINT32 tpc = 0; tpc < LWC0E0_CTRL_GR_ECC_TPC_COUNT; ++tpc)
        {
            for (UINT32 tex = 0; tex < LWC0E0_CTRL_GR_ECC_TEX_COUNT; ++tex)
            {
                pParams->eccCounts[gpc][tpc][tex].corr =
                    rmParams.gpcEcc[gpc][tpc].tex[tex].correctable.count;
                pParams->eccCounts[gpc][tpc][tex].uncorr =
                    rmParams.gpcEcc[gpc][tpc].tex[tex].unCorrectable.count;
            }
        }
    }
    return rc;
}

TegraSubdeviceGraphics::TegraSubdeviceGraphics(GpuSubdevice* pGpuSubdevice)
    : SubdeviceGraphics(pGpuSubdevice, GK110_SUBDEVICE_GRAPHICS)
{
}

string TegraSubdeviceGraphics::GetEccFileName(const char* pattern, UINT32 gpc, UINT32 tpc)
{
    string fileName(pattern);

    const size_t gpcPos = fileName.find("%G");
    if (gpcPos != string::npos)
    {
        const string gpcStr = Utility::StrPrintf("%u", gpc);
        fileName.replace(gpcPos, 2, gpcStr);
    }

    const size_t tpcPos = fileName.find("%T");
    if (tpcPos != string::npos)
    {
        const string tpcStr = Utility::StrPrintf("%u", tpc);
        fileName.replace(tpcPos, 2, tpcStr);
    }

    return fileName;
}

RC TegraSubdeviceGraphics::GetEccCounts
(
    const char*             corrected,
    const char*             uncorrected,
    Ecc::DetailedErrCounts* pParams
)
{
    GpuSubdevice* const pGpuSubdevice = GetGpuSubdevice();
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice);

    const bool hasGpc = strstr(corrected, "%G") != nullptr;
    const bool hasTpc = strstr(corrected, "%T") != nullptr;

    const UINT32 numGpcs = pGpuSubdevice->GetGpcCount();
    for (UINT32 gpc = 0; gpc < numGpcs; gpc++)
    {
        const UINT32 numTpcs = pGpuSubdevice->GetTpcCountOnGpc(gpc);
        for (UINT32 tpc = 0; tpc < numTpcs; tpc++)
        {
            RC rc;
            string file = GetEccFileName(corrected, gpc, tpc);
            CHECK_RC(CheetAh::GetTegraEccCount(file, &pParams->eccCounts[gpc][tpc].corr));
            file = GetEccFileName(uncorrected, gpc, tpc);
            CHECK_RC(CheetAh::GetTegraEccCount(file, &pParams->eccCounts[gpc][tpc].uncorr));

            // Run at least once
            if (!hasTpc)
            {
                break;
            }
        }

        // Run at least once
        if (!hasGpc && numTpcs)
        {
            break;
        }
    }

    return RC::OK;
}

RC TegraSubdeviceGraphics::GetL1cDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    ResizeAndClearEccData(&pParams->eccCounts, GetGpuSubdevice());
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetRfDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_tpc%T_sm_lrf_ecc_single_err_count",
                        "gpc%G_tpc%T_sm_lrf_ecc_double_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetSHMDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_tpc%T_sm_shm_ecc_sec_count",
                        "gpc%G_tpc%T_sm_shm_ecc_ded_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetTexDetailedEccCounts(GetTexDetailedEccCountsParams *pParams)
{
    GpuSubdevice* const pGpuSubdevice = GetGpuSubdevice();
    const UINT32        numTexPerTpc  = pGpuSubdevice->GetTexCountPerTpc();
    ResizeAndClearEccData(&pParams->eccCounts, pGpuSubdevice, numTexPerTpc);

    const UINT32 gpcCount = pGpuSubdevice->GetGpcCount();
    for (UINT32 gpc = 0; gpc < gpcCount; ++gpc)
    {
        const UINT32 tpcCount = pGpuSubdevice->GetTpcCountOnGpc(gpc);
        for (UINT32 tpc = 0; tpc < tpcCount; ++tpc)
        {
            for (UINT32 tt = 0; tt < numTexPerTpc; tt++)
            {
                RC rc;
                string file = Utility::StrPrintf("gpc%u_tpc%u_tex_ecc_total_sec_pipe%u_count", gpc, tpc, tt);
                CHECK_RC(CheetAh::GetTegraEccCount(file, &pParams->eccCounts[gpc][tpc][tt].corr));
                file = Utility::StrPrintf("gpc%u_tpc%u_tex_ecc_total_ded_pipe%u_count", gpc, tpc, tt);
                CHECK_RC(CheetAh::GetTegraEccCount(file, &pParams->eccCounts[gpc][tpc][tt].uncorr));
            }
        }
    }

    return RC::OK;
}

RC TegraSubdeviceGraphics::GetL1DataDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_tpc%T_sm_l1_data_ecc_corrected_err_count",
                        "gpc%G_tpc%T_sm_l1_data_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetL1TagDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_tpc%T_sm_l1_tag_ecc_corrected_err_count",
                        "gpc%G_tpc%T_sm_l1_tag_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetCbuDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_tpc%T_sm_cbu_ecc_corrected_err_count",
                        "gpc%G_tpc%T_sm_cbu_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetSmIcacheDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_tpc%T_sm_icache_ecc_corrected_err_count",
                        "gpc%G_tpc%T_sm_icache_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetGccL15DetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_gcc_l15_ecc_corrected_err_count",
                        "gpc%G_gcc_l15_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetGpcMmuDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    RC rc;
    CHECK_RC(GetEccCounts("gpc%G_mmu_l1tlb_ecc_corrected_err_count",
                          "gpc%G_mmu_l1tlb_ecc_uncorrected_err_count",
                          pParams));
    for (auto& eccCounts : pParams->eccCounts)
    {
        eccCounts.resize(1);
    }
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetHubMmuL2TlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("mmu_l2tlb_ecc_corrected_err_count",
                        "mmu_l2tlb_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetHubMmuHubTlbDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("mmu_hubtlb_ecc_corrected_err_count",
                        "mmu_hubtlb_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetHubMmuFillUnitDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("mmu_fillunit_ecc_corrected_err_count",
                        "mmu_fillunit_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetGpccsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_gpccs_ecc_corrected_err_count",
                        "gpc%G_gpccs_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetFecsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gr0_fecs_ecc_corrected_err_count",
                        "gr0_fecs_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetSmRamsDetailedEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return GetEccCounts("gpc%G_tpc%T_sm_rams_ecc_corrected_err_count",
                        "gpc%G_tpc%T_sm_rams_ecc_uncorrected_err_count",
                        pParams);
}

RC TegraSubdeviceGraphics::GetL1cDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetRfDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetSHMDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetTexDetailedAggregateEccCounts(GetTexDetailedEccCountsParams *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetL1DataDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetL1TagDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetCbuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetSmIcacheDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetGccL15DetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetGpcMmuDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetHubMmuL2TlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetHubMmuHubTlbDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetHubMmuFillUnitDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetGpccsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetFecsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}

RC TegraSubdeviceGraphics::GetSmRamsDetailedAggregateEccCounts(Ecc::DetailedErrCounts *pParams)
{
    return RC::OK;
}
