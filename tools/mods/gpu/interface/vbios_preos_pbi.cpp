/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vbios_preos_pbi.h"

#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include <vector>

namespace
{
    //!
    //! \brief PBI interface command statuses.
    //!
    enum class CommandStatus : UINT32
    {
        Undefined          = 0x00, //!< Command submitted
        Success            = 0x01, //!< Command successfully completed
        Pending            = 0x02, //!< Command accepted
        Busy               = 0x03, //!< Command processing
        UnspecifiedFailure = 0x04, //!< Unknown failure or hang
        IlwalidAddress     = 0x05, //!< Invalid address submitted
        MoreData           = 0x06, //!< User needs to send more data
        IlwalidCommand     = 0x07  //!< Invalid command submitted
    };

    const char* ToString(CommandStatus status)
    {
        switch (status)
        {
            case CommandStatus::Undefined:          return "undefined";
            case CommandStatus::Success:            return "success";
            case CommandStatus::Pending:            return "pending";
            case CommandStatus::Busy:               return "busy";
            case CommandStatus::UnspecifiedFailure: return "unspecified failure";
            case CommandStatus::IlwalidAddress:     return "invalid address";
            case CommandStatus::MoreData:           return "more data";
            case CommandStatus::IlwalidCommand:     return "invalid command";
            default:                                return "unknown";
        }
    }

    //!
    //! \brief Command functions possibly supported by an interface.
    //!
    enum class CommandFunction : UINT08
    {
        GetCapabilities = 0,
        ExelwteRoutine = 11
    };

    //!
    //! \brief What each bit represents in the return value of the 'Get
    //! Capabities' command.
    //!
    enum GetCapabitiesBit
    {
        CAPABILITY_BIT_EXELWTE_ROUTINE = 11,     //!< Command 'Execute Routine'
        CAPABILITY_BIT_GET_DRAM_INFORMATION = 28 //!< Routine 'Get DRAM Information'
    };

    //!
    //! \brief Routine functions possibly support by the Execute Routine command
    //! function.
    //!
    //! \see CommandFunction::ExelwteRoutine
    //!
    enum class Routine : UINT08
    {
        GetDramInformation = 0x84
    };

    //!
    //! \brief DRAM type values for the 'Get DRAM Information' routine.
    //!
    enum class GetDramInformationDramType : UINT08
    {
        Hbm = 0x1
    };

    //!
    //! \brief DRAM type HBM version for the 'Get DRAM Information' routine.
    //!
    enum class GetDramInformationDramTypeHbmVersion : UINT08
    {
        V1 = 0x1
    };

    //!
    //! \brief Client IDs used when acquiring PBI mutex.
    //!
    enum class Client : UINT32
    {
        None = 0x00,
        SbiosSmi = 0x01
    };

    // Expected PCI capability values if the Post-Box interface exists for this
    // card
    static constexpr UINT08 s_CapIdValid       = 0x09;
    static constexpr UINT08 s_CapListSizeValid = 0x14;
    static constexpr UINT08 s_CapControlValid  = 0x01;

    //! \brief MODS client ID number. Used for interface mutex value.
    static constexpr Client s_ModsMutexClient = Client::SbiosSmi;

    //!
    //! \brief Create a command.
    //!
    //! \param gpuIntr True if there is valid data to be processed. Always true
    //! when sending commands.
    //! \param driverNotify True if driver interrupt should be set on command
    //! completion or error.
    //! \param sysNotify True if system interrupt should be set on command
    //! completion or error.
    //! \param cmdFunc Command action to be performed.
    //! \param bufSize Number of DWORDs sent over interface.
    //! \param bufIndex DWORD sequence of data in the data-in register.
    //! 0-indexed.
    //! \param status Status of the command. Normally set to zero.
    //!
    UINT32 CreateCommand
    (
        bool gpuIntr,
        bool driverNotify,
        bool sysNotify,
        CommandFunction cmdFunc,
        UINT08 bufSize,
        UINT08 bufIndex,
        UINT08 status
    )
    {
        using namespace Utility;

        UINT32 value = 0;

        // bit [31]: GPU interrupt
        value |= (gpuIntr ? 1 : 0) << 31;
        // bit [30]: driver notify
        value |= (driverNotify ? 1 : 0) << 30;
        // bit [29]: system notify
        value |= (sysNotify ? 1 : 0) << 29;

        // bit [28]: reserved

        // bit [27:24]: command function
        value |= static_cast<UINT08>(cmdFunc) << 24;

        // bit [23:20]: buffer size
        AssertWithinBitWidth(bufSize, 4, "buffer size");
        value |= bufSize << 20;

        // bit [19:16]: buffer index
        AssertWithinBitWidth(bufIndex, 4, "buffer index");
        value |= bufIndex << 16;

        // bit [15: 8]: reserved

        // bit [ 7: 0]: status
        AssertWithinBitWidth(status, 8, "status");
        value |= status << 0;

        return value;
    }

    //!
    //! \brief Create data-in value for the 'Get DRAM Information' routine.
    //!
    //! \param infoSel Information selector index.
    //!
    UINT32 CreateRoutineGetDramInfoDataIn(UINT08 infoSel)
    {
        UINT32 value = 0;

        // bit [ 7: 0]: routine
        value |= static_cast<UINT08>(Routine::GetDramInformation);

        // bit [15: 8]: information selector
        value |= infoSel << 8;

        return value;
    }

    //!
    //! \brief Print.
    //!
    //! \param pri Print priority.
    //! \param cmd Relevant command name.
    //! \param msg Message.
    //!
    void Print(Tee::Priority pri, const char* cmd, const char* msg)
    {
        MASSERT(cmd);
        MASSERT(msg);
        Printf(pri, "VBIOS preOS PBI command '%s': %s\n", cmd, msg);
    }

    //!
    //! \brief Return the client using the interface.
    //!
    //! \param pRegs Register HAL.
    //! \param[out] pClient Client using the interface.
    //!
    RC GetLwrrentInterfaceClient(RegHal* pRegs, Client* pClient)
    {
        MASSERT(pRegs);
        MASSERT(pClient);
        RC rc;

        // The client lwrrently using the interface is the value of the mutex.
        const UINT32 clientId = pRegs->Read32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_MUTEX_BITS);
        *pClient = static_cast<Client>(clientId);

        return rc;
    }

    //!
    //! \brief Acquire PBI mutex.
    //!
    //! \param pRegs Register HAL.
    //! \return RC::RESOURCE_IN_USE if the interface is in use.
    //!
    RC AcquireMutex(RegHal* pRegs)
    {
        MASSERT(pRegs);

        UINT32 id = pRegs->Read32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_MUTEX_BITS);

        if (id != static_cast<UINT32>(Client::None))
        {
            return RC::RESOURCE_IN_USE;
        }

        // Attempt to acquire
        pRegs->Write32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_MUTEX_BITS,
                       static_cast<UINT32>(s_ModsMutexClient));

        // Check if acquired
        id = pRegs->Read32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_MUTEX_BITS);
        if (id != static_cast<UINT32>(s_ModsMutexClient))
        {
            Print(Tee::PriError, "interface acquisition",
                  "Unable to aquire interface");
            return RC::SOFTWARE_ERROR;
        }

        return RC::OK;
    }

    //!
    //! \brief Acquire PBI mutex. Retries if unsuccessful.
    //!
    //! \param pRegs Register HAL.
    //! \param maxAttempts Maximum number of attempts.
    //! \param retryDelayMs Time in miliseconds to wait between attempts.
    //! \return RC::RESOURCE_IN_USE if the interface is in use.
    //!
    RC PollAcquireMutex
    (
        RegHal* pRegs,
        UINT32 maxAttempts,
        FLOAT64 retryDelayMs
    )
    {
        MASSERT(pRegs);
        RC rc;

        UINT32 attempts = 0;
        do
        {
            rc = AcquireMutex(pRegs);
            if (rc == RC::OK) { break; /* Got it */ }

            if (rc == RC::RESOURCE_IN_USE)
            {
                // Interface in use. Try again.
                rc.Clear();
                attempts++;
                Tasker::Sleep(retryDelayMs);
            }
            else
            {
                return rc;
            }
        } while (attempts <= maxAttempts);

        if (attempts == maxAttempts)
        {
            // Interface was continuously in use
            Print(Tee::PriError, "interface acquisition",
                  "Interface in use, unable to acquire");

            Client lwrClient;
            CHECK_RC(GetLwrrentInterfaceClient(pRegs, &lwrClient));

            if (lwrClient == s_ModsMutexClient)
            {
                Printf(Tee::PriDebug, "The client using the interface is using "
                       "the MODS client ID. Did MODS crash without freeing the "
                       "mutex? If so, may need a system restart.\n");
            }

            return RC::RESOURCE_IN_USE;
        }

        return rc;
    }

    //!
    //! \brief Release PBI mutex.
    //!
    //! \param pRegs Register HAL.
    //!
    RC ReleaseMutex(RegHal* pRegs)
    {
        RC rc;

        const UINT32 id = pRegs->Read32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_MUTEX_BITS);
        if (id != static_cast<UINT32>(s_ModsMutexClient))
        {
            Printf(Tee::PriError, "VBIOS preOS PBI: Attempt to release "
                   "interface mutex while owned by another client\n");
            MASSERT(!"Attempt to release interface mutex while owned by "
                    "another client");
            return RC::SOFTWARE_ERROR;
        }

        pRegs->Write32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_MUTEX_BITS,
                       static_cast<UINT32>(Client::None));

        return rc;
    }


    //!
    //! \brief Returns RC::OK if the VBIOS preOD PBI inteface exists or is valid,
    //! non-RC::OK otherwise.
    //!
    //! \param pRegs Register HAL.
    //!
    RC CheckInterfaceExists(RegHal* pRegs)
    {
        MASSERT(pRegs);

        const UINT32 capability = pRegs->Read32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_HEADER);
        if (pRegs->GetField(capability,
                            MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_HEADER_CAP_ID) != s_CapIdValid ||
            pRegs->GetField(capability,
                            MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_HEADER_LIST_LENGTH) != s_CapListSizeValid ||
            pRegs->GetField(capability,
                            MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_HEADER_FEATURE_MSGBOX) != s_CapControlValid)
        {
            Printf(Tee::PriLow, "VBIOS preOS PBI interface is not available or is not valid\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        return RC::OK;
    }

    struct WaitForStatusPollArgs
    {
        StickyRC*      pFirstRc = nullptr;
        CommandStatus* pStatus  = nullptr;
        RegHal*        pRegs    = nullptr;

        WaitForStatusPollArgs(StickyRC* pFirstRc, CommandStatus* pStatus, RegHal* pRegs)
            : pFirstRc(pFirstRc), pStatus(pStatus), pRegs(pRegs) {}
    };

    bool WaitForStatusPollFunc(void* pArgs)
    {
        MASSERT(pArgs);
        WaitForStatusPollArgs* pPollArgs =
            static_cast<WaitForStatusPollArgs*>(pArgs);
        if (*(pPollArgs->pFirstRc) != RC::OK) { return true; }

        RegHal*        pRegs    = pPollArgs->pRegs;
        StickyRC*      pFirstRc = pPollArgs->pFirstRc;
        CommandStatus* pStatus  = pPollArgs->pStatus;

        const UINT32 cmdResult = pRegs->Read32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_COMMAND);

        constexpr UINT32 cmdStatusMask = 0xff;
        *pStatus = static_cast<CommandStatus>(cmdResult & cmdStatusMask);
        constexpr UINT32 cmdIntrMask = 1U << 31;
        const bool intr = cmdResult & cmdIntrMask;
        if (intr)
        {
            // Still exelwting, keep polling
            return false;
        }

        switch (*pStatus)
        {
            case CommandStatus::Undefined:
            case CommandStatus::Busy:
            case CommandStatus::Pending:
                return false; // Keep polling

            default:
                *pFirstRc = RC::PCI_FUNCTION_NOT_SUPPORTED;
                // [[ fallthrough ]] @c++17
            case CommandStatus::Success:
                return true;
        }
    }

    //!
    //! \brief Wait for command status. Blocking call.
    //!
    //! If the status indicates a bad state, returns a non-OK RC. The status
    //! will always be set.
    //!
    //! \param pRegs Register HAL.
    //! \param pStatus Status of the given command.
    //!
    RC WaitForStatus(RegHal* pRegs, CommandStatus* pStatus)
    {
        MASSERT(pRegs);
        MASSERT(pStatus);
        StickyRC firstRc;

        constexpr FLOAT64 timeoutMs = 2000;
        WaitForStatusPollArgs args(&firstRc, pStatus, pRegs);
        firstRc = POLLWRAP(WaitForStatusPollFunc, &args, timeoutMs);
        return firstRc;
    }

    //!
    //! \brief Send command to the interface.
    //!
    //! \param pRegs Register HAL.
    //! \param command Command.
    //! \param dataIn Command's data-in.
    //! \param[out] pDataOut Data-out of command. Nullptr if command has no
    //! output.
    //! \param[out] pStatus Status of the command after exelwtion.
    //!
    RC ExelwteCommand
    (
        RegHal* pRegs,
        UINT32 command,
        UINT32 dataIn,
        UINT32* pDataOut,
        CommandStatus* pStatus
    )
    {
        MASSERT(pRegs);
        MASSERT(pStatus);
        RC rc;

        // Send command
        pRegs->Write32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_DATA_IN_BITS, dataIn);
        pRegs->Write32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_COMMAND, command);

        // Wait for completion
        CHECK_RC(WaitForStatus(pRegs, pStatus));

        if (pDataOut)
        {
            *pDataOut = pRegs->Read32(MODS_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_DATA_OUT_BITS);
        }

        return rc;
    }

    RC CheckDramInfoDataHeader
    (
        GpuSubdevice* pSubdev,
        UINT32 maxSelectorVal,
        UINT32 dramType,
        UINT32 dramTypeVersion
    )
    {
        RC rc;

        const UINT32 numHbmSites = pSubdev->GetNumHbmSites();
        if (numHbmSites > 0)
        {
            if (maxSelectorVal > numHbmSites)
            {
                Print(Tee::PriLow, "Get DRAM Information",
                      Utility::StrPrintf("Number of reported HBM sites (%u) is greater "
                                         "than expected (%u)\n",
                                         maxSelectorVal, numHbmSites).c_str());
            }

            if (dramType != static_cast<UINT32>(GetDramInformationDramType::Hbm))
            {
                Print(Tee::PriLow, "Get DRAM Information", "Unsupported DRAM type");
                return RC::UNSUPPORTED_FUNCTION;
            }

            if (dramTypeVersion !=
                static_cast<UINT32>(GetDramInformationDramTypeHbmVersion::V1))
            {
                Print(Tee::PriLow, "Get DRAM Information", "Unsupported DRAM type version");
                return RC::UNSUPPORTED_FUNCTION;
            }
        }
        else
        {
            // TODO[aperiwal]
        }

        return rc;
    }
};

RC VbiosPreOsPbi::GetDramInformation
(
    GpuSubdevice* pSubdev,
    vector<Memory::DramDevId>* pDramDevIds
)
{
    MASSERT(pSubdev);
    MASSERT(pDramDevIds);
    RC rc;
    RegHal& regs = pSubdev->Regs();

    const UINT32 numHbmSites = pSubdev->GetNumHbmSites();
    if (numHbmSites == 0)
    {
        // GDDR info reads are not supported by VBIOS lwrrently, so just return early
        // Remove this condition when (and if) support is added 
        Printf(Tee::PriLow, "DRAM info read from PreOS PBI not supported\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Confirm VBIOS preOS PBI exists
    CHECK_RC(CheckInterfaceExists(&regs));

    // Acquire the interface
    constexpr UINT32 maxAcquisitionAttempts = 5;
    constexpr FLOAT64 acquisitionWaitMs = 10;
    CHECK_RC(PollAcquireMutex(&regs, maxAcquisitionAttempts,
                              acquisitionWaitMs));

    DEFER
    {
        if (ReleaseMutex(&regs) != RC::OK)
        {
            Print(Tee::PriWarn, "release mutex", "Failed to release muteX");
        }
    };

    // Get PBI capabilities
    //
    CommandStatus status;
    UINT32 command = CreateCommand(true, false, false,
                                   CommandFunction::GetCapabilities,
                                   0, 0, 0);

    constexpr UINT32 noDataIn = 0;
    UINT32 capabilities = 0;
    rc = ExelwteCommand(&regs, command, noDataIn, &capabilities, &status);
    if (rc != RC::OK)
    {
        // A device can pretend to have PBI support, using a fake PCI config
        // space entry, even though it does not respond to PBI calls.
        Print(Tee::PriLow, "Get Capabilities", "Device did not respond");
        return RC::UNSUPPORTED_FUNCTION;
    }
    MASSERT(status == CommandStatus::Success);

    // Must have 'execute routine' capability
    const bool supportExecRoutine =
        capabilities & (1U << CAPABILITY_BIT_EXELWTE_ROUTINE);
    if (!supportExecRoutine)
    {
        Print(Tee::PriLow, "Execute Routine", "Device does not support command");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Must support 'get dram information' routine
    const bool supportGetDramInfo =
        capabilities & (1U << CAPABILITY_BIT_GET_DRAM_INFORMATION);
    if (!supportGetDramInfo)
    {
        Print(Tee::PriLow, "Get DRAM Information", "Device does not support routine");
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Get DRAM information
    //
    // Get header
    command = CreateCommand(true, false, false, CommandFunction::ExelwteRoutine,
                            0, 0, 0);
    UINT32 dataIn = CreateRoutineGetDramInfoDataIn(0);

    UINT32 header = 0;
    CHECK_RC(ExelwteCommand(&regs, command, dataIn, &header, &status));
    MASSERT(status == CommandStatus::Success);

    const UINT32 getDramInfoVersion = header & 0xff;
    const UINT32 maxSelectorVal     = (header >> 8) & 0xff;
    const UINT32 dramType           = (header >> 16) & 0xff;
    const UINT32 dramTypeVersion    = (header >> 24) & 0xff;

    // Check if API is supported
    if (getDramInfoVersion != 1)
    {
        Print(Tee::PriLow, "Get DRAM Information", "Unsupported routine version");
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(CheckDramInfoDataHeader(pSubdev, maxSelectorVal, dramType, dramTypeVersion));

    // Get each chip's information
    constexpr UINT32 numBufEls = 3;
    for (UINT32 selector = 1; selector <= maxSelectorVal; ++selector)
    {
        Memory::DramDevId devIdInfo = {};
        // Selector value N>0 corresponds to HBM site N-1
        devIdInfo.index = selector - 1;
        devIdInfo.rawDevId.reserve(numBufEls);

        for (UINT32 bufIndex = 0; bufIndex < numBufEls; ++bufIndex)
        {
            command = CreateCommand(true, false, false,
                                    CommandFunction::ExelwteRoutine,
                                    0, bufIndex, 0);
            dataIn = CreateRoutineGetDramInfoDataIn(selector);
            UINT32 dataOut = 0;

            rc = ExelwteCommand(&regs, command, dataIn, &dataOut, &status);

            if (status == CommandStatus::IlwalidAddress)
            {
                // No data available, possibly due to inactive site
                MASSERT(rc != RC::OK);
                Print(Tee::PriLow, "Get DRAM Information",
                      "No data at the specific buffer index or selector");
                rc.Clear();
                break; // Move to next site
            }
            else if (status == CommandStatus::MoreData)
            {
                // Expected status, not an error
                MASSERT(rc != RC::OK);
                Print(Tee::PriLow, "Get DRAM Information", "More data to read");
                rc.Clear();

                if (bufIndex + 1 >= numBufEls)
                {
                    Print(Tee::PriError, "Get DRAM Information",
                          "Interface reports more data to read, but reported "
                          "max selector value has been reached\n");
                    return RC::SOFTWARE_ERROR;
                }
            }
            else if (rc != RC::OK)
            {
                Printf(Tee::PriError, "Unexpected error: %s\n",
                       ToString(status));
                return rc;
            }

            devIdInfo.rawDevId.push_back(dataOut);
        }

        if (!devIdInfo.rawDevId.empty())
        {
            pDramDevIds->push_back(devIdInfo);
        }
    }

    return rc;
}
