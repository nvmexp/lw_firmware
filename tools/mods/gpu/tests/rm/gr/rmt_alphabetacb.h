#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/tests/rmtest.h"

#include "gpu/tests/gputestc.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"

#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl9197.h" // FERMI_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/cl9097sw.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#define RM_PAGE_SIZE    (1024 * 4)

#define KASSERT( condition, errorMessage, rcVariable )                                   \
if ( ! ( condition ) )                                                                   \
{                                                                                        \
    Printf( Tee::PriHigh, errorMessage );                                                \
    rcVariable = RC::LWRM_ERROR;                                                         \
}

//!
//! Flag to indicate Successful semaphore releases
//!
#define FE_SEMAPHORE_VALUE  0xF000F000

#define CLASS9197_SUCH_FERMI        0x0
#define NUM_TEST_CHANNELS           2

#define CIRLWLAR_BUFFER                   0
#define ALPHA_CIRLWLAR_BUFFER             1

class AlphaBetaCBTest: public RmTest
{
public:
    AlphaBetaCBTest();
    virtual ~AlphaBetaCBTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    UINT64           m_gpuAddr                     = 0;
    UINT32 *         m_cpuAddr                     = nullptr;

    Channel *       m_pCh[NUM_TEST_CHANNELS]       = {};

    LwRm::Handle    m_hCh[NUM_TEST_CHANNELS]       = {};
    LwRm::Handle    m_hVA                          = 0;
    LwRm::Handle    m_hSemMem                      = 0;

    Notifier        m_Notifier[NUM_TEST_CHANNELS];
    ThreeDAlloc     m_3dAlloc[NUM_TEST_CHANNELS];
    FLOAT64         m_TimeoutMs                    = Tasker::NO_TIMEOUT;

    UINT32          m_cbInitial                    = 0;
    UINT32          m_alphaCbInitial               = 0;

    LwBool          m_bNeedWfi                     = LW_FALSE;

    RC VerifyCB();
    RC VerifyAlphaCB();
    RC FermiInit(UINT32 ch);
    RC FermiNotify(UINT32 ch);
    RC CheckCBValue(UINT32 ch, UINT32 expected);
    RC CheckAlphaCBValue(UINT32 ch, UINT32 expected);
    RC FermiSetCBSize(UINT32 ch, UINT32 cbSize, UINT32 gpuFamily);
    RC FermiSetAlphaCBSize(UINT32 ch, UINT32 cbSize, UINT32 gpuFamily);
    RC VerifyChannelContext(UINT32 ch, bool typeFlag, UINT32 cbSize, UINT32 warpValue, UINT32 globalValue, UINT32 gpuFamily);
    void FermiGetBetaInitValue(GpuSubdevice *pSubdev);
    void FermiGetAlphaInitValue(GpuSubdevice *pSubdev);
    RC CheckPascalCBValue(UINT32 ch, UINT32 expected);
    RC CheckPascalAlphaCBValue(UINT32 ch, UINT32 expected);
    void PascalGetBetaInitValue(GpuSubdevice *pSubdev);
    void PascalGetAlphaInitValue(GpuSubdevice *pSubdev);
};

bool RmtPollFunc(void *pArg);

