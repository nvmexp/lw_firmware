/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tegratsec.cpp
 * @brief  Implementation of new TSEC test that works on production firmware.
 *
 */

#include "class/cl95a1.h"
#include "class/cle26e.h"
#include "core/include/channel.h"
#include "core/include/golden.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "gputest.h"
#include "gputestc.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "tsec/t18x/tsec_drv.h"

// TODO Discuss with hardware team and decide with testing at different input sizes help ?
// vendor/lwpu/cheetah/multimedia/security/include/tsechdcp.h
#define MTHD_RPLY_BUF_SIZE           256
// vendor/lwpu/cheetah/tests-multimedia/security/tsechdcp/tsechdcp_test.h
#define INPUT_BUFFER_SIZE            256
#define OUTPUT_BUFFER_SIZE           256
#define SCRATCH_SIZE                 2304

//! Default width of the TSEC surfaces that are sized based.  This is helpful
//! since the surface filling routines want a surface with a real size and a
//! surface of width 1M x 1 will throw them off
#define DEFAULT_WIDTH     1024

class TegraTsec;

//----------------------------------------------------------------------------
//! TsecEncryptor
//!
//! Helper class which tests an instance of Tsec engine. As part of the test,
//! engine encrypts the buffer using AES encryption algorithm in ECB mode
class TsecEncryptor
{
public:
    TsecEncryptor(UINT32 channelClassID, TegraTsec * pTest);
    RC Setup();
    RC PrepareForEncryption();
    RC SendCommandsToTsecEngine();
    RC ProcessEncryptedData();
    RC Cleanup();
    RC SetLoop(UINT32 loop);
    const char* GetName() const { return m_Name; }
    UINT32 GetEncryptionCount () const { return m_EncryptionCount; }

private:
    //! Enumeration of the various surfaces used in the test
    enum SurfId
    {
         OUTPUT_SURF     //!< Output buffer
        ,INPUT_SURF      //!< Input buffer for encryption
        ,SCRATCH_SURF    //!< Scratch buffer
        ,AES_ECB_CRYPT_PARAMS_SURF //!< AES ECB Encryption parameters
        ,SURF_COUNT
    };

    RC AllocSurfaces();
    RC PopulateAesEcbCryptParams();
    RC PopulateInputSurface();
    RC GetSurfaceSize(const SurfId surfId, UINT32 *pWidth, UINT32 *pHeight);

    // Private Static methods
    static const char* GetNameFromChannelClass(UINT32 channelClass);

    // Per Tsec instance data/state
    UINT32                m_ChannelClassID;
    Surface2D             m_TestSurfs[SURF_COUNT];
    aes_ecb_crypt_param   m_AesEcbCryptParams = {};
    Channel              *m_pCh = nullptr;
    UINT32                m_Loop = 0;
    UINT32                m_Seed = 0;
    UINT32                m_EncryptionCount = 0;
    const char*           m_Name;

    //! Pointer to test object
    TegraTsec            *m_pTest = nullptr;
};
//----------------------------------------------------------------------------
//! TegraTsec Test.
//!
//! The test passes a buffer and key to Tsec engine and encrypts the buffer
//! using AES encryption algorithm in ECB mode
//!
class TegraTsec : public GpuTest
{
public:
    TegraTsec();
    ~TegraTsec() {}

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;
    GpuTestConfiguration* GetTestConfig() const { return m_pTestConfig; };
    RC CheckOrStoreGoldens(Surface2D& outputBuffer, UINT32 loop);

    SETGET_PROP(EngineSkipMask, UINT32);
    SETGET_PROP(KeepRunning,    bool);

private:
    vector <unique_ptr<TsecEncryptor>> m_TsecEncryptors;
    TsecAlloc                          m_TsecAlloc;
    GpuTestConfiguration              *m_pTestConfig = nullptr;
    Goldelwalues                      *m_pGoldelwalues = nullptr;
    unique_ptr<GpuGoldenSurfaces>      m_pGGSurfs;

    // Test arguments
    UINT32                             m_EngineSkipMask = 0;
    bool                               m_KeepRunning    = false;
};

//----------------------------------------------------------------------------
//! \brief Constructor
//!
TegraTsec::TegraTsec()
{
    SetName("TegraTsec");
    m_pTestConfig = GetTestConfiguration();
    m_pGoldelwalues = GetGoldelwalues();
}

//----------------------------------------------------------------------------
//! \brief Return whether the TegraTsec test is supported
//!
//! \return true if supported, false otherwise
bool TegraTsec::IsSupported()
{
    GpuDevice * pGpudev = GetBoundGpuDevice();

    if (!m_TsecAlloc.IsSupported(pGpudev))
    {
        return false;
    }

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    if (!pSubdev->IsSOC())
    {
        return false;
    }
    // TegraTsec not supported on T210/T214 and Sunstreaker platforms yet
    if (pSubdev->HasBug(2024122))
    {
        Printf(Tee::PriNormal, "TegraTsec not supported on T210/T214 and sunstreaker platforms yet\n");
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------
//! \brief Setup the TegraTsec test
//!
//! \return OK if setup succeeds, not OK otherwise
RC TegraTsec::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    m_pGGSurfs = make_unique<GpuGoldenSurfaces>(GetBoundGpuDevice());

    // Cheat GpuGoldenSurfaces and set up a surface, the pointers
    // will dangle, but we will be calling ReplaceSurface() anyway.
    Surface2D fakeOPSurf;
    fakeOPSurf.SetWidth(1);
    fakeOPSurf.SetHeight(1);
    fakeOPSurf.SetColorFormat(ColorUtils::VOID32);
    fakeOPSurf.SetLocation(Memory::Coherent);
    fakeOPSurf.SetVASpace(Surface2D::TegraVASpace);
    m_pGGSurfs->AttachSurface(&fakeOPSurf, "OUTPUT_SURF", 0);
    CHECK_RC(m_pGoldelwalues->SetSurfaces(m_pGGSurfs.get()));

    vector<UINT32> channelClasses =
        CheetAh::SocPtr()->GetChannelClassList(m_TsecAlloc.GetSupportedClass(GetBoundGpuDevice()));

    if (channelClasses.empty())
    {
        Printf(Tee::PriError, "%s : No TSEC channel classes found\n", GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (m_EngineSkipMask)
    {
       UINT32 index = 0;
       vector<UINT32> selectedChannelClasses;
       for (auto channelClass : channelClasses)
       {
           if (!(m_EngineSkipMask & (1 << index)))
           {
               selectedChannelClasses.push_back(channelClass);
           }
           index++;
       }
       if (selectedChannelClasses.empty())
       {
           Printf(Tee::PriError, "%s : Invalid EngineSkipMask. All Tsec instances"
                                 " have been skipped\n", GetName().c_str());
           return RC::ILWALID_TEST_INPUT;
       }
       channelClasses = selectedChannelClasses;
    }

    // Only generate goldens on the first instance
    if (m_pGoldelwalues->GetAction() == Goldelwalues::Store)
        channelClasses.resize(1);

    for (const auto chClass : channelClasses)
    {
        auto pTsecEncryptor = make_unique<TsecEncryptor>(
                              chClass,
                              this);
        CHECK_RC(pTsecEncryptor->Setup());
        m_TsecEncryptors.push_back(std::move(pTsecEncryptor));
    }

    return OK;
}

//TODO Phase 2 and 3 changes in https://confluence.lwpu.com/display/~ppalaniramam/Tsec2+Test
//----------------------------------------------------------------------------
//! \brief Run the TegraTsec test
//!
//! \return OK if running succeeds, not OK otherwise
RC TegraTsec::Run()
{
    StickyRC rc;
    const UINT32 startLoop = m_pTestConfig->StartLoop();
    const UINT32 endLoop = m_pTestConfig->StartLoop() + m_pTestConfig->Loops();

    const bool isBackground = m_KeepRunning;

    for (UINT32 loop = startLoop; (loop < endLoop && ! isBackground) || m_KeepRunning; loop++)
    {
        if (loop == endLoop)
        {
            loop = startLoop;
        }

        // This awkward split is because lwrrently we use same thread to test both Tsec instances
        // To achieve certain degree of overlap in exelwtion, we first prepare for encryption and
        // execute flush in quick succession to send commands to both engines.
        // TODO Create separate threads to test different instances
        for (auto& encryptor: m_TsecEncryptors)
        {
            CHECK_RC(encryptor->SetLoop(loop)); // Needed for Golden CRCs identification
            CHECK_RC(encryptor->PrepareForEncryption());
        }
        for (auto& encryptor: m_TsecEncryptors)
        {
            CHECK_RC(encryptor->SendCommandsToTsecEngine());
        }
        for (auto& encryptor: m_TsecEncryptors)
        {
            CHECK_RC(encryptor->ProcessEncryptedData());
        }
    }

    // Golden errors that are deferred due to "-run_on_error" can only be
    // retrieved by running Golden::ErrorRateTest().  This must be done
    // before clearing surfaces
    CHECK_RC(m_pGoldelwalues->ErrorRateTest(GetJSObject()));

    for (auto& encryptor: m_TsecEncryptors)
    {
        if (encryptor->GetEncryptionCount() == 0)
        {
            Printf(Tee::PriError, "%s not tested\n", encryptor->GetName());
            return RC::NO_TESTS_RUN;
        }
        VerbosePrintf("Tested %s\n", encryptor->GetName());
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Cleanup the TegraTsec test
//!
//! \return OK if cleanup succeeds, not OK otherwise
RC TegraTsec::Cleanup()
{
    StickyRC rc;

    m_TsecAlloc.Free();

    for (auto& encryptor: m_TsecEncryptors)
    {
        rc = encryptor->Cleanup();
    }
    m_TsecEncryptors.clear();

    // Free golden buffer.
    if (m_pGGSurfs)
    {
        m_pGoldelwalues->ClearSurfaces();
        m_pGGSurfs.reset();
    }

    rc = GpuTest::Cleanup();
    return rc;
}

RC TegraTsec::CheckOrStoreGoldens(Surface2D& outputBuffer, UINT32 loop)
{
    RC rc;
    // TODO Need to add mutex when we add support for multiple threads
    m_pGGSurfs->ReplaceSurface(0, &outputBuffer);
    m_pGoldelwalues->SetLoop(loop);
    CHECK_RC(m_pGoldelwalues->Run());
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \return OK if successful, not OK otherwise
void TegraTsec::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tEngineSkipMask:\t\t\t0x%08x\n", m_EngineSkipMask);
    Printf(pri, "\tKeepRunning:\t\t\t%s\n", m_KeepRunning ? "true" : "false");
    GpuTest::PrintJsProperties(pri);
}

// *******************************************************************
// TsecEncryptor Methods
// *******************************************************************
//----------------------------------------------------------------------------
//! \brief TsecEncryptor class constructor
//!
//! \param channelClassID : Channel class ID
//! \param pTest          : Pointer to test object (used to make call backs)
//!
TsecEncryptor::TsecEncryptor
(
    UINT32           channelClassID,
    TegraTsec       *pTest
):
    m_ChannelClassID(channelClassID),
    m_Name(GetNameFromChannelClass(channelClassID)),
    m_pTest(pTest)
{
}

//----------------------------------------------------------------------------
//! \brief Allocate all the surfaces
//!
//! \return OK if surface allocation succeeds, not OK otherwise
RC TsecEncryptor::AllocSurfaces()
{
    RC rc;
    for (UINT32 surfId = 0; surfId < SURF_COUNT; surfId++)
    {
        UINT32 width, height;
        GetSurfaceSize(static_cast<SurfId>(surfId), &width, &height);
        MASSERT(width && height);
        Surface2D& surface = m_TestSurfs[surfId];
        surface.SetWidth(width);
        surface.SetHeight(height);
        surface.SetColorFormat(ColorUtils::VOID32);
        surface.SetLocation(Memory::Coherent);
        surface.SetVASpace(Surface2D::TegraVASpace);

        CHECK_RC(surface.Alloc(m_pTest->GetBoundGpuDevice()));
        CHECK_RC(surface.Map());
        CHECK_RC(surface.Fill(0));
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Allocates resources necessary to perform encryption
//!
//! \return OK if cleanup succeeds, not OK otherwise
RC TsecEncryptor::Setup()
{
    RC rc;
    LwRm::Handle hCh;
    GpuTestConfiguration *pTestCfg = m_pTest->GetTestConfig();

    // Allocate surfaces
    CHECK_RC(AllocSurfaces());

    // Allocate channel
    CHECK_RC(pTestCfg->AllocateChannel(&m_pCh, &hCh));
    CHECK_RC(m_pCh->SetClass(0, m_ChannelClassID));

    m_Seed = pTestCfg->Seed();
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Cleanup resources allocated by TsecEncryptor
//!
//! \return OK if cleanup succeeds, not OK otherwise
RC TsecEncryptor::Cleanup()
{
    StickyRC rc;
    for (UINT32 surfId = 0; surfId < SURF_COUNT; surfId++)
    {
        m_TestSurfs[surfId].Free();
    }

    if (m_pCh != nullptr)
    {
        rc = m_pTest->GetTestConfig()->FreeChannel(m_pCh);
        m_pCh = nullptr;
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Populate AES ECB crypto parameters
//!
//! \return OK if succeeds, not OK otherwise
RC TsecEncryptor::PopulateAesEcbCryptParams()
{
    RC rc;
    UINT08 key[LW95A1_AES_ECB_KEY_SIZE_BYTES];
    /* Generate a random key */
    CHECK_RC(Memory::FillRandom(
                    &key[0],
                    m_Seed,
                    LW95A1_AES_ECB_KEY_SIZE_BYTES));

    m_AesEcbCryptParams.size = INPUT_BUFFER_SIZE;
    m_AesEcbCryptParams.retCode = LW95A1_AES_ECB_CRYPT_ERROR_NONE;
    m_AesEcbCryptParams.feedback = 0;
    m_AesEcbCryptParams.ctr = 1;

    Platform::MemCopy(&m_AesEcbCryptParams.key,
                      &key,
                      LW95A1_AES_ECB_KEY_SIZE_BYTES);

    SurfaceUtils::MappingSurfaceWriter surfaceWriter(m_TestSurfs[AES_ECB_CRYPT_PARAMS_SURF]);
    CHECK_RC(surfaceWriter.WriteBytes(0, &m_AesEcbCryptParams, sizeof(m_AesEcbCryptParams)));
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Populate input surface with data
//!
//! \return OK if setup succeeds, not OK otherwise
RC TsecEncryptor::PopulateInputSurface()
{
    RC rc;
    const UINT64 inputSurfSize = m_TestSurfs[INPUT_SURF].GetSize();
    vector<UINT08> data(inputSurfSize);
    CHECK_RC(Memory::FillRandom(&data[0],
                                m_Seed,
                                inputSurfSize));
    SurfaceUtils::MappingSurfaceWriter surfaceWriter(m_TestSurfs[INPUT_SURF]);
    CHECK_RC(surfaceWriter.WriteBytes(0, &data[0], inputSurfSize));
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Prepares various surfaces needed for encryption
//!
//! \return OK if setup succeeds, not OK otherwise
RC TsecEncryptor::PrepareForEncryption()
{
    RC rc;

    CHECK_RC(PopulateAesEcbCryptParams());
    CHECK_RC(PopulateInputSurface());

    // Encrypt Buffer using AES-ECB 128 bit encryption
    CHECK_RC(m_pCh->Write(0,
                          LW95A1_SET_APPLICATION_ID,
                          LW95A1_SET_APPLICATION_ID_ID_HDCP));

    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_HDCP_SET_SCRATCH_BUFFER,
                                     m_TestSurfs[SCRATCH_SURF],
                                     0, 8));

    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_HDCP_SET_ENC_INPUT_BUFFER,
                                     m_TestSurfs[INPUT_SURF],
                                     0, 8));

    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_HDCP_SET_ENC_OUTPUT_BUFFER,
                                     m_TestSurfs[OUTPUT_SURF],
                                     0, 8));

    CHECK_RC(m_pCh->WriteWithSurface(0,
                                     LW95A1_HDCP_AES_ECB_CRYPT,
                                     m_TestSurfs[AES_ECB_CRYPT_PARAMS_SURF],
                                     0, 8));
    CHECK_RC(m_pCh->Write(0,
                          LW95A1_EXELWTE,
                          0x100));

    return OK;
}

//----------------------------------------------------------------------------
//! \brief Send commands to Tsec engine.
//! Procedure sends commands to Tsec engine but does not wait for Tsec to complete it
//!
//! \return OK if setup succeeds, not OK otherwise
RC TsecEncryptor::SendCommandsToTsecEngine()
{
    RC rc;
    CHECK_RC(m_pCh->Flush());
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Process encrypted data
//!
//! \return OK if setup succeeds, not OK otherwise
RC TsecEncryptor::ProcessEncryptedData()
{
    RC rc;

    CHECK_RC(m_pCh->WaitIdle(m_pTest->GetTestConfig()->TimeoutMs()));

    SurfaceUtils::MappingSurfaceReader surfaceReader(m_TestSurfs[AES_ECB_CRYPT_PARAMS_SURF]);
    CHECK_RC(surfaceReader.ReadBytes(0, &m_AesEcbCryptParams, sizeof(m_AesEcbCryptParams)));

    if (m_AesEcbCryptParams.retCode != LW95A1_AES_ECB_CRYPT_ERROR_NONE)
    {
        Printf(Tee::PriError, "Tsec Encryption failed with status 0x%x\n",
                                           m_AesEcbCryptParams.retCode);
        return RC::ENCRYPT_DECRYPT_FAILED;
    }

    Printf(Tee::PriDebug, "Encrypted data on %s for loop %u\n",
                          GetName(), m_Loop);

    CHECK_RC(m_pTest->CheckOrStoreGoldens(m_TestSurfs[OUTPUT_SURF], m_Loop));
    m_EncryptionCount++;
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Return the required surface size for a TSEC surface
//!
//! \param surfId  : Surface ID to get the size for
//! \param pWidth  : Returned width of the surface
//! \param pHeight : Returned height of the surface
//!
//! \return OK if setup succeeds, not OK otherwise
RC TsecEncryptor::GetSurfaceSize(const SurfId surfId, UINT32 *pWidth, UINT32 *pHeight)
{
    UINT32 bytesPerPixel = ColorUtils::PixelBytes(ColorUtils::VOID32);
    UINT32 defaultPitch = DEFAULT_WIDTH * bytesPerPixel;
    UINT32 size = 0;

    *pWidth = 0;
    *pHeight = 0;

    switch (surfId)
    {
        case OUTPUT_SURF:
            size = OUTPUT_BUFFER_SIZE;
            break;
        case INPUT_SURF:
            size = INPUT_BUFFER_SIZE;
            break;
        case SCRATCH_SURF:
            size = SCRATCH_SIZE;
            break;
        case AES_ECB_CRYPT_PARAMS_SURF:
            size = MTHD_RPLY_BUF_SIZE;
            break;
        default:
            Printf(Tee::PriError, "Unknown surface ID");
            return RC::SOFTWARE_ERROR;
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
    return OK;
}

RC TsecEncryptor::SetLoop(UINT32 loop)
{
    m_Loop = loop;
     // Since loop index got updated, update random seed as well
    m_Seed = m_pTest->GetTestConfig()->Seed() + loop;
    return OK;
}

/* static */ const char* TsecEncryptor::GetNameFromChannelClass(UINT32 channelClass)
{
    switch (channelClass)
    {
        case LWE26E_CMD_SETCL_CLASSID_TSEC:  return "TSEC instance A";
        case LWE26E_CMD_SETCL_CLASSID_TSECB: return "TSEC instance B";
        default: MASSERT(!"Unrecognized channel class");
    }
    return "";
}
//----------------------------------------------------------------------------
// Tsec script interface.
JS_CLASS_INHERIT(TegraTsec, GpuTest, "TegraTsec test");

CLASS_PROP_READWRITE(TegraTsec, EngineSkipMask, UINT32,
    "Bits representing the engines which will be skipped in TegraTsec test."
    "The high bits in the input represent the engines on which the test will be"
    "skipped. (default = 0)");
CLASS_PROP_READWRITE(TegraTsec, KeepRunning, bool,
    "Continue running the test until the flag is reset to false. (default=false)");
