/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_VOLTAGPU_H
#define INCLUDED_VOLTAGPU_H

#include "gpu/include/gpusbdev.h"
#include <array>

class VoltaGpuSubdevice : public GpuSubdevice
{
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

    VoltaGpuSubdevice(const char*, Gpu::LwDeviceId, const PciDevInfo*);
    virtual ~VoltaGpuSubdevice() { }

    virtual void            ApplyJtagChanges(UINT32 processFlag);
    virtual void            ApplyMemCfgProperties(bool InPreVBIOSSetup);
    virtual GpuPerfmon *    CreatePerfmon(void);
    virtual bool            DoesFalconExist(UINT32 falconId);
    virtual bool            FaultBufferOverflowed() const;
#ifdef LW_VERIF_FEATURES
    virtual void            FilterMemCfgRegOverrides
    (
        vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS>* pOverrides
    );
#endif
    virtual RC              GetAteSpeedo(UINT32 SpeedoNum, UINT32 *pSpeedo, UINT32 *pVersion);
    virtual RC              GetAteSramIddq(UINT32 *pIddq);
    virtual RC              GetSramBin(INT32* pSramBin);
    virtual RC              GetFalconStatus(UINT32 falconId, UINT32 *pLevel);
    virtual UINT32          GetPLLCfg(PLLType Pll) const;
    virtual RC              GetSmEccEnable(bool * pEnable, bool * pOverride) const;
    virtual RC              GetStraps(vector<pair<UINT32, UINT32> >* pStraps);

    virtual RC              GetWPRInfo
    (
        UINT32 regionID,
        UINT32 *pReadMask,
        UINT32 *pWriteMask,
        UINT64 *pStartAddr,
        UINT64 *pEndAddr
    );
    virtual bool            IsFeatureEnabled(Feature feature);
    virtual RC              LoadMmeShadowData();
    virtual UINT32          MemoryStrap();

    virtual void            PrintVoltaMonitorInfo(UINT32 Flags);
    static  bool            SetBootStraps(Gpu::LwDeviceId deviceId,
                                        void * LinLwBase, UINT32 boot0Strap,
                                        UINT32 boot3Strap, UINT64 fbStrap, UINT32 gc6Strap);
    virtual void            SetPLLCfg(PLLType Pll, UINT32 cfg);
    virtual void            SetPLLLocked(PLLType Pll);
    virtual bool            IsPLLLocked(PLLType Pll) const;
    virtual bool            IsPLLEnabled(PLLType Pll) const;
    virtual void            GetPLLList(vector<PLLType> *pPllList) const;
    virtual RC              SetSmEccEnable(bool bEnable, bool bOverride);
    virtual void            SetupBugs();
    virtual void            SetupFeatures();

    virtual UINT32          GetMmeInstructionRamSize();
    virtual UINT32          GetMmeVersion();

    virtual RC              PrepareEndOfSimCheck(Gpu::ShutDownMode mode);

    virtual RC              GetMaxCeBandwidthKiBps(UINT32 *pBwKiBps);
    virtual UINT32          GetNumGraphicsRunqueues() { return 2; }

    static bool             PollForFBStopAck(void *pVoidPollArgs);
    virtual RC              AssertFBStop();
    virtual void            DeAssertFBStop();
    virtual bool            IsQuadroEnabled();
    virtual bool            IsMsdecEnabled() { return false; }
    virtual UINT32          Io3GioConfigPadStrap();
    virtual UINT32          RamConfigStrap();
    virtual void            CreatePteKindTable();
    void                    AddCommolwoltaPteKinds();
    virtual void            EnablePwrBlock();
    virtual void            DisablePwrBlock();
    virtual void            EnableSecBlock();
    virtual void            DisableSecBlock();
    virtual void            PrintMonitorInfo();

    virtual UINT32          NumAteSpeedo() const;
    virtual RC              GetAteIddq(UINT32 *pIddq, UINT32 *pVersion);
    virtual RC              GetAteRev(UINT32* pRevision);
    virtual UINT32          GetMaxCeCount() const;
    virtual UINT32          GetMaxPceCount() const;
    virtual UINT32          GetMmeShadowRamSize();

    virtual RC              GetH2jUnlockInfo(h2jUnlockInfo *pInfo);
    virtual RC              GetSelwreKeySize(UINT32 *pChkSize, UINT32 *pCfgSize);

    virtual UINT32          GetMaxZlwllCount() const;

    virtual bool            IsDisplayFuseEnabled();

    virtual UINT32          GetTexCountPerTpc() const;
    virtual UINT32          UserStrapVal();
    virtual UINT64          FramebufferBarSize() const;

    virtual RC              SetFbMinimum(UINT32 fbMinMB);
    virtual RC              SetBackdoorGpuPowerGc6(bool pwrOn);
    virtual RC              PrintMiscFuseRegs();
    virtual RC              DisableEcc();
    virtual RC              EnableEcc();
    virtual RC              DisableDbi();
    virtual RC              EnableDbi();

    virtual RC              IlwalidateCompBitCache(FLOAT64 timeoutMs);
    virtual UINT32          GetComptagLinesPerCacheLine();
    virtual UINT32          GetCompCacheLineSize();
    virtual UINT32          GetCbcBaseAddress();

    virtual RC              PostRmInit();

    virtual RC GetVprSize(UINT64 *size, UINT64 *alignment);
    virtual RC GetAcr1Size(UINT64 *size, UINT64 *alignment);
    virtual RC GetAcr2Size(UINT64 *size, UINT64 *alignment);
    virtual RC SetVprRegisters(UINT64 size, UINT64 physAddr);

    virtual RC SetAcrRegisters
    (
        UINT64 acr1Size,
        UINT64 acr1PhysAddr,
        UINT64 acr2Size,
        UINT64 acr2PhysAddr
    );

    virtual RC HaltAndExitPmuPreOs();
protected:
    enum PrintVoltaMonitorInfoFlags  //!< Passed to PrintMaxwellMonitorInfo
    {
        MMONINFO_NO_VIDEO = 0x01, //!< Don't print video info
    };
    virtual bool ApplyFbCmdMapping(const string &FbCmdMapping,
                                   UINT32 *pPfbCfgBcast,
                                   UINT32 *pPfbCfgBcastMask) const;
    virtual RC DumpEccAddress() const;
    virtual RC PrepareEndOfSimCheckImpl(Gpu::ShutDownMode mode,
                                        bool bWaitgfxDeviceIdle,
                                        bool bForceFenceSyncBeforeEos);
    virtual bool GetFbGddr5x8Mode();
    virtual UINT32 GetUserdAlignment() const;
    virtual RC              UnlockHost2Jtag(h2jUnlockInfo *pUnlockInfo);

    static const array<ModsGpuRegAddress, 8> m_Mc2MasterRegs;
    static constexpr UINT32 m_FbpsPerSite = 2;
    vector <UINT32>         m_InpPerFbpRowBits;
    UINT32                  m_FbDualRanks;
    UINT32                  m_FbMc2MasterMask;
    virtual void            LoadIrqInfo(IrqParams* pIrqInfo, UINT08 irqType,
                                        UINT32 irqNumber, UINT32 maskInfoCount);

private:
    void                    GetMemCfgOverrides();
    virtual RC              InitFalconBases();
    virtual unsigned        GetNumIntrTrees() const;
    virtual void            SetSWIntrPending(unsigned intrTree, bool pending);
    virtual void            SetIntrEnable(unsigned intrTree, IntrEnable state);
    virtual RC              UnlockHost2Jtag( const vector<UINT32> &secCfgMask,
                                             const vector<UINT32> &selwreKeys);
    virtual RC              VerifyHost2JtagUnlockStatus(h2jUnlockInfo *pUnlockInfo);
    // These versions of Host2Jtag() support multiple jtag clusters on the same Host2Jtag
    // read/write operation.
    virtual RC  ReadHost2Jtag(
                    vector<JtagCluster> &jtagClusters// vector of { chipletId, instrId } tuples
                    ,UINT32 chainLen                 // number of bits in the chain
                    ,vector<UINT32> *pResult         // location to store the data
                    ,UINT32 capDis = 0           // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit
                    ,UINT32 updtDis = 0);        // sets the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit

    virtual RC  WriteHost2Jtag(
                    vector<JtagCluster> &jtagClusters// vector of { chipletId, instrId } tuples
                    ,UINT32 chainLen                 // number of bits in the chain
                    ,const vector<UINT32> &inputData // location to store the data
                    ,UINT32 capDis = 0           // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit
                    ,UINT32 updtDis = 0);        // set the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit

    bool IsFbMc2MasterValid();
    UINT32 GetFbMc2MasterValue(size_t fbp);

    map<UINT32, UINT32>     m_FalconBases;
};

#endif // INCLUDED_VOLTAGPU_H
