/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "gpu/include/gpusbdev.h"

#include "core/include/hbmtypes.h"
#include "ctrl/ctrl2080/ctrl2080gpu.h"

#include "gpu/utility/fsp.h"

class HopperGpuSubdevice : public GpuSubdevice
{
    using HbmSite = Memory::Hbm::Site;

public:
    enum secClusterIdx
    {
        secCmd = 0,
        secCfg = 1,
        secChk = 2,
        selwnlock = 3
    };
    struct h2jUnlockInfo
    {
        vector<GpuSubdevice::JtagCluster>clusters;
        vector<UINT32> selwreKeys;      // IN:secure keys provide by the Green Hill Server
        vector<UINT32> cfgMask;         // IN:bitmask of the features to enable
        vector<UINT32> selwreData;      // OUT:bitstream for the SEC_CHK chain
        vector<UINT32> cfgData;         // OUT:bitstream for the SEC_CFG chain
        vector<UINT32> cmdData;         // OUT:bitstream for the SEC_CMD chain
        vector<UINT32> chkData;         // OUT:bitstream for the SEC_CHK chain after writing the
                                        //     secure data
        vector<UINT32> unlockData;      // OUT:bitstread for the SEC_UNLOCK_STATUS chain after
                                        //     writing the secure data.
        UINT32 numChiplets;             // OUT:number of chiplets in this cluster.
        UINT32 unlockFeatureStartBit;   // OUT:location of the feature bits in the unlockData[]
        UINT32 cfgFeatureStartBit;      // OUT:location of the feature bits in the cfgData[]
        UINT32 numFeatureBits;          // OUT:number of feature bits to compare when verifying the
                                        //     unlock sequence.
    };

    HopperGpuSubdevice(const char*, Gpu::LwDeviceId, const PciDevInfo*);
    virtual ~HopperGpuSubdevice() { }

    void            ApplyJtagChanges(UINT32 processFlag) override;
    void            ApplyMemCfgProperties(bool InPreVBIOSSetup) override;
    GpuPerfmon *    CreatePerfmon(void) override;
    bool            DoesFalconExist(UINT32 falconId) override;
    bool            FaultBufferOverflowed() const override;
#ifdef LW_VERIF_FEATURES
    void            FilterMemCfgRegOverrides
    (
        vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS>* pOverrides
    ) override;
#endif
    RC              GetAteSpeedo(UINT32 SpeedoNum, UINT32 *pSpeedo, UINT32 *pVersion) override;
    RC              GetAteSramIddq(UINT32 *pIddq) override;
    RC              GetAteMsvddIddq(UINT32 *pIddq) override;
    RC              GetSramBin(INT32* pSramBin) override;
    RC              GetFalconStatus(UINT32 falconId, UINT32 *pLevel) override;
    UINT32          GetPLLCfg(PLLType Pll) const override;
    RC              GetSmEccEnable(bool * pEnable, bool * pOverride) const override;
    RC              GetStraps(vector<pair<UINT32, UINT32> >* pStraps) override;

    RC              GetWPRInfo
    (
        UINT32 regionID,
        UINT32 *pReadMask,
        UINT32 *pWriteMask,
        UINT64 *pStartAddr,
        UINT64 *pEndAddr
    ) override;
    UINT32          GetKindTraits(INT32 pteKind) override;
    bool            IsFeatureEnabled(Feature feature) override;
    RC              LoadMmeShadowData() override;
    UINT32          MemoryStrap() override;

    static  bool    SetBootStraps(Gpu::LwDeviceId deviceId,
                                  const PciDevInfo& pciInfo, UINT32 boot0Strap,
                                  UINT32 boot3Strap, UINT64 fbStrap, UINT32 gc6Strap);
    void            SetPLLCfg(PLLType Pll, UINT32 cfg) override;
    void            SetPLLLocked(PLLType Pll) override;
    bool            IsPLLLocked(PLLType Pll) const override;
    bool            IsPLLEnabled(PLLType Pll) const override;
    void            GetPLLList(vector<PLLType> *pPllList) const override;
    RC              SetSmEccEnable(bool bEnable, bool bOverride) override;
    void            SetupBugs() override;
    void            SetupFeatures() override;

    UINT32          GetMmeInstructionRamSize() override;
    UINT32          GetMmeVersion() override;

    RC              PrepareEndOfSimCheck(Gpu::ShutDownMode mode) override;
    void            EndOfSimDelay() const override;
    bool            PollAllChannelsIdle(LwRm *pLwRm) override;

    RC              GetMaxCeBandwidthKiBps(UINT32 *pBwKiBps) override;
    UINT32          GetNumGraphicsRunqueues() override { return 2; }

    static bool     PollForFBStopAck(void *pVoidPollArgs);
    RC              AssertFBStop();
    void            DeAssertFBStop();
    bool            IsQuadroEnabled() override;
    bool            IsMsdecEnabled() override { return false; }
    UINT32          Io3GioConfigPadStrap() override;
    UINT32          RamConfigStrap() override;
    void            CreatePteKindTable() override;
    void            EnablePwrBlock() override;
    void            DisablePwrBlock() override;
    void            EnableSecBlock() override;
    void            DisableSecBlock() override;
    void            PrintMonitorInfo(Gpu::MonitorFilter monFilter) override;
    void            PrintMonitorInfo(RegHal * pRegs, Gpu::MonitorFilter monFilter) override;
    UINT32          NumAteSpeedo() const override;
    RC              GetAteIddq(UINT32 *pIddq, UINT32 *pVersion) override;
    RC              GetAteRev(UINT32* pRevision) override;
    UINT32          GetMaxCeCount() const override;
    UINT32          GetMmeStartAddrRamSize() override;
    UINT32          GetMmeDataRamSize() override;
    UINT32          GetMmeShadowRamSize() override;

    UINT32          GetMaxPceCount() const override;
    UINT32          GetMaxGrceCount() const override;
    UINT32          GetMaxOfaCount() const override;
    RC              GetPceLceMapping(LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS* pParams) override;
    UINT32          GetMaxSyspipeCount() const override;

    RC              GetH2jUnlockInfo(h2jUnlockInfo *pInfo);
    RC              GetSelwreKeySize(UINT32 *pChkSize, UINT32 *pCfgSize);

    UINT32          GetMaxZlwllCount() const override;

    bool            IsDisplayFuseEnabled() override;

    UINT32          GetTexCountPerTpc() const override;
    UINT32          UserStrapVal() override;
    UINT64          FramebufferBarSize() const override;
    bool            IsGpcGraphicsCapable(UINT32 hwGpcNum) const override;
    UINT32          GetGraphicsTpcMaskOnGpc(UINT32 hwGpcNum) const override;

    RC              SetFbMinimum(UINT32 fbMinMB) override;
    RC              SetBackdoorGpuPowerGc6(bool pwrOn) override;
    RC              PrintMiscFuseRegs() override;
    RC              DisableEcc() override;
    RC              EnableEcc() override;
    RC              DisableDbi() override;
    RC              EnableDbi() override;
    RC              SkipInfoRomRowRemapping() override;
    RC              DisableRowRemapper() override;

    RC              IlwalidateCompBitCache(FLOAT64 timeoutMs) override;
    UINT32          GetComptagLinesPerCacheLine() override;
    UINT32          GetCompCacheLineSize() override;
    UINT32          GetCbcBaseAddress() override;

    RC              GetVprSize(UINT64 *size, UINT64 *alignment) override;
    RC              GetAcr1Size(UINT64 *size, UINT64 *alignment) override;
    RC              GetAcr2Size(UINT64 *size, UINT64 *alignment) override;
    RC              SetVprRegisters(UINT64 size, UINT64 physAddr) override;

    RC              SetAcrRegisters
    (
        UINT64 acr1Size,
        UINT64 acr1PhysAddr,
        UINT64 acr2Size,
        UINT64 acr2PhysAddr
    ) override;
    RC              SetBackdoorGpuLinkL2(bool isL2) override;
    RC              SetBackdoorGpuAssertPexRst(bool isAsserted) override;
    RC              GetKappaInfo(UINT32* pKappa, UINT32* pVersion, INT32* pValid) override;
    void            EnableSMC() override { m_IsSmcEnabled = true; }
    void            DisableSMC() override  { m_IsSmcEnabled = false; };
    bool            IsSMCEnabled() const override { return m_IsSmcEnabled; }
    bool            IsRunlistRegister(UINT32 offset) const override;

    unique_ptr<PciCfgSpace> AllocPciCfgSpace() const override;

    RC SetupUserdWriteback(RegHal &regs) override;
    void StopVpmCapture() override;

    RC GetXbarSecondaryPortEnabled(UINT32 port, bool *pEnabled) const override;
    RC EnableXbarSecondaryPort(UINT32 port, bool enable) override;
    bool IsXbarSecondaryPortConfigurable() const override;

    RC GetKFuseStatus(KFuseStatus *pStatus, FLOAT64 timeoutMs) override;

    double GetIssueRate(Instr type) override;
    RC   CheckIssueRateOverride() override;
    RC   GetIssueRateOverride(IssueRateOverrideSettings *pOverrides) override;
    RC   IssueRateOverride(const IssueRateOverrideSettings& overrides) override;
    RC   IssueRateOverride(const string& unit, UINT32 throttle) override;

    RC   SetEccOverrideEnable(LW2080_ECC_UNITS eclwnit, bool bEnabled, bool bOverride) override;
    UINT08 GetFbEclwncorrInjectBitSeparation() override { return GetFB()->IsHbm() ? 4 : 1; }

    RC GetAndClearL2EccCounts
    (
        UINT32 ltc,
        UINT32 slice,
        L2EccCounts * pL2EccCounts
    ) override;
    RC SetL2EccCounts
    (
        UINT32 ltc,
        UINT32 slice,
        const L2EccCounts & l2EccCounts
    ) override;
    RC GetAndClearL2EccInterrupts
    (
        UINT32 ltc,
        UINT32 slice,
        bool *pbSbeInterrupt,
        bool *pbDbeInterrupt
    ) override;

    UINT32 NumRowRemapTableEntries() const override { return 8; }

    RC GetHbmLinkRepairRegister
    (
        const LaneError& laneError,
        UINT32* pOutRegAddr
    ) const override;
    RC GetIsHulkFlashed(bool* pHulkFlashed) override;

    RC CheckHbmIeee1500RegAccess(const HbmSite& hbmSite) const override;
    RC FbFlush(FLOAT64 TimeOutMs) override;

    void PostRmShutdown() override;

    RC GetPoisonEnabled(bool* pEnabled) override;
    RC OverridePoisolwal(bool bEnable) override;

    RC OverrideSmInternalErrorContainment(bool enSmInternalErrorContainment) override;
    RC OverrideGccCtxswPoisonContainment(bool enGccCtxswPoisonContainment) override;

    UINT32 GetHsHubPceMask() const override;
    UINT32 GetFbHubPceMask() const override;

    RC GetPerGpcClkMonFaults(UINT08 patitionIndex, UINT32 *pFaultMask) override;
    RC GetPerFbClkMonFaults(UINT08 partitionIndex, UINT32 *pFaultMask) override;
    bool IsSupportedNonHalClockMonitor(Gpu::ClkDomain clkDomain) override;
    RC GetNonHalClkMonFaultMask(Gpu::ClkDomain clkDom, UINT32 *pFaultMask) override;
    
    Fsp *GetFspImpl() const override { return m_pFspImpl.get(); }

    bool IsGFWBootStarted() override;
    RC PollGFWBootComplete(Tee::Priority pri) override;

    bool GpioIsOutput(UINT32 whichGpio) override;
    void GpioSetOutput(UINT32 whichGpio, bool level) override;
    bool GpioGetOutput(UINT32 whichGpio) override;
    bool GpioGetInput(UINT32 whichGpio) override;
    RC   SetGpioDirection(UINT32 gpioNum, GpioDirection direction) override;

protected:
    bool            ApplyFbCmdMapping
    (
        const string &FbCmdMapping,
        UINT32       *pPfbCfgBcast,
        UINT32       *pPfbCfgBcastMask
    ) const override;
    RC              DumpEccAddress() const override;
    RC              PrepareEndOfSimCheckImpl
    (
        Gpu::ShutDownMode mode,
        bool bWaitgfxDeviceIdle,
        bool bForceFenceSyncBeforeEos
    );
    bool            GetFbGddr5x8Mode();
    UINT32          GetUserdAlignment() const override;
    void            LoadIrqInfo
    (
        IrqParams* pIrqInfo,
        UINT08 irqType,
        UINT32 irqNumber,
        UINT32 maskInfoCount
    ) override;

    RC EnableHwDevice(GpuSubdevice::HwDevType hwType, bool bEnable) override;

    unique_ptr<ScopedRegister> EnableScopedBar1PhysMemAccess() override;
    unique_ptr<ScopedRegister> ProgramScopedPraminWindow(UINT64 addr, Memory::Location loc) override;

    FLOAT64 GetGFWBootTimeoutMs() const override;

    UINT32 GetDomainBase(UINT32 domainIdx, RegHalDomain domain, LwRm *pLwRm) override;

private:
    void            GetMemCfgOverrides();
    RC              InitFalconBases();
    unsigned        GetNumIntrTrees() const override;
    bool            IsPerSMCEngineReg(UINT32 offset) const override;
    void            SetSWIntrPending(unsigned intrTree, bool pending) override;
    void            SetIntrEnable(unsigned intrTree, IntrEnable state) override;
    UINT32          GetIntrStatus(unsigned intrTree) const override;
    bool          GetSWIntrStatus(unsigned intrTree) const override;
    RC              UnlockHost2Jtag(const vector<UINT32> &secCfgMask,
                                    const vector<UINT32> &selwreKeys) override;
    RC              UnlockHost2Jtag(h2jUnlockInfo *pUnlockInfo);
    RC              VerifyHost2JtagUnlockStatus(h2jUnlockInfo *pUnlockInfo);
    // These versions of Host2Jtag() support multiple jtag clusters on the same Host2Jtag
    // read/write operation.
    RC              ReadHost2Jtag
    (
        vector<JtagCluster> &jtagClusters  // vector of { chipletId, instrId } tuples
       ,UINT32 chainLen                    // number of bits in the chain
       ,vector<UINT32> *pResult            // location to store the data
       ,UINT32 capDis = 0                  // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit
       ,UINT32 updtDis = 0                 // sets the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit
    ) override;

    RC              WriteHost2Jtag
    (
        vector<JtagCluster> &jtagClusters // vector of { chipletId, instrId } tuples
       ,UINT32 chainLen                   // number of bits in the chain
       ,const vector<UINT32> &inputData   // location to store the data
       ,UINT32 capDis = 0                 // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit
       ,UINT32 updtDis = 0                // set the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit
    ) override;

    template<typename T> void ForEachFbpBySite(T func);
    vector<UINT32> GetSiteFbps(UINT32 site);
    bool           IsFbMc2MasterValid();
    UINT32         GetFbMc2MasterValue(UINT32 site, UINT32 siteFbpIdx);

    RC GetRunlistPriBases
    (
        LwRm * pLwRm,
        LwU32 engineCount,
        LW2080_CTRL_GPU_GET_ENGINE_RUNLIST_PRI_BASE_PARAMS *pBaseParams
    )   const;

    RC GetHwEngineIds
    (
        LwRm * pLwRm,
        LwU32 engineCount,
        LW2080_CTRL_GPU_GET_HW_ENGINE_ID_PARAMS *pHwIdParams
    );

    // Should never be able to get here, but implementation is required
    void PrintMonitorInfo() override { MASSERT(!"PrintMonitorInfo flags are required\n"); }
    void PrintRunlistInfo(RegHal * pRegs);
    vector<GpuSubdevice::HwDevInfo>::const_iterator GetHwEngineInfo(UINT32 hwEngineId);
    string GetLogicalEngineName(UINT32 engine);

    static const array<ModsGpuRegAddress, 8> m_Mc2MasterRegs;
    static constexpr UINT32 m_FbpsPerSite = 2;

    map<UINT32, UINT32>     m_FalconBases;
    vector <UINT32>         m_InpPerFbpRowBits;
    // TODO: m_FbDualRanks is deprecated in Hopper+.
    // Remove once tests have transitioned to use m_FbNumSids
    UINT32                  m_FbDualRanks;
    UINT32                  m_FbNumSids;
    UINT32                  m_FbRowAMaxCount;
    UINT32                  m_FbRowBMaxCount;
    UINT32                  m_FbMc2MasterMask;
    UINT32                  m_FbDualRequest;
    UINT32                  m_FbClamshell;
    bool                    m_IsSmcEnabled;
    UINT32                  m_FbBanksPerRowBg;
    UINT32                  m_LwLinksPerIoctl;

    unique_ptr<Fsp>         m_pFspImpl;
};
