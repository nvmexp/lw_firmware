/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/rc.h"
#include "core/include/massert.h"
#include "core/include/tasker.h"
#include "core/include/display.h"
#include "core/include/modsedid.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0073.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gsyncdev.h"
#include "gpu/include/gsyncvcp.h"

//! LWPU EDID CEA Block definitions for determining GSync presence
#define LWIDIA_CEA_VENDOR_LOW 0x4B
#define LWIDIA_CEA_VENDOR_MID 0x04
#define LWIDIA_CEA_VENDOR_HI  0x00
#define LWIDIA_CEA_OPCODE_IDX 0x00

//-----------------------------------------------------------------------------
//!
GsyncSink::Protocol GsyncSink::GetProtocol(Display *pDisplay, UINT32 dispMask)
{
    UINT32 protocol;
    RC rc = pDisplay->GetORProtocol(dispMask, nullptr, nullptr,
                &protocol);
    if (rc != OK || !s_ProtocolGsyncCap.at(protocol))
    {
        return GsyncSink :: Protocol :: Unsupported;
    }
    else
    {
        return static_cast <GsyncSink :: Protocol> (protocol);
    }
}

//-----------------------------------------------------------------------------
//!
GsyncDevice::GsyncDevice
(
    Display *pDisplay,
    GpuSubdevice *pSubdev,
    UINT32 displayMask
) : m_pDisplay(pDisplay)
    , m_pSubdev(pSubdev)
    , m_DisplayMask(displayMask)
    , m_NumCrcFrames(1)
    , m_CrcIdlePollHz(20)
    , m_CrcIdleTimeoutMs(5000)
    , m_DebugLevel(DBG_NONE)
    , m_DpAuxRetries(3)
    , m_DpAuxRetryDelayMs(10)
    , m_VCPCmdRetries(16)
    , m_ImageChangeSettleFrames(2)
    , m_ModesetSettleFrames(10)
    , m_IsR3Supported(false)
    , m_SorProtocol(GsyncSink::GetProtocol(pDisplay, displayMask))
{
}

GsyncDevice::GsyncDevice
(
    Display *pDisplay,
    GpuSubdevice *pSubdev,
    UINT32 displayMask,
    GsyncSink::Protocol protocol
) : m_pDisplay(pDisplay)
    , m_pSubdev(pSubdev)
    , m_DisplayMask(displayMask)
    , m_NumCrcFrames(1)
    , m_CrcIdlePollHz(20)
    , m_CrcIdleTimeoutMs(5000)
    , m_DebugLevel(DBG_NONE)
    , m_DpAuxRetries(3)
    , m_DpAuxRetryDelayMs(10)
    , m_VCPCmdRetries(16)
    , m_ImageChangeSettleFrames(2)
    , m_ModesetSettleFrames(10)
    , m_IsR3Supported(false)
    , m_SorProtocol(protocol)
{
}

//-----------------------------------------------------------------------------
//! \brief Initializes Gsync device
RC GsyncDevice::Initialize()
{
    RC rc;
    UINT16 boardId;

    if (m_SorProtocol == GsyncSink::Protocol::Unsupported)
    {
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(GetBoardId(&boardId));
    const UINT16 boardType = boardId & 0xfff;

    // Valid range for board types
    MASSERT(((boardType > 0) && (boardType < 0x400)));
    // Gsync R3 devices are expected to have boardType > 0x1ff
    if (boardType > 0x1ff)
    {
        m_IsR3Supported = true;
    }
    Printf(Tee::PriLow,"Gsync device is %s compatible \n",(m_IsR3Supported? "R3" : "R2"));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the number of CRC bins (called from Golden code when Gsync is
//!        used as an external CRC device)
//!
//! \return Number of bins for CRCs when used as an external CRC device
/* virtual */ UINT32 GsyncDevice::NumCrcBins() const
{
    return GetCrcsPerFrame(CRC_PRE_AND_POST_MEM) * m_NumCrcFrames;
}

//-----------------------------------------------------------------------------
//! \brief Get the CRC signature (called from Golden code when Gsync is used as
//!        an external CRC device)
//!
//! \param pCrcSignature : Pointer to returned CRC signature
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC GsyncDevice::GetCrcSignature(string *pCrcSignature)
{
    RC rc;
    CHECK_RC(m_pDisplay->GetCrcSignature(m_DisplayMask,
                                         Display::DAC_CRC,
                                         pCrcSignature));
    pCrcSignature->append("_GSYNC");
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the CRC values (called from Golden code when Gsync is used as an
//!        external CRC device)
//!
//! \param pCrcs : Pointer to returned CRC buffer
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC GsyncDevice::GetCrcValues(UINT32 *pCrcs)
{
    RC rc;
    vector<CrcData> suCrcs;
    vector<CrcData> ppuCrcs;

    CHECK_RC(GetCrcs(true, m_NumCrcFrames, &suCrcs, &ppuCrcs));

    for (UINT32 frameIdx = 0; frameIdx < m_NumCrcFrames; frameIdx++)
    {
        for (size_t crcIdx = 0; crcIdx < suCrcs[frameIdx].size(); crcIdx++, pCrcs++)
            *pCrcs = suCrcs[frameIdx][crcIdx];
        for (size_t crcIdx = 0; crcIdx < ppuCrcs[frameIdx].size(); crcIdx++, pCrcs++)
            *pCrcs = ppuCrcs[frameIdx][crcIdx];
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Report detailed CRC miscompare information (called from Golden code
//!        when Gsync is used as an external CRC device)
//!
//! \param bin         : Miscomparing CRC bin
//! \param expectedCRC : Expected CRC
//! \param actualCRC   : Actual CRC
//!
//! \return OK if successful, not OK otherwise
/* virtual */ void GsyncDevice::ReportCrcMiscompare
(
    UINT32 bin,
    UINT32 expectedCRC,
    UINT32 actualCRC
)
{
    const UINT32 crcsPerFrame = GetCrcsPerFrame(CRC_PRE_AND_POST_MEM);
    const UINT32 frameIdx = bin / crcsPerFrame;
    const UINT32 crcIdx = (bin % crcsPerFrame);
    const UINT32 preMemCRCsPerFrame = GetPreMemCrcsPerFrame();
    const bool bIsPreMemCRC = crcIdx < preMemCRCsPerFrame;
    const UINT32 lane = bIsPreMemCRC ? crcIdx : crcIdx - preMemCRCsPerFrame;

    Printf(Tee::PriError,
           "Gsync %s CRC Frame %u, Lane %u: 0x%x != measured 0x%x\n",
           bIsPreMemCRC ? "Pre-Memory" : "Post-Memory",
           frameIdx,
           lane,
           expectedCRC,
           actualCRC);
}

//-----------------------------------------------------------------------------
//! \brief Set the number of CRC frames to use (when used as an external CRC
//!        device)
//!
//! \param numCrcFrames : Number of CRC frames to use
//!
//! \return OK if successful, not OK otherwise
void GsyncDevice::SetNumCrcFrames(UINT32 numCrcFrames)
{
    if (numCrcFrames > GetMaxCrcFrames(CRC_PRE_AND_POST_MEM))
    {
        Printf(Tee::PriError,
              "%s : numCrcFrames (%u) too large using maximum (%u) instead\n",
              __FUNCTION__, numCrcFrames, GetMaxCrcFrames(CRC_PRE_AND_POST_MEM));
        m_NumCrcFrames = GetMaxCrcFrames(CRC_PRE_AND_POST_MEM);
    }
    else
    {
        m_NumCrcFrames = numCrcFrames;
    }
}

//-----------------------------------------------------------------------------
//! \brief Perform a factory reset of the Gsync device
//!
//! \return OK if successful, not OK otherwise
RC GsyncDevice::FactoryReset()
{
    RC rc;
    VCPFeatureData factoryReset = { 0, 0, 0, 1 };
    CHECK_RC(SetVCPFeature(FACTORY_RESET_OPCODE, factoryReset));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the temperature from the Gsync temperature sensor
//!
//! \param pTempC : Pointer to returned temperature
//!
//! \return OK if successful, not OK otherwise
RC GsyncDevice::GetTemperature(UINT32 *pTempC)
{
    MASSERT(pTempC);

    RC rc;
    VCPFeatureData siliconTemp;
    CHECK_RC(GetVCPFeature(GSYNC_DIE_TEMP_OPCODE, &siliconTemp, Tee::PriNormal));
    *pTempC = siliconTemp.sl;
    return rc;
}

RC GsyncDevice::GetBoardId(UINT16 *pBoardId)
{
    RC rc;
    VCPFeatureData gsyncBoardId;

    MASSERT(pBoardId);
    CHECK_RC(GetVCPFeature(GSYNC_BOARD_ID, &gsyncBoardId, Tee::PriNormal));
    *pBoardId = (gsyncBoardId.sh << 8) | gsyncBoardId.sl;
    return rc;
}

RC GsyncDevice::GetRefreshRate(UINT32* pRefRate)
{
    MASSERT(pRefRate);
    RC rc;

    bool bNormalEnabled = false;
    CHECK_RC(NormalModeEnabled(&bNormalEnabled));
    if (!bNormalEnabled)
    {
        CHECK_RC(EnableNormalMode());
    }

    VCPFeatureData refRate = {0, 0, 0, 0};
    CHECK_RC(GetVCPFeature(MONITOR_MODE_OPCODE, &refRate, Tee::PriNormal));
    *pRefRate = refRate.sl | (refRate.sh << 8);

    return rc;
}

// GetCRCs return pre-memory (CRCs generated from data before it is written to Gsync memory)
// and post-memory (CRCs generated from data as soon as it is read from Gsync memory) CRCs.
// On pre-R3 Gsync devices, these CRCs were generated from SU/PPU blocks. In Gsync R3, they
// are generated from PACKER and UNPACKER blocks.Refer lwbug 1847181 for documentation.

//-----------------------------------------------------------------------------
//! \brief Get Gsync CRCs
//!
//! \param bResetCrcs   : True to reset CRC memory before aclwmulating
//! \param numFrames    : Number of frames of CRC data to accumulate
//! \param pPreMemCrcs  : pointer to storage for per-frame pre-mem CRCs
//! \param pPostMemCrcs : pointer to storage for per-frame post-mem CRCs
//!
//! \return OK if reading CRCs was successful, not OK otherwise
RC GsyncDevice::GetCrcs
(
    bool             bResetCrcs,
    UINT32           numFrames,
    vector<CrcData> *pPreMemCrcs,
    vector<CrcData> *pPostMemCrcs
)
{
    RC rc;
    VCPFeatureData vcpData = { 0 };
    Tee::Priority vcpPri = (m_DebugLevel > DBG_NONE) ? Tee::PriNormal :
                                                       Tee::PriSecret;

    bool bCrcsEnabled = false;
    CHECK_RC(CrcModeEnabled(&bCrcsEnabled));
    if (!bCrcsEnabled)
    {
        CHECK_RC(EnableCrcMode());
    }

    CrcIdlePollArgs crcIdleArgs;
    crcIdleArgs.pDev   = this;
    crcIdleArgs.sleepMs = m_CrcIdlePollHz ? (1000 / m_CrcIdlePollHz) : 0;
    crcIdleArgs.retries = 0;
    crcIdleArgs.pri     = vcpPri;

    if (bResetCrcs)
    {
        // Reset the entire CRC buffer to 0 and wait for the operation to
        // complete
        vcpData.sl = GSYNC_CRC_STAT_SL_WR_RESET;
        CHECK_RC(SetVCPFeature(GSYNC_CRC_STAT_OPCODE, vcpData));

        Printf(vcpPri, "Display 0x%08x : Resetting GSync CRCs !\n",
               m_DisplayMask);

        CHECK_RC(POLLWRAP_HW(PollCrcIdle, &crcIdleArgs,
                             m_CrcIdleTimeoutMs));
        CHECK_RC(crcIdleArgs.pollRc);
    }

    Printf(vcpPri, "Display 0x%08x : GSync CRCs reset!\n", m_DisplayMask);

    // Setup and start CRC acquisition, capture an additional frame of CRCs
    // since SU/PPU CRCs are delayed by a frame. This statement is false for
    // R3 module.
    const UINT32 captureFrames = m_IsR3Supported ? numFrames : (numFrames + 1);
    vcpData.sh = (captureFrames >> 8) & 0xFF;
    vcpData.sl = captureFrames & 0xFF;
    CHECK_RC(SetVCPFeature(GSYNC_CRC_FRAMES_OPCODE, vcpData));

    const CrcMode crcMode = CRC_PRE_AND_POST_MEM;
    vcpData.sh = 0;
    vcpData.sl = (m_IsR3Supported) ? GSYNC_CRC_PACKER_UNPACKER_CRC:
                                     GSYNC_CRC_MODE_SU_PPU;

    CHECK_RC(SetVCPFeature(GSYNC_CRC_MODE_OPCODE, vcpData));

    // Read and discard any DP symbol errors
    CHECK_RC(GetDpSymbolErrors(nullptr, nullptr));

    vcpData.sl = GSYNC_CRC_STAT_SL_WR_START;
    CHECK_RC(SetVCPFeature(GSYNC_CRC_STAT_OPCODE, vcpData));

    Printf(vcpPri, "Display 0x%08x : Acquiring GSync CRCs !\n", m_DisplayMask);

    // Wait for CRC acquisition to complete and retrieve CRCs
    crcIdleArgs.retries = 0;
    CHECK_RC(POLLWRAP_HW(PollCrcIdle, &crcIdleArgs, m_CrcIdleTimeoutMs));
    CHECK_RC(crcIdleArgs.pollRc);

    Printf(vcpPri, "Display 0x%08x : GSync CRCs acquired!\n", m_DisplayMask);

    UINT32 dpSymbolErrors = 0;
    if ((OK == GetDpSymbolErrors(&dpSymbolErrors, nullptr)) &&
        (dpSymbolErrors > 0))
    {
        Printf(Tee::PriError,
               "Display 0x%08x found %u DP symbol errors when "
               "acquiring GSync CRCs!\n",
               m_DisplayMask, dpSymbolErrors);
    }

    vector<UINT08> crcData;
    size_t prevSize = 0;
    const UINT32 crcsPerFrame  = GetCrcsPerFrame(crcMode);
    const size_t expectedBytes = captureFrames * GSYNC_BYTES_PER_CRC *
                                 crcsPerFrame;
    do
    {
        prevSize = crcData.size();
        CHECK_RC(GetVCPTable(GSYNC_CRC_BUFF_OPCODE,
                             static_cast<UINT16>(prevSize),
                             &crcData));
    } while ((prevSize != crcData.size()) && (crcData.size() < expectedBytes));

    // The expected number of CRC bytes should have been received
    if (crcData.size() < expectedBytes)
    {
        Printf(Tee::PriError,
               "Display 0x%08x : Incorrect number of CRC bytes received "
               "from GSync!\n"
               "    Expected : %u, Received : %u\n",
               m_DisplayMask,
               static_cast<UINT32>(expectedBytes),
               static_cast<UINT32>(crcData.size()));
        return RC::HW_STATUS_ERROR;
    }

    if (!pPreMemCrcs && !pPostMemCrcs)
        return rc;
    // Pack the CRCs into UINT32 values
    UINT32 *pCrcs = reinterpret_cast<UINT32 *>(&crcData[0]);

    const UINT32 preMemCRCsPerFrame = GetPreMemCrcsPerFrame();
    const UINT32 postMemCRCsPerFrame = GetPostMemCrcsPerFrame();

    // Only loop over the number of frames requested rather than captured (an
    // extra frame was captured so that the SU/PPU CRCs can be reported from
    // the same frame - PPU CRCs lag SU CRCs by a frame). Same assumption holds
    // for PACKER and UNPACKER CRCs in Gsync R3
    for (UINT32 frameIdx = 0; frameIdx < numFrames; frameIdx++)
    {
        if (pPreMemCrcs != nullptr)
        {
            CrcData preMemCrcs;
            for (UINT32 suIdx = 0; suIdx < preMemCRCsPerFrame; suIdx++)
            {
                const UINT32 crcIdx = frameIdx * crcsPerFrame + suIdx;
                const UINT32 crc = pCrcs[crcIdx];
                preMemCrcs.push_back(crc);
                Printf(vcpPri, "Display 0x%08x Frame %u, CrcIdx %u Pre-Mem CRC %d : 0x%08x\n",
                       m_DisplayMask,
                       frameIdx,
                       crcIdx,
                       suIdx,
                       crc);
            }
            pPreMemCrcs->push_back(preMemCrcs);
        }
        if (pPostMemCrcs != nullptr)
        {
            // Post-Mem CRCs are offset by 1 frame, keep the CRCs from the same
            // frame data by copying the Post-Mem CRCs from the next frame
            CrcData postMemCrcs;
            for (UINT32 ppuIdx = 0; ppuIdx < postMemCRCsPerFrame; ppuIdx++)
            {
                const UINT32 offset = m_IsR3Supported ? frameIdx : frameIdx + 1;
                const UINT32 crcIdx = offset * crcsPerFrame +
                                      preMemCRCsPerFrame + ppuIdx;
                const UINT32 crc = pCrcs[crcIdx];
                postMemCrcs.push_back(crc);
                Printf(vcpPri, "Display 0x%08x Frame %u, CrcIdx %u Post-Mem CRC %d : 0x%08x\n",
                       m_DisplayMask,
                       frameIdx,
                       crcIdx,
                       ppuIdx,
                       crc);
            }
            pPostMemCrcs->push_back(postMemCrcs);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Notify the GSync device that the mode has changed
//!
//! \return OK if successful, not OK otherwise
RC GsyncDevice::NotifyModeset()
{
    return GetCrcs(false, m_ModesetSettleFrames, nullptr, nullptr);
}

//-----------------------------------------------------------------------------
//! \brief Notify the GSync device that the image has changed
//!
//! \return OK if successful, not OK otherwise
RC GsyncDevice::NotifyImageChange()
{
    return GetCrcs(false, m_ImageChangeSettleFrames, nullptr, nullptr);
}

//-----------------------------------------------------------------------------
//! \brief Static function to get the maximum CRC frames for a CRC mode
//!
//! \param mode : Mode to get the maximum number of CRC frames for
//!
//! \return Maximum number frames for CRC aclwmulation
UINT32 GsyncDevice::GetMaxCrcFrames(CrcMode mode) const
{
    // Report 1 less than are really available since we always capture an
    // additional frame due to SU/PPU CRCs being offset by a frame
    return (GSYNC_CRC_MEMSIZE / GSYNC_BYTES_PER_CRC / GetCrcsPerFrame(mode)) - 1;
}

//-----------------------------------------------------------------------------
//! \brief function to identify if the panel supports GSync
/*static*/ bool GsyncDevice::IsConnectorGsyncCapable
(
    const UINT32 connector,
    Display *pDisplay
)
{
    MASSERT(Utility::CountBits(connector) == 1);
    return (GsyncSink::Protocol::Unsupported !=
            GsyncSink::GetProtocol(pDisplay, connector)) ?
        true : false;
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
    //! \brief The display Gsync silicon is whitelisted
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
//! \brief Static function to find all gysnc connections of a display
//!
//! \param pDisplay       : Display to check for GSync connections
//! \param pSubdev        : Subdevice pointer being used
//! \param gsyncDisplayId : if this is not null then find only the Gsync device
//!                         with this DisplayID
//! \param pGsyncDevices  : Pointer to returned vector of Gsync devices
//!
//! \return Maximum number frames for CRC aclwmulation
/* static */ RC GsyncDevice::FindGsyncDevices
(
    Display *pDisplay,
    GpuSubdevice *pSubdev,
    DisplayID gsyncDisplayId,
    vector<GsyncDevice> *pGsyncDevices
)
{
    RC rc;
    MASSERT(pGsyncDevices);

    UINT32 connectors = 0;
    if (gsyncDisplayId == Display::NULL_DISPLAY_ID)
    {
         CHECK_RC(pDisplay->GetDetected(&connectors));
    }
    else
    {
        connectors = gsyncDisplayId.Get();
    }

    for (INT32 connector = Utility::BitScanForward(connectors);
         connector >= 0;
         connector = Utility::BitScanForward(connectors, connector + 1))
    {
        UINT32 connectorToCheck = 1 << connector;

        if (!IsConnectorGsyncCapable(connectorToCheck, pDisplay))
        {
            continue;
        }
        Printf(Tee::PriDebug, "%s: Display 0x%x is gsync capable\n",
                __FUNCTION__, connectorToCheck);
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
            Printf(Tee::PriDebug,
                    "%s: GsyncFound. Initializing the Gsync dev\n", __FUNCTION__);
            GsyncDevice lwrDev(pDisplay, pSubdev, connectorToCheck);
            CHECK_RC(lwrDev.Initialize());
            pGsyncDevices->push_back(lwrDev);
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Determine if GSync CRC mode is enabled
//!
//! \param pbEnabled : Pointer to returned status of GSync CRC mode
//!
//! \return OK if was successful, not OK otherwise
RC GsyncDevice::CrcModeEnabled(bool *pbEnabled)
{
    RC rc;
    VCPFeatureData magic0 = { 0 };
    VCPFeatureData magic1 = { 0 };

    // Readback the magic registers to check the mode
    CHECK_RC(GetVCPFeature(GSYNC_MAGIC0_OPCODE, &magic0, Tee::PriNormal));
    CHECK_RC(GetVCPFeature(GSYNC_MAGIC1_OPCODE, &magic1, Tee::PriNormal));
    *pbEnabled = ((magic0.sl == GSYNC_MAGIC0_CRC_EN_SL) &&
                  (magic1.sl == GSYNC_MAGIC1_CRC_EN_SL));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Enable GSync CRC mode
//!
//! \return OK if GSync CRC enable was successful, not OK otherwise
RC GsyncDevice::EnableCrcMode()
{
    RC rc;

    // Enabling GSync CRC requires writing 2 magic registers to enable the
    // GSync device CRC register page.
    VCPFeatureData magic0, magic1;
    magic0.sh = GSYNC_MAGIC0_UNLOCK_SH;
    magic0.sl = GSYNC_MAGIC0_UNLOCK_SL;
    magic1.sh = GSYNC_MAGIC1_UNLOCK_CRC_SH;
    magic1.sl = GSYNC_MAGIC1_UNLOCK_CRC_SL;
    CHECK_RC(SetVCPFeature(GSYNC_MAGIC0_OPCODE, magic0));
    CHECK_RC(SetVCPFeature(GSYNC_MAGIC1_OPCODE, magic1));

    // Readback the magic registers to ensure that CRC was truly enabled
    memset(&magic0, 0, sizeof(magic0));
    memset(&magic1, 0, sizeof(magic1));
    CHECK_RC(GetVCPFeature(GSYNC_MAGIC0_OPCODE, &magic0, Tee::PriNormal));
    CHECK_RC(GetVCPFeature(GSYNC_MAGIC1_OPCODE, &magic1, Tee::PriNormal));
    if ((magic0.sl != GSYNC_MAGIC0_CRC_EN_SL) ||
        (magic1.sl != GSYNC_MAGIC1_CRC_EN_SL))
    {
        Printf(Tee::PriError,
               "Display 0x%08x : Unable to enable CRC gathering!\n",
               m_DisplayMask);
        return RC::HW_STATUS_ERROR;
    }

    // Wait for CRC idle
    CrcIdlePollArgs crcIdleArgs;
    crcIdleArgs.pDev   = this;
    crcIdleArgs.sleepMs = m_CrcIdlePollHz ? (1000 / m_CrcIdlePollHz) : 0;
    crcIdleArgs.retries = 0;
    crcIdleArgs.pri = (m_DebugLevel > DBG_NONE) ? Tee::PriNormal : Tee::PriLow;
    CHECK_RC(POLLWRAP_HW(PollCrcIdle, &crcIdleArgs, m_CrcIdleTimeoutMs));

    Printf((m_DebugLevel > DBG_NONE) ? Tee::PriNormal : Tee::PriLow,
           "Display 0x%08x : GSync CRC mode successfully enabled!\n",
           m_DisplayMask);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Determine if GSync normal mode is enabled
//!
//! \param pbEnabled : Pointer to returned status of GSync normal mode
//!
//! \return OK if was successful, not OK otherwise
RC GsyncDevice::NormalModeEnabled(bool *pbEnabled)
{
    RC rc;
    VCPFeatureData magic0 = { 0 };
    VCPFeatureData magic1 = { 0 };

    // Readback the magic registers to check the mode
    CHECK_RC(GetVCPFeature(GSYNC_MAGIC0_OPCODE, &magic0, Tee::PriNormal));
    CHECK_RC(GetVCPFeature(GSYNC_MAGIC1_OPCODE, &magic1, Tee::PriNormal));
    *pbEnabled = ((magic0.sl == GSYNC_MAGIC0_NORMAL_EN_SL) &&
                  (magic1.sl == GSYNC_MAGIC1_NORMAL_EN_SL));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Enable GSync Normal mode
//!
//! \return OK if GSync Normal enable was successful, not OK otherwise
RC GsyncDevice::EnableNormalMode()
{
    RC rc;

    // Enabling GSync normal mode requires writing 2 magic registers to enable the
    // GSync device normal register page.
    VCPFeatureData magic0, magic1;
    magic0.sh = GSYNC_MAGIC0_UNLOCK_SH;
    magic0.sl = GSYNC_MAGIC0_UNLOCK_SL;
    magic1.sh = GSYNC_MAGIC1_UNLOCK_NORMAL_SH;
    magic1.sl = GSYNC_MAGIC1_UNLOCK_NORMAL_SL;
    CHECK_RC(SetVCPFeature(GSYNC_MAGIC0_OPCODE, magic0));
    CHECK_RC(SetVCPFeature(GSYNC_MAGIC1_OPCODE, magic1));

    // Readback the magic registers to ensure that CRC was truly enabled
    memset(&magic0, 0, sizeof(magic0));
    memset(&magic1, 0, sizeof(magic1));
    CHECK_RC(GetVCPFeature(GSYNC_MAGIC0_OPCODE, &magic0, Tee::PriError));
    CHECK_RC(GetVCPFeature(GSYNC_MAGIC1_OPCODE, &magic1, Tee::PriError));
    if ((magic0.sl != GSYNC_MAGIC0_NORMAL_EN_SL) ||
        (magic1.sl != GSYNC_MAGIC1_NORMAL_EN_SL))
    {
        Printf(Tee::PriError,
               "Display 0x%08x : Unable to enable normal mode!\n",
               m_DisplayMask);
        return RC::HW_STATUS_ERROR;
    }

    Printf((m_DebugLevel > DBG_NONE) ? Tee::PriNormal : Tee::PriLow,
           "Display 0x%08x : GSync Normal mode successfully enabled!\n",
           m_DisplayMask);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief VCP GetFeature operation (DDC/CI spec v1.1 section 4.3)
//!
//! \param vcpOpcode : VCP opcode to get
//! \param pVcpData  : Pointer to returned data for the VCP opcode
//! \param errPri    : Priority to print errors at
//!
//! \return OK getting the VCP feature was successful, not OK otherwise
RC GsyncDevice::GetVCPFeature
(
    UINT08          vcpOpcode,
    VCPFeatureData *pVcpData,
    Tee::Priority   errPri
)
{
    StickyRC rc;
    UINT08 vcpGetFeature[VCP_GET_FEATURE_PKT_LENGTH];
    vcpGetFeature[0] = VCP_REG_ADDR;
    vcpGetFeature[1] = VCP_GET_FEATURE_DATA_LENGTH | DP_LENGTH_MASK;
    vcpGetFeature[2] = VCP_GET_FEATURE_OPCODE;
    vcpGetFeature[3] = vcpOpcode;
    vcpGetFeature[4] = DP_WRITE_ADDR ^ ComputeVCPPacketChecksum(vcpGetFeature,
                                              VCP_GET_FEATURE_PKT_LENGTH - 1);
    UINT08 retData[VCP_GET_FEATURE_READ_PKT_LENGTH] = { 0 };
    UINT32 retryNum = 0;
    string errorStr = Utility::StrPrintf("%s: Display 0x%x VCP Opcode 0x%02x failed",
            __FUNCTION__, m_DisplayMask, vcpOpcode);
    do
    {
        rc.Clear();
        rc = SendCommandToSink(Display::DP_AUX_CHANNEL_COMMAND_WRITE,
                vcpGetFeature,
                VCP_GET_FEATURE_PKT_LENGTH,
                Display::DP_AUX_RETRY_DISABLED);
        if (rc == RC::LWRM_TIMEOUT)
        {
            continue;
        }
        else if (OK != rc)
        {
            Printf(errPri, "%s Command Write.\n", errorStr.c_str());
            return rc;
        }

        rc = SendCommandToSink(Display::DP_AUX_CHANNEL_COMMAND_READ,
                retData,
                VCP_GET_FEATURE_READ_PKT_LENGTH,
                Display::DP_AUX_RETRY_DISABLED);
        if (rc == OK)
        {
            break;
        }
        else if (rc == RC::LWRM_TIMEOUT)
        {
            continue;
        }
        else
        {
            Printf(errPri, "%s Command Read.\n", errorStr.c_str());
            return rc;
        }
    } while (++retryNum < m_VCPCmdRetries);

    if (rc == RC::LWRM_TIMEOUT)
    {
            Printf(errPri, "%s Timed out.\n", errorStr.c_str());
        return rc;
    }

    PrintVCPPacket("GetFeature", vcpOpcode, &retData[0],
                   VCP_GET_FEATURE_READ_PKT_LENGTH);

    if (retData[VCP_GET_FEATURE_RC_IDX] != 0)
    {
        Printf(Tee::PriError,
               "Display 0x%08x : VCP GetFeature 0x%02x returned packet "
               "error : %u\n",
               m_DisplayMask, vcpOpcode, retData[VCP_GET_FEATURE_RC_IDX]);
        return RC::HW_STATUS_ERROR;
    }
    pVcpData->mh = retData[VCP_GET_FEATURE_MH_IDX];
    pVcpData->ml = retData[VCP_GET_FEATURE_ML_IDX];
    pVcpData->sh = retData[VCP_GET_FEATURE_SH_IDX];
    pVcpData->sl = retData[VCP_GET_FEATURE_SL_IDX];

    Printf((m_DebugLevel > DBG_NONE) ? Tee::PriNormal : Tee::PriSecret,
         "Display 0x%08x VCP GetFeature 0x%02x : MH=0x%02x, ML=0x%02x, "
         "SH=0x%02x, SL=0x%02x\n",
         m_DisplayMask, vcpOpcode, pVcpData->mh, pVcpData->ml, pVcpData->sh,
           pVcpData->sl);

    return rc;
}

//-----------------------------------------------------------------------------
RC GsyncDevice::AccessGsyncSink
(
    GsyncSink::TMDS           type,
    Display::DpAuxTransaction command,
    UINT08 * const            pBuffer,
    const UINT32              buffBytes,
    Display::DpAuxRetry       enableRetry //Don't care
)
{
    return OK;
}

//-----------------------------------------------------------------------------
RC GsyncDevice::AccessGsyncSink
(
    GsyncSink::DP             type,
    Display::DpAuxTransaction command,
    UINT08 * const            pBuffer,
    UINT32                    bufferSize,
    Display::DpAuxRetry       enableRetry
)
{
    StickyRC rc;
    UINT32 lwrRetry = 0;
    bool   bComplete = false;
    const UINT32 addr = (command == Display::DP_AUX_CHANNEL_COMMAND_WRITE) ?
        (DP_WRITE_ADDR >> 1) : (DP_READ_ADDR >> 1);
    const RC auxNakError = (command == Display::DP_AUX_CHANNEL_COMMAND_WRITE) ?
        RC::LWRM_ERROR : RC::LWRM_ILWALID_STATE;
    Tee::Priority errPri = (m_DebugLevel > DBG_NONE) ? Tee::PriNormal : Tee::PriLow;

    do
    {
        rc = m_pDisplay->AccessDpAuxChannel(m_DisplayMask,
                                       m_pSubdev,
                                       command,
                                       Display::DP_AUX_CHANNEL_COMMAND_TYPE_I2C,
                                       pBuffer,
                                       addr,
                                       bufferSize,
                                       Display::DP_AUX_CHANNEL_ADDR_STATIC,
                                       enableRetry);

        if (rc == RC::LWRM_TIMEOUT)
        {
            //No need to retry after RM timeout. This indicates NO_STOP_ERROR in aux transaction
            Printf(errPri, "%s Timed out\n", __FUNCTION__);
            break;
        }
        // If RM returns an error code associated with a NAK, then delay and try
        // again, on any other error or success, exit
        if (rc == auxNakError)
        {
            Printf(errPri,
                   "AccessDpAuxChannel returned %d, retrying!\n",
                   rc.Get());

            rc.Clear();
            if (m_DpAuxRetryDelayMs)
                Tasker::Sleep(m_DpAuxRetryDelayMs);
        }
        else
        {
            bComplete = true;
        }

    } while (!bComplete && (++lwrRetry < m_DpAuxRetries));

    if ((OK == rc) && !bComplete)
        rc = RC::HW_STATUS_ERROR;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief VCP GetTable operation (DDC/CI spec v1.1 section 4.8.2)
//!
//! \param vcpOpcode   : VCP opcode to get table on
//! \param offset      : Offset within the table to start reading
//! \param pTableData  : Pointer to returned table data
//!
//! \return OK getting the VCP table was successful, not OK otherwise
RC GsyncDevice::GetVCPTable
(
    UINT08 vcpOpcode,
    UINT16 offset,
    vector<UINT08> *pTableData
)
{
    StickyRC rc;
    UINT08 vcpReadTable[VCP_READ_TABLE_PKT_LENGTH];
    vcpReadTable[0] = VCP_REG_ADDR;
    vcpReadTable[1] = VCP_READ_TABLE_DATA_LENGTH | DP_LENGTH_MASK;
    vcpReadTable[2] = VCP_READ_TABLE_OPCODE;
    vcpReadTable[3] = vcpOpcode;
    vcpReadTable[4] = (offset >> 8) & 0xff;
    vcpReadTable[5] = offset & 0xff;
    vcpReadTable[6] = DP_WRITE_ADDR ^ ComputeVCPPacketChecksum(vcpReadTable,
                                              VCP_READ_TABLE_PKT_LENGTH - 1);
    UINT08 retData[VCP_READ_TABLE_READ_PKT_LENGTH] = { 0 };
    UINT32 retryNum = 0;
    string errorStr = Utility::StrPrintf("%s: Display 0x%x VCP Opcode 0x%02x failed",
            __FUNCTION__, m_DisplayMask, vcpOpcode);
    do
    {
        rc.Clear();
        rc = SendCommandToSink(Display::DP_AUX_CHANNEL_COMMAND_WRITE,
                                 vcpReadTable,
                                 VCP_READ_TABLE_PKT_LENGTH,
                                 Display::DP_AUX_RETRY_DISABLED);
        if (rc == RC::LWRM_TIMEOUT)
        {
            continue;
        }
        else if (rc != OK)
        {
            Printf(Tee::PriError, "%s Command Write\n", errorStr.c_str());
            return rc;
        }

        // GSYNC HW returns wrong bytes if there is no delay between write and read in HDMI i2c
        // Introducing a delay of 50 ms fixs the problem
        if ((m_SorProtocol == GsyncSink::Protocol::Tmds_A     ||
             m_SorProtocol == GsyncSink::Protocol::Tmds_B     ||
             m_SorProtocol == GsyncSink::Protocol::Tmds_Dual) &&
             m_pSubdev->HasBug(2080917))
        {
            Tasker::Sleep(50);
        }

        rc = SendCommandToSink(Display::DP_AUX_CHANNEL_COMMAND_READ,
                                 retData,
                                 VCP_READ_TABLE_READ_PKT_LENGTH,
                                 Display::DP_AUX_RETRY_DISABLED);
        if (rc == OK)
        {
            break;
        }
        else if (rc == RC::LWRM_TIMEOUT)
        {
            continue;
        }
        else
        {
            Printf(Tee::PriError, "%s Command Read\n", errorStr.c_str());
            return rc;
        }
    } while (++retryNum < m_VCPCmdRetries);

    if (rc == RC::LWRM_TIMEOUT)
    {
        Printf(Tee::PriError, "%s Timed out\n", errorStr.c_str());
        return rc;
    }

    PrintVCPPacket("GetTable", vcpOpcode, &retData[0],
                   VCP_READ_TABLE_READ_PKT_LENGTH);

    UINT32 dataRcvd = retData[VCP_READ_TABLE_LEN_IDX] & ~DP_LENGTH_MASK;
    dataRcvd = (dataRcvd > VCP_PKT_OVERHEAD) ? dataRcvd - VCP_PKT_OVERHEAD : 0;
    for (UINT32 i = 0; i < dataRcvd; i++)
    {
        pTableData->push_back(retData[VCP_READ_TABLE_DATA_START_IDX + i]);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief VCP SetFeature operation (DDC/CI spec v1.1 section 4.4)
//!
//! \param vcpOpcode : VCP opcode to set feature on
//! \param vcpData   : VCP data to set
//!
//! \return OK setting the VCP feature was successful, not OK otherwise
RC GsyncDevice::SetVCPFeature
(
    UINT08                vcpOpcode,
    const VCPFeatureData &vcpData
)
{
    RC rc;
    UINT08 vcpSetFeature[VCP_SET_FEATURE_PKT_LENGTH];
    vcpSetFeature[0] = VCP_REG_ADDR;
    vcpSetFeature[1] = VCP_SET_FEATURE_DATA_LENGTH | DP_LENGTH_MASK;
    vcpSetFeature[2] = VCP_SET_FEATURE_OPCODE;
    vcpSetFeature[3] = vcpOpcode;
    vcpSetFeature[4] = vcpData.sh;
    vcpSetFeature[5] = vcpData.sl;
    vcpSetFeature[6] = DP_WRITE_ADDR ^ ComputeVCPPacketChecksum(vcpSetFeature,
                                              VCP_SET_FEATURE_PKT_LENGTH - 1);
    rc = SendCommandToSink(Display::DP_AUX_CHANNEL_COMMAND_WRITE,
                             vcpSetFeature,
                             VCP_SET_FEATURE_PKT_LENGTH,
                             Display::DP_AUX_RETRY_ENABLED);
    if (OK != rc)
    {
        Printf(Tee::PriError, "VCP SetFeature 0x%02x failed\n", vcpOpcode);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Perform a DPAux or I2C transaction with retries and a delay in between
//!
//! \param command    : Command to send
//! \param buffer     : Input or output data depending upon read/write
//! \param bufferSize : Number of bytes to read/write
//!
//! \return Data checksum
RC GsyncDevice::SendCommandToSink
(
    Display::DpAuxTransaction command,
    UINT08*                   pBuffer,
    UINT32                    bufferSize,
    Display::DpAuxRetry       enableRetry
)
{
    RC rc;
    switch (m_SorProtocol)
    {
        case GsyncSink::Protocol::Dp_A:
        case GsyncSink::Protocol::Dp_B:
            {
                GsyncSink::SinkType<GsyncSink::Protocol::Dp_A>::Type sink;
                CHECK_RC(AccessGsyncSink(sink,
                            command,
                            pBuffer,
                            bufferSize,
                            enableRetry));
            }
            break;
        case GsyncSink::Protocol::Tmds_A:
        case GsyncSink::Protocol::Tmds_B:
        case GsyncSink::Protocol::Tmds_Dual:
            {
                GsyncSink::SinkType<GsyncSink::Protocol::Tmds_A>::Type sink;
                CHECK_RC(AccessGsyncSink(sink,
                            command,
                            pBuffer,
                            bufferSize,
                            enableRetry));
            }
            break;
        default:
            MASSERT(0);
            return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Compute a VCP checksum
//!
//! \param pData  : Data to checksum
//! \param length : Length of the data
//!
//! \return Data checksum
UINT08 GsyncDevice::ComputeVCPPacketChecksum(const UINT08 *pData, UINT08 length)
{
    UINT08 checksum = 0;
    UINT08 i;
    for (i = 0; i < length; i++)
    {
        checksum ^= pData[i];
    }
    return checksum;
}

//-----------------------------------------------------------------------------
//! \brief Print a VCP packet
//!
//! \param packetStr : String representing the type of packet
//! \param vcpOpcode : VCP Opcode
//! \param pPktData  : Raw packet data
//! \param pktLen    : Length of the packet
//!
//! \return Data checksum
void GsyncDevice::PrintVCPPacket
(
    string         packetStr,
    UINT08         vcpOpcode,
    const UINT08 * pPktData,
    UINT32         pktLen
)
{
    Tee::Priority vcpPri = (m_DebugLevel == DBG_ALL) ? Tee::PriNormal :
                                                       Tee::PriSecret;
    Printf(vcpPri, "Display 0x%08x : VCP %s 0x%02x:\n\t",
           m_DisplayMask, packetStr.c_str(), vcpOpcode);
    for (UINT32 ii = 0; ii < pktLen; ii++)
    {
        if ((ii != 0) && ((ii % 8) == 0))
            Printf(vcpPri, "\n\t");
        Printf(vcpPri, "0x%02x ", pPktData[ii]);
    }
    Printf(vcpPri, "\n");
}

//-----------------------------------------------------------------------------
//! \brief Retrieve the DP symbol errors per lane
//!
//! \param pTotalErrors   : Pointer to returned total errors
//! \param pPerLaneErrors : Pointer to returned per-lane errors
//!
//! \return OK if retrieving errors was successful, not OK otherwise
RC GsyncDevice::GetDpSymbolErrors
(
    UINT32 *pTotalErrors,
    vector<UINT16> *pPerLaneErrors
)
{
    RC rc;

    UINT32 laneCount = 0;
    CHECK_RC(m_pDisplay->GetLwrrentDpLaneCount(m_DisplayMask, &laneCount));

    const UINT32 DPCD_LANE_SYMBOL_ERRORS_ADDR = 0x210;
    vector<UINT16> laneErrors(laneCount, 0);
    CHECK_RC(m_pDisplay->ReadDPCD(m_DisplayMask,
                                  m_pSubdev,
                                  &laneErrors[0],
                                  DPCD_LANE_SYMBOL_ERRORS_ADDR,
                                  static_cast<UINT32>(laneErrors.size() *
                                      sizeof(laneErrors[0]))));

    UINT32 totalErrors = 0;
    static const UINT16 ERRORS_VALID_BIT = 0x8000;
    for (size_t lane = 0; lane < laneErrors.size(); lane++)
    {
        if (laneErrors[lane] & ERRORS_VALID_BIT)
            laneErrors[lane] &= ~ERRORS_VALID_BIT;
        else
            laneErrors[lane] = 0;
        totalErrors += laneErrors[lane];
    }

    Printf((m_DebugLevel > DBG_NONE) ? Tee::PriNormal : Tee::PriLow,
           "Display 0x%08x : %u DP symbol errors found:\n",
           m_DisplayMask, totalErrors);
    if (totalErrors)
    {
        for (size_t lane = 0; lane < laneErrors.size(); lane++)
        {
            Printf((m_DebugLevel > DBG_NONE) ? Tee::PriNormal : Tee::PriLow,
                   "    Lane %u : %u\n",
                   static_cast<UINT32>(lane), laneErrors[lane]);
        }
    }

    if (pTotalErrors != nullptr)
        *pTotalErrors = totalErrors;

    if (pPerLaneErrors != nullptr)
        *pPerLaneErrors = laneErrors;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the number of GSync CRCs per frame
//!
//! \param mode : Gsync mode to get the number of CRCs per frame
//!
//! \return Number of GSync CRCs per frame
UINT32 GsyncDevice::GetCrcsPerFrame(CrcMode mode) const
{
    switch (mode)
    {
        default:
            MASSERT(!"Unknown GSync CRC mode");
            Printf(Tee::PriNormal, "Unknown GSync CRC mode %d", mode);
            // Intentional fall through
        case CRC_SU_ONLY:
            return GSYNC_SU_CRCS_PER_FRAME;
        case CRC_PRE_AND_POST_MEM:
            return ( m_IsR3Supported ?
                    (GSYNC_PACKER_CRCS_PER_FRAME + GSYNC_UNPACKER_CRCS_PER_FRAME)
                    :(GSYNC_SU_CRCS_PER_FRAME + GSYNC_PPU_CRCS_PER_FRAME));
    }

    return GSYNC_SU_CRCS_PER_FRAME;
}

UINT32 GsyncDevice::GetPreMemCrcsPerFrame() const
{
    return (m_IsR3Supported ? GSYNC_PACKER_CRCS_PER_FRAME
                            : GSYNC_SU_CRCS_PER_FRAME);
}

UINT32 GsyncDevice::GetPostMemCrcsPerFrame() const
{
    return (m_IsR3Supported ? GSYNC_UNPACKER_CRCS_PER_FRAME
                            : GSYNC_PPU_CRCS_PER_FRAME);
}

//-----------------------------------------------------------------------------
//! \brief Static function for polling GSync CRC idle
//!
//! \param pvArgs : void pointer to polling arguments
//!
//! \return true if idle or error, false otherwise
/* static */ bool GsyncDevice::PollCrcIdle(void *pvArgs)
{
    CrcIdlePollArgs *pArgs = static_cast<CrcIdlePollArgs *>(pvArgs);

    VCPFeatureData crcStatus;
    static const UINT32 maxIdleErrorRetries = 10;

    pArgs->pollRc = pArgs->pDev->GetVCPFeature(GSYNC_CRC_STAT_OPCODE,
                                               &crcStatus,
                                               pArgs->pri);
    bool bReturn = false;
    if ((pArgs->pollRc != OK) && (pArgs->retries < maxIdleErrorRetries))
    {
        Printf(pArgs->pri,
               "PollCrcIdle GetVCPFeature returned %d, retrying!\n",
               pArgs->pollRc.Get());
        pArgs->retries++;
        pArgs->pollRc.Clear();
    }
    else
    {
        if (OK != pArgs->pollRc)
        {
            Printf(Tee::PriError,
                   "PollCrcIdle GetVCPFeature failed %d\n",
                   pArgs->pollRc.Get());
        }
        bReturn = (OK != pArgs->pollRc) || (crcStatus.sl == 0);
    }

    // Need to slow down polling rate over the DPAUX channel otherwise
    // spurious seem to happen
    if (!bReturn && pArgs->sleepMs)
        Tasker::Sleep(pArgs->sleepMs);

    return bReturn;
}

bool GsyncDevice::IsDeviceR3Capable() const
{
    return m_IsR3Supported;
}
