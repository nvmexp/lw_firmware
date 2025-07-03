/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "c2cdiagdriver.h"
// TODO: include th500 c2c file if manuals are different from GH100
#include "gh100c2cdiagregs.h"

#define C2C_DIAG_DRIVER_TH500_MAX_C2C_PARTITIONS        2
#define C2C_DIAG_DRIVER_TH500_MAX_LINKS_PER_PARTITION   5

//--------------------------------------------------------------------
//! \brief C2cDiagDriver implementation used for TH500
//!
class Th500C2cDiagDriver: public C2cDiagDriver
{
public:
    Th500C2cDiagDriver() : C2cDiagDriver(
                                C2C_DIAG_DRIVER_TH500_MAX_C2C_PARTITIONS,
                                C2C_DIAG_DRIVER_TH500_MAX_LINKS_PER_PARTITION) {}

    ~Th500C2cDiagDriver() {}

    //!
    //! \brief Initialize C2C diag driver using cpu pointer for accesing C2C register access.
    //!
    //! This function should be used for initializing Th500. The C2C registers should be
    //! readable and writable using cpu pointer.Also, This function should be called
    //! before ilwoking any of the other driver functions from this library.
    //!
    //! \param[in] c2cRegBase          Virtual address at where the C2C registers are
    //!                                accessible. All the C2C partition's registers
    //!                                should be accessible contiguously at this address.
    //! \param[in] nrPartitions        Total number of C2C partitions present.
    //! \param[in] nrLinksPerPartition Number of C2C links per C2C partition.
    //!
    //! \return RC::OK, if all okay, else error.
    //!
    //! TODO: Remove cpu pointer initialization once BAR0 decoupler is enabled for GH100
    //!
    RC Init(void *c2cRegBase, LwU32 nrPartitions, LwU32 nrLinksPerPartition);

private:
    RC ReadC2CRegister(LwU32 regIdx, LwU32 *regValue);
    RC WriteC2CRegister(LwU32 regIdx, LwU32 regValue);
    RC InitDevicePointer(void *c2cRegBase);

    // CPU C2C base pointer.
    LwU64 m_c2cRegBase = 0;
};

