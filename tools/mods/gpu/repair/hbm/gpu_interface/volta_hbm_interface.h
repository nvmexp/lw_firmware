/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/repair/hbm/gpu_interface/gpu_hbm_interface.h"
#include "gpu/repair/hbm/hbm_interface/hbm_interface.h"

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief The Volta interface to HBM using the FB PRIV path (IEEE1500
    //! bridge).
    //!
    class VoltaHbmInterface : public GpuHbmInterface
    {
    public:
        VoltaHbmInterface(GpuSubdevice *pSubdev) : GpuHbmInterface(pSubdev) {}

        RC WirRead
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            WdrData* pData
        ) const override;

        RC WirWrite
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            const WdrData& data
        ) const override;

        RC WirWriteRaw
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel
        ) const override;

        RC WirWriteRaw
        (
            const Site& hbmSite,
            const UINT32 data
        ) const override;

        RC ModeWriteRaw
        (
            const Wir& wir,
            const Site& hbmSite
        ) const override;

        RC ModeWriteRaw
        (
            const Site& hbmSite,
            const UINT32 data
        ) const override;

        RC WdrWrite
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            const WdrData& data
        ) const override;

        RC WdrReadRaw
        (
            const Wir& wir,
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            WdrData* pData
        ) const override;

        RC WdrReadRaw
        (
            const Site& hbmSite,
            UINT32* pData
        ) const override;

        RC WdrReadCompareRaw
        (
            const Site& hbmSite,
            UINT32 data
        ) const override;

        RC WdrWriteRaw
        (
            const Site& hbmSite,
            Wir::ChannelSelect chanSel,
            const WdrData& data
        ) const override;

        RC WdrWriteRaw
        (
            const Site& hbmSite,
            const UINT32 data
        ) const override;

        RC ResetSite(const Site& hbmSite) const override;

        RC ToggleWrstN(const Site& hbmSite) const override;

        FbioSubp FbpaSubpToFbioSubp(const FbpaSubp& fbpaSubp) const override;
        FbpaSubp FbioSubpToFbpaSubp(const FbioSubp& fbioSubp) const override;

        RC GetHbmLane(const FbpaLane& fbpaLane, HbmLane* pHbmLane) const override;
        RC GetFbpaLane(const HbmLane& hbmLane, FbpaLane* pFbpaLane) const override;

        RC GetHbmDwordAndByte
        (
            const HbmLane& lane,
            UINT32* pDword,
            UINT32* pByte
        ) const override;

        RC GetFbpaDwordAndByte
        (
            const FbpaLane& lane,
            UINT32* pFbpaDword,
            UINT32* pFbpaByte
        ) const override;

        RC Ieee1500WaitForIdle
        (
            Site hbmSite,
            WaitMethod waitMethod,
            FLOAT64 timeoutMs
        ) const override;

        RC GetLinkRepairRegRemapValue
        (
            const HwFbpa& hwFbpa,
            const FbpaSubp& fbpaSubp,
            UINT32 fbpaDword,
            UINT16* pRemapValue,
            bool* pIsBypassSet
        ) const override;

    private:
        //!
        //! \brief Normalizes WDR data.
        //!
        //! Values are placed into the WDR register by shifting from the most
        //! significant bit (MSB). If the value partially fills a register,
        //! then it will not be in the correct bit position. This call
        //! normalizes it.
        //!
        //! For example, a 63 bit response from the WDR register:
        //! - Read in 2 32-bit chunks.
        //! - First chunk will be a full 32 bits.
        //! - Second chunk will be the remaining 31 bits, partially filling the
        //!   32-bit register. The value is in the position val << 1.
        //! - To get the actual value of the second chunk, it needs to be
        //!   shifted down to bit 0 of the register (val >> 1).
        //!
        //! \param wir WIR that produced the WDR data.
        //! \param[in,out] pWdrData Data to normalize read from the WDR.
        //!
        static void WdrReadNormalization
        (
            const Wir& wir,
            WdrData* pWdrData
        );

        //!
        //! \brief Check if the given WIR can write WDR data.
        //!
        //! \param wir WIR.
        //!
        RC CheckWirWriteCapable(const Wir& wir) const;

        //!
        //! \brief Check if the given WIR can read WDR data.
        //!
        //! \param wir WIR.
        //!
        RC CheckWirReadCapable(const Wir& wir) const;

        //!
        //! \brief Check if the length of the given WDR is appropriate for the
        //! given WIR.
        //!
        //! \param wir WIR.
        //! \param data WDR data.
        //! \param allowEmpty True if the WDR is allowed to be empty, false o/w.
        //!
        RC CheckWdrLength
        (
            const Wir& wir,
            const WdrData& data,
            bool allowEmpty = false
        ) const;

        //!
        //! \brief Clear the PRIV error register.
        //!
        //! \param pRegs RegHal instance for GPU.
        //!
        virtual RC ClearPrivError(RegHal* pRegs) const;

        //!
        //! \brief Check the PRIV error register.
        //!
        //! \param pRegs RegHal instance for GPU.
        //! \param pUseFriendlyRegisterName Name of the register being checked. Will be
        //! printed in case of error.
        //!
        //! \return RC::OK when there is no error, non-OK status otherwise.
        //!
        RC CheckPrivViolation
        (
            RegHal* pRegs,
            const char* pUserFriendlyRegisterName
        ) const;

        //!
        //! \brief Return the value of the IEEE1500 instruction mode field set to normal
        //! mode.
        //!
        //! \param hbmSite HBM site.
        //! \param[out] pVal Normal mode value.
        //!
        RC GetIeee1500InstrNormalModeVal(const Site& hbmSite, UINT32* pVal) const;

        //!
        //! \brief Return the DWORD and byte number corresponding to the given
        //! lane. No swizzling is applied.
        //!
        //! \param lane Lane.
        //! \param[out] pDword DWORD (0-indexed)
        //! \param[out] pByte Byte within DWORD (0-indexed)
        //!
        RC GetDwordAndByte(const Lane& lane, UINT32* pDword, UINT32* pByte) const;
    };
} // namespace Memory
} // namespace Hbm
