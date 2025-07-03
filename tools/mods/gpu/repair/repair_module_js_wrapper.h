/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/repair/repair_module.h"

#include <memory>

//!
//! \brief Static wrapper for the RepairModule. Facilitates information transfer
//! from JS.
//!
//! NOTE: Because MODS does not guarantee that CLI args are handled in the same
//! order they appear on the command line, the command buffer may (read 'will')
//! be out-of-order.
//!
class RepairModuleJsWrapper
{
public:
    //!
    //! \brief Clear all commands from the command buffer.
    //!
    static void ResetCommands();

    //!
    //! \brief Add remap GPU lanes command to command buffer.
    //!
    //! \param mappingFile File containing lanes to be remapped.
    //!
    static void AddCommandRemapGpuLanes(string mappingFile);

    //!
    //! \brief Add memory repair sequence command to command buffer.
    //!
    static void AddCommandMemRepairSeq();

    //!
    //! \brief Add lane repair command to command buffer.
    //!
    //! \param isHard True if a hard repair should be performed.
    //! \param argument String that represents either a file containing lanes, or is
    //! a specific lane name.
    //!
    static void AddCommandLaneRepair(bool isHard, string argument);

    //!
    //! \brief Add row repair command to command buffer.
    //!
    //! \param isHard True if a hard repair should be performed.
    //! \param argument File containing rows to be repaired.
    //!
    static void AddCommandRowRepair(bool isHard, string argument);

    //!
    //! \brief Add lane remapping information command to command buffer.
    //!
    //! \param deviceId String representing a device ID name (ex. GV100).
    //! \param argument String that represents either a file containing lanes, or is
    //! a specific lane name.
    //!
    static void AddCommandLaneRemapInfo(string deviceId, string argument);

    //!
    //! \brief Add InfoROM RPR populate command to command buffer.
    //!
    static void AddCommandInfoRomRepairPopulate();

    //!
    //! \brief Add InfoROM RPR validate command to command buffer.
    //!
    static void AddCommandInfoRomRepairValidate();

    //!
    //! \brief Add InfoROM RPR clear command to command buffer.
    //!
    static void AddCommandInfoRomRepairClear();

    //!
    //! \brief Add InfoROM RPR print command to command buffer.
    //!
    static void AddCommandInfoRomRepairPrint();

    //!
    //! \brief Add HBM device ID information print command to command buffer.
    //!
    static void AddCommandPrintHbmDevInfo();

    //!
    //! \brief Add print repaired lanes command to command buffer.
    //!
    static void AddCommandPrintRepairedLanes();

    //!
    //! \brief Add print repaired rows command to command buffer.
    //!
    static void AddCommandPrintRepairedRows();

    //!
    //! \brief Add TPC repair populate command to command buffer.
    //!
    static void AddCommandInfoRomTpcRepairPopulate(string inputData, bool bAppend);

    //!
    //! \brief Add TPC repair clearcommand to command buffer.
    //!
    static void AddCommandInfoRomTpcRepairClear();

    //!
    //! \brief Add TPC repair print command to command buffer.
    //!
    static void AddCommandInfoRomTpcRepairPrint();

    //!
    //! \brief Add run MBIST command to command buffer.
    //!
    static void AddCommandRunMBIST();

    //!
    //! \brief Main entry point from MODS JS core.
    //!
    static RC RunJsEntryPoint();

private:
    static std::unique_ptr<RepairModule> s_pRepairModule;
    static std::unique_ptr<Repair::CommandBuffer> s_pCommandBuffer;

    //!
    //! \brief Collects run settings from JavaScript.
    //!
    //! \param[out] pSettings Settings.
    //!
    static RC PullSettingsFromJs(Repair::Settings* pSettings);

    //!
    //! \brief Collect all the run configuration from JavaScript.
    //!
    //! \param pSettings Run settings.
    //! \param ppCmfBuf Run command buffer.
    //!
    static RC CollectConfiguration
    (
        Repair::Settings* pSettings,
        std::unique_ptr<Repair::CommandBuffer>* ppCmdBuf
    );
};
