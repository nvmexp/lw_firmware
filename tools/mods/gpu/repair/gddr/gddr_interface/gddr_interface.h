/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "gpu/repair/gddr/gddr_model.h"
#include "gpu/repair/mem_repair_config.h"
#include "gpu/repair/repair_types.h"
#include "gpu/repair/repair_util.h"

#include <memory>

//!
//! \file gddr_interface.h
//!
//! \brief GDDR interfaces.
//!

//!
//! \def GDDR_ROW_REPAIR_FIRST_RC_SKIP(f)
//! \brief Updates \a firstRepairRc, skips repair if the RC of 'f' is not OK.
//! Requires 'StickyRC firstRepairRc' to be defined.
//!
//! If 'f's RC is not OK, the row repair post-amble is called.
//!
//! NOTE: Can't wrap in a do-while(0) due to 'continue'.
//!
#define GDDR_ROW_REPAIR_FIRST_RC_SKIP(f)                            \
{                                                                   \
    RC rc_ = (f);        /* Save RC for cmd in row repair */        \
    firstRepairRc = rc_; /* Record RC for this row repair */        \
    if (rc_ != RC::OK)                                              \
    {                                                               \
        PostSingleRowRepair(settings, repairType, rowErr, rc_);     \
        continue;                                                   \
    }                                                               \
    rc_.Clear();                                                    \
}

namespace Memory
{
namespace Gddr
{
    //!
    //! \brief Represents the interface abstraction of a particular model of
    //! GDDR memory.
    //!
    class GddrInterface
    {
        using RowError        = Repair::RowError;
        using Settings        = Repair::Settings;
        using SystemState     = Repair::SystemState;

    public:
        //!
        //! \brief Constructor.
        //!
        GddrInterface(const Model& gddrModel, GpuSubdevice *pSubdev);

        virtual ~GddrInterface() {}

        GddrInterface(GddrInterface&& o);
        GddrInterface& operator=(GddrInterface&& o);

        //!
        //! \brief Setup.
        //!
        //! NOTE: Must be called by all subclasses.
        //!
        virtual RC Setup();

        //!
        //! \brief Return true if the GDDR interface is setup.
        //!
        bool IsSetup() const { return m_IsSetup; }

        //!
        //! \brief Perform row repair on the given row.
        //!
        //! \param settings Repair settings.
        //! \param rowErrors Rows to repair.
        //! \param repairType Type of repair to perform.
        //!
        virtual RC RowRepair
        (
            const Settings& settings,
            const vector<RowError>& rowErrors,
            RepairType repairType
        );

        void SleepMs(FLOAT64 ms) const;
        void SleepUs(UINT32 us) const;

        void SetSettings(const Settings &settings);
        Settings* GetSettings() const;

    protected:
        //!
        //! \brief Performs actions required before the repair of a single row.
        //! Call before each individual row repair.
        //!
        //! \see PostSingleRowRepair
        //!
        RC PreSingleRowRepair
        (
            const RowError& rowError
        ) const;

        //!
        //! \brief Performs actions required after the repair of a single row.
        //! Call before each individual row repair, regardless of if the repair
        //! succeeds.
        //!
        //! \param repairType Type of repair to perform.
        //! \param rowError Row that was repaired.
        //! \param repairStatus Status of the repair.
        //!
        //! \see PreSingleRowRepair
        //!
        RC PostSingleRowRepair
        (
            const Settings& settings,
            RepairType repairType,
            const RowError& rowError,
            RC repairStatus
        ) const;

    private:
        //! Model of GDDR represented by this interface.
        Model m_GddrModel;

        //! True if Setup has been called, false otherwise.
        bool m_IsSetup = false;

        //! Pointer to the GPU subdevice object
        GpuSubdevice *m_pSubdev;

        //! Repair settings
        Settings m_Settings;
    };
} // namespace Gddr
} // namespace Memory
