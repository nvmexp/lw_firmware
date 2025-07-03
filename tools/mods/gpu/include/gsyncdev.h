/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <vector>
#include "core/include/display.h"
#include "core/include/extcrcdev.h"

class GpuSubdevice;

//! GsyncSink is the Display Panel or Module which has Gsync chip inside
namespace GsyncSink
{
    enum class Protocol
    {
        Dp_A = LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A,
        Dp_B = LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B,
        Tmds_A = LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A,
        Tmds_B = LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B,
        Tmds_Dual = LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS,
        Unsupported = static_cast<INT32>(LW0073_CTRL_SPECIFIC_OR_PROTOCOL_UNKNOWN)
    };
    Protocol GetProtocol(Display *pDisplay, UINT32 dispMask);

    const map<UINT32, bool> s_ProtocolGsyncCap =
    {
        { LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_LVDS_LWSTOM,   false },
        { LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A, true },
        { LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B, true },
        { LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS,     true },
        { LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A,          true },
        { LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B,          true },
    };

    //! describes the Sink based on the SOR Protocol.
    struct GsyncEncoder{};
    struct DP : GsyncEncoder{};
    struct TMDS : GsyncEncoder{};
    struct Unsupported : GsyncEncoder{};
    template <Protocol> struct SinkType
    { using Type = Unsupported; };
    template <> struct SinkType<Protocol::Dp_A>
    { using Type = DP; };
    template <> struct SinkType<Protocol::Dp_B>
    { using Type = DP; };
    template <> struct SinkType<Protocol::Tmds_A>
    { using Type = TMDS; };
    template <> struct SinkType<Protocol::Tmds_B>
    { using Type = TMDS; };
    template <> struct SinkType<Protocol::Tmds_Dual>
    { using Type = TMDS; };
};

// GsyncDevice Class
//
// Interface for communicating with a Gsync device
//
class GsyncDevice : public CrcGoldenInterface
{
public:
    GsyncDevice(Display *pDisplay, GpuSubdevice *pSubdev, UINT32 displayMask);
    GsyncDevice(Display *pDisplay, GpuSubdevice *pSubdev, UINT32 displayMask,
            GsyncSink::Protocol protocol);
    virtual ~GsyncDevice() {}

    //! CrcData is a vector because there are 4 CRCs per frame
    typedef vector<UINT32> CrcData;

    //! enum describing CRC collection modes supported by the GSync device
    enum CrcMode
    {
         CRC_SU_ONLY          //!< SU (pre Gsync memory) CRCs
        ,CRC_PRE_AND_POST_MEM //!< Pre and Post Gsync memory CRCs.CRCs generated at
                              //!< SU/PPU for Gsync R2 or PACKER/UNPACKER block for R3
    };

    //! enum describing debug modes for the Gsync device
    enum DebugLevel
    {
        DBG_NONE   = 0
       ,DBG_SOME   = 1
       ,DBG_ALL    = 2
    };

    UINT32 GetDisplayMask() const { return m_DisplayMask; }
    DisplayID GetDisplayID() const { return DisplayID(m_DisplayMask); }
    void SetDebugLevel(DebugLevel debugLevel) { m_DebugLevel = debugLevel; }
    void SetCrcIdlePollHz(UINT32 pollRateHz) { m_CrcIdlePollHz = pollRateHz; }
    void SetCrcIdleTimeout(UINT32 timeoutMs) { m_CrcIdleTimeoutMs = timeoutMs; }
    void SetModesetSettleFrames(UINT32 frames) { m_ModesetSettleFrames = frames; }
    void SetImageChangeSettleFrames(UINT32 frames) { m_ImageChangeSettleFrames = frames; }
    void SetDpAuxRetries(UINT32 retries) { m_DpAuxRetries = retries; }
    void SetDpAuxRetryDelayMs(UINT32 delayMs) { m_DpAuxRetryDelayMs = delayMs; }
    void SetVCPTableCommandRetries(UINT32 retries) { m_VCPCmdRetries = retries; }

    virtual UINT32 NumCrcBins() const;
    virtual RC GetCrcSignature(string *pCrcSignature);
    virtual RC GetCrcValues(UINT32 *pCrcs);
    virtual void ReportCrcMiscompare
    (
        UINT32 bin,
        UINT32 expectedCRC,
        UINT32 actualCRC
    );
    void SetNumCrcFrames(UINT32 numCrcFrames);

    RC FactoryReset();
    RC GetTemperature(UINT32 * pTempC);
    RC GetBoardId(UINT16 *pBoardId);
    RC GetRefreshRate(UINT32 * pRefRate);
    RC GetCrcs
    (
        bool             bResetCrcs,
        UINT32           numFrames,
        vector<CrcData> *pSUCrcs,
        vector<CrcData> *pPPUCrcs
    );
    RC NotifyModeset();
    RC NotifyImageChange();

    UINT32 GetMaxCrcFrames(CrcMode mode) const;
    static RC FindGsyncDevices
    (
        Display *pDisplay,
        GpuSubdevice *pSubdev,
        DisplayID gsyncDisplayId,
        vector<GsyncDevice> *pGsyncDevices
    );
    RC Initialize();
    // TODO Temporary change to expose device revision so that only selective tests
    // are enabled on R3. Once all tests are supported, we can remove this method
    bool IsDeviceR3Capable()const;

    Display* GetPDisplay() const { return m_pDisplay; }

    //! Structure for VCP Feature Get/Set packets
    struct VCPFeatureData
    {
        UINT08 mh; //!< Maximum value high byte
        UINT08 ml; //!< Maximum value low byte
        UINT08 sh; //!< Current value high byte
        UINT08 sl; //!< Current value low byte
    };

    RC GetVCPFeature
    (
        UINT08          vcpOpcode,
        VCPFeatureData *pVcpData,
        Tee::Priority   errPri
    );
    RC SetVCPFeature
    (
        UINT08                vcpOpcode,
        const VCPFeatureData &vcpData
    );

protected:
    GsyncSink::Protocol GetSORProtocol() const { return m_SorProtocol; }
    virtual RC GetDpSymbolErrors
    (
        UINT32 *pTotalErrors,
        vector<UINT16> *pPerLaneErrors
    );
private:

    //! Polling structure for waiting for CRC idle
    struct CrcIdlePollArgs
    {
        RC            pollRc;  //!< RC value from CRC idle query
        GsyncDevice * pDev;    //!< Pointer to gsync device object
        UINT32        sleepMs; //!< Sleep time between polling
        UINT32        retries; //!< Current number of retries for reading
                               //!< CRC idle
        Tee::Priority pri;     //!< Priority for prints during idle
    };

    RC CrcModeEnabled(bool *pbEnabled);
    RC EnableCrcMode();
    RC NormalModeEnabled(bool *pbEnabled);
    RC EnableNormalMode();
    RC GetVCPTable
    (
        UINT08          vcpOpcode,
        UINT16          offset,
        vector<UINT08> *pTableData
    );
    RC AccessGsyncSink
    (
        GsyncSink::DP             type,
        Display::DpAuxTransaction command,
        UINT08 * const            pBuffer,
        const UINT32              bufferSize,
        Display::DpAuxRetry       enableRetry
    );
    virtual RC AccessGsyncSink
    (
        GsyncSink::TMDS           type,
        Display::DpAuxTransaction command,
        UINT08 * const            pBuffer,
        const UINT32              bufferSize,
        Display::DpAuxRetry       enableRetry
    );
    RC SendCommandToSink
    (
        Display::DpAuxTransaction command,
        UINT08*                   pBuffer,
        UINT32                    bufferSize,
        Display::DpAuxRetry       enableRetry
    );
    UINT08 ComputeVCPPacketChecksum(const UINT08 *pData, UINT08 length);
    void PrintVCPPacket
    (
        string        packetStr,
        UINT08        vcpOpcode,
        const UINT08 *pPktData,
        UINT32        pktLen
    );

    UINT32 GetCrcsPerFrame(CrcMode crcMode) const;
    UINT32 GetPreMemCrcsPerFrame() const;
    UINT32 GetPostMemCrcsPerFrame() const;
    static bool PollCrcIdle(void *pvArgs);

    static bool IsConnectorGsyncCapable
    (
        const UINT32 connector
        , Display *pDisplay
    );
    Display      *m_pDisplay;         //!< Display connected to the Gsync device
    GpuSubdevice *m_pSubdev;          //!< Subdevice used on connected display
    UINT32        m_DisplayMask;      //!< Display mask connected to the Gsync
                                      //!< device

    UINT32        m_NumCrcFrames;     //!< Number of frames to CRC
    UINT32        m_CrcIdlePollHz;    //!< CRC idle poll rate in Hz (default=20)
    UINT32        m_CrcIdleTimeoutMs; //!< Timeout in ms for polling CRC idle
                                      //!< (default = 5000)
    DebugLevel    m_DebugLevel;       //!< Debug Level
    UINT32        m_DpAuxRetries;     //!< Number of retries for DP AUX
                                      //!< transactions (default = 3)
    UINT32        m_DpAuxRetryDelayMs;//!< Delay time in ms between DP AUX
                                      //!< retries (default=10)
    UINT32        m_VCPCmdRetries;    //!< GetVCPTable and VCP feature sequence
                                      //!< retry limit.
    UINT32        m_ImageChangeSettleFrames; //!< Number of frames to wait after
                                             //!< image changes (default = 2)
    UINT32        m_ModesetSettleFrames; //!< Number of frames to wait after
                                         //!< mode changes (default = 10)
    bool          m_IsR3Supported;       //!< Is Gsync R3 supported on this device

    const GsyncSink::Protocol m_SorProtocol;   //!< type of GsyncSink
};
