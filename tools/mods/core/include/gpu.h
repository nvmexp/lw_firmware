/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/**
 * @file    gpu.h
 * @brief   Declare class Gpu.
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef  INCLUDE_GPU

#ifndef INCLUDED_GPU_STUB_H
    #include "stub/gpu_stub.h"
#endif // INCLUDED_GPU_STUB_H

#else

#ifndef INCLUDED_GPU_H
#define INCLUDED_GPU_H

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif
#ifndef INCLUDED_LWRM_H
#include "lwrm.h"
#endif
#ifndef INCLUDED_TASKER_H
#include "tasker.h"
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif
#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif
#ifndef INCLUDED_STL_MEMORY
#include <memory>
#define INCLUDED_STL_MEMORY
#endif
#ifndef INCLUDED_RM_H
#include "rm.h"
#endif
#include "lwmisc.h"
#include "g_lwconfig.h"

// These macros from g_lwconfig.h cause a clash with mdiag, undefined them
#ifdef FERMI
#undef FERMI
#endif
#ifdef TESLA
#undef TESLA
#endif

#include "class/cl0080.h" // LW0080_MAX_DEVICES
#include "class/cl2080.h" // LW2080_MAX_SUBDEVICES
#include "ctrl/ctrl2080.h"

class RefManual;
class FrameBuffer;
class GpuDevice;
class GpuSubdevice;
class TegraRm;
typedef struct PciDevInfo_t PciDevInfo;

// Definition of a RAM in the graphics ramchain
struct RamChainEntryInfo {
    char Name[256];
    int  Type;
    int  RamID;
    int  BitWidth;
    int  Depth;
    int  Entries;
    int  ClassMask;
};

/**
 * @class(Gpu)  Graphics Processing Unit.
 *
 * - Responsible for detecting and initializing all LWpu graphics devices
 *   in the system.
 * - Responsible for initializing the resource manager.
 * - Controls which is the "current GPU" that the resman talks to,
 *   and that register and framebuffer reads/writes go to.
 * - Has Gpu register read/write and framebuffer read/write functions.
 *
 * Note that this class is not virtualized, and doesn't use the
 * Singleton template.  However, it is written so that if we ever need to
 * virtualize it, we can without changing the files that use it.
 */
class Gpu
{
public:
    ~Gpu();

    enum GpuType
    {
        Stub,
        LwGpu,
    };

    GpuType GetGpuType() { return LwGpu; }
    void* GetOneHzMutex();

    // used to be static data private to gpu.cpp but factoring out simgpu and
    // hwgpu necessitated making these accessible to subclasses
    // They are static so they can also be accessed "globally" by the javascript
    // mods interface macros.
    static bool            s_IsInitialized;   // found gpu...
    static bool            s_RmInitCompleted; // Initialized RM
    static bool            s_SortPciAscending;
    static const PHYSADDR   s_IlwalidPhysAddr;

    // Used to return invalid values back from various functions
    enum
    {
        IlwalidPciDomain = 0xFFFFFFFF,
        IlwalidPciBus = IlwalidPciDomain,
        IlwalidPciDevice = IlwalidPciDomain,
        IlwalidPciFunction = IlwalidPciDomain,
        IlwalidPciDeviceId = IlwalidPciDomain,
        IlwalidLwApertureSize = IlwalidPciDomain,
        IlwalidFbApertureSize = IlwalidPciDomain,
        IlwalidInstApertureSize = IlwalidPciDomain,
        IlwalidRopCount = 0,
        IlwalidShaderCount = IlwalidRopCount,
        IlwalidVpeCount = IlwalidRopCount,
        IlwalidFrameBufferUnitCount = IlwalidRopCount,
        IlwalidTpcCount = IlwalidRopCount,
        IlwalidTpcMask = IlwalidRopCount,
        IlwalidFbMask = IlwalidRopCount,
        IlwalidSmMask = IlwalidRopCount,
        IlwalidXpMask = IlwalidRopCount,
        IlwalidMevpMask = IlwalidRopCount,
        IlwalidMemoryStrap = IlwalidRopCount,
        IlwalidBoardId = IlwalidPciDomain,
        IlwalidPciAdNormal = IlwalidRopCount,
        IlwalidFoundry = IlwalidPciDomain,
    };

    enum MonitorFilter
    {
         MF_ALL
        ,MF_ONLY_ENGINES
        ,MF_NUM_FILTERS
    };
    struct MonitorArgs
    {
        FLOAT64 monitorIntervalMs = 0;
        UINT32  monitorFilter     = 0;
    };

    /**
     * @brief Find and initialize all LWpu GPUs, init the resman.
     *
     * If the board supports external power and DoExternalPowerCheck is true than
     * we will check if the external power is connected.
     *
     * This function is called only once after startup.
     * Once the hardware is initialized, you can run the GPU tests.
     */
    RC Initialize();
    bool IsInitialized() const;
    bool IsRmInitCompleted()const;
    bool IsMgrInit() const;
    bool IsInitSkipped() const;
    RC CheckScpm(GpuSubdevice *pSubdev);
    enum class ShutDownMode : UINT32 {Normal, QuickAndDirty};
    RC ShutDown(ShutDownMode Mode);
    RC DramclkDevInitWARkHz(UINT32 freqkHz);

    RC ValidateAndHookIsr();

    enum class ResetMode : UINT32
    {
        NoReset
       ,HotReset
       ,SwReset
       ,FundamentalReset
       ,ResetDevice
       ,FLReset
    };
    static string ToString(ResetMode mode);
    RC Reset(ResetMode mode);
    RC DisableEcc();
    RC EnableEcc();
    RC SkipInfoRomRowRemapping();
    RC DisableRowRemapper();
    void ConfigureSimulation();
    bool PostBar0Setup(const PciDevInfo& pciDevInfo);
    RC PreVBIOSSetup(UINT32 GpuInstance);
    RC PostVBIOSSetup(UINT32 GpuInstance);

    TegraRm* GetTegraRm() const { return m_pTegraRm.get(); }

    /**
     * @brief Select the LWpu graphics device to be tested
     *
     * The Graphics devices specified by their names (eg GF104, GF106) are tested
     * The devices not specified are not tested by mods
     */
    void SetGpusUnderTest(const set<string>& gpus) { m_GpusUnderTest = gpus; }
    void GetGpusUnderTest(set<string>* pGpus) const { *pGpus = m_GpusUnderTest; }

    enum
    {
        MaxNumDevices = LW0080_MAX_DEVICES,
        UNSPECIFIED_DEV = LW0080_MAX_DEVICES+1
    };

    void SetAllowedPciDevices(const set<UINT64>& devices) { m_AllowedPciDevices = devices; }
    void GetAllowedPciDevices(set<UINT64>* pDevices) const { *pDevices = m_AllowedPciDevices; }
    void SetAllowedDevIds(const set<UINT32>& devIds) { m_AllowedDevIds = devIds; }
    void GetAllowedDevIds(set<UINT32> *pDevIds) const { *pDevIds = m_AllowedDevIds; }

    bool IsPciDeviceInAllowedList(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function) const;

    /**
     * @brief Select current LWpu graphics subdevice.
     *
     * Normally, one graphics device == one GPU chip.
     * There are two exceptions: multi-board (MB) and multi-chip (MC).
     *
     * Multi-Chip is when we have multiple GPUs on a board, behind a
     * bridge (built into the GPUs on lw25 and lw30, or a separate
     * AGP-to-AGP or PCIE-to-PCIE bridge chip).
     * The board looks like one device to SBIOS and has one shared
     * set of BAR resources assigned.
     *
     * Multi-Board is when we have multiple graphics boards, each visible to
     * the SBIOS and with their own BAR resources.
     * The separate boards are lumped together in software, based on a registry
     * key read by the resman.  They may have differing GPU types.
     *
     * Normally there is only one subdevice, subdev 1 (master).
     *
     * With MB or MC, there is also a subdev 2 (and possibly more).
     * Subdevice 1 (master) is the one that has the display connection.
     *
     * With MC, there is a pseudo-subdevice 0 (broadcast) that allows writing
     * to registers and FB of all GPUs at once.  Be careful reading from the
     * broadcast subdevice, you might get data from subdev 1, or you might
     * get 0xdeadbeef, depending on the bridge design.
     *
     * MC and usually MB have a video bridge that allows subdevice 2 to pass
     * video through to the master subdevice 1 for split-screen rendering.
     *
     * Returns error if you attempt to set a subdev > NumSubdevices.
     */
    enum
    {
        MaxNumSubdevices   = LW2080_MAX_SUBDEVICES,
        MasterSubdevice    = 1,

        //! Default argument for methods that directly or indirectly need
        //! a subdevice argument but may not supply one.
        //! If we have an SLI multi-GPU device, UNSPECIFIED_SUBDEV will result
        //! in Gpu::AssertLwrrent being true (if set).
        //! See FixSubdeviceInst() for colwerting a potential
        //! UNSPECIFIED_SUBDEV to an actual subdevice instance
        UNSPECIFIED_SUBDEV = LW2080_MAX_SUBDEVICES+1
    };

    /**
     * @{
     * @brief Set/Get whether a GPU has a bug number - forced value
     */
    void SetHasBugOverride(UINT32 GpuInst, UINT32 BugNum, bool HasBug);
    bool GetHasBugOverride(UINT32 GpuInst, UINT32 BugNum, bool *pHasBug);

    /**
     * @{
     * @brief Read and write GPU registers (BAR0) on the first GPU.
     *        Still only necessary for gdbg_util_mods
     *        Should use particular GpuSubdevice object instead.
     */
    UINT08 RegRd08 (UINT32 Offset);
    UINT16 RegRd16 (UINT32 Offset);
    UINT32 RegRd32 (UINT32 Offset);
    void   RegWr08 (UINT32 Offset, UINT08 Data);
    void   RegWr16 (UINT32 Offset, UINT16 Data);
    void   RegWr32 (UINT32 Offset, UINT32 Data);

    /** @} */

    RC SocRegRd32 (UINT32 DevType, UINT32 Index, UINT32 Offset, UINT32 *pValue);
    RC SocRegRd32 (UINT32 DevIndex, UINT32 Offset, UINT32 *pValue);
    RC SocRegRd32 (PHYSADDR PhysAddr, UINT32 *pValue);
    RC SocRegWr32 (UINT32 DevType, UINT32 Index, UINT32 Offset, UINT32 Data);
    RC SocRegWr32 (UINT32 DevIndex, UINT32 Offset, UINT32 Data);
    RC SocRegWr32 (PHYSADDR PhysAddr, UINT32 Data);

    //! Return true if writing this register is disallowed
    bool   RegWriteExcluded(UINT32 Offset);
    UINT32 GetRegWriteMask (UINT32 GpuInst, UINT32 Offset) const;

    string GetHulkXmlCertArg() const { return m_HulkXmlCert; }
    RC SetHulkXmlCertArg(string hulkXmlCert);

    /**
     * @brief Stop VPM capture
     */
    void StopVpmCapture();

    bool   GetUseVirtualDma() const;
    RC     SetUseVirtualDma(bool Vaule);

    /** @} */

    enum LwDeviceId
    {
#define DEFINE_NEW_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
        GpuId = GpuConstant,
#define DEFINE_X86_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
        GpuId = GpuConstant,
#define DEFINE_ARM_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
        GpuId = GpuConstant,
#define DEFINE_SIM_GPU(DevIdStart, DevIdEnd, ChipId, GpuId, GpuConstant, ...) \
        GpuId = GpuConstant,
#define DEFINE_DUP_GPU(...) // don't care
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_NEW_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_OBS_GPU
#undef DEFINE_X86_GPU
#undef DEFINE_ARM_GPU
#undef DEFINE_SIM_GPU
    };

    struct Chip
    {
        UINT32          ChipId;
        UINT32          DevIdStart;
        UINT32          DevIdEnd;
        UINT32          LwId;
        bool            IsObsolete;
        GpuSubdevice*   (*CreateGpuSubdevice)(Gpu::LwDeviceId,
                                              const PciDevInfo*);
    };

    enum GpuStatus
    {
         gpuStatusValid              = 0
        ,gpuStatusIgnoredPciDev      = 1<<0
        ,gpuStatusIgnoredGpuClass    = 1<<1
        ,gpuStatusUnknown            = 1<<2
        ,gpuStatusObsolete           = 1<<3
        ,gpuStatusFellOffTheBus      = 1<<4
    };

    // Support for -ignore_family -only_family etc.
    static RC IgnoreFamily(const string & family);
    static RC OnlyFamily(const string & family);

    UINT32 GetSubdeviceStatus
    (
        const PciDevInfo& pciDevInfo,
        const Chip** ppChip,
        const set<string>& GpusUnderTest
    );

    //! Corresponds to LW2080_CTRL_PERF_VOLTAGE_DOMAINS_*
    //! Deprecated from GP100 and forward due to split rail, see below
    enum VoltageDomain
    {
        CoreV,
        VoltageDomain_NUM,
        ILWALID_VoltageDomain = VoltageDomain_NUM
    };

    //! Corresponds to LW2080_CTRL_VOLT_VOLT_DOMAIN_*
    //! This enum also supports MULTI_RAIL for GA10X+
    enum SplitVoltageDomain
    {
        VOLTAGE_LOGIC,
        VOLTAGE_SRAM,
        VOLTAGE_MSVDD,
        SplitVoltageDomain_NUM,
        ILWALID_SplitVoltageDomain = SplitVoltageDomain_NUM
    };

    enum ClkDomain  // from sdk/lwpu/inc/ctrl/ctrl2080/ctrl2080clk.h
    {
        ClkUnkUnsp      = 0,    // unknown or unspecified
        ClkM            = 1,    // external FB memory clock
        ClkFirst        = 1,
        ClkHost         = 2,    // host (pci/agp/pci-express) clock
        ClkDisp         = 3,    // G8x display clock
        ClkPA           = 4,    // pixel-clock head A
        ClkPB           = 5,    // pixel-clock head B
        ClkX            = 6,    // we export XCLK for clients for cross-checking
        ClkGpc2         = 7,    //! deprecated
        ClkLtc2         = 8,    //! deprecated
        ClkXbar2        = 9,   //! deprecated
        ClkSys2         = 10,   //! deprecated
        ClkHub2         = 11,   //! deprecated
        ClkGpc          = 12,
        ClkLtc          = 13,
        ClkXbar         = 14,
        ClkSys          = 15,
        ClkHub          = 16,
        ClkLeg          = 17,
        ClkUtilS        = 18,
        ClkPwr          = 19,
        ClkMSD          = 20,
        ClkLwd          = ClkMSD, // alias for MSDCLK
        ClkPexGen       = 21,   // PEX gen. Not a true clock frequency - just a gen number (1, 2, 3)
        ClkPexRef       = 22,
        ClkLwlCom       = 23,
        ClkDomain_NUM   = 24
    };

    // Based on values from sdk/lwpu/inc/ctrl/ctrl2080/ctrl2080clk.h
    // (which are the sources the RM can report in the LW2080_CTRL_CMD_CLK_GET_INFO
    // control call) and drivers/resman/kernel/inc/objclk.h (which is a
    // superset of the values defined in ctrl2080clk.h).  At a minimum,
    // this enumeration should contain all the values from ctrl2080clk.h
    // so that the results of the RM control call can be correctly interpreted
    enum ClkSrc
    {
        ClkSrcDefault             = 0,    // default
        ClkSrcXTAL                = 1,
        ClkSrcEXTREF              = 2,
        ClkSrcQUALEXTREF          = 3,
        ClkSrcEXTSPREAD           = 4,
        ClkSrcSPPLL0              = 5, // SPPLL0 and 1 enums should be conselwtive.
        ClkSrcSPPLL1              = 6,
        ClkSrcMPLL                = 7,
        ClkSrcXCLK                = 8,
        ClkSrcXCLK3XDIV2          = 9,
        ClkSrcDISPPLL             = 10,  // XCLK3DIV128
        ClkSrcHOSTCLK             = 11,
        ClkSrcPEXREFCLK           = 12,
        ClkSrcVPLL0               = 13,  // VPLL0 and 1 enums should be conselwtive.
        ClkSrcVPLL1               = 14,
        ClkSrcVPLL2               = 15,
        ClkSrcVPLL3               = 16,

        ClkSrcSYSPLL              = 17,
        ClkSrcGPCPLL              = 18,
        ClkSrcLTCPLL              = 19,
        ClkSrcXBARPLL             = 20,
        // NOTE we are using addition of the form dacPLLWhich_VPLLA + Head so
        // don't put stuff behind the VPLL* unless it is safe to do so.
        //
        ClkSrcREFMPLL             = 21,
        ClkSrcTestClk             = 22,
        ClkSrcXTAL4X              = 23,
        ClkSrcPWRCLK              = 24,
        ClkSrcHDACLK              = 25,
        ClkSrcXCLK500             = 26,
        ClkSrcXCLKGEN3            = 27,

        ClkSrcHBMPLL              = 28,
        ClkSrcNAFLL               = 29,

        // Need this to allocate memory for each clock
        ClkSrcSupported           = 30,

        // Need this to allocate memory for each clock
        ClkSrc_NUM                = 31

    };

    enum BusArchitecture
    {
        Pci        = LW2080_CTRL_BUS_INFO_TYPE_PCI,
        PciExpress = LW2080_CTRL_BUS_INFO_TYPE_PCI_EXPRESS,
        FPci       = LW2080_CTRL_BUS_INFO_TYPE_FPCI,
        Axi        = LW2080_CTRL_BUS_INFO_TYPE_AXI,
    };

    enum HbmDeviceInitMethod
    {
        DoNotInitHbm,          // Do not initialize
        InitHbmFbPrivToIeee,   // Use FB Priv registers interface to init HBM device
        InitHbmHostToJtag,     // Use host to jtag interface to init HBM device
        InitHbmTempDefault,    // Temporarily init HBM using default interface and query HBM data
    };

    /**
     * Returns linear address of aperture of the specified device.
     * @param devIndex Device index in the relocation table
     * @param ppLinAperture Pointer to a variable which is filled
     *                      with the aperture pointer upon return.
     *                      The returned pointer can only be used with
     *                      MEM_RD and MEM_WR macros.
     */
    RC GetSOCAperture(UINT32 devIndex, void** ppLinAperture);

    /**
     * @brief Colwert an LwDeviceId enum value to a string.
     *
     * This is helpful because the integer value of i.e. G80 is 0x50!
     * Also, it puts the "LWxx" vs. "Gxx" vs "GTxxx" formatting all in one place.
     */
    static string DeviceIdToString(LwDeviceId ndi);
    static string DeviceIdToInternalString(LwDeviceId ndi);
    static string DeviceIdToExternalString(LwDeviceId ndi);
    static UINT32 DeviceIdToFamily(LwDeviceId ndi);

    /**
     * @brief Colwert a ClkSrc enum value to a string.
     */
    static const char * ClkSrcToString(ClkSrc cs);

    RC GetVPLLLockDelay(UINT32* delay);
    RC SetVPLLLockDelay(UINT32 delay);

    //@{
    //!
    //! All Interrupt hooks are deprecated.  Please do not add any additional
    //! code that uses them.  Please do not add any more functions that install
    //! callbacks with the Resource Manager, that approach does not work on
    //! real operating systems where the RM lives in the kernel

    bool GrIntrHook(UINT32 GrIdx, UINT32 GrIntr, UINT32 ClassError,
                    UINT32 Addr, UINT32 DataLo);
    bool DispIntrHook(UINT32 Intr0, UINT32 Intr1);

    typedef bool (*GrIntrHookProc)(UINT32 GrIdx, UINT32 GrIntr, UINT32 ClassError,
                                   UINT32 Addr, UINT32 DataLo,
                                   void *CallbackData);
    typedef bool (*DispIntrHookProc)(UINT32 Intr0, UINT32 Intr1, void *CallbackData);

    void RegisterGrIntrHook(GrIntrHookProc IntrHook, void *CallbackData);
    void RegisterDispIntrHook(DispIntrHookProc IntrHook, void *CallbackData);
    //@}

    RC HandleRmPolicyManagerEvent(UINT32 GpuInst, UINT32 EventId);
    bool RcCheckCallback(UINT32 GpuInstance);
    bool RcCallback(UINT32 GpuInstance, LwRm::Handle hClient,
                    LwRm::Handle hDevice, LwRm::Handle hObject,
                    UINT32 errorLevel, UINT32 errorType, void *pData,
                    void *pRecoveryCallback);
    void StopEngineScheduling(UINT32 GpuInstance, UINT32 EngineId, bool Stop);
    bool CheckEngineScheduling(UINT32 GpuInstance, UINT32 EngineId);

    RC ThreadFile(const string& FileName);
    const string& ThreadMonitor() { return s_ThreadMonitor; }
    const string& ThreadShutdown() { return s_ThreadShutdown; }

    UINT32 GetRunlistSize() const { return m_RunlistSize; }
    RC SetRunlistSize(UINT32 size) { m_RunlistSize = size; return OK; }

    bool GetForceRepost() const { return m_ForceGpuRepost; }

    bool GetOnlySocDevice(){ return m_OnlySocDevice; }
    RC   SetOnlySocDevice(bool bVal){ m_OnlySocDevice = bVal; return OK; }

    bool GetPollInterrupts() const { return m_PollInterrupts; }
    bool IsFusing() const { return m_IsFusing; }
    bool ForceNoDevices() const { return m_ForceNoDevices; }
    bool GetMsiInterrupts() const { return m_MsiInterrupts; }
    bool GetMsixInterrupts() const { return m_MsixInterrupts; }
    RC   SetRequireChannelEngineId(bool bRequire);
    bool GetRequireChannelEngineId() const;

    RC SetHulkCert(const vector<UINT08> &hulk);
    RC LoadHulkLicence();
    RC VerifyHulkLicense();

    UINT64 GetStrapFb() { return m_StrapFb; } 
    void SetStrapFb(UINT64 strapFb) { m_StrapFb = strapFb; }
     
    RC   SetUsePlatformFLR(bool bUsePlatformFLR);
    bool GetUsePlatformFLR() const { return m_bUsePlatformFLR; } 

    bool GetGenerateSweepJsonArg() const { return m_bGenerateSweepJsonArg; }
    RC   SetGenerateSweepJsonArg(bool bGenerateSweepJsonArg);

    // Support emulation for -reg_remap <srcAddr> <dstAddr>
    static UINT32 GetRemappedReg(UINT32 offset);
    static RC RemapReg(UINT32 fromAddr, UINT32 toAddr);

private:
    // @@@
    /** This is a singleton class, constructed only by the helper class GpuPtr. */
    /** However, derivative classes (such as HWGpu and SimulationGpu also need
        to access **/
    Gpu();

    bool              m_SkipDevInit             = false;
    bool              m_SkipRmStateInit         = false;
    bool              m_PollInterrupts          = false;
    UINT32            m_IrqType                 = 0;
    bool              m_AutoInterrupts          = false;
    bool              m_IntaInterrupts          = false;
    bool              m_MsiInterrupts           = false;
    bool              m_MsixInterrupts          = false;
    bool              m_SkipIntaIntrCheck       = false;
    bool              m_SkipMsiIntrCheck        = false;
    bool              m_SkipMsixIntrCheck       = false;
    bool              m_ForceGpuRepost          = false;
    bool              m_SkipVbiosPost           = false;
    bool              m_SkipRestoreDisplay      = false;
    bool              m_IsFusing                = false;
    bool              m_ForceNoDevices          = false;
    MonitorArgs       m_GpuMonitorData;
    GrIntrHookProc    m_GrIntrHook              = nullptr;
    DispIntrHookProc  m_DispIntrHook            = nullptr;
    void *            m_GrIntrHookData          = nullptr;
    void *            m_DispIntrHookData        = nullptr;
    bool              m_UseVirtualDma           = false;
    UINT32            m_RunlistSize             = 256;
    bool              m_bRequireChannelEngineId = false;
    UINT64            m_StrapFb                 = 0;
    bool              m_bUsePlatformFLR         = false;
    bool              m_bGenerateSweepJsonArg   = false;
    string            m_HulkXmlCert;
    bool              m_DisableFspSupport       = false;

    UINT32 ComputeBoot0Strap();

    FLOAT64           m_ThreadIntervalMs        = 0;

    static string     s_ThreadMonitor;
    static string     s_ThreadShutdown;

    RC EnumSOCDevices(UINT32 devType);
    RC EnumAllSOCDevicesForRM(GpuSubdevice* pSubdev);
    friend class GpuPtr;

    set<UINT64> m_AllowedPciDevices;
    set<UINT32> m_AllowedDevIds;
    bool m_OnlySocDevice = false;

    // m_HasBugOverride is set per GPU;
    vector< map<UINT32, bool> > m_HasBugOverride;

    set <string> m_GpusUnderTest;
    SocInitInfo m_SocInitInfo = {};
    vector<SocDevInfo> m_SocDevices;

    // Support for -ignore_family.
    static vector<bool> s_IgnoredFamilies;
    static bool IsIgnoredFamily(Gpu::LwDeviceId id);

    // Support emulation for -reg_remap
    // A map of registers to remap to old addresses. This feature is to support emulation teams
    // when the arch team releases a new set of register defines that MODS picks up at TOT but they
    // still want to use a newlist built with the old register addresses.
    static map<UINT32, UINT32> s_RemapRegs;

    // CheetAh RM holder
    unique_ptr<TegraRm> m_pTegraRm;

    RC ValidateIsr(bool *allowAutoMsix, bool *allowAutoMsi, bool *allowAutoInta);
    RC HookIsr(bool allowAutoMsix, bool allowAutoMsi, bool allowAutoInta);

    // HULK information
    vector<UINT08> m_HulkCert;
    UINT32         m_HulkDisableFeatures = 0;
    bool           m_HulkIgnoreErrors    = false;
    bool           m_LoadHulkViaMods     = false;
    bool           m_SkipHulkIfRevoked   = false;
};

/**
 * @class(GpuPtr)  A helper class, providing simplified singleton access.
 *
 * If Gpu is virtualized someday, we can hide that change
 * by replacing GpuPtr with Singleton<Gpu*>.
 */
class GpuPtr
{
public:
    explicit GpuPtr() {}

    static Gpu s_Gpu; //!< The only Gpu object that ever exists.

    Gpu * Instance()   const { return &s_Gpu; }
    Gpu * operator->() const { return &s_Gpu; }
    Gpu & operator*()  const { return s_Gpu; }
    operator bool() const { return true; }

    static bool IsInstalled() { return true; }
};

/*
 * Structure definitions for register write exclusion ranges
 */
typedef struct RegRange_t
{
    UINT32 Start;
    UINT32 End;
} REGISTER_RANGE;

class RegMaskRange
{
 private:
     UINT32 m_GpuInst;
     UINT32 m_Startreg;
     UINT32 m_Endreg;
     UINT32 m_Bitmask;
 public:
     RegMaskRange();
     RegMaskRange(UINT32 GpuInst, UINT32 addr1, UINT32 addr2, UINT32 msk);
     void SetValues(UINT32 GpuInst, UINT32 addr1, UINT32 addr2, UINT32 msk);
     bool IsInRange(UINT32 GpuInst, UINT32 Offset) const
     {
         return (GpuInst == m_GpuInst && Offset >= m_Startreg && Offset <= m_Endreg);
     }
     UINT32 GetMask(void) const
     {
         return m_Bitmask;
     }
};

extern vector<RegMaskRange> g_RegWrMask;
extern vector<REGISTER_RANGE> g_GpuWriteExclusionRegions;
extern vector<REGISTER_RANGE> g_GpuLogAccessRegions;


#endif // !INCLUDED_GPU_H
#endif // INCLUDE_GPU
