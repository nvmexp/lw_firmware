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

#include "gpu/repair/gddr/gddr_interface/gddr_interface.h"

#include <memory>

//!
//! \file gddr_interface.h
//!
//! \brief GDDR interfaces.
//!

namespace Memory
{
namespace Gddr
{
    //!
    //! \brief Represents the interface abstraction of a particular model of
    //! GDDR memory.
    //!
    class SamsungGddr6 : public GddrInterface
    {
        using RowError        = Repair::RowError;
        using Settings        = Repair::Settings;
        using SystemState     = Repair::SystemState;

    public:
        //!
        //! \brief Constructor.
        //!
        SamsungGddr6
        (
            const Model& gddrModel,
            GpuSubdevice *pSubdev
        ) : GddrInterface(gddrModel, pSubdev),
            m_pSubdev(pSubdev),
            m_GddrModel(gddrModel)
        {
            MASSERT(pSubdev);
        }

        RC Setup() override;

        RC RowRepair
        (
            const Settings& settings,
            const vector<RowError>& rowErrors,
            RepairType repairType
        ) override;

    protected:
        static constexpr UINT32 s_NumPseudoChannels   = 2;

        //!
        //! \brief Perform a hard row repair.
        //!
        //! \param setting   Repair settings.
        //! \param rowErrors Rows to repair.
        //!
        virtual RC HardRowRepair
        (
            const Settings& settings,
            const vector<RowError>& rowErrors
        );

        //!
        //! \brief Perform a soft row repair.
        //!
        //! \param setting Repair settings.
        //! \param rowErrors Rows to repair.
        //!
        virtual RC SoftRowRepair
        (
            const Settings& settings,
            const vector<RowError>& rowErrors
        );

        virtual RC DoHardRowRepair
        (
            Row row
        ) const;


        //!
        //! \brief Checks the already repaired rows and determines if a row can be repaired or not
        //!
        //! \param row Row to be checked for
        //! \return True if row can be repaired
        //!
        virtual bool IsRowRepairable
        (
            Row row
        ) const;

        //!
        //! \brief Adds given row to list of repaired rows
        //!
        //! \param row Repaired row
        //!
        void AddRowToRepairList
        (
            Row row
        );

    private:
        //! Pointer to the GPU subdevice object
        GpuSubdevice *m_pSubdev;

        Model m_GddrModel;

        vector<Row> m_RepairedRows;

        static constexpr UINT32 s_NumRepairsPerBank = 1;
    };
} // namespace Gddr
} // namespace Memory
