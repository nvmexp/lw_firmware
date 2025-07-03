/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/hbm/hbm_spec_devid.h"
#include "gpu/repair/hbm/hbm_interface/hbm_interface.h"

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief HBM2 interface.
    //!
    class Hbm2Interface : public HbmInterface
    {
        using HbmChannel = Memory::Hbm::Channel;
        using FbpaLane = Memory::FbpaLane;
        using Settings = Repair::Settings;

    public:
        //!
        //! \brief Full remap value for a single HBM DWORD.
        //!
        //! \see LaneRemap for remap encodings.
        //!
        class HbmDwordRemap
        {
        private:
            UINT16 m_Value = 0; //!< DWORD remap value

        public:
            HbmDwordRemap() {}
            explicit HbmDwordRemap(UINT16 remapValue) : m_Value(remapValue) {}

            //!
            //! \brief DWORD remap encoding sentinal values.
            //!
            enum DwordEncoding : UINT16
            {
                DWORD_NO_REMAP = 0xffff //!< No DWORD remapping.
            };

            //! DWORD remap encoding bit width.
            static constexpr UINT32 dwordEncodingBitWidth =
                LaneRemap::bytePairEncodingBitWidth * 2;
            static_assert(sizeof(DwordEncoding) == sizeof(m_Value),
                          "DWORD remap value types not in sync");

            //!
            //! \brief Get the DWORD remap value.
            //!
            UINT16 Get() const { return m_Value; }

            //!
            //! \brief Return all remapped lanes within the DWORD.
            //!
            //! \param[out] Remapped lanes within the DWORD.
            //!
            RC GetRemappedLanes(vector<DwordLane>* pLanes) const;

            //!
            //! \brief True if there is no remapping done on the DWORD, false
            //! otherwise.
            //!
            bool IsNoRemap() const;

            //!
            //! \brief True if the given remap can be applied, false otherwise.
            //!
            //! \param desiredRemap Remap.
            //!
            bool IsRemappable(const LaneRemap& desiredRemap) const;
        };

        //!
        //! \brief Lane remapping data read from the HBM. Consists of the full
        //! channel worth of remap data.
        //!
        class HbmLaneRemapData : public WdrData
        {
        public:
            //!
            //! \brief Extract DWORD remapping value.
            //!
            //! DWORD refers to a Data Word, which is 32 DQ/DATA lanes plus any
            //! additional per-dword and per-byte lanes like ECC and DBI.
            //!
            //! \param dword DWORD number to extract. 0-indexed.
            //!
            //! \see ApplyRemap
            //!
            HbmDwordRemap ExtractDwordRemapping(UINT32 dword) const;

            //!
            //! \brief Modify the HBM lane remap data to include the given
            //! remapping for the given DWORD.
            //!
            //! \param dword DWORD to remap.
            //! \param remap Lane remapping.
            //!
            //! \see ExtractDwordRemapping
            //!
            void ApplyRemap(UINT32 dword, const LaneRemap& remap);
        };

        //!
        //! \brief Write data for WIR HARD_REPAIR.
        //!
        struct HardLaneRepairWriteData : public HbmLaneRemapData
        {
        public:
            HardLaneRepairWriteData() { m_Data.resize(3, ~0U); }

            //!
            //! \brief Modify the HBM lane remap data to include the given
            //! remapping for the given DWORD.
            //!
            //! \param dword DWORD to remap.
            //! \param remap Lane remapping.
            //! \param nibbleMask Mask identifying which nibble in the given
            //! remap value should be applied.
            //!
            //! \see ExtractDwordRemapping
            //!
            void ApplyRemap(UINT32 dword, const LaneRemap& remap, UINT16 nibbleMask);
        };

        //!
        //! \brief Constructor.
        //!
        //! \param hbmModel The HBM model this interface represents.
        //! \param pGpuHbmInterface The GPU-to-HBM interface that is used to
        //! communicate with the HBM.
        //!
        Hbm2Interface(const Model& hbmModel, std::unique_ptr<GpuHbmInterface> pGpuHbmInterface);

        RC Setup() override;
        RC LaneRepair
        (
            const Settings& settings,
            const vector<FbpaLane>& lanes,
            RepairType repairType
        ) const override;

        RC CalcLaneRemapValue(const HbmLane& lane, LaneRemap* pRemap) const override;

        RC GetHbmSiteDeviceInfo(Site hbmSite, HbmDeviceIdData* pDevIdData) const override;

        RC GetRepairedLanesFromGpu(vector<FbpaLane>* pLanes) const override;

        RC GetRepairedLanesFromHbm(vector<FbpaLane>* pLanes) const override;
        RC GetRepairedLanesFromHbm
        (
            const Site& hbmSite,
            const HbmChannel& hbmChannel,
            vector<FbpaLane>* pLanes
        ) const override;

    protected:
        //! Maximum number of channels available.
        static constexpr UINT32 s_MaxNumChannels = 8;
        //! Number of banks per channel.
        static constexpr UINT32 s_NumBanksPerChannel = 16;
        //! Number of data lane DWORDs per channel.
        static constexpr UINT32 s_NumDwordsPerChannel = 4;

    private:
        //!
        //! \brief Perform a soft lane repair.
        //!
        //! \param settings Repair settings.
        //! \param lanes FBPA lanes to repair.
        //!
        RC SoftLaneRepair(const Settings& settings, const vector<FbpaLane>& lanes) const;

        //!
        //! \brief Perform a hard lane repair.
        //!
        //! \param settings Repair settings.
        //! \param lanes FBPA lanes to repair.
        //!
        RC HardLaneRepair(const Settings& settings, const vector<FbpaLane>& lanes) const;

        //!
        //! \brief Create lane remapping nibble masks for hard lane repairs.
        //!
        //! Hard lane repairs requires fusing an 8-bit value one nibble at a
        //! time. The first nibble fused must be the "repair in other byte"
        //! sential value. For colwenience, the first mask in the resulting
        //! masks will be for the "repair in other byte" nibble.
        //!
        //! \param laneRemap Remap value for a single lane.
        //! \param[out] pNibbleMasks Nibble masks. First mask will be for the
        //! "repair in other byte" remap value.
        //!
        void CreateLaneRepairFuseNibbleMasks
        (
            const LaneRemap& laneRemap,
            vector<UINT16>* pNibbleMasks
        ) const;

        //!
        //! \brief Return lanes that have been remapped in the given HBM DWORD
        //! remapping. No lanes added if no remapping was done.
        //!
        //! Since there can be multiple repairs per HBM fuse remap value, the
        //! corresponding number of object will be created; one object per repair.
        //!
        //! \param hbmSite HBM site.
        //! \param hbmChannel HBM channel.
        //! \param hbmDword DWORD within the HBM channel.
        //! \param dwordRemap HBM DWORD remap.
        //! \param[out] Remapped lanes within the DWORD.
        //!
        RC ExtractRepairedLanesFromHbmDwordRemap
        (
            const Site& hbmSite,
            const HbmChannel& hbmChannel,
            UINT32 hbmDword,
            const HbmDwordRemap& dwordRemap,
            vector<FbpaLane>* pLanes
        ) const;

        //!
        //! \brief Return lanes that have been remapped in the given DWORD
        //! according to the GPU link repair registers.
        //!
        //! \param hwFbpa Haredware FBPA.
        //! \param fbpaSubp FBPA subpartition.
        //! \param fbpaDword FBPA DWORD within the subpartition.
        //! \param[out] Remapped lanes within the DWORD.
        //!
        RC ExtractLaneRepairFromLinkRepairReg
        (
            HwFbpa hwFbpa,
            FbpaSubp fbpaSubp,
            UINT32 fbpaDword,
            vector<FbpaLane>* pLanes
        ) const;
    };
} // namespace Hbm
} // namespace Memory
