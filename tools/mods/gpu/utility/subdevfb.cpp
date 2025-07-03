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

#include "subdevfb.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "cheetah/include/tegra_gpu_file.h"
#include "class/cl2080.h" // LW20_SUBDEVICE_0
#include "class/cl90e1.h" // GF100_SUBDEVICE_FB
#include "class/cla0e1.h" // GK110_SUBDEVICE_FB
#include "class/clc0e1.h" // GP100_SUBDEVICE_FB
#include "class/clc3e1.h" // GV100_SUBDEVICE_FB
#include "ctrl/ctrl2080.h" // LW2080_CTRL_CMD_ECC*
#include "ctrl/ctrl90e1.h" // GF100_SUBDEVICE_FB
#include "ctrl/ctrla0e1.h" // GK110_SUBDEVICE_FB
#include "ctrl/ctrlc0e1.h" // GP100_SUBDEVICE_FB
#include "ctrl/ctrlc3e1.h" // GV100_SUBDEVICE_FB

//--------------------------------------------------------------------
//! \brief Abstraction layer around GF100_SUBDEVICE_FB class
//!
class GF100SubdeviceFb : public SubdeviceFb
{
public:
    GF100SubdeviceFb(GpuSubdevice *pGpuSubdevice);
    virtual RC GetDramDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetLtcDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetEdcCounts(GetEdcCountsParams *pParams);
    virtual RC GetDramDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetLtcDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetEdcCrcData(GetEdcCrcDataParams *pParams);
    virtual RC GetLatestEccAddresses(GetLatestEccAddressesParams *pParams);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around GK110_SUBDEVICE_FB class
//!
class GK110SubdeviceFb : public SubdeviceFb
{
public:
    GK110SubdeviceFb(GpuSubdevice *pGpuSubdevice);
    virtual RC GetDramDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetLtcDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetEdcCounts(GetEdcCountsParams *pParams);
    virtual RC GetDramDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetLtcDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetEdcCrcData(GetEdcCrcDataParams *pParams);
    virtual RC GetLatestEccAddresses(GetLatestEccAddressesParams *pParams);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around GP100_SUBDEVICE_FB class
//!
class GP100SubdeviceFb : public SubdeviceFb
{
public:
    GP100SubdeviceFb(GpuSubdevice *pGpuSubdevice);
    virtual RC GetDramDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetLtcDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetEdcCounts(GetEdcCountsParams *pParams);
    virtual RC GetDramDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetLtcDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetEdcCrcData(GetEdcCrcDataParams *pParams);
    virtual RC GetLatestEccAddresses(GetLatestEccAddressesParams *pParams);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around GP100_SUBDEVICE_FB class
//!
class GV100SubdeviceFb : public SubdeviceFb
{
public:
    GV100SubdeviceFb(GpuSubdevice *pGpuSubdevice);
    virtual RC GetDramDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetLtcDetailedEccCounts(Ecc::DetailedErrCounts *pParams);
    virtual RC GetEdcCounts(GetEdcCountsParams *pParams);
    virtual RC GetDramDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetLtcDetailedAggregateEccCounts(
        Ecc::DetailedErrCounts *pParams);
    virtual RC GetEdcCrcData(GetEdcCrcDataParams *pParams);
    virtual RC GetLatestEccAddresses(GetLatestEccAddressesParams *pParams);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around GPU driver sysfs nodes
//!
class TegraSubdeviceFb : public SubdeviceFb
{
public:
    TegraSubdeviceFb(GpuSubdevice* pGpuSubdevice);
    virtual RC GetDramDetailedEccCounts(Ecc::DetailedErrCounts* pParams);
    virtual RC GetLtcDetailedEccCounts(Ecc::DetailedErrCounts* pParams);
    virtual RC GetEdcCounts(GetEdcCountsParams* pParams);
    virtual RC GetDramDetailedAggregateEccCounts(Ecc::DetailedErrCounts* pParams);
    virtual RC GetLtcDetailedAggregateEccCounts(Ecc::DetailedErrCounts* pParams);
    virtual RC GetEdcCrcData(GetEdcCrcDataParams* pParams);
    virtual RC GetLatestEccAddresses(GetLatestEccAddressesParams* pParams);
};

//--------------------------------------------------------------------
//! \brief Abstraction layer around 2080 based FB ECC/EDC interfaces
//!
class GenericSubdeviceFb : public SubdeviceFb
{
public:
    GenericSubdeviceFb(GpuSubdevice* pGpuSubdevice);
    virtual RC GetDramDetailedEccCounts(Ecc::DetailedErrCounts* pParams);
    virtual RC GetLtcDetailedEccCounts(Ecc::DetailedErrCounts* pParams);
    virtual RC GetEdcCounts(GetEdcCountsParams* pParams);
    virtual RC GetDramDetailedAggregateEccCounts(Ecc::DetailedErrCounts* pParams);
    virtual RC GetLtcDetailedAggregateEccCounts(Ecc::DetailedErrCounts* pParams);
    virtual RC GetEdcCrcData(GetEdcCrcDataParams* pParams);
    virtual RC GetLatestEccAddresses(GetLatestEccAddressesParams* pParams);
private:
    RC HbmFlipSublocation(UINT32 fbio, UINT32 *pEccSubLocation);
};

//--------------------------------------------------------------------
//! \brief Factory method to alloc the latest supported subclass
//!
/* static */ SubdeviceFb *SubdeviceFb::Alloc(GpuSubdevice *pGpuSubdevice)
{
    if (pGpuSubdevice->IsSOC() && Platform::UsesLwgpuDriver())
    {
        return new TegraSubdeviceFb(pGpuSubdevice);
    }

    if (pGpuSubdevice->HasFeature(Device::GPUSUB_SUPPORTS_RM_GENERIC_ECC))
    {
        return new GenericSubdeviceFb(pGpuSubdevice);
    }

    UINT32 classList[] =
    {
        GV100_SUBDEVICE_FB,
        GP100_SUBDEVICE_FB,
        GK110_SUBDEVICE_FB,
        GF100_SUBDEVICE_FB
    };
    UINT32 myClass;

    if (OK == LwRmPtr()->GetFirstSupportedClass(
            static_cast<UINT32>(NUMELEMS(classList)), classList, &myClass,
            pGpuSubdevice->GetParentDevice()))
    {
        switch (myClass)
        {
            case GV100_SUBDEVICE_FB:
                return new GV100SubdeviceFb(pGpuSubdevice);
            case GP100_SUBDEVICE_FB:
                return new GP100SubdeviceFb(pGpuSubdevice);
            case GK110_SUBDEVICE_FB:
                return new GK110SubdeviceFb(pGpuSubdevice);
            case GF100_SUBDEVICE_FB:
                return new GF100SubdeviceFb(pGpuSubdevice);
        }
    }
    return NULL;
}

//--------------------------------------------------------------------
// Constructors
//
SubdeviceFb::SubdeviceFb
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

GF100SubdeviceFb::GF100SubdeviceFb(GpuSubdevice *pGpuSubdevice) :
    SubdeviceFb(pGpuSubdevice, GF100_SUBDEVICE_FB)
{
#ifdef DEBUG
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    const UINT32 numSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();
    MASSERT(LW90E1_CTRL_FB_ECC_PARTITION_COUNT >= numPartitions);
    MASSERT(LW90E1_CTRL_FB_ECC_SLICE_COUNT >= numSubPartitions);
    MASSERT(LW90E1_CTRL_FB_ECC_SLICE_COUNT >= numSlices);
#endif
}

GK110SubdeviceFb::GK110SubdeviceFb(GpuSubdevice *pGpuSubdevice) :
    SubdeviceFb(pGpuSubdevice, GK110_SUBDEVICE_FB)
{
#ifdef DEBUG
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    MASSERT(LWA0E1_CTRL_FB_DRAM_ECC_MAX_COUNT >=
            numPartitions * numSubPartitions);
#endif
}

GP100SubdeviceFb::GP100SubdeviceFb(GpuSubdevice *pGpuSubdevice) :
    SubdeviceFb(pGpuSubdevice, GP100_SUBDEVICE_FB)
{
#ifdef DEBUG
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    MASSERT(LWC0E1_CTRL_FB_DRAM_ECC_MAX_COUNT >=
            numPartitions * numSubPartitions);
#endif
}

GV100SubdeviceFb::GV100SubdeviceFb(GpuSubdevice *pGpuSubdevice) :
    SubdeviceFb(pGpuSubdevice, GV100_SUBDEVICE_FB)
{
#ifdef DEBUG
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    const UINT32 numLtcs = pGpuSubdevice->GetFB()->GetLtcCount();
    const UINT32 numSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();
    MASSERT(LWC3E1_CTRL_FB_ECC_PARTITION_COUNT >= numPartitions);
    MASSERT(LWC3E1_CTRL_FB_ECC_SUBPARTITION_COUNT >= numSubPartitions);
    MASSERT(LWC3E1_CTRL_FB_ECC_PARTITION_COUNT >= numLtcs);
    MASSERT(LWC3E1_CTRL_FB_ECC_SLICE_COUNT >= numSlices);
#endif
}

GenericSubdeviceFb::GenericSubdeviceFb(GpuSubdevice *pGpuSubdevice) :
    SubdeviceFb(pGpuSubdevice, LW20_SUBDEVICE_0)
{
}

//--------------------------------------------------------------------
// Colwenience method to resize & zero-fill the ECC data vectors.
// Avoid clear(), to save time if the caller allocated the vectors.
//

// 1D data
template<class DATA_STRUCT> static void ResizeAndClearEccData
(
    vector<DATA_STRUCT> *pVector,
    UINT32 size1
)
{
    static const DATA_STRUCT ZERO_DATA = {0};
    pVector->resize(size1);
    fill(pVector->begin(), pVector->end(), ZERO_DATA);
}

// 2D data
template<class DATA_STRUCT> static void ResizeAndClearEccData
(
    vector<vector<DATA_STRUCT> > *pVector,
    UINT32 size1,
    UINT32 size2
)
{
    pVector->resize(size1);
    for (UINT32 ii = 0; ii < size1; ++ii)
        ResizeAndClearEccData(&(*pVector)[ii], size2);
}

//--------------------------------------------------------------------
// Wrappers around LW**E1_CTRL_CMD_FB_GET_DRAM_ECC_COUNTS
//
/* virtual */ RC GF100SubdeviceFb::GetDramDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E1 doesn't implement GET_DRAM_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LW90E1_CTRL_FB_GET_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.sliceCount = LW90E1_CTRL_FB_ECC_SLICE_COUNT;
    rmParams.partitionCount = LW90E1_CTRL_FB_ECC_PARTITION_COUNT;
    rmParams.flags = DRF_DEF(90E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E1_CTRL_CMD_FB_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    const UINT32 subpartitionCount = pGpuSubdevice->GetFB()->GetSubpartitions();
    MASSERT(rmParams.partitionCount == pGpuSubdevice->GetFB()->GetFbioCount());
    MASSERT(rmParams.sliceCount >= subpartitionCount);

    ResizeAndClearEccData(&pParams->eccCounts,
                          rmParams.partitionCount, subpartitionCount);
    for (UINT32 part = 0; part < rmParams.partitionCount; ++part)
    {
        for (UINT32 subPart = 0; subPart < subpartitionCount; ++subPart)
        {
            pParams->eccCounts[part][subPart].corr =
                rmParams.partitionEcc[part][subPart].fbSbe.count;
            pParams->eccCounts[part][subPart].uncorr =
                rmParams.partitionEcc[part][subPart].fbDbe.count;
        }
        for (UINT32 subPart = subpartitionCount;
             subPart < rmParams.sliceCount; ++subPart)
        {
            // GK104 has 4 slices and 2 subpartitions per FBIO, but
            // the slice errors & subpartition errors are stored in
            // the same struct.  The subpartition errors in the last 2
            // structs should be zero.
            //
            MASSERT(rmParams.partitionEcc[part][subPart].fbSbe.count == 0);
            MASSERT(rmParams.partitionEcc[part][subPart].fbDbe.count == 0);
        }
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceFb::GetDramDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    RC rc;

    LWA0E1_CTRL_FB_GET_DRAM_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.flags = DRF_DEF(90E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E1_CTRL_CMD_FB_GET_DRAM_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    ResizeAndClearEccData(&pParams->eccCounts,
                          numPartitions, numSubPartitions);
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E1_CTRL_FB_GET_DRAM_ECC_COUNTS &entry = rmParams.dramEcc[ii];
        MASSERT(entry.partition < numPartitions);
        MASSERT(entry.subPartition < numSubPartitions);
        pParams->eccCounts[entry.partition][entry.subPartition].corr =
            entry.sbe;
        pParams->eccCounts[entry.partition][entry.subPartition].uncorr =
            entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceFb::GetDramDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    RC rc;

    LWC0E1_CTRL_FB_GET_DRAM_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.flags = DRF_DEF(90E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E1_CTRL_CMD_FB_GET_DRAM_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(C0E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    ResizeAndClearEccData(&pParams->eccCounts,
                          numPartitions, numSubPartitions);
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWC0E1_CTRL_FB_GET_DRAM_ECC_COUNTS &entry = rmParams.dramEcc[ii];
        MASSERT(entry.partition < numPartitions);
        MASSERT(entry.subPartition < numSubPartitions);
        pParams->eccCounts[entry.partition][entry.subPartition].corr =
            entry.sbe;
        pParams->eccCounts[entry.partition][entry.subPartition].uncorr =
            entry.dbe;
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceFb::GetDramDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    RC rc;

    // C3E1 doesn't implement FB_GET_DRAM_ECC_COUNTS; use FB_GET_ECC_COUNTS instead
    //
    LWC3E1_CTRL_FB_GET_ECC_COUNTS_PARAMS rmParams = { };
    rmParams.flags = DRF_DEF(C3E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC3E1_CTRL_CMD_FB_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts,
                          numPartitions, numSubPartitions);
    MASSERT(rmParams.partitionCount >= numPartitions);
    MASSERT(rmParams.subPartitionCount >= numSubPartitions);
    for (UINT32 ii = 0; ii < rmParams.partitionCount; ++ii)
    {
        const LWC3E1_CTRL_FB_GET_ECC_PARTITION_COUNTS &partEntry = rmParams.partitionEcc[ii];
        for (UINT32 jj = 0; jj < rmParams.subPartitionCount; ++jj)
        {
            const LWC3E1_CTRL_FB_GET_ECC_SUBPARTITION_COUNTS &subPartEntry = partEntry.fb[jj];
            if ((ii < numPartitions) && (jj < numSubPartitions))
            {
                pParams->eccCounts[ii][jj].corr = subPartEntry.sbe.count;
                pParams->eccCounts[ii][jj].uncorr = subPartEntry.dbe.count;
            }
            else
            {
                MASSERT(subPartEntry.sbe.count == 0);
                MASSERT(subPartEntry.dbe.count == 0);
            }
        }
    }
    return rc;
}

/* virtual */ RC GenericSubdeviceFb::GetDramDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numChannelsPerFbio = pGpuSubdevice->GetFB()->GetChannelsPerFbio();

    ResizeAndClearEccData(&pParams->eccCounts, numPartitions, numChannelsPerFbio);

    LW2080_CTRL_ECC_GET_DETAILED_ERROR_COUNTS_PARAMS rmParams;

    for (UINT32 fbio = 0; fbio < pParams->eccCounts.size(); fbio++)
    {
        // Pre-GH100, sublocation is subpartition
        // For GH100, sublocation is local channel index within fbio
        for (UINT32 sublocation = 0;
             sublocation < pParams->eccCounts[fbio].size();
             sublocation++)
        {
            memset(&rmParams, 0, sizeof(rmParams));
            rmParams.unit = ECC_UNIT_DRAM;
            rmParams.location.dram.partition = fbio;
            rmParams.location.dram.sublocation = sublocation;

            // TODO: Have RM take flip strap registers into account when
            // getting ECC errors
            CHECK_RC(HbmFlipSublocation(fbio, &rmParams.location.dram.sublocation));

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_ECC_GET_DETAILED_ERROR_COUNTS,
                     &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[fbio][sublocation].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[fbio][sublocation].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
// Wrappers around LW**E1_CTRL_CMD_FB_GET_LTC_ECC_COUNTS
//
/* virtual */ RC GF100SubdeviceFb::GetLtcDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E1 doesn't implement GET_LTC_ECC_COUNTS; use GET_ECC_COUNTS instead
    //
    LW90E1_CTRL_FB_GET_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.sliceCount = LW90E1_CTRL_FB_ECC_SLICE_COUNT;
    rmParams.partitionCount = LW90E1_CTRL_FB_ECC_PARTITION_COUNT;
    rmParams.flags = DRF_DEF(90E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E1_CTRL_CMD_FB_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(rmParams.partitionCount == pGpuSubdevice->GetFB()->GetLtcCount());
    MASSERT(rmParams.sliceCount == pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc());
    ResizeAndClearEccData(&pParams->eccCounts,
                          rmParams.partitionCount, rmParams.sliceCount);
    for (UINT32 part = 0; part < rmParams.partitionCount; ++part)
    {
        for (UINT32 slice = 0; slice < rmParams.sliceCount; ++slice)
        {
            pParams->eccCounts[part][slice].corr =
                rmParams.partitionEcc[part][slice].ltcSbe.count;
            pParams->eccCounts[part][slice].uncorr =
                rmParams.partitionEcc[part][slice].ltcDbe.count;
        }
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceFb::GetLtcDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numLtcs   = pGpuSubdevice->GetFB()->GetLtcCount();
    const UINT32 numSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();
    RC rc;

    LWA0E1_CTRL_FB_GET_LTC_ECC_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E1_CTRL_CMD_FB_GET_LTC_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    ResizeAndClearEccData(&pParams->eccCounts, numLtcs, numSlices);
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E1_CTRL_FB_GET_LTC_ECC_COUNTS &entry = rmParams.ltcEcc[ii];
        MASSERT(entry.partition < numLtcs);
        MASSERT(entry.slice < numSlices);
        pParams->eccCounts[entry.partition][entry.slice].corr = entry.sbe;
        pParams->eccCounts[entry.partition][entry.slice].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceFb::GetLtcDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numLtcs   = pGpuSubdevice->GetFB()->GetLtcCount();
    const UINT32 numSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();
    RC rc;

    LWC0E1_CTRL_FB_GET_LTC_ECC_COUNTS_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E1_CTRL_CMD_FB_GET_LTC_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(C0E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    ResizeAndClearEccData(&pParams->eccCounts, numLtcs, numSlices);
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWC0E1_CTRL_FB_GET_LTC_ECC_COUNTS &entry = rmParams.ltcEcc[ii];
        MASSERT(entry.partition < numLtcs);
        MASSERT(entry.slice < numSlices);
        pParams->eccCounts[entry.partition][entry.slice].corr = entry.sbe;
        pParams->eccCounts[entry.partition][entry.slice].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceFb::GetLtcDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numLtcs = pGpuSubdevice->GetFB()->GetLtcCount();
    const UINT32 numSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();
    RC rc;

    // C3E1 doesn't implement FB_GET_LTC_ECC_COUNTS; use FB_GET_ECC_COUNTS instead
    //
    LWC3E1_CTRL_FB_GET_ECC_COUNTS_PARAMS rmParams = { };
    rmParams.flags = DRF_DEF(C3E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC3E1_CTRL_CMD_FB_GET_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts,
                          numLtcs, numSlices);
    MASSERT(rmParams.partitionCount >= numLtcs);
    MASSERT(rmParams.sliceCount >= numSlices);
    for (UINT32 ii = 0; ii < rmParams.partitionCount; ++ii)
    {
        const LWC3E1_CTRL_FB_GET_ECC_PARTITION_COUNTS &partEntry = rmParams.partitionEcc[ii];
        for (UINT32 jj = 0; jj < rmParams.sliceCount; ++jj)
        {
            const LWC3E1_CTRL_FB_GET_ECC_SUBPARTITION_COUNTS &sliceEntry = partEntry.ltc[jj];
            if ((ii < numLtcs) && (jj < numSlices))
            {
                pParams->eccCounts[ii][jj].corr = sliceEntry.sbe.count;
                pParams->eccCounts[ii][jj].uncorr = sliceEntry.dbe.count;
            }
            else
            {
                MASSERT(sliceEntry.sbe.count == 0);
                MASSERT(sliceEntry.dbe.count == 0);
            }
        }
    }
    return rc;
}

/* virtual */ RC GenericSubdeviceFb::GetLtcDetailedEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numLtcs = pGpuSubdevice->GetFB()->GetLtcCount();
    const UINT32 maxSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();

    ResizeAndClearEccData(&pParams->eccCounts, numLtcs, maxSlices);
    for (UINT32 ltc = 0; ltc < pParams->eccCounts.size(); ltc++)
    {
        // Consider floorsweeping
        const UINT32 numSlices = pGpuSubdevice->GetFB()->GetSlicesPerLtc(ltc);
        for (UINT32 slice = 0; slice < numSlices; slice++)
        {
            LW2080_CTRL_ECC_GET_DETAILED_ERROR_COUNTS_PARAMS rmParams = {};
            rmParams.unit = ECC_UNIT_LTC;
            rmParams.location.ltc.partition = ltc;
            rmParams.location.ltc.slice = slice;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_ECC_GET_DETAILED_ERROR_COUNTS,
                     &rmParams, sizeof(rmParams)));
            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags));

            pParams->eccCounts[ltc][slice].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[ltc][slice].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
// Wrappers around LW**E1_CTRL_CMD_FB_GET_EDC_COUNTS
//
/* virtual */ RC GF100SubdeviceFb::GetEdcCounts
(
    SubdeviceFb::GetEdcCountsParams *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    RC rc;

    LW90E1_CTRL_FB_GET_EDC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E1_CTRL_CMD_FB_GET_EDC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(LW90E1_CTRL_FB_ECC_PARTITION_COUNT >= numPartitions);
    pParams->edcCounts.resize(numPartitions);
    for (size_t part = 0; part < pParams->edcCounts.size(); ++part)
    {
        pParams->edcCounts[part] = rmParams.edcCounts[part].count;
    }
    for (size_t part = pParams->edcCounts.size();
         part < LW90E1_CTRL_FB_ECC_PARTITION_COUNT; ++part)
    {
        MASSERT(rmParams.edcCounts[part].count == 0);
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceFb::GetEdcCounts
(
    SubdeviceFb::GetEdcCountsParams *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    RC rc;

    LWA0E1_CTRL_FB_GET_EDC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E1_CTRL_CMD_FB_GET_EDC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(LWA0E1_CTRL_FB_ECC_PARTITION_COUNT >= numPartitions);
    pParams->edcCounts.resize(numPartitions);
    for (size_t part = 0; part < pParams->edcCounts.size(); ++part)
    {
        pParams->edcCounts[part] = rmParams.edcCounts[part].count;
    }
    for (size_t part = pParams->edcCounts.size();
         part < LWA0E1_CTRL_FB_ECC_PARTITION_COUNT; ++part)
    {
        MASSERT(rmParams.edcCounts[part].count == 0);
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceFb::GetEdcCounts
(
    SubdeviceFb::GetEdcCountsParams *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    RC rc;

    LWC0E1_CTRL_FB_GET_EDC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E1_CTRL_CMD_FB_GET_EDC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    MASSERT(LWC0E1_CTRL_FB_ECC_PARTITION_COUNT >= numPartitions);
    pParams->edcCounts.resize(numPartitions);
    for (size_t part = 0; part < pParams->edcCounts.size(); ++part)
    {
        pParams->edcCounts[part] = rmParams.edcCounts[part].count;
    }
    for (size_t part = pParams->edcCounts.size();
         part < LWC0E1_CTRL_FB_ECC_PARTITION_COUNT; ++part)
    {
        MASSERT(rmParams.edcCounts[part].count == 0);
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceFb::GetEdcCounts
(
    SubdeviceFb::GetEdcCountsParams *pParams
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GenericSubdeviceFb::GetEdcCounts
(
    SubdeviceFb::GetEdcCountsParams *pParams
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
// Wrappers around LW**E1_CTRL_CMD_FB_GET_AGGREGATE_DRAM_ECC_COUNTS
//
/* virtual */ RC GF100SubdeviceFb::GetDramDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E1 doesn't implement GET_AGGREGATE_DRAM_ECC_COUNTS;
    // use GET_AGGREGATE_ECC_COUNTS instead
    //
    LW90E1_CTRL_FB_GET_AGGREGATE_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E1_CTRL_CMD_FB_GET_AGGREGATE_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts,
                          LW90E1_CTRL_FB_ECC_PARTITION_COUNT,
                          LW90E1_CTRL_FB_ECC_SLICE_COUNT);
    for (UINT32 part = 0; part < LW90E1_CTRL_FB_ECC_PARTITION_COUNT; ++part)
    {
        for (UINT32 subPart = 0;
             subPart < LW90E1_CTRL_FB_ECC_SLICE_COUNT; ++subPart)
        {
            pParams->eccCounts[part][subPart].corr =
                rmParams.partitionEcc[part][subPart].fbSbe.count;
            pParams->eccCounts[part][subPart].uncorr =
                rmParams.partitionEcc[part][subPart].fbDbe.count;
        }
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceFb::GetDramDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // Get data from RM
    //
    LWA0E1_CTRL_FB_GET_AGGREGATE_DRAM_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.flags = DRF_DEF(90E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E1_CTRL_CMD_FB_GET_AGGREGATE_DRAM_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    // Resize the output array to the floorswept GPU size, or the
    // data size returned by the RM, whichever is larger.
    //
    UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        if (numPartitions < rmParams.dramEcc[ii].partition)
            numPartitions = rmParams.dramEcc[ii].partition;
        if (numSubPartitions < rmParams.dramEcc[ii].subPartition)
            numSubPartitions = rmParams.dramEcc[ii].subPartition;
    }
    ResizeAndClearEccData(&pParams->eccCounts,
                          numPartitions, numSubPartitions);

    // Fill the output array
    //
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E1_CTRL_FB_GET_DRAM_ECC_COUNTS &entry =
            rmParams.dramEcc[ii];
        pParams->eccCounts[entry.partition][entry.subPartition].corr =
            entry.sbe;
        pParams->eccCounts[entry.partition][entry.subPartition].uncorr =
            entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceFb::GetDramDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // Get data from RM
    //
    LWC0E1_CTRL_FB_GET_AGGREGATE_DRAM_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.flags = DRF_DEF(90E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E1_CTRL_CMD_FB_GET_AGGREGATE_DRAM_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(C0E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    // Resize the output array to the floorswept GPU size, or the
    // data size returned by the RM, whichever is larger.
    //
    UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        if (numPartitions < rmParams.dramEcc[ii].partition)
            numPartitions = rmParams.dramEcc[ii].partition;
        if (numSubPartitions < rmParams.dramEcc[ii].subPartition)
            numSubPartitions = rmParams.dramEcc[ii].subPartition;
    }
    ResizeAndClearEccData(&pParams->eccCounts,
                          numPartitions, numSubPartitions);

    // Fill the output array
    //
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWC0E1_CTRL_FB_GET_DRAM_ECC_COUNTS &entry =
            rmParams.dramEcc[ii];
        pParams->eccCounts[entry.partition][entry.subPartition].corr =
            entry.sbe;
        pParams->eccCounts[entry.partition][entry.subPartition].uncorr =
            entry.dbe;
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceFb::GetDramDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numSubPartitions = pGpuSubdevice->GetFB()->GetSubpartitions();

    ResizeAndClearEccData(&pParams->eccCounts, numPartitions, numSubPartitions);

    LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));

    for (UINT32 fbio = 0; fbio < pParams->eccCounts.size(); fbio++)
    {
        for (UINT32 subp = 0; subp < pParams->eccCounts[fbio].size(); subp++)
        {
            rmParams.unit = ECC_UNIT_DRAM;
            rmParams.location.dram.partition = fbio;
            rmParams.location.dram.sublocation = subp;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
                     &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[fbio][subp].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[fbio][subp].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

/* virtual */ RC GenericSubdeviceFb::GetDramDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    const UINT32 numChannelsPerFbio = pGpuSubdevice->GetFB()->GetChannelsPerFbio();

    ResizeAndClearEccData(&pParams->eccCounts, numPartitions, numChannelsPerFbio);

    LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams;

    for (UINT32 fbio = 0; fbio < pParams->eccCounts.size(); fbio++)
    {
        // Pre-GH100, sublocation is subpartition
        // For GH100, sublocation is local channel index within fbio
        for (UINT32 sublocation = 0;
             sublocation < pParams->eccCounts[fbio].size();
             sublocation++)
        {
            memset(&rmParams, 0, sizeof(rmParams));
            rmParams.unit = ECC_UNIT_DRAM;
            rmParams.location.dram.partition = fbio;
            rmParams.location.dram.sublocation = sublocation;

            // TODO: Have RM take flip strap registers into account when
            // getting ECC errors
            CHECK_RC(HbmFlipSublocation(fbio, &rmParams.location.dram.sublocation));

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
                     &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[fbio][sublocation].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[fbio][sublocation].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
// Wrappers around LW**E1_CTRL_CMD_FB_GET_AGGREGATE_LTC_ECC_COUNTS
//
/* virtual */ RC GF100SubdeviceFb::GetLtcDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // 90E1 doesn't implement GET_AGGREGATE_LTC_ECC_COUNTS;
    // use GET_AGGREGATE_ECC_COUNTS instead
    //
    LW90E1_CTRL_FB_GET_AGGREGATE_ECC_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E1_CTRL_CMD_FB_GET_AGGREGATE_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));

    ResizeAndClearEccData(&pParams->eccCounts,
                          LW90E1_CTRL_FB_ECC_PARTITION_COUNT,
                          LW90E1_CTRL_FB_ECC_SLICE_COUNT);
    for (UINT32 part = 0; part < LW90E1_CTRL_FB_ECC_PARTITION_COUNT; ++part)
    {
        for (UINT32 slice = 0; slice < LW90E1_CTRL_FB_ECC_SLICE_COUNT; ++slice)
        {
            pParams->eccCounts[part][slice].corr =
                rmParams.partitionEcc[part][slice].ltcSbe.count;
            pParams->eccCounts[part][slice].uncorr =
                rmParams.partitionEcc[part][slice].ltcDbe.count;
        }
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceFb::GetLtcDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // Get data from RM
    //
    LWA0E1_CTRL_FB_GET_AGGREGATE_LTC_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.flags = DRF_DEF(90E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E1_CTRL_CMD_FB_GET_AGGREGATE_LTC_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(A0E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    // Resize the output array to the floorswept GPU size, or the
    // data size returned by the RM, whichever is larger.
    //
    UINT32 numLtcs   = pGpuSubdevice->GetFB()->GetLtcCount();
    UINT32 numSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        if (numLtcs < rmParams.ltcEcc[ii].partition)
            numLtcs = rmParams.ltcEcc[ii].partition;
        if (numSlices < rmParams.ltcEcc[ii].slice)
            numSlices = rmParams.ltcEcc[ii].slice;
    }
    ResizeAndClearEccData(&pParams->eccCounts, numLtcs, numSlices);

    // Fill the output array
    //
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWA0E1_CTRL_FB_GET_LTC_ECC_COUNTS &entry = rmParams.ltcEcc[ii];
        pParams->eccCounts[entry.partition][entry.slice].corr = entry.sbe;
        pParams->eccCounts[entry.partition][entry.slice].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceFb::GetLtcDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    RC rc;

    // Get data from RM
    //
    LWC0E1_CTRL_FB_GET_AGGREGATE_LTC_ECC_COUNTS_PARAMS rmParams = {0};
    rmParams.flags = DRF_DEF(90E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS, _TYPE, _RAW);
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E1_CTRL_CMD_FB_GET_AGGREGATE_LTC_ECC_COUNTS,
                 &rmParams, sizeof(rmParams)));
    MASSERT(FLD_TEST_DRF(C0E1_CTRL_FB_GET_ECC_COUNTS, _FLAGS,
                         _OVERFLOW, _FALSE, rmParams.flags));

    // Resize the output array to the floorswept GPU size, or the
    // data size returned by the RM, whichever is larger.
    //
    UINT32 numLtcs   = pGpuSubdevice->GetFB()->GetLtcCount();
    UINT32 numSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        if (numLtcs < rmParams.ltcEcc[ii].partition)
            numLtcs = rmParams.ltcEcc[ii].partition;
        if (numSlices < rmParams.ltcEcc[ii].slice)
            numSlices = rmParams.ltcEcc[ii].slice;
    }
    ResizeAndClearEccData(&pParams->eccCounts, numLtcs, numSlices);

    // Fill the output array
    //
    for (UINT32 ii = 0; ii < rmParams.entryCount; ++ii)
    {
        const LWC0E1_CTRL_FB_GET_LTC_ECC_COUNTS &entry = rmParams.ltcEcc[ii];
        pParams->eccCounts[entry.partition][entry.slice].corr = entry.sbe;
        pParams->eccCounts[entry.partition][entry.slice].uncorr = entry.dbe;
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceFb::GetLtcDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numLtcs = pGpuSubdevice->GetFB()->GetLtcCount();
    const UINT32 numSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();

    ResizeAndClearEccData(&pParams->eccCounts, numLtcs, numSlices);

    LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));

    for (UINT32 ltc = 0; ltc < pParams->eccCounts.size(); ltc++)
    {
        for (UINT32 slice = 0; slice < pParams->eccCounts[ltc].size(); slice++)
        {
            rmParams.unit = ECC_UNIT_LTC;
            rmParams.location.ltc.partition = ltc;
            rmParams.location.ltc.slice = slice;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
                     &rmParams, sizeof(rmParams)));

            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags)); //$

            pParams->eccCounts[ltc][slice].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[ltc][slice].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

/* virtual */ RC GenericSubdeviceFb::GetLtcDetailedAggregateEccCounts
(
    Ecc::DetailedErrCounts *pParams
)
{
    RC rc;
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numLtcs = pGpuSubdevice->GetFB()->GetLtcCount();
    const UINT32 maxSlices = pGpuSubdevice->GetFB()->GetMaxSlicesPerLtc();

    ResizeAndClearEccData(&pParams->eccCounts, numLtcs, maxSlices);
    for (UINT32 ltc = 0; ltc < pParams->eccCounts.size(); ltc++)
    {
        // Consider floorsweeping
        const UINT32 numSlices = pGpuSubdevice->GetFB()->GetSlicesPerLtc(ltc);
        for (UINT32 slice = 0; slice < numSlices; slice++)
        {
            LW2080_CTRL_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS_PARAMS rmParams = {};
            rmParams.unit = ECC_UNIT_LTC;
            rmParams.location.ltc.partition = ltc;
            rmParams.location.ltc.slice = slice;

            CHECK_RC(LwRmPtr()->ControlBySubdevice(
                     pGpuSubdevice,
                     LW2080_CTRL_CMD_ECC_GET_DETAILED_AGGREGATE_ERROR_COUNTS,
                     &rmParams, sizeof(rmParams)));
            MASSERT(FLD_TEST_DRF(2080, _ECC_COUNTS_FLAGS, _VALID, _TRUE, rmParams.eccCounts.flags));

            pParams->eccCounts[ltc][slice].corr = rmParams.eccCounts.correctedTotalCounts;
            pParams->eccCounts[ltc][slice].uncorr = rmParams.eccCounts.uncorrectedTotalCounts;
        }
    }
    return rc;
}

//--------------------------------------------------------------------
// Wrappers around LW**E1_CTRL_CMD_FB_GET_EDC_CRC_DATA
//
/* virtual */ RC GF100SubdeviceFb::GetEdcCrcData
(
    SubdeviceFb::GetEdcCrcDataParams *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    RC rc;

    LW90E1_CTRL_FB_GET_EDC_CRC_DATA_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LW90E1_CTRL_CMD_FB_GET_EDC_CRC_DATA,
                 &rmParams, sizeof(rmParams)));

    MASSERT(LW90E1_CTRL_FB_ECC_PARTITION_COUNT >= numPartitions);
    ResizeAndClearEccData(&pParams->crcData, numPartitions);
    for (size_t part = 0; part < pParams->crcData.size(); ++part)
    {
        pParams->crcData[part].expected = (UINT32)rmParams.expectedCrcs[part];
        pParams->crcData[part].actual = (UINT32)rmParams.actualCrcs[part];
    }
    for (size_t part = pParams->crcData.size();
         part < LW90E1_CTRL_FB_ECC_PARTITION_COUNT; ++part)
    {
        MASSERT(rmParams.expectedCrcs[part] == 0);
        MASSERT(rmParams.actualCrcs[part] == 0);
    }
    return rc;
}

/* virtual */ RC GK110SubdeviceFb::GetEdcCrcData
(
    SubdeviceFb::GetEdcCrcDataParams *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    RC rc;

    LWA0E1_CTRL_FB_GET_EDC_CRC_DATA_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWA0E1_CTRL_CMD_FB_GET_EDC_CRC_DATA,
                 &rmParams, sizeof(rmParams)));

    MASSERT(LWA0E1_CTRL_FB_ECC_PARTITION_COUNT >= numPartitions);
    ResizeAndClearEccData(&pParams->crcData, numPartitions);
    for (size_t part = 0; part < pParams->crcData.size(); ++part)
    {
        pParams->crcData[part].expected = (UINT32)rmParams.expectedCrcs[part];
        pParams->crcData[part].actual = (UINT32)rmParams.actualCrcs[part];
    }
    for (size_t part = pParams->crcData.size();
         part < LWA0E1_CTRL_FB_ECC_PARTITION_COUNT; ++part)
    {
        MASSERT(rmParams.expectedCrcs[part] == 0);
        MASSERT(rmParams.actualCrcs[part] == 0);
    }
    return rc;
}

/* virtual */ RC GP100SubdeviceFb::GetEdcCrcData
(
    SubdeviceFb::GetEdcCrcDataParams *pParams
)
{
    const GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const UINT32 numPartitions = pGpuSubdevice->GetFB()->GetFbioCount();
    RC rc;

    LWA0E1_CTRL_FB_GET_EDC_CRC_DATA_PARAMS rmParams;
    memset(&rmParams, 0, sizeof(rmParams));
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 pGpuSubdevice, GetClass(),
                 LWC0E1_CTRL_CMD_FB_GET_EDC_CRC_DATA,
                 &rmParams, sizeof(rmParams)));

    MASSERT(LWA0E1_CTRL_FB_ECC_PARTITION_COUNT >= numPartitions);
    ResizeAndClearEccData(&pParams->crcData, numPartitions);
    for (size_t part = 0; part < pParams->crcData.size(); ++part)
    {
        pParams->crcData[part].expected = (UINT32)rmParams.expectedCrcs[part];
        pParams->crcData[part].actual = (UINT32)rmParams.actualCrcs[part];
    }
    for (size_t part = pParams->crcData.size();
         part < LWA0E1_CTRL_FB_ECC_PARTITION_COUNT; ++part)
    {
        MASSERT(rmParams.expectedCrcs[part] == 0);
        MASSERT(rmParams.actualCrcs[part] == 0);
    }
    return rc;
}

/* virtual */ RC GV100SubdeviceFb::GetEdcCrcData
(
    SubdeviceFb::GetEdcCrcDataParams *pParams
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GenericSubdeviceFb::GetEdcCrcData
(
    SubdeviceFb::GetEdcCrcDataParams *pParams
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
// Wrappers around LW**E1_CTRL_CMD_FB_GET_LATEST_ECC_ADDRESSES
//
/* virtual */ RC GF100SubdeviceFb::GetLatestEccAddresses
(
    GetLatestEccAddressesParams *pParams
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GK110SubdeviceFb::GetLatestEccAddresses
(
    GetLatestEccAddressesParams *pParams
)
{
    RC rc;

    LWA0E1_CTRL_FB_GET_LATEST_ECC_ADDRESSES_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 GetGpuSubdevice(), GetClass(),
                 LWA0E1_CTRL_CMD_FB_GET_LATEST_ECC_ADDRESSES,
                 &rmParams, sizeof(rmParams)));

    pParams->addresses.resize(rmParams.addressCount);
    for (UINT32 ii = 0; ii < rmParams.addressCount; ++ii)
    {
        pParams->addresses[ii].physAddress = rmParams.addresses[ii].physAddress;
        pParams->addresses[ii].isSbe = FLD_TEST_DRF(
                A0E1, _CTRL_FB_GET_LATEST_ECC_ADDRESSES, _ERROR_TYPE, _SBE,
                rmParams.addresses[ii].errorType);
        pParams->addresses[ii].sbeBitPosition = _UINT32_MAX;
        pParams->addresses[ii].rbcAddress = rmParams.addresses[ii].rbcAddress;
        pParams->addresses[ii].eccSource = Ecc::Source::GPU;
    }
    pParams->totalAddressCount = rmParams.totalAddressCount;
    return rc;
}

/* virtual */ RC GP100SubdeviceFb::GetLatestEccAddresses
(
    GetLatestEccAddressesParams *pParams
)
{
    RC rc;

    LWC0E1_CTRL_FB_GET_LATEST_ECC_ADDRESSES_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 GetGpuSubdevice(), GetClass(),
                 LWC0E1_CTRL_CMD_FB_GET_LATEST_ECC_ADDRESSES,
                 &rmParams, sizeof(rmParams)));

    pParams->addresses.resize(rmParams.addressCount);
    for (UINT32 ii = 0; ii < rmParams.addressCount; ++ii)
    {
        pParams->addresses[ii].physAddress = rmParams.addresses[ii].physAddress;
        pParams->addresses[ii].isSbe = FLD_TEST_DRF(
                C0E1, _CTRL_FB_GET_LATEST_ECC_ADDRESSES, _ERROR_TYPE, _SBE,
                rmParams.addresses[ii].errorType);
        pParams->addresses[ii].sbeBitPosition =
            rmParams.addresses[ii].sbeBitPosition;
        pParams->addresses[ii].rbcAddress = rmParams.addresses[ii].rbcAddress;
        pParams->addresses[ii].eccSource = Ecc::Source::GPU;
    }
    pParams->totalAddressCount = rmParams.totalAddressCount;
    return rc;
}

/* virtual */ RC GV100SubdeviceFb::GetLatestEccAddresses
(
    GetLatestEccAddressesParams *pParams
)
{
    RC rc;

    LWC3E1_CTRL_FB_GET_LATEST_ECC_ADDRESSES_PARAMS rmParams = {0};
    CHECK_RC(LwRmPtr()->ControlSubdeviceChild(
                 GetGpuSubdevice(), GetClass(),
                 LWC3E1_CTRL_CMD_FB_GET_LATEST_ECC_ADDRESSES,
                 &rmParams, sizeof(rmParams)));

    pParams->addresses.resize(rmParams.addressCount);
    for (UINT32 ii = 0; ii < rmParams.addressCount; ++ii)
    {
        pParams->addresses[ii].physAddress = rmParams.addresses[ii].physAddress;
        pParams->addresses[ii].isSbe = FLD_TEST_DRF(
                C3E1, _CTRL_FB_GET_LATEST_ECC_ADDRESSES, _ERROR_TYPE, _SBE,
                rmParams.addresses[ii].errorType);
        pParams->addresses[ii].sbeBitPosition =
            rmParams.addresses[ii].sbeBitPosition;
        pParams->addresses[ii].rbcAddress = rmParams.addresses[ii].rbcAddress;
        pParams->addresses[ii].eccSource = Ecc::Source::GPU;
    }
    pParams->totalAddressCount = rmParams.totalAddressCount;
    return rc;
}

/* virtual */ RC GenericSubdeviceFb::GetLatestEccAddresses
(
    GetLatestEccAddressesParams *pParams
)
{
    RC rc;
    RegHal& regs = GetGpuSubdevice()->Regs();

    LW2080_CTRL_ECC_GET_LATEST_ECC_ADDRESSES_PARAMS rmParams = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
                 GetGpuSubdevice(),
                 LW2080_CTRL_CMD_ECC_GET_LATEST_ECC_ADDRESSES,
                 &rmParams, sizeof(rmParams)));

    pParams->addresses.resize(rmParams.addressCount);
    for (UINT32 i = 0; i < rmParams.addressCount; ++i)
    {
        auto& entry = pParams->addresses[i];
        auto& rmEntry = rmParams.addresses[i];

        entry.physAddress = rmEntry.physAddress;
        entry.isSbe = FLD_TEST_DRF(2080, _CTRL_ECC_GET_LATEST_ECC_ADDRESSES,
                                   _ERROR_TYPE, _SBE, rmEntry.errorType);
        entry.sbeBitPosition = rmEntry.sbeBitPosition;
        entry.rbcAddress = rmEntry.rbcAddress;

        if (regs.IsSupported(MODS_PFB_FBPA_ECC_ADDR_EXT2_LEGACY_SOURCE))
        {
            if (regs.TestField(rmEntry.rbcAddressExt2,
                               MODS_PFB_FBPA_ECC_ADDR_EXT2_LEGACY_SOURCE_DRAM))
            {
                entry.eccSource = Ecc::Source::DRAM;
            }
            else if (regs.TestField(rmEntry.rbcAddressExt2,
                                    MODS_PFB_FBPA_ECC_ADDR_EXT2_LEGACY_SOURCE_DRAM_AND_GPU))
            {
                entry.eccSource = Ecc::Source::DRAM_AND_GPU;
            }
            else if (regs.TestField(rmEntry.rbcAddressExt2,
                                    MODS_PFB_FBPA_ECC_ADDR_EXT2_LEGACY_SOURCE_GPU))
            {
                entry.eccSource = Ecc::Source::GPU;
            }
            else if (regs.TestField(rmEntry.rbcAddressExt2,
                                    MODS_PFB_FBPA_ECC_ADDR_EXT2_LEGACY_SOURCE_UNDEFINED))
            {
                entry.eccSource = Ecc::Source::UNDEFINED;
            }
            else
            {
                // If MODS_PFB_FBPA_ECC_ADDR_EXT2_LEGACY_SOURCE is supported but
                // Ecc source does not match any of the above, return failure.
                return RC::SOFTWARE_ERROR;
            }
        }
        // If MODS_PFB_FBPA_ECC_ADDR_EXT2_LEGACY_SOURCE isn't supported (pre-GH100),
        // the ecc source is from the GPU.
        else
        {
            entry.eccSource = Ecc::Source::GPU;
        }
    }
    pParams->totalAddressCount = rmParams.totalAddressCount;

    return rc;
}

TegraSubdeviceFb::TegraSubdeviceFb(GpuSubdevice* pGpuSubdevice)
    : SubdeviceFb(pGpuSubdevice, GK110_SUBDEVICE_FB)
{
}

RC TegraSubdeviceFb::GetDramDetailedEccCounts(Ecc::DetailedErrCounts* pParams)
{
    return OK;
}

RC TegraSubdeviceFb::GetLtcDetailedEccCounts(Ecc::DetailedErrCounts* pParams)
{
    const FrameBuffer* pFb       = GetGpuSubdevice()->GetFB();
    const UINT32       numLtcs   = pFb->GetLtcCount();
    const UINT32       numSlices = pFb->GetMaxSlicesPerLtc();
    MASSERT(numLtcs == 2);

    ResizeAndClearEccData(&pParams->eccCounts, numLtcs, numSlices);

    for (UINT32 ltc = 0; ltc < numLtcs; ++ltc)
    {
        for (UINT32 slice = 0; slice < numSlices; ++slice)
        {
            string file;
            RC rc;
            file = Utility::StrPrintf("ltc%u_lts%u_ecc_sec_count", ltc, slice);
            CHECK_RC(CheetAh::GetTegraEccCount(
                            file, &pParams->eccCounts[ltc][slice].corr));
            file = Utility::StrPrintf("ltc%u_lts%u_ecc_ded_count", ltc, slice);
            CHECK_RC(CheetAh::GetTegraEccCount(
                            file, &pParams->eccCounts[ltc][slice].uncorr));
        }
    }

    return RC::OK;
}

RC TegraSubdeviceFb::GetEdcCounts(GetEdcCountsParams* pParams)
{
    return OK;
}

RC TegraSubdeviceFb::GetDramDetailedAggregateEccCounts(Ecc::DetailedErrCounts* pParams)
{
    return OK;
}

RC TegraSubdeviceFb::GetLtcDetailedAggregateEccCounts(Ecc::DetailedErrCounts* pParams)
{
    return OK;
}

RC TegraSubdeviceFb::GetEdcCrcData(GetEdcCrcDataParams* pParams)
{
    return OK;
}

RC TegraSubdeviceFb::GetLatestEccAddresses(GetLatestEccAddressesParams* pParams)
{
    return OK;
}

// For GH100, flip strap registers must be taken into account
// to get the true ECC location
RC GenericSubdeviceFb::HbmFlipSublocation(UINT32 fbio, UINT32 *pEccSubLocation)
{
    RC rc;

    MASSERT(pEccSubLocation);

#if LWCFG(GLOBAL_ARCH_HOPPER)
    GpuSubdevice *pGpuSubdevice = GetGpuSubdevice();
    const RegHal& regs = pGpuSubdevice->Regs();

    if (pGpuSubdevice->DeviceId() == Gpu::GH100)
    {
        ModsGpuRegField regField = static_cast<ModsGpuRegField>(0);
        UINT32 flipBits = 0;
        switch (pGpuSubdevice->GetFB()->GetRamProtocol())
        {
            case FrameBuffer::RamHBM2:
                regField = MODS_PFB_FBPA_0_DEBUG_2_HBM2_PC_FLIP_STRAP;
                flipBits = 0x1;
                break;
            case FrameBuffer::RamHBM3:
                regField = MODS_PFB_FBPA_0_DEBUG_2_HBM3_PC_FLIP_STRAP;
                flipBits = 0x3;
                break;
            default:
                MASSERT(!"Invalid Ram Protocol!");
                rc = RC::SOFTWARE_ERROR;
                break;
        }

        const bool shouldFlip = regs.IsSupported(regField, fbio)
                              ? static_cast<bool>(regs.Read32(regField, fbio))
                              : false;

        if (shouldFlip)
        {
            // Flip the subpartition and the channel within the subpartition
            // sublocation is the local channel index within the partition (max 4 for HBM3)
            *pEccSubLocation ^= flipBits;
        }
    }
#endif

    return rc;
}
