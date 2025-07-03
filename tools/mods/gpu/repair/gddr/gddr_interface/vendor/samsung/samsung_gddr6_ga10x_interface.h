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

#include "gpu/repair/gddr/gddr_interface/vendor/samsung/samsung_gddr6_interface.h"

#include <memory>

//!
//! \file samsung_gddr_ga10x_interface.h
//!
//! \brief Samsung GA10x GDDR6 interfaces.
//!

namespace Memory
{
namespace Gddr
{
    //!
    //! \brief Class for GA10x GDDR6 Samsung memory.
    //!
    class SamsungGddr6GA10x : public SamsungGddr6
    {
        using RowError        = Repair::RowError;
        using Settings        = Repair::Settings;
        using SystemState     = Repair::SystemState;

    public:
        //!
        //! \brief Constructor.
        //!
        SamsungGddr6GA10x
        (
            const Model& gddrModel,
            GpuSubdevice *pSubdev
        ) : SamsungGddr6(gddrModel, pSubdev),
            m_pSubdev(pSubdev),
            m_GddrModel(gddrModel)
        {
            MASSERT(pSubdev);
        }

    protected:
        RC DoHardRowRepair
        (
            Row row
        ) const override;

    private:
        //! Pointer to the GPU subdevice object
        GpuSubdevice *m_pSubdev;

        Model m_GddrModel;

        vector<Row> m_RepairedRows;

        static constexpr UINT32 s_NumRepairsPerBank = 1;
    };
} // namespace Gddr
} // namespace Memory
