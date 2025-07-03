/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_HBM_DEV_ID_H
#define INCLUDED_HBM_DEV_ID_H

#ifdef SIM_BUILD
#define USE_FAKE_HBM_FUSE_DATA
#endif

#include "core/include/rc.h"
#include "core/include/hbm_model.h"

class HbmDeviceIdData
{
public:
    enum class Manufacturer : UINT08
    {
        Unknown    = 0,
        Samsung    = 1,
        SkHynix    = 6,
        SkHynixOld = 12,        // GP100 HBM
        Micron     = 15
    };

    HbmDeviceIdData() = default;
    ~HbmDeviceIdData() = default;

    static RC GetHbmModel
    (
        Manufacturer manufacturerId,
        UINT08 modelPartNum,
        UINT08 stackHeight,
        Memory::Hbm::Model* pHbmModel
    );
    static string ColwertModelPartNumberToRevisionStr
    (
        Manufacturer manufacturerId
        ,UINT08 modelPartNum
        ,UINT08 stackHeight
    );
    static string ColwertManufacturerIdToVendorName(Manufacturer manufacturerId);

    void Initialize(const vector<UINT32> &regValues);

    bool IsInitialized() const;
    RC   DumpDevIdData() const;

    // Accessors that require some processing
    UINT08 GetStackHeight() const;
    string GetVendorName() const;
    string GetChannelsAvail() const;
    string GetAddressMode() const;

    //!
    //! \brief Get the memory density per channel in gigabits.
    //!
    UINT16 GetDensityPerChanGb() const;

    UINT16 GetManufacturingYear() const;
    string GetRevisionStr() const;
    bool   IsDataValid() const;
    bool   GetPseudoChannelSupport() const;
    RC     GetHbmModel(Memory::Hbm::Model* pHbmModel) const;

    // Simple accessors
    UINT64 GetSerialNumber() const      { return m_SerialNumber; }
    Manufacturer GetVendorId() const    { return m_ManufacturerId; }
    UINT08 GetModelPartNumber() const   { return m_ModelPartNumber; }
    UINT08 GetManufacturingWeek() const { return m_ManufacturingWeek; }
    UINT08 GetManufacturingLoc() const  { return m_ManufacturingLoc; }
    bool   GetEccSupported() const      { return m_EccSupported; }
    bool   GetGen2Supported() const     { return m_Gen2Supported; }
    UINT08 GetChannelsAvailVal() const  { return m_ChannelsAvailable; }
    UINT08 GetAddressModeVal() const { return m_AddressingMode; }
    bool IsSameHbmModel(const HbmDeviceIdData& o) const;

private:
    //!
    //! \brief HBM DEVICE_ID per channel memory density values (in gigabits).
    //!
    enum class Hbm2ChannelMemoryDensity : UINT08
    {
        _1Gb    = 1, //!< 1 Gb
        _2Gb    = 2, //!< 2 Gb
        _4Gb    = 3, //!< 4 Gb
        _8Gb    = 4, //!< 8 Gb
        _16Gb   = 5, //!< 16 Gb
        _32Gb   = 6  //!< 32 Gb
    };
    UINT16 GetHbm2DensityPerChanGb() const;

    //!
    //! \brief HBM DEVICE_ID per channel memory density values (in gigabits) for HBM 2E
    //!
    enum class Hbm2eChannelMemoryDensity : UINT08
    {
        _1Gb       = 0b0001, //!< 1 Gb
        _2Gb       = 0b0010, //!< 2 Gb
        _4Gb       = 0b0011, //!< 4 Gb
        _8Gb_8Hi   = 0b0100, //!< 8 Gb (8 Gb 8-High)
        _6Gb       = 0b0101, //!< 6 Gb
        _8Gb       = 0b0110, //!< 8 Gb
        _12Gb_8Hi  = 0b1000, //!< 12 Gb (12 Gb 8-High)
        _8Gb_12Hi  = 0b1001, //!< 12 Gb (8 Gb 12-High)
        _16Gb_8Hi  = 0b1010, //!< 16 Gb (16 Gb 8-High)
        _12Gb_12Hi = 0b1011, //!< 18 Gb (12 Gb 12-High)
        _16Gb_12Hi = 0b1100  //!< 24 Gb (16 Gb 12-High)
    };
    UINT16 GetHbm2eDensityPerChanGb() const;

    void InitFakeData();

    bool    m_Gen2Supported     = false;
    bool    m_EccSupported      = false;
    Manufacturer m_ManufacturerId = Manufacturer::Unknown;
    UINT08  m_ManufacturingLoc  = 0;
    UINT08  m_ManufacturingYear = 0;
    UINT08  m_ManufacturingWeek = 0;
    UINT64  m_SerialNumber      = 0;
    UINT08  m_AddressingMode    = 0;
    UINT08  m_ChannelsAvailable = 0;
    UINT08  m_StackHeight       = 0;
    UINT08  m_ModelPartNumber   = 0;

protected:
    UINT08  m_DensityPerChan    = 0;
};

#endif // INCLUDED_HBM_DEV_ID_H
