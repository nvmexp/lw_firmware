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

#include "core/include/utility.h"

#include "gddr_interface.h"

using namespace Memory;
using namespace Repair;

/******************************************************************************/
// GddrInterface

Gddr::GddrInterface::GddrInterface
(
    const Model& gddrModel,
    GpuSubdevice *pSubdev
)
    : m_GddrModel(gddrModel),
      m_pSubdev(pSubdev)
{
    MASSERT(pSubdev);
}

Gddr::GddrInterface::GddrInterface(GddrInterface&& o)
    : m_GddrModel(std::move(o.m_GddrModel)),
      m_pSubdev(std::move(o.m_pSubdev))
{}

Gddr::GddrInterface& Gddr::GddrInterface::operator=(GddrInterface&& o)
{
    if (this != &o)
    {
        m_GddrModel = std::move(o.m_GddrModel);
        m_IsSetup   = std::move(o.m_IsSetup);
        m_pSubdev   = std::move(o.m_pSubdev);
    }

    return *this;
}

RC Gddr::GddrInterface::Setup()
{
    m_IsSetup = true;
    return RC::OK;
}

//!
//! \brief Sleep the CPU for the given number of milliseconds.
//!
//! If no work is being done on the GPU, it implies sleeping the GPU as well.
//!
//! \param ms Time to sleep in milliseconds.
//!
void Gddr::GddrInterface::SleepMs(FLOAT64 ms) const
{
    static constexpr FLOAT64 usPerMs = 1000;
    SleepUs(static_cast<UINT32>(ms * usPerMs));
}

//!
//! \brief Sleep the CPU for the given number of microseconds.
//!
//! If no work is being done on the GPU, it implies sleeping the GPU as well.
//!
//! \param us Time to sleep in microseconds.
//!
void Gddr::GddrInterface::SleepUs(UINT32 us) const
{
    Utility::SleepUS(us);
}

RC Gddr::GddrInterface::PreSingleRowRepair
(
    const RowError& rowError
) const
{
    const Row& row = rowError.row;

    // Set the origin test for recording purposes
    //
    // NOTE: If there was to be a failure, the test number that would be
    // associated with the RC would be the test ID set here.
    ErrorMap::SetTest(rowError.originTestId);

    // Perform some basic error checks
    //
    const UINT32 hwFbpa   = row.hwFbio.Number(); // Not a typo
    const UINT32 maxFbpas = m_pSubdev->Regs().Read32(MODS_PTOP_SCAL_NUM_FBPAS);
    if (hwFbpa >= maxFbpas)
    {
        Printf(Tee::PriError, "Invalid FBPA number: num(%u), total_fbpas(%u)\n",
                               hwFbpa, maxFbpas);
        MASSERT(!"Invalid FBPA number");
        return RC::SOFTWARE_ERROR;
    }

    // Make sure it isn't floorswept
    const UINT32 fbpaMask = m_pSubdev->GetFsImpl()->FbpaMask();
    if (((fbpaMask >> hwFbpa) & 1) == 0)
    {
        Printf(Tee::PriError, "Cannot repair row %s because FBPA %u is floorswept\n",
                               row.rowName.c_str(), hwFbpa);
        MASSERT(!"FBPA floorswept");
        return RC::SOFTWARE_ERROR;
    }

    // Print repair header
    //
    Printf(Tee::PriNormal, "\n[repair] Repairing row %s\n", row.rowName.c_str());

    return RC::OK;
}

RC Gddr::GddrInterface::PostSingleRowRepair
(
    const Settings& settings,
    RepairType repairType,
    const RowError& rowError,
    RC repairStatus
) const
{
    RC rc;

    const Row& row = rowError.row;

    if (repairStatus != RC::OK)
    {
        Printf(Tee::PriError, "[repair] FAILED with RC %d: %s\n",
               repairStatus.Get(), repairStatus.Message());
    }
    else
    {
        Printf(Tee::PriNormal, "[repair] SUCCESS: row %s\n",
               row.rowName.c_str());

        // Report successfully repaired rows to JS layer
        JavaScriptPtr pJs;
        JsArray Args(1);
        CHECK_RC(pJs->ToJsval(row.rowName, &Args[0]));
        CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "AddRepairedRow", Args, 0));
    }

    // Unset the row's origin test ID
    ErrorMap::SetTest(s_UnknownTestId);

    return rc;
}

RC Gddr::GddrInterface::RowRepair
(
    const Settings& settings,
    const vector<RowError>& rowErrors,
    RepairType repairType
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//!
//! \brief Sets Settings for repair configurations.
//!
void Gddr::GddrInterface::SetSettings(const Settings &settings)
{
    m_Settings = settings;
}

//!
//! \brief Sets Settings for repair configurations.
//!
Repair::Settings* Gddr::GddrInterface::GetSettings() const
{
    return const_cast<Settings*>(&m_Settings);
}
