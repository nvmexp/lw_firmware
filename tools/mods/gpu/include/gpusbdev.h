/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/framebuf.h"
#include "core/include/hbmtypes.h"
#include "core/include/ismspeedo.h"
#include "core/include/jsonlog.h"
#include "core/include/memerror.h"
#include "core/include/memtypes.h"
#include "core/include/lwrm.h"
#include "core/include/pci.h"
#include "core/include/tasker.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl208f.h"
#include "device/interface/i2c.h"
#include "device/interface/lwlink.h"
#include "gcximpl.h"
#include "gpu/include/testdevice.h"
#include "gpu/reghal/reghal.h"
#include "gpu/utility/blacklist.h"
#include "gpu/utility/eclwtil.h"
#include "gpu/utility/pcie/pexdev.h"
#include "mods_reg_hal.h"
#include "plllockrestriction.h"
#include "rm.h"

#ifdef LW_VERIF_FEATURES
#include "Lwcm.h"  // for LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS
#endif

#include <atomic>
#include <functional>

// WAR(MODSAMPERE-1222): Including vmiopelw.h causes definition conflicts due to
// mods_kd_api.h defines.
// #include "gpu/vmiop/vmiopelw.h"
class VmiopElw;

using namespace IsmSpeedo;

class EventThread;
class GpuDevice;
class PMU;
class Sec2Rtos;
class Thermal;
class Perf;
class Power;
class Fuse;
class PexDevice;
class Surface2D;
class ClockThrottle;
class EccErrCounter;
class EdcErrCounter;
class GpuPerfmon;
class AzaliaController;
class GpuIsm;
class SubdeviceGraphics;
class SubdeviceFb;
class FalconEcc;
class HsHubEcc;
class PciBusEcc;
class SpeedoImpl;
class PciCfgSpace;
class FloorsweepImpl;
class RefManualRegister;
class Avfs;
class Volt3x;
class HBMImpl;
class SmcRegHal;
class ScopedRegister;
class Fsp;

class TempCtrl;
class ImpiTempCtrl;
typedef shared_ptr<TempCtrl> TempCtrlPtr;
typedef map<UINT32, TempCtrlPtr> TempCtrlMap;

#define ILWALID_SUBDEVICE 0xffffffff

#define PTE_KIND_TABLE_ENTRY(name)  PteKindTableEntry(LW_MMU_PTE_KIND_##name, #name)

typedef LW2080_CTRL_FB_GET_CALIBRATION_LOCK_FAILED_PARAMS FbCalibrationLockCount;
typedef LW208F_CTRL_GPU_RAM_SVOP_VALUES_PARAMS SvopValues;

class GpuSubdevice :
    public TestDevice
  , public PllLockRestrictionCheck
{
    using HbmSite = Memory::Hbm::Site;

public:
    INTERFACE_HEADER(GpuSubdevice);

    // RAII class for disabling the power gating check during register access
    // Declaring a class of this type will disable the power gating check
    // during register access until the class is destroyed (at which point
    // the old value is restored)
    class DontCheckForPowerGatingDuringRegAccess
    {
    public:
        DontCheckForPowerGatingDuringRegAccess(GpuSubdevice * pGpuSub) :
            m_pGpuSub(pGpuSub)
        {
            m_SavedEnable =
                m_pGpuSub->EnablePowerGatingTestDuringRegAccess(false);
        }
        ~DontCheckForPowerGatingDuringRegAccess()
        {
            m_pGpuSub->EnablePowerGatingTestDuringRegAccess(m_SavedEnable);
        }
    private:
        GpuSubdevice * m_pGpuSub;
        bool m_SavedEnable;
    };

    // Class for creating a named GpuSubdevice property at runtime.  Not all
    // GPUs will contain all named properties
    class NamedProperty
    {
    public:
        NamedProperty();
        NamedProperty(bool   bDefault);
        NamedProperty(UINT32 uDefault);
        NamedProperty(UINT32 size, UINT32 uDefault);
        NamedProperty(const string &sDefault);

        UINT32 GetArraySize();
        RC Set(bool Value);
        RC Set(UINT32 Value);
        RC Set(const vector<UINT32> &Values);
        RC Set(const string &Value);

        RC Get(bool   *pValue) const;
        RC Get(UINT32 *pValue) const;
        RC Get(vector<UINT32> *pValues) const;
        RC Get(string *Value) const;

        bool IsSet() const { return m_IsSet; }
    private:
        enum NamedPropertyType
        {
            Uint32,
            Bool,
            Array,
            String,
            Invalid
        };
        NamedPropertyType m_Type;
        vector<UINT32>   m_Data;
        string           m_sData;

        // m_IsSet is true IFF the value of the property was explicitly set
        //
        bool             m_IsSet;

        RC ValidateType(NamedPropertyType type, string function) const;
    };

    struct MmeMethod
    {
        UINT32 ShadowClass;
        UINT32 ShadowMethod;
        bool operator<(const MmeMethod &rhs) const;
    };
    typedef map<MmeMethod, bool> MmeMethods;

public:
    GpuSubdevice(const char *Name, Gpu::LwDeviceId DeviceId,
                 const PciDevInfo* pPciDevInfo);
    virtual ~GpuSubdevice();

    virtual RC EarlyInitialization();
    virtual void Reset();
    RC Shutdown() override;
    RC FLReset() override;
    RC HotReset(FundamentalResetOptions funResetOption = FundamentalResetOptions::Default) override;
    RC PexReset() override { return RC::UNSUPPORTED_FUNCTION; }
    void ReassignPciDev(const PciDevInfo& pciDevInfo);
    bool IsSamePciAddress(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function) const;

    void         DestroyInRm();
    virtual void PostRmShutdown();
    void DumpMicronGDDR6Info();

    virtual RC GetBoardBOM(UINT32* pVal);

    struct GpuSensorInfo
    {
        LW2080_CTRL_PERF_PERF_CF_SENSORS_INFO     sensors;
        LW2080_CTRL_PERF_PERF_CF_TOPOLOGYS_INFO   info;
        LW2080_CTRL_PERF_PERF_CF_TOPOLOGYS_STATUS status;
        bool queried = false;
    };

    float GetGpuUtilizationFromSensorTopo(const GpuSensorInfo &gpuSensorInfo, UINT32 idx);
    RC SetGpuUtilSensorInformation();
    RC SetGpuUtilStatusInformation();
    GpuSubdevice::GpuSensorInfo GetGpuUtilSensorInformation();


    enum class Trigger
    {
        INIT_SHUTDOWN,       // standard MODS init/shutdown
        STANDBY_RESUME,      // StandBy/Resume for gpu reset tests
        RECOVER_FROM_RESET   // Legacy reset support for trace_3d/lwsr system check tests
    };

    enum PteKindTraits
    {
        Ilwalid_Kind = 0x0,
        Compressible = 0x1,     //KIND_COMPRESSIBLE(k) in kind_macro.h
        Color = 0x2,            //KIND_C(k)
        Z = 0x4,                //KIND_Z(k)
        Z24S8 = 0x8,            //KIND_Z24S8(k)
        Pitch = 0x10,            //KIND_PITCH(k)
        Generic_16BX2 = 0x20,   //LW_MMU_PTE_KIND_GENERIC_16BX2 in dev_mmu.h
        Generic_Memory = 0x40,  //LW_MMU_PTE_KIND_GENERIC_MEMORY
    };

    virtual UINT32 GetKindTraits(INT32 pteKind) {return Ilwalid_Kind;};

    RC InitGpuInRm(bool skipRmStateInit, bool forceRepost, bool skipVbiosPost,
                   bool hotSbrRecovery);
    RC Initialize() override;
    RC Initialize(Trigger trigger);
    virtual RC Initialize(GpuDevice *parent, UINT32 subdevInst);
    virtual void ShutdownSubdevice();
    void ShutdownSubdevice(Trigger trigger);
    virtual void EnableInterruptRouting() {};
    RC GetVBiosImage(vector<UINT08>* pBiosImage) const;
    RC GetNetlistImage(vector<UINT08>* pNetlistImage) const;
    static RC LoadImageFromFile(const string& filename, vector<UINT08>* pImage);
    static RC GetSocBiosImage(const string& filename, vector<UINT08>* pBiosImage);
    RC ValidateBiosImage(const string& filename, vector<UINT08>* pBiosImage) const;
    RC GetVBiosImageHash(vector<UINT08>* pBiosImageHash) const;

    string DeviceIdString() const override { return Gpu::DeviceIdToString(DeviceId()); }
    string GetClassName() const;
    RC GetChipPrivateRevision(UINT32 *pPrivRev) override;
    RC GetChipRevision(UINT32 *pRev) override;
    RC GetChipSubRevision(UINT32 *pRev) override;
    RC GetChipSkuModifier(string *pStr) override;
    RC GetChipSkuNumber(string *pStr) override;
    RC GetChipTemps(vector<FLOAT32> *pTempsC) override;
    RC GetClockMHz(UINT32 *pClkMhz) override { return RC::UNSUPPORTED_FUNCTION; }
    RC GetDeviceErrorList(vector<DeviceError>* pErrorList) override;
    RC GetEcidString(string* pStr) override;
    RC GetFoundry(ChipFoundry *pFoundry) override;
    string GetName() const override;
    RC GetRomVersion(string* pVersion) override;
    RC GetTestPhase(UINT32* pPhase) override;
    Type GetType() const override { return TYPE_LWIDIA_GPU; }
    RC GetVoltageMv(UINT32 *pMv) override { return RC::UNSUPPORTED_FUNCTION; }
    bool IsModsInternalPlaceholder() const override { return false; }
    bool IsSysmem() const override { return false; }
    bool IsEndpoint() const override { return true; }
    INT32 GetNumaNode() const;
    void Print(Tee::Priority pri, UINT32 code, string prefix) const override;

    TestDevicePtr GetTestDevice();

    RC GetFlaCapable(bool* bCapable) const;

    enum ResetControls : UINT32
    {
        DefaultReset = 0,
        DoStandby = (1 << 0),
        DoResume = (1 << 1),
        DoZpiReset = (1 << 2),
        GpuIsLost = (1 << 3)
    };

    RC AllocClientStuff() { return AllocClientStuff(DefaultReset); };
    RC AllocClientStuff(UINT32 resetControlsMask);
    RC FreeClientStuff() { return FreeClientStuff(DefaultReset); }
    RC FreeClientStuff(UINT32 resetControlsMask);

    RC StandBy();
    RC Resume();
    bool IsInStandBy() const { return m_InStandBy; }

    virtual RC SetBackdoorGpuPowerGc6(bool pwrOn);
    virtual RC SetBackdoorGpuLinkL2(bool isL2);
    virtual RC SetBackdoorGpuAssertPexRst(bool isAsserted);

    RC EnterGc6(GC6EntryParams &entryParams);
    RC ExitGc6(GC6ExitParams &exitParams);
    void InGc6(bool inGc6) {m_InGc6 = inGc6;}
    bool IsInGc6() const {return m_InGc6;}

    void PausePollInterrupts()    { m_PausePollInterrupts = true; }
    void ResumePollInterrupts()   { m_PausePollInterrupts = false; }
    bool IsPausedPollInterrupts() { return m_PausePollInterrupts; }

    virtual bool IsGFWBootStarted();
    virtual RC PollGFWBootComplete(Tee::Priority pri);
    virtual RC ReportGFWBootMemoryErrors(Tee::Priority pri);
    virtual RC SetGFWBootHoldoff(bool holdoff);
    virtual bool GetGFWBootHoldoff();

    enum HookedIntrType
    {
        HOOKED_NONE = 0,
        HOOKED_INTA = 1,
        HOOKED_MSI  = 2,
        HOOKED_MSIX = 4
    };
    RC HookInterruptsByType(HookedIntrType intrType);
    RC HookInterrupts(UINT32 irqType);
    RC UnhookISR();
    virtual void RearmMSI();
    virtual void RearmMSIX();
    RC ServiceInterrupt(UINT32 irq);
    bool InterruptsHooked() const { return m_HookedIntr != HOOKED_NONE; }

    const FbRanges& GetCriticalFbRanges();
    void ClearCriticalFbRanges() { m_CriticalFbRanges.clear(); }
    void AddCriticalFbRange(UINT64 Start, UINT64 Length);

    RC ValidateInterrupts(UINT32 interruptMask);
    RC ValidateSOCInterrupts();

    RC SetSubdeviceInst(UINT32 subdevInst);
    RC SetParentDevice(GpuDevice *parent);
    UINT32 GetSubdeviceInst() const{ return m_Subdevice; }

    LwRm::Handle GetSubdeviceDiag ()const {return m_SubdeviceDiag; }
    LwRm::Handle GetZbc();
    GpuDevice *GetParentDevice() const{ return m_Parent; }
    AzaliaController* GetAzaliaController() const;
    virtual bool IsAzaliaMandatory() const;

    void SetFbHeapSize(UINT64 val);
    UINT64 FbHeapSize() const { return m_FbHeapSize; }

    void SetL2CacheOnlyMode(bool val);
    bool getL2CacheOnlyMode() const { return m_L2CacheOnlyMode; }

    enum UsableKinds
    {
        useCompressed       = 1
        ,useDisplayable     = 2
        ,useProtected       = 4
        ,useNonProtected    = 8
    };
    UINT64 UsableFbHeapSize(UINT32 kinds) const;

    bool IsSOC() const override { return m_pPciDevInfo->SocDeviceIndex != ~0U; }
    bool IsFakeTegraGpu() const
    {
#if LWCFG(GLOBAL_CHIP_T194)
        return DeviceId() == Gpu::Txxx;
#else
        return false;
#endif
    }
    bool IsHookMsiSupported() const;
    bool IsHookMsixSupported() const;
    bool IsHookIntSupported() const;
    bool IsHookIrqSupported() const;
    static const char *GetIRQTypeName(UINT32 irqType);
    UINT32 GetNumIRQs(UINT32 irqType);
    UINT32 GetSocDeviceIndex() const { return m_pPciDevInfo->SocDeviceIndex; }
    PHYSADDR GetPhysLwBase() const { return m_pPciDevInfo->PhysLwBase; }
    PHYSADDR GetPhysFbBase() const { return m_pPciDevInfo->PhysFbBase; }
    PHYSADDR GetPhysInstBase() const { return m_pPciDevInfo->PhysInstBase; }
    UINT32 GetIrq() const { return m_Irqs.empty() ? m_pPciDevInfo->Irq : m_Irqs[0]; }
    UINT32 GetNumIrqs() const { return (UINT32)m_Irqs.size(); }
    UINT32 GetIrq(UINT32 irqNum) const { return irqNum >= m_Irqs.size() ? ~0U : m_Irqs[irqNum]; }
    HookedIntrType GetHookedIntrType() const { return m_HookedIntr; }
    bool HasIrq(UINT32 irq) const;
    UINT32 GetMsiCapOffset() const { return m_pPciDevInfo->MsiCapOffset; }
    UINT32 GetMsixCapOffset() const { return m_pPciDevInfo->MsixCapOffset; }
    bool GetSbiosBoot() const { return m_pPciDevInfo->SbiosBoot != 0; }
    bool IsPrimary() const { return m_pPciDevInfo->SbiosBoot != 0; }
    bool IsVgaSupported() const { return m_pPciDevInfo->PciClassCode == Pci::CLASS_CODE_VIDEO_CONTROLLER_VGA; }
    UINT32 GetGpuInst() const { return m_pPciDevInfo->GpuInstance; }
    virtual UINT32 GetDriverId() const { return GetGpuInst(); }
    PHYSADDR GetDmaBaseAddress() const { return m_pPciDevInfo->DmaBaseAddress; }
    bool IsUefiBoot() const { return m_pPciDevInfo->UefiBoot != 0; }
    string GetBootInfoString() const;

    // We need to be able to set GPU instance after GPU subdevice is created
    // in order to be able to sort them in the GPU manager.
    // The implementation of this is inside GPU manager so that nobody else
    // can do it.
    class GpuInstSetter;
    GpuInstSetter& GetGpuInstSetter();

    void SetGpuId(UINT32 GpuId) { m_GpuId = GpuId; }
    UINT32 GetGpuId() const { return m_GpuId; }

    Gpu::LwDeviceId DeviceId() const { return m_DeviceId; }
    Device::LwDeviceId GetDeviceId() const override { return static_cast<Device::LwDeviceId>(DeviceId()); }
    UINT32 PciDeviceId() const { return m_pPciDevInfo->PciDeviceId; }

    RC SetClock(Gpu::ClkDomain clkDomain, UINT64 targetHz);
    virtual RC SetClock(Gpu::ClkDomain clkDomain,
                        UINT64 targetHz,
                        UINT32 PStateNum,
                        UINT32 flags);

    /* The difference between GetClock, Perf::MeasureClock, and MeasureCoreClock
     * GetClock
     *   The values returned by GetClock are based on the actual discretized
     *   values that the clock sources are generating.
     * MeasureClock
     *   MeasureClock and MeasureCoreClock both obtain their frequency based
     *   on clock pulse counters. RM measures the number of clock pulses
     *   on a clock output over a known period of time and then callwlates
     *   the frequency.
     *
     * The way to think about GetClock vs MeasureClock is the difference between
     * callwlating a clock frequency given the hardware specification and
     * settings vs actually measuring the clock.
     * For example, on a microcontroller you often have direct control over
     * the clock programming registers. Using an equation in the manual, you
     * can determine what the output clock frequency is *supposed* to be. This
     * is analogous to the values returned by GetClock. If you took an
     * oscilloscope and measured the clock signal, that would be analogous
     * to the values returned by MeasureClock.
     */

    //! GetClock: Gets frequency of a clock domain in three types
    //! Note: pTargetPllHz, pActualPllHz, pSlowedHz will be the same on Android
    //!   pTargetPllHz
    //!     The frequency we want to set for this clock domain.
    //!     As of June 2015 will return the same value as ActualPllHz. See RM
    //!     function clkGetInfos_CLK2 for proof.
    //!   pActualPllHz
    //!     The frequency that the PLL is programmed to give us. Due to constraints
    //!     in the actual PLL hardware, the actual frequency can only be set to
    //!     certain values (for example, because a divider has only so many bits
    //!     of precision).
    //!   pSlowedHz
    //!     Same as ActualPllHz, unless slowed down by some event (eg thermal)
    //!   pSrc
    //!     Returns the source for the given clock (eg which PLL)
    virtual RC GetClock(Gpu::ClkDomain which, UINT64 * pActualPllHz,
                        UINT64 * pTargetPllHz, UINT64 * pSlowedHz, Gpu::ClkSrc * pSrc);
    // Deprecated starting with Volta, use GetClockDomainFreqKHz().
    RC GetClockSourceFreqKHz(UINT32 clk2080ClkSource, LwU32 *clkSrcFreqKHz);
    RC GetClockDomainFreqKHz(UINT32 clk2080ClkDomain, LwU32 *clkDomainFreqKHz);

    RC GetPllCount(Gpu::ClkDomain clkToGet, UINT32 *pCount);

    static UINT32 ClkSrcToCtrl2080ClkSrc(Gpu::ClkSrc src);
    Gpu::ClkSrc Ctrl2080ClkSrcToClkSrc(UINT32 src);

    virtual RC SetSysPll(UINT64 targetHz);

    virtual RC GetWPRInfo(UINT32 regionID, UINT32 *pReadMask,
       UINT32 *pWriteMask, UINT64 *pStartAddr, UINT64 *pEndAddr);

    virtual RC GetFalconStatus(UINT32 falconId, UINT32 *pLevel);
    virtual bool DoesFalconExist(UINT32 falconId){return false;}

    enum NotifierMemAction
    {
        NOTIFIER_MEMORY_DISABLED = 0
       ,NOTIFIER_MEMORY_ENABLED  = 1
    };
    RC HookResmanEvent(
            UINT32 Index,
            void   (*Handler)(void*),
            void* args,
            UINT32 Action,
            NotifierMemAction notifierMemAction);
    RC UnhookResmanEvent(UINT32 Index);
    RC GetResmanEventData(UINT32 Index, LwNotification* pNotifierData) const;
    bool IsResmanEventHooked(UINT32 index) const;

    virtual RC SmidToHwGpcHwTpc(UINT32 VirtualGpcNum, UINT32 *pHwGpcNum, UINT32 *pHwTpcNum) const;
    virtual RC LocalGpcToHwGpc(UINT32 partIdx, UINT32 smcEngineIdx,
                               UINT32 localGpcNum, UINT32 *pHwGpcNum) const;
    virtual RC VirtualGpcToHwGpc(UINT32 VirtualGpcNum, UINT32 *pHwGpcNum) const;
    virtual RC HwGpcToVirtualGpc(UINT32 HwGpcNum, UINT32 *pVirtualGpcNum) const;
    RC HwGpcToVirtualGpcMask(UINT32 HwGpcMask, UINT32 *pVirtualGpcMask) const;
    virtual RC VirtualTpcToHwTpc(UINT32 HwGpcNum, UINT32 VirtualTpcNum,
                                 UINT32 *pHwTpcNum) const;
    virtual RC HwTpcToVirtualTpc(UINT32 HwGpcNum, UINT32 HwTpcNum,
                                 UINT32 *pVirtualTpcNum) const;

    virtual UINT32 RopCount() const;
    virtual UINT32 RopPixelsPerClock() const;
    virtual UINT32 ShaderCount() const;
    virtual UINT32 VpeCount() const;
    UINT32 GetFrameBufferUnitCount() const;
    UINT32 GetFrameBufferIOCount() const;
    UINT32 GetGpcCount() const;
    //! \brief Get the nuumber of physical SYS partitions on a gpu
    //!
    //! SYS are collections of common HW code irrespective to GPC.
    //! They include host/display/FE/PEX/PLLs and other miscelanious partitions.
    //! On gf10x gpus, each of these partitions has a speedo register, which is
    //! the reason for this method's existence.
    virtual UINT32 GetSysPartitionCount();
    virtual UINT32 GetSpeedoCount();
    virtual UINT32 GetMaxGpcCount() const;
    virtual UINT32 GetTpcCount() const;
    virtual UINT32 GetTpcCountOnGpc(UINT32 VirtualGpcNum) const;
    virtual UINT32 GetPesCountOnGpc(UINT32 VirtualGpcNum) const;
    virtual UINT32 GetMaxShaderCount() const;
    virtual UINT32 GetMaxSmPerTpcCount() const;
    virtual UINT32 GetMaxTpcCount() const;
    virtual UINT32 GetMaxPesCount() const;
    virtual UINT32 GetMaxZlwllCount() const;
    virtual UINT32 GetMaxFbpCount() const;
    virtual UINT32 GetTexCountPerTpc() const;
    virtual UINT32 GetMaxPceCount() const;
    virtual UINT32 GetMaxGrceCount() const;
    virtual UINT32 GetMaxLwencCount() const;
    virtual UINT32 GetMaxLwdecCount() const;
    virtual UINT32 GetMaxLwjpgCount() const;
    virtual UINT32 GetMaxL2Count() const;
    virtual UINT32 GetMaxOfaCount() const;
    virtual UINT32 GetMaxSyspipeCount() const;
    virtual UINT32 GetMaxLwlinkCount() const;
    virtual UINT32 GetMaxPerlinkCount() const;
    UINT32 GetMaxHsHubCount() const;
    virtual UINT32 GetMmuCountPerGpc(UINT32 virtualGpc) const;
    virtual bool   SupportsReconfigTpcMaskOnGpc(UINT32 hwGpcNum) const { return false; }
    virtual UINT32 GetReconfigTpcMaskOnGpc(UINT32 HwGpcNum) const { return 0; }
    // pre-Hopper all GPCs are Graphics-capable
    virtual bool IsGpcGraphicsCapable(UINT32 hwGpcNum) const { return true; }
    virtual INT32 GetFullGraphicsTpcMask(UINT32 hwGpcNum) const;
    virtual UINT32 GetGraphicsTpcMaskOnGpc(UINT32 hwGpcNum) const;
    RC ValidatePreInitRmRegistries() const;

    //!
    //! \brief Return a collection of virtual GPCs that are active, meaning
    //! assigned to an SMC engine.
    //!
    //! Can only be used when SMC is enabled.
    //!
    virtual vector<UINT32> GetActiveSmcVirtualGpcs() const;

    virtual RC GetPceLceMapping(LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS* pParams);
    virtual UINT32 GetHsHubPceMask() const
        { return GetFsImpl()->PceMask(); }
    virtual UINT32 GetFbHubPceMask() const
        { return 0; }

    enum class Instr
    {
        UNKNOWN,
        FFMA,                // FFMA FP32 dst <- FP32 src
        DFMA,                // DFMA FP64 dst <- FP64 src
        DMMA_884,            // DMMA FP64 dst <- FP64 src
        DMMA_16816,          // DMMA FP64 dst <- FP64 src
        HFMA2,               // HFMA2 FP16 dst <- FP16 src
        HMMA_884_F16_F16,    // HMMA FP16 dst <- FP16 src
        HMMA_884_F32_F16,    // HMMA FP32 dst <- FP16 src
        HMMA_1688_F16_F16,   // HMMA FP16 dst <- FP16 src
        HMMA_1688_F32_F16,   // HMMA FP32 dst <- FP16 src
        HMMA_1688_F32_BF16,  // HMMA FP32 dst <- BF16 src
        HMMA_16816_F32_BF16, // HMMA FP32 dst <- BF16 src
        HMMA_1684_F32_TF32,  // HMMA FP32 dst <- TF32 src
        HMMA_1688_F32_TF32,  // HMMA FP32 dst <- TF32 src
        HMMA_16816_F16_F16,  // HMMA FP16 dst <- FP16 src
        HMMA_16816_F32_F16,  // HMMA FP32 dst <- FP16 src
        HMMA_16832SP_F32_F16,// Sparse HMMA  FP32 dst <- FP16 src
        HGMMA_F16_F16,       // HGMMA FP16 dst <- FP16 src
        HGMMA_F32_F16,       // HGMMA FP32 dst <- FP16 src
        HGMMA_F32_BF16,      // HGMMA FP32 dst <- BF16 src
        HGMMA_F32_TF32,      // HGMMA FP32 dst <- TF32 src
        HGMMA_SP_F16_F16,    // Sparse HGMMA FP16 dst <- FP16 src
        HGMMA_SP_F32_F16,    // Sparse HGMMA FP32 dst <- FP16 src
        IDP4A_S32_S8,        // IDP4A INT32 dst <- INT8 src
        IMMA_8816_S32_S8,    // IMMA  INT32 dst <- INT8 src
        IMMA_16832_S32_S8,   // IMMA  INT32 dst <- INT8 src
        IMMA_16864SP_S32_S8, // Sparse IMMA INT32 dst <- INT8 src
        IMMA_16864_S32_S4,   // IMMA  INT32 dst <- INT4 src
        IGMMA_S32_S8,        // IGMMA INT32 dst <- INT8 src
        IGMMA_SP_S32_S8,     // Sparse IGMMA INT32 dst <- INT8 src
        BMMA_XOR_168256_S32_B1, // BMMA (XOR) INT32 dst <- INT1 src
        BMMA_AND_168256_S32_B1, // BMMA (AND) INT32 dst <- INT1 src
    };

    //! Get efficiency of an instruction for a given data type used by LwdaLinpack
    //!
    //! The values are taken from decoder ring.  Often the values differ between
    //! chips within a family and can also depend on other factors like fuses.
    //!
    //! The function should call GetIssueRate() to take fuses into account.
    //!
    //! @return GOPS (giga-ops per second) per SM at 1 GHz.  Returns zero if the
    //!         value is not known, not supported or hasn't been updated for a new chip.
    virtual double GetArchEfficiency(Instr type);

    //! Get issue rate of an instruction for a given data type used by LwdaLinpack
    //!
    //! @return Issue rate as a fraction of top speed, e.g. 0.5 or 0.25, depending
    //!         on fuses.
    virtual double GetIssueRate(Instr type);

    virtual bool   GetFbMs01Mode();
    virtual bool   GetFbGddr5x16Mode();
    virtual UINT32 GetFbBankSwizzle();
    virtual bool   GetFbBankSwizzleIsSet();
    virtual string GetFermiGpcTileMap();
    RC             ExelwtePreGpuInitScript();
    RC             GetMemoryBandwidthBps(UINT64* pBps);
    // Returns the maximum no. of CE units present in hardware
    virtual UINT32 GetMaxCeCount() const;
    virtual RC     GetMaxCeBandwidthKiBps(UINT32 *pBwKiBps);
    struct IssueRateOverrideSettings
    {
        UINT32 speedSelect0 = 0;
        UINT32 speedSelect1 = 0;
    };
    RC OverrideIssueRate(const string& overrideStr, IssueRateOverrideSettings* pOrigSettings);
    RC RestoreIssueRate(const IssueRateOverrideSettings& origSettings);
    virtual RC GetMaxL2BandwidthBps(UINT64 l2FreqHz, UINT64* pReadBps, UINT64 *pWriteBps);

    // Max L1 cache size usable by global LDG instructions in LWCA
    virtual RC GetMaxL1CacheSize(UINT32* pSizeBytes) const;
    virtual RC GetMaxL1CacheSizePerSM(UINT32* pSizeBytes) const;

    UINT08 GetSmbusAddress() const;
    virtual RC DramclkDevInitWARkHz(UINT32 freqkHz);
    virtual RC DisableEcc();
    virtual RC EnableEcc();
    virtual RC DisableDbi();
    virtual RC EnableDbi();
    virtual RC SkipInfoRomRowRemapping();
    virtual RC DisableRowRemapper();
    virtual bool GetL2EccCheckbitsEnabled() const;
    virtual RC SetL2EccCheckbitsEnabled(bool enable);

    struct L2EccCounts
    {
        UINT16 correctedTotal;
        UINT16 correctedUnique;
        UINT16 uncorrectedTotal;
        UINT16 uncorrectedUnique;
    };
    virtual RC GetAndClearL2EccCounts
    (
        UINT32 ltc,
        UINT32 slice,
        L2EccCounts * pL2EccCounts
    ) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC SetL2EccCounts
    (
        UINT32 ltc,
        UINT32 slice,
        const L2EccCounts & l2EccCounts
    ) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetAndClearL2EccInterrupts
    (
        UINT32 ltc,
        UINT32 slice,
        bool *pbCorrInterrupt,
        bool *pbUncorrInterrupt
    ) { return RC::UNSUPPORTED_FUNCTION; }
    virtual bool   GetL1EccEnabled() const;
    virtual RC     SetL1EccEnabled(bool enable);
    virtual RC     ForceL1EccCheckbits(UINT32 data);
    virtual RC     IlwalidateCCache();
    virtual RC     SetSMDebuggerMode(bool enable);
    virtual RC     GetSmEccEnable(bool * pEnable, bool * pOverride) const;
    virtual RC     SetSmEccEnable(bool bEnable, bool bOverride);
    virtual RC     GetTexEccEnable(bool * pEnable, bool * pOverride) const;
    virtual RC     SetTexEccEnable(bool bEnable, bool bOverride);
    virtual RC     GetSHMEccEnable(bool * pEnable, bool * pOverride) const;
    virtual RC     SetSHMEccEnable(bool bEnable, bool bOverride);
    virtual RC     SetEccOverrideEnable(LW2080_ECC_UNITS eclwnit, bool bEnabled, bool bOverride);
    virtual UINT08 GetFbEclwncorrInjectBitSeparation() { return 1; }

    struct FBConfigData
    {
        UINT32 readFtCfg;
        UINT32 readBankQCfg;
    };
    virtual RC     SaveFBConfig(FBConfigData* pData);
    virtual RC     RestoreFBConfig(const FBConfigData& data);
    virtual RC     EnableFBRowDebugMode();

    virtual RC     GetEccEnabled(bool *pbSupported, UINT32 *pEnabledMask) const;
    virtual RC     GetEdcEnabled(bool *pbSupported, UINT32 *pEnabledMask);

    enum PLLType
    {
        LW_PLL,
        H_PLL,
        SP_PLL0,
        SP_PLL1,
        SYS_PLL,
        GPC_PLL,
        LTC_PLL,
        XBAR_PLL
    };

    virtual UINT32 GetPLLCfg(PLLType Pll) const;
    virtual void   SetPLLCfg(PLLType Pll, UINT32 cfg);
    virtual void   SetPLLLocked(PLLType Pll);
    virtual bool   IsPLLLocked(PLLType Pll) const;
    virtual bool   IsPLLEnabled(PLLType Pll) const;
    virtual void   GetPLLList(vector<PLLType> *pPllList) const;
    static const char *GetPLLName(PLLType Pll);

    virtual RC     CalibratePLL(Gpu::ClkSrc clkSrc);

    // this sets bar0+Addr to Data during GPU shutdown
    void SetRegDuringShutdown(UINT32 Addr, UINT32 Data){m_PrivRegTable[Addr]=Data;}
    void SetConfRegDuringShutdown(UINT32 Addr, UINT32 Data){m_ConfRegTable[Addr]=Data;}
    void ResetExceptionsDuringShutdown(bool bEnable);

    // bus info functions
    RC LoadBusInfo();

    bool HasVbiosModsFlagA();
    bool HasVbiosModsFlagB();
    bool IsMXMBoard();
    UINT32 MxmVersion();
    UINT32 VbiosRevision();
    UINT32 VbiosOEMRevision();

    //! Checks whether or not a HULK license is flashed onto the InfoROM
    virtual RC GetIsHulkFlashed(bool* pHulkFlashed);

    //! Determines whether InfoROM is valid.
    RC CheckInfoRom() const;
    void SetInfoRomVersion(string InfoRomVersion);
    const string& GetInfoRomVersion();
    string GetBoardSerialNumber() const;
    string GetBoardMarketingName() const;
    string GetBoardProjectSerialNumber() const;

    RC EndOfLoopCheck(bool captureReference) override;

    UINT32 BusType() override;
    bool SupportsNonCoherent();
    UINT64 GpuGartSize();
    UINT32 GpuGartFlags();
    UINT32 NCFlags();
    UINT32 CFlags();

    // GPU info functions
    RC LoadGpuInfo();
    RC ChipId(vector<UINT32>* pEcids);
    virtual string GetUUIDString(bool bRemoveGPUPrefix = true);
    virtual string ChipIdCooked();
    string GpuIdentStr();

    virtual string DeviceName();
    virtual string DeviceShortName();
    virtual UINT32 Architecture();
    virtual UINT32 Implementation();
    virtual UINT32 RevisionWithoutRm();
    virtual UINT32 ExtMinorRevision();
    virtual UINT32 NetlistRevision0();
    virtual UINT32 NetlistRevision1();
    virtual UINT32 PrivateRevisionWithoutRm();
    virtual UINT32 BoardId();
    virtual UINT32 SampleType();
    UINT32 HQualType();
    bool IsEmulation() const override;
    bool IsEmOrSim() const;
    bool IsDFPGA() const;
    UINT32 GetCoreCount() const;
    UINT32 GetRTCoreCount() const;

    bool IsEcidLo32Valid();
    bool IsEcidHi32Valid();
    bool IsExtendedValid();
    bool IsExtMinorRevisiolwalid();
    bool IsNetlistRevision0Valid();
    bool IsNetlistRevision1Valid();
    bool IsSampleTypeValid();
    bool IsHwQualTypeValid();

    //! Read and write GPU registers (BAR0).
    UINT08 RegRd08(UINT32 Offset) const;
    UINT16 RegRd16(UINT32 Offset) const;
    UINT32 RegRd32(UINT32 Offset) const override;
    UINT32 RegRd32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset
    ) override;
    UINT64 RegRd64(UINT32 Offset) const override;
    UINT64 RegRd64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset
    ) override;
    UINT32 RegOpRd32(UINT32 Offset, LwRm * pLwRm = nullptr) const;

    void   RegWr08(UINT32 Offset, UINT08 Data);
    void   RegWr16(UINT32 Offset, UINT16 Data);
    void   RegWr32(UINT32 Offset, UINT32 Data) override;
    void   RegWr32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT32        data
    ) override;
    void   RegWr64(UINT32 Offset, UINT64 Data) override;
    void   RegWr64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) override;
    void   RegWrBroadcast32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT32        data
    ) override;
    void   RegWrBroadcast64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) override;
    void RegOpWr32(UINT32 Offset, UINT32 Data, LwRm * pLwRm = nullptr);

    //! Read GPU register directly from BAR0 when it is safe.
    //! The caller must ensure that the GPU is not power gated.
    UINT32 RegRd32PGSafe(UINT32 offset) const;

    //! Write GPU register directly to BAR0 when it is safe.
    //! The caller must ensure that the GPU is not power gated.
    void   RegWr32PGSafe(UINT32 offset, UINT32 data);

    void   RegWrSync08(UINT32 Offset, UINT08 Data);
    void   RegWrSync16(UINT32 Offset, UINT16 Data);
    void   RegWrSync32(UINT32 Offset, UINT32 Data) override;
    void   RegWrSync32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT32        data
    ) override;
    void   RegWrSync64(UINT32 Offset, UINT64 Data) override;
    void   RegWrSync64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) override;

    // New register interface:

    RC PollGpuRegFieldGreaterOrEqual
    (
        ModsGpuRegField field,
        UINT32 regIndex0,
        UINT32 regIndex1,
        UINT32 value,
        FLOAT64 timeoutMs
    );
    RC PollGpuRegFieldGreaterOrEqual
    (
        ModsGpuRegField field,
        UINT32 regIndex0,
        UINT32 value,
        FLOAT64 timeoutMs
    )
    { return PollGpuRegFieldGreaterOrEqual(field, regIndex0, 0, value,timeoutMs); }
    RC PollGpuRegFieldGreaterOrEqual
    (
        ModsGpuRegField field,
        UINT32 value,
        FLOAT64 timeoutMs
    )
    { return PollGpuRegFieldGreaterOrEqual(field, 0, 0, value,timeoutMs); }
    RC PollGpuRegValue
    (
        ModsGpuRegValue value,
        UINT32 regIndex1,
        UINT32 regIndex2,
        FLOAT64 timeoutMs
    );
    RC PollGpuRegValue
    (
        ModsGpuRegValue value,
        UINT32 regIndex1,
        FLOAT64 timeoutMs
    );

    //! Host to JTAG operations
    // This is a secure API the user needs to query the Green Hill Server to get the proper security
    // keys for the requested security configuration mask of features to unlock and the boards raw
    // ECID value.
    virtual RC UnlockHost2Jtag(const vector<UINT32> &secCfgMask,
                               const vector<UINT32> &selwreKeys);
    struct JtagCluster {
        UINT32 chipletId;
        UINT32 instrId;
        UINT32 chainLen;
        JtagCluster(UINT32 cid, UINT32 iid, UINT32 len=0)
            : chipletId(cid),instrId(iid),chainLen(len){}
    };
    // These versions of Host2Jtag() support multiple jtag clusters on the same r/w operation.
    virtual RC ReadHost2Jtag(
        vector<JtagCluster> &jtagClusters   // vector of {chipletId,instrId} tuples
        ,UINT32 chainLen                    // total number of bits for all the chains
        ,vector<UINT32> *pResult            // location to store the data
        ,UINT32 capDis = 0                  // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit
        ,UINT32 updtDis = 0);               // sets the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit

    virtual RC WriteHost2Jtag(
        vector<JtagCluster> &jtagClusters   // vector of {chipletId,instrId} tuples
        ,UINT32 chainLen                    // total number of bits for the chains
        ,const vector<UINT32> &inputData    // vector of bits to write
        ,UINT32 capDis = 0                  // sets the LW_PJTAG_ACCESS_CONFIG_CAPT_DIS bit
        ,UINT32 updtDis = 0);               // sets the LW_PJTAG_ACCESS_CONFIG_UPDT_DIS bit

    RC ReadHost2Jtag(UINT32 Chiplet,
                     UINT32 Instruction,
                     UINT32 ChainLength,
                     vector<UINT32> *pResult) override;
    RC WriteHost2Jtag(UINT32 Chiplet,
                      UINT32 Instruction,
                      UINT32 ChainLength,
                      const vector<UINT32> &InputData) override;

   //! Log a register read(even when GpuSubdevice is not allocated
   // or not initialized)
   static void sLogRegRead(const GpuSubdevice *pSubdev, UINT32 gpuInst,
                           UINT32 Offset, UINT32 Data, UINT32 Bits);

   //! Log a register write(even when GpuSubdevice is not allocated
   // or not initialized)
   static void sLogRegWrite(const GpuSubdevice *pSubdev, UINT32 gpuInst,
                            UINT32 Offset, UINT32 Data, UINT32 Bits);

    RC   CtxRegWr32(LwRm::Handle hCtx, UINT32 Offset, UINT32 Data, LwRm* pLwRm = nullptr);
    RC   CtxRegRd32(LwRm::Handle hCtx, UINT32 Offset, UINT32 *pData, LwRm* pLwRm = nullptr);

    bool   EnablePowerGatingTestDuringRegAccess(bool bEnable);
    bool   EnablePrivProtectedRegisters(bool Enable);

    //! VGA register access
    UINT08 ReadCrtc( UINT08 Index, INT32 Head = 0);
    RC     WriteCrtc(UINT08 Index, UINT08 Data, INT32 Head = 0);
    UINT08 ReadSr( UINT08 Index, INT32 Head = 0);
    RC     WriteSr(UINT08 Index, UINT08 Data, INT32 Head = 0 );
    UINT08 ReadAr( UINT08 Index, INT32 Head = 0 );
    RC     WriteAr(UINT08 Index, UINT08 Data, INT32 Head = 0 );
    UINT08 ReadGr( UINT08 Index, INT32 Head = 0 );
    RC     WriteGr(UINT08 Index, UINT08 Data, INT32 Head = 0 );

    virtual UINT32 NumAteSpeedo() const{return 0;};
    //! Read Speedo value set by ATE
    virtual RC GetAteSpeedo(UINT32 SpeedoNum, UINT32 *pSpeedo,
                            UINT32 *pVersion);
    RC GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion) override;
    virtual RC GetAteSpeedoOverride(UINT32 *pSpeedo) {return RC::UNSUPPORTED_FUNCTION;};

    virtual RC GetKappaInfo(UINT32* pKappa, UINT32* pVersion, INT32* pValid) { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetKappaValue(FLOAT64* pKappaValue) { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC KappaValueToFuse(FLOAT32 kappa, UINT32 version, UINT32 valid, UINT32* pFuseValue)
    { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetPerGpcClkMonFaults(UINT08 patitionIndex, UINT32 *pFaultMask)
    { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetPerFbClkMonFaults(UINT08 partitionIndex, UINT32 *pFaultMask)
    { return RC::UNSUPPORTED_FUNCTION; }

    virtual bool IsSupportedNonHalClockMonitor(Gpu::ClkDomain clkDomain)
    { return false; }

    virtual RC GetNonHalClkMonFaultMask(Gpu::ClkDomain clkDom, UINT32 *pFaultMask)
    { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetGPCClkFaultStatus(bool* isFault)
    { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetDRAMClkFaultStatus(bool* isFault)
    { return RC::UNSUPPORTED_FUNCTION; }

    //! Read Iddq set by ATE
    RC GetAteIddq(UINT32 *pIddq, UINT32* pVersion) override;
    virtual RC GetAteSramIddq(UINT32 *pIddq)
    { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetAteMsvddIddq(UINT32 *pIddq)
    { return RC::UNSUPPORTED_FUNCTION; }

    RC GetAteRev(UINT32* pRevision) override
    { return RC::UNSUPPORTED_FUNCTION; }

    RC GetAteDvddIddq(UINT32 *pIddq) override
    { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetSramBin(INT32* pSramBin)
    {return RC::UNSUPPORTED_FUNCTION;};

    // from here down, taken from GpuImpl
    //! \brief Read "speedo" ring-oscillator (process corner data).
    //!
    //! Use a set of N ring-oscillator cirlwits to measure how "fast" the
    //! silicon process is for this chip.
    //! We count the number of cycles the self-clocked ring completes during
    //! an arbitrary time period.  This number can then be compared between
    //! chips of the same type.
    //!
    //! For example, in G80+ we return cycles per 256 refclock counts where
    //! refclk is 27mhz.
    //!
    //! Gpus have one ring-oscillator speedo circuit per FB partition.
    //! If maxNumSpeedos is != numPartitions, returns error.
    //!
    //! Does nothing on emulation or sim.
    //!
    virtual RC ReadSpeedometers
    (
        vector<UINT32>* pSpeedos        //!< (out) measured ring cycles
        ,vector<IsmInfo>* pInfo //!< (out) uniquely identifies each ISM.
        ,PART_TYPES SpeedoType  //!< (in) what type of speedo to read
        ,vector<IsmInfo>* pSettings//!< (in) how to configure each ISM
        ,UINT32 DurationClkCycles = ~0   //!< (in)  sample len in cl cycles.
    );

    // from here down, taken from GpuImpl
    virtual RC PreVBIOSSetup();
    virtual RC PostVBIOSSetup();
    virtual RC ValidatePreInitRegisters();

    virtual bool GetPteKindFromName(const char *Name, UINT32 *PteKind);
    virtual const char *GetPteKindName(UINT32 PteKind);

    virtual void PrintMonitorInfo(Gpu::MonitorFilter monFilter) { PrintMonitorInfo(); }
    virtual void PrintMonitorInfo(RegHal *pRegs, Gpu::MonitorFilter monFilter)
        { PrintMonitorInfo(monFilter); }
    RC SetMonitorRegisters(const string &registers);
    virtual void PrintMonitorRegisters() const;

    enum Engine
    {
        GR_ENGINE,
        MSVLD_ENGINE,
        MSPDEC_ENGINE,
        MSPPP_ENGINE,
        SEC_ENGINE,
        PCE0_ENGINE,
        PCE1_ENGINE
    };

    virtual UINT32 GetEngineStatus(Engine EngineName);

    virtual FrameBuffer *GetFB() const { return m_Fb; }
    Thermal *GetThermal(){ return m_pThermal; }
    virtual RC SetIntThermCalibration(UINT32 ValueA, UINT32 ValueB);
    RC SetOverTempRange(OverTempCounter::TempType overTempType, INT32 min, INT32 max) override;
    RC GetOverTempCounter(UINT32 *pCount) override;
    RC GetExpectedOverTempCounter(UINT32 *pCount) override;

    Perf *GetPerf(){return m_pPerf;}
    Power *GetPower(){return m_pPower;}
    Fuse *GetFuse() override { return m_pFuse.get(); }
    Avfs *GetAvfs(){return m_pAvfs;}
    Volt3x *GetVolt3x(){return m_pVolt3x;}
    SubdeviceGraphics *GetSubdeviceGraphics() const {return m_pSubdevGr.get();}
    SubdeviceFb *GetSubdeviceFb() const { return m_pSubdevFb.get(); }
    FalconEcc *GetFalconEcc() const { return m_pFalconEcc.get(); }
    HsHubEcc *GetHsHubEcc() const { return m_pHsHubEcc.get(); }
    PciBusEcc *GetPciBusEcc() const { return m_pPciBusEcc.get(); }
    TempCtrlMap& GetTempCtrl(){return m_pTempCtrl;}
    TempCtrlPtr GetTempCtrlViaId(UINT32 Id);

    virtual UINT64 FramebufferBarSize() const;

    //-------------------------------------------------------------------------
    // Return a vector of pairs with the GPUs strap registers. The first element
    // of pair is the register offset, the second element is the value.
    virtual RC GetStraps(vector<pair<UINT32, UINT32>>* pStraps);

    struct KFuseStatus
    {
        bool    CrcPass;
        UINT32  Error1;
        UINT32  Error2;
        UINT32  Error3;
        UINT32  FatalError;
        vector<UINT32> Keys;
        KFuseStatus() : CrcPass(false), Error1(0), Error2(0),
                Error3(0), FatalError(0) {}
    };
    virtual RC GetKFuseStatus(KFuseStatus *pStatus, FLOAT64 TimeoutMs);

    //! Attempt to read PDI from fuses.
    RC GetPdi(UINT64* pPdi) override;

    //! Attempt to read raw ECID directly from fuses bypassing RM.
    RC GetRawEcid(vector<UINT32>* pEcid, bool bReverseFields = false) override;

    //! Attempt to read human-readable ECID directly from fuses bypassing RM.
    virtual RC GetEcid(string* pEcid);

    //! Check that the fused ECIDs and the ones return from RM control are
    //! identical
    bool AreEcidsIdentical() const;

    virtual void StopVpmCapture();

    enum BusArchitecture
    {
       Pci        = LW2080_CTRL_BUS_INFO_TYPE_PCI,
       PciExpress = LW2080_CTRL_BUS_INFO_TYPE_PCI_EXPRESS,
       FPci       = LW2080_CTRL_BUS_INFO_TYPE_FPCI,
       Axi        = LW2080_CTRL_BUS_INFO_TYPE_AXI,
    };

    //Extended Edc Error counters
    struct ExtEdcInfo
    {
        UINT08 EdcErrType;
        bool EdcRateExceeded;
        UINT32 EdcTrgByteNum;
    };
    virtual void ResetEdcErrors();
    virtual RC GetEdcDebugInfo(UINT32 virtualFbio, ExtEdcInfo *result);
    virtual void ClearEdcDebug(UINT32 virtualFbio);
    virtual RC GetEdcDirectCounts(UINT32 virtualFbio, bool supported[], UINT64 counts[]);
    virtual RC GetRdEdcErrors(UINT32* count) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetWrEdcErrors(UINT32* count) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC ClearEdcMaxDelta(UINT32 virtualFbio);

    virtual UINT32 MemoryStrap();
    virtual UINT32 MemoryIndex();
    virtual UINT32 RamConfigStrap();
    virtual UINT32 TvEncoderStrap();
    virtual UINT32 Io3GioConfigPadStrap();
    virtual UINT32 UserStrapVal();

    /** Get linear memory base address and mapped size of the framebuffer for the  GPU. */
    // The last place using GetLwBase() is mdiag/resource/pcidev.cpp
    void * GetLwBase() const { return m_pPciDevInfo->LinLwBase; } // XXX Ick, remove me
    UINT32 LwApertureSize() const { return m_pPciDevInfo->LwApertureSize; }
    UINT64 FbApertureSize() const { return m_pPciDevInfo->FbApertureSize; }
    UINT64 InstApertureSize() const { return m_pPciDevInfo->InstApertureSize; }

    UINT32 ChipArchId() const { return m_pPciDevInfo->ChipId; }
    enum SOCDevFilter
    {
        SOC_RM              = 1     // SOC devices to be known to RM
        ,SOC_INTERRUPT      = 2     // SOC devices for which we hook interrupts
    };
    virtual RC GetSOCDeviceTypes(vector<UINT32>* pDeviceTypes, SOCDevFilter filter);
    virtual RC GetSOCInterrupts(vector<UINT32>* pInterrupts);

    UINT32 Foundry();

    //! Gob Information
    virtual UINT32 GobHeight() const { return 8; }
    virtual UINT32 GobWidth() const { return 64; }
    virtual UINT32 GobDepth() const { return 1; }
    virtual UINT32 GobSize() const { return GobHeight() * GobWidth() * GobDepth(); }
    virtual UINT32 GobWord() const { return 16; }

    //! Tile information
    UINT32 TileHeight() const { return 16; }
    UINT32 TileWidth() const { return 16; }
    UINT32 TileSize() const { return TileHeight() * TileWidth(); }

    //! Feature functions
    bool HasFeature(Feature feature) const override;
    void SetHasFeature(Feature feature) override;
    void ClearHasFeature(Feature feature) override;
    virtual bool IsFeatureEnabled(Feature feature);

    //! HasBug
    bool HasBug(UINT32 bugNum) const override;

    //! GPIO functions
    virtual bool GpioIsOutput(UINT32 WhichGpio);
    virtual void GpioSetOutput(UINT32 WhichGpio, bool Level);
    virtual bool GpioGetOutput(UINT32 WhichGpio);
    virtual bool GpioGetInput(UINT32 WhichGpio);

    enum GpioDirection
    {
        INPUT,
        OUTPUT,
    };
    virtual RC SetGpioDirection(UINT32 GpioNum, GpioDirection Direction);

    //! Zlwll functions
    virtual UINT32 GetZlwllPixelsCovered() const;
    virtual UINT32 GetZlwllNumZFar() const;
    virtual UINT32 GetZlwllNumZNear() const;
    virtual RC SetZlwllSize(UINT32 zlwllRegion,
                            UINT32 displayWidth,
                            UINT32 displayHeight);

    virtual char RevisionSuffix() { return '\0'; }
    virtual RC SetDebugState(string Name, UINT32 Value);
    virtual UINT32 GetTextureAlignment() { return 32; }
    virtual RefManual *GetRefManual() const;

    RC   GetPmu(PMU **ppPmu);
    RC   GetSec2Rtos(Sec2Rtos **ppSec2);

    bool HasPendingInterrupt();
    RC ClearStuckInterrupts();

    void SetRomVersion(string romVersion);
    const string& GetRomVersion();
    string GetRomType();
    UINT32 GetRomTypeEnum();
    UINT32 GetRomTimestamp();
    UINT32 GetRomExpiration();
    UINT32 GetRomSelwrityStatus();
    string GetRomOemVendor();
    const string& GetRomPartner();
    const string& GetRomProjectId();

    RC GetEmulationInfo(LW208F_CTRL_GPU_GET_EMULATION_INFO_PARAMS *pEmulationInfo);

    virtual RC PrintMiscFuseRegs()
                { return RC::UNSUPPORTED_FUNCTION; }
    virtual void PrintDisplayPllsRegs(Tee::Priority Priority) {};

    RC ValidateSoftwareTree() override;

    enum VbiosStatus
    {
        VBIOS_OK,
        VBIOS_ERROR,
        VBIOS_WARNING
    };
    virtual RC ValidateVbios
    (
        bool         bIgnoreFailure,
        VbiosStatus *pStatus,
        bool        *pbIgnored
    );

    bool CanClkDomainBeProgrammed(Gpu::ClkDomain cd) const;
    virtual bool HasDomain (Gpu::ClkDomain cd) const;
    Memory::Location FixOptimalLocation(Memory::Location loc);

    enum L2Mode
    {
       L2_DISABLED,
       L2_ENABLED,
       L2_DEFAULT,
    };
    bool CanL2BeDisabled();
    RC SetL2Mode(L2Mode Mode);
    RC GetL2Mode(L2Mode *pMode) const;

    enum L2IlwalidateFlags
    {
        L2_ILWALIDATE_CLEAN = 0x1,  //bit 0: 0= ilwalidate, 1= evict
        L2_FB_FLUSH         = 0x2,  //bit 1: 0= no FB flush, 1= FB flush
        L2_ILWALIDATE_CLEAN_FB_FLUSH = 0x3
    };
    virtual RC IlwalidateL2Cache(UINT32 flags);

    RC CheckXenCompatibility() const;

    //! Perform an escape read/write into the simulator
    int EscapeWrite(const char *Path, UINT32 Index, UINT32 Size, UINT32 Value) override;
    int EscapeRead(const char *Path, UINT32 Index, UINT32 Size, UINT32 *Value) override;
    virtual int EscapeWriteBuffer(const char *Path, UINT32 Index, size_t Size, const void *Buf);
    virtual int EscapeReadBuffer(const char *Path, UINT32 Index, size_t Size, void *Buf) const;

    //! Return true if the RM detects an SLI video bridge
    RC DetectVbridge(bool *Result);

    virtual RC FbFlush(FLOAT64 TimeOutMs);

    RC GetFBCalibrationCount(FbCalibrationLockCount* pCount, bool Reset);

    //! Setup features and bugs, these need to be called before initializtion to
    //! setup bugs that need to be detected early and then again after so
    //! that bugs that require initialization may be setup
    virtual void SetupBugs() = 0;
    virtual void SetupFeatures();

    virtual RC SetCompressionMap(Surface2D* pSurf, const vector<UINT32>& ComprMap);
    virtual bool Is32bitChannel();

    static bool SetBoot0Strap(Gpu::LwDeviceId deviceId, void * LinLwBase, UINT32 boot0Strap);

    void GetNamedPropertyList(set<string> *pNamedProperties);
    RC   GetNamedProperty(string Name, NamedProperty **pProperty);
    RC   GetNamedProperty(string Name, const NamedProperty **pProperty) const;
    bool GetNamedPropertyValue(const string& Name) const;
    bool HasNamedProperty(string Name) const;

    RC ReserveClockThrottle(ClockThrottle ** ppClockThrottle);
    RC ReleaseClockThrottle(const ClockThrottle * pClockThrottle);

    string GetBoardNumber();
    string GetBoardSkuNumber();
    string GetBoardSkuMod();
    string GetCDPN();   // Collaborative design project number
    UINT32 GetBusinessCycle();

    virtual void GetMmeToMem2MemRegInfo(bool bEnable, UINT32 *pOffset,
                                        UINT32 *pMask, UINT32 *pData);
    virtual UINT32 GetMmeInstructionRamSize();
    virtual UINT32 GetMmeStartAddrRamSize();
    virtual UINT32 GetMmeDataRamSize();
    virtual UINT32 GetMmeShadowRamSize();
    virtual UINT32 GetMmeVersion();
    MmeMethods GetMmeShadowMethods();
    virtual RC LoadMmeShadowData();
    virtual RC MmeShadowRamWrite(const MmeMethod Method, UINT32 Data);
    virtual RC MmeShadowRamRead(const MmeMethod Method, UINT32 *pData);
    EccErrCounter *GetEccErrCounter() { return m_pEccErrCounter; }
    EdcErrCounter *GetEdcErrCounter() { return m_pEdcErrCounter; }
    RC DisableEccInfoRomReporting();
    bool IsEccInfoRomReportingDisabled() const;
    RC GetAllEccErrors(UINT32 *pCorr, UINT32 *pUncorr);

    void SetEarlyReg(string reg, UINT32 value);
    void ApplyEarlyRegsChanges(bool InPreVBIOSSetup);
    void RestoreEarlyRegs();

    RC GetRamSvop(SvopValues *pSvop);
    RC SetRamSvop(SvopValues *pSvop);

    RC GetPerfmon(GpuPerfmon **ppPerfmon);
    RC GetISM(GpuIsm **ppISM);
    Ism* GetIsm() override;
    virtual unique_ptr<PciCfgSpace> AllocPciCfgSpace() const;
    FloorsweepImpl *GetFsImpl() const {return m_pFsImpl.get();}
    GCxImpl *GetGCxImpl() const {return m_pGCxImpl.get();}
    HBMImpl *GetHBMImpl() const {return m_pHBMImpl.get();}
    virtual Fsp *GetFspImpl() const {return nullptr;}

    virtual bool IsQuadroEnabled();
    virtual bool IsMsdecEnabled();
    virtual bool IsEccFuseEnabled();
    virtual bool Is2XTexFuseEnabled();

    virtual RC SetFbMinimum(UINT32 fbMinMB);

    RC InitializeWithoutRm(GpuDevice *parent, UINT32 subdevInst);

    RC GetInfoRomObjectVersion
    (
        const char * objName,
        UINT32 *     pVersion,
        UINT32 *     pSubVersion
    ) const;

    RC DumpInfoRom(JsonItem *pJsi) const;
    RC CheckInfoRomForEccErr(Ecc::Unit eclwnit, Ecc::ErrorType errType) const;
    RC CheckInfoRomForPBLCapErr(UINT32 PBLCap) const;
    RC CheckInfoRomForDprTotalErr(UINT32 maxPages, bool onlyDbe) const;
    RC CheckInfoRomForDprDbeRateErr(UINT32 ratePages, UINT32 rateDays) const;

    UINT32 GetFbRegionCount() const { return m_FbRegionInfo.numFBRegions; }
    UINT32 GetFbRegionPerformance(UINT32 regionIndex);
    void GetFbRegionAddressRange(UINT32 regionIndex, UINT64 *minAddress,
        UINT64 *maxAddress);

    void DumpRegs(UINT32 startAddr, UINT32 endAddr) const;

    RC GetBoardInfoFromBoardDB(string *pBoardName, UINT32 *pBoardDefIndex,
                               bool ignoreCache);

    RC PollRegValue(UINT32 Address, UINT32 Value, UINT32 Mask,
        FLOAT64 TimeoutMs) const;

    RC PollRegValueGreaterOrEqual(UINT32 address, UINT32 value, UINT32 mask,
            FLOAT64 timeoutMs) const;

    // WAR 870748 APIs
    virtual UINT32 GetCropWtThreshold();
    virtual void SetCropWtThreshold(UINT32 Thresh);

    //Set BAR1 BLOCK register for physical access to BAR1 memory space
    virtual void EnableBar1PhysMemAccess();
    virtual unique_ptr<ScopedRegister> EnableScopedBar1PhysMemAccess();

    //Program BAR0 PRAMIN window
    virtual unique_ptr<ScopedRegister> ProgramScopedPraminWindow(UINT64 addr, Memory::Location loc);

    virtual RC SetVerboseJtag(bool enable);
    virtual bool GetVerboseJtag();

    RC SetGpuLocId(UINT32 locId);
    UINT32 GetGpuLocId() const { return m_GpuLocId; }

    void AddNamedProperty(string Name, const NamedProperty &namedProperty);

    const Blacklist &GetBlacklistOnEntry() const { return m_BlacklistOnEntry; }

    virtual RC DoEndOfSimCheck(Gpu::ShutDownMode mode);

    // Sets the CPU affinity for this GPU Subdevice for the current thread so
    // that in a NUMA system the thread is on a CPU close to the GPU
    RC SetThreadCpuAffinity();

    RC GetGOMMode(UINT32 *pGomMode) const;
    Display* GetDisplay() override;
    virtual bool HasDisplay();
    virtual bool IsDisplayFuseEnabled();
    bool HasSORXBar();
    bool IsInitialized() const { return m_Initialized; }

    virtual bool FaultBufferOverflowed() const;

    virtual RC IlwalidateCompBitCache(FLOAT64 timeoutMs);
    virtual UINT32 GetComptagLinesPerCacheLine();
    virtual UINT32 GetCompCacheLineSize();
    virtual UINT32 GetCbcBaseAddress();

    bool IsFbBroken(); //-fb_broken is used or not
    bool IsFakeBar1Enabled(); //-use_fake_bar1 is used or not
    virtual UINT32 GetNumGraphicsRunqueues() { return 1; }

    virtual void EnablePwrBlock();
    virtual void DisablePwrBlock();
    virtual bool GetHBMTypeInfo(string *pVendor, string *pRevision) const;
    struct HbmSiteInfo
    {
        UINT16  manufacturingYear;
        UINT08  manufacturingWeek;
        UINT08  manufacturingLoc;
        UINT08  modelPartNumber;
        UINT64  serialNumber;
        string  vendorName;
        string  revisionStr;
    };
    virtual bool GetHBMSiteInfo(const UINT32 siteID, HbmSiteInfo *pHbmSiteInfo);
    virtual RC SetHBMSiteInfo(const UINT32 siteID, const HbmSiteInfo& hbmSiteInfo);
    struct HbmDeviceIdInfo
    {
        bool eccSupported;
        bool gen2Supported;
        bool pseudoChannelSupported;
        UINT08 stackHeight;
    };
    virtual void SetHBMDeviceIdInfo(const HbmDeviceIdInfo& hbmDeviceIdInfo);
    virtual bool GetHBMDeviceIdInfo(HbmDeviceIdInfo *pHbmDeviceIdInfo);

    virtual RC ReconfigGpuHbmLanes(const LaneError& laneError);

    virtual void EnableSecBlock(){}
    virtual void DisableSecBlock(){}

    virtual RC GetXbarSecondaryPortEnabled(UINT32 port, bool *pEnabled) const { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC EnableXbarSecondaryPort(UINT32 port, bool enable) { return RC::UNSUPPORTED_FUNCTION; }
    virtual bool IsXbarSecondaryPortSupported() const;
    virtual bool IsXbarSecondaryPortConfigurable() const;

    // TODO MH-394: Get rid of GetUGpuCount and use GetUGpuMask instead in mods to avoid confusion
    //       with GetMaxUGpuount
    virtual UINT32 GetUGpuCount() const { return 0; }
    virtual UINT32 GetUGpuMask() const { return 0; }
    virtual UINT32 GetMaxUGpuCount() const { return 0; }
    virtual bool IsUGpuEnabled(UINT32 uGpu) const { return false; }
    virtual UINT32 GetL2CacheSizePerUGpu(UINT32 uGpu) const { return 0; }
    virtual INT32 GetUGpuFromLtc(UINT32 ltcid) const { return -1; }
    virtual INT32 GetUGpuFromGpc(UINT32 gpcid) const { return -1; }
    virtual INT32 GetUGpuFromFbp(UINT32 fbpid) const { return -1; }
    virtual INT32 GetUGpuLtcMask(UINT32 uGpu) const { return -1; }
    virtual INT32 GetUGpuGpcMask(UINT32 uGpu) const { return -1; }
    virtual INT32 GetUGpuFbpMask(UINT32 uGpu) const { return -1; }
    virtual INT32 GetFbpNocPair(UINT32 fbp) const { return -1; }
    virtual INT32 GetFbpLtcNocPair(UINT32 fbp, UINT32 ltc) const { return -1; }
    virtual INT32 GetFbpSliceNocPair(UINT32 fbp, UINT32 slice) const { return -1; }

    enum ProcessFlag
    {
        PreVbiosInit
       ,PostVbiosInit
       ,PostRMInit
    };
    virtual void ApplyJtagChanges(UINT32 processFlag);
    virtual RC PostRmInit();
    virtual UINT32 GetUserdAlignment() const;

    RC FlushL2ProbeFilterCache();
    virtual RC GetVprSize(UINT64 *size, UINT64 *alignment);
    virtual RC GetAcr1Size(UINT64 *size, UINT64 *alignment);
    virtual RC GetAcr2Size(UINT64 *size, UINT64 *alignment);
    virtual RC SetVprRegisters(UINT64 size, UINT64 physAddr);
    virtual RC RestoreVprRegisters();
    virtual RC ReadVprRegisters(UINT64 *size, UINT64 *physAddr);
    virtual RC SetAcrRegisters
    (
        UINT64 acr1Size,
        UINT64 acr1PhysAddr,
        UINT64 acr2Size,
        UINT64 acr2PhysAddr
    );

    //!
    //! \brief Set pOutFbpaNum to the number of the FBPA master for the given
    //! site.
    //!
    //! Meant to be used in conjuntion with RegHal register array indexing. The
    //! FBPA number can be used to index the registers 'MODS_PFB_FBPA_x_*'.
    //!
    virtual RC GetHBMSiteMasterFbpaNumber(const UINT32 siteID, UINT32* const pOutFbpaNum) const;

    //!
    //! \brief Get physical IDs of the FBPs connected to a HBM site
    //!
    virtual RC GetHBMSiteFbps
    (
        const UINT32 siteID,
        UINT32* const pFbpNum1,
        UINT32* const pFbpNum2
    ) const;

    //!
    //! \brief Get HBM Link Repair Register corresponding to the given lane
    //! error.
    //!
    //! NOTE: Each GPU needs to implement this according to their swizzling
    //! between the FBPA and FBIO.
    //!
    //! See GetHbmLinkRepairRegister(const LaneError&, UINT32*, bool).
    //!
    virtual RC GetHbmLinkRepairRegister
    (
        const LaneError& laneError
        ,UINT32* pOutRegAddr
    ) const
        { return RC::UNSUPPORTED_FUNCTION; }

    UINT32 HbmChannelToGpuHbmLinkRepairChannel(UINT32 channel) const;

    //!
    //! \brief Trigger memory failure that emulation team can use to capture mem traces
    //!
    RC EmuMemTrigger();

    //! For GA100 when GPU is boot up, GPU is in legacy mode.
    //! Then once MODS asks RM to configure GPU, GPU will go to SMC mode.
    //! After all GPU partitions are destroyed, GPU will be back to legacy mode.
    virtual void EnableSMC() { }
    virtual void DisableSMC() { }
    virtual bool UsingMfgModsSMC() const;
    virtual bool IsSMCEnabled() const { return false; }
    virtual bool IsPerSMCEngineReg(UINT32 offset) const { return false; }
    virtual bool IsPgraphRegister(UINT32 offset) const;
    virtual bool IsRunlistRegister(UINT32 offset) const { return false; }
    virtual UINT32 GetNumJtagChiplets() { return 31; }

    //! Check if SMC mode has been turned on prior to running MODS (e.g. via lwpu-smi).
    virtual bool IsPersistentSMCEnabled() const;

    void SetGrRouteInfo(LW2080_CTRL_GR_ROUTE_INFO* grRouteInfo, UINT32 smcEngineId) const;

    //! Retrieve the index of the partition we are subscribed to
    UINT32 GetPartIdx() const { return m_SmcResource.partIdx; }

    //! Retrieve the swizzid of the partition we are subscribed to
    UINT32 GetSwizzId() const { return GetPartInfo(GetPartIdx()).swizzId; }

    //! Retrieve the local index of the SMC engine we are subscribed to
    UINT32 GetSmcEngineIdx() const { return m_SmcResource.smcEngineIdx; }

    //! Lookup the partition info of the specified partition
    LW2080_CTRL_GPU_GET_PARTITION_INFO GetPartInfo(UINT32 partIdx) const
    {
        MASSERT(partIdx < m_PartInfo.size());
        return m_PartInfo[partIdx];
    }

    //! Return the nth usable SMC partition's partition index
    //!
    //! Only when an SMC partition doesn't have any GPCs assigned to SMC engines
    //! will this differ from the partition index.
    UINT32 GetUsablePartIdx(UINT32 usableIdx) const
    {
        MASSERT(usableIdx < m_UsablePartIdx.size());
        return m_UsablePartIdx[usableIdx];
    }

    //! Return the SMC engine to use when running with SR-IOV
    //!
    UINT32 GetSriovSmcEngineIdx(UINT32 partIdx) const
    {
        MASSERT(partIdx < m_PartInfo.size());
        MASSERT(m_SriovSmcEngineIndices.count(partIdx) > 0);
        return m_SriovSmcEngineIndices.at(partIdx);
    }

    //! Retrieve a list of SMC engines
    vector<UINT32> GetSmcEngineIds() const { return m_SmcResource.smcEngineIds; }

    //!
    //! \brief Initialize a new temperature controller and add it to the map
    //!
    RC AddTempCtrl
    (
        UINT32 id,
        string name,
        UINT32 min,
        UINT32 max,
        UINT32 numInst,
        string units,
        string interfaceType,
        JSObject *pInterfaceParams
    );

    enum class HwDevType
    {
        HDT_GRAPHICS,
        HDT_LWDEC,
        HDT_LWENC,
        HDT_LWJPG,
        HDT_OFA,
        HDT_SEC,
        HDT_LCE,
        HDT_GSP,
        HDT_IOCTRL,
        HDT_FLA,
        HDT_C2C,
        HDT_HSHUB,
        HDT_FSP,
        HDT_ILWALID,
        HDT_ALL_ENGINES,
        HDT_ALL_DEVICES
    };

    static string HwDevTypeToString(GpuSubdevice::HwDevType hwType);
    static GpuSubdevice::HwDevType StringToHwDevType(const string& hwTypeStr);

    struct HwDevInfo
    {
        HwDevType hwType         = HwDevType::HDT_ILWALID;
        UINT32    hwInstance     = 0;

        UINT32    faultId        = 0;
        bool      bFaultIdValid  = false;

        UINT32    resetId        = 0;
        bool      bResetIdValid  = false;

        UINT32    priBase        = 0;

        bool      bIsEngine      = false;
        UINT32    runlistId      = 0;
        UINT32    runlistPriBase = 0;
        UINT32    hwEngineId     = 0;

        void Print(Tee::Priority pri);
    };

    void ClearHwDevInfo();
    const vector<HwDevInfo> & GetHwDevInfo();
    virtual RC EnableHwDevice(GpuSubdevice::HwDevType hwType, bool bEnable)
        { return RC::UNSUPPORTED_FUNCTION; }
    HwDevType RegDevTypeToEnum(UINT32 regType);

    virtual RC SetupUserdWriteback(RegHal &regs) { return OK; }
    virtual bool PollAllChannelsIdle(LwRm *pLwRm);
    virtual UINT32 GetDomainBase(UINT32 domainIdx, RegHalDomain domain, LwRm *pLwRm);

    enum RgbMlwType
    {
         RgbMlwGpu
        ,RgbMlwSli
        ,NumRgbMlwType
    };
    static const char* RgbMlwTypeToStr(RgbMlwType t);

    RC GetRgbMlwFwVersion(RgbMlwType type, UINT32 *pMajorVer, UINT32 *pMinorVer);
    RC DumpCieData();

    RC CheckL2Mode(GpuSubdevice::L2Mode Mode);

    virtual RC RunFpfFub();
    virtual RC RunFpfFubUseCase(UINT32 useCase) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC RunRirFrb() { return RC::UNSUPPORTED_FUNCTION; }

    RC RegisterGetSmcRegHalsCB(function<void* (vector<SmcRegHal*>*)> f);
    RegHal& Regs() override;
    const RegHal& Regs() const override;

    virtual void LoadIrqInfo(IrqParams* pIrqInfo, UINT08 irqType,
                             UINT32 irqNumber, UINT32 maskInfoCount);

    void* GetSmcEnabledMutex() const { return m_pSmcEnabledMutex; }

    // SMC Topology information is stored for use by the physical function in MFG MODS
    struct SmcGpcTopo
    {
        UINT32 localGpc;
        UINT32 virtGpc;
        UINT32 physGpc;
        UINT32 uGpuId;
        UINT32 numTpcs;
    };
    struct SmcEngineTopo
    {
        UINT32 localSyspipe;
        UINT32 physSyspipe;
        vector<SmcGpcTopo> gpcs;
    };
    struct SmcPartTopo
    {
        UINT32 swizzId;
        vector<SmcEngineTopo> smcEngines;
    };
    RC GetSmcPartitionInfo(UINT32 partitionId, SmcPartTopo *smcInfo);

    //!
    //! \brief Number of physical HBM sites on the chip. Count includes inactive
    //! sites.
    //!
    virtual UINT32 GetNumHbmSites() const;

    //!
    //! \brief Read a PF register from a VF.
    //!
    //! If register is not supported, call will fail.
    //!
    //! \param regAddr Register to read.
    //! \param pRegValue Value read from the PF register.
    //!
    //! \see CheckPfRegSupportedFromVf
    //!
    RC ReadPfRegFromVf(ModsGpuRegAddress regAddr, UINT32 *pRegValue) const;

    //!
    //! \brief Check if a register is supported on the Physical Function (PF)
    //! from a Virtual Function (VF).
    //!
    //! \param refAddr Register to check.
    //! \param [out] pIsSupported True if reg is supported on the PF, false
    //! otherwise.
    //!
    //! \see ReadPfRegFromVf
    //!
    RC CheckPfRegSupportedFromVf(ModsGpuRegAddress regAddr, bool* pIsSupported) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param field Register field.
    //! \param value Test value.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        ModsGpuRegField field,
        UINT32 value,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param field Register field.
    //! \param regIndex Register index.
    //! \param value Test value.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        ModsGpuRegField field,
        UINT32 regIndex,
        UINT32 value,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param field Register field.
    //! \param regIndexes Register indexes.
    //! \param value Test value.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        ModsGpuRegField field,
        RegHal::ArrayIndexes regIndexes,
        UINT32 value,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param field Register field.
    //! \param value Test value.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        UINT32 domainIndex,
        ModsGpuRegField field,
        UINT32 value,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param field Register field.
    //! \param regIndex Register index.
    //! \param value Test value.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        UINT32 domainIndex,
        ModsGpuRegField field,
        UINT32 regIndex,
        UINT32 value,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param field Register field.
    //! \param regIndexes Register indexes.
    //! \param value Test value.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        UINT32 domainIndex,
        ModsGpuRegField field,
        RegHal::ArrayIndexes regIndexes,
        UINT32 value,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param value Register value. Corresponding register will be checked.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        ModsGpuRegValue value,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param value Register value. Corresponding register will be checked.
    //! \param regIndexes Register indexes.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        ModsGpuRegValue value,
        UINT32 regIndex,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param value Register value. Corresponding register will be checked.
    //! \param regIndexes Register indexes.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        ModsGpuRegValue value,
        RegHal::ArrayIndexes regIndexes,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param value Register value. Corresponding register will be checked.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        UINT32 domainIndex,
        ModsGpuRegValue value,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param value Register value. Corresponding register will be checked.
    //! \param regIndex Register index.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        UINT32 domainIndex,
        ModsGpuRegValue value,
        UINT32 regIndex,
        bool* pResult
    ) const;

    //!
    //! \brief Test register value on the Physical Function (PF) from a Virtual
    //! Function (VF).
    //!
    //! \param domainIndex Register domain.
    //! \param value Register value. Corresponding register will be checked.
    //! \param regIndexes Register indexes.
    //! \param[out] pResult True if the register field has the given value,
    //! false otherwise.
    //!
    RC Test32PfRegFromVf
    (
        UINT32 domainIndex,
        ModsGpuRegValue value,
        RegHal::ArrayIndexes regIndexes,
        bool* pResult
    ) const;

    //! Check if GPC RG is supported
    virtual bool HasGpcRg() { return false; }
    //! Enable or Disable GPC - RG
    virtual RC EnableGpcRg(bool bEnable) { return RC::UNSUPPORTED_FUNCTION; }

    //! \brief Number of entries in a row remapping table.
    //!
    //! \return Number of row remap table entries. Return 0 for chips that don't
    //! support row remapping.
    //!
    virtual UINT32 NumRowRemapTableEntries() const { return 0; }

    virtual RC CheckHbmIeee1500RegAccess(const HbmSite& hbmSite) const;

    RC GetInfoRomRprMaxDataCount(UINT32* pCount) const;

    struct AdcCalibrationReg
    {
        UINT32 fieldVcmOffset;
        UINT32 fieldDiffGain;
        UINT32 fieldVcmCoarse;
        UINT32 fieldOffsetCal;

        AdcCalibrationReg() : fieldVcmOffset(0), fieldDiffGain(0),
            fieldVcmCoarse(0), fieldOffsetCal(0) {}
    };

    struct AdcRawCorrectionReg
    {
        UINT32 fieldValue;

        AdcRawCorrectionReg() : fieldValue(0) {}
    };

    struct AdcMulSampReg
    {
        UINT32 fieldNumSamples;

        AdcMulSampReg() : fieldNumSamples(0) {}
    };

    virtual RC GetAdcCalFuseVals(AdcCalibrationReg *pFusedRegister) const;
    virtual RC GetAdcProgVals(AdcCalibrationReg *pProgrammedRegister) const;
    virtual RC GetAdcRawCorrectVals
    (
        AdcRawCorrectionReg *pCorrectionRegister
    ) const;
    virtual RC GetAdcMulSampVals(AdcMulSampReg *pAdcMulSampRegister) const;

    virtual RC HaltAndExitPmuPreOs() { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC GetPoisonEnabled(bool* pEnabled) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC OverridePoisolwal(bool bEnable) { return RC::UNSUPPORTED_FUNCTION; }

    virtual RC OverrideSmInternalErrorContainment(bool bEnable) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC OverrideGccCtxswPoisonContainment(bool bEnable) { return RC::UNSUPPORTED_FUNCTION; }

    void SetSkipAzaliaInit(bool bSkip) { m_SkipAzaliaInit = bSkip; }
    bool GetSkipAzaliaInit() const { return m_SkipAzaliaInit; }

    void SetSkipEccErrCtrInit(bool bSkip) { m_SkipEccErrCtrInit = bSkip; }
    bool GetSkipEccErrCtrInit() const { return m_SkipEccErrCtrInit; }

	void SetGspFwImg(const string &gspFwImg) { m_GspFwImg = gspFwImg; }
    string GetGspFwImg() const { return m_GspFwImg; }

    void SetGspLogFwImg(const string &gspLogFwImg) { m_GspLogFwImg = gspLogFwImg; }
    string GetGspLogFwImg() const { return m_GspLogFwImg; }

    virtual RC GetIsDevidHulkEnabled(bool *pIsEnabled) const override;

    virtual RC IsSltFused(bool *pIsFused) const override;

    virtual string GetPackageStr() const { return ""; }
    virtual void AddPriFence();

    bool GetUseRegOps() { return m_bUseRegOps; }
    RC   SetUseRegOps(bool bUseRegOps) { m_bUseRegOps = bUseRegOps; return RC::OK; }
    struct MSVDDRAMAssistStatus
    {
        bool  ramAssistEngaged;
        UINT32 vCritLowLUTVal;
        UINT32 vCritHighLUTVal;
        float sysTsenseTempFP;
        bool tsenseStateValid;
        UINT32 hwlutIndex;
    };
    virtual RC GetMsvddRAMAssistStatus(MSVDDRAMAssistStatus* msvddRAMAssistStatus) {return RC::UNSUPPORTED_FUNCTION; }
    virtual RC OverrideQuickSlowdownB(bool enable, UINT32 ovrMask) {return RC::UNSUPPORTED_FUNCTION; };

protected:

    // Added this contructor for exclusive use in cases like unit testing which don't follow the
    // modsMain()'s init sequence and don't care about the global context setting done
    // there.
    // The existing contructor GpuSubdevice(const char*, Gpu::LwDeviceId, const PciDevInfo*) deals
    // with a lot of context specific initialization and in general, code that ilwokes global
    // variables.
    GpuSubdevice(Gpu::LwDeviceId deviceId,
                 const PciDevInfo* pPciDevInfo);
    virtual bool RegPowerGated(UINT32 Offset) const;
    virtual bool RegPrivProtected(UINT32 Offset) const;
    virtual bool RegDecodeTrapped(UINT32 Offset) const;

    //! Setup MODS bugs from the RM bug database
    RC SetupRmBugs();

    //! Setup MODS bugs specific to OpenGL driver
    RC SetupOglBugs();

    void IntRegClear();
    void IntRegAdd(ModsGpuRegField topField, UINT32 topIndex,
                   ModsGpuRegAddress leafAddr, UINT32 leafIndex,
                   UINT32 leafValue);
    void IntRegAdd(ModsGpuRegAddress topAddr, UINT32 topIndex,
                   UINT32 topMask, ModsGpuRegAddress leafAddr,
                   UINT32 leafIndex, UINT32 leafMask, UINT32 leafValue);

    //! Get the ref manual filename
    virtual string GetRefManualFile() const;

    //! Is the given virtual address inside BAR0 windows
    bool IsBar0VirtAddr(const volatile void* Addr) const;
    //! Is the given virtual address inside BAR1 windows
    bool IsBar1VirtAddr(const volatile void* Addr, UINT64* GpuVirtAddr) const;

    void ClearMmeShadowMethods() { m_MmeShadowMethods.clear(); }
    void AddMmeShadowMethod(UINT32 ShadowClass, UINT32 ShadowMethod, bool bShared);

    virtual GpuPerfmon * CreatePerfmon(void);
    struct PteKindTableEntry
    {
        PteKindTableEntry(UINT32 kind, const char *name)
          : PteKind(kind), Name(name) {}
        UINT32 PteKind;
        string Name;
    };

    typedef vector<PteKindTableEntry> PteKindTable;
    PteKindTable m_PteKindTable;

    unique_ptr<PciDevInfo> m_pPciDevInfo;  //!< Data shared with RM API
    vector<UINT32> m_Irqs;                 //!< List of CPU interrupt vectors for the GPU
    HookedIntrType m_HookedIntr;

    virtual RC DumpEccAddress() const;

    bool GetEosAssert() const { return m_EosAssert; }
    bool m_VerboseJtag;

    bool IsRegTableRestored() const { return m_RestoreRegTableDone; }

    virtual void CreatePteKindTable();

    RC ReconfigGpuHbmLanes
    (
        const LaneError& laneError
        ,bool withDwordSwizzle
    );

    RC GetHbmLinkRepairRegister
    (
        const LaneError& laneError
        ,UINT32* pOutRegAddr
        ,bool withDwordSwizzle
    ) const;

    enum IntrEnable
    {
        intrDisabled,
        intrHardware,
        intrSoftware
    };

    virtual RC SetFbMinimumInternal
        (
            UINT32 fbMinMB,
            const UINT32 minFbColumnBits,
            const UINT32 minFbRowBits,
            const UINT32 minBankBits
        );
    void SetupBugsKepler();
    void PrintFermiMonitorInfo(bool HasPbdmaStatus,
                               UINT32 PfifoEngineStatusSize,
                               UINT32 PfifoEngineChannelSize,
                               UINT32 PbdmaChannelSize);
    virtual void ApplyMemCfgProperties(bool InPreVBIOSSetup);
    void ApplyMemCfgProperties_Impl(
            bool InPreVBIOSSetup, UINT32 extraPfbCfg0,
            UINT32 extraPfbCfg0Mask, bool ReqFilterOverrides);
#ifdef LW_VERIF_FEATURES
    virtual void FilterMemCfgRegOverrides(
        vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS>* pOverrides);
    // Multicast priv-regs schemes, we dup the overrides using provided offsets
    // to cover the multicast versions of each register.
    virtual void FilterMemCfgRegOverrides
    (
        UINT32 offsetMC0,
        UINT32 offsetMC1,
        UINT32 offsetMC2,
        vector<LW_CFGEX_SET_GRCTX_INIT_OVERRIDES_PARAMS>* pOverrides
    );
#endif
    virtual bool ApplyFbCmdMapping(const string &FbCmdMapping,
                                   UINT32 *pPfbCfgBcast,
                                   UINT32 *pPfbCfgBcastMask) const;
    static bool PollJtagForNonzero(void *pVoidPollArgs);
    virtual RC PrepareEndOfSimCheck(Gpu::ShutDownMode mode);
    virtual void EndOfSimDelay() const;
    RC InsertEndOfSimFence(UINT32 pprivSysPriFence,
                           UINT32 pprivGpcGpc0PriFence,
                           UINT32 pprivGpcStride,
                           UINT32 ppriveFbpFbp0PrivFence,
                           UINT32 pprivFbpStride,
                           bool bForceFenceSyncBeforeEos) const;
    bool m_SavedBankSwizzle;
    UINT32 m_OrigBankSwizzle;
    vector<HwDevInfo>                  m_HwDevInfo;
    function<void* (vector<SmcRegHal*>*)> m_GetSmcRegHalsCb;
    unique_ptr<SmcRegHal> m_SmcRegHal;

    RC GetVmiopElw(VmiopElw** ppVmiopElw) const;

    RC GetFaultMask(ModsGpuRegValue dcFaultReg,
                            ModsGpuRegValue lowerThresFaultReg,
                            ModsGpuRegValue higherThresFaultReg,
                            ModsGpuRegValue overFlowErrorReg,
                            UINT32 *pFaultMask);
    virtual FLOAT64 GetGFWBootTimeoutMs() const;

private:
    GpuSensorInfo m_SensorInfo;
    RC AllocateIRQs(UINT32 irqType);
    RC HookINTA();
    RC UnhookINTA();
    RC HookMSI();
    RC UnhookMSI();
    RC HookMSIX();
    RC UnhookMSIX();
    RC ValidateInterrupt(HookedIntrType intrType, unsigned intrTree);
    virtual UINT32 GetIntrStatus(unsigned intrTree) const;
    virtual bool GetSWIntrStatus(unsigned intrTree) const;
    virtual void SetSWIntrPending(unsigned intrTree, bool pending);
    virtual void SetIntrEnable(unsigned intrTree, IntrEnable state);
    virtual unsigned GetNumIntrTrees() const;
    void AckDirectRegAccess(UINT32 reg) const;
    UINT32 GetGrInfoTotal(UINT32 rmInfoIndex) const;

    RC InitializeGPU();

    bool DoIsPllLockRestricted(int) override
    {
        return false;
    }

    RC CheckEccErr(Ecc::Unit eclwnit, Ecc::ErrorType errType) const;
    RC GetAggregateEccInfo(Ecc::Unit eclwnit, Ecc::DetailedErrCounts * pEccData) const;
    RC PrintAggregateEccInfo(Ecc::Unit eclwnit) const;
    virtual UINT32 GetEmulationRev();

    void AddPteKinds();
    void AddPteKindsFloatDepth();
    void AddPteKindsCompressed();
    virtual void PrintMonitorInfo() = 0;

    RC GetRomProjectInfo();
    void InitHwDevInfo();
    void GFWWaitPostReset();

    // Fields set by constructor
    //
    const Gpu::LwDeviceId m_DeviceId; //!< Device ID
    bool m_Initialized;
    GpuDevice *m_Parent;
    UINT32 m_Subdevice;
    string m_GpuName; //!< Name set by the user via -gpu_name
    // Identifies physical location of GPU on multi-gpu systems (DGX2 etc.)
    UINT32 m_GpuLocId; // set by user via -gpu_loc_id
    LwRm::Handle m_SubdeviceDiag;
    LwRm::Handle m_hZbc;
    UINT32 m_GpuId;
    FrameBuffer *m_Fb;
    Thermal *m_pThermal;
    Perf* m_pPerf;
    Power* m_pPower;
    unique_ptr<Fuse> m_pFuse;
    Avfs* m_pAvfs;
    Volt3x* m_pVolt3x;
    unique_ptr<SubdeviceGraphics> m_pSubdevGr;
    unique_ptr<SubdeviceFb> m_pSubdevFb;
    unique_ptr<FalconEcc> m_pFalconEcc;
    unique_ptr<HsHubEcc> m_pHsHubEcc;
    unique_ptr<PciBusEcc> m_pPciBusEcc;
    map<UINT32, TempCtrlPtr> m_pTempCtrl;

    HookedIntrType m_StandByIntr;
    bool m_InStandBy;
    bool m_InGc6;
    bool m_PausePollInterrupts;
    bool m_PerfWasOwned;
    Perf::PerfPoint m_LwrPerfPoint; // Saved across suspend state
    unique_ptr<PexDevMgr::PauseErrorCollectorThread> m_pPausePexErrThread; // used during standby

    Tasker::Mutex m_ValidationMutex = Tasker::NoMutex();
    atomic<ModsEvent*> m_pValidationEvent{nullptr};
    unsigned m_ValidatedTree;

    UINT64 m_FbHeapSize;          //!< Size of the usable region of the FB
    UINT32 m_FbHeapFreeKBReference;
    bool m_L2CacheOnlyMode;       //!< L2 Cache Only Mode
    bool m_BusInfoLoaded;
    bool m_GpuInfoLoaded;
    bool m_SkipAzaliaInit = false;
    string m_GspFwImg = "";
    string m_GspLogFwImg = "";

    struct UpStreamInfo
    {
        PexDevice* pPexDev;
        UINT32 PortIndex;         //! this is the downstream port index on the Upstreamdevice
    };
    UpStreamInfo m_UpStreamInfo;
    FbRanges     m_CriticalFbRanges;

    struct
    {
        LW2080_CTRL_BUS_INFO caps;
        LW2080_CTRL_BUS_INFO bustype;
        LW2080_CTRL_BUS_INFO noncoherent;
        LW2080_CTRL_BUS_INFO coherent;
        LW2080_CTRL_BUS_INFO gpugartsize;
        LW2080_CTRL_BUS_INFO gpugartsizehi;
        LW2080_CTRL_BUS_INFO gpugartflags;
    } m_BusInfo;

    struct
    {
        LW2080_CTRL_GPU_INFO ecidLo32;
        LW2080_CTRL_GPU_INFO ecidHi32;
        LW2080_CTRL_GPU_INFO extended;
        LW2080_CTRL_GPU_INFO extMinorRevision;
        LW2080_CTRL_GPU_INFO netlistRevision0;
        LW2080_CTRL_GPU_INFO netlistRevision1;
        LW2080_CTRL_GPU_INFO sampleType;
        LW2080_CTRL_GPU_INFO hwQualType;
    } m_GpuInfo;

    struct
    {
        bool ecidLo32Valid;
        bool ecidHi32Valid;
        bool extendedValid;
        bool extMinorRevisiolwalid;
        bool netlistRevision0Valid;
        bool netlistRevision1Valid;
        bool sampleTypeValid;
        bool hwQualTypeValid;
    } m_GpuInfoValid;

    RC GetSkuInfoFromBios();
    RC GetBiosInfo(UINT32 InfoIndex, UINT32 *pVersion);
    string RomTypeToString(UINT32 Type);

    static void sLogReg(const GpuSubdevice *pSubdev, UINT32 gpuInst,
                        UINT32 Offset, UINT32 Data, UINT32 Bits,
                        const string &regPrefix);

    bool   m_ReadSkuInfoFromBios;
    bool   m_ReadBoardInfoFromBoardDB;
    string m_ChipSkuId;
    string m_ChipSkuModifier;
    string m_BoardNum;
    string m_BoardSkuNum;
    string m_BoardSkuMod;
    string m_BoardName;
    UINT32 m_BusinessCycle; // eg. N12, N13, etc
    INT32  m_BoardDefIndex;
    string m_CDPN;   // collaborative design project number

    typedef map<UINT32, UINT32> RegTable;
    RegTable             m_PrivRegTable;
    RegTable             m_ConfRegTable;
    bool                 m_RestoreRegTableDone;

    virtual RC     ParseIssueRateOverride(const string& overrideStr, vector<pair<string, UINT32>>* pOverrides);
    virtual RC     CheckIssueRateOverride();
    virtual RC     GetIssueRateOverride(IssueRateOverrideSettings *pOverrides);
    virtual RC     IssueRateOverride(const IssueRateOverrideSettings& overrides);
    virtual RC     IssueRateOverride(const string& unit, UINT32 throttle);
    IssueRateOverrideSettings m_OrigSpeedSelect;
    bool                      m_UseIssueRateOverride;

    struct MonitorRegister
    {
        UINT32 Offset;
        const RefManualRegister *pRMR;
        UINT32 i;
        UINT32 j;
    };
    vector <MonitorRegister> m_MonitorRegisters;

    bool m_ResetExceptions;

    struct EventHook
    {
        UINT32 hEvent;
        EventThread *pEventThread;
    };
    map<UINT32, EventHook> m_eventHooks;

    RC WriteVGAReg(UINT08 Index, UINT08 Data, INT32 Head, UINT32 Prop);
    RC ReadVGAReg(UINT08 Index, INT32 Head, UINT32 Prop);

    enum ClkDomainProp
    {
        CLK_DOMAIN_READABLE,
        CLK_DOMAIN_WRITABLE,
    };
    bool RmHasClkDomain(Gpu::ClkDomain cd, ClkDomainProp DomainProp) const;

    struct InterruptMapEntry
    {
        ModsGpuRegAddress topAddr;  //!< Which reg has the interrupt bit(s)
        UINT32    topIndex;  //!< Index of the topAddr register
        UINT32    topMask;   //!< Interrupt bit(s) in topAddr to clear
        ModsGpuRegAddress leafAddr; //!< Register that controls the bit(s)
        UINT32    leafIndex; //!< Index of the leafAddr register
        UINT32    leafMask;  //!< Mask to write to leafAddr to clear the bit(s)
        UINT32    leafValue; //!< Value to write to leafAddr to clear the bit(s)
    };
    vector<InterruptMapEntry> m_InterruptMap;

    static map<Gpu::LwDeviceId, RefManual> m_ManualMap;

    PMU      *m_pPmu;
    Sec2Rtos *m_pSec2Rtos;

    string m_RomVersion;
    string m_InfoRomVersion;
    UINT32 m_RomType;
    UINT32 m_RomTimestamp;
    UINT32 m_RomExpiration;
    UINT32 m_RomSelwrityStatus;
    string m_RomProjectId;
    string m_RomPartner;
    bool   m_bCheckPowerGating;
    bool   m_CheckPrivProtection;

    void  *m_pGetBar1OffsetMutex;
    void  *m_pSmcEnabledMutex;

    map<string, NamedProperty> m_NamedProperties;

    ClockThrottle * m_pClockThrottle;
    bool            m_ClockThrottleReserved;

    MmeMethods m_MmeShadowMethods;
    EccErrCounter *m_pEccErrCounter;
    EdcErrCounter *m_pEdcErrCounter;
    bool m_EccInfoRomReportingDisabled;

    GpuPerfmon *m_pPerfmon;

    map<UINT32, UINT32> m_EarlyRegsMap;
    map<UINT32, UINT32> m_RestoreEarlyRegsMap;

    bool m_IsEmulation;
    UINT32 m_EmulationRev;
    bool m_IsDFPGA;

    bool m_DoEmuMemTrigger = false;

    bool m_HasUsableAzalia;

    LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS m_FbRegionInfo;

    bool m_EosAssert;

    // Indicates whether Early Initialization procedure is completed successfully
    bool m_IsEarlyInitComplete;

    // Indicates MODS detected that I/O space was enabled before init
    bool m_IsIOSpaceEnabled = false;

    // Support for GCx power saving features
    unique_ptr<GCxImpl> m_pGCxImpl;

    // Support for new ISM speedo access on Kepler & newer GPUs
    unique_ptr<GpuIsm> m_pISM;

    // Support for floorsweep implementor
    unique_ptr<FloorsweepImpl> m_pFsImpl;

    // Support for HBM functionality
    unique_ptr<HBMImpl> m_pHBMImpl;

    // Store HBM Sites Info
    map<UINT32, HbmSiteInfo> m_HbmSitesInfo;

    // Store HBM Device ID Info
    HbmDeviceIdInfo m_HbmDeviceIdInfo;
    bool m_IsHbmDeviceIdInfoValid = false;

    // Store max GPC count
    mutable UINT32 m_MaxGpcCount = 0;

    // Store max TPC count
    mutable UINT32 m_MaxTpcCount = 0;

    // Store TPC count
    mutable UINT32 m_TpcCount = 0;

    // Store Max SMs Per Tpc
    mutable UINT32 m_MaxSmPerTpcCount = 0;

    // Default SMC partition resource info
    struct SmcResourceInfo
    {
        UINT32 partIdx = ~0u;
        UINT32 smcEngineIdx = ~0u;
        vector<UINT32> smcEngineIds;
        map<UINT32, LwRm::Handle> clientPartitionHandle; // client's SmcResource Handle
    };
    SmcResourceInfo m_SmcResource;

    // Information retrieved from LW2080_CTRL_CMD_GPU_GET_PARTITIONS
    vector<LW2080_CTRL_GPU_GET_PARTITION_INFO> m_PartInfo;

    // Partition indexes corresponding to partitions with assigned GPCs
    // This is used to create virtual functions for SMC+SR-IOV mode
    vector<UINT32> m_UsablePartIdx;

    // Mapping of partition index to index returned by LW2080_CTRL_CMD_GPU_GET_PARTITIONS
    //
    // We need to keep track of this so that '-smc_part_idx <part>' corresponds
    // to the proper partition on the command line
    map<UINT32, UINT32> m_PartIdx2QueryIdx;

    // Mapping of which SMC engine to use for which partition (When using SMC + SR-IOV)
    map<UINT32, UINT32> m_SriovSmcEngineIndices;

    mutable map<UINT32, SmcPartTopo> m_SmcTopology;

    // Memory storing notifer from RM that contains the data
    // of LW01_EVENT_OS_EVENT event
    unique_ptr<Surface2D> m_pNotifierSurface;
    LwNotification*  m_pNotifierMem;

    Blacklist m_BlacklistOnEntry;

    RC DumpLWLInfoRomData(Tee::Priority pri) const;
    RC DumpBBXInfoRomData(Tee::Priority pri) const;

    bool m_IsLwRmSubdevHandlesValid = false;
    static bool PollMmeWriteComplete(void *pvArgs);
    static bool PollMmeReadComplete(void *pvArgs);

    // Skip initializing ECC error counter
    bool m_SkipEccErrCtrInit = false;

    RC GetRgbMlwI2cInfo(RgbMlwType type, I2c::I2cDcbDevInfoEntry *mlwI2cEntry);
    RC DumpMlwCieData(UINT32 port, UINT32 address);
    RC PartitionSmc(UINT32 client);
    RC InitSmcResources(UINT32 client);
    RC AllocSmcResource(UINT32 client);

    enum class ResetType : UINT32
    {
        Hot,
        Fundamental,
        FlrMods,
        FlrPlatform
    };
    enum class ResetPciFunction : UINT32
    {
        Gpu    = 1 << 0,
        Azalia = 1 << 1,
        Usb    = 1 << 2,
        Ppc    = 1 << 3
    };
    struct ResetInfo
    {
        ResetType resetType;
        UINT32    pciCfgMask          = 0;
        bool      modifyResetCoupling = false;
    };
    static string ResetTypeToString(ResetType type);
    RC ResetImpl(const ResetInfo &resetInfo);
    RC InitializePerfSoc();
    RC InitializePerfGPU();

    bool m_bUseRegOps = false;
};

#define ADD_NAMED_PROPERTY(Name, InitType, Default)          \
    {                                                        \
        InitType Value = Default;                            \
        NamedProperty initProp(Value);                       \
        AddNamedProperty(#Name, initProp);                   \
    }

#define ADD_ARRAY_NAMED_PROPERTY(Name, Size, Default)  \
    {                                                  \
        NamedProperty initProp(Size, Default);         \
        AddNamedProperty(#Name, initProp);             \
    }

#define CREATE_GPU_SUBDEVICE_FUNCTION(Class)            \
    GpuSubdevice *Create##Class                         \
    (                                                   \
        Gpu::LwDeviceId DeviceId,                       \
        const PciDevInfo *pPciDevInfo                   \
    )                                                   \
    {                                                   \
        return new Class(#Class, DeviceId, pPciDevInfo);\
    }

//
// DAC register access macros.
//
#define DISP_SUBDEV (GetOwningDevice()->GetDisplaySubdevice())
#define DAC_REG_OFFSET(a,h)   ((a) + (0x2000 * h))
#define DAC_REG_RD08(a,h)     (DISP_SUBDEV->RegRd08(DAC_REG_OFFSET(a,h)))
#define DAC_REG_RD32(a,h)     (DISP_SUBDEV->RegRd32(DAC_REG_OFFSET(a,h)))
#define DAC_REG_WR32(a,d,h)   (DISP_SUBDEV->RegWr32(DAC_REG_OFFSET(a,h), d))

#define DAC_FLD_WR_DRF_DEF(d,r,f,c,h) \
        DAC_REG_WR32(LW##d##r, (DAC_REG_RD32(LW##d##r, h)&~(DRF_MASK(LW##d##r##f)<<DRF_SHIFT(LW##d##r##f)))|DRF_DEF(d,r,f,c), h)

#define DAC_FLD_WR_DRF_NUM(d,r,f,n,h) \
        DAC_REG_WR32(LW##d##r, (DAC_REG_RD32(LW##d##r, h)&~(DRF_MASK(LW##d##r##f)<<DRF_SHIFT(LW##d##r##f)))|DRF_NUM(d,r,f,n), h)

#define DAC_REG_WR_DRF_DEF(d,r,f,c,h) \
        DAC_REG_WR32(LW ## d ## r, DRF_DEF(d,r,f,c), h)

#define DAC_REG_RD_DRF(d,r,f,h) \
    ((DAC_REG_RD32(LW##d##r, h)>>DRF_SHIFT(LW ## d ## r ## f))&DRF_MASK(LW ## d ## r ## f))
