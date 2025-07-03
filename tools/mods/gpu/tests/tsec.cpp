/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tsec.cpp
 * @brief  Implementation of TSEC test.
 *
 */

#include "gputest.h"
#include "gpu/utility/surf2d.h"
#include "gputestc.h"
#include "core/include/golden.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "core/include/tar.h"
#include "core/include/platform.h"
#include "class/cle26e.h"
#include "class/cl95a1.h"
#include "core/include/utility.h"
#include "gpu/utility/gpurectfill.h"
#include "core/include/fileholder.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "t11x/t114/dev_sec_pri.h" // LW_PSEC_THI_METHOD0, LW_PSEC_THI_METHOD1
#include "gpu/include/gralloc.h"
#include "tsec_drv.h"
#include "core/include/xp.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "core/include/version.h"

// TODO : The TSEC Application Ucode should be submitted in a common location
#include "gpu/tests/tsec/cl95a1_secopuc.h"

// TODO : The defines in this file should probably live outside of MODS
#include "gpu/tests/tsec/cl95a1_secopsc.h"

using Utility::AlignUp;
using Utility::AlignDown;

// AES block size constants
#define AES_SIZE_BYTES   16
#define AES_SIZE_UINT32  (AES_SIZE_BYTES / sizeof(UINT32))

// TS Packet definitions
#define PID_MAILWIDEO           (0x1011)
#define PID_SUBVIDEO            (0x00001b00)
#define PID_PMT                 (0x00000100)
#define TAG_HDMV_COPYCTRL       (0x88)
#define TAG_BD_SYSTEMUSE        (0x89)

// Methods for communicating with the TSEC on CheetAh (the methods need to be
// wrapped)
#define TEGRA_TSEC_METHOD0    ((LW_PSEC_THI_METHOD0 - DRF_BASE(LW_PSEC)) >> 2)
#define TEGRA_TSEC_METHOD1    ((LW_PSEC_THI_METHOD1 - DRF_BASE(LW_PSEC)) >> 2)

// Default surface sizes for TSEC surfaces, these maximums were determined by
// checking the maximum used by any of the current streams.  If any of the
// streams changes, these maximums will also have to change
#define TSIN_SIZE         0xf0000   //!< TS Input surface size
#define TSOUT_SIZE        0x80000   //!< TS Output surface size
#define ESMAIN_SIZE       0xf0000   //!< ES Main Output surface size
#define ESSUB_SIZE        0x80000   //!< ES Sub Output surface size

#define TS_PACKET_SIZE    0xC0      //!< Size of a TS packet

//! Aligned size for processing batches of TS data (must start on the alignment
//! and also be a multiple of the alignment)
#define ALIGNED_UNIT_SIZE 0x1800

//! Loops per stream, used for golden callwlations.  This number should be set
//! higher that the number of batches in any stream and for easy bookkeeping
//! an even number should be chosen (i.e. stream 0 starts at loop 0, stream 1
//! starts at loop 100, etc.)
#define LOOPS_PER_STREAM  100

//! Default width of the TSEC surfaces that are sized based.  This is helpful
//! since the surface filling routines want a surface with a real size and a
//! surface of width 1M x 1 will throw them off
#define DEFAULT_WIDTH     1024

//----------------------------------------------------------------------------
//! TSEC Test.
//!
//! The TSEC test passes multiple encoded bitstreams through the TSEC engine
//! and CRCs the three output surfaces (TS Out, ES Main, and ES Sub).
//!
//! This test was ported from the mpeg2ts security tests found in GIT at
//!   vendor/lwpu/cheetah/multimedia/security/tsec_mpeg2ts/...
//!   vendor/lwpu/cheetah/tests-multimedia/security/mpeg2ts/...
//!
class Tsec : public GpuTest
{
public:
    Tsec();
    ~Tsec() {}

    virtual bool IsSupported();
    RC Setup();
    RC Run();
    RC Cleanup();

    SETGET_PROP(StreamEnableMask, UINT32);
    SETGET_PROP(MaxBatchesPerStream, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(DumpSurfaces, bool);
    SETGET_PROP(UseNotify, bool);
    SETGET_PROP(FlushPes, bool);

private:
    //! Enumeration of the various surfaces used in the TSEC test
    enum SurfId
    {
        // Output surfaces, cleared between each stream
        TSOUT_SURF      //!< Output transport stream
        ,ESMAIN_SURF    //!< Output main elementary stream (video)
        ,ESSUB_SURF     //!< Output sub elementary stream (audio)

        // Input surfaces, cleared between each stream
        ,TSIN_SURF      //!< Input transport stream
        ,BSVM_SURF      //!< Input bitstream virtual machine data
        ,FLOWCTRL_SURF  //!< Input flow control data

        // Permanent surfaces, not cleared between streams, initialized once
        ,UCODE_SURF     //!< TSEC bitstream application uCode
        ,KEY_SURF       //!< TSEC keystore data
        ,SEM_SURF       //!< TSEC semaphore for completion notification

        ,SURF_COUNT
        ,FIRST_INPUT_SURF = TSIN_SURF
        ,FIRST_PERMANENT_SURF = UCODE_SURF
    };

    //! Structure describing a TSEC stream
    struct StreamParams
    {
        string BitstreamFile;   //!< Bitstream file name
        UINT32 MainCodec;       //!< Main codec type (video)
        UINT32 SubCodec;        //!< Sub codec type (audio)
        UINT32 BatchSize;       //!< Size of input transport stream batch to
                                //!< process
        UINT32 PatchMode;       //!< Patch mode for the input stream
        bool   bPlainCopy;      //!< true to pass the input transport stream
                                //!< unencrypted to the TSEC
    };

    //! Structure for polling on the completion notifier
    struct NotifyPollArgs
    {
        void * pAddr;   //!< CPU virtual address to poll
        UINT32 Data;    //!< Data that the CPU needs to wait for
    };

    // MODS versions of TSEC structures
    typedef secop_flow_ctrl    TsecFlowControl;
    typedef secop_bitstream_vm TsecBitstreamVm;
    typedef secop_key_store    TsecKeyStore;

    // Pointers to generic test data
    GpuTestConfiguration *  m_pTestConfig;
    GpuGoldenSurfaces *     m_pGGSurfs;
    Goldelwalues *          m_pGolden;
    Channel *               m_pCh;

    //! Current loop number being run in the test.  Standard looping rules do
    //! not apply.  The looping structure is:
    //!   1. Each stream starts at StreamIndex * LOOPS_PER_STREAM
    //!   2. Each batch of processed TS data is considered a Loop
    //!   3. There must not be more than LOOPS_PER_STREAM batches in a single
    //!      stream
    UINT32                  m_LoopNum;

    // Values set from JS
    UINT32          m_StreamEnableMask;
    UINT32          m_MaxBatchesPerStream;
    bool            m_KeepRunning = false;
    bool            m_DumpSurfaces;
    bool            m_UseNotify;
    bool            m_FlushPes;

    //! Test surfaces (all types) used for communication with the TSEC
    Surface2D       m_TestSurfs[SURF_COUNT];

    //! Class used to perform Gpu based rectangle fills (i.e. clears on the
    //! test surfaces
    GpuRectFill     m_GpuRectFill;
    UINT32          m_UcodeSize;   //!< Size of the TSEC bitstream ucode
    TarArchive      m_FileArchive; //!< Tar file containing all the input
                                   //!< bitstreams
    TsecAlloc       m_TsecAlloc;

    bool            m_OrigTsecDebug;

    // Structures used for communication with the TSEC engine
    TsecFlowControl m_FlowControl;
    TsecBitstreamVm m_BitstreamVm;
    TsecKeyStore    m_KeyStore;

    // Bitstream input data
    static const StreamParams s_Streams[];
    static const UINT32 s_StreamCount;

    void GetSurfaceSize(const SurfId surfId, UINT32 *pWidth, UINT32 *pHeight);
    RC   ClearSurface(const SurfId surfId, bool bWait);
    RC   AllocSurfaces();
    void DumpSurfaces(const bool bPreBatch);
    void GenerateFakePatchRecord(UINT08* pPacket);
    RC InitializeStream
    (
        const StreamParams * pStream,
        const TarFile *      pTarFile,
        UINT32 *             pBatchCount
    );
    RC RunBatch
    (
        const StreamParams * pStream,
        const UINT32         tsInBuffOff,
        const UINT32         skipPackets,
        const UINT32         processPackets,
        const bool           bFlushPes
    );
    RC RunStream
    (
        const StreamParams * pStream,
        const TarFile *      pTarFile,
        const UINT32         batchCount,
        bool                 bFlushPes
    );

    static bool NotifierPollFunc(void *pvArgs);

    //-------------------------------------------------------------------------
    // AES Data and encryption routines
    //
    // Static data used by the AES encryption routines
    static const LwU32  s_Kt0[AES_SIZE_UINT32];
    static const UINT32 s_IvCtrTs[AES_SIZE_UINT32];
    static const UINT32 s_IvCtrEs0[AES_SIZE_UINT32];
    static const UINT32 s_IvCtrEs1[AES_SIZE_UINT32];
    static const UINT32 s_IvCbc[AES_SIZE_UINT32];
    static const LwU32  s_Ks[AES_SIZE_UINT32];
    static const LwU32  s_Kr[AES_SIZE_UINT32];
    static const LwU32  s_Kps[AES_SIZE_UINT32];
    static const LwU32  s_Kvm[AES_SIZE_UINT32];
    static const LwU32  s_Khs[AES_SIZE_UINT32];
    static const LwU32  s_Kc[AES_SIZE_UINT32];
    static const UINT08 s_TeS[256];
    static const UINT32 s_Te0[256];
    static const UINT32 s_Te1[256];
    static const UINT32 s_Te2[256];
    static const UINT32 s_Te3[256];
    static const UINT32 s_DecryptiolwM[];
    static const UINT32 s_Rcon[];
    static const UINT32 s_PatchVM[];
    static const UINT32 s_PatchVMSize;

    void AesGenerateVMBuffer(TsecBitstreamVm* pvmData, UINT32 patchMode);
    void AesDmHash
    (
        const UINT08* pData,
        const UINT32  size,
        LwU32         hash[AES_SIZE_UINT32]
    );
    void AesDmHashMac
    (
        const UINT08* pData,
        const UINT32  size,
        const LwU32   key[AES_SIZE_UINT32],
        LwU32         mac[AES_SIZE_UINT32]
    );
    void AesEcbEncrypt
    (
        const UINT08* pData,
        const UINT32  size,
        const LwU32   key[AES_SIZE_UINT32],
        UINT08*       pCipherText
    );
    void AesRijndaelKeySetupEnc(LwU32 rk[44], const LwU32 cipherKey[4]);
    void AesRijndaelEncrypt
    (
        const LwU32 rk[44],
        const LwU32 pt[4],
        LwU32       ct[4]
    );
    void AesGenerateStaticTestKeyStoreBuffer(TsecKeyStore* pKeyStore);
    void AesProtectKeyStoreBuffer(TsecKeyStore* pKeyStore);
    void AesEncryptAlignedUnit
    (
        UINT08 * pTSEncOut,
        UINT08 * pTSIn,
        bool     bPlainCopy
    );
    void AesGeneratePatchParamBuffer(UINT08 *pPatchData, UINT32 patchMode);
    // End of AES Data and encryption routines
    //-------------------------------------------------------------------------
};

//! Stream data for the TSEC test
const Tsec::StreamParams Tsec::s_Streams[] =
{
//        BS File  MainCod  SubCod  Batch Size  Patch Mode  Plain Copy
     { "tsec.mp2",       1,      0,    0x30000,          0,      false }
    ,{ "tsec.vc1",       2,      0,    0x30000,          1,      false }
    ,{ "tsec.264",       3,      0,    0x30000,          1,      true  }
};
const UINT32 Tsec::s_StreamCount = sizeof(s_Streams) /
                                        sizeof(Tsec::StreamParams);

//----------------------------------------------------------------------------
// Tsec script interface.
JS_CLASS_INHERIT(Tsec, GpuTest, "Tsec object.");
CLASS_PROP_READWRITE(Tsec, StreamEnableMask, UINT32,
    "Mask of streams to enable (default = all streams)");
CLASS_PROP_READWRITE(Tsec, MaxBatchesPerStream, UINT32,
    "Set the maximum number of batches per stream to process (default = 100)");
CLASS_PROP_READWRITE(Tsec, KeepRunning, bool,
    "The test will keep running as long as this flag is set");
CLASS_PROP_READWRITE(Tsec, DumpSurfaces, bool,
    "Dump surfaces for each batch (useful for validating correctness)");
CLASS_PROP_READWRITE(Tsec, UseNotify, bool,
    "Use notifier instead of WaitIdle when waiting for batches to complete");
CLASS_PROP_READWRITE(Tsec, FlushPes, bool,
    "Generate a flush after the last batch when processing packets");

//----------------------------------------------------------------------------
//! \brief Constructor
//!
Tsec::Tsec()
 :  m_pTestConfig(NULL)
   ,m_pGGSurfs(NULL)
   ,m_pGolden(NULL)
   ,m_pCh(NULL)
   ,m_LoopNum(0)
   ,m_StreamEnableMask(~0)
   ,m_MaxBatchesPerStream(100)
   ,m_DumpSurfaces(false)
   ,m_UseNotify(false)
   ,m_FlushPes(false)
   ,m_UcodeSize(0)
   ,m_OrigTsecDebug(false)
{
    SetName("Tsec");
    m_pTestConfig = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();
}

//----------------------------------------------------------------------------
//! \brief Return whether the TSEC test is supported
//!
//! \return true if supported, false otherwise
bool Tsec::IsSupported()
{
    GpuDevice * pGpudev = GetBoundGpuDevice();

    if (!m_TsecAlloc.IsSupported(pGpudev))
    {
        return false;
    }

    // This test is written around the TSEC BITPROCESS application which is
    // not present in non-SOC chips even if the class is supported
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    if (!pSubdev->IsSOC())
    {
        return false;
    }

    if (Platform::HasClientSideResman())
    {
        // This test lwrrently only works when SEC debug is enabled since it is
        // using fixed keys.  For kernel driver debug mode will be forced during
        // Run
        if (!pSubdev->HasFeature(Device::GPUSUB_HAS_SEC_DEBUG) ||
            !pSubdev->IsFeatureEnabled(Device::GPUSUB_HAS_SEC_DEBUG))
        {
            Printf(Tee::PriLow,
                   "Tsec test not supported if SEC debug is not enabled!!\n");
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------
//! \brief Setup the TSEC test
//!
//! \return OK if setup succeeds, not OK otherwise
RC Tsec::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    if (!Platform::HasClientSideResman() && !CheetAh::SocPtr()->GetTsecDebug())
    {
        m_OrigTsecDebug = false;

        CHECK_RC(CheetAh::SocPtr()->EnableTsecDebug(true));

        if (!CheetAh::SocPtr()->GetTsecDebug())
        {
            Printf(Tee::PriHigh, "%s : Unable to enable TSEC debug mode!\n",
                   GetName().c_str());
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }
    }
    else
    {
        m_OrigTsecDebug = true;
    }

    // Channel should be allocated prior to allocating the rect fill object
    CHECK_RC(m_GpuRectFill.Initialize(m_pTestConfig));

    // UCode size needs to be initialized prior surfaces since the ucode
    // surface size depends on the actual size of the ucode (go figure!)
    m_UcodeSize = cl95a1_secop_ucode_size[LW95A1_SECOP_BITPROCESS_UCODE_ID];
    m_UcodeSize = (m_UcodeSize + 0xFF) & ~0xFF;
    MASSERT(m_UcodeSize);

    CHECK_RC(AllocSurfaces());

    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
    m_pGGSurfs->AttachSurface(&m_TestSurfs[TSOUT_SURF], "TSOUT", 0);
    m_pGGSurfs->AttachSurface(&m_TestSurfs[ESMAIN_SURF], "ESMAIN", 0);
    m_pGGSurfs->AttachSurface(&m_TestSurfs[ESSUB_SURF], "ESSUB", 0);
    CHECK_RC(m_pGolden->SetSurfaces(m_pGGSurfs));

    AesGenerateStaticTestKeyStoreBuffer(&m_KeyStore);
    AesProtectKeyStoreBuffer(&m_KeyStore);
    Platform::MemCopy(m_TestSurfs[KEY_SURF].GetAddress(),
                      &m_KeyStore,
                      sizeof(m_KeyStore));

    LwU32 *pUCode = NULL;
    // Ucode offset is the byte offset, but the uCode array is actually a
    // LwU32 array
    const UINT32 uCodeOff =
        cl95a1_secop_ucode_offset[LW95A1_SECOP_BITPROCESS_UCODE_ID] /
        sizeof(LwU32);
    pUCode = cl95a1_secop_ucode_img + uCodeOff;
    MASSERT(pUCode);

    Platform::MemCopy(m_TestSurfs[UCODE_SURF].GetAddress(),
                      pUCode,
                      m_UcodeSize);

    const string tsecArchive = Utility::DefaultFindFile("tsec.bin", true);
    if (!m_FileArchive.ReadFromFile(tsecArchive, true))
    {
        Printf(Tee::PriHigh, "Error loading %s\n", tsecArchive.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }
    // Ensure that all previous CPU accesses to the surface are fully flushed
    Platform::FlushCpuWriteCombineBuffer();

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the TSEC test
//!
//! \return OK if running succeeds, not OK otherwise
RC Tsec::Run()
{
    const bool isBackground = m_KeepRunning;

    StickyRC rc;
    vector<UINT32> channelClasses;
    channelClasses =
        CheetAh::SocPtr()->GetChannelClassList(m_TsecAlloc.GetSupportedClass(GetBoundGpuDevice()));

    if (channelClasses.empty())
    {
        Printf(Tee::PriHigh, "%s : No TSEC channel classes found\n", GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    // Only generate goldens on the first instance (allow check to run on all
    // instances)
    if (m_pGolden->GetAction() == Goldelwalues::Store)
        channelClasses.resize(1);

    for (UINT32 tsecInst = 0;
         (isBackground && m_KeepRunning) || (tsecInst < channelClasses.size());
         tsecInst++)
    {
        if (tsecInst >= channelClasses.size())
        {
            tsecInst = 0;
        }

        LwRm::Handle hCh;
        CHECK_RC(m_pTestConfig->AllocateChannel(&m_pCh, &hCh));

        Printf(GetVerbosePrintPri(),
               "%s : Running on TSEC instance %u\n",
               GetName().c_str(), tsecInst);

        CHECK_RC(m_pCh->SetClass(0, channelClasses[tsecInst]));
        for (UINT32 stream = 0;
             (stream < s_StreamCount) && (!isBackground || m_KeepRunning);
             stream++)
        {
            m_LoopNum = stream * LOOPS_PER_STREAM;
            m_pGolden->SetLoop(m_LoopNum);

            if (!(m_StreamEnableMask & (1 << stream)))
                continue;

            const StreamParams * pStream = &s_Streams[stream];
            TarFile *pTarFile = m_FileArchive.Find(pStream->BitstreamFile);
            if (pTarFile == NULL)
            {
                Printf(Tee::PriHigh, "File %s does not exist in archive\n",
                       pStream->BitstreamFile.c_str());
                return RC::FILE_DOES_NOT_EXIST;
            }

            UINT32 batchCount;
            CHECK_RC(InitializeStream(pStream, pTarFile, &batchCount));

            bool bFlushPes = m_FlushPes;
            UINT32 pesBatch = bFlushPes ? 1 : 0;
            if ((batchCount + pesBatch) > m_MaxBatchesPerStream)
            {
                batchCount = m_MaxBatchesPerStream;

                // If the stream was clipped then the flush at end gets clipped
                // first
                bFlushPes = false;
            }

            // When generating goldens we force a skip count of 1, this allows
            // generated goldens to be used with any user set MaxBatchesPerStream
            // value since the each subsequent frame output surface also contains
            // all previous frames.  When checking, only check at the end of all
            // batches to save test time
            m_pGolden->SetSkipCount(1);
            if (m_pGolden->GetAction() == Goldelwalues::Check)
                m_pGolden->SetSkipCount(batchCount);

            rc = RunStream(pStream, pTarFile, batchCount, bFlushPes);

            // If the test fails due to a miscompare then regress the failing batch
            // which is possible due to how goldens are generated
            if ((m_pGolden->GetAction() == Goldelwalues::Check) &&
                (m_pGolden->GetSkipCount() != 1) &&
                ((rc == RC::GOLDEN_VALUE_MISCOMPARE) ||
                 (rc == RC::GOLDEN_VALUE_MISCOMPARE_Z)))
            {
                Printf(Tee::PriHigh, "Regressing failing batch...");
                m_pGolden->SetLoop(stream * LOOPS_PER_STREAM);
                m_pGolden->SetSkipCount(1);
                rc = RunStream(pStream, pTarFile, batchCount, bFlushPes);
            }
            CHECK_RC(rc);
        }
        CHECK_RC(GetTestConfiguration()->FreeChannel(m_pCh));
        m_pCh = nullptr;
    }

    // Golden errors that are deferred due to "-run_on_error" can only be
    // retrieved by running Golden::ErrorRateTest().  This must be done
    // before clearing surfaces
    CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Cleanup the TSEC test
//!
//! \return OK if cleanup succeeds, not OK otherwise
RC Tsec::Cleanup()
{
    StickyRC rc;

    m_TsecAlloc.Free();

    if (!Platform::HasClientSideResman() && !m_OrigTsecDebug &&
        (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
    {
        rc = CheetAh::SocPtr()->EnableTsecDebug(false);
    }

    // Free golden buffer.
    if (m_pGGSurfs)
    {
        m_pGolden->ClearSurfaces();
        delete m_pGGSurfs;
        m_pGGSurfs = NULL;
    }

    for (UINT32 surfId = 0; surfId < SURF_COUNT; surfId++)
    {
        m_TestSurfs[surfId].Free();
    }

    rc = m_GpuRectFill.Cleanup();
    if (m_pCh != nullptr)
        rc = GetTestConfiguration()->FreeChannel(m_pCh);
    rc = GpuTest::Cleanup();

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Return the required surface size for a TSEC surface
//!
//! \param surfId  : Surface ID to get the size for
//! \param pWidth  : Returned width of the surface
//! \param pHeight : Returned height of the surface
//!
void Tsec::GetSurfaceSize(const SurfId surfId, UINT32 *pWidth, UINT32 *pHeight)
{
    UINT32 bytesPerPixel = ColorUtils::PixelBytes(ColorUtils::VOID32);
    UINT32 defaultPitch = DEFAULT_WIDTH * bytesPerPixel;
    UINT32 size = 0;

    *pWidth = 0;
    *pHeight = 0;

    switch (surfId)
    {
        case TSIN_SURF:
            size = TSIN_SIZE;
            break;
        case TSOUT_SURF:
            size = TSOUT_SIZE;
            break;
        case ESMAIN_SURF:
            size = ESMAIN_SIZE;
            break;
        case ESSUB_SURF:
            size = ESSUB_SIZE;
            break;
        case UCODE_SURF:
            size = m_UcodeSize;
            break;
        case KEY_SURF:
            size = sizeof(TsecKeyStore);
            break;
        case BSVM_SURF:
            size = sizeof(TsecBitstreamVm);
            break;
        case FLOWCTRL_SURF:
            size = sizeof(TsecFlowControl);
            break;
        case SEM_SURF:
            size = 256;
            break;
        default:
            MASSERT(!"Unknown surface ID");
            return;
    }

    if (size >= defaultPitch)
    {
        *pWidth = DEFAULT_WIDTH;
        *pHeight = (size + defaultPitch - 1) / defaultPitch;
    }
    else
    {
        *pWidth = (size + bytesPerPixel - 1) / bytesPerPixel;
        *pHeight = 1;
    }
}

//----------------------------------------------------------------------------
//! \brief Clear a surface using a GPU rectangle fill (fill with zeros)
//!
//! \param surfId : Surface ID to get the size for
//! \param bWait  : true to wait for the fill to be complete
//!
//! \return OK if surface clear succeeds, not OK otherwise
RC Tsec::ClearSurface(const SurfId surfId, const bool bWait)
{
    MASSERT(surfId < SURF_COUNT);
    RC rc;
    Surface2D *pSurf = &m_TestSurfs[surfId];
    CHECK_RC(m_GpuRectFill.SetSurface(pSurf));
    CHECK_RC(m_GpuRectFill.Fill(0,
                                0,
                                pSurf->GetPitch() / pSurf->GetBytesPerPixel(),
                                pSurf->GetHeight(),
                                0,
                                bWait));
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Allocate all the surfaces
//!
//! \return OK if surface clear succeeds, not OK otherwise
RC Tsec::AllocSurfaces()
{
    RC rc;
    for (UINT32 surfId = 0; surfId < SURF_COUNT; surfId++)
    {
        UINT32 width, height;
        GetSurfaceSize(static_cast<SurfId>(surfId), &width, &height);
        MASSERT(width && height);
        m_TestSurfs[surfId].SetWidth(width);
        m_TestSurfs[surfId].SetHeight(height);
        m_TestSurfs[surfId].SetColorFormat(ColorUtils::VOID32);
        m_TestSurfs[surfId].SetTiled(false);
        m_TestSurfs[surfId].SetLocation(Memory::NonCoherent);

        // Need both GPU/CheetAh so that we can use 2D to clear the surfaces
        m_TestSurfs[surfId].SetVASpace(Surface2D::GPUAndTegraVASpace);
        CHECK_RC(m_TestSurfs[surfId].Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_TestSurfs[surfId].Map());

        // Clear any permanent surface during allocation.  Non-permanent
        // surfaces are cleared prior to every stream
        if (surfId >= FIRST_PERMANENT_SURF)
        {
            CHECK_RC(ClearSurface(static_cast<SurfId>(surfId), false));
        }
    }
    CHECK_RC(m_GpuRectFill.Wait());
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Dump surfaces as a debugging aid.  Note that the same file names are
//!        used regardless of the stream.  This is to keep file names in sync
//!        with the ARCH test file names to aid in comparison for test
//!        correctness validation
//!
//! \param bPreBatch : true if the dump is pre batch processing or post
//!
//! \return OK if surface clear succeeds, not OK otherwise
void Tsec::DumpSurfaces(const bool bPreBatch)
{
    static const char * surfString[] =
    {
        "tsout"
        ,"esmain"
        ,"essub"
        ,"tsin"
        ,"vm"
        ,"flow_ctrl"
        ,"secop_uc"
        ,"key_store"
    };

    if (!m_DumpSurfaces)
        return;

    for (UINT32 surfId = 0; surfId < SURF_COUNT; surfId++)
    {
        bool bDumpSurface = false;
        if ((surfId >= FIRST_INPUT_SURF) == bPreBatch)
            bDumpSurface = true;
        if (surfId == FLOWCTRL_SURF)
            bDumpSurface = true;
        if (surfId == SEM_SURF)
            bDumpSurface = false;

        if (bDumpSurface)
        {
            const char *pSurfName = "undefined";
            if (surfId < (sizeof(surfString) / sizeof(const char *)))
                pSurfName = surfString[surfId];

            string Filename;
            if (bPreBatch)
            {
                Filename = Utility::StrPrintf("surface_%s_%05d.bin",
                                              pSurfName,
                                              m_LoopNum % LOOPS_PER_STREAM);
            }
            else
            {
                Filename = Utility::StrPrintf("out%s_%05d.bin",
                                              pSurfName,
                                              m_LoopNum % LOOPS_PER_STREAM);
            }
            Surface2D *pSurf = &m_TestSurfs[surfId];
            UINT08 *pSurfData =
                            static_cast<UINT08 *>(pSurf->GetAddress());
            FileHolder f(Filename, "wb");
            UINT08 data;

            for (UINT32 i = 0; i < pSurf->GetSize(); i++, pSurfData++)
            {
                data = MEM_RD08(pSurfData);
                fputc(data, f.GetFile());
            }
        }
    }
}

//----------------------------------------------------------------------------
//! \brief : This function only affects PMT packets.  It traverses the PMT 1st
//!          loop and replaces the HDMV copy control descriptor (tag = 0x88)
//!          with BD system use descriptor (tag = 0x89)
//!
//! \param pPacket : Pointer to the packed to check and patch
//!
void Tsec::GenerateFakePatchRecord(UINT08 * pPacket)
{
    UINT32 pid = ((pPacket[5] & 0x1f) << 8) | pPacket[6];
    if (pid == PID_PMT)
    {
        UINT32 descOff;
        UINT32 pgmInfoLen = (UINT32)pPacket[20];
        UINT08* pDescriptor = &pPacket[21];
        for (descOff = 0; descOff < pgmInfoLen;
              descOff += (UINT32)pDescriptor[descOff + 1] + 2)
        {
            if (pDescriptor[descOff] == TAG_HDMV_COPYCTRL)
            {
                pDescriptor[descOff] = TAG_BD_SYSTEMUSE;
                for (UINT32 i = 0; i < (UINT32)pDescriptor[descOff + 1]; i++)
                {
                    pDescriptor[descOff + 2 + i] = i;
                }
                break;
            }
        }
    }
}

//----------------------------------------------------------------------------
//! \brief : Initialize stream processing
//!
//! \param pStream     : Pointer to the stream data to initialize
//! \param pTarFile    : Pointer to tar file for the stream
//! \param pBatchCount : Pointer to returned batch count for the stream
//!
//! \return OK if successful, not OK otherwise
RC Tsec::InitializeStream
(
    const StreamParams * pStream,
    const TarFile *      pTarFile,
    UINT32 *             pBatchCount
)
{
    RC rc;

    for (UINT32 surfId = 0; surfId < FIRST_PERMANENT_SURF; surfId++)
    {
        CHECK_RC(ClearSurface(static_cast<SurfId>(surfId), false));
    }
    CHECK_RC(m_GpuRectFill.Wait());

    UINT32 bsSize = pTarFile->GetSize();
    *pBatchCount = (bsSize + pStream->BatchSize - 1) / pStream->BatchSize;

    memset(&m_FlowControl, 0, sizeof(m_FlowControl));
    m_FlowControl.drv2secop.patch_mode         = pStream->PatchMode;
    if(pStream->PatchMode)
        m_FlowControl.drv2secop.patch_vm_blocks = s_PatchVMSize >> 8;
    m_FlowControl.drv2secop.bitstream_id[0]= pStream->MainCodec * 2;
    m_FlowControl.drv2secop.bitstream_id[1]= pStream->SubCodec * 2 + 1;
    m_FlowControl.drv2secop.tsin_size =
        static_cast<LwU32>(m_TestSurfs[TSIN_SURF].GetSize());
    m_FlowControl.drv2secop.tsout_size =
        static_cast<LwU32>(m_TestSurfs[TSOUT_SURF].GetSize());
    m_FlowControl.drv2secop.es_size[0] =
        static_cast<LwU32>(m_TestSurfs[ESMAIN_SURF].GetSize());
    m_FlowControl.drv2secop.es_size[1] =
        static_cast<LwU32>(m_TestSurfs[ESSUB_SURF].GetSize());
    memcpy(m_FlowControl.secop2drv.ctr_tsout, s_IvCtrTs, sizeof(s_IvCtrTs));
    memcpy(m_FlowControl.secop2drv.ctr_es[0], s_IvCtrEs0, sizeof(s_IvCtrEs0));
    memcpy(m_FlowControl.secop2drv.ctr_es[1], s_IvCtrEs1, sizeof(s_IvCtrEs1));

    AesGenerateVMBuffer(&m_BitstreamVm, pStream->PatchMode);
    Platform::MemCopy(m_TestSurfs[BSVM_SURF].GetAddress(),
                      &m_BitstreamVm,
                      sizeof(TsecBitstreamVm));
    Platform::FlushCpuWriteCombineBuffer();
    return rc;
}

//----------------------------------------------------------------------------
//! \brief : Run one batch of input TS data
//!
//! \param pStream        : Pointer to the stream data to run
//! \param tsInBuffOff    : Last valid position in the TS input buffer that was
//!                         written
//! \param skipPackets    : Number of TS packets to skip at the start of the input
//! \param processPackets : Number of TS packets to process
//! \param bFlushPes      : true if this is a packet end of stream batch
//!
//! \return OK if successful, not OK otherwise
RC Tsec::RunBatch
(
    const StreamParams * pStream,
    const UINT32         tsInBuffOff,
    const UINT32         skipPackets,
    const UINT32         processPackets,
    const bool           bFlushPes
)
{
    StickyRC rc;

    UINT32 tsInSize = static_cast<UINT32>(m_TestSurfs[TSIN_SURF].GetSize());
    secop_flow_ctrl_drv2secop * pDrv2Secop = &m_FlowControl.drv2secop;
    secop_flow_ctrl_secop2drv * pSecop2Drv = &m_FlowControl.secop2drv;
    secop_flow_ctrl_batch_params  * pBatchParams =
        &m_FlowControl.batch_params[pDrv2Secop->drv_batch_index & 0xf];
    secop_batch_param_header* hdr = &pBatchParams->hdr;

    m_FlowControl.drv2secop.tsin_writeptr = tsInBuffOff;

    hdr->video_info[0].codec_type = pStream->MainCodec;
    hdr->video_info[0].enabled    = pStream->MainCodec ? 1 : 0;
    hdr->video_info[0].flush_pes  = bFlushPes ? 1 : 0;
    hdr->video_info[1].codec_type = (pStream->SubCodec * 2);
    hdr->video_info[1].enabled    = (pStream->SubCodec * 2) != 0 ? 1 : 0;
    hdr->video_info[1].flush_pes  = bFlushPes ? 1 : 0;
    hdr->mainaudio_pid            = PID_MAILWIDEO;
    hdr->subvideo_pid             = PID_SUBVIDEO;
    hdr->filter_audio             = 0;
    hdr->mainaudio_pid            = 0;
    hdr->subaudio_pid             = 0;
    hdr->skip_packet_count        = skipPackets;
    hdr->process_packet_count     = processPackets;
    hdr->batch_byte_count         =
        (tsInSize + pDrv2Secop->tsin_writeptr - pSecop2Drv->tsin_readptr) % tsInSize;

    // update patch parameters if patching is required
    if (pStream->PatchMode)
    {
        hdr->patch_param_blocks = 8;
        AesGeneratePatchParamBuffer(pBatchParams->params_patch,
                                    pStream->PatchMode);
    }

    // clear batch status
    memset(&pBatchParams->status, 0, sizeof(secop_batch_status));

    // increment batch index
    pDrv2Secop->drv_batch_index++;

    // for now, always assume that downstream catches up completely
    pDrv2Secop->tsout_readptr  = pSecop2Drv->tsout_writeptr;
    pDrv2Secop->es_readptr[0]  = pSecop2Drv->es_writeptr[0];
    pDrv2Secop->es_readptr[1]  = pSecop2Drv->es_writeptr[1];

    Platform::MemCopy(m_TestSurfs[FLOWCTRL_SURF].GetAddress(),
                      &m_FlowControl,
                      sizeof(m_FlowControl));
    Platform::FlushCpuWriteCombineBuffer();

    DumpSurfaces(true);

    CHECK_RC(m_pCh->Write(0,
                          LW95A1_SET_APPLICATION_ID,
                          LW95A1_SET_APPLICATION_ID_ID_UCODE_LOADER));
    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_SET_UCODE_LOADER_OFFSET,
                                     m_TestSurfs[UCODE_SURF],
                                     0, 8));
    CHECK_RC(m_pCh->Write(0,
       LW95A1_SET_UCODE_LOADER_PARAMS,
       DRF_NUM(95A1, _SET_UCODE_LOADER_PARAMS, _BLOCK_COUNT, m_UcodeSize >> 8) |
       DRF_NUM(95A1, _SET_UCODE_LOADER_PARAMS, _SELWRITY_PARAM, 5)));
    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_SECOP_BITPROCESS_SET_TSIN_OFFSET,
                                     m_TestSurfs[TSIN_SURF],
                                     0, 8));
    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_SECOP_BITPROCESS_SET_TSOUT_OFFSET,
                                     m_TestSurfs[TSOUT_SURF],
                                     0, 8));
    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_SECOP_BITPROCESS_SET_ESMAIN_OFFSET,
                                     m_TestSurfs[ESMAIN_SURF],
                                     0, 8));
    if (pStream->SubCodec)
    {
        CHECK_RC(m_pCh->WriteWithSurface(0,
                                         LW95A1_SECOP_BITPROCESS_SET_ESSUB_OFFSET,
                                         m_TestSurfs[ESSUB_SURF],
                                         0, 8));
    }
    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_SECOP_BITPROCESS_SET_KEYSTORE_OFFSET,
                                     m_TestSurfs[KEY_SURF],
                                     0, 8));
    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_SECOP_BITPROCESS_SET_VM_OFFSET,
                                     m_TestSurfs[BSVM_SURF],
                                     0, 8));
    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_SECOP_BITPROCESS_SET_FLOWCTRL_OFFSET,
                                     m_TestSurfs[FLOWCTRL_SURF],
                                     0, 8));
    CHECK_RC(m_pCh->Write(0, LW95A1_SECOP_BITPROCESS_SET_CTXDMA_INDEX, 0));

    Surface2D *pSemSurf = &m_TestSurfs[SEM_SURF];
    UINT32 exelwteData = 0;
    if (m_UseNotify)
    {
        MEM_WR32(pSemSurf->GetAddress(), 0);
        CHECK_RC(m_pCh->WriteWithSurface(0,
                                         LW95A1_SEMAPHORE_A,
                                         m_TestSurfs[SEM_SURF],
                                         0, 32));
        CHECK_RC(m_pCh->WriteWithSurface(0,
                                         LW95A1_SEMAPHORE_B,
                                         m_TestSurfs[SEM_SURF],
                                         0, 0));
        CHECK_RC(m_pCh->Write(0,
                              LW95A1_SEMAPHORE_C,
                              DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, m_LoopNum + 1)));
        exelwteData = DRF_DEF(95A1, _EXELWTE, _NOTIFY, _ENABLE) |
                      DRF_DEF(95A1, _EXELWTE, _NOTIFY_ON, _END) |
                      DRF_DEF(95A1, _EXELWTE, _AWAKEN, _DISABLE);
    }
    else
    {
        exelwteData = DRF_DEF(95A1, _EXELWTE, _NOTIFY, _DISABLE) |
                      DRF_DEF(95A1, _EXELWTE, _AWAKEN, _ENABLE);
    }
    CHECK_RC(m_pCh->Write(0, LW95A1_EXELWTE, exelwteData));
    CHECK_RC(m_pCh->Flush());

    // Defer returning if the wait fails until after dumping surfaces so that
    // the surfaces are dumped even on failure (useful for debugging)
    if (m_UseNotify)
    {
        NotifyPollArgs ntfyArgs;
        ntfyArgs.Data   = m_LoopNum + 1;
        ntfyArgs.pAddr  = pSemSurf->GetAddress();
        rc = POLLWRAP(NotifierPollFunc, &ntfyArgs, m_pTestConfig->TimeoutMs());
    }
    else
    {
        rc = m_pCh->WaitIdle(m_pTestConfig->TimeoutMs());
    }
    DumpSurfaces(false);

    CHECK_RC(rc);

    Platform::MemCopy(&m_FlowControl,
                      m_TestSurfs[FLOWCTRL_SURF].GetAddress(),
                      sizeof(m_FlowControl));

    return rc;
}

//----------------------------------------------------------------------------
//! \brief : Run one an entire TS stream
//!
//! \param pStream    : Pointer to the stream data to run
//! \param pTarFile   : Pointer to tar file for the stream
//! \param batchCount : Number of batches to run
//! \param bFlushPes  : Flush after packet end stream
//!
//! \return OK if successful, not OK otherwise
RC Tsec::RunStream
(
    const StreamParams * pStream,
    const TarFile *      pTarFile,
    const UINT32         batchCount,
    bool                 bFlushPes
)
{
    RC rc;
    UINT32         tsInBufOff = 0;
    vector<UINT08> tsInput(pStream->BatchSize + 2 * ALIGNED_UNIT_SIZE, 0);
    vector<UINT08> tsEncInput(pStream->BatchSize + 2 * ALIGNED_UNIT_SIZE, 0);
    Tee::Priority pri = GetVerbosePrintPri();

    for (UINT32 lwrBatch = 0; (lwrBatch < batchCount) || bFlushPes;
          lwrBatch++, m_LoopNum++)
    {
        UINT32 skipPackets    = 0;
        UINT32 processPackets = 0;
        if (lwrBatch < batchCount)
        {
            UINT32 batchStart = pStream->BatchSize * lwrBatch;
            UINT32 batchEnd   = batchStart + pStream->BatchSize;
            if (batchEnd >= pTarFile->GetSize())
                batchEnd = pTarFile->GetSize();

            UINT32 alignBatchStart, alignBatchEnd, batchSize;
            alignBatchStart = AlignDown<ALIGNED_UNIT_SIZE>(batchStart);
            alignBatchEnd   = AlignUp<ALIGNED_UNIT_SIZE>(batchEnd);
            batchSize       = alignBatchEnd - alignBatchStart;

            skipPackets    = (batchStart - alignBatchStart) / TS_PACKET_SIZE;
            processPackets = (batchEnd - batchStart) / TS_PACKET_SIZE;

            pTarFile->Seek(alignBatchStart);
            pTarFile->Read(reinterpret_cast<char *>(&tsInput[0]), batchSize);

            for (UINT32 batchOff = 0; batchOff < batchSize;
                  batchOff += ALIGNED_UNIT_SIZE)
            {
                if (pStream->PatchMode == secop_patch_mode_pmt)
                {
                    UINT32 pktOff;
                    for (pktOff = 0; pktOff < ALIGNED_UNIT_SIZE;
                          pktOff += TS_PACKET_SIZE)
                    {
                        GenerateFakePatchRecord(&tsInput[batchOff + pktOff]);
                    }
                }
                AesEncryptAlignedUnit(&tsInput[batchOff],
                                      &tsEncInput[batchOff],
                                      pStream->bPlainCopy);

                UINT08 *pTSSurfAddr =
                    static_cast<UINT08 *>(m_TestSurfs[TSIN_SURF].GetAddress());
                pTSSurfAddr += tsInBufOff;
                Platform::MemCopy(pTSSurfAddr,
                                  &tsEncInput[batchOff],
                                  ALIGNED_UNIT_SIZE);
                tsInBufOff += ALIGNED_UNIT_SIZE;
                if(tsInBufOff >= m_TestSurfs[TSIN_SURF].GetSize())
                    tsInBufOff = 0;
            }
            Platform::FlushCpuWriteCombineBuffer();
        }
        else
        {
            tsInBufOff     = 0;
            skipPackets    = 0;
            processPackets = 0;
            bFlushPes      = false;
        }
        CHECK_RC(RunBatch(pStream, tsInBufOff, skipPackets, processPackets,
                          (lwrBatch == batchCount)));

        Printf(pri, "Stream : %s\n"
                    "  lwrBatch = %d, tsInBufOff = 0x%08x\n"
                    "  m_FlowControl.secop2drv.tsout_writeptr = 0x%08x\n"
                    "  m_FlowControl.secop2drv.es_writeptr[0] = 0x%08x\n"
                    "  m_FlowControl.secop2drv.es_writeptr[1] = 0x%08x\n",
               pStream->BitstreamFile.c_str(),
               lwrBatch,
               tsInBufOff,
               static_cast<UINT32>(m_FlowControl.secop2drv.tsout_writeptr),
               static_cast<UINT32>(m_FlowControl.secop2drv.es_writeptr[0]),
               static_cast<UINT32>(m_FlowControl.secop2drv.es_writeptr[1]));

        if (OK != (rc = m_pGolden->Run()))
        {
            string errStr =
                Utility::StrPrintf(
                               "Golden error %d after on stream %d (%s) after ",
                               INT32(rc),
                               m_LoopNum / LOOPS_PER_STREAM,
                               pStream->BitstreamFile.c_str());
            if (lwrBatch == batchCount)
            {
                errStr += "end of stream flush\n";
            }
            else
            {
                errStr += Utility::StrPrintf("batch %d\n", lwrBatch);
            }
            Printf(Tee::PriHigh, "%s", errStr.c_str());
            break;
        }
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief : Static function for polling on the notifier results of the TSEC
//!
//! \param pvArgs    : Poll args for the notifier
//!
//! \return true if the notifier is the desired data, false otherwise
/* static */ bool Tsec::NotifierPollFunc(void *pvArgs)
{
    NotifyPollArgs *pArgs = static_cast<NotifyPollArgs *>(pvArgs);
    return (MEM_RD32(pArgs->pAddr) == pArgs->Data);
}

//----------------------------------------------------------------------------
// AES Data and defines
//----------------------------------------------------------------------------
#define ROTL(v, s) (((v) << (s)) ^ ((v) >> (32 - (s))))

#define AES_RK          44

#define FAKE_PATCH_COUNT    (8)
#define TS_SYNC_BYTE            (0x47)

#define ROTL_BYTE(a, b, c, d)   \
    a = ROTL(a, 8);\
    b = ROTL(b, 8);\
    c = ROTL(c, 8);\
    d = ROTL(d, 8);

#define ENCRYPT_ROUND(a, b, c, d, e, f, g, h, r)                                                                            \
    e = s_Te0[a & 0xff] ^ s_Te1[b & 0xff] ^ s_Te2[c & 0xff] ^ s_Te3[d & 0xff] ^ rk[r];                ROTL_BYTE(a, b, c, d);\
    f = s_Te0[c & 0xff] ^ s_Te1[d & 0xff] ^ s_Te2[a & 0xff] ^ s_Te3[b & 0xff] ^ ROTL(rk[r + 1], 24); ROTL_BYTE(a, b, c, d);\
    g = s_Te0[a & 0xff] ^ s_Te1[b & 0xff] ^ s_Te2[c & 0xff] ^ s_Te3[d & 0xff] ^ ROTL(rk[r + 2], 16); ROTL_BYTE(a, b, c, d);\
    h = s_Te0[c & 0xff] ^ s_Te1[d & 0xff] ^ s_Te2[a & 0xff] ^ s_Te3[b & 0xff] ^ ROTL(rk[r + 3], 8);

const LwU32 Tsec::s_Kt0[AES_SIZE_UINT32] =
    {0x53525150, 0x57565554, 0x5b5a5958, 0x5f5e5d5c};
// IV values
const UINT32 Tsec::s_IvCtrTs[AES_SIZE_UINT32]  =
    {0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef};
const UINT32 Tsec::s_IvCtrEs0[AES_SIZE_UINT32] =
    {0xbaadf00d, 0xbaadf00d, 0xbaadf00d, 0xbaadf00d};
const UINT32 Tsec::s_IvCtrEs1[AES_SIZE_UINT32] =
    {0xbaadf00d, 0xbaadf00d, 0xbaadf00d, 0xbaadf00d};
const UINT32 Tsec::s_IvCbc[AES_SIZE_UINT32]    =
    {0xddf8a00b, 0xb31fa6fe, 0x569fdfd8, 0x780f056a};  // this is defined by AACS

//----------------------------------------------------------------------------
// SCP debug mode s_Ks is defined as the constant AES_E(2, Secret5_Debug)
const LwU32 Tsec::s_Ks[AES_SIZE_UINT32]  = {0xc0cccc01, 0x947c41b1, 0xd594814f, 0x82f2974b};
// fake keys
const LwU32 Tsec::s_Kr[AES_SIZE_UINT32]  = {0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c};
const LwU32 Tsec::s_Kps[AES_SIZE_UINT32] = {0x13121110, 0x17161514, 0x1b1a1918, 0x1f1e1d1c};
const LwU32 Tsec::s_Kvm[AES_SIZE_UINT32] = {0x23222120, 0x27262524, 0x2b2a2928, 0x2f2e2d2c};
const LwU32 Tsec::s_Khs[AES_SIZE_UINT32] = {0x33323130, 0x37363534, 0x3b3a3938, 0x3f3e3d3c};
const LwU32 Tsec::s_Kc[AES_SIZE_UINT32]  = {0x43424140, 0x47464544, 0x4b4a4948, 0x4f4e4d4c};

//! Decryption virtual machine data for the bitstream
const UINT32 Tsec::s_DecryptiolwM[] = {
    0x0000fc0a, 0x21fc0019, 0x0004fc0a, 0x21fc0419,
    0x0064fc0a, 0x30fc3019, 0x17f0080a, 0x0000fc0a,
    0x00fc0008, 0x00180002, 0x00010b1b, 0x00c0fc0a,
    0xfc10f80d, 0x0000fc0a, 0x00fcf808, 0x00140002,
    0x0114091b, 0x40102016, 0x02320d1b, 0x00150001,
    0x00020c1b, 0x1800fc0a, 0xfc000011, 0x00070001,
    0x00000000, 0xddf8a00b, 0xb31fa6fe, 0x569fdfd8,
    0x780f056a, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

//! Decryption virtual machine data for the patch
const UINT32 Tsec::s_PatchVM[] =
{
    0x0000fc0a, 0x21fc0019, 0x0000040a, 0x04040414,
    0x04040413, 0x0404040f, 0x0404040e, 0x0404040d,
    0x04040412, 0x04040411, 0x04040410, 0x04040414,
    0x04040413, 0x0404040f, 0x0404040e, 0x0404040d,
    0x04040412, 0x04040411, 0x04040410, 0x04040414,
    0x04040413, 0x0404040f, 0x0404040e, 0x0404040d,
    0x04040412, 0x04040411, 0x04040410, 0x04040414,
    0x04040413, 0x0404040f, 0x0404040e, 0x0404040d,
    0x04040412, 0x04040411, 0x04040410, 0x0001fc0a,
    0x00fc0008, 0x003d0003, 0x0000fc0a, 0x02fc0419,
    0x0089fc0a, 0x00fc0408, 0x003c0003, 0x0001fc0a,
    0x02fc0c19, 0x0000080a, 0x000c0808, 0x003c0007,
    0x0002fc0a, 0x08fcf810, 0x02f81019, 0x0002fc0a,
    0xfc10f813, 0x0008fc0a, 0xf8fcf410, 0x21f41419,
    0x00050e1b, 0x0001fc0a, 0xfc080810, 0x002e0001,
    0x004e0001, 0x0002fc0a, 0x00fc0008, 0x004e0003,
    0x0004fc0a, 0x21fc0419, 0x0000080a, 0x00040808,
    0x004e0007, 0x0002fc0a, 0xfc08f813, 0x0008fc0a,
    0xf8fcf410, 0x21f40c19, 0x00030e1b, 0x0001fc0a,
    0xfc080810, 0x00430001, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};
const UINT32 Tsec::s_PatchVMSize = sizeof(Tsec::s_PatchVM);

//----------------------------------------------------------------------------
//
// The Rijndael s-box for encryption
//
const UINT08 Tsec::s_TeS[256] =
{
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
    0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
    0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
    0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
    0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
    0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
    0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
    0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
    0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
    0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
    0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
    0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

//
// hshi: I have swapped all the 32-bit constants (s_Te0,...s_Te3) in this file
//       from big endian to small endian to optimize performance on x86 CPU...
//       All algorithms in the file have been modified to accomodate for that
//       change.  This saves us 8 endian-swaps for every block and about 4%
//       speedup in AES-128 encryption/decryption.
//       Correctness as per the original AES spec is preserved and verified.
//

#define _(a_b_c, d, i)                                                                                                              \
    ((UINT32)(((UINT32)(0x ## a_b_c ## d) << (i * 8)) ^ ((0x ## a_b_c) >> (24 - i * 8))))

#define TABLE_ENCRYPTION(i)                                         \
{                                                                   \
    _(a56363,c6,i), _(847c7c,f8,i), _(997777,ee,i), _(8d7b7b,f6,i), \
    _(0df2f2,ff,i), _(bd6b6b,d6,i), _(b16f6f,de,i), _(54c5c5,91,i), \
    _(503030,60,i), _(030101,02,i), _(a96767,ce,i), _(7d2b2b,56,i), \
    _(19fefe,e7,i), _(62d7d7,b5,i), _(e6abab,4d,i), _(9a7676,ec,i), \
    _(45caca,8f,i), _(9d8282,1f,i), _(40c9c9,89,i), _(877d7d,fa,i), \
    _(15fafa,ef,i), _(eb5959,b2,i), _(c94747,8e,i), _(0bf0f0,fb,i), \
    _(ecadad,41,i), _(67d4d4,b3,i), _(fda2a2,5f,i), _(eaafaf,45,i), \
    _(bf9c9c,23,i), _(f7a4a4,53,i), _(967272,e4,i), _(5bc0c0,9b,i), \
    _(c2b7b7,75,i), _(1cfdfd,e1,i), _(ae9393,3d,i), _(6a2626,4c,i), \
    _(5a3636,6c,i), _(413f3f,7e,i), _(02f7f7,f5,i), _(4fcccc,83,i), \
    _(5c3434,68,i), _(f4a5a5,51,i), _(34e5e5,d1,i), _(08f1f1,f9,i), \
    _(937171,e2,i), _(73d8d8,ab,i), _(533131,62,i), _(3f1515,2a,i), \
    _(0c0404,08,i), _(52c7c7,95,i), _(652323,46,i), _(5ec3c3,9d,i), \
    _(281818,30,i), _(a19696,37,i), _(0f0505,0a,i), _(b59a9a,2f,i), \
    _(090707,0e,i), _(361212,24,i), _(9b8080,1b,i), _(3de2e2,df,i), \
    _(26ebeb,cd,i), _(692727,4e,i), _(cdb2b2,7f,i), _(9f7575,ea,i), \
    _(1b0909,12,i), _(9e8383,1d,i), _(742c2c,58,i), _(2e1a1a,34,i), \
    _(2d1b1b,36,i), _(b26e6e,dc,i), _(ee5a5a,b4,i), _(fba0a0,5b,i), \
    _(f65252,a4,i), _(4d3b3b,76,i), _(61d6d6,b7,i), _(ceb3b3,7d,i), \
    _(7b2929,52,i), _(3ee3e3,dd,i), _(712f2f,5e,i), _(978484,13,i), \
    _(f55353,a6,i), _(68d1d1,b9,i), _(000000,00,i), _(2ceded,c1,i), \
    _(602020,40,i), _(1ffcfc,e3,i), _(c8b1b1,79,i), _(ed5b5b,b6,i), \
    _(be6a6a,d4,i), _(46cbcb,8d,i), _(d9bebe,67,i), _(4b3939,72,i), \
    _(de4a4a,94,i), _(d44c4c,98,i), _(e85858,b0,i), _(4acfcf,85,i), \
    _(6bd0d0,bb,i), _(2aefef,c5,i), _(e5aaaa,4f,i), _(16fbfb,ed,i), \
    _(c54343,86,i), _(d74d4d,9a,i), _(553333,66,i), _(948585,11,i), \
    _(cf4545,8a,i), _(10f9f9,e9,i), _(060202,04,i), _(817f7f,fe,i), \
    _(f05050,a0,i), _(443c3c,78,i), _(ba9f9f,25,i), _(e3a8a8,4b,i), \
    _(f35151,a2,i), _(fea3a3,5d,i), _(c04040,80,i), _(8a8f8f,05,i), \
    _(ad9292,3f,i), _(bc9d9d,21,i), _(483838,70,i), _(04f5f5,f1,i), \
    _(dfbcbc,63,i), _(c1b6b6,77,i), _(75dada,af,i), _(632121,42,i), \
    _(301010,20,i), _(1affff,e5,i), _(0ef3f3,fd,i), _(6dd2d2,bf,i), \
    _(4ccdcd,81,i), _(140c0c,18,i), _(351313,26,i), _(2fecec,c3,i), \
    _(e15f5f,be,i), _(a29797,35,i), _(cc4444,88,i), _(391717,2e,i), \
    _(57c4c4,93,i), _(f2a7a7,55,i), _(827e7e,fc,i), _(473d3d,7a,i), \
    _(ac6464,c8,i), _(e75d5d,ba,i), _(2b1919,32,i), _(957373,e6,i), \
    _(a06060,c0,i), _(988181,19,i), _(d14f4f,9e,i), _(7fdcdc,a3,i), \
    _(662222,44,i), _(7e2a2a,54,i), _(ab9090,3b,i), _(838888,0b,i), \
    _(ca4646,8c,i), _(29eeee,c7,i), _(d3b8b8,6b,i), _(3c1414,28,i), \
    _(79dede,a7,i), _(e25e5e,bc,i), _(1d0b0b,16,i), _(76dbdb,ad,i), \
    _(3be0e0,db,i), _(563232,64,i), _(4e3a3a,74,i), _(1e0a0a,14,i), \
    _(db4949,92,i), _(0a0606,0c,i), _(6c2424,48,i), _(e45c5c,b8,i), \
    _(5dc2c2,9f,i), _(6ed3d3,bd,i), _(efacac,43,i), _(a66262,c4,i), \
    _(a89191,39,i), _(a49595,31,i), _(37e4e4,d3,i), _(8b7979,f2,i), \
    _(32e7e7,d5,i), _(43c8c8,8b,i), _(593737,6e,i), _(b76d6d,da,i), \
    _(8c8d8d,01,i), _(64d5d5,b1,i), _(d24e4e,9c,i), _(e0a9a9,49,i), \
    _(b46c6c,d8,i), _(fa5656,ac,i), _(07f4f4,f3,i), _(25eaea,cf,i), \
    _(af6565,ca,i), _(8e7a7a,f4,i), _(e9aeae,47,i), _(180808,10,i), \
    _(d5baba,6f,i), _(887878,f0,i), _(6f2525,4a,i), _(722e2e,5c,i), \
    _(241c1c,38,i), _(f1a6a6,57,i), _(c7b4b4,73,i), _(51c6c6,97,i), \
    _(23e8e8,cb,i), _(7cdddd,a1,i), _(9c7474,e8,i), _(211f1f,3e,i), \
    _(dd4b4b,96,i), _(dcbdbd,61,i), _(868b8b,0d,i), _(858a8a,0f,i), \
    _(907070,e0,i), _(423e3e,7c,i), _(c4b5b5,71,i), _(aa6666,cc,i), \
    _(d84848,90,i), _(050303,06,i), _(01f6f6,f7,i), _(120e0e,1c,i), \
    _(a36161,c2,i), _(5f3535,6a,i), _(f95757,ae,i), _(d0b9b9,69,i), \
    _(918686,17,i), _(58c1c1,99,i), _(271d1d,3a,i), _(b99e9e,27,i), \
    _(38e1e1,d9,i), _(13f8f8,eb,i), _(b39898,2b,i), _(331111,22,i), \
    _(bb6969,d2,i), _(70d9d9,a9,i), _(898e8e,07,i), _(a79494,33,i), \
    _(b69b9b,2d,i), _(221e1e,3c,i), _(928787,15,i), _(20e9e9,c9,i), \
    _(49cece,87,i), _(ff5555,aa,i), _(782828,50,i), _(7adfdf,a5,i), \
    _(8f8c8c,03,i), _(f8a1a1,59,i), _(808989,09,i), _(170d0d,1a,i), \
    _(dabfbf,65,i), _(31e6e6,d7,i), _(c64242,84,i), _(b86868,d0,i), \
    _(c34141,82,i), _(b09999,29,i), _(772d2d,5a,i), _(110f0f,1e,i), \
    _(cbb0b0,7b,i), _(fc5454,a8,i), _(d6bbbb,6d,i), _(3a1616,2c,i)  \
}

const UINT32 Tsec::s_Te0[256] = TABLE_ENCRYPTION(0);
const UINT32 Tsec::s_Te1[256] = TABLE_ENCRYPTION(1);
const UINT32 Tsec::s_Te2[256] = TABLE_ENCRYPTION(2);
const UINT32 Tsec::s_Te3[256] = TABLE_ENCRYPTION(3);

// for 128-bit blocks, Rijndael never uses more than 10 s_Rcon values
const UINT32 Tsec::s_Rcon[] =
{
    0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010,
    0x00000020, 0x00000040, 0x00000080, 0x0000001B, 0x00000036
};

//----------------------------------------------------------------------------
// AES Functions
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void Tsec::AesGenerateVMBuffer(TsecBitstreamVm* pvmData, UINT32 patchMode)
{
    memset(pvmData, 0, sizeof(TsecBitstreamVm));

    // encrypt and generation mac for the decryption VM
    AesEcbEncrypt(reinterpret_cast<const UINT08*>(s_DecryptiolwM),
                  sizeof(s_DecryptiolwM),
                  s_Kvm,
                  pvmData->vm_decryptor);
    AesDmHashMac(pvmData->vm_decryptor,
                 sizeof(s_DecryptiolwM),
                 s_Kvm,
                 pvmData->hmac_decryptor);
    // encrypt and generation mac for the patching VM
    if (patchMode)
    {
        AesEcbEncrypt(reinterpret_cast<const UINT08*>(s_PatchVM),
                      sizeof(s_PatchVM),
                      s_Kvm,
                      pvmData->vm_patch);
        AesDmHashMac(pvmData->vm_patch,
                     sizeof(s_PatchVM),
                     s_Kvm,
                     pvmData->hmac_patch);
    }
}

//----------------------------------------------------------------------------
void Tsec::AesDmHash
(
    const UINT08* pData,
    const UINT32  size,
    LwU32         hash[AES_SIZE_UINT32]
)
{
    LwU32 rk[AES_RK];
    LwU32 temp[AES_SIZE_UINT32];
    UINT32 i,j;

    memset(hash, 0, AES_SIZE_BYTES);
    for (i = 0; i < size; i += AES_SIZE_BYTES)
    {
        AesRijndaelKeySetupEnc(rk, reinterpret_cast<const LwU32*>(pData + i));
        AesRijndaelEncrypt(rk, hash, temp);
        for (j = 0; j < AES_SIZE_UINT32; j++)
        {
            hash[j] ^= temp[j];
        }
    }
}

//----------------------------------------------------------------------------
void Tsec::AesDmHashMac
(
    const UINT08* pData,
    const UINT32  size,
    const LwU32   key[AES_SIZE_UINT32],
    LwU32         mac[AES_SIZE_UINT32]
)
{
    LwU32 rk[AES_RK];
    LwU32 hash[AES_SIZE_UINT32] = {0};
    AesDmHash(pData, size, hash);
    AesRijndaelKeySetupEnc(rk, key);
    AesRijndaelEncrypt(rk, hash, mac);
}

//----------------------------------------------------------------------------
void Tsec::AesEcbEncrypt
(
    const UINT08 * pData,
    const UINT32   size,
    const LwU32    key[AES_SIZE_UINT32],
    UINT08 *       pCipherText
)
{
    LwU32 rk[AES_RK];
    UINT32 i;
    AesRijndaelKeySetupEnc(rk, key);
    for (i = 0; i < size; i += AES_SIZE_BYTES)
    {
        AesRijndaelEncrypt(rk,
                           reinterpret_cast<const LwU32 *>(pData + i),
                           reinterpret_cast<LwU32 *>(pCipherText + i));
    }
}

//----------------------------------------------------------------------------
// Expand the cipher key into the encryption key schedule.
// hshi: Now we are using direct copy of DWORDs
//       no SWAPing is necessary
//
void Tsec::AesRijndaelKeySetupEnc(LwU32 rk[44], const LwU32 cipherKey[4])
{
    UINT32 i;

    rk[0] = cipherKey[0];
    rk[1] = cipherKey[1];
    rk[2] = cipherKey[2];
    rk[3] = cipherKey[3];

    // fixed at 128-bit key, 10 round AES
    for (i = 0; i < 10; i++)
    {
        rk[4] = rk[0] ^ (s_TeS[(rk[3] >>  8) & 0xff]      ) ^
                        (s_TeS[(rk[3] >> 16) & 0xff] <<  8) ^
                        (s_TeS[(rk[3] >> 24)       ] << 16) ^
                        (s_TeS[(rk[3]      ) & 0xff] << 24) ^
                        s_Rcon[i];
        rk[5] = rk[1] ^ rk[4];
        rk[6] = rk[2] ^ rk[5];
        rk[7] = rk[3] ^ rk[6];
        rk += 4;
    }
}

//----------------------------------------------------------------------------
void Tsec::AesRijndaelEncrypt
(
    const LwU32 rk[44],
    const LwU32 pt[4],
    LwU32       ct[4]
)
{
    UINT32 i;

    UINT32 s0, s1, s2, s3;
    UINT32 t0, t1, t2, t3;

    // round 0
    s0 = pt[0] ^ rk[0];
    s1 = ROTL(pt[1] ^ rk[1], 24);
    s2 = ROTL(pt[2] ^ rk[2], 16);
    s3 = ROTL(pt[3] ^ rk[3], 8);

    // round 1 - 8
    for (i = 0; i < 4; i++, rk += 8)
    {
        ENCRYPT_ROUND(s0, s1, s2, s3, t0, t1, t2, t3, 4);
        ENCRYPT_ROUND(t0, t1, t2, t3, s0, s1, s2, s3, 8);
    }

    // round 9
    ENCRYPT_ROUND(s0, s1, s2, s3, t0, t1, t2, t3, 4);

    // round 10
    ct[0] = s_TeS[t0 & 0xff] ^ (s_TeS[t1 & 0xff] << 8) ^ (s_TeS[t2 & 0xff] << 16) ^ (s_TeS[t3 & 0xff] << 24) ^ rk[8];   ROTL_BYTE(t0, t1, t2, t3);
    ct[1] = s_TeS[t1 & 0xff] ^ (s_TeS[t2 & 0xff] << 8) ^ (s_TeS[t3 & 0xff] << 16) ^ (s_TeS[t0 & 0xff] << 24) ^ rk[9];   ROTL_BYTE(t0, t1, t2, t3);
    ct[2] = s_TeS[t2 & 0xff] ^ (s_TeS[t3 & 0xff] << 8) ^ (s_TeS[t0 & 0xff] << 16) ^ (s_TeS[t1 & 0xff] << 24) ^ rk[10];  ROTL_BYTE(t0, t1, t2, t3);
    ct[3] = s_TeS[t3 & 0xff] ^ (s_TeS[t0 & 0xff] << 8) ^ (s_TeS[t1 & 0xff] << 16) ^ (s_TeS[t2 & 0xff] << 24) ^ rk[11];
}

//----------------------------------------------------------------------------
void Tsec::AesGenerateStaticTestKeyStoreBuffer(TsecKeyStore* pKeyStore)
{
    // fill in static keys for testing purposes only
    memcpy(pKeyStore->wrapped_kr,  s_Kr,  sizeof(s_Kr));
    memcpy(pKeyStore->wrapped_sk,  s_Khs, sizeof(s_Khs));
    memcpy(pKeyStore->wrapped_ck,  s_Kc,  sizeof(s_Kc));
    memcpy(pKeyStore->wrapped_kps, s_Kps, sizeof(s_Kps));
    memcpy(pKeyStore->wrapped_kvm, s_Kvm, sizeof(s_Kvm));
    memset(pKeyStore->wrapped_kt,      0, sizeof(pKeyStore->wrapped_kt));
    memcpy(pKeyStore->wrapped_kt,  s_Kt0, sizeof(s_Kt0));
    memset(pKeyStore->reserved,        0, sizeof(pKeyStore->reserved));
}

//----------------------------------------------------------------------------
void Tsec::AesProtectKeyStoreBuffer(TsecKeyStore* pKeyStore)
{
    LwU32 rk[AES_RK];
    UINT32 i, j;

    // decrement ck by LW95A1_SECOP_BITPROCESS_KEY_INCREMENT
    for (i = 15, j = LW95A1_SECOP_BITPROCESS_KEY_INCREMENT; i >= 8; i--)
    {
        if (((UINT08*)pKeyStore->wrapped_ck)[i] >= j)
        {
            ((UINT08*)pKeyStore->wrapped_ck)[i] -= (UINT08)j;
            break;
        }
        else
        {
            ((UINT08*)pKeyStore->wrapped_ck)[i] -= (UINT08)j;
            j = 1;
        }
    }

    // generate wrapped keys
    AesRijndaelKeySetupEnc(rk, pKeyStore->wrapped_kr);
    AesRijndaelEncrypt(rk, pKeyStore->wrapped_kps, pKeyStore->wrapped_kps);
    AesRijndaelEncrypt(rk, pKeyStore->wrapped_kvm, pKeyStore->wrapped_kvm);
    for (i = 0; i < 8; i++)
    {
        AesRijndaelEncrypt(rk,
                           pKeyStore->wrapped_kt[i],
                           pKeyStore->wrapped_kt[i]);
    }

    AesRijndaelKeySetupEnc(rk, pKeyStore->wrapped_sk);
    AesRijndaelEncrypt(rk, pKeyStore->wrapped_ck, pKeyStore->wrapped_ck);

    AesRijndaelKeySetupEnc(rk, s_Ks);
    AesRijndaelEncrypt(rk, pKeyStore->wrapped_kr, pKeyStore->wrapped_kr);
    AesRijndaelEncrypt(rk, pKeyStore->wrapped_sk, pKeyStore->wrapped_sk);

    // generate mac
    AesDmHashMac(reinterpret_cast<const UINT08 *>(pKeyStore),
                 sizeof(secop_key_store) - AES_SIZE_BYTES,
                 s_Ks,
                 pKeyStore->hmac);
}

//----------------------------------------------------------------------------
//    update tsin offset after this function
void Tsec::AesEncryptAlignedUnit
(
    UINT08 * pTSIn,
    UINT08 * pTSEncOut,
    bool     bPlainCopy
)
{
    UINT32 i, j;
    LwU32 dwRK[AES_RK];
    LwU32 dwTemp[AES_SIZE_UINT32];
    LwU32 dwBlockKey[AES_SIZE_UINT32];
    void *pintTSIn = pTSIn;

    if (bPlainCopy)
    {
        // set up the CopyPermissionIndicator
        pTSIn[0] &= ~0xC0;

        // plain copy the AU
        memcpy(pTSEncOut, pTSIn, ALIGNED_UNIT_SIZE);
        return;
    }

    // set up the CopyPermissionIndicator
    pTSIn[0] |= 0xC0;

    // Copy the first block
    memcpy(pTSEncOut, pTSIn, AES_SIZE_BYTES);

    // Callwlate Block Key from the Title Key (aka CPS Unit Key)
    AesRijndaelKeySetupEnc(dwRK, s_Kt0);
    AesRijndaelEncrypt(dwRK,
                       reinterpret_cast<const LwU32 *>(pTSIn),
                       dwBlockKey);
    for (i = 0; i < AES_SIZE_UINT32; i++)
    {
        dwBlockKey[i] ^= ((UINT32*)(pintTSIn))[i];
    }
    AesRijndaelKeySetupEnc(dwRK, dwBlockKey);

    // Perform AES-CBC encryption on the last 6128 bytes of AU
    memcpy(dwTemp, s_IvCbc, AES_SIZE_BYTES);

    for (i = AES_SIZE_BYTES; i < ALIGNED_UNIT_SIZE; i += AES_SIZE_BYTES)
    {
        LwU32* pDataIn  = (LwU32*)(void*)(pTSIn + i);
        LwU32* pDataOut = (LwU32*)(void*)(pTSEncOut + i);
        for (j = 0; j < AES_SIZE_UINT32; j++)
        {
            dwTemp[j] ^= pDataIn[j];
        }
        AesRijndaelEncrypt(dwRK, dwTemp, pDataOut);
        memcpy(dwTemp, pDataOut, AES_SIZE_BYTES);
    }
}

//----------------------------------------------------------------------------
void Tsec::AesGeneratePatchParamBuffer(UINT08 *pPatchData, UINT32 patchMode)
{
    UINT32 i, PatchParam[32] = {0};
    PatchParam[0] = patchMode;
    PatchParam[1] = FAKE_PATCH_COUNT;
    for (i = 0; i < FAKE_PATCH_COUNT; i++)
    {
        PatchParam[2 + i] = ((TS_SYNC_BYTE << 24) | (TS_PACKET_SIZE * i + 4));
    }
    AesEcbEncrypt(reinterpret_cast<const UINT08 *>(PatchParam),
                  sizeof(PatchParam),
                  s_Kps,
                  pPatchData);
}

