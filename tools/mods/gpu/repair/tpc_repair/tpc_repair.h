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

#include "ctrl/ctrl90e7.h"
#include "gpu/include/gpusbdev.h"

class TpcRepair
{
    using TpcRepairData = std::pair<UINT32,UINT32>;

public:
    TpcRepair(GpuSubdevice *pSubdev);
    virtual ~TpcRepair() {}

    //!
    //! \brief Populate the infoROM RPR object with given TPC repair information.
    //!
    //! \param repairInput Command line input representing repair data to be written.
    //! \param bAppend     Whether the entries must overwrite the existing data
    //!                    or just be appended to it.
    //!
    RC PopulateInforomEntries(string repairInput, bool bAppend) const;

    //!
    //! \brief Clear TPC repair infomation from the RPR object in the infoROM.
    //!
    RC ClearInforomEntries() const;

    //!
    //! \brief Print the TPC repair infomation from the RPR object in the infoROM.
    //!
    RC PrintInforomEntries() const;

private:
    GpuSubdevice *m_pSub;

    //! \brief  Check if TPC repair is supported on the subdevice.
    RC IsSupported() const;

    //!
    //! \brief  Check whether the given address belongs to a TPC disable extend register.
    //!
    //! \param  address  Register address to check.
    //!
    bool IsTpcRepairReg(UINT32 address) const;

    //!
    //! \brief  Parse the populate infoROM input.
    //!         Input is in the format "<gpcN1>:<tpc_mask>,<gpcN2>:<tpc_mask>...".
    //!
    //! \param      cmdInput    Command line input.
    //! \param[out] pRepairData Parsed repair information.
    //!
    RC ParsePopulateInforomArgs(string cmdInput, vector<TpcRepairData> *pRepairData) const;

    //!
    //! \brief  Read the current RPR object from the infoROM.
    //!
    //! \param  pInforomParams[out]       Filled with the entries from the current RPR object.
    //!
    RC ReadLwrrInforomParams(LW90E7_CTRL_RPR_GET_INFO_PARAMS *pInforomParams) const;

    //!
    //! \brief  Write given RPR object params to the infoROM.
    //!
    //! \param  pInforomParams            RPR object to be written to the infoROM.
    //!
    RC WriteInforomParams(LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pInforomParams) const;

    //!
    //! \brief  Copy the current IFR entries in the given RPR object.
    //!
    //! \param  pInforomParams[out]       Filled with the entries from the current RPR object.
    //! \param  includeTpcRepairData  Whether the TPC repair entries should be included
    //!                               in the returned object (if any).
    //!
    RC CopyLwrrInforomParams
    (
        LW90E7_CTRL_RPR_WRITE_OBJECT_PARAMS *pInforomParams,
        bool ignoreTpcRepairData
    ) const;
};
