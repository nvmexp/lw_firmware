/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/types.h"
#include "core/include/rc.h"
#include "lwstatus.h"

//--------------------------------------------------------------------
//! \brief Common C2cDiagDriver implementation used for C2C devices
//!
class C2cDiagDriver
{
public:
    enum LinkStatus
    {
        LINK_NOT_TRAINED,           // Link not trained.
        LINK_TRAINED,               // Link trained successfully.
        LINK_TRAINING_FAILED,       // Link training failed.
    } ;

    C2cDiagDriver() = delete;
    virtual ~C2cDiagDriver() {}

    //!
    //! \brief Get C2C link status.
    //!
    //! This function provides the link status of the C2C link index passed as
    //! input.
    //!
    //! \param[in]  linkIdx     Global C2C link index e.g. For a system with 2 C2C
    //!                         partitions and each C2C partition with 5 C2C links,
    //!                         6 is the link index for addressing the second link
    //!                         in second C2C partition.
    //! \param[out] linkStatus  Link status of the C2C link corresponding to the
    //!                         input link index, one of LinkStatus.
    //!
    //! \return OK if all is okay.  Otherwise an error-specific value.
    //!
    RC GetLinkStatus(LwU32 linkIdx, LinkStatus *linkStatus);

    //!
    //! \brief Get C2C line rate in Gbps.
    //!
    //! This function provides the line rate for desired C2C partition.
    //!
    //! \param[in]  partitionIdx  C2C partition index
    //! \param[out] lineRate      Line rate of the C2C partition corresponding to
    //!                           the input partition index in Gbps.
    //!
    //! \return OK if all is okay.  Otherwise an error-specific value.
    //!
    RC GetLineRate(LwU32 partitionIdx, LwF32 *lineRate);

    //!
    //! \brief Get C2C register value
    //!
    //! This function provides direct register read capacbility.
    //!
    //! \param[in]  regIdx       Register offset.
    //! \param[out] regValue     Register value of the input register offset.
    //!
    //! \return OK if all is okay.  Otherwise an error-specific value.
    //!
    RC ReadReg(LwU32 regIdx, LwU32 *regValue);

    //!
    //! \brief Enable link 0 only mode
    //!
    //! This forces all reads/writes to use the link 0 unicast register (necessary for
    //! emulation trim netlists)
    void SetLink0Only(bool bLink0Only) { m_bLink0Only = bLink0Only; }
protected:
    //!
    //! \brief Constructor for C2cDiagDriver
    //!
    //! Initializes m_maxC2CPartitions and m_maxLinksPerPartition. These will
    //! be used to validate user given link index and partition index in API calls.
    //!
    //! \param[in]  nrC2cPartitions         Maximum number of C2C partitions
    //! \param[in]  nrLinksPerPartition     Maximum number of C2C links per partition
    //!
    C2cDiagDriver(LwU32 nrC2cPartitions, LwU32 nrLinksPerPartition) :
                    m_maxC2CPartitions(nrC2cPartitions),
                    m_maxLinksPerPartition(nrLinksPerPartition) {}

    //!
    //! \brief Utility function to initialize number of partitions and links.
    //!
    //! \param[in] nrPartitions        Total number of C2C partitions present.
    //! \param[in] nrLinksPerPartition Number of C2C links per C2C partition.
    //!
    //! \return OK if initialized, otherwise error.
    //!
    RC InitInternal(LwU32 nrPartitions, LwU32 nrLinksPerPartition);

    //!
    //! \brief This function reads a C2C register.
    //!
    //! The register read will differ for GH100 & TH500. For TH500, read can be
    //! using cpu pointer assuming it is permitted to read C2C registers in TH500.
    //! For GH100, the read cannot happen using cpu pointer due to BAR0 de-coupler.
    //! Once BAR0 de-coupler is enabled, read would happen using GpuSubDevice API.
    //!
    //! \param[in]  regIdx       Register offset.
    //! \param[out] regValue     Register value of the input register offset.
    //!
    //! \return OK if all is okay.  Otherwise an error-specific value.
    //!
    //! TODO: Implement GpuSubDevice based API for GH100 once BAR0 decoupler is enabled.
    //!
    virtual RC ReadC2CRegister(LwU32 regIdx, LwU32 *regValue) = 0;

    //!
    //! \brief This function writes a C2C register.
    //!
    //! The register write will differ for GH100 & TH500. For TH500, write can be
    //! using cpu pointer assuming it is permitted to write C2C registers in TH500.
    //! For GH100, the write cannot happen using cpu pointer due to BAR0 de-coupler.
    //! Once BAR0 de-coupler is enabled, write would happen using GpuSubDevice API.
    //!
    //! \param[in]  regIdx       Register offset.
    //! \param[in]  regValue     Value to write.
    //!
    //! \return OK if all is okay.  Otherwise an error-specific value.
    //!
    //! TODO: Implement GpuSubDevice based API for GH100 once BAR0 decoupler is enabled.
    //!
    virtual RC WriteC2CRegister(LwU32 regIdx, LwU32 regValue) = 0;

    //!
    //! \brief Utility function to check if link index is valid.
    //!
    //! \param[in]  linkIdx     Link index of C2C link.
    //!
    //! \return true if link Index is valid otherwise false.
    //!
    bool IsLinkIdxValid(LwU32 linkIdx);

    //!
    //! \brief Utility function to check if partition index is valid.
    //!
    //! \param[in]  partitionIdx     C2C partition index.
    //!
    //! \return true if partition is valid otherwise false.
    //!
    bool IsPartitionIdxValid(LwU32 partitionIdx);

    // Number of C2C partitions
    LwU32   m_nrPartitions = 0;
    // Number of links per partition
    LwU32   m_nrLinksPerPartition = 0;

    bool    m_bLink0Only = false;

    // Maximum number of C2C partitions allowed
    // Initialized from Child class constructor
    const LwU32 m_maxC2CPartitions;
    // Maximum number of links allowed per partition
    // Initialized from Child Class constructor
    const LwU32 m_maxLinksPerPartition;
};
