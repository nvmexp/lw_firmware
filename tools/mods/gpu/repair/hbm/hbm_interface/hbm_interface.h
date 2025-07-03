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

#include "core/include/rc.h"
#include "gpu/repair/hbm/hbm_wir.h"
#include "gpu/repair/hbm/gpu_interface/gpu_hbm_interface.h"
#include "gpu/repair/mem_repair_config.h"
#include "gpu/repair/repair_types.h"
#include "gpu/repair/repair_util.h"

#include <memory>

//!
//! \file hbm_interface.h
//!
//! \brief HBM interfaces.
//!

//!
//! \def LANE_REPAIR_FIRST_RC_SKIP(f)
//! \brief Updates \a firstRepairRc, skips repair if the RC of 'f' is not OK.
//! Requires 'StickyRC firstRepairRc' to be defined.
//!
//! If 'f's RC is not OK, the lane repair post-amble is called.
//!
//! NOTE: Can't wrap in a do-while(0) due to 'continue'.
//!
#define LANE_REPAIR_FIRST_RC_SKIP(f)                                    \
    {                                                                   \
        RC rc_ = (f);        /* Save RC for cmd in lane repair */       \
        firstRepairRc = rc_; /* Record RC for this lane repair */       \
        if (rc_ != RC::OK)                                              \
        {                                                               \
            PostSingleLaneRepair(settings, repairType, lane, hbmLane, remap, \
                                 originalHbmLaneRemapData, hbmLaneRemapData, rc_); \
            continue;                                                   \
        }                                                               \
    }

//!
//! \def ROW_REPAIR_FIRST_RC_SKIP(f)
//! \brief Updates \a firstRepairRc, skips repair if the RC of 'f' is not OK.
//! Requires 'StickyRC firstRepairRc' to be defined.
//!
//! If 'f's RC is not OK, the row repair post-amble is called.
//!
//! NOTE: Can't wrap in a do-while(0) due to 'continue'.
//!
#define ROW_REPAIR_FIRST_RC_SKIP(f)                                     \
    {                                                                   \
        RC rc_ = (f);        /* Save RC for cmd in row repair */        \
        firstRepairRc = rc_; /* Record RC for this row repair */        \
        if (rc_ != RC::OK)                                              \
        {                                                               \
            PostSingleRowRepair(settings, repairType, rowErr, hbmSite, hbmChannel, rc_); \
            continue;                                                   \
        }                                                               \
        rc_.Clear();                                                    \
    }

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief Describes a lane within a 32-bit DWORD.
    //!
    struct DwordLane
    {
        UINT32 bit;      //!< Lane bit
        Lane::Type type; //!< Lane type

        DwordLane(UINT32 laneBit, Lane::Type laneType)
            : bit(laneBit), type(laneType) {}
    };

    //!
    //! \brief Lane remap data.
    //!
    //! NOTE: Remember that each nibble in the remap value represents the
    //! remapping for a byte worth of DQ lanes and the required supporting
    //! lanes (ie. DBI, ECC, ...).
    //!
    // TODO(stewarts): Format is HBM2 specific. Add additional layer of
    // abstraction when required.
    struct LaneRemap
    {
        //!
        //! \brief Sentinal values for byte encoding.
        //!
        enum ByteEncoding : UINT08
        {
            //! Remap in other byte in pair. Also referred to as
            //! "repair in other".
            BYTE_REMAP_IN_OTHER = 0x0e,

            //! No remap in byte.
            BYTE_NO_REMAP       = 0x0f
        };

        //! Byte remap encoding bit width.
        static constexpr UINT32 byteEncodingBitWidth = 4;
        static_assert(sizeof(ByteEncoding) >= byteEncodingBitWidth / CHAR_BIT,
                      "ByteEncoding minimum bit width not met");

        //!
        //! \brief Sentinal values for byte pair encoding.
        //!
        enum BytePairEncoding : UINT08
        {
            BYTE_PAIR_NO_REMAP = 0xff //!< No remap in byte pair.
        };

        //! Byte pair encoding bit width.
        static constexpr UINT32 bytePairEncodingBitWidth = byteEncodingBitWidth * 2;
        static_assert(sizeof(BytePairEncoding) >= byteEncodingBitWidth / CHAR_BIT,
                      "BytePairEncoding minimum bit width not met");

        //! Value corresponding to a single remapped lane.
        UINT16 value = 0;

        //! Mask depicting which bits of 'value' are relevant to the remapped
        //! lane.
        UINT16 mask = 0;

        LaneRemap() = default;

        //!
        //! \brief Constructor.
        //!
        //! \param value Remap value.
        //! \param mask Mask of the portion of \a value that is relevant.
        //!
        LaneRemap(UINT16 value, UINT16 mask) : value(value), mask(mask) {}
        string ToString() const;
    };

    //!
    //! \brief Row Error data.
    //!
    struct RowErrorInfo
    {
        Repair::RowError error;    //!< Error Location information
        Hbm::Channel     hbmChan;  //!< Channel having the error
    };

    bool CmpRowErrorInfoByRow(const RowErrorInfo& a, const RowErrorInfo& b);

    //!
    //! \brief Represents the interface abstraction of a particular model of
    //! HBM.
    //!
    //! This class contains commands or actions that the HBM itself supports.
    //! Those commands are sent to the HBM over a GPU-to-HBM interface. The
    //! GPU-to-HBM interface handles the GPU specific way of sending the HBM
    //! commands to the HBM.
    //!
    //! \see GpuHbmInterface
    //!
    class HbmInterface
    {
        using HbmChannel      = Hbm::Channel;
        using RowError        = Repair::RowError;
        using Settings        = Repair::Settings;
        using SystemState     = Repair::SystemState;
        using SiteRowErrorMap = map<Site, vector<Hbm::RowErrorInfo>>;
 
    public:
        //!
        //! \brief Constructor.
        //!
        //! \param hbmModel The HBM model this interface represents.
        //! \param pGpuHbmInterface The GPU-to-HBM interface that is used to
        //! communicate with the HBM.
        //!
        HbmInterface(const Model& hbmModel, std::unique_ptr<GpuHbmInterface> pGpuHbmInterface);

        virtual ~HbmInterface() {}

        HbmInterface(HbmInterface&& o);
        HbmInterface& operator=(HbmInterface&& o);

        //!
        //! \brief Setup.
        //!
        //! NOTE: Must be called by all subclasses.
        //!
        virtual RC Setup();

        //!
        //! \brief Return true if the HBM interface is setup.
        //!
        bool IsSetup() const { return m_IsSetup; }

        //!
        //! \brief Perform lane repair on the given lanes.
        //!
        //! \param settings Repair settings.
        //! \param lanes FBPA lane to repair.
        //! \param repairType Type of repair to perform.
        //!
        virtual RC LaneRepair
        (
            const Settings& settings,
            const vector<FbpaLane>& lanes,
            RepairType repairType
        ) const = 0;

        //!
        //! \brief Perform row repair on the given row.
        //!
        //! \param settings Repair settings.
        //! \param rowErrors Rows to repair.
        //! \param repairType Type of repair to perform.
        //!
        RC RowRepair
        (
            const Settings& settings,
            const vector<RowError>& rowErrors,
            RepairType repairType
        ) const;

        //!
        //! \brief Perform row repair on the given row.
        //!
        //! \param settings Repair settings.
        //! \param rowErrors Rows to repair mapped by site
        //! \param repairType Type of repair to perform.
        //!
        virtual RC RowRepair
        (
            const Settings& settings,
            const SiteRowErrorMap& rowErrors,
            RepairType repairType
        ) const = 0;

        //!
        //! \brief Perform MBIST operation for all HBM Sites.(Vendor specific implementations)
        //!
        virtual RC RunMBist() const;

        //!
        //! \brief Print the HBM device information.
        //!
        RC PrintHbmDeviceInfo() const;

        //!
        //! \brief Print repaired lanes.
        //!
        //! Checking the GPU for lane remapping will only represent the repaired
        //! HBM lanes if:
        //! - The lanes were remapped from the HBM fuses at Init-From-ROM (IFR)
        //! - There was no other modifications performed to the GPU lane
        //!   remapping during this run.
        //!
        //! May not show soft repairs depending on:
        //! - If checking the GPU lane remapping:
        //!   - If lane remapping has been performed at IFR or during the run.
        //! - If checking the HBM:
        //!   - How the HBM model implements soft repairs.
        //!
        //! \param readHbmLanes If true, check the HBM for repaired lanes.
        //! Otherwise check the GPU lane remapping.
        //!
        RC PrintRepairedLanes(bool readHbmLanes) const;

        //!
        //! \brief Print all repaired rows on the HBM.
        //!
        //! May show soft row repairs depending on the HBM model's
        //! implementation.
        //!
        virtual RC PrintRepairedRows() const = 0;

        //!
        //! \brief Callwlate the remap value for the given lane.
        //!
        //! \param lane Lane to generate the remap value for.
        //! \param pRemap Lane remapping value.
        //!
        virtual RC CalcLaneRemapValue(const HbmLane& lane, LaneRemap* pRemap) const = 0;

        //!
        //! \brief Get the device ID data for the given site.
        //!
        //! \param hbmSite HBM site.
        //! \param[out] pDevIdData Device ID data.
        //!
        virtual RC GetHbmSiteDeviceInfo(Site hbmSite, HbmDeviceIdData* pDevIdData) const = 0;

        //!
        //! \brief Collect the repaired lanes across all HBM sites, sourced from
        //! the GPU.
        //!
        //! \param[out] pLanes Repaired lanes.
        //!
        virtual RC GetRepairedLanesFromGpu(vector<FbpaLane>* pLanes) const = 0;

        //!
        //! \brief Collect the repaired lanes across all HBM sites, sourced from
        //! the HBM.
        //!
        //! \param[out] pLanes Repaired lanes.
        //!
        virtual RC GetRepairedLanesFromHbm(vector<FbpaLane>* pLanes) const = 0;

        //!
        //! \brief Collect the repaired lanes on the given HBM site, sourced from
        //! the HBM.
        //!
        //! \param hbmSite HBM site.
        //! \param hbmChannel HBM channel.
        //! \param[out] pLanes Repaired lanes.
        //!
        virtual RC GetRepairedLanesFromHbm
        (
            const Site& hbmSite,
            const HbmChannel& hbmChannel,
            vector<FbpaLane>* pLanes
        ) const = 0;

    protected:
        //! Model of HBM represented by this interface.
        Model m_HbmModel;

        //! Collection of WIRs that are supported on this interface. Maps WIR
        //! type to its description.
        WirMap m_WirMap;

        //!
        //! \brief Add supported WIRs from the given collection to the supported
        //! WIRs list for this interface.
        //!
        //! Support is determined based on the HBM model this interface
        //! represents.
        //!
        //! \param wirs Collection of WIRs to, if supported, add to the
        //! supported WIRs.
        //!
        RC AddWirs(const vector<Wir>& wirs);

        //!
        //! \brief Find the WIR description of the given WIR type for this HBM
        //! model.
        //!
        //! Checks the supported WIRs map.
        //!
        //! \see m_WirMap
        //!
        RC LookupWir(Wir::Type wirType, const Wir** ppWir) const;

        //!
        //! \brief Return the GPU-to-HBM interface.
        //!
        GpuHbmInterface* GpuInterface() const { return m_pGpuHbmInterface.get(); }

        //!
        //! \brief Return the HBM device object associated with the physical
        //! HBM.
        //!
        HBMDevice* GetHbmDevice() const { return m_pGpuHbmInterface->GetHbmDevice(); }

        /* Methods to be called before and after their associated repairs */

        //!
        //! \brief Performs actions rquired before the repair of a single lane.
        //! Call prior to each individual lane repair.
        //!
        //! \param hbmSite HBM site being repaired.
        //! \param hbmChannel HBM channel being repaired.
        //! \param lane FBPA lane being repaired.
        //!
        //! \see PostSingleLaneRepair
        //!
        RC PreSingleLaneRepair
        (
            Site hbmSite,
            Channel hbmChannel,
            const FbpaLane& lane
        ) const;

        //!
        //! \brief Performs actions required after the repair of a single lane.
        //! Call after each individual lane repair, regardless of if the repair
        //! succeeds.
        //!
        //! \param settings Repair settings.
        //! \param repairType Type of repair to perform.
        //! \param lane HBM lane that was remapped.
        //! \param remap Lane remapping value.
        //! \param originalHbmLaneRemapData The HBM's full lane remap data
        //! before the repair.
        //! \param hbmLaneRemapData The HBM's full lane remap data after the
        //! repair.
        //! \param repairStatus Status of the repair.
        //!
        //! \see PreSingleLaneRepair
        //!
        RC PostSingleLaneRepair
        (
            const Settings& settings,
            RepairType repairType,
            const FbpaLane& lane,
            const HbmLane& hbmLane,
            const LaneRemap& remap,
            const WdrData& originalHbmLaneRemapData,
            const WdrData& hbmLaneRemapData,
            RC repairStatus
        ) const;

        //!
        //! \brief Performs actions required before the repair of a single row.
        //! Call before each individual row repair.
        //!
        //! \param hbmSite HBM site being repaired.
        //! \param hbmChannel HBM channel being repaired.
        //! \param rowError Row being repaired.
        //!
        //! \see PostSingleRowRepair
        //!
        RC PreSingleRowRepair
        (
            Site hbmSite,
            Channel hbmChannel,
            const RowError& rowError
        ) const;

        //!
        //! \brief Performs actions required after the repair of a single row.
        //! Call before each individual row repair, regardless of if the repair
        //! succeeds.
        //!
        //! \param settings Repair settings.
        //! \param repairType Type of repair to perform.
        //! \param rowError Row that was repaired.
        //! \param hbmSite HBM site that was repaired.
        //! \param hbmChannel HBM channel that was repaired.
        //! \param repairStatus Status of the repair.
        //!
        //! \see PreSingleRowRepair
        //!
        RC PostSingleRowRepair
        (
            const Settings& settings,
            RepairType repairType,
            const RowError& rowError,
            Site hbmSite,
            Channel hbmChannel,
            RC repairStatus
        ) const;

    private:
        //! True if Setup has been called, false otherwise.
        bool m_IsSetup = false;

        //! Interface from the GPU to the HBM.
        std::unique_ptr<GpuHbmInterface> m_pGpuHbmInterface;
    };
} // namespace Hbm
} // namespace Memory
