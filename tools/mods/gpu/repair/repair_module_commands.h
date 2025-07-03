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

#include "core/include/hbmtypes.h"
#include "core/include/rc.h"
#include "gpu/repair/gddr/gddr_interface/gddr_interface.h"
#include "gpu/repair/hbm/gpu_interface/gpu_hbm_interface.h"
#include "gpu/repair/hbm/hbm_interface/hbm_interface.h"
#include "gpu/repair/hbm/hbm_wir.h"
#include "gpu/repair/mem_repair_config.h"
#include "gpu/repair/tpc_repair/tpc_repair.h"

#include <functional> // std::reference_wrapper
#include <list>
#include <memory>
#include <vector>

namespace Repair
{
    //!
    //! \brief A repair-related command to be run.
    //!
    class Command
    {
    public:
        //!
        //! \brief Command properties.
        //!
        enum class Flags : UINT32
        {
            None              = 0,      //!< Used for type safe zeroing of the flags
            PrimaryCommand    = 1 << 0, //!< Can only run one primary command per MODS exelwtion
            ExplicitRepair    = 1 << 1, //!< A user specified repair
            NoGpuRequired     = 1 << 2, //!< Does not require a GPU to run the command
            InstInSysRequired = 1 << 3, //!< Must be running with -inst_in_sys to run command
        };

        //!
        //! \brief The command group the command belongs to
        //!
        enum class Group : UINT32
        {
            Unknown,
            Mem,
            Tpc,
        };

        //!
        //! \brief Possible commands.
        //!
        enum class Type : UINT32
        {
            Unknown,
            ReconfigGpuLanes,
            MemRepairSeq,
            LaneRepair,
            RowRepair,
            LaneRemapInfo,
            InfoRomRepairPopulate,
            InfoRomRepairValidate,
            InfoRomRepairClear,
            InfoRomRepairPrint,
            PrintHbmDevInfo,
            PrintRepairedLanes,
            PrintRepairedRows,
            InfoRomTpcRepairPopulate,
            InfoRomTpcRepairClear,
            InfoRomTpcRepairPrint,
            RunMBIST
        };
        static string ToString(Repair::Command::Type commandType);

        //!
        //! \brief Resulting information from command validation.
        //!
        struct ValidationResult
        {
            //! True if the command is attempting a hard repair.
            bool isAttemptingHardRepair = false;
        };

        Command(Type type, Group group, Flags flags) :
            m_Type(type), m_Group(group), m_Flags(flags) {}
        Command(Type type, Group group) :
            m_Type(type), m_Group(group), m_Flags(Flags::None) {}
        virtual ~Command() {}

        //!
        //! \brief Return the command type.
        //!
        Type GetType() const { return m_Type; }

        //!
        //! \brief Return the command group
        //!
        Group GetGroup() const { return m_Group; }

        //!
        //! \brief Return the command flags.
        //!
        Flags GetFlags() const { return m_Flags; }

        //!
        //! \brief Return true if the given flags are set.
        //!
        //! \param flags Flags to check.
        //!
        bool IsSet(Flags flags) const;

        //!
        //! \brief Validate the command configuration.
        //!
        //! \param settings Repair settings.
        //! \param[out] pResult Additional result information.
        //!
        virtual RC Validate
        (
            const Settings& settings,
            ValidationResult* pResult
        ) const;

        //! \brief Execute the command
        //!
        //! \param settings  Repair settings.
        //! \param tpcRepair TPC repair object
        //!
        virtual RC Execute
        (
            const Settings& settings,
            const TpcRepair& tpcRepair
        ) const;

        //!
        //! \brief Execute the command over the HBM interface.
        //!
        //! \param settings       Repair settings.
        //! \param pLwrrSysState  Current system state info.
        //! \param ppHbmInterface Target HBM interface.
        //!
        virtual RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const;

        //!
        //! \brief Execute the command over the HBM interface.
        //!
        //! \param settings Repair settings.
        //! \param hbmInterface Target HBM interface.
        //!
        virtual RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState,
            std::unique_ptr<Memory::Gddr::GddrInterface>* ppGddrInterface
        ) const;

        virtual string ToString() const;

    protected:
        Type  m_Type  = Command::Type::Unknown;  //!< Command type
        Group m_Group = Command::Group::Unknown; //!< Command group
        Flags m_Flags = Command::Flags::None;    //!< Command flags/properties
    };
    BITFLAGS_ENUM(Command::Flags);

    //!
    //! \brief Remaps GPU lanes to their spares.
    //!
    class RemapGpuLanesCommand : public Command
    {
    public:
        //!
        //! \brief Constructor.
        //!
        //! \param mappingFile File containing FBPA lanes to remap.
        //!
        RemapGpuLanesCommand(string mappingFile);

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;

        string ToString() const override;

    private:
        string m_MappingFile;
    };

    //!
    //! \brief Performs the memory repair sequence: test, repair, and validate.
    //!
    //! NOTE: The repair mode comes from global modifiers. It is diffilwlt to get
    //! both the memrepair sequence and the repair mode at the same time since
    //! they are seperate MODS args.
    //!
    class MemRepairSeqCommand : public Command
    {
    public:
        MemRepairSeqCommand();

        RC Validate
        (
            const Settings& settings,
            ValidationResult* pResult
        ) const override;

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Repair lanes.
    //!
    class LaneRepairCommand : public Command
    {
    public:
        //!
        //! \brief Constructor.
        //!
        //! \param repairType Type of repair to be performed.
        //! \param argument Either a path to a file with FBPA lanes to be
        //! repaired or a single FBPA lane name.
        //!
        LaneRepairCommand(Memory::RepairType repairType, string argument);

        RC Validate
        (
            const Settings& settings,
            ValidationResult* pResult
        ) const override;

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;

        string ToString() const override;

    private:
        //! Type of repair to be performed.
        Memory::RepairType m_RepairType = Memory::RepairType::UNKNOWN;

        //! Either a filename or a specific lane.
        string m_Argument;
    };

    //!
    //! \brief Repair rows.
    //!
    class RowRepairCommand : public Command
    {
    public:

        //!
        //! \brief Constructor.
        //!
        //! \param repairType Type of repair to be performed.
        //! \param argument Path to a file with row to be repaired.
        //!
        RowRepairCommand(Memory::RepairType repairType, string argument);

        RC Validate
        (
            const Settings& settings,
            ValidationResult* pResult
        ) const override;

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState,
            std::unique_ptr<Memory::Gddr::GddrInterface>* ppGddrInterface
        ) const override;

        string ToString() const override;

    private:
        //! Type of repair to be performed.
        Memory::RepairType m_RepairType = Memory::RepairType::UNKNOWN;

        //! Path to a file containing failing pages
        string m_FailingPagesFilePath;

        RC GetRowErrorsFromFile(vector<RowError> *pRowErrors) const;
    };

    //!
    //! \brief Display information about lane remapping that would occur for
    //! particular lanes if repaired.
    //!
    class LaneRemapInfoCommand : public Command
    {
    public:

        //!
        //! \brief Constructor.
        //!
        //! \param deviceIdStr Device intended to be remapped.
        //! \param argument Either a path to a file with FBPA lanes to be
        //! remapped or a single FBPA lane name.
        //!
        LaneRemapInfoCommand(string deviceIdStr, string argument);

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;

        string ToString() const override;

    private:
        string m_DeviceIdStr;
        string m_Argument;
    };

    //!
    //! \brief Populate the contents of the InfoROM RPR object.
    //!
    class InfoRomRepairPopulateCommand : public Command
    {
    public:
        InfoRomRepairPopulateCommand();

        RC Validate
        (
            const Settings& settings,
            ValidationResult* pResult
        ) const override;

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Validate the contents the InfoROM RPR object.
    //!
    class InfoRomRepairValidateCommand : public Command
    {
    public:
        InfoRomRepairValidateCommand();

        RC Validate
        (
            const Settings& settings,
            ValidationResult* pResult
        ) const override;

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Clear the contents of the InfoROM RPR object.
    //!
    class InfoRomRepairClearCommand : public Command
    {
    public:
        InfoRomRepairClearCommand();

        RC Validate
        (
            const Settings& settings,
            ValidationResult* pResult
        ) const override;

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Populate the InfoROM RPR object.
    //!
    class InfoRomRepairPrintCommand : public Command
    {
    public:
        InfoRomRepairPrintCommand();

        RC Validate
        (
            const Settings& settings,
            ValidationResult* pResult
        ) const override;

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Populate the contents of the InfoROM object with TPC repair data
    //!
    class InfoRomTpcRepairPopulateCommand : public Command
    {
    public:
        InfoRomTpcRepairPopulateCommand(string repairData, bool bAppend);

        RC Execute
        (
            const Settings& settings,
            const TpcRepair& tpcRepair
        ) const override;

    private:
        string m_RepairData;
        bool   m_bAppend;
    };

    //!
    //! \brief Clear the TPC repair entries from InfoROM object
    //!
    class InfoRomTpcRepairClearCommand : public Command
    {
    public:
        InfoRomTpcRepairClearCommand();

        RC Execute
        (
            const Settings& settings,
            const TpcRepair& tpcRepair
        ) const override;
    };

    //!
    //! \brief Print the TPC repair entries from InfoROM object
    //!
    class InfoRomTpcRepairPrintCommand : public Command
    {
    public:
        InfoRomTpcRepairPrintCommand();

        RC Execute
        (
            const Settings& settings,
            const TpcRepair& tpcRepair
        ) const override;
    };

    //!
    //! \brief Print the HBM device information.
    //!
    class PrintHbmDevInfoCommand : public Command
    {
    public:
        PrintHbmDevInfoCommand();

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Print repaired lanes.
    //!
    class PrintRepairedLanesCommand : public Command
    {
    public:
        PrintRepairedLanesCommand();

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Print repaired rows.
    //!
    class PrintRepairedRowsCommand : public Command
    {
    public:
        PrintRepairedRowsCommand();

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState, 
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Run MBIST.
    //!
    class RunMBISTCommand : public Command
    {
    public:

        //!
        //! \brief Constructor.
        //!
        //!
        RunMBISTCommand();

        RC Execute
        (
            const Settings& settings,
            SystemState* pLwrrSysState,
            std::unique_ptr<Memory::Hbm::HbmInterface>* ppHbmInterface
        ) const override;
    };

    //!
    //! \brief Buffers commands. Insertion order is preserved.
    //!
    class CommandBuffer
    {
        //! Collection of references to other commands.
        using Buffer = vector<std::reference_wrapper<const Command>>;

    public:
        //!
        //! \brief Access the command buffer.
        //!
        const Buffer& Get() const { return m_Buffer; }

        //!
        //! \brief Return true if the buffer is empty, false otherwise.
        //!
        bool Empty() const { return m_Buffer.empty(); }

        //!
        //! \brief Clear the contents of the command buffer.
        //!
        void Clear();

        //!
        //! \brief Add a remap GPU lanes command.
        //!
        //! \param mappingFile File containing FBPA lanes to remap.
        //!
        void AddCommandRemapGpuLanes(string mappingFile);

        //!
        //! \brief Add a memory repair sequence command.
        //!
        void AddCommandMemRepairSeq();

        //!
        //! \brief Add a lane repair command.
        //!
        //! \param repairType Type of repair to be performed.
        //! \param argument Either a path to a file with FBPA lanes to be
        //! repaired or a single FBPA lane name.
        //!
        void AddCommandLaneRepair(Memory::RepairType repairType, string argument);

        //!
        //! \brief Add a row repair command.
        //!
        //! \param repairType Type of repair to be performed.
        //! \param argument Path to a file with row to be repaired.
        //!
        void AddCommandRowRepair(Memory::RepairType repairType, string argument);

        //!
        //! \brief Add a lane remap information command.
        //!
        //! \param deviceIdStr Device intended to be remapped.
        //! \param argument Either a path to a file with FBPA lanes to be
        //! remapped or a single FBPA lane name.
        //!
        void AddCommandLaneRemapInfo(string deviceIdStr, string argument);

        //!
        //! \brief Add a InfoROM RPR object populate command.
        //!
        void AddCommandInfoRomRepairPopulate();

        //!
        //! \brief Add a InfoROM RPR object validate command.
        //!
        void AddCommandInfoRomRepairValidate();

        //!
        //! \brief Add a InfoROM RPR object clear command.
        //!
        void AddCommandInfoRomRepairClear();

        //!
        //! \brief Add a InfoROM RPR object print command.
        //!
        void AddCommandInfoRomRepairPrint();

        //!
        //! \brief Add a print HBM device information command.
        //!
        void AddCommandPrintHbmDevInfo();

        //!
        //! \brief Add a print repaired lanes command.
        //!
        void AddCommandPrintRepairedLanes();

        //!
        //! \brief Add a print repaired rows command.
        //!
        void AddCommandPrintRepairedRows();

        //!
        //! \brief Add a InfoROM TPC repair populate command
        //!
        //! \param inputData TPC Repair info provided as input
        //! \param bAppend   Whether entries should be appended to the existing list or
        //!                  overwrite them
        //!
        void AddCommandInfoRomTpcRepairPopulate(string inputData, bool bAppend);

        //!
        //! \brief Add a InfoROM TPC repair entries clear command.
        //!
        void AddCommandInfoRomTpcRepairClear();

        //!
        //! \brief Add a InfoROM TPC repair entries print command.
        //!
        void AddCommandInfoRomTpcRepairPrint();

        //!
        //! \brief Run MBIST.
        //!
        void AddCommandRunMBIST();

        //! Command comparison function.
        using CommandOrderCmpFunc = bool(*)(const Command&, const Command&);

        //!
        //! \brief Reorder the command buffer according to the given comparator.
        //!
        //! \param cmp Comparator.
        //!
        void OrderCommandBuffer(CommandOrderCmpFunc cmp);

        //!
        //! \brief Return true if the buffer contains the given command, false
        //! otherwise.
        //!
        //! \param type Command type to check for membership.
        //!
        bool HasCommand(Repair::Command::Type type) const;
        string ToString() const;

    private:
        //! Collection of all the inserted commands.
        Buffer m_Buffer;

        /*
         * Backing stores to hold the individual command settings.
         *
         * NOTE: Specifically uses std::list to avoid pointer ilwalidation in
         * the command buffer.
         */
        list<RemapGpuLanesCommand>            m_RemapGpuLanesBackingStore;
        list<MemRepairSeqCommand>             m_MemRepairSeqBackingStore;
        list<LaneRepairCommand>               m_LaneRepairBackingStore;
        list<RowRepairCommand>                m_RowRepairBackingStore;
        list<LaneRemapInfoCommand>            m_LaneRemapInfoBackingStore;
        list<InfoRomRepairPopulateCommand>    m_InfoRomRepairPopulateBackingStore;
        list<InfoRomRepairValidateCommand>    m_InfoRomRepairValidateBackingStore;
        list<InfoRomRepairClearCommand>       m_InfoRomRepairClearBackingStore;
        list<InfoRomRepairPrintCommand>       m_InfoRomRepairPrintBackingStore;
        list<PrintHbmDevInfoCommand>          m_PrintHbmDevInfoBackingStore;
        list<PrintRepairedLanesCommand>       m_PrintRepairedLanesBackingStore;
        list<PrintRepairedRowsCommand>        m_PrintRepairedRowsBackingStore;
        list<InfoRomTpcRepairPopulateCommand> m_InfoRomTpcRepairPopulateBackingStore;
        list<InfoRomTpcRepairClearCommand>    m_InfoRomTpcRepairClearBackingStore;
        list<InfoRomTpcRepairPrintCommand>    m_InfoRomTpcRepairPrintBackingStore;
        list<RunMBISTCommand>                 m_RunMBISTBackingStore;
    };
};
