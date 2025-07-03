/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2016,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef DISP_TEST_H
#define DISP_TEST_H

#include <string>
#include <vector>

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

#ifndef INCLUDED_LWRM_H
#include "lwrm.h"
#endif

#include "evntthrd.h"

//the header for the DispTest::Display class we are migrating to
#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/tests/disptest/display_base.h"
#else
#include "gpu/tests/disptest/display_base.h"
#endif

#include "class/cl5070.h"

namespace DispTest
{
    //for some reason we have to re-declare the DispTest::Display class
    class Display;
    ///a vector of pointers to the display devices
    extern std::vector<Display *> m_display_devices;

    // polling
    RC PollHWValue(const char * , UINT32, UINT32, UINT32, UINT32, FLOAT64);
    RC PollHWGreaterEqualValue(const char * , UINT32, UINT32, UINT32, UINT32, FLOAT64);
    RC PollHWLessEqualValue(const char * , UINT32, UINT32, UINT32, UINT32, FLOAT64);
    RC PollHWNotValue(const char * , UINT32, UINT32, UINT32, UINT32, FLOAT64);
    RC PollIORegValue(UINT16, UINT16, UINT16, FLOAT64);
    RC PollRegValue(UINT32 , UINT32 , UINT32 , FLOAT64 );
    RC PollRmaRegValue(UINT32 , UINT32 , UINT32 , FLOAT64 );
    RC PollRegNotValue(UINT32 , UINT32 , UINT32 , FLOAT64 );
    RC PollRegLessValue(UINT32 , UINT32 , UINT32 , FLOAT64 );
    RC PollRegGreaterValue(UINT32 , UINT32 , UINT32 , FLOAT64 );
    RC Poll2RegsGreaterValue(UINT32 , UINT32 , UINT32 , UINT32 , UINT32 , UINT32 , FLOAT64 );
    RC PollRegLessEqualValue(UINT32 , UINT32 , UINT32 , FLOAT64 );
    RC PollRegGreaterEqualValue(UINT32 , UINT32 , UINT32 , FLOAT64 );
    RC PollRGFlipLocked(UINT32, FLOAT64);
    RC PollRGScanLocked(UINT32, FLOAT64);
    RC PollRGFlipUnlocked(UINT32, FLOAT64);
    RC PollRGScanUnlocked(UINT32, FLOAT64);
    RC PollRegValueNoUpdate(UINT32 , UINT32 , UINT32 , FLOAT64 );

    // vga init/shutdown routines
    RC IsoInitVGA(UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, FLOAT64);
    RC VGASetClocks(UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, FLOAT64);
    RC VGASetClocks_gt21x(UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, FLOAT64);
    RC VGASetClocks_gf10x(UINT32, UINT32, UINT32, UINT32, UINT32, UINT32, FLOAT64);
    RC IsoProgramVPLL(UINT32, UINT32, UINT32, UINT32);
    RC IsoProgramVPLL_gt21x(UINT32, UINT32, UINT32, UINT32);
    RC IsoProgramVPLL_gf10x(UINT32, UINT32, UINT32, UINT32);
    RC IsoCalcVPLLCoeff_gf10x(UINT32, UINT32, UINT32);
    RC IsoCalcVPLLCoeff_gt21x(UINT32, UINT32, UINT32);
    RC IsoInitLiteVGA(UINT32);
    RC IsoShutDowlwGA(UINT32, UINT32, FLOAT64);

    // struct and method used to get n, m, p coefficients to program pclk in IsoInitVGA
    typedef struct _def_clock_setup_params
    {
         UINT32 TargetFreq;    // In 10s of KHz.  This is what we are trying to hit
         UINT32 ActualFreq;    // In 10s of KHz.  Actual freq after the pulse eater
         UINT32 SetupHints;    // See defines above.
         UINT32 Ma, Mb, Na, Nb, P;
    } CLKSETUPPARAMS;
    RC CalcClockSetup_TwoStage(CLKSETUPPARAMS *pClk);

    // read/write VGA indexed and priv registers
    RC IsoIndexedRegWrVGA(UINT16, UINT08, UINT08);
    RC IsoIndexedRegRdVGA(UINT16, UINT08, UINT08&);
    RC IsoAttrRegWrVGA(UINT32, UINT32);
    RC RmaRegWr32(UINT32, UINT32);
    RC RmaRegRd32(UINT32, UINT32 *);

    RC UpdateNow(FLOAT64 timeoutMs);

    // depricated VGA CRC API
    void* GetFbVgaPhysAddr();
    void  ReleaseFbVgaPhysAddr(void*);
    RC BackdoorVgaInit();
    RC BackdoorVgaRelease();
    RC BackdoorVgaWr08(UINT32, UINT08);
    RC BackdoorVgaRd08(UINT32, UINT08*);
    RC BackdoorVgaWr16(UINT32, UINT16);
    RC BackdoorVgaRd16(UINT32, UINT16*);
    RC BackdoorVgaWr32(UINT32, UINT32);
    RC BackdoorVgaRd32(UINT32, UINT32*);
    RC FillVGABackdoor(string, UINT32, UINT32);
    RC SaveVGABackdoor(string, UINT32, UINT32);
    RC VgaCrlwpdate();
    RC AssignHead(UINT32, UINT32, UINT32, UINT32);

    // crc API that needs to be visible from other C++ classes
    RC CrcAddHead(UINT32, LwRm::Handle, LwRm::Handle);

    // colwerts raw RTL output into .tga files
    RC IsoDumpImages(const std::string& rtl_pixel_trace_file, const std::string& test_name, bool add2p4);
    RC SetLegacyCrc(UINT32 enable, UINT32 head);

    // Collect lwdps crcs on the specified head.
    RC CollectLwDpsCrcs(UINT32);

    // Collect custom events for output after the test completes
    RC CollectLwstomEvent(string, string);

    // Write out the custom event files collected with CollectLwstomEvent
    RC WriteLwstomEventFiles();

    // Initialize the custom event capture
    RC LwstomInitialize();

    // Deallocate memory used for custom event capture data structures
    void LwstomCleanup();

    // get scanline from base channel
    RC GetBaseChannelScanline(LwRm::Handle, UINT32*);

    // get put pointer for channel
    RC GetChannelPut(LwRm::Handle, UINT32*);
    // set put pointer for channel
    RC SetChannelPut(LwRm::Handle, UINT32);
    // get get pointer for channel
    RC GetChannelGet(LwRm::Handle, UINT32*);
    // set get pointer for channel
    RC SetChannelGet(LwRm::Handle, UINT32);

    // read ptimer
    RC GetPtimer(UINT32*, UINT32*);

    // give access to vpll lock delay (added mostly for dtb)
    RC GetVPLLLockDelay(UINT32* delay);
    RC SetVPLLLockDelay(UINT32 delay);

    // sets the sppll clock spread percentage value
    RC SetSPPLLSpreadPercentage(UINT32 spread_value);
    RC SetMscg(UINT32 spread_value);

    // overrides the callwlated timeslot value
    RC SetForceDpTimeslot(UINT32 sor_num, UINT32 headnum, UINT32 timeslot_value, bool update_alloc_pbn);

    // TODO!
    // this is a copy of P4Action() in mdiag dir (which is in golobal namespace)
    // but it can not be used here since some configs are compiled witout mdiag
    // perhaps we need to have only one copy... not sure where to put it right now
    int P4Action(const char *action, const char *filename);

    // Utility functions to set and get global timeout values.
    FLOAT64 GetGlobalTimeout();
    RC      SetGlobalTimeout(FLOAT64 timeoutMs);

    GpuSubdevice *GetBoundGpuSubdevice();
    GpuDevice    *GetBoundGpuDevice();

    RC SetDClkMode(UINT32 , UINT32 );
    RC GetDClkMode(UINT32 , UINT32* );

    UINT32 GetPClkMode();
    RC     SetPClkMode(UINT32 PClkMode);
}

#ifdef DISPTEST_PRIVATE
////////////////////////////////////////////////////////////////////////////////////////////
//
//  DISPTEST_PRIVATE - Private classes, data structures, defines and method definitions for DispTest
//

// sizes of context dma memory areas allocated by display test
#define DISPTEST_PB_SIZE         0x00001000                      /* size of channel pushbuffer        */
#define DISPTEST_ERR_NOT_SIZE    0x00001000                      /* size of error notifier            */
#define DISPTEST_NOTIFIER_SIZE   0x00001000                      /* size of user-allocated notifiers  */
#define DISPTEST_SEMAPHORE_SIZE  0x00001000                      /* size of user-allocated semaphores */

// maximum size of status output messages
#define DISPTEST_STATUS_MESSAGE_MAXSIZE         256

// crc manager default parameters
#define DISPTEST_CRC_DEFAULT_TOLERANCE          1024                /* check tolerance (in microseconds) */
#define DISPTEST_CRC_DEFAULT_TOLERANCE_FMODEL   32                /* check tolerance when using the fmodel (in microseconds) */
#define DISPTEST_CRC_DEFAULT_TEST_OWNER         "none"           /* default value for owner field of output crc files */
#define DISPTEST_CRC_DEFAULT_FILENAME_PREFIX           "test"           /* prefix of file in which to store CRCs for the heads */
#define DISPTEST_CRC_DEFAULT_HEAD_FILENAME_SUFFIX      ".crcfile"       /* file in which to store CRCs for the heads */
#define DISPTEST_CRC_DEFAULT_NOTIFIER_FILENAME_SUFFIX  ".notifier"      /* file in which to store notifier CRCs      */
#define DISPTEST_CRC_DEFAULT_SEMAPHORE_FILENAME_SUFFIX ".semaphore"     /* file in which to store semaphore CRCs     */
#define DISPTEST_CRC_DEFAULT_INTERRUPT_FILENAME_SUFFIX ".interrupt"     /* file in which to store interrupt CRCs     */
#define ILWALID_HEAD_NUMBER 0xF

#define VGA_BACKDOOR_SIZE 0x01000000
//
// Defines to be used in CalcClockSetup_TwoStage
#define CLK_SETUP_HINTS_NONE             0
#define CLK_SETUP_HINTS_FIND_BEST_FIT    1

#define DISPTEST_SET_LWRRENT_DEVICE \
DispTest::BindDevice(DispTest::m_DeviceIndex[(DispTest::DispTestDeviceInfo*)JS_GetPrivate(pContext, pObject)]);

namespace DispTest
{
    typedef std::map<UINT32, struct crc_notifier_info*> OffsetToNotifierMap;

    extern UINT32 activeSubdeviceIndex;    // active subdevice for control call

    struct DmaContext
    {
        void * Address;
        UINT64 Limit;
        UINT32 Flags;
        string *Target;
        UINT64 Offset;
    };

    struct ChannelContextInfo
    {
        UINT32 Class;
        UINT32 Head; // Head on which the channel was "setup"
        LwRm::Handle hErrorNotifierCtxDma;
        LwRm::Handle hPBCtxDma;
    };

    /* CRC manager global parameters */
    struct crc_info {
        string *p_test_name;                // name of the test
        string *p_subtest_name;             // name of the subtest being run
        string *p_owner;                    // string specifying name of test owner
        UINT32 tolerance;                   // default tolerance (non-fmodel mode)
        UINT32 tolerance_fmodel;            // default tolerance (fmodel mode)
        string *p_head_filename;            // name of file for head crc output
        string *p_notifier_filename;        // name of file for notifier crc output
        string *p_semaphore_filename;       // name of file for semaphore crc output
        string *p_interrupt_filename;       // name of file for interrupt crc output
    };

    /* CRC parameters for notifiers */
    struct crc_notifier_info {
        UINT32 tag;                         // tag identifying notifer
        LwRm::Handle channel_handle;        // parent channel handle for notifier
        LwRm::Handle ctx_dma_handle;        // notifier handle
        UINT32 offset;                      // offset within context dma of start of notifier memory
        string *p_notifier_type;            // type of notifier (used to compute status_pos, bitmask, poll_value, n_words)
        UINT32 status_pos;                  // position of status word relative to start of notifier memory
        UINT32 bitmask;                     // bitmask to apply to status
        UINT32 poll_value;                  // status value to poll
        UINT32 n_words;                     // number of words of notifier memory
        UINT32 tolerance;                   // check tolerance
        bool is_active;                     // record events from this notifier?
        bool is_ref_head;                   // whether the notifier entry will be dumped with exact logic head information
                                            // by default all entries will be appended with full heads list test used
    };

    /* CRC parameters for semaphores */
    struct crc_semaphore_info {
        UINT32 tag;                         // tag identifying semaphore
        LwRm::Handle channel_handle;        // parent channel handle for semaphore
        LwRm::Handle ctx_dma_handle;        // semaphore handle
        UINT32 offset;                      // offset within context dma of start of semaphore memory
        UINT32 status_pos;                  // position of status word relative to start of semaphore memory
        UINT32 bitmask;                     // bitmask to apply to status
        UINT32 poll_value;                  // status value to poll
        UINT32 tolerance;                   // check tolerance
        bool is_active;                     // record events from this semaphore?
        bool is_ref_head;                   // whether the notifier entry will be dumped with exact logic head information
                                            // by default all entries will be appended with full heads list test used
    };

    /* CRC parameters for interrupts */
    struct crc_interrupt_info {
        UINT32 tag;                         // tag identifying interrupt
        string *p_name;                     // name of the interrupt
        UINT32 address;                     // address of interrupt register
        UINT32 bitmask;                     // interrupt register bitmask
        UINT32 poll_value;                  // value of register when interrupt has oclwred
        UINT32 tolerance;                   // check tolerance
        bool is_active;                     // record events from this interrupt?
        UINT32 count;                       // number of events for this interrupt that already oclwrred
        UINT32 expected_count;                 // Total expected count for this interrupt type
    };

    /* notifier event */
    struct crc_notifier_event {
        struct crc_notifier_info *p_info;   // notifier associated with event
        UINT32 time;                        // time of event
        UINT32 *p_words;                    // notifier memory at time of event
    };

    /* semaphore event */
    struct crc_semaphore_event {
        struct crc_semaphore_info *p_info;  // semaphore associated with event
        UINT32 time;                        // time of event
    };

    /* interrupt event */
    struct crc_interrupt_event {
        struct crc_interrupt_info *p_info;  // interrupt associated with event
        UINT32 time;                        // time of event
        UINT32 count;                       // number of interrupts of this type that already oclwrred
        UINT32 expected_count;
    };

    class DispTestDeviceInfo
    {
    public:
        DispTestDeviceInfo() {
            FbVGA_address = 0;  // address of VGA space in framebuffer

            ObjectHandle = 0;
            DispCommonHandle = 0;
            DoSwapHeads = false;
            UseHead = false;
            VgaHead = 0;
            CrcIsInitialized = false;
            CrcStartTime = 0;
            pCrcInfo = nullptr;
            pCrcHeadList = nullptr;
            pCrcNotifierList = nullptr;
            pCrcSemaphoreList = nullptr;
            pCrcInterruptList = nullptr;
            pCrcHeadMap = nullptr;
            pCrcNotifierMap = nullptr;
            pCrcSemaphoreMap = nullptr;
            pCrcInterruptMap = nullptr;
            pLwstomEventMap = nullptr;
            pCrcHeadEventList = nullptr;
            pCrcNotifierEventList = nullptr;
            pCrcSemaphoreEventList = nullptr;
            pCrcInterruptEventList = nullptr;
            pCrcHeadEventMap = nullptr;
            pCrcNotifierEventMap = nullptr;
            pCrcSemaphoreEventMap = nullptr;
            pCrcInterruptEventMap = nullptr;
            m_MyDeviceIndex = 0;

            // 0 default that means uninitialized
            unInitializedClass = 0;

        };

        UINT32 unInitializedClass;

        map<LwRm::Handle, DmaContext *> DmaContexts;

        // parameters for allocating Evo object
        LW5070_ALLOCATION_PARAMETERS DispAllocParams = {};

        UINT08 *FbVGA_address;  // address of VGA space in framebuffer

        /* handles */
        LwRm::Handle ObjectHandle;            // EVO object handle

        LwRm::Handle DispCommonHandle;        // Display common handle (used for edid read, device detection ctrl calls)

        /* map of (channel handle) -> (channel name string) */
        map<LwRm::Handle, string> ChannelNameMap;

        /* map of (channel handle) -> (channel number) */
        map<LwRm::Handle, UINT32> ChannelNumberMap;

        /* map of (channel handle) -> (channel class) */
        map<LwRm::Handle, UINT32> ChannelClassMap;

        /* map of (channel handle) -> (Head) */
        map<LwRm::Handle, UINT32> ChannelHeadMap;

        // List of memory allocations that need to be freed when shutting down
        map<LwRm::Handle, void*> MemMap;

        /**
         * map of ChannelHandle -> All necessary context needed to switch
         *                         to that channel.
         */
        map<LwRm::Handle, ChannelContextInfo *> d_ChannelCtxInfo;

        /* swap head0 <-> head1 */

        bool DoSwapHeads;

        /* use head = VgaHead */

        bool UseHead;
        UINT32 VgaHead;

        /*
         * CRC manager data structures
         */

        /* Flag to check whether the Crc Manager has been initialized*/
        bool CrcIsInitialized;

        /*
         * CRC data
         */

        /* CRC start time */
        UINT32 CrcStartTime;

        /* CRC data */
        struct crc_info                  *pCrcInfo;
        list<struct crc_head_info*>      *pCrcHeadList;
        list<struct crc_notifier_info*>  *pCrcNotifierList;
        list<struct crc_semaphore_info*> *pCrcSemaphoreList;
        list<struct crc_interrupt_info*> *pCrcInterruptList;

        /* CRC lookup maps for head notifiers, notifiers, and semaphores */
        map< UINT32,       struct crc_head_info* >                     *pCrcHeadMap;        // (head number -> info)
        map< LwRm::Handle, map<UINT32, struct crc_notifier_info*> * >  *pCrcNotifierMap;    // (handle -> offset -> info)
        map< LwRm::Handle, map<UINT32, struct crc_semaphore_info*> * > *pCrcSemaphoreMap;   // (handle -> offset -> info)
        map< UINT32,       struct crc_interrupt_info* >                *pCrcInterruptMap;   // (tag -> info)

        // Custom event map - this is for the test.custom files
        map< string,  list<string> * >   *pLwstomEventMap;    // (filename -> filedata)

        /* chronological lists of events (by event type) */
        list<struct crc_head_event*>      *pCrcHeadEventList;
        list<struct crc_notifier_event*>  *pCrcNotifierEventList;
        list<struct crc_semaphore_event*> *pCrcSemaphoreEventList;
        list<struct crc_interrupt_event*> *pCrcInterruptEventList;

        /* chronological lists of events (per head/notifier/semaphore/interrupt) */
        map< struct crc_head_info*,      list<struct crc_head_event*> * >       *pCrcHeadEventMap;
        map< struct crc_notifier_info*,  list<struct crc_notifier_event*> * >   *pCrcNotifierEventMap;
        map< struct crc_semaphore_info*, list<struct crc_semaphore_event*> * >  *pCrcSemaphoreEventMap;
        map< struct crc_interrupt_info*, list<struct crc_interrupt_event*> * >  *pCrcInterruptEventMap;

        UINT32 m_MyDeviceIndex;

        UINT32 GetDeviceIndex() {
            return m_MyDeviceIndex;
        };
        RC SetDeviceIndex(UINT32 index) {
            m_MyDeviceIndex = index;
            return OK;
        };
    };

    DispTestDeviceInfo* LwrrentDevice();
    DispTestDeviceInfo* GetDeviceInfo(int index);
    RC BindDevice(UINT32 index);

    // Used by the ErrorLogger for filtering results (test configurable, see InstallErrorLoggerFilter)
    extern std::string s_ErrLoggerIgnorePattern;

    // Used SkipDsiSupervisor2Event callback
    extern string s_SkipDsiSupervisor2EventJSFunc;
    extern EventThread s_SkipDsiSupervisor2EventThread;
    void SkipDsiSupervisor2EventHandler();

    extern string s_SupervisorRestartEventJSFunc;
    extern EventThread s_SupervisorRestartEventThread;
    void SupervisorRestartEventHandler();

    extern string s_ExceptionRestartEventJSFunc;
    extern EventThread s_ExceptionRestartEventThread;
    void ExceptionRestartEventHandler();

    // Used by the callback function for VBIOS attention event
    extern string s_SetVbiosAttentionEventJSFunc;
    extern EventThread s_VbiosAttentionEventThread;
    void VbiosAttentionEventHandler();

    // Used by UnfrUpdEventHandler
    extern string s_SetUnfrUpdEventJSFunc;
    extern EventThread s_UnfrUpdEventThread;
    void UnfrUpdEventHandler();

    UINT32 LwrrentDisplayClass();

    RC GetChannelNum(UINT32 channelClass, UINT32 channelInstance, UINT32 *channelNum);

    bool bIsCoreChannel(UINT32 channelClass);

    // turn verbose debug messages on/off
    RC SetDebugMessageDisplay(bool);

    // Falcon related function
    UINT64 AllocDispMemory(UINT64 Size);
    UINT64 GetUcodePhyAddr();
    RC BootupFalcon(UINT64 bootVec, UINT64 bl_dmem_offset, string Falcon);
    RC BootupAFalcon(UINT32 hMem, UINT64 bootVec, UINT64 bl_dmem_offset);
    void StoreBin2FB(string filename,UINT64 SysAddr, UINT64 Offset, UINT32 fset);
    void LoadFB2Bin(string filename,UINT64 SysAddr, UINT64 Offset, int length);
    RC ConfigZPW(bool zpw_enable, bool zpw_ctx_mode, bool zpw_fast_mode, string ucode_dir);
    // initialization and setup
    RC Initialize(string, string);
    RC Setup(string, string, UINT32, LwRm::Handle *);
    RC GetClassType(string, UINT32 *, bool *);
    RC AllocatePushBufferMemoryAndContextDma(LwRmPtr, LwRm::Handle *,
                                             UINT64 *, void **,
                                             LwRm::Handle *, UINT32 *,
                                             Memory::Location *, string );
    RC AllocateErrorNotifierMemoryAndContextDma(LwRmPtr, UINT32 *,
                                                UINT64 *, void **,
                                                LwRm::Handle *, UINT32);
    RC AllocateChannelNotifierMemoryAndContextDma(LwRmPtr, LwRm::Handle *,
                                                  UINT64 *, void **,
                                                  LwRm::Handle *, UINT32 );
    RC SetPioChannelTimeout(LwRm::Handle, FLOAT64 TimeoutMs);
    RC Release(LwRm::Handle);
    void Cleanup();

    // Channel grab
    RC SwitchToChannel(LwRm::Handle hDispChan);

    // wait for channel idle state
    RC IdleChannelControl(UINT32 channelHandle, UINT32 desiredChannelStateMask,
                          UINT32 accel, UINT32 timeout);

    // set channel state for poll
    RC SetChnStatePollNotif(UINT32 ChannelHandle, UINT32 NotifChannelState,
                            UINT32 hNotifierCtxDma, UINT32 offset);
    RC SetChnStatePollNotif(string channelName, UINT32 channelInstance,
                            UINT32 NotifChannelState, UINT32 hNotifierCtxDma,
                            UINT32 offset);
    // set channel blocking
    RC SetChnStatePollNotifBlocking(UINT32 hNotifierCtxDma, UINT32 offset, UINT32 value);
    // get channel polling
    RC GetChnStatePollNotifRMPolling(UINT32 hNotifierCtxDma, UINT32 offset, FLOAT64 timeoutMs);

    // method enqueue and flush
    RC EnqMethod(LwRm::Handle, UINT32, UINT32);
    RC EnqMethodMulti(LwRm::Handle, UINT32, UINT32, const UINT32 *);
    RC EnqMethodNonInc(LwRm::Handle, UINT32, UINT32);
    RC EnqMethodNonIncMulti(LwRm::Handle, UINT32, UINT32, const UINT32 *);
    RC EnqMethodNop(LwRm::Handle);
    RC EnqMethodNoData(LwRm::Handle);
    RC EnqMethodJump(LwRm::Handle, UINT32);
    RC EnqMethodSetSubdevice(LwRm::Handle, UINT32 Subdevice);
    RC EnqMethodOpcode(LwRm::Handle, UINT32, UINT32, UINT32, const UINT32 *);
    RC EnqUpdateAndMode(LwRm::Handle, UINT32, UINT32, const char *);
    RC EnqCoreUpdateAndMode(LwRm::Handle, UINT32, UINT32, const char *, const char *, const char *, const char *);
    RC EnqCrcMode(UINT32 headnum, const char * Head_Mode);
    RC SetAutoFlush(LwRm::Handle, bool, UINT32);
    RC Flush(LwRm::Handle);

    // context dma creation
    RC AddDmaContext(LwRm::Handle, const char *, UINT32, void *, UINT64, UINT64);
    DmaContext *GetDmaContext(LwRm::Handle);
    void AddChannelContext(LwRm::Handle hCh, UINT32 Class, UINT32 Head, LwRm::Handle hErrCtx, LwRm::Handle hPBCtx);
    ChannelContextInfo *GetChannelContext(LwRm::Handle hCh);
    RC CreateNotifierContextDma(LwRm::Handle, string, LwRm::Handle *);
    RC CreateNotifierContextDma(LwRm::Handle, string, LwRm::Handle *, bool);
    RC CreateSemaphoreContextDma(LwRm::Handle, string, LwRm::Handle *);
    RC CreateContextDma(LwRm::Handle, UINT32, UINT32, string, LwRm::Handle *);
    RC DeleteContextDma(LwRm::Handle);
    RC SetContextDmaDebugRegs(LwRm::Handle);

    // memory read/write
    RC WriteVal32(LwRm::Handle, UINT32, UINT32);
    RC ReadVal32(LwRm::Handle, UINT32, UINT32 *);

    // VGA Registers Access
    RC ReadCrtc(UINT32, UINT32, UINT32*);
    RC WriteCrtc(UINT32, UINT32, UINT32);
    RC ReadAr(UINT32, UINT32, UINT32*);
    RC WriteAr(UINT32, UINT32, UINT32);
    RC ReadSr(UINT32, UINT32, UINT32*);
    RC WriteSr(UINT32, UINT32, UINT32);

    // Polling
    RC PollValue(LwRm::Handle, UINT32, UINT32, UINT32, FLOAT64 TimeoutMs);
    RC PollGreaterEqualValue(LwRm::Handle, UINT32, UINT32, UINT32, FLOAT64 TimeoutMs);
    RC PollValueAtAddr(PHYSADDR , UINT32, UINT32, UINT32, FLOAT64 TimeoutMs);
    RC PollScanlineGreaterEqualValue(UINT32 , UINT32, FLOAT64 TimeoutMs);
    RC PollScanlineLessValue(UINT32 , UINT32, FLOAT64 TimeoutMs);
    RC PollDone(LwRm::Handle, UINT32, UINT32, bool, FLOAT64 TimeoutMs);

    // crc initialization and options
//    static bool DispTest::CrcInterruptEventHandler(UINT32 intr0, UINT32 intr1, void *data);
    RC CrcInitialize(string, string, string);
    RC CrcSetOwner(string);
    RC CrcSetCheckTolerance(UINT32);
    RC CrcSetFModelCheckTolerance(UINT32);

    // crc setup
    RC CrcNoCheckHead(UINT32);
    RC CrcAddVgaHead(UINT32, const char *, UINT32, bool);
    RC CrcAddFcodeHead(UINT32, PHYSADDR *, const char *);
    RC CrcAddNotifier(UINT32, LwRm::Handle, UINT32, string, LwRm::Handle, UINT32, bool RefHead=false);
    RC CrcAddSemaphore(UINT32, LwRm::Handle, UINT32, UINT32, LwRm::Handle, UINT32, bool RefHead=false);
    RC CrcAddInterrupt(UINT32, string, UINT32);
    RC CrcAddInterruptAndCount(UINT32, string, UINT32, UINT32);

    // crc event update
    void CrcSetStartTime();
    void Crlwpdate();
    //RC VgaCrlwpdate();
    void CrlwpdateInterrupt(UINT32, UINT32);

    // crc output helper function
    void CrcWriteHeadList(FILE *);

    // crc output
    RC CrcWriteEventFiles();
    RC CrcWriteHeadEventFile();
    RC CrcWriteNotifierEventFile();
    RC CrcWriteSemaphoreEventFile();
    RC CrcWriteInterruptEventFile();

    // crc cleanup
    void CrcCleanup();

    RC BackdoorVgaInit();
    RC BackdoorVgaRelease();
    RC BackdoorVgaWr08(UINT32, UINT08);
    RC BackdoorVgaRd08(UINT32, UINT08*);
    RC BackdoorVgaWr16(UINT32, UINT16);
    RC BackdoorVgaRd16(UINT32, UINT16*);

    RC BackdoorVgaWr32(UINT32, UINT32);
    RC BackdoorVgaRd32(UINT32, UINT32*);
    RC FillVGABackdoor(string, UINT32, UINT32);
    RC SaveVGABackdoor(string, UINT32, UINT32);

    RC CrcInterruptProcessing();

    RC SetInterruptHandlerName(UINT32 intrHandleSrc, const string& funcname);
    RC GetInterruptHandlerName(UINT32 intrHandleSrc, string &funcname);

    // register a SkipDsiSupervisor2Event callback
    RC SetSkipDsiSupervisor2Event(string funcname);
    RC SetSupervisorRestartMode(UINT32 WhichSv, UINT32 RestartMode, bool exelwteRmSvCode,
                               string funcname, bool clientRestart);
    RC GetSupervisorRestartMode(UINT32 WhichSv, UINT32* RestartMode, bool* exelwteRmSvCode,
                               string *funcname, bool* clientRestart);
    RC SetExceptionRestartMode(UINT32 Channel, UINT32 Reason, UINT32 RestartMode,
                               bool Assert, bool ResumeArg, UINT32 Override, string funcname, bool manualRestart);
    RC GetExceptionRestartMode(UINT32 Channel, UINT32 Reason, UINT32* RestartMode, bool* Assert,
                               bool* ResumeArg, UINT32* Override, string *funcname, bool* manualRestart);
    RC SetSkipFreeCount(UINT32 Channel, bool skip);

    RC SetSwapHeads(bool swap);

    RC HandleSwapHead(UINT32& Method, UINT32& Data);

    RC SetUseHead(bool use_head, UINT32 vga_head);

    RC HandleUseHead(UINT32& Method, UINT32& Data);

    RC GetInternalTVRasterTimings(UINT32 Protocol,
                                  UINT32* PClkFreqKHz,     // [OUT]
                                  UINT32* HActive,         // [OUT]
                                  UINT32* VActive,         // [OUT]
                                  UINT32* Width,           // [OUT]
                                  UINT32* Height,          // [OUT]
                                  UINT32* SyncEndX,        // [OUT]
                                  UINT32* SyncEndY,        // [OUT]
                                  UINT32* BlankEndX,       // [OUT]
                                  UINT32* BlankEndY,       // [OUT]
                                  UINT32* BlankStartX,     // [OUT]
                                  UINT32* BlankStartY,     // [OUT]
                                  UINT32* Blank2EndY,      // [OUT]
                                  UINT32* Blank2StartY    // [OUT]
        );

    RC GetRGStatus(UINT32 Head,
                   UINT32* scanLocked,     // [OUT]
                   UINT32* flipLocked      // [OUT]
        );

    RC SetVPLLRef(UINT32 Head, UINT32 RefName, UINT32 Frequency);
    RC SetVPLLArchType(UINT32 Head, UINT32 ArchType);

    // Set the active subdevice index for the control calls.
    // By default the subdevice is 0 (broadcast)
    RC ControlSetActiveSubdevice(UINT32 subdeviceIndex);

    // GET/SET SOR/PIOR sequence
    RC ControlSetSorSequence(UINT32 OrNum,
                             bool pdPc_specified,        UINT32 pdPc,
                             bool puPc_specified,        UINT32 puPc,
                             bool puPcAlt_specified,     UINT32 puPcAlt,
                             bool normalStart_specified, UINT32 normalStart,
                             bool safeStart_specified,   UINT32 safeStart,
                             bool normalState_specified, UINT32 normalState,
                             bool safeState_specified,   UINT32 safeState,
                             bool SkipWaitForVsync,
                             bool SeqProgPresent,
                             UINT32* SequenceProgram);

    RC ControlGetSorSequence(UINT32 OrNum,
                             UINT32* puPcAlt, UINT32* pdPc, UINT32* pdPcAlt,
                             UINT32* normalStart, UINT32* safeStart,
                             UINT32* normalState, UINT32* safeState,
                             bool  GetSeqProg,    UINT32* seqProgram);

    RC ControlSetUseTestPiorSettings(UINT32 unEnable);

    RC ControlSetPiorSequence(UINT32 OrNum,
                              bool pdPc_specified,        UINT32 pdPc,
                              bool puPc_specified,        UINT32 puPc,
                              bool puPcAlt_specified,     UINT32 puPcAlt,
                              bool normalStart_specified, UINT32 normalStart,
                              bool safeStart_specified,   UINT32 safeStart,
                              bool normalState_specified, UINT32 normalState,
                              bool safeState_specified,   UINT32 safeState,
                              bool SkipWaitForVsync,
                              bool SeqProgPresent,
                              UINT32* SequenceProgram);
    RC ControlGetPiorSequence(UINT32 OrNum,
                              UINT32* puPcAlt, UINT32* pdPc, UINT32* pdPcAlt,
                              UINT32* normalStart, UINT32* safeStart,
                              UINT32* normalState, UINT32* safeState,
                              bool  GetSeqProg,    UINT32* seqProgram);

    RC ControlGetSorOpMode(UINT32 orNumber, UINT32 category,
                           UINT32* puTxda, UINT32* puTxdb,
                           UINT32* puTxca, UINT32* puTxcb,
                           UINT32* upper, UINT32* mode,
                           UINT32* linkActA, UINT32* linkActB,
                           UINT32* lvdsEn, UINT32* lvdsDual, UINT32* dupSync,
                           UINT32* newMode, UINT32* balanced,
                           UINT32* plldiv,
                           UINT32* rotClk, UINT32* rotDat);

    RC ControlSetSorOpMode(UINT32 orNumber, UINT32 category,
                           UINT32 puTxda, UINT32 puTxdb,
                           UINT32 puTxca, UINT32 puTxcb,
                           UINT32 upper, UINT32 mode,
                           UINT32 linkActA, UINT32 linkActB,
                           UINT32 lvdsEn, UINT32 lvdsDual, UINT32 dupSync,
                           UINT32 newMode, UINT32 balanced,
                           UINT32 plldiv,
                           UINT32 rotClk, UINT32 rotDat);

    RC ControlSetSorPwmMode(UINT32 orNumber, UINT32 targetFreq,
                            UINT32 *actualFreq, UINT32 div,
                            UINT32 resolution, UINT32 dutyCycle,
                            UINT32 flags);

    RC ControlGetDacPwrMode(UINT32 orNumber, UINT32* normalHSync,
                            UINT32* normalVSync, UINT32* normalData,
                            UINT32* normalPower, UINT32* safeHSync,
                            UINT32* safeVSync, UINT32* safeData,
                            UINT32* safePower);

    RC ControlSetDacPwrMode(UINT32 orNumber, UINT32 normalHSync,
                            UINT32 normalVSync, UINT32 normalData,
                            UINT32 normalPower, UINT32 safeHSync,
                            UINT32 safeVSync, UINT32 safeData,
                            UINT32 safePower, UINT32 flags);

    RC ControlSetRgUnderflowProp(UINT32 head, UINT32 enable,
                                 UINT32 clearUnderflow, UINT32 mode);

    RC ControlSetSemaAcqDelay(UINT32 delayUs);

    RC ControlSetPiorOpMode(UINT32 orNumber, UINT32 category,
                            UINT32 clkPolarity, UINT32 clkMode,
                            UINT32 clkPhs, UINT32 unusedPins,
                            UINT32 polarity, UINT32 dataMuxing,
                            UINT32 clkDelay, UINT32 dataDelay,
                            UINT32 DroMaster, UINT32 DroPinset);

    RC ControlGetPiorOpMode(UINT32 orNumber, UINT32* category,
                            UINT32* clkPolarity, UINT32* clkMode,
                            UINT32* clkPhs, UINT32* unusedPins,
                            UINT32* polarity, UINT32* dataMuxing,
                            UINT32* clkDelay, UINT32* dataDelay);

    RC ControlSetOrBlank(UINT32 orNumber, UINT32 orType,
                         UINT32 transition, UINT32 status,
                         UINT32 waitForCompletion);

    RC ControlSetSfBlank(UINT32 sfNumber, UINT32 transition,
                         UINT32 status, UINT32 waitForCompletion);

    RC ControlSetDacConfig(UINT32 orNumber,
                           UINT32 cpstDac0Src,
                           UINT32 cpstDac1Src,
                           UINT32 cpstDac2Src,
                           UINT32 cpstDac3Src,
                           UINT32 compDac0Src,
                           UINT32 compDac1Src,
                           UINT32 compDac2Src,
                           UINT32 compDac3Src,
                           UINT32 driveSync);

    RC ControlSetErrorMask(UINT32 channelHandle,
                           UINT32 method,
                           UINT32 mode,
                           UINT32 errorCode);

    RC ControlSetDsiInstMem0(UINT32 target,
                             UINT32 address,
                             UINT32 size);

    // Set the call back function for VBIOS attention event
    RC ControlSetVbiosAttentionEvent(string funcname);

    RC ControlSetRgFliplockProp(UINT32 head,
                                UINT32 maxSwapLockoutSkew,
                                UINT32 swapLockoutStart);

    RC ControlGetPrevModeSwitchFlags(UINT32 WhichHead, UINT32* blank, UINT32* shutdown);

    RC ControlGetDacLoad(UINT32  orNumber, UINT32* mode,
                         UINT32* valDCCrt, UINT32* valACCrt,
                         UINT32* perDCCrt, UINT32* perSampleCrt,
                         UINT32* valDCTv,  UINT32* valACTv,
                         UINT32* perDCTv,  UINT32* perSampleTv,
                         UINT32* perAuto,
                         UINT32* load,     UINT32* status);

    RC ControlSetDacLoad(UINT32 orNumber, UINT32 mode,
                         UINT32 valDCCrt, UINT32 valACCrt,
                         UINT32 perDCCrt, UINT32 perSampleCrt,
                         UINT32 valDCTv,  UINT32 valACTv,
                         UINT32 perDCTv,  UINT32 perSampleTv,
                         UINT32 perAuto,
                         UINT32 flags);

    RC ControlGetOverlayFlipCount(UINT32 channelInstance, UINT32* value);

    RC ControlSetDmiElv(UINT32 channelInstance, UINT32 what, UINT32 value);

    RC ControlPrepForResumeFromUnfrUpd();

    RC ControlSetUnfrUpdEvent(string funcname);

    RC ControlGetOverlayFlipCount(UINT32 channelInstance, UINT32* value);

    RC ControlSetOverlayFlipCount(UINT32 channelInstance, UINT32 forceCount);

    RC GetCrcInterruptCount(string InterruptName, UINT32* count);

    RC SetHDMIEnable(UINT32 displayId, bool enable);

    RC SetHDMISinkCaps(UINT32 displayId, UINT32 caps);

    RC GetHDMICapableDisplayIdForSor(UINT32 sorIndex, UINT32 *pRtnDisplayId);

    // Get pinset settings.
    RC GetPinsetCount(UINT32 *puPinsetCount);
    RC GetPinsetLockPins(UINT32 uPinset, UINT32 *puScanLockPin, UINT32 *puFlipLockPin);
    RC GetStereoPin(UINT32 *puStereoPin);
    RC GetExtSyncPin(UINT32 *puExtSyncPin);

    // set/get the Dsi force bits
    RC SetDsiForceBits(UINT32 WhichHead,
                       UINT32 ChangeVPLL, UINT32 NochangeVPLL,
                       UINT32 Blank, UINT32 NoBlank,
                       UINT32 Shutdown, UINT32 NoShutdown,
                       UINT32 NoBlankShutdown, UINT32 NoBlankWakeup
        );
    RC GetDsiForceBits(UINT32 WhichHead,
                       UINT32* ChangeVPLL, UINT32* NochangeVPLL,
                       UINT32* Blank, UINT32* NoBlank,
                       UINT32* Shutdown, UINT32* NoShutdown,
                       UINT32* NoBlankShutdown, UINT32* NoBlankWakeup
        );

    // support for low power verication
    RC LowPower();
    RC LowerPower();
    RC LowestPower();
    RC BlockLevelClockGating();
    RC SlowLwclk();

    RC ModifyTestName(string sTestName);
    RC ModifySubtestName(string sSubtestName);
    RC AppendSubtestName(string sSubtestNameAdmendment);

    //Support for Display Port multi-stream verification. These will only be supported in DTB
    RC TimeSlotsCtl (bool value);
    RC ActivesymCtl (bool value);
    RC UpdateTimeSlots (UINT32 sornum, UINT32 headnum);

    //Enable or disable flush mode in SOR. This is used if RM wants to link train without disconnecting SOR from head. Bug : 562336
    RC sorSetFlushMode (UINT32 sornum,
                        bool enable,
                        bool bImmediate,
                        bool bFireAndForget,
                        bool bForceRgDiv,
                        bool bUseBFM
                        );

    //Disable code that automatically handles displayID
    RC DisableAutoSetDisplayId();
    UINT32 LookupDisplayId(UINT32 ortype, UINT32 ornum, UINT32 protocol, UINT32 headnum, UINT32 streamnum);

    // DP link training
    RC DpCtrl(UINT32 sornum,
              UINT32 protocol,
              bool laneCount_Specified,
              UINT32 *pLaneCount,
              bool *pLaneCount_Error,
              bool linkBw_Specified,
              UINT32 *pLinkBw,
              bool *pLinkBw_Error,
              bool enhancedFraming_Specified,
              bool enhancedFraming,
              bool isMultiStream,
              bool disableLinkConfigCheck,
              bool fakeLinkTrain,
              bool *pLinkTrain_Error,
              bool *pIlwalidParam_Error,
              UINT32 *pRetryTimeMs);

    // Setup single-head dual stream mode
    RC ControlSetSfDualMst(UINT32 sorNum, UINT32 headNum, UINT32 enableDualMst, UINT32 isDPB);

    // Setup SOR XBAR to connect SOR and analog link
    RC ControlSetSorXBar(UINT32 sorNum, UINT32 protocolNum, UINT32 linkPrimary, UINT32 linkSecondary);

    // Helper function for tests to add a ErrorLogger filter
    void InstallErrorLoggerFilter(std::string pattern);
    bool ErrorLogFilter(const char *errMsg);

}
#endif      // DISPTEST_PRIVATE

#endif /* DISP_TEST_H */

