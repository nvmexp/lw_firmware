/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @brief Hopper Confidential Compute work launch test with SEC2 and copy engine channels.
 *
 * See: //hw/doc/gpu/hopper/hopper/design/Functional_Descriptions/Hopper_526_ConfidentialComputing.docx
 *
 * This test implements the FD WL/LCIC channels as described.  The scheme can be optmized to specific
 * use cases, but the test will stick to the generalized scheme for now.
 *
 * Written directly with the RMAPI and not mods/rmtest helpers.  Ideally this helps code
 * port to RM/other platforms as the helpers are a bit different everywhere.  Local helpers
 * are implemented to keep the code size in check.
 *
 * Only supports the Hopper CC model lwrrently.
 *
 * Bootstrap channel - This is an initialization channel to copy data into protected memory.  Since it
 *     goes through SEC2 it is relatively slow. The intent is to only use this to update the WL CE
 *     channels which provide higher bandwidth.  Until SEC2 is ready we also support CE for testing the
 *     work flow.
 *
 * Fed channel - This is a working channel that is fed its methods through the WL channel.  The test
 *     will support the CE, gr or a video engine.  The test only uses backend semaphores on each engine
 *     to keep things simple.
 *
 * notificationProxy channel - This is a CE channel to allow the Fed channel to send push buffer
 *     segment notification back to the CPU.
 *
 * WL Channel -  The work launch channel copies encrypted CE methods from unprotected system memory
 *     into a video memory push buffer and exelwtes them.  This is primarily used to copy encrypted
 *     push buffer segments from unprotected system memory into protected memory.  But it can also
 *     be used to perform encrypted copies back to system memory.  These copies are requied to
 *     return results to the CPU.
 *
 *     The WL channel is tricky in that we cannot update GP_PUT from the CPU efficiently when CC
 *     is enabled.  To get around this we initialize the GP_PUT to indicate the channel is pending.
 *     When starting a channel like this it looks like the RM generates a doorbell, and the channel
 *     may run before we are ready.  To get around this issue we run one segment during initialization.
 *
 *     To aid in debugging this shakey construct, the test writes canary methods that will generate
 *     a page fault into buffers that are not ready.  If these segments are exelwted mistakenly, they
 *     will generate a page fault with the segment ID in the faulting VA.  This only works for
 *     unencrypted channels as we do not have to manage the IV.  With encryption we will get decryption
 *     failures if the WL channel gets ahead.
 *
 *     The WL channel updates the LCI channel to run one segment when it is done.
 *
 *     The CPU is responsible for ringing the WL doorbell.  The driver must be careful to only allow
 *     one pending WL segment to execute at a time. Otherwise the doorbell ring can be lost if it
 *     arrives before the LCI channel updates GP_PUT, and the channel will hang.  This adds a potential
 *     bubble to the pipeline, but hopefully the WL can keep the work engine gpfifo pending.
 *
 * LCI Channel - This channel has two responsibilities.  It updates the GP_PUT of the WL segment
 *     by 2, and notifies the CPU of the WL channel progress.
 *
 * Flow for fedTest with WL channel:
 *  CPU
 *     1) Driver (fedTest) writes methods with fedWriteMethods() to staging buffer
 *     2) Driver calls fedFlush() to execute these methods on the target engine
 *     3) fedFlush() adds notification methods to the engine push buffer
 *        a) gp_get tracking release to vidmem
 *        b) update notification proxy gp_put
 *        c) ring notification proxy channel to forward 3a data to CPU
 *     4) fedFlush() updates push buffer segment with wlCopyProtectedToVid()
 *        a) Encrypt copy protected buffer to unprotected DMA buffer
 *        b) queue CE methods to copy buffer to engine push buffer
 *     5) fedFlush() updates fed channels gpfifo entry with wlCopyProtectedToVid()
 *        a) 64-bit copy generates a CE release methods to update with inline data
 *     6 fedFlush() updates fed channels GP_PUT with wlCopyProtectedToVid()
 *        a) 32-bit copy generates a CE release methods to update with inline data
 *     6) fedFlush() enqueues CE methods to ring engine doorbell with esched methodds
 *     7) fedFlush() calls wlFlush()
 *     8) wlFlush()
 *        a) Enqueue END_OF_PB_SEGMENT in pushbuffer
 *        b) Encrypt copy WL pb segment to unproteced system memory buffer
 *        c) Ring WL doorbell
 *  GPU
 *     9) WL will exelwtes fixed even push buffer segment
 *        a) Decrypt WL CE methods from sysmem to WL odd push buffer in vidmem
 *        b) Update LCI channels GP_PUT and ring doorbell
 *     10) WL will execute odd push buffer segment
 *        a) Copy fed channel push buffer segment
 *        b) Update fed channel gpfifo entry
 *        c) Update fed channel userd
 *        d) Ring fed channel doorbell
 *     11) LCI Channel will run
 *        a) Update system memory with encrypted progress notification
 *        b) Update WL channel GP_PUT value
 *     12) Fed channel will execute push buffer segment
 *     13) Notify CPU that fed buffer has exelwted
 *        a) Release engine semaphore to video memory
 *        b) Update notification proxy GP_PUT
 *        c) Ring 
 *
 *  CPU
 *     14) Poll for buffer notification from notification proxy
 */

#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "lwRmApi.h"
#include "lwos.h"

#include "ctrl/ctrlc86f.h"

#include "class/cl0000.h"    // LW01_NULL_OBJECT
#include "class/cl0002.h"    // LW01_NULL_OBJECT
#include "class/cl003e.h"    // LW01_MEMORY_SYSTEM
#include "class/cl0040.h"    // LW01_MEMORY_LOCAL_USER
#include "class/cl0070.h"    // LW01_MEMORY_VIRTUAL
#include "class/cla06c.h"    // KEPLER_CHANNEL_GROUP_A
#include "class/cla06fsubch.h"
#include "class/cla0b5sw.h"  // LWA0B5_ALLOCATION_PARAMETERS

#include "class/clc661.h"    // HOPPER_USERMODE_A
#include "class/clc8b5.h"    // HOPPER_DMA_COPY_A
#include "class/clcbc0.h"    // HOPPER_COMPUTE_A
#include "class/clc86f.h"    // HOPPER_CHANNEL_GPFIFO_A

static void memzero(void *, LwU32 size); // Uses MEM_WR32() for simulation

static LwHandle NextHandle;

//
// Notification in system memory.  Will allocate a full page of system memory
// so there is some memory left over for other uses @ scratchOffset.
//
struct notify
{
    LwHandle        hClient;
    LwHandle        hDevice;
    LwHandle        hVASpace;       // For GPU mapping to scratch area

    LwHandle        hMemory;        // Base memory object
    LwU64           limit;
    void           *p;
    LwU64           gpuVA;

#define MAX_NOTIFIERS    8
    LwHandle        hNotify[MAX_NOTIFIERS];
    LwNotification *pNotify[MAX_NOTIFIERS];
    LwU32           n;

    LwU32           scratchOffset;
};
static RC allocNotify(LwHandle, LwHandle, LwHandle, LwU32 n, struct notify *);
static void freeNotify(struct notify *);

struct client_config
{
    LwHandle hClient;
    LwHandle hDevice;
    LwHandle hSubDevice;
    LwHandle hVASpace;

    LwU32  usermodeClass;
    LwU32 channelClass;
    LwU32 ceClass;
    LwU32 computeClass;

    struct
    {
        LwU8  *p;
        LwU64  gpuVA;
    } usermode;
};

#define MAX_SEGMENTS 32     /* Should be malloc()ed, but keep simple for now */

//
// Base channel definition used by other channel types. Does not include
// progress tracking notification as that is different for each type.
//
struct channel
{
    struct client_config *pConfig;
    char    *name;

    bool bForceVid;
    bool bCpuVisible;

    struct notify notifiers;

    LwHandle hChannel;
    LwU32    channelToken;
    LwU32    gpPut;

    LwHandle hEngine;
    LwU32    subch;
    LwU32    setObjectData;

    struct
    {
        LwHandle handle;
        LwU32    entries;
        LwU64    size;
        void    *p;
        volatile LwU64 *pGp;
        LwU64    gpuVA;
    } gpfifo;

    struct
    {
        LwHandle       handle;
        LwU64          size;
        void          *p;
        LwU64          gpuVA;
        Lwc86fControl *pControl;
    } userd;

    struct
    {
        LwHandle handle;
        LwU64    size;
        LwU32    index;
        void    *p;
        LwU64    gpuVA;
        struct
        {
            volatile LwU32 *p;
            LwU64  gpuVA;
        } segment[MAX_SEGMENTS];
    } pb;

    struct
    {
        LwU32 put;
        LwU32 get;
        volatile LwU32 *p;
        LwU64 gpuVA;
    } sequence;
};
static RC   channelAlloc(struct channel *, struct client_config *, char *,
                         LwHandle hChannelGroup, LwU32 engineType,
                         bool, bool,
                         LwU32, LwU32);
static RC   channelSchedule(struct channel *);
static RC   channelDisable(struct channel *);
static void channelFlush(struct channel *);
static void channelRingDoorbell(struct channel *);
static void channelFree(struct channel *);

typedef void initMethods(struct channel *, LwU32 *pMethods, LwU32 *pCount);
typedef void releaseMethods(struct channel *, LwU32 *pMethods, LwU32 *pCount, LwU64 gpuVA, LwU32 *p, LwU32 flags);
#define RELEASE_MEMBAR       BIT(0)
#define RELEASE_SYSMEMBAR    BIT(1)
#define RELEASE_64BIT        BIT(2)

static releaseMethods eschedReleaseMethods;

static initMethods ceInitMethods;
static void ceCopyMethods(struct channel *, LwU32 *, LwU32 *, LwU64 srcVA, LwU64 dstVA, LwU32 size, bool bSysFlush);
static releaseMethods ceReleaseMethods;

static initMethods computeInitMethods;
static releaseMethods computeReleaseMethods;

// Channel helpers for completion checking. Depends on caller managing get/put.
static bool channelEmpty(void *vc);
static bool channelNotFull(void *vc);

// RmApi helper functions
static RC   allocSysmem(struct channel *, LwHandle *, LwU64 *pSize, void **ppCpu, LwU64 *pGpuVA);
static RC   allocVidmem(struct channel *, LwHandle *, LwU64 *pSize, void **ppCpu, LwU64 *pGpuVA);
static void freeAlloc(struct channel *, LwHandle, void *pCpu, LwU64 gpuVA);
static RC   mapCpu(struct channel *, LwHandle, LwU64, void **);
static void unmapCpu(struct channel *, LwHandle, void *);
static RC   mapGpu(struct channel *, LwHandle, LwU64, LwU64 *);
static void unmapGpu(struct channel *, LwHandle, LwU64);

//
// Secure bootstrap channel through SEC (or CE for testing/bring-up)
//
struct bootstrap
{
    struct client_config *pConfig;
    LwHandle hChannelGroup;
    struct notify notifiers;

    struct channel channel;

    bool bUseUnencryptedCE;   // Use uncrypted CE until SEC is ready

    struct
    {
        LwHandle handle;
        LwU64    size;
        void    *p;
        LwU64    gpuVA;
        struct
        {
            struct
            {
                LwU32 *p;
                LwU64 gpuVA;
            } data;
        } segment[MAX_SEGMENTS];
    } staging;
};
#define BOOTSTRAP_ALIGNMENT 256
static RC bootstrapAlloc(struct bootstrap *, struct client_config *, LwU64 stagingBufferSize);
static RC bootstrapCopyProtectedToVid(struct bootstrap *, LwU32 *, LwU64 gpuVA, LwU32 size);
static RC bootstrapCopy(struct bootstrap *, LwU64 srcVA, LwU64 dstVA, LwU32 size);
static RC bootstrapZeroVid(struct bootstrap *, LwU64, LwU32);
static RC bootstrapRingGivenDoorbell(struct bootstrap *, struct channel *);
static RC bootstrapWriteMethods(struct bootstrap *, LwU32 *methods, LwU32 count);
static RC bootstrapWait(struct bootstrap *, bool bEmpty);
static RC bootstrapFlush(struct bootstrap *);
static void bootstrapFree(struct bootstrap *);

//
// Hopper work launch channel pair as described in the Confidential Compute GFD.
//
struct wl
{
    struct client_config *pConfig;

    LwHandle hChannelGroup;
    struct notify notifiers;
    struct channel channel;

    // Push buffer staging
    struct
    {
        LwU32 *pSelwre;             // Secure non-DMA buffer

        LwHandle handle;            // System memory DMA buffer
        void *p;
        LwU64 gpuVA;

        LwU64    size;
        LwU32    entries;
        LwU32    index;
        LwU32    dataindex;
        LwU32    put;
        LwU32    get;

        struct
        {
            struct
            {
                LwU32 *pSelwre;
                LwU32 *p;
                LwU64  gpuVA;
            } method;
            struct
            {
                LwU32 *p;
                LwU64 gpuVA;
            } data;
        } segment[MAX_SEGMENTS];
    } staging;

    struct
    {
        struct channel channel;
    } lci;
};
static RC   wlAlloc(struct wl *, struct client_config *, struct bootstrap *,
                    LwU32 pbEntries, LwU32 pbSize);
static void wlMethodStagingCanary(struct wl *, LwU32 index);
static RC   wlCopyProtectedToVid(struct wl *, LwU32 *pSrc, LwU64 dstVA, LwU32 size, bool bMembar);
static RC   wlCopy(struct wl *, LwU64 srcVA, LwU64 dstVA, LwU32 size, bool bMembar);
static RC   wlRingGivenDoorbell(struct wl *, struct channel *);
static RC   wlWriteMethods(struct wl *, LwU32 *pMethods, LwU32 count);
static RC   wlFlush(struct wl *);
static RC   wlWait(struct wl *, bool bEmpty);
static void wlFree(struct wl *);

static RC copyTest(struct bootstrap *, struct wl *);

//
// Secure CE based notification proxy channel.  Fowards
// notification from a video memory CC channel
// to system memory.
//
struct notificationProxy
{
    struct client_config *pConfig;

    LwHandle hChannelGroup;
    struct notify notifiers;
    struct channel channel;

    LwU64 targetVA;

    struct
    {
        LwHandle handle;
        LwU64    size;
        LwU64    baseVA;
        LwU32    segments;
        LwU64    gpuVA[MAX_SEGMENTS];
    } proxy;
};
static RC   notificationProxyAlloc(struct notificationProxy *, struct client_config *pConfig,
                                   struct bootstrap *,
                                   LwU64 targetVA, LwU32 pbEntries);
static void  notificationProxyNext(struct notificationProxy *, LwU64 *, LwU32 *, LwU64 *);
static void  notificationProxyFree(struct notificationProxy *);

//
// CC channel that is "fed" from WL process.
//
struct fed
{
    struct client_config *pConfig;

    LwHandle hChannelGroup;
    struct notify notifiers;

    struct notificationProxy notificationProxy;

    // Single PB segment of method staging
    struct
    {
        LwU64 gpfifo[MAX_SEGMENTS];
        struct
        {
            LwU32 size;
            LwU32 *p;
            LwU32 index;
        } method;
    } staging;

    struct channel channel;

    LwU32  fedClass;
    bool   bUseCE;
    bool   bUseEsched;

    struct bootstrap *pBootstrap;
    struct wl *pWl;

    releaseMethods *pReleaseMethods;
    initMethods *pInitMethods;
    LwU32 nopMethod;
};
static RC   fedAlloc(struct fed *, struct client_config *,
                     struct bootstrap *, struct wl *,
                     LwU32 fedClass,
                     LwU32 pbEntries, LwU32 pbSize);
static RC   fedWriteMethods(struct fed *, LwU32 *, LwU32 count);
static RC   fedFlush(struct fed *);
static RC   fedWait(struct fed *, bool bEmpty);
static void fedFree(struct fed *);
static RC   fedTest(struct fed *);

class CCLaunchTest : public RmTest
{
public:
    CCLaunchTest();
    virtual ~CCLaunchTest()
    {
    };
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    struct client_config config;

    struct
    {
        LwHandle handle;
        LwU64    size;
    } usermode;

    // Bootstrapping Channel on SEC for production, CE for development
    struct bootstrap bootstrap;

    // Hopper work launch channels
    struct wl wl;

    // CE test channel to be fed by wl channels
    struct fed fed;
};

#define LW_METHOD_N(SubCh, Method, N)  ( DRF_DEF(C86F, _DMA, _SEC_OP, _INC_METHOD)      |    \
                                         DRF_NUM(C86F, _DMA, _METHOD_COUNT, (N))        |    \
                                         DRF_NUM(C86F, _DMA, _METHOD_SUBCHANNEL, SubCh) |    \
                                         DRF_NUM(C86F, _DMA, _METHOD_ADDRESS, (Method) >> 2) )

#define LW_METHOD(SubCh, Method)       LW_METHOD_N(SubCh, Method, 1)

CCLaunchTest::CCLaunchTest()
{
    SetName("CCLaunchTest");

    NextHandle = 0xdead0000;

    config.hClient = LW01_NULL_OBJECT;
    config.hDevice = LW01_NULL_OBJECT;
    config.hSubDevice = LW01_NULL_OBJECT;
    config.hVASpace = LW01_NULL_OBJECT;
}

string CCLaunchTest::IsTestSupported()
{
    if (IsClassSupported(HOPPER_DMA_COPY_A))
    {
        config.usermodeClass = HOPPER_USERMODE_A,
        config.channelClass = HOPPER_CHANNEL_GPFIFO_A;
        config.ceClass = HOPPER_DMA_COPY_A;
        config.computeClass = HOPPER_COMPUTE_A;
        return RUN_RMTEST_TRUE;
    }

    return "Unsupported classes";
}

RC CCLaunchTest::Setup()
{
    LW_STATUS status;
    RC rc;

    CHECK_RC(InitFromJs());

    status = LwRmAllocRoot(&config.hClient);
    CHECK_RC(RmApiStatusToModsRC(status));

    LW0080_ALLOC_PARAMETERS allocDeviceParams = { 0 };
    config.hDevice = NextHandle++;
    allocDeviceParams.deviceId = GetBoundGpuDevice()->GetDeviceInst();
    allocDeviceParams.hClientShare = config.hClient;
    status = LwRmAlloc(config.hClient,
                       config.hClient,
                       config.hDevice,
                       LW01_DEVICE_0,
                       &allocDeviceParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    LW2080_ALLOC_PARAMETERS allocSubDeviceParams = { 0 };
    config.hSubDevice = NextHandle++;
    allocSubDeviceParams.subDeviceId = 0;
    status = LwRmAlloc(config.hClient,
                       config.hDevice,
                       config.hSubDevice,
                       LW20_SUBDEVICE_0,
                       &allocSubDeviceParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vasAllocParams = { 0 };
    config.hVASpace = NextHandle++;
    status = LwRmAlloc(config.hClient,
                       config.hDevice,
                       config.hVASpace,
                       LW01_MEMORY_VIRTUAL,
                       &vasAllocParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    // Allocate and map usermode map interface
    LW_HOPPER_USERMODE_A_PARAMS usermodeParams = { 0 };
    usermodeParams.bBar1Mapping = LW_TRUE;
    usermode.handle = NextHandle++;
    usermode.size = DRF_SIZE(LWC661);
    status = LwRmAlloc(config.hClient,
                       config.hSubDevice,
                       usermode.handle,
                       config.usermodeClass,
                       &usermodeParams);
    CHECK_RC(RmApiStatusToModsRC(status));
    status = LwRmMapMemory(config.hClient, config.hSubDevice, usermode.handle, 0, usermode.size - 1, (void **)&config.usermode.p, 0);
    CHECK_RC(RmApiStatusToModsRC(status));
    config.usermode.gpuVA = 0;
    status = LwRmMapMemoryDma(config.hClient, config.hDevice, config.hVASpace, usermode.handle, 0, usermode.size - 1,
                              DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE), &config.usermode.gpuVA);
    CHECK_RC(RmApiStatusToModsRC(status));

    return rc;
}

/*!
 * @brief Run the Hopper CC test.
 */
RC CCLaunchTest::Run()
{
    const LwU32 pbEntries = 8;   // 8 PB gpfifo entries
    const LwU32 pbSize = 4096;   // 4K per PB entry
    RC rc = OK;

    // Allocate bootstrap and run sanity test
    CHECK_RC(bootstrapAlloc(&bootstrap, &config, 4096));

    // Only run unit tests during development/debugging
    if (0)
    {
        CHECK_RC(copyTest(&bootstrap, NULL));

        // Run fed channel with bootstrap channel. This helps bring-up the test.
        CHECK_RC(fedAlloc(&fed, &config, &bootstrap, NULL,
                           config.ceClass,
                           pbEntries, pbSize));
        CHECK_RC_CLEANUP(fedTest(&fed));
        fedFree(&fed);
    }

    // Allocate WL channels and run a simple test.
    CHECK_RC_CLEANUP(wlAlloc(&wl, &config, &bootstrap, pbEntries, pbSize));

    // Only run unit tests during development/debugging
    if (0)
    {
        // Simple wl channel test
        CHECK_RC_CLEANUP(copyTest(NULL, &wl));
    }

    // Allocate a new fed channel and test with WL bootstrap on isolated GR engine
    CHECK_RC_CLEANUP(fedAlloc(&fed, &config, &bootstrap, &wl,
                              config.computeClass,
                              pbEntries, pbSize));
    bootstrapFree(&bootstrap);    // We should no longer need bootstrap channel
    CHECK_RC_CLEANUP(fedTest(&fed));
    fedFree(&fed);
    wlFree(&wl);

    return OK;

Cleanup:
    Printf(Tee::PriHigh, "Error running fedTest! \n");
    //
    // Sloppy clean-up.  We need to clean up fed channel on timeout and
    // disable pending WL channel before free to avoid RC. Let RM teardown
    // handle the rest.
    //
    fedFree(&fed);
    wlFree(&wl);

    return rc;
}

RC CCLaunchTest::Cleanup()
{
    LwRmUnmapMemoryDma(config.hClient,
                       config.hDevice,
                       config.hVASpace,
                       usermode.handle,
                       0,
                       config.usermode.gpuVA);
    LwRmUnmapMemory(config.hClient, config.hSubDevice, usermode.handle, config.usermode.p, 0);
    LwRmFree(config.hClient, config.hSubDevice, usermode.handle);

    LwRmFree(config.hClient, config.hDevice, config.hVASpace);
    LwRmFree(config.hClient, config.hClient, config.hSubDevice);
    LwRmFree(config.hClient, config.hClient, config.hDevice);
    LwRmFree(config.hClient, config.hClient, config.hClient);

    return OK;
}

/*!
 * @brief bootstrap channel support
 *
 * Support allocating a CE or Hopper SEC2 channel to copy methods form UMD
 * to target work creation CE channels.
 *
 * CE support is only for initial development prior to SEC2 avaliablity.
 */
static RC
bootstrapAlloc
(
    struct bootstrap *pBootstrap,
    struct client_config *pConfig,
    LwU64 stagingSize
)
{
    struct channel *pChannel = &pBootstrap->channel;
    LW_STATUS status;
    LwU64 totalSize;
    RC rc = OK;

    pBootstrap->pConfig = pConfig;

    // TODO: SEC implementation
    pBootstrap->bUseUnencryptedCE = true;

    // Allocate channel group error notifier
    CHECK_RC(allocNotify(pConfig->hClient, pConfig->hDevice, pConfig->hVASpace, 1,
                         &pBootstrap->notifiers));

    // Allocate channel group
    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS ChannelGroupParams = {0};
    pBootstrap->hChannelGroup = NextHandle++;
    ChannelGroupParams.hObjectError = pBootstrap->notifiers.hNotify[0];
    ChannelGroupParams.hVASpace = LW01_NULL_OBJECT;
    if (pBootstrap->bUseUnencryptedCE)
        ChannelGroupParams.engineType = LW2080_ENGINE_TYPE_COPY(1);
    else
        ChannelGroupParams.engineType = LW2080_ENGINE_TYPE_SEC2;
    status = LwRmAlloc(pConfig->hClient,
                       pConfig->hDevice,
                       pBootstrap->hChannelGroup,
                       KEPLER_CHANNEL_GROUP_A,
                       &ChannelGroupParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    CHECK_RC(channelAlloc(pChannel, pConfig, "bootstrap",
                          pBootstrap->hChannelGroup, ChannelGroupParams.engineType,
                          false, true, 8, 4096));

    // Allocate engine object
    pChannel->hEngine = NextHandle++;
    if (pBootstrap->bUseUnencryptedCE)
    {
        LWA0B5_ALLOCATION_PARAMETERS ceAllocParams = { LWA0B5_ALLOCATION_PARAMETERS_VERSION_1, 0 };
        ceAllocParams.version =  LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
        ceAllocParams.engineType = ChannelGroupParams.engineType;
        status = LwRmAlloc(pConfig->hClient,
                           pChannel->hChannel,
                           pChannel->hEngine,
                           pConfig->ceClass,
                           &ceAllocParams);
        CHECK_RC(RmApiStatusToModsRC(status));

        pChannel->subch = LWA06F_SUBCHANNEL_COPY_ENGINE;
    }
    else
    {
        // TODO: SEC2 implementation
    }

    CHECK_RC(channelSchedule(pChannel));

    pBootstrap->staging.size = stagingSize;
    totalSize = pBootstrap->staging.size * pChannel->gpfifo.entries;
    CHECK_RC(allocSysmem(pChannel, &pBootstrap->staging.handle, &totalSize,
                         &pBootstrap->staging.p,
                         &pBootstrap->staging.gpuVA));
    for (LwU32 i = 0; i < pChannel->gpfifo.entries; i++)
    {
        pBootstrap->staging.segment[i].data.p = (LwU32 *)(((LwU8 *)pBootstrap->staging.p) + (i * pBootstrap->staging.size));
        pBootstrap->staging.segment[i].data.gpuVA = pBootstrap->staging.gpuVA + (i * pBootstrap->staging.size);
    }

    // Set our engine object
    LwU32 methods[32];
    LwU32 i = 0;
    if (pBootstrap->bUseUnencryptedCE)
    {
        ceInitMethods(pChannel, methods, &i);
    }
    else
    {
        // TODO: SEC2 implementation
    }
    bootstrapWriteMethods(pBootstrap, methods, i);

    return rc;
}

static RC
bootstrapCopy(struct bootstrap *pBootstrap, LwU64 srcVA, LwU64 dstVA, LwU32 size)
{
    struct channel *pChannel = &pBootstrap->channel;
    LwU32 methods[32];
    LwU32 i = 0;
    RC rc;

    // SEC has very restrictive alignment requirements
    if ((srcVA|dstVA|size) & (BOOTSTRAP_ALIGNMENT-1))
        return RC::ILWALID_ADDRESS;

    if (pBootstrap->bUseUnencryptedCE)
    {
        ceCopyMethods(pChannel, methods, &i, srcVA, dstVA, size, false);
    }
    else
    {
        // TODO: SEC2 implementation
    }

    CHECK_RC(bootstrapWriteMethods(pBootstrap, methods, i));

    // Move to next segment as we are using the staging buffer
    CHECK_RC(bootstrapFlush(pBootstrap));

    return rc;
}

static RC
_bootstrapCopyStagingToVid(struct bootstrap *pBootstrap, LwU64 gpuVA, LwU32 size)
{
    struct channel *pChannel = &pBootstrap->channel;

    return bootstrapCopy(pBootstrap, pBootstrap->staging.segment[pChannel->gpPut].data.gpuVA, gpuVA, size);
}

static RC
bootstrapCopyProtectedToVid(struct bootstrap *pBootstrap, LwU32 *p, LwU64 gpuVA, LwU32 size)
{
    struct channel *pChannel = &pBootstrap->channel;
    LwU32 size32 = size / sizeof(LwU32);
    RC rc = OK;

    if (size > pBootstrap->staging.size)
        return RC::BAD_PARAMETER;

    // Fill staging DMA buffer, encrypting it if supported
    if (pBootstrap->bUseUnencryptedCE)
    {
        for (LwU32 i = 0; i < size32; i++)
            MEM_WR32(pBootstrap->staging.segment[pChannel->gpPut].data.p + i, p[i]);
    }
    else
    {
        // TODO: Encrypted copy to DMA buffer for SEC2 decode
    }

    return _bootstrapCopyStagingToVid(pBootstrap, gpuVA, size);
}

static RC
bootstrapZeroVid(struct bootstrap *pBootstrap, LwU64 gpuVA, LwU32 size)
{
    struct channel *pChannel = &pBootstrap->channel;
    RC rc;

    // Have to handle > 4KB due to FB heap allocation round ups
    for (LwU32 i = 0; i < size; i += pBootstrap->staging.size)
    {
        LwU32 chunkSize = LW_MIN(size - i, pBootstrap->staging.size);

        memzero(pBootstrap->staging.segment[pChannel->gpPut].data.p, LW_MIN(size, pBootstrap->staging.size));

        if (!pBootstrap->bUseUnencryptedCE)
        {
            // TODO: SEC encrypt chunk
        }

        CHECK_RC(_bootstrapCopyStagingToVid(pBootstrap, gpuVA, chunkSize));

        gpuVA += chunkSize;
    }

    return OK;
}

static RC
bootstrapRingGivenDoorbell(struct bootstrap *pBootstrap, struct channel *pChannel)
{
    LwU32 methods[16];
    LwU32 i = 0;
    RC rc;

    eschedReleaseMethods(&pBootstrap->channel, methods, &i,
                         pBootstrap->pConfig->usermode.gpuVA + LWC661_DOORBELL,
                         &pChannel->channelToken,
                         RELEASE_MEMBAR);

    CHECK_RC(bootstrapWriteMethods(pBootstrap, methods, i));

    return OK;
}

static RC
bootstrapWriteMethods(struct bootstrap *pBootstrap, LwU32 *pMethods, LwU32 count)
{
    struct channel *pChannel = &pBootstrap->channel;
    volatile LwU32 *pb = &pChannel->pb.segment[pChannel->gpPut].p[pChannel->pb.index];

    // Do not overflow this pb segment
    if ((pChannel->pb.index + count) > (pChannel->pb.size/sizeof(LwU32)))
        return RC::ILWALID_ADDRESS;

    // On a new buffer ensure the next slot of idle
    if (pChannel->pb.index == 0)
        bootstrapWait(pBootstrap, false);

    for (LwU32 i = 0; i < count; i++)
    {
        MEM_WR32(pb + i, pMethods[i]);
    }

    pChannel->pb.index += count;

    return OK;
}

static RC
bootstrapFlush(struct bootstrap *pBootstrap)
{
    struct channel *pChannel = &pBootstrap->channel;
    LwU32 methods[32];
    LwU32 i = 0;
    RC rc;

    // Any pending methods?
    if (pChannel->pb.index == 0)
        return OK;

    pChannel->sequence.put++;

    if (pBootstrap->bUseUnencryptedCE)
    {
        ceReleaseMethods(pChannel, methods, &i, pChannel->sequence.gpuVA, &pChannel->sequence.put, RELEASE_SYSMEMBAR);
    }
    else
    {
        // TODO: SEC2 implementation
    }

    CHECK_RC(bootstrapWriteMethods(pBootstrap, methods, i));
    channelFlush(pChannel);

    return OK;
}

static RC
bootstrapWait(struct bootstrap *pBootstrap, bool bEmpty)
{
    if (bEmpty)
    {
        bootstrapFlush(pBootstrap);
        return POLLWRAP(channelEmpty, &pBootstrap->channel, 50);
    }

    return POLLWRAP(channelNotFull, &pBootstrap->channel, 50);
}

static RC
copyTest(struct bootstrap *pBootstrap, struct wl *pWl)
{
#define CTDWORDS (BOOTSTRAP_ALIGNMENT/sizeof(LwU32))
    struct channel *pChannel;
    LwHandle hSys;
    LwHandle hVid;
    LwU32 data[CTDWORDS];
    LwU64 size = 4096;
    LwU64 sysVA;
    LwU64 vidVA;
    LwU32 *p;
    LwU32 i;
    RC rc;

    pChannel = (pBootstrap != NULL ) ? &pBootstrap->channel : &pWl->channel;
    Printf(Tee::PriHigh, "Running %s copy test\n", pChannel->name);

    CHECK_RC(allocSysmem(pChannel, &hSys, &size, (void **)&p, &sysVA));
    CHECK_RC(allocVidmem(pChannel, &hVid, &size, NULL, &vidVA));

    memzero(p, CTDWORDS*sizeof(LwU32));

    memset(data, 0x55, sizeof(data));
    if (pBootstrap != NULL)
    {
        CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, data, vidVA, sizeof(data)));
        CHECK_RC(bootstrapCopy(pBootstrap, vidVA, sysVA, sizeof(data)));
        CHECK_RC(bootstrapWait(pBootstrap, true));
    }
    else
    {
        CHECK_RC(wlCopyProtectedToVid(pWl, data, vidVA, sizeof(data), true));
        CHECK_RC(wlCopy(pWl, vidVA, sysVA,sizeof(data), true));
        CHECK_RC(wlWait(pWl, true));
    }
    for (i = 0; i < CTDWORDS; i++)
    {
        if (MEM_RD32(p+i) != 0x55555555)
        {
            Printf(Tee::PriHigh, "%s copy test Error: Got 0x%08x expected 0x55555555\n", pChannel->name, MEM_RD32(p+i));
            return RC::UNEXPECTED_RESULT;
        }
    }

    memset(data, 0xaa, sizeof(data));
    if (pBootstrap)
    {
        CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, data, vidVA, sizeof(data)));
        CHECK_RC(bootstrapCopy(pBootstrap, vidVA, sysVA, sizeof(data)));
        CHECK_RC(bootstrapWait(pBootstrap, true));
    }
    else
    {
        CHECK_RC(wlCopyProtectedToVid(pWl, data, vidVA, sizeof(data), true));
        CHECK_RC(wlCopy(pWl, vidVA, sysVA,sizeof(data), true));
        CHECK_RC(wlWait(pWl, true));
    }
    for (i = 0; i < CTDWORDS; i++)
    {
        if (MEM_RD32(p+i) != 0xaaaaaaaa)
        {
            Printf(Tee::PriHigh, "%s copy test Error: Got 0x%08x expected 0xaaaaaaaa\n", pChannel->name, MEM_RD32(p+i));
            return RC::UNEXPECTED_RESULT;
        }
    }

    freeAlloc(pChannel, hSys, p, sysVA);
    freeAlloc(pChannel, hVid, NULL, vidVA);

    return rc;
}

static void
bootstrapFree(struct bootstrap *pBootstrap)
{
    freeAlloc(&pBootstrap->channel, pBootstrap->staging.handle, pBootstrap->staging.p, pBootstrap->staging.gpuVA);

    channelFree(&pBootstrap->channel);

    LwRmFree(pBootstrap->pConfig->hClient, pBootstrap->pConfig->hDevice, pBootstrap->hChannelGroup);
    freeNotify(&pBootstrap->notifiers);
}

static RC
notificationProxyAlloc
(
    struct notificationProxy *pNotificationProxy,
    struct client_config *pConfig,
    struct bootstrap *pBootstrap,
    LwU64 targetVA,
    LwU32 pbEntries
)
{
    struct channel *pChannel = &pNotificationProxy->channel;
    LwU64 gp[MAX_SEGMENTS];
    LwU32 methods[64] = { 0 };
    LW_STATUS status;
    LwU64 gpuVA;
    LwU32 gp0;
    LwU32 gp1;
    RC rc;

    pNotificationProxy->pConfig = pConfig;
    pNotificationProxy->targetVA = targetVA;

    // Allocate channel group error notifier
    CHECK_RC(allocNotify(pConfig->hClient, pConfig->hDevice, pConfig->hVASpace, 1, &pNotificationProxy->notifiers));

    // Allocate channel group
    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS ChannelGroupParams = {0};
    pNotificationProxy->hChannelGroup = NextHandle++;
    ChannelGroupParams.hObjectError = pNotificationProxy->notifiers.hNotify[0];
    ChannelGroupParams.hVASpace = LW01_NULL_OBJECT;
    ChannelGroupParams.engineType = LW2080_ENGINE_TYPE_COPY(1);
    status = LwRmAlloc(pConfig->hClient,
                       pConfig->hDevice,
                       pNotificationProxy->hChannelGroup,
                       KEPLER_CHANNEL_GROUP_A,
                       &ChannelGroupParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    CHECK_RC(channelAlloc(pChannel, pConfig, "proxy",
                          pNotificationProxy->hChannelGroup, ChannelGroupParams.engineType,
                          true, false, pbEntries, 1024));

    LWA0B5_ALLOCATION_PARAMETERS ceAllocParams = { LWA0B5_ALLOCATION_PARAMETERS_VERSION_1, 0 };
    pChannel->hEngine = NextHandle++;
    ceAllocParams.version =  LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
    ceAllocParams.engineType = ChannelGroupParams.engineType;
    status = LwRmAlloc(pConfig->hClient,
                       pChannel->hChannel,
                       pChannel->hEngine,
                       pConfig->ceClass,
                       &ceAllocParams);
    CHECK_RC(RmApiStatusToModsRC(status));
    pChannel->subch = LWA06F_SUBCHANNEL_COPY_ENGINE;

    CHECK_RC(channelSchedule(pChannel));

    pNotificationProxy->proxy.segments = pbEntries;
    pNotificationProxy->proxy.size = 256 * pbEntries;
    CHECK_RC(allocVidmem(pChannel, &pNotificationProxy->proxy.handle,
                         &pNotificationProxy->proxy.size, NULL, &pNotificationProxy->proxy.baseVA));
    for (LwU32 entry = 0; entry < pbEntries; entry++)
    {
        LwU32 i = 0;

        pNotificationProxy->proxy.gpuVA[entry] = pNotificationProxy->proxy.baseVA + (256 * entry);

        if (entry == 0)
        {
            ceInitMethods(pChannel, methods, &i);
        }
        ceCopyMethods(pChannel, methods, &i, pNotificationProxy->proxy.gpuVA[entry],
                      pNotificationProxy->targetVA, sizeof(LwU32), false);
        CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, methods,
                                             pChannel->pb.segment[entry].gpuVA,
                                             sizeof(methods)));

        // GPFIFO entry
        gpuVA = pChannel->pb.segment[entry].gpuVA;
        gp0 = DRF_DEF(C86F, _GP_ENTRY0, _FETCH, _UNCONDITIONAL) |
            DRF_NUM(C86F, _GP_ENTRY0, _GET, LwU64_LO32(gpuVA) >> 2);

        gp1 = DRF_NUM(C86F, _GP_ENTRY1, _GET_HI, LwU64_HI32(gpuVA)) |
            DRF_NUM(C86F, _GP_ENTRY1, _LENGTH, i) |
            DRF_DEF(C86F, _GP_ENTRY1, _SYNC, _WAIT) |
            DRF_DEF(C86F, _GP_ENTRY1, _LEVEL, _MAIN);

        gp[entry] = (((LwU64)gp1)<<32) | gp0;
    }

    // Copy gpfifo entries
    CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, (LwU32 *)gp,
                                         pChannel->gpfifo.gpuVA,
                                         sizeof(gp)));

    CHECK_RC(bootstrapWait(pBootstrap, true));

    return OK;
}

static void
notificationProxyNext
(
    struct notificationProxy *pNotificationProxy,
    LwU64 *pNotifVA,
    LwU32 *pGpPut,
    LwU64 *pGpPutVA
)
{
    struct channel *pChannel = &pNotificationProxy->channel;

    // USERD location is constant
    *pGpPutVA = pChannel->userd.gpuVA + offsetof(Lwc86fControl, GPPut);

    // Notify target is the current buffer
    *pNotifVA = pNotificationProxy->proxy.gpuVA[pChannel->gpPut];

    // Incrememt GP_PUT
    pChannel->gpPut = (pChannel->gpPut + 1) & (pChannel->gpfifo.entries - 1);
    *pGpPut = pChannel->gpPut;
}

static void
notificationProxyFree
(
    struct notificationProxy *pNotificationProxy
)
{
    freeAlloc(&pNotificationProxy->channel, pNotificationProxy->proxy.handle, NULL, pNotificationProxy->proxy.baseVA);

    channelFree(&pNotificationProxy->channel);

    LwRmFree(pNotificationProxy->pConfig->hClient, pNotificationProxy->pConfig->hDevice, pNotificationProxy->hChannelGroup);
    freeNotify(&pNotificationProxy->notifiers);
}

/*!
 * @brief Allocate sample work channel on CE
 *
 * This channel will be fed by the WL channel.  The work channel needs to be
 * allocated first so the WL channel can reference its resources.
 *
 * The bootstrap channel is used to initialize the fed channel.  The wl
 * channels are used to run the channel.
 */
static RC
fedAlloc
(
    struct fed *pFed,
    struct client_config *pConfig,
    struct bootstrap *pBootstrap,
    struct wl *pWl,
    LwU32 fedClass,
    LwU32 pbEntries,
    LwU32 pbSize
)
{
    struct channel *pChannel = &pFed->channel;
    LwU32 engineType;
    LW_STATUS status;
    RC rc = OK;

    pFed->pConfig = pConfig;

    // Allow running with WL or with just bootstrap copy for comparison
    if (pWl == NULL)
    {
        pFed->pBootstrap = pBootstrap;
        pFed->pWl = NULL;
    }
    else
    {
        pFed->pBootstrap = NULL;
        pFed->pWl = pWl;
    }

    pFed->fedClass = fedClass;
    pFed->bUseCE = (fedClass & 0xff) == 0xb5;
    pFed->bUseEsched = (fedClass & 0xff) == 0x6f;

    if (pFed->bUseCE)
    {
        pFed->pInitMethods = ceInitMethods;
        pFed->pReleaseMethods = ceReleaseMethods;
        pFed->nopMethod = LWC8B5_NOP;
        engineType = LW2080_ENGINE_TYPE_COPY(1);
    }
    else if (pFed->bUseEsched)
    {
        // Use just esched for methods, but on gr pbdma
        pFed->pInitMethods = computeInitMethods;
        pFed->pReleaseMethods = eschedReleaseMethods;
        pFed->nopMethod = LWC86F_DMA_NOP;
        engineType = LW2080_ENGINE_TYPE_GR(0);
    }
    else
    {
        pFed->pInitMethods = computeInitMethods;
        pFed->pReleaseMethods = computeReleaseMethods;
        pFed->nopMethod = LWCBC0_PIPE_NOP;
        engineType = LW2080_ENGINE_TYPE_GR(0);
    }

    // Allocate channel group error notifier
    CHECK_RC(allocNotify(pConfig->hClient, pConfig->hDevice, pConfig->hVASpace, 1, &pFed->notifiers));

    // Allocate channel group
    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS ChannelGroupParams = {0};
    pFed->hChannelGroup = NextHandle++;
    ChannelGroupParams.hObjectError = pFed->notifiers.hNotify[0];
    ChannelGroupParams.hVASpace = LW01_NULL_OBJECT;
    ChannelGroupParams.engineType = engineType;
    status = LwRmAlloc(pConfig->hClient,
                       pConfig->hDevice,
                       pFed->hChannelGroup,
                       KEPLER_CHANNEL_GROUP_A,
                       &ChannelGroupParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    CHECK_RC(channelAlloc(pChannel, pConfig, "fed",
                          pFed->hChannelGroup, ChannelGroupParams.engineType,
                          true, false, pbEntries, pbSize));

    // Initialize memory that core channel code could not
    CHECK_RC(bootstrapZeroVid(pBootstrap, pChannel->gpfifo.gpuVA, pChannel->gpfifo.size));
    CHECK_RC(bootstrapZeroVid(pBootstrap, pChannel->userd.gpuVA, pChannel->userd.size));
    CHECK_RC(bootstrapWait(pBootstrap, true));

    // Allocate work engine object
    pChannel->hEngine = NextHandle++;
    if (pFed->bUseCE)
    {
        LWA0B5_ALLOCATION_PARAMETERS ceAllocParams = { LWA0B5_ALLOCATION_PARAMETERS_VERSION_1, 0 };
        ceAllocParams.version =  LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
        ceAllocParams.engineType = ChannelGroupParams.engineType;
        status = LwRmAlloc(pConfig->hClient,
                           pChannel->hChannel,
                           pChannel->hEngine,
                           pConfig->ceClass,
                           &ceAllocParams);
        CHECK_RC(RmApiStatusToModsRC(status));
        pChannel->subch = LWA06F_SUBCHANNEL_COPY_ENGINE;
    }
    else // Use gr engine for compute and plain esched
    {
        status = LwRmAlloc(pConfig->hClient,
                           pChannel->hChannel,
                           pChannel->hEngine,
                           pConfig->computeClass,
                           NULL);
        CHECK_RC(RmApiStatusToModsRC(status));
        pChannel->subch = LWA06F_SUBCHANNEL_COMPUTE;
    }

    CHECK_RC(channelSchedule(pChannel));

    memset(pFed->staging.gpfifo, 0, sizeof(pFed->staging.gpfifo));

    pFed->staging.method.p = (LwU32 *)malloc(pbSize);
    if (pFed->staging.method.p == NULL)
            return RC::CANNOT_ALLOCATE_MEMORY;
    pFed->staging.method.size = pbSize;
    pFed->staging.method.index = 0;

    // Set our engine object
    LwU32 methods[32];
    LwU32 i = 0;
    pFed->pInitMethods(pChannel, methods, &i);
    CHECK_RC(fedWriteMethods(pFed, methods, i));

    CHECK_RC(notificationProxyAlloc(&pFed->notificationProxy, pConfig, pBootstrap,
                                    pChannel->sequence.gpuVA, pbEntries));

    return rc;
}

static RC
fedWriteMethods(struct fed *pFed, LwU32 *pMethods, LwU32 count)
{
    // Do not overflow the method staging buffer
    if ((pFed->staging.method.index + count) > (pFed->staging.method.size/sizeof(LwU32)))
        return RC::BAD_PARAMETER;

    for (LwU32 i = 0; i < count; i++)
    {
        pFed->staging.method.p[pFed->staging.method.index++] = pMethods[i];
    }

    return OK;
}

static RC
fedFlush(struct fed *pFed)
{
    struct channel *pChannel = &pFed->channel;
    LwU32 methods[32];
    LwU64 proxyVA;
    LwU32 gpPut;
    LwU64 gpPutVA;
    LwU32 gp[2];
    LwU32 i = 0;
    RC rc;

    // Any pending methods?
    if (pFed->staging.method.index == 0)
        return OK;

    pChannel->sequence.put++;

    // Get next proxy notification parameters
    notificationProxyNext(&pFed->notificationProxy, &proxyVA, &gpPut, &gpPutVA);

    //
    // Send push buffer notification value to staging buffer in vidmem
    // Update proxy GPPUT
    // Ring proxy doorbell
    //
    pFed->pReleaseMethods(pChannel, methods, &i,
                          proxyVA,
                          &pChannel->sequence.put,
                          0);
    pFed->pReleaseMethods(pChannel, methods, &i,
                          gpPutVA,
                          &gpPut,
                          RELEASE_SYSMEMBAR);
    pFed->pReleaseMethods(pChannel, methods, &i,
                          pFed->pConfig->usermode.gpuVA + LWC661_DOORBELL,
                          &pFed->notificationProxy.channel.channelToken,
                          RELEASE_SYSMEMBAR);

    CHECK_RC(fedWriteMethods(pFed, methods, i));

    // Ensure the next pb entry is free
    CHECK_RC(fedWait(pFed, false));

    // Copy complete method buffer in one shot to vidmem
    if (pFed->pBootstrap)
    {
        LwU32 size = pFed->staging.method.index * sizeof(LwU32);

        size = AlignUp<BOOTSTRAP_ALIGNMENT>(size);

        CHECK_RC(bootstrapCopyProtectedToVid(pFed->pBootstrap, pFed->staging.method.p,
                                             pChannel->pb.segment[pChannel->gpPut].gpuVA,
                                             size));
    }
    else
    {
        CHECK_RC(wlCopyProtectedToVid(pFed->pWl,
                                      pFed->staging.method.p,
                                      pChannel->pb.segment[pChannel->gpPut].gpuVA,
                                      pFed->staging.method.index * sizeof(LwU32), false));
    }

    // Construct GPFIFO entry
    LwU64 gpuva = pChannel->pb.segment[pChannel->gpPut].gpuVA;
    gp[0] = DRF_DEF(C86F, _GP_ENTRY0, _FETCH, _UNCONDITIONAL) |
        DRF_NUM(C86F, _GP_ENTRY0, _GET, LwU64_LO32(gpuva) >> 2);

    gp[1] = DRF_NUM(C86F, _GP_ENTRY1, _GET_HI, LwU64_HI32(gpuva)) |
        DRF_NUM(C86F, _GP_ENTRY1, _LENGTH, pFed->staging.method.index) |
        DRF_DEF(C86F, _GP_ENTRY1, _SYNC, _PROCEED) |
        DRF_DEF(C86F, _GP_ENTRY1, _LEVEL, _MAIN);

    pFed->staging.gpfifo[pChannel->gpPut] = ((((LwU64)gp[1]) << 32) | gp[0]);

    // Write GPFIFO entry
    if (pFed->pBootstrap)
    {
        CHECK_RC(bootstrapCopyProtectedToVid(pFed->pBootstrap,
                                             (LwU32 *)pFed->staging.gpfifo,
                                             pChannel->gpfifo.gpuVA,
                                             sizeof(pFed->staging.gpfifo)));
    }
    else
    {
        CHECK_RC(wlCopyProtectedToVid(pFed->pWl, gp,
                                      pChannel->gpfifo.gpuVA + (pChannel->gpPut * sizeof(gp)),
                                      sizeof(LwU64), true));
    }

    // Update to next GPFIFO entry in channel.  Reset fed staging buffer index.
    pChannel->gpPut = (pChannel->gpPut + 1) & (pChannel->gpfifo.entries - 1);
    pFed->staging.method.index = 0;

    // Write "next" GPPUT value
    if (pFed->pBootstrap)
    {
        LwU32 ctrl[64] = { 0 };

        ctrl[offsetof(Lwc86fControl, GPPut)/sizeof(LwU32)] = pChannel->gpPut;

        CHECK_RC(bootstrapCopyProtectedToVid(pFed->pBootstrap, ctrl, pChannel->userd.gpuVA, sizeof(ctrl)));
    }
    else
    {
        CHECK_RC(wlCopyProtectedToVid(pFed->pWl, &pChannel->gpPut,
                                      pChannel->userd.gpuVA + offsetof(Lwc86fControl, GPPut),
                                      sizeof(LwU32), true));
    }
    pChannel->pb.index = 0;

    // Ring doorbell
    if (pFed->pBootstrap)
    {
        // From the bootstrap channel esched
        CHECK_RC(bootstrapRingGivenDoorbell(pFed->pBootstrap, pChannel));
        CHECK_RC(bootstrapFlush(pFed->pBootstrap));
    }
    else
    {
        // From the WL channel esched
        CHECK_RC(wlRingGivenDoorbell(pFed->pWl, pChannel));
        CHECK_RC(wlFlush(pFed->pWl));
    }

    return OK;
}

static RC
fedWait(struct fed *pFed, bool bEmpty)
{
    if (bEmpty)
    {
        fedFlush(pFed);
        return POLLWRAP(channelEmpty, &pFed->channel, 1000);
    }

    return POLLWRAP(channelNotFull, &pFed->channel, 1000);
}

static RC
fedTest(struct fed *pFed)
{
    struct channel *pChannel = &pFed->channel;
    LwU32 methods[64];
    LwHandle hVid;
    LwU64 size;
    LwU64 vidGpuVA;
    LwHandle hSys;
    LwU64 sysGpuVA;
    LwU32 *pSys;
    LwU32 count = (2 * pChannel->gpfifo.entries) + 2;   // wrap GPFIFO a few times
    LwU32 i;
    RC rc;

    Printf(Tee::PriHigh, "Running fed channel %s test\n", (pFed->pBootstrap != NULL) ? "bootstrap" : "WL");

    size = 4096;
    CHECK_RC(allocSysmem(pChannel, &hSys, &size, (void **)&pSys, &sysGpuVA));
    CHECK_RC(allocVidmem(pChannel, &hVid, &size, NULL, &vidGpuVA));

    // Send a increasing array with a varied method structure
    for (LwU32 cnt = 0; cnt < count; cnt++)
    {
        LwU64 semaVA = vidGpuVA + (cnt * sizeof(LwU32));

        i = 0;
        // Variable padding for some variance
        for (LwU32 j = 0; j < cnt; j++)
        {
            methods[i++] = LW_METHOD_N(pChannel->subch, pFed->nopMethod, 0);
        }
        pFed->pReleaseMethods(pChannel, methods, &i, semaVA, &cnt, 0);
        CHECK_RC(fedWriteMethods(pFed, methods, i));

        // Force out this pb segment. We want to use multiple segments.
        CHECK_RC(fedFlush(pFed));
    }

    CHECK_RC(fedWait(pFed, true));

    // Copy buffer back to sysmem
    if (pFed->pBootstrap)
    {
        LwU32 size = count * sizeof(LwU32);

        size = AlignUp<BOOTSTRAP_ALIGNMENT>(size);

        CHECK_RC(bootstrapCopy(pFed->pBootstrap, vidGpuVA, sysGpuVA, size));
        CHECK_RC(bootstrapFlush(pFed->pBootstrap));
        CHECK_RC(bootstrapWait(pFed->pBootstrap, true));
    }
    else
    {
        CHECK_RC(wlCopy(pFed->pWl, vidGpuVA, sysGpuVA, count * sizeof(LwU32), true));
        CHECK_RC(wlFlush(pFed->pWl));
        CHECK_RC(wlWait(pFed->pWl, true));
    }

    // TODO: decrypt copy payload.

    for (LwU32 cnt = 0; cnt < count; cnt++)
    {
        LwU32 data = MEM_RD32(&pSys[cnt]);

        if (cnt != data)
        {
            Printf(Tee::PriHigh, "Error: Work Test Got 0x%08x expected 0x%08x\n", data, cnt);
            rc = RC::UNEXPECTED_RESULT;
        }
    }
    freeAlloc(pChannel, hSys, pSys, sysGpuVA);
    freeAlloc(pChannel, hVid, NULL, vidGpuVA);

    return rc;
}

static void
fedFree(struct fed *pFed)
{
    channelFree(&pFed->channel);
    notificationProxyFree(&pFed->notificationProxy);

    free(pFed->staging.method.p);
    pFed->staging.method.p = NULL;

    LwRmFree(pFed->pConfig->hClient, pFed->pConfig->hDevice, pFed->hChannelGroup);
    freeNotify(&pFed->notifiers);
}

static RC
wlAlloc
(
    struct wl *pWl,
    struct client_config *pConfig,
    struct bootstrap *pBootstrap,
    LwU32 pbEntries,
    LwU32 pbSize
)
{
    struct channel *pChannel = &pWl->channel;
    struct channel *pLciChannel = &pWl->lci.channel;
    LwU64 gp[MAX_SEGMENTS];
    LwU32 methods[64];
    LW_STATUS status;
    LwU64 gpuVA;
    LwU32 gp0;
    LwU32 gp1;
    RC rc = OK;

    pWl->pConfig = pConfig;

    // Allocate channel group error notifier
    CHECK_RC(allocNotify(pConfig->hClient, pConfig->hDevice, pConfig->hVASpace, 2, &pWl->notifiers));

    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS ChannelGroupParams = {0};
    pWl->hChannelGroup = NextHandle++;
    ChannelGroupParams.hObjectError = pWl->notifiers.hNotify[0];
    ChannelGroupParams.hVASpace = LW01_NULL_OBJECT;
    ChannelGroupParams.engineType = LW2080_ENGINE_TYPE_COPY(1);
    status = LwRmAlloc(pConfig->hClient,
                       pConfig->hDevice,
                       pWl->hChannelGroup,
                       KEPLER_CHANNEL_GROUP_A,
                       &ChannelGroupParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    // Allocate WL Channel
    {
        // Allocate work channel with all memory in video memory with no CPU access
        // TODO allocate encrypted channel
        CHECK_RC(channelAlloc(pChannel, pConfig, "WL",
                              pWl->hChannelGroup, ChannelGroupParams.engineType,
                              true, false, pbEntries * 2, pbSize));

        // Initialize memory that core channel code could not for an CPU invisible channel
        CHECK_RC(bootstrapZeroVid(pBootstrap, pChannel->gpfifo.gpuVA, pChannel->gpfifo.size));
        CHECK_RC(bootstrapZeroVid(pBootstrap, pChannel->userd.gpuVA, pChannel->userd.size));
        CHECK_RC(bootstrapWait(pBootstrap, true));

        // Allocate CE engine object
        pChannel->hEngine = NextHandle++;
        LWA0B5_ALLOCATION_PARAMETERS ceAllocParams = { LWA0B5_ALLOCATION_PARAMETERS_VERSION_1, 0 };
        ceAllocParams.version =  LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
        ceAllocParams.engineType = ChannelGroupParams.engineType;
        status = LwRmAlloc(pConfig->hClient,
                           pChannel->hChannel,
                           pChannel->hEngine,
                           pConfig->ceClass,
                           &ceAllocParams);
        CHECK_RC(RmApiStatusToModsRC(status));
        pChannel->subch = LWA06F_SUBCHANNEL_COPY_ENGINE;
    }

    // Allocate LCI Channel
    {
        // Allocate work channel with all memory in video memory with no CPU access
        CHECK_RC(channelAlloc(pLciChannel, pConfig, "LCI",
                              pWl->hChannelGroup, ChannelGroupParams.engineType,
                              true, false, pbEntries, 4096));

        // Initialize memory that core channel code could not
        CHECK_RC(bootstrapZeroVid(pBootstrap, pLciChannel->gpfifo.gpuVA, pLciChannel->gpfifo.size));
        CHECK_RC(bootstrapZeroVid(pBootstrap, pLciChannel->userd.gpuVA, pLciChannel->userd.size));
        CHECK_RC(bootstrapWait(pBootstrap, true));

        // Allocate work CE engine object
        pLciChannel->hEngine = NextHandle++;
        LWA0B5_ALLOCATION_PARAMETERS ceAllocParams = { LWA0B5_ALLOCATION_PARAMETERS_VERSION_1, 0 };
        ceAllocParams.version =  LWA0B5_ALLOCATION_PARAMETERS_VERSION_1;
        ceAllocParams.engineType = ChannelGroupParams.engineType;
        status = LwRmAlloc(pConfig->hClient,
                           pLciChannel->hChannel,
                           pLciChannel->hEngine,
                           pConfig->ceClass,
                           &ceAllocParams);
        CHECK_RC(RmApiStatusToModsRC(status));
        pLciChannel->subch = LWA06F_SUBCHANNEL_COPY_ENGINE;
    }

    //
    // Allocate staging buffers. pSelwre is filled, then encrypted into
    // a dma buffer to be pulled by the Wl channel.
    //
    {
        pWl->staging.size = pbSize;
        pWl->staging.entries = pbEntries;
        pWl->staging.index = 0;
        pWl->staging.dataindex = 0;
        pWl->staging.get = 0;
        pWl->staging.put = 0;

        pWl->staging.pSelwre = (LwU32 *)malloc(pWl->staging.size * pWl->staging.entries);
        if (pWl->staging.pSelwre == NULL)
            return RC::CANNOT_ALLOCATE_MEMORY;

        // Allocate both method and data buffers for GPU view
        LwU64 totalSize = pWl->staging.size * pWl->staging.entries * 2;
        CHECK_RC(allocSysmem(pChannel, &pWl->staging.handle, &totalSize, &pWl->staging.p, &pWl->staging.gpuVA));
        for (LwU32 i = 0; i < pWl->staging.entries; i++)
        {
            pWl->staging.segment[i].method.pSelwre = (LwU32 *)(((LwU8 *)pWl->staging.pSelwre) + (i * pWl->staging.size));
            pWl->staging.segment[i].method.p = (LwU32 *)(((LwU8 *)pWl->staging.p) + (i * pWl->staging.size));
            pWl->staging.segment[i].method.gpuVA = pWl->staging.gpuVA + (i * pWl->staging.size);
            wlMethodStagingCanary(pWl, i);

            pWl->staging.segment[i].data.p = (LwU32 *)(((LwU8 *)pWl->staging.p) + ((i + pWl->staging.entries) * pWl->staging.size));
            pWl->staging.segment[i].data.gpuVA = pWl->staging.gpuVA + ((i + pWl->staging.entries) * pWl->staging.size);
        }
    }

    // Schedulule channels, needed to get channel tokens
    CHECK_RC(channelSchedule(pChannel));
    CHECK_RC(channelSchedule(pLciChannel));

    // Write WL push buffers
    memset(gp, 0, sizeof(gp));
    for (LwU32 entry = 0; entry < pbEntries; entry++)
    {
        LwU32 wlentry = entry * 2;        // even push buffer sements
        LwU32 i = 0;

        memset(methods, 0, sizeof(methods));

        // Set WL engine object
        if (entry == 0)
        {
            // Set up initial state before any engine methods.  Ok to repeat.
            ceInitMethods(pChannel, methods, &i);
        }

        // 4a. Copy method staging buffer to odd push buffer entry with membar.gl
        ceCopyMethods(pChannel, methods, &i,
                      pWl->staging.segment[entry].method.gpuVA,
                      pChannel->pb.segment[wlentry + 1].gpuVA,
                      pWl->staging.size,
                      false);

        // 4b Non-WFI esched semaphore release that updates GP_PUT of LCIC
        LwU32 val = (entry + 1) & (pLciChannel->gpfifo.entries - 1);
        eschedReleaseMethods(pChannel, methods, &i,
                             pLciChannel->userd.gpuVA + offsetof(Lwc86fControl, GPPut),
                             &val,
                             0);

        // 4c Non-WFI esched semaphore release that rings the doorbell of LCIC
        eschedReleaseMethods(pChannel, methods, &i,
                             pConfig->usermode.gpuVA + LWC661_DOORBELL,
                             &pLciChannel->channelToken,
                             0);

        // 4d LW_UDMA_WFI to make esched wait on CE work to finish
        methods[i++] = LW_METHOD(pChannel->subch, LWC86F_WFI);
        methods[i++] = DRF_DEF(C86F, _WFI, _SCOPE, _ALL);

        CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, methods,
                                             pChannel->pb.segment[wlentry].gpuVA,
                                             sizeof(methods)));

        // Even entry exelwtes above methods and continues
        gpuVA = pChannel->pb.segment[wlentry].gpuVA;
        gp0 = DRF_DEF(C86F, _GP_ENTRY0, _FETCH, _UNCONDITIONAL) |
            DRF_NUM(C86F, _GP_ENTRY0, _GET, LwU64_LO32(gpuVA) >> 2);

        gp1 = DRF_NUM(C86F, _GP_ENTRY1, _GET_HI, LwU64_HI32(gpuVA)) |
            DRF_NUM(C86F, _GP_ENTRY1, _LENGTH, i) |
            DRF_DEF(C86F, _GP_ENTRY1, _SYNC, _PROCEED) |
            DRF_DEF(C86F, _GP_ENTRY1, _LEVEL, _MAIN);
        gp[wlentry] = (((LwU64)gp1)<<32) | gp0;

        // Odd entry exelwtes copied commands and yields (SYNC_WAIT)
        gpuVA = pChannel->pb.segment[wlentry + 1].gpuVA;
        gp0 = DRF_DEF(C86F, _GP_ENTRY0, _FETCH, _UNCONDITIONAL) |
            DRF_NUM(C86F, _GP_ENTRY0, _GET, LwU64_LO32(gpuVA) >> 2);

        gp1 = DRF_NUM(C86F, _GP_ENTRY1, _GET_HI, LwU64_HI32(gpuVA)) |
            DRF_NUM(C86F, _GP_ENTRY1, _LENGTH, pbSize/sizeof(LwU32)) |
            DRF_DEF(C86F, _GP_ENTRY1, _SYNC, _WAIT) |
            DRF_DEF(C86F, _GP_ENTRY1, _LEVEL, _MAIN);
        gp[wlentry + 1] = (((LwU64)gp1)<<32) | gp0;
    }

    // Copy WL gpfifo entries into place
    CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, (LwU32 *)gp,
                                         pChannel->gpfifo.gpuVA,
                                         sizeof(gp)));

    // Write LCIC push buffers
    memset(gp, 0, sizeof(gp));
    for (LwU32 entry = 0; entry < pbEntries; entry++)
    {
        LwU32 i = 0;

        memset(methods, 0, sizeof(methods));

        if (entry == 0)
        {
            // Set up initial state before any engine methods.  Ok to repeat.
            ceInitMethods(pLciChannel, methods, &i);
        }

        // 3a Update WLC GP_PUT - (start_gp_put + 2 + (entry *2)) & (gpfifo_entries - 1)
        LwU32 val = (2 + 2 + (entry * 2)) & (pChannel->gpfifo.entries - 1);
        ceReleaseMethods(pLciChannel, methods, &i,
                         pChannel->userd.gpuVA + offsetof(Lwc86fControl, GPPut),
                         &val, RELEASE_MEMBAR);

        //
        // 3b Copy operation to write encrypted completion notification
        // TODO: Not to location X as in spec now. When payload is
        // encrypted we will have to do this to pipelined locations.
        //
        LwU32 notif = (entry + 1) & (pLciChannel->gpfifo.entries - 1);
        ceReleaseMethods(pLciChannel, methods, &i, pLciChannel->sequence.gpuVA, &notif, RELEASE_SYSMEMBAR);

        CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, methods,
                                             pLciChannel->pb.segment[entry].gpuVA,
                                             sizeof(methods)));

        // GPFIFO entry
        gpuVA = pLciChannel->pb.segment[entry].gpuVA;
        gp0 = DRF_DEF(C86F, _GP_ENTRY0, _FETCH, _UNCONDITIONAL) |
            DRF_NUM(C86F, _GP_ENTRY0, _GET, LwU64_LO32(gpuVA) >> 2);

        gp1 = DRF_NUM(C86F, _GP_ENTRY1, _GET_HI, LwU64_HI32(gpuVA)) |
            DRF_NUM(C86F, _GP_ENTRY1, _LENGTH, i) |
            DRF_DEF(C86F, _GP_ENTRY1, _SYNC, _WAIT) |
            DRF_DEF(C86F, _GP_ENTRY1, _LEVEL, _MAIN);

        gp[entry] = (((LwU64)gp1)<<32) | gp0;
    }

    // Copy LCI gpfifo entries into place
    CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, (LwU32 *)gp,
                                         pLciChannel->gpfifo.gpuVA,
                                         sizeof(gp)));

    //
    // Run first PB segment to get WL in "armed" state.  Saw issues
    // with channel starting w/o a doorbell ring when GPU_PUT starts
    // at 2 with the initial channel state.
    //
    // TODO encrypt block of methods
    MEM_WR32(pWl->staging.segment[pWl->staging.put].method.p, DRF_DEF(C86F, _DMA, _SEC_OP, _END_PB_SEGMENT));
    pWl->staging.put = (pWl->staging.put + 1) & (pWl->staging.entries - 1);
    Platform::FlushCpuWriteCombineBuffer();

    // Set WL channel GP_PUT to 2
    LwU32 ctrl[64] = { 0 };
    pChannel->gpPut = 2;
    ctrl[offsetof(Lwc86fControl, GPPut)/sizeof(LwU32)] = pChannel->gpPut;
    CHECK_RC(bootstrapCopyProtectedToVid(pBootstrap, ctrl, pChannel->userd.gpuVA, sizeof(ctrl)));

    CHECK_RC(bootstrapWait(pBootstrap, true));

    channelRingDoorbell(pChannel);

    CHECK_RC(wlWait(pWl, true));

    return rc;
}

//
// For unencrypted copy, insert a page fault crash if the channel runs early
// TODO: remove when encrypted.  Expect a decrypt error in that case
//
static void
wlMethodStagingCanary(struct wl *pWl, LwU32 index)
{
    LwU32 methods[32];
    LwU32 count = 0;

    ceReleaseMethods(&pWl->channel, methods, &count, 0x1000 + index, methods, RELEASE_SYSMEMBAR);

    for (LwU32 i = 0; i < count; i++)
        MEM_WR32(pWl->staging.segment[index].method.p + i, methods[i]);
}

//
// Stage pSrc client data and generate WL methods to copy it. Used to copy
// fed channels methods into place.
//
static RC
wlCopyProtectedToVid(struct wl *pWl, LwU32 *pSrc, LwU64 dstVA, LwU32 size, bool bMembar)
{
    LwU32 size32 = size / sizeof(LwU32);
    RC rc;

    //
    // Handle 4/8 byte copy inline with a semaphore release.  Methods
    // and data are encrypted/authenticated with the push buffer.
    //
    if ((size == 4) || size == 8)
    {
        LwU32 methods[32];
        LwU32 i = 0;

        LwU32 flags = bMembar ? RELEASE_MEMBAR : 0;
        if (size == 8)
            flags |= RELEASE_64BIT;
        ceReleaseMethods(&pWl->channel, methods, &i, dstVA, pSrc, flags);

        return wlWriteMethods(pWl, methods, i);
    }

    if (size > pWl->staging.size)
        return RC::BAD_PARAMETER;

    // If we had a previous copy flush
    // TODO: could allow multiple copies to split staging buffer
    if (pWl->staging.dataindex != 0)
        CHECK_RC(wlFlush(pWl));

    // Copy data into staging buffer
    // TODO: encrypt into staging buffer
    for (LwU32 i = 0; i < size32; i++)
        MEM_WR32(pWl->staging.segment[pWl->staging.put].data.p + i, pSrc[i]);

    pWl->staging.dataindex += size;

    CHECK_RC(wlCopy(pWl, pWl->staging.segment[pWl->staging.put].data.gpuVA, dstVA, size, bMembar));

    return OK;
}

//
// Schedule a CE copy on the WL channel.  May go either to video or system. Does
// not handle encryption/decryption.
//
static RC
wlCopy(struct wl *pWl, LwU64 srcVA, LwU64 dstVA, LwU32 size, bool bMembar)
{
    LwU32 methods[32];
    LwU32 i = 0;
    RC rc;

    ceCopyMethods(&pWl->channel, methods, &i,
                  srcVA,
                  dstVA,
                  size,
                  bMembar);

    CHECK_RC(wlWriteMethods(pWl, methods, i));

    return OK;
}

static RC
wlRingGivenDoorbell(struct wl *pWl, struct channel *pChannel)
{
    LwU32 methods[16];
    LwU32 i = 0;
    RC rc;

    eschedReleaseMethods(&pWl->channel, methods, &i,
                         pWl->pConfig->usermode.gpuVA + LWC661_DOORBELL,
                         &pChannel->channelToken,
                         RELEASE_MEMBAR);

    CHECK_RC(wlWriteMethods(pWl, methods, i));

    return OK;
}

//
// This writes WLC CE methods into a clear text staging buffer
//
static RC
wlWriteMethods(struct wl *pWl, LwU32 *pMethods, LwU32 count)
{
    LwU32 *pSelwre;
    LwU32  segment;
    RC     rc;

    segment = pWl->staging.put;
    pSelwre = &pWl->staging.segment[segment].method.pSelwre[pWl->staging.index];

    // Do not overflow this pb segment
    if ((pWl->staging.index + count) > (pWl->staging.size/sizeof(LwU32)))
        return RC::BAD_PARAMETER;

    // On a new buffer ensure the next slot is idle
    if (pWl->staging.index == 0)
        CHECK_RC(wlWait(pWl, false));

    // Copy into CPU side non-DMA staging buffer
    for (LwU32 i = 0; i < count; i++)
    {
        pSelwre[i] = pMethods[i];
    }

    pWl->staging.index += count;

    return OK;
}

//
// This copies WL CE methods from a staging buffer into
// the DMA buffer.  The WLC channel will copy this
// buffer and execute it.
//
static RC
wlFlush(struct wl *pWl)
{
    struct channel *pChannel = &pWl->channel;
    LwU32 methods[32];
    LwU32 segment;
    LwU32 i = 0;
    RC    rc;

    // Any pending methods?
    if (pWl->staging.index == 0)
        return OK;

    // No flush semaphore is used for WL channel. Update comes from LCIC.

    // Insert end host of PB segment method. GpFifo is programmed to full buffer.
    methods[i++] = DRF_DEF(C86F, _DMA, _SEC_OP, _END_PB_SEGMENT);
    CHECK_RC(wlWriteMethods(pWl, methods, i));

    segment = pWl->staging.put;

    //
    // Move data from the staging to the DMA buffer
    // Leaves stale data between staging.index and staging.size
    //
    // TODO: this step should encrypt
    for (LwU32 ri = 0; ri < pWl->staging.index; ri++)
    {
        MEM_WR32(pWl->staging.segment[segment].method.p + ri, pWl->staging.segment[segment].method.pSelwre[ri]);
    }

    // Test is not written to use WC, but this is the supported fence
    Platform::FlushCpuWriteCombineBuffer();

    // GPFIFO entry is fixed and writty by wlAlloc()

    // GP_PUT is written automatically by LCIC

    // Update our staging buffer location
    pWl->staging.dataindex = 0;
    pWl->staging.index = 0;

    //
    // Next WL buffer is ready, Wait until the WL queue is empty so we know
    // GP_PUT has been updated before we ring the doorbell from he CPU.
    //
    wlWait(pWl, true);

    // Note that next buffer is queued
    pWl->staging.put = (pWl->staging.put + 1) & (pWl->staging.entries - 1);

    // Ring WLC doorbell from the CPU
    channelRingDoorbell(pChannel);

    return OK;
}

static bool wlEmpty(void *vc)
{
    struct wl *pWl = (struct wl *)vc;

    pWl->staging.get = MEM_RD32(pWl->lci.channel.sequence.p);

    return pWl->staging.get == pWl->staging.put;
}

static bool wlNotFull(void *vc)
{
    struct wl *pWl = (struct wl *)vc;
    LwU32 mask = pWl->staging.entries - 1;
    LwU32 used;

    pWl->staging.get = MEM_RD32(pWl->lci.channel.sequence.p);

    used = (pWl->staging.put - pWl->staging.get) & mask;

    return used != mask;
}

static RC
wlWait(struct wl *pWl, bool bEmpty)
{
    LwU32 get = pWl->staging.get;
    RC rc;

    if (bEmpty)
    {
        wlFlush(pWl);
        rc = POLLWRAP(wlEmpty, pWl, 50);
    }
    else
    {
        rc = POLLWRAP(wlNotFull, pWl, 50);
    }

    // Rewrite canary methods
    for (LwU32 i = get; i != pWl->staging.get; i = (i + 1) & (pWl->staging.entries - 1))
    {
        wlMethodStagingCanary(pWl, i);
    }

    return rc;
}

static void wlFree(struct wl *pWl)
{
    if (pWl->hChannelGroup == LW01_NULL_OBJECT)
        return;

    channelFree(&pWl->lci.channel);

    //
    // WL channel looks to be pending as GP_PUT is automatically += 2 by the
    // LCI channel.  Disable so RM does not try to flush work on free.
    //
    channelDisable(&pWl->channel);

    freeAlloc(&pWl->channel, pWl->staging.handle, pWl->staging.p, pWl->staging.gpuVA);
    free(pWl->staging.pSelwre);
    channelFree(&pWl->channel);
    LwRmFree(pWl->pConfig->hClient, pWl->pConfig->hDevice, pWl->hChannelGroup);
    pWl->hChannelGroup = LW01_NULL_OBJECT;

    freeNotify(&pWl->notifiers);
}

static RC
allocNotify(LwHandle hClient, LwHandle hDevice, LwHandle hVASpace, LwU32 n, struct notify *pNotify)
{
#define NOTIFICATION_SIZE (LW_CHANNELGPFIFO_NOTIFICATION_TYPE__SIZE_1 * sizeof(LwNotification))
    LW_STATUS status;
    RC rc = OK;

    pNotify->hClient = hClient;
    pNotify->hDevice = hDevice;
    pNotify->hVASpace = hVASpace;

    if (n >= MAX_NOTIFIERS)
        return RC::BAD_PARAMETER;

    pNotify->n = n;
    pNotify->scratchOffset = n * NOTIFICATION_SIZE;

    //
    // Allocate 4KB buffer for notifiers and scratch space.
    // The first LW_CHANNELGPFIFO_NOTIFICATION_TYPE__SIZE_1 * sizeof(LwNotification) bytes are
    // used for notification.
    //
    pNotify->limit = 4096 - 1;
    pNotify->hMemory = NextHandle++;
    status = LwRmAllocMemory64(hClient,
                               hDevice,
                               pNotify->hMemory,
                               LW01_MEMORY_SYSTEM,
                               DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
                               DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED),
                               &pNotify->p,
                               &pNotify->limit);
    CHECK_RC(RmApiStatusToModsRC(status));
    memzero(pNotify->p, pNotify->limit + 1);

    for (LwU32 i = 0; i < pNotify->n; i++)
    {
        LwU64 offset = i * NOTIFICATION_SIZE;

        pNotify->pNotify[0] = (LwNotification *)(((LwU8 *)pNotify->p) + offset);

        pNotify->hNotify[i] = NextHandle++;
        status = LwRmAllocContextDma2(hClient,
                                      pNotify->hNotify[i],
                                      LW01_CONTEXT_DMA,
                                      DRF_DEF(OS03, _FLAGS, _HASH_TABLE, _DISABLE) |
                                      DRF_DEF(OS03, _FLAGS, _MAPPING, _KERNEL),
                                      pNotify->hMemory,
                                      offset,
                                      NOTIFICATION_SIZE - 1);
        CHECK_RC(RmApiStatusToModsRC(status));
    }

    status = LwRmMapMemoryDma(hClient,
                              hDevice,
                              hVASpace,
                              pNotify->hMemory,
                              0,
                              pNotify->limit,
                              DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                              DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                              &pNotify->gpuVA);
    CHECK_RC(RmApiStatusToModsRC(status));

    return rc;
}

static void
freeNotify(struct notify *pNotify)
{
    LwRmUnmapMemoryDma(pNotify->hClient,
                       pNotify->hDevice,
                       pNotify->hVASpace,
                       pNotify->hMemory,
                       0,
                       pNotify->gpuVA);

    for (LwU32 i = 0; i < pNotify->n; i++)
    {
        LwRmFree(pNotify->hClient, pNotify->hDevice, pNotify->hNotify[i]);
        pNotify->hNotify[i] = LW01_NULL_OBJECT;
    }

    LwRmFree(pNotify->hClient, pNotify->hDevice, pNotify->hMemory);
    pNotify->hMemory = LW01_NULL_OBJECT;
}

static void memzero(void *vp, LwU32 size)
{
    LwU32 *p = (LwU32 *)vp;
    LwU32 count = size / sizeof (LwU32);
    LwU32 i;

    // Must be 4 byte aligned
    for (i=0; i < count; i++)
        MEM_WR32(&p[i], 0);
}

static RC
allocSysmem(struct channel *pChannel, LwHandle *pHandle, LwU64 *pSize, void **ppCpu, LwU64 *pGpuVA)
{
    LW_MEMORY_ALLOCATION_PARAMS allocParams = {0};
    LW_STATUS status;
    RC rc;

    if (ppCpu != NULL)
        *ppCpu = NULL;
    if (pGpuVA != NULL)
        *pGpuVA = 0;

    allocParams.owner = 0xcafe;
    allocParams.type = LWOS32_TYPE_IMAGE;
    allocParams.size = *pSize;
    allocParams.attr = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI) |
                       DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED) |
                       DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS);
    allocParams.attr2 = 0;

    *pHandle = NextHandle++;
    status = LwRmAlloc(pChannel->pConfig->hClient,
                       pChannel->pConfig->hDevice,
                       *pHandle,
                       LW01_MEMORY_SYSTEM,
                       &allocParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    *pSize = allocParams.size;

    if (ppCpu != NULL)
        CHECK_RC(mapCpu(pChannel, *pHandle, *pSize, ppCpu));
    if (pGpuVA != NULL)
        CHECK_RC(mapGpu(pChannel, *pHandle, *pSize, pGpuVA));

    return OK;
}

static RC
allocVidmem(struct channel *pChannel, LwHandle *pHandle, LwU64 *pSize, void **ppCpu, LwU64 *pGpuVA)
{
    LW_MEMORY_ALLOCATION_PARAMS allocParams = {0};
    LW_STATUS status;
    RC rc;

    if (ppCpu != NULL)
        *ppCpu = NULL;
    if (pGpuVA != NULL)
        *pGpuVA = 0;

    allocParams.owner = 0xcafe;
    allocParams.type = LWOS32_TYPE_IMAGE;
    allocParams.size = *pSize;
    allocParams.attr = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);
    allocParams.attr2 = 0;

    *pHandle = NextHandle++;
    status = LwRmAlloc(pChannel->pConfig->hClient,
                       pChannel->pConfig->hDevice,
                       *pHandle,
                       LW01_MEMORY_LOCAL_USER,
                       &allocParams);
    CHECK_RC(RmApiStatusToModsRC(status));

    *pSize = allocParams.size;

    if (ppCpu != NULL)
        CHECK_RC(mapCpu(pChannel, *pHandle, *pSize, ppCpu));
    if (pGpuVA != NULL)
        CHECK_RC(mapGpu(pChannel, *pHandle, *pSize, pGpuVA));

    return OK;
}
static void
freeAlloc(struct channel *pChannel, LwHandle h, void *pCpu, LwU64 gpuVA)
{
    if (gpuVA != 0)
        unmapGpu(pChannel, h, gpuVA);

    if (pCpu != NULL)
        unmapCpu(pChannel, h, pCpu);

    LwRmFree(pChannel->pConfig->hClient, pChannel->pConfig->hDevice, h);
}

static RC
mapCpu(struct channel *pChannel, LwHandle h, LwU64 size, void **ppCpu)
{
    LW_STATUS status;
    RC rc;

    status = LwRmMapMemory(pChannel->pConfig->hClient, pChannel->pConfig->hSubDevice, h, 0, size - 1, ppCpu, 0);
    return RmApiStatusToModsRC(status);
}

static void unmapCpu(struct channel *pChannel, LwHandle h, void *pCpu)
{
    LwRmUnmapMemory(pChannel->pConfig->hClient, pChannel->pConfig->hSubDevice, h, pCpu, 0);
}

static RC
mapGpu(struct channel *pChannel, LwHandle h, LwU64 size, LwU64 *pGpuVA)
{
    LW_STATUS status;

    status = LwRmMapMemoryDma(pChannel->pConfig->hClient,
                              pChannel->pConfig->hDevice,
                              pChannel->pConfig->hVASpace,
                              h,
                              0,
                              size - 1,
                              DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                              DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                              pGpuVA);
    return RmApiStatusToModsRC(status);
}

static void unmapGpu(struct channel *pChannel, LwHandle h, LwU64 gpuVA)
{
    LwRmUnmapMemoryDma(pChannel->pConfig->hClient, pChannel->pConfig->hDevice, pChannel->pConfig->hVASpace, h, 0, gpuVA);
}

static RC
channelAlloc
(
    struct channel *pChannel,
    struct client_config *pConfig,
    char     *name,
    LwHandle  hChannelGroup,
    LwU32     engineType,
    bool      bForceVid,
    bool      bCpuVisible,
    LwU32     gpFifoEntries,
    LwU32     pbSize
)
{
    LW_STATUS status;
    LwU64 totalSize;
    RC rc;

    pChannel->pConfig = pConfig;
    pChannel->name    = name;
    pChannel->pb.size = pbSize;
    pChannel->bForceVid = bForceVid;
    pChannel->bCpuVisible = bCpuVisible;

    CHECK_RC(allocNotify(pConfig->hClient, pConfig->hDevice, pConfig->hVASpace, 1, &pChannel->notifiers));

    // Allocate GPFIFO buffer
    pChannel->gpfifo.entries = gpFifoEntries;
    pChannel->gpfifo.size = (pChannel->gpfifo.entries * LWC86F_GP_ENTRY__SIZE);
    pChannel->gpfifo.gpuVA = 0;
    if (bForceVid)
    {
        CHECK_RC(allocVidmem(pChannel, &pChannel->gpfifo.handle, &pChannel->gpfifo.size,
                             bCpuVisible ? &pChannel->gpfifo.p : NULL,
                             &pChannel->gpfifo.gpuVA));
    }
    else
    {
        CHECK_RC(allocSysmem(pChannel, &pChannel->gpfifo.handle, &pChannel->gpfifo.size,
                             &pChannel->gpfifo.p,
                             &pChannel->gpfifo.gpuVA));
    }
    if (bCpuVisible)
        memzero(pChannel->gpfifo.p, pChannel->gpfifo.size);
    pChannel->gpfifo.pGp = (volatile LwU64 *)pChannel->gpfifo.p;

    // Allocate push buffer for method data
    pChannel->pb.index = 0;
    pChannel->pb.p = NULL;
    totalSize = pChannel->pb.size * pChannel->gpfifo.entries;
    if (bForceVid)
    {
        CHECK_RC(allocVidmem(pChannel, &pChannel->pb.handle, &totalSize,
                             bCpuVisible ? &pChannel->pb.p : NULL,
                             &pChannel->pb.gpuVA));
    }
    else
    {
        CHECK_RC(allocSysmem(pChannel, &pChannel->pb.handle, &totalSize,
                             &pChannel->pb.p,
                             &pChannel->pb.gpuVA));
    }
    for (LwU32 i = 0; i < pChannel->gpfifo.entries; i++)
    {
        if (bCpuVisible)
            pChannel->pb.segment[i].p = (volatile LwU32 *)(((LwU8 *)pChannel->pb.p) + (i * pChannel->pb.size));
        else
            pChannel->pb.segment[i].p = NULL;
        pChannel->pb.segment[i].gpuVA = pChannel->pb.gpuVA + (i * pChannel->pb.size);
    }

    // Allocate USERD page - CheetAh sim does not support sysmem for userd
    pChannel->userd.size = 4096;
    pChannel->userd.p = NULL;
    pChannel->userd.gpuVA = 0;
    if (bForceVid)
    {
        CHECK_RC(allocVidmem(pChannel, &pChannel->userd.handle, &pChannel->userd.size,
                             bCpuVisible ? &pChannel->userd.p : NULL,
                             bCpuVisible ? NULL : &pChannel->userd.gpuVA));
    }
    else
    {
        CHECK_RC(allocSysmem(pChannel, &pChannel->userd.handle, &pChannel->userd.size,
                             bCpuVisible ? &pChannel->userd.p : NULL,
                             bCpuVisible ? NULL : &pChannel->userd.gpuVA));
    }
    if (bCpuVisible)
        memzero(pChannel->userd.p, pChannel->userd.size);
    pChannel->userd.pControl = (Lwc86fControl *)pChannel->userd.p;

    // Allocate channel
    LW_CHANNELGPFIFO_ALLOCATION_PARAMETERS channelAllocParams = { 0 };
    pChannel->hChannel = NextHandle++;
    channelAllocParams.hObjectError = pChannel->notifiers.hNotify[0];
    channelAllocParams.hVASpace = LW01_NULL_OBJECT; // Share device VA space
    channelAllocParams.gpFifoOffset = pChannel->gpfifo.gpuVA;
    channelAllocParams.gpFifoEntries = pChannel->gpfifo.entries;
    channelAllocParams.engineType = engineType;
    channelAllocParams.hUserdMemory[0] = pChannel->userd.handle;
    channelAllocParams.userdOffset[0] = 0;
    status = LwRmAlloc(pConfig->hClient,
                       hChannelGroup,
                       pChannel->hChannel,
                       pConfig->channelClass,
                       &channelAllocParams);
    CHECK_RC(RmApiStatusToModsRC(status));
    pChannel->gpPut = 0;

    return OK;
}

static RC channelSchedule(struct channel *pChannel)
{
    LW_STATUS status;
    RC rc;

    // Schedule channels
    LWC86F_CTRL_GPFIFO_SCHEDULE_PARAMS lwC86fScheduleParams = { 0 };
    lwC86fScheduleParams.bEnable = LW_TRUE;
    status = LwRmControl(pChannel->pConfig->hClient,
                         pChannel->hChannel,
                         LWC86F_CTRL_CMD_GPFIFO_SCHEDULE,
                         &lwC86fScheduleParams, sizeof(lwC86fScheduleParams));
    CHECK_RC(RmApiStatusToModsRC(status));

    // Fetch work submit token - also updates channelNotifier.p[1].info32
    LWC86F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN_PARAMS ws = { 0 };
    status = LwRmControl(pChannel->pConfig->hClient,
                         pChannel->hChannel,
                         LWC86F_CTRL_CMD_GPFIFO_GET_WORK_SUBMIT_TOKEN,
                         &ws, sizeof(ws));
    CHECK_RC(RmApiStatusToModsRC(status));
    pChannel->channelToken = ws.workSubmitToken;

    LW906F_CTRL_GET_CLASS_ENGINEID_PARAMS engineIdParams = { 0 };
    engineIdParams.hObject = pChannel->hEngine;
    status = LwRmControl(pChannel->pConfig->hClient,
                         pChannel->hChannel,
                         LW906F_CTRL_GET_CLASS_ENGINEID,
                         &engineIdParams, sizeof(engineIdParams));
    CHECK_RC(RmApiStatusToModsRC(status));
    pChannel->setObjectData = engineIdParams.classEngineID;

    pChannel->sequence.p = (volatile LwU32 *)(((LwU8 *)pChannel->notifiers.p) + pChannel->notifiers.scratchOffset);
    pChannel->sequence.gpuVA = pChannel->notifiers.gpuVA + pChannel->notifiers.scratchOffset;
    pChannel->sequence.put = 0x0;
    pChannel->sequence.get = 0x0;
    MEM_WR32(pChannel->sequence.p, pChannel->sequence.get);
    pChannel->notifiers.scratchOffset += 16;

    return OK;
}

static RC channelDisable(struct channel *pChannel)
{
    LW_STATUS status;
    RC rc;

    // Schedule channels
    LWC86F_CTRL_GPFIFO_SCHEDULE_PARAMS lwC86fScheduleParams = { 0 };
    lwC86fScheduleParams.bEnable = LW_FALSE;
    status = LwRmControl(pChannel->pConfig->hClient,
                         pChannel->hChannel,
                         LWC86F_CTRL_CMD_GPFIFO_SCHEDULE,
                         &lwC86fScheduleParams, sizeof(lwC86fScheduleParams));
    CHECK_RC(RmApiStatusToModsRC(status));

    return OK;
}

static void
channelFlush(struct channel *pChannel)
{
    LwU32 gp[2];

    // Construct GPFIFO entry
    LwU64 gpuVA = pChannel->pb.segment[pChannel->gpPut].gpuVA;
    gp[0] = DRF_DEF(C86F, _GP_ENTRY0, _FETCH, _UNCONDITIONAL) |
        DRF_NUM(C86F, _GP_ENTRY0, _GET, LwU64_LO32(gpuVA) >> 2);

    gp[1] = DRF_NUM(C86F, _GP_ENTRY1, _GET_HI, LwU64_HI32(gpuVA)) |
        DRF_NUM(C86F, _GP_ENTRY1, _LENGTH, pChannel->pb.index) |
        DRF_DEF(C86F, _GP_ENTRY1, _SYNC, _PROCEED) |
        DRF_DEF(C86F, _GP_ENTRY1, _LEVEL, _MAIN);

    // Write GPFIFO entry
    MEM_WR64(&pChannel->gpfifo.pGp[pChannel->gpPut], (((LwU64)gp[1])<<32) | gp[0]);

    // Update to next GPFIFO entry
    pChannel->gpPut = (pChannel->gpPut + 1) & (pChannel->gpfifo.entries - 1);

    // Write "next" GPPUT value
    MEM_WR32(&pChannel->userd.pControl->GPPut, pChannel->gpPut);
    pChannel->pb.index = 0;

    // Test is not written to use WC, but this is the supported fence
    Platform::FlushCpuWriteCombineBuffer();

    // Ring doorbell from the CPU
    channelRingDoorbell(pChannel);
}

static void channelRingDoorbell(struct channel *pChannel)
{
    MEM_WR32(pChannel->pConfig->usermode.p + LWC661_DOORBELL, pChannel->channelToken);
}

static bool channelEmpty(void *vc)
{
    struct channel *pChannel = (struct channel *)vc;

    pChannel->sequence.get = MEM_RD32(pChannel->sequence.p);

    return pChannel->sequence.get == pChannel->sequence.put;
}

static bool channelNotFull(void *vc)
{
    struct channel *pChannel = (struct channel *)vc;
    LwU32 mask = pChannel->gpfifo.entries - 1;
    LwU32 used;

    pChannel->sequence.get = MEM_RD32(pChannel->sequence.p);

    used = (pChannel->sequence.put - pChannel->sequence.get) & mask;

    return used != mask;
}

static void
channelFree(struct channel *pChannel)
{
    LwRmFree(pChannel->pConfig->hClient, pChannel->pConfig->hDevice, pChannel->hChannel);
    freeAlloc(pChannel, pChannel->gpfifo.handle, pChannel->gpfifo.p, pChannel->gpfifo.gpuVA);
    freeAlloc(pChannel, pChannel->pb.handle, pChannel->pb.p, pChannel->pb.gpuVA);
    freeAlloc(pChannel, pChannel->userd.handle, pChannel->userd.p, pChannel->userd.gpuVA);
    freeNotify(&pChannel->notifiers);
}

static void
eschedReleaseMethods(struct channel *pChannel, LwU32 *pMethods, LwU32 *pi, LwU64 gpuVA, LwU32 *pVal, LwU32 flags)
{
    LwU32 execute = DRF_DEF(C86F, _SEM_EXELWTE, _OPERATION, _RELEASE) |
                    ((flags & RELEASE_64BIT) ?
                        DRF_DEF(C86F, _SEM_EXELWTE, _PAYLOAD_SIZE, _32BIT) :
                        DRF_DEF(C86F, _SEM_EXELWTE, _PAYLOAD_SIZE, _32BIT)) |
                    DRF_DEF(C86F, _SEM_EXELWTE, _RELEASE_TIMESTAMP, _DIS);

    if (flags & (RELEASE_MEMBAR|RELEASE_SYSMEMBAR))
        execute |= DRF_DEF(C86F, _SEM_EXELWTE, _RELEASE_WFI, _EN);
    else
        execute |= DRF_DEF(C86F, _SEM_EXELWTE, _RELEASE_WFI, _DIS);

    pMethods[(*pi)++] = LW_METHOD_N(pChannel->subch, LWC86F_SEM_ADDR_LO, 5);
    pMethods[(*pi)++] = LwU64_LO32(gpuVA);
    pMethods[(*pi)++] = LwU64_HI32(gpuVA);
    pMethods[(*pi)++] = pVal[0];
    pMethods[(*pi)++] = (flags & RELEASE_64BIT) ? pVal[1] : 0;
    pMethods[(*pi)++] = execute;
}

static void
computeInitMethods(struct channel *pChannel, LwU32 *pMethods, LwU32 *pi)
{
    pMethods[(*pi)++] = LW_METHOD(pChannel->subch, LWC86F_SET_OBJECT);
    pMethods[(*pi)++] = pChannel->setObjectData;
}

static void
computeReleaseMethods(struct channel *pChannel, LwU32 *pMethods, LwU32 *pi, LwU64 gpuVA, LwU32 *pVal, LwU32 flags)
{
    pMethods[(*pi)++] = LW_METHOD_N(pChannel->subch, LWCBC0_SET_REPORT_SEMAPHORE_PAYLOAD_LOWER, 5);
    pMethods[(*pi)++] = pVal[0];
    pMethods[(*pi)++] = (flags & RELEASE_64BIT) ? pVal[1] : 0;
    pMethods[(*pi)++] = LwU64_LO32(gpuVA);
    pMethods[(*pi)++] = LwU64_HI32(gpuVA);
    pMethods[(*pi)++] =
        DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _OPERATION, _RELEASE) |
        DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _AWAKEN_ENABLE, _FALSE) |
        ((flags & RELEASE_64BIT) ?
            DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _STRUCTURE_SIZE, _SEMAPHORE_TWO_WORDS) :
            DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _STRUCTURE_SIZE, _SEMAPHORE_ONE_WORD)) |
        ((flags & (RELEASE_SYSMEMBAR | RELEASE_MEMBAR)) ?
            DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _FLUSH_DISABLE, _FALSE) :
            DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _FLUSH_DISABLE, _TRUE)) |
        DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _REDUCTION_ENABLE, _FALSE) |
        ((flags & RELEASE_64BIT) ?
            DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _PAYLOAD_SIZE64, _TRUE) :
            DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _PAYLOAD_SIZE64, _FALSE)) |
        DRF_DEF(CBC0, _REPORT_SEMAPHORE_EXELWTE, _TRAP_TYPE, _TRAP_NONE);
}

static void
ceInitMethods(struct channel *pChannel, LwU32 *pMethods, LwU32 *pi)
{
    pMethods[(*pi)++] = LW_METHOD(pChannel->subch, LWC86F_SET_OBJECT);
    pMethods[(*pi)++] = pChannel->setObjectData;
    pMethods[(*pi)++] = LW_METHOD(pChannel->subch, LWC8B5_LINE_COUNT);
    pMethods[(*pi)++] = 1;
}

static void
ceCopyMethods(struct channel *pChannel, LwU32 *pMethods, LwU32 *pi, LwU64 srcVA, LwU64 dstVA, LwU32 size, bool bSysFlush)
{
    LwU32 launch = DRF_DEF(C8B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _NONE) |
        DRF_DEF(C8B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
        DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
        DRF_DEF(C8B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
        DRF_DEF(C8B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
        DRF_DEF(C8B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _PIPELINED);

    if (bSysFlush)
        launch |= DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_TYPE, _SYS);
    else
        launch |= DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_TYPE, _GL);

    pMethods[(*pi)++] = LW_METHOD_N(pChannel->subch, LWC8B5_OFFSET_IN_UPPER, 4);
    pMethods[(*pi)++] = LwU64_HI32(srcVA);
    pMethods[(*pi)++] = LwU64_LO32(srcVA);
    pMethods[(*pi)++] = LwU64_HI32(dstVA);
    pMethods[(*pi)++] = LwU64_LO32(dstVA);
    pMethods[(*pi)++] = LW_METHOD(pChannel->subch, LWC8B5_LINE_LENGTH_IN);
    pMethods[(*pi)++] = size;

    pMethods[(*pi)++] = LW_METHOD(pChannel->subch, LWC8B5_LAUNCH_DMA);
    pMethods[(*pi)++] = launch;
}

static void
ceReleaseMethods(struct channel *pChannel, LwU32 *pMethods, LwU32 *pi, LwU64 gpuVA, LwU32 *pVal, LwU32 flags)
{
    LwU32 launch = DRF_DEF(C8B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
        DRF_DEF(C8B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
        DRF_DEF(C8B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NONE);

    // High dword w/o a flush
    // TODO: Use Ampere+ 64-bit semaphore release by default?
    if (flags & RELEASE_64BIT)
    {
        pMethods[(*pi)++] = LW_METHOD_N(pChannel->subch, LWC8B5_SET_SEMAPHORE_A, 3);
        pMethods[(*pi)++] = LwU64_HI32(gpuVA + 4);
        pMethods[(*pi)++] = LwU64_LO32(gpuVA + 4);
        pMethods[(*pi)++] = pVal[1];
        pMethods[(*pi)++] = LW_METHOD(pChannel->subch, LWC8B5_LAUNCH_DMA);
        pMethods[(*pi)++] = launch;
    }

    if (flags & RELEASE_SYSMEMBAR)
        launch |= DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_TYPE, _SYS) |
            DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE);
    else if (flags & RELEASE_MEMBAR)
        launch |= DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_TYPE, _GL) |
            DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE);

    pMethods[(*pi)++] = LW_METHOD_N(pChannel->subch, LWC8B5_SET_SEMAPHORE_A, 3);
    pMethods[(*pi)++] = LwU64_HI32(gpuVA);
    pMethods[(*pi)++] = LwU64_LO32(gpuVA);
    pMethods[(*pi)++] = pVal[0];
    pMethods[(*pi)++] = LW_METHOD(pChannel->subch, LWC8B5_LAUNCH_DMA);
    pMethods[(*pi)++] = launch;
}

//! \brief Setup the JS hierarchy to match the C++ one.
JS_CLASS_INHERIT(CCLaunchTest, RmTest, "Confidential Compute Launch Test.\n");
