/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/display.h"
#include "core/include/massert.h"
#include "core/include/modsedid.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0073.h"
#include "gpu/display/tegra_disp.h"
#include "gpu/include/gsyncvcp.h"
#include "gpu/include/tegragsyncdev.h"

//! LWPU EDID CEA Block definitions for determining GSync presence
#define LWIDIA_CEA_VENDOR_LOW 0x4B
#define LWIDIA_CEA_VENDOR_MID 0x04
#define LWIDIA_CEA_VENDOR_HI  0x00
#define LWIDIA_CEA_OPCODE_IDX 0x00

UINT08 TegraGsyncDevice::s_HDMIPowerToggleValue = 0;

//-----------------------------------------------------------------------------
//!
TegraGsyncDevice::TegraGsyncDevice
(
    Display *pDisplay,
    GpuSubdevice *pSubdev,
    UINT32 displayMask,
    GsyncSink::Protocol protocol,
    Display::I2CPortNum i2cPort
)
: GsyncDevice(pDisplay, pSubdev, displayMask, protocol)
, m_I2cPort(i2cPort)
{
}

namespace
{
    //-----------------------------------------------------------------------------
    //! \brief Edid CEA contains LWDA device registration number
    bool IsLwidiaGsyncPanel
    (
        const LWT_EDID_CEA861_INFO& info
    )
    {
        for (UINT32 vsdb_idx = 0;
            vsdb_idx < info.total_vsdb;
            vsdb_idx++)
        {
            if (info.vsdb[vsdb_idx].ieee_id == LWT_CEA861_LWDA_IEEE_ID)
            {
                if (info.vsdb[vsdb_idx].vendor_data[LWIDIA_CEA_OPCODE_IDX] > 0)
                {
                    return true;
                }
            }
        }
        return false;
    }

    //-----------------------------------------------------------------------------
    //! \brief The display dfpga is whitelisted
    bool IsWhitelistedManufacturer
    (
        const EDID_MISC &edidMisc
    )
    {
        //-----------------------------------------------------------------------------
        //! Structure for relating manufacturer and product IDs
        struct MfrProductData
        {
            string manufacturer;
            UINT16 productID;
        };

        //! EDID manufacturer & device ID combinations used as a secondary method of
        //! detecting GSync device presence
        const MfrProductData mfrProductWhitelist[] =
        {
            { "LWD", 0xFFFC },
            { "LWD", 0xFFFE },
            { "LWD", 0xFFF9 },
        };

        const UINT32 whitelistSize = sizeof(mfrProductWhitelist) /
            sizeof(mfrProductWhitelist[0]);
        for (UINT32 mfrProdIdx = 0; mfrProdIdx < whitelistSize; mfrProdIdx++)
        {
            if (edidMisc.manufacturer
                    == mfrProductWhitelist[mfrProdIdx].manufacturer &&
                    edidMisc.productID
                    == mfrProductWhitelist[mfrProdIdx].productID)
            {
                return true;
            }
        }
        return false;
    }
}

//-----------------------------------------------------------------------------
//! \brief Static function to find all gsync connections of a display
//!
//! \param pDisplay      : Display to check for GSync connections
//! \param pSubdev       : Subdevice pointer being used
//! \param pGsyncDevices : Pointer to returned vector of Gsync devices
//!
//! \return Maximum number frames for CRC aclwmulation
/* static */ RC TegraGsyncDevice::FindTegraGsyncDevices
(
    TegraDisplay *pDisplay,
    GpuSubdevice *pSubdev,
    vector<TegraGsyncDevice> *pGsyncDevices
)
{
    RC rc;
    Display::Encoder enc;
    MASSERT(pGsyncDevices);

    UINT32 connectors = 0;
    CHECK_RC(pDisplay->GetDetected(&connectors));

    for (INT32 connector = Utility::BitScanForward(connectors);
         connector >= 0;
         connector = Utility::BitScanForward(connectors, connector + 1))
    {
        UINT32 connectorToCheck = 1 << connector;

        TegraDisplay::ConnectionType conType;
        CHECK_RC(pDisplay->Select(connectorToCheck));
        CHECK_RC(pDisplay->GetConnectionType(connectorToCheck, &conType));
        if (conType != TegraDisplay::CT_EDP && conType != TegraDisplay::CT_HDMI)
        {
            continue;
        }
        LWT_EDID_CEA861_INFO cea861Info;
        CHECK_RC(pDisplay->GetEdidMember()->GetCea861Info(connectorToCheck,
                                                          &cea861Info));

        bool bFoundGSync = IsLwidiaGsyncPanel(cea861Info);
        if (!bFoundGSync)
        {
            // If Gsync presence is not found in the LWPU header, also check
            // against whitelisted Manufacturer & Product ID combinations
            EDID_MISC edidMisc;
            CHECK_RC(pDisplay->GetEdidMember()->GetEdidMisc(connectorToCheck,
                                                            &edidMisc));
            bFoundGSync = IsWhitelistedManufacturer(edidMisc);
        }

        if (bFoundGSync)
        {
            GsyncSink::Protocol protocol = GsyncSink::Protocol::Unsupported;

            if (conType == TegraDisplay::CT_EDP)
                protocol = GsyncSink::Protocol::Dp_A;

            if (conType == TegraDisplay::CT_HDMI)
            {
                protocol = GsyncSink::Protocol::Tmds_A;
                // By default the HDMI link will be powergated after tinylinux bootup
                // since we don't send out any images actively.
                // For I2C communications to work before the first modeset
                // We need to powerToggleHDMI nodes to 1 to use I2C over Aux in HDMI
                // In DP we use dp sysfs nodes for i2c communications
                // which internally power gates the link.
                // In HDMI we use kernel i2c, so we need to powergate
                CHECK_RC(TegraGsyncDevice::PowerToggleHDMI(pDisplay, 1));
                CHECK_RC(pDisplay->GetEncoder(connectorToCheck, &enc));
            }

            TegraGsyncDevice lwrDev((Display*)pDisplay, pSubdev, connectorToCheck, protocol,
                    enc.I2CPort);
            CHECK_RC(lwrDev.Initialize());
            pGsyncDevices->push_back(lwrDev);
        }
    }
    return rc;
}

/* static */ RC TegraGsyncDevice::PowerToggleHDMI
(
    TegraDisplay *pDisplay,
    UINT32 value
)
{
    RC rc;

    if (s_HDMIPowerToggleValue != value)
    {
        CHECK_RC(pDisplay->SetHdmiPowerToggle(value));
        s_HDMIPowerToggleValue = value;
    }
    return rc;
}

/*virtual*/ RC TegraGsyncDevice::GetDpSymbolErrors
(
    UINT32 *pTotalErrors,
    vector<UINT16> *pPerLaneErrors
)
{
    RC rc;

    switch (GetSORProtocol())
    {
        case GsyncSink::Protocol::Dp_A:
        case GsyncSink::Protocol::Dp_B:
        {
            GsyncDevice::GetDpSymbolErrors(pTotalErrors, pPerLaneErrors);
        }
        break;
        case GsyncSink::Protocol::Tmds_A:
        case GsyncSink::Protocol::Tmds_B:
        case GsyncSink::Protocol::Tmds_Dual:
        {
            if (pTotalErrors)
                *pTotalErrors = 0;
            return rc;
        }
        break;
        default:
        {
            Printf(Tee::PriError, "Unknown SorProtocol!");
            return RC::SOFTWARE_ERROR;
        }
    }
    return rc;
}

/*virtual*/ RC TegraGsyncDevice::AccessGsyncSink
(
    GsyncSink::TMDS           type,
    Display::DpAuxTransaction command,
    UINT08 * const            pBuffer,
    const UINT32              numElems,
    Display::DpAuxRetry       enableRetry //Don't care
)
{
    RC rc;
    CHECK_RC(const_cast<SysUtil::I2c&>(GetI2cDevice()).Open(m_I2cPort));

    DEFER
    {
        const_cast<SysUtil::I2c&>(GetI2cDevice()).Close();
    };

    if (command == Display::DP_AUX_CHANNEL_COMMAND_WRITE)
    {
        CHECK_RC(const_cast<SysUtil::I2c&>(GetI2cDevice()).Write(
                I2C_SLAVE_ADDR,
                0,
                pBuffer,
                0,
                numElems));
    }
    else if (command == Display::DP_AUX_CHANNEL_COMMAND_READ)
    {
        CHECK_RC(const_cast<SysUtil::I2c&>(GetI2cDevice()).Read(
                I2C_SLAVE_ADDR,
                0,
                pBuffer,
                0,
                numElems));
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the CRC signature (called from Golden code)
//!
//! \param pCrcSignature : Pointer to returned CRC signature
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC TegraGsyncDevice::GetCrcSignature(string *pCrcSignature)
{
    RC rc;

    if (pCrcSignature == nullptr)
    {
        MASSERT(pCrcSignature);
        return RC::SOFTWARE_ERROR;
    }

    TegraDisplay *pDisplay = (TegraDisplay*)GetPDisplay();
    string orStr;
    UINT32 width, height;

    switch (GetSORProtocol())
    {
        case GsyncSink::Protocol::Dp_A:
        case GsyncSink::Protocol::Dp_B:
            orStr = "EDP";
            break;
        case GsyncSink::Protocol::Tmds_A:
        case GsyncSink::Protocol::Tmds_B:
        case GsyncSink::Protocol::Tmds_Dual:
            orStr = "HDMI";
            break;
        default:
            MASSERT(!"Unknown SorProtocol!");
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(pDisplay->GetMode(GetDisplayMask(), &width, &height));
    *pCrcSignature = Utility::StrPrintf("_DD_%ux%u_%s_GSYNC",
                                  width,
                                  height,
                                  orStr.c_str());
    return rc;
}
