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

#include "core/utility/strong_typedef_helper.h"
#include "gpu/repair/hbm/hbm_interface/hbm2_interface.h"

// Since MBIST_REPAIR_VECTOR is 384 bit data, we need to read it 12 times
#define MBIST_REPAIR_VECTOR_REG_SIZE  12

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief Micron HBM2e base implementation.
    //!
    //! Contains Micron HBM2e functionality that is consistent across all GPUs.
    //!
    //!
    class MicronHbm2e : public Hbm2Interface
    {
        using HbmChannel      = Hbm::Channel;
        using RowError        = Repair::RowError;
        using Settings        = Repair::Settings;
        using SiteRowErrorMap = map<Site, vector<Hbm::RowErrorInfo>>;

    public:

        struct Dword : public NumericIdentifier
        {
            using NumericIdentifier::NumericIdentifier;
        };

        class RowRepairWriteData : public WdrData
        {
        public:
            RowRepairWriteData() {}

            //!
            //! \brief Constructor.
            //!
            //! \param stack Stack ID.
            //! \param bank Bank number.
            //! \param row Row number.
            //! \param dword DWORD number.
            //!
            RowRepairWriteData
            (
                Stack stack,
                Bank bank,
                Row row,
                Dword dword
            );
        };

        //!
        //! \brief Data corresponding the WIR:PPR_RESPONSE WIR.
        //!
        //! Contains the HBM row repair status.
        //!
        //! NOTE: PPR_INFO only retrieves status of previous repair operation
        //!
        class PprInfoReadData : public WdrData
        {
        public:

            //!
            //! \brief Return true if the given fuse is burned, false otherwise.
            //!
            bool IsPprResponseSuccess() const;
        };

        MicronHbm2e
        (
            const Model& hbmModel,
            std::unique_ptr<GpuHbmInterface> pGpuHbmInterface
        ) : Hbm2Interface(hbmModel, std::move(pGpuHbmInterface)) {}

        RC Setup() override;

        //!
        //! NOTE: Not supported.
        //!
        RC PrintRepairedRows() const override;

        RC RowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors,
            RepairType repairType
        ) const override;

        //!
        //! \brief Perform MBIST operation.
        //!
        RC RunMBist() const override;

        //!
        //! NOTE: This function unlocks additional spare row resources.
        //!
        RC UnlockSpareRows(const Site &hbmSite) const;

    private:
        //!
        //! NOTE: union to access MBIST_REPAIR_VECTOR details
        //!
#pragma pack(push)  // Push current alignment to stack
#pragma pack(1)     // Set alignment to 1 byte boundary
        typedef union
        {
            struct DwordInfo
            {
                    UINT08 failStatus:1;
                    UINT08 sid:1;
                    UINT08 ba:4;
                    UINT08 failingDword:2;
                    UINT16 ra:15;
                    UINT08 repairStatus:1;
            };

            struct DwordInfo dwordInfo[16];
            UINT32 data[MBIST_REPAIR_VECTOR_REG_SIZE];
        } MbistRepairVector;
#pragma pack(pop)
        //!
        //! NOTE: Helper function for UnlockSpareRows
        //!
        RC UnlockSpareRowsSequence
        (
            const Site& hbmSite,
            const Channel& hbmChannel,
            UINT08 instr,
            UINT32 wdrLength,
            UINT32 data
        ) const;

        //!
        //! \brief Perform MBIST refresh1 pattern.
        //!
        RC RunMbistRefresh1Pattern
        (
        ) const;

        //!
        //! \brief Perform MBIST refresh2 pattern.
        //!
        RC RunMbistRefresh2Pattern
        (
        ) const;

        //!
        //! \brief Perform MBIST functional pattern.
        //!
        RC RunMbistFunctionalPattern
        (
        ) const;

        //!
        //! \brief Perform MBIST Fmax 0246 pattern.
        //!
        RC RunMbistFmax0246Pattern
        (
        ) const;

        //!
        //! \brief Perform MBIST Fmax 1357 pattern.
        //!
        RC RunMbistFmax1357Pattern
        (
        ) const;

        //!
        //! \brief Process MBIST_SUMMARY register.
        //!
        RC ProcessMbistSummary
        (
            const Site &hbmSite
        ) const;

        //!
        //! \brief Process MBIST_REPAIR_VECTOR register.
        //!
        RC ProcessMbistRepairVector
        (
            const Site &hbmSite,
            SiteRowErrorMap *pSiteRowErrorMap
        ) const;

    protected:;

        //!
        //! \brief Perform a row repair.
        //!
        //! \param setting Repair settings.
        //! \param rowErrors Rows to repair.
        //! \param pWirRepair Wir for given repair type.
        //! \param repairType Repair type.
        //!
        RC DoRowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors,
            const Wir* pWirRepair,
            RepairType repairType
        ) const;

        struct PprResponse
        {
            HbmChannel channel;
            UINT08 dword;
            Stack stack;

            bool operator < (const PprResponse& resp) const
            {
                return std::tie(channel, dword, stack) <
                        std::tie(resp.channel, resp.dword, resp.stack);
            }
        };

        map<PprResponse, UINT08> s_PprResponseTable;

        void SetupPprResponseTable();

        static constexpr UINT32 s_NumDwordsPerRepair = 2;
        static constexpr UINT08 s_NumChannels = 8;
    };
} // namespace Hbm
} // namespace Memory
