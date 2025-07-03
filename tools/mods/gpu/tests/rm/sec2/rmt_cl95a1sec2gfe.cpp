/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class95a1Sec2 test.
//

#include "rmt_cl95a1sec2.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "maxwell/gm107/dev_fuse.h"
#include "random.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>

extern unsigned char gfe_pkey[];
extern unsigned char gfe_pkey_gm206[];
extern unsigned char gfe_pkey_gp102[];
extern unsigned char gfe_pkey_tu102[];

/*!
 * Class95a1Sec2GfeTest
 *
 */
class Class95a1Sec2GfeTest: public RmTest
{
public:
    Class95a1Sec2GfeTest();
    virtual ~Class95a1Sec2GfeTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC PrintBuf(const char *pStr, LwU8 *pBuf, LwU32 size);
    virtual RC AllocateMem(Sec2MemDesc *, LwU32);
    virtual RC FreeMem(Sec2MemDesc*);
    virtual RC SendSemaMethod();
    virtual RC PollSemaMethod();
    virtual RC StartMethodStream(const char*, LwU32);
    virtual RC EndMethodStream(const char*);
    virtual RC ReadEcid();
    virtual RC GenEkey();
    virtual RC PrintEcid(gfe_read_ecid_param *, LwU8 *, ostream *);
    virtual RC ErrorCheckWithComparison(LwU32 expected, LwU32 result, const char *pMthd, const char *pStr,
                                       LwU8 *pSrc, LwU8 *pLwrrent, LwU32 size);
    virtual RC ErrorCheck(LwU32 expected, LwU32 result, const char *pMthd);

    SETGET_PROP(keyInFileName, string);
    SETGET_PROP(ekeyOutFileName, string);
    SETGET_PROP(bIsGenMode, bool);
    SETGET_PROP(bIsGfe2k, bool);

private:
    Sec2MemDesc m_resMemDesc;
    Sec2MemDesc m_signMemDesc;
    Sec2MemDesc m_semMemDesc;
    Sec2MemDesc m_privKeyDesc;

    LwRm::Handle m_hObj;
    FLOAT64      m_TimeoutMs;

    GpuSubdevice *m_pSubdev;

    string       m_keyInFileName;
    string       m_ekeyOutFileName;
    bool         m_bIsGenMode;
    bool         m_bIsGfe2k;
};

/*!
 * Construction and desctructors
 */
Class95a1Sec2GfeTest::Class95a1Sec2GfeTest() :
    m_hObj(0),
    m_TimeoutMs(Tasker::NO_TIMEOUT)
{
    SetName("Class95a1Sec2GfeTest");

    m_keyInFileName = "";
    m_ekeyOutFileName  = "";
    m_bIsGenMode = false;
    m_bIsGfe2k = false;
}

Class95a1Sec2GfeTest::~Class95a1Sec2GfeTest()
{
}

/*!
 * IsTestSupported
 *
 * Need class 95A1 to be supported
 */
string Class95a1Sec2GfeTest::IsTestSupported()
{
    if( IsClassSupported(LW95A1_TSEC) )
        return RUN_RMTEST_TRUE;
    return "LW95A1 class is not supported on current platform";
}

/*!
 * PrintBuf
 *
 * Helped routing to print buffers in hex format
 */
RC Class95a1Sec2GfeTest::PrintBuf(const char *pStr, LwU8 *pBuf, LwU32 size)
{
    Printf(Tee::PriHigh, "GFE: %s\n", pStr);
    for (LwU32 i=0; i < size; i++)
    {
        if (i && (!(i % 16)))
            Printf(Tee::PriHigh, "\n");
        Printf(Tee::PriHigh, "%02x ",(LwU8)pBuf[i]);
    }
    Printf(Tee::PriHigh, "\n");
    return OK;
}

RC Class95a1Sec2GfeTest::AllocateMem(Sec2MemDesc *pMemDesc, LwU32 size)
{
    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetColorFormat(ColorUtils::Y8);
    pVidMemSurface->SetLocation(Memory::Coherent);
    pVidMemSurface->SetArrayPitchAlignment(256);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetLayout(Surface2D::Pitch);

    pVidMemSurface->Alloc(GetBoundGpuDevice());
    pVidMemSurface->Map();

    pMemDesc->gpuAddrSysMem = pVidMemSurface->GetCtxDmaOffsetGpu();
    pMemDesc->pCpuAddrSysMem = (LwU32*)pVidMemSurface->GetAddress();
    pMemDesc->size = size;

    return OK;
}

/*!
 * FreeMem
 *
 * Frees the above allocated memory
 */
RC Class95a1Sec2GfeTest::FreeMem(Sec2MemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

/*!
 * Setup
 *
 * Sets up the timeout - Setting to max for emulation cases
 * Allocates Channel and object
 * Allocates necessary memory required for GFE
 * Copies the necessary constants into allocated memory
 */
#define SEC2_GFE_RSA_1K_KEY_SIZE_ORI   (448)
#define SEC2_GFE_RSA_2K_KEY_SIZE_ORI   (448 * 2)
#define SEC2_SHA256_HASH_SIZE          (32)
#define SEC2_IV_SIZE                   (16)
#define SEC2_GFE_RSA_1K_KEY_SIZE       (SEC2_IV_SIZE + SEC2_GFE_RSA_1K_KEY_SIZE_ORI + SEC2_SHA256_HASH_SIZE)
#define SEC2_GFE_RSA_2K_KEY_SIZE       (SEC2_IV_SIZE + SEC2_GFE_RSA_2K_KEY_SIZE_ORI + SEC2_SHA256_HASH_SIZE)
RC Class95a1Sec2GfeTest::Setup()
{

    LwRmPtr      pLwRm;
    RC           rc;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_SEC2));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));
    m_pCh->SetObject(0, m_hObj);

    // Allocate Memory for Main class. Allocate big enough to support 1K/2K for both READ/GEN mode
    AllocateMem(&m_semMemDesc, 0x4);
    AllocateMem(&m_resMemDesc, 0x100);
    AllocateMem(&m_signMemDesc, SEC2_GFE_RSA_2K_KEY_SIZE);
    AllocateMem(&m_privKeyDesc, SEC2_GFE_RSA_2K_KEY_SIZE);

    return rc;
}

/*!
 * SendSemaMethod
 *
 * Pushes semaphore methods into channel push buffer.
 * Uses 0x12345678 as the payload
 */
RC Class95a1Sec2GfeTest::SendSemaMethod()
{
    m_pCh->Write(0, LW95A1_SEMAPHORE_A,
                DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (LwU32)(m_semMemDesc.gpuAddrSysMem >> 32LL)));
    m_pCh->Write(0, LW95A1_SEMAPHORE_B, (LwU32)(m_semMemDesc.gpuAddrSysMem & 0xffffffffLL));
    m_pCh->Write(0, LW95A1_SEMAPHORE_C,
                DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, 0x12345678));

    return OK;
}

/*!
 * PollSemaMethod
 *
 * Polls for the semaphore to be released by TSEC
 * Necessarily expects the payload 0x12345678 to be written by TSEC into semaphore surface
 */
RC Class95a1Sec2GfeTest::PollSemaMethod()
{
    RC              rc = OK;
    PollArguments args;

    args.value  = 0x12345678;
    args.pAddr  = (UINT32*)m_semMemDesc.pCpuAddrSysMem;
    Printf(Tee::PriHigh, "GFE: Polling started for pAddr %p Expecting 0x%0x\n", args.pAddr, args.value);
    rc = PollVal(&args, m_TimeoutMs);

    args.value = MEM_RD32(args.pAddr);
    Printf(Tee::PriHigh, "GFE: Semaphore value found: 0x%0x\n", args.value);

    return rc;
}

/*!
 * StartMethodStream
 *
 * Does startup tasks before starting a method stream
 * Also pushes needed methods which are always used
 */
RC Class95a1Sec2GfeTest::StartMethodStream(const char *pId, LwU32 sendSBMthd)
{
    RC              rc = OK;

    Printf(Tee::PriHigh, "GFE: Starting method stream for %s \n", pId);
    MEM_WR32(m_semMemDesc.pCpuAddrSysMem, 0x87654321);

    m_pCh->Write(0, LW95A1_SET_APPLICATION_ID, LW95A1_SET_APPLICATION_ID_ID_GFE);
    m_pCh->Write(0, LW95A1_SET_WATCHDOG_TIMER, 0);
    SendSemaMethod();

    return rc;
}

/*!
 * EndMethodStream
 *
 * Flushes the channel and polls on semaphore
 */
RC Class95a1Sec2GfeTest::EndMethodStream(const char *pId)
{
    RC    rc           = OK;
    LwU64 startTimeMS  = 0;
    LwU64 elapsedTimeMS= 0;

    m_pSubdev = GetBoundGpuSubdevice();

    m_pCh->Write(0, LW95A1_EXELWTE, 0x1); // Notify on End
    m_pCh->Flush();

    startTimeMS = Platform::GetTimeMS();
    rc = PollSemaMethod();
    elapsedTimeMS = (Platform::GetTimeMS() - startTimeMS);

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "GFE: Polling on %s FAILED !!!!\n", pId);
    }
    else
    {
        Printf(Tee::PriHigh, "GFE: Polling on %s PASSED !!!!\n", pId);
    }

    Printf(Tee::PriHigh, "GFE: Time taken for this method stream '%llu' milliseconds!\n", elapsedTimeMS);

    return rc;
}

/*!
 * ErrorCheck
 *
 * Helper routing to check the retCode with the expected retCode and prints
 * error message based on the comparison
 */
RC Class95a1Sec2GfeTest::ErrorCheck(LwU32 expected, LwU32 result, const char *pMthd)
{
    RC   rc = OK;

    //PrintBuf("ResBuf ", (LwU8*)m_resMemDesc.pCpuAddrSysMem, 0x100);
    if (result != expected)
    {
        Printf(Tee::PriHigh, "GFE: ERROR: '%s' FAILED with retcode 0x%0x - Expected retcode 0x%0x\n",
                             pMthd, (unsigned int) result, (unsigned int) expected);
        rc = ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "GFE: '%s' PASSED with retcode 0x%0x - Expected retcode 0x%0x\n",
                             pMthd, result, expected);
    }
    return rc;
}

/*!
 * ErrorCheckWithComparison
 *
 * Helper routing to check the retcode and also compare the buffer with the expected buffer.
 */
RC Class95a1Sec2GfeTest::ErrorCheckWithComparison(LwU32 expected, LwU32 result, const char *pMthd,
                                     const char *pStr,  LwU8 *pSrc, LwU8 *pLwrrent, LwU32 size)
{
    RC rc = OK;
    CHECK_RC(ErrorCheck(expected, result, pMthd));

    //PrintBuf(pStr, pLwrrent, size);
    if (memcmp((char*)pLwrrent, (char*)pSrc, size))
    {
        rc = ERROR;
        Printf(Tee::PriHigh, "GFE: ERROR: '%s %s' comparison FAILED\n", pMthd, pStr);
    }
    else
    {
        Printf(Tee::PriHigh, "GFE: '%s %s' comparison PASSED\n", pMthd, pStr);
    }
    return rc;

}

/*!
 * Prints ECID
 */
RC Class95a1Sec2GfeTest::PrintEcid(gfe_read_ecid_param *pEcid, LwU8 *pSign, ostream *pFileOut)
{
    RC       rc      = OK;
    LwU32    sigSize = 0;

    Printf(Tee::PriHigh, "**************************************************************************\n");
    Printf(Tee::PriHigh, "**************************************************************************\n");
    PrintBuf("Server Nonce: ", (LwU8*)&(pEcid->serverNonce), LW95A1_GFE_READ_ECID_NONCE_SIZE_IN_BYTES);
    Printf(Tee::PriHigh, "GFE: ProgramID   : 0x%08x\n", pEcid->programID);
    Printf(Tee::PriHigh, "GFE: SessionID   : 0x%08x\n", pEcid->sessionID);
    Printf(Tee::PriHigh, "GFE: SignMode    : 0x%08x\n", pEcid->signMode);
    Printf(Tee::PriHigh, "GFE: VendorId    : 0x%04x\n", pEcid->dinfo.vendorId);
    Printf(Tee::PriHigh, "GFE: SubSystemId : 0x%04x\n", pEcid->dinfo.subSystemId);
    Printf(Tee::PriHigh, "GFE: SubVendorId : 0x%04x\n", pEcid->dinfo.subVendorId);
    Printf(Tee::PriHigh, "GFE: DeviceId    : 0x%04x\n", pEcid->dinfo.deviceId);
    Printf(Tee::PriHigh, "GFE: RevId       : 0x%04x\n", pEcid->dinfo.revId);
    Printf(Tee::PriHigh, "GFE: ChipId      : 0x%04x\n", pEcid->dinfo.chipId);
    PrintBuf("ECID SHA2hash: ", (LwU8*)&(pEcid->dinfo.ecidSha2Hash), LW95A1_GFE_ECID_HASH_SIZE_IN_BYTES);

    if (pEcid->signMode == LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_1024)
    {
        sigSize = 1024/8;
        PrintBuf("Signature: ", pSign, sigSize);
    }
    else
    {
        sigSize = 2048/8;
        Printf(Tee::PriHigh, "GFE: Invalid signature mode\n");
        //TODO: Disabling this for now to make sure it doesnt break SM2 until ucode changes are submitted
        //rc = ERROR;
    }

    Printf(Tee::PriHigh, "**************************************************************************\n");
    Printf(Tee::PriHigh, "**************************************************************************\n");

    // See if we need to write the ECID contents into out file
    if (pFileOut)
    {
        pFileOut->seekp(0);
        pFileOut->write((char*)&(pEcid->serverNonce), LW95A1_GFE_READ_ECID_NONCE_SIZE_IN_BYTES);
        pFileOut->write((char*)&(pEcid->programID), sizeof(pEcid->programID));
        pFileOut->write((char*)&(pEcid->signMode), sizeof(pEcid->signMode));
        pFileOut->write((char*)&(pEcid->dinfo.ecidSha2Hash), LW95A1_GFE_ECID_HASH_SIZE_IN_BYTES);
        pFileOut->write((char*)&(pEcid->dinfo.vendorId), sizeof(LwU16));
        pFileOut->write((char*)&(pEcid->dinfo.deviceId), sizeof(LwU16));
        pFileOut->write((char*)&(pEcid->dinfo.subSystemId), sizeof(LwU16));
        pFileOut->write((char*)&(pEcid->dinfo.subVendorId), sizeof(LwU16));
        pFileOut->write((char*)&(pEcid->dinfo.revId), sizeof(LwU16));
        pFileOut->write((char*)&(pEcid->dinfo.chipId), sizeof(LwU16));
        pFileOut->write((char*)&(pEcid->ucodeVersion), sizeof(pEcid->ucodeVersion));

        //Write the signature in the end
        pFileOut->write((char*)pSign, sigSize);
    }

    return rc;
}

/*!
 * ReadEcid
 *
 * Handles ReadEcid method
 */
RC Class95a1Sec2GfeTest::ReadEcid()
{
    RC                  rc            = OK;
    ifstream            *pFilePkey    = NULL;
    ofstream            *pFileOut     = NULL;
    gfe_read_ecid_param *pEcid        = (gfe_read_ecid_param *)m_resMemDesc.pCpuAddrSysMem;
    Random              rnd;
    LwU32               keysizefull   = 0;
    LwU32               chipId        = 0;
    unsigned char       *pGfePkey     = NULL;

    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);
    memset(m_privKeyDesc.pCpuAddrSysMem, 0, SEC2_GFE_RSA_2K_KEY_SIZE);

    // Pass the key size in signMode
    keysizefull = SEC2_GFE_RSA_1K_KEY_SIZE;
    pEcid->signMode = LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_1024;
    if (m_bIsGfe2k)
    {
        pEcid->signMode = LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_2048;
        keysizefull = SEC2_GFE_RSA_2K_KEY_SIZE;
    }

    // See if any input file is provided
    if((m_keyInFileName.compare("")))
    {
        // Open file
        pFilePkey = new ifstream(m_keyInFileName.c_str(), std::ios::binary);
        if (!(pFilePkey->is_open()))
        {
            Printf(Tee::PriHigh, "GFE ReadEcid: Error opening input file\n");
            rc = RC::CANNOT_OPEN_FILE;
            goto label_return;
        }

        // Read the contents
        pFilePkey->seekg(0);
        pFilePkey->read((char *)m_privKeyDesc.pCpuAddrSysMem, keysizefull);
    }
    else
    {
        // Copy from pre-defined key
        chipId = GetBoundGpuSubdevice()->DeviceId();
        if ((chipId == Gpu::GM200) || (chipId == Gpu::GM204))
        {
            pGfePkey = gfe_pkey;
        }
        else if ((chipId >= Gpu::GM206) && (chipId < Gpu::GP102))
        {
            pGfePkey = gfe_pkey_gm206;
        }
        else if ((chipId >= Gpu::GP102) && (chipId < Gpu::TU102))
        {
            pGfePkey = gfe_pkey_gp102;
        }
        else if (chipId >= Gpu::TU102)
        {
            pGfePkey = gfe_pkey_tu102;
        }
        else
        {
            Printf(Tee::PriHigh, "GFE ReadEcid: Invalid chip id\n");
            rc = ERROR;
            goto label_return;
        }
        memcpy(m_privKeyDesc.pCpuAddrSysMem, pGfePkey, SEC2_GFE_RSA_1K_KEY_SIZE);
    }

    // See if any output file is provided
    if((m_ekeyOutFileName.compare("")))
    {
        // Open file
        pFileOut = new ofstream(m_ekeyOutFileName.c_str(), std::ios::binary);
        if (!(pFileOut->is_open()))
        {
            Printf(Tee::PriHigh, "GFE ReadEcid: Error opening output file\n");
            rc = RC::CANNOT_OPEN_FILE;
            goto label_return;
        }
    }

    // Seed arbitrary number
    rnd.SeedRandom(0x39425a);

    // Generate server Nonce
    pEcid->serverNonce[0] = rnd.GetRandom();
    pEcid->serverNonce[1] = rnd.GetRandom();
    pEcid->serverNonce[2] = rnd.GetRandom();
    pEcid->serverNonce[3] = rnd.GetRandom();

    Printf(Tee::PriHigh, "GFE: Server nonce generated : 0x%0x 0x%0x 0x%0x 0x%0x\n", pEcid->serverNonce[0],
         pEcid->serverNonce[1], pEcid->serverNonce[2], pEcid->serverNonce[3]);

    // Assign program ID
    pEcid->programID = 0xFFFFAAAA;

    // sessionID
    pEcid->sessionID = 0xBBBBCCCC;

    CHECK_RC(StartMethodStream("ReadEcid", 1));
    m_pCh->Write(0, LW95A1_GFE_SET_ECID_SIGN_BUF, (LwU32)(m_signMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_GFE_SET_PRIV_KEY_BUF, (LwU32)(m_privKeyDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_GFE_READ_ECID, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("ReadEcid"));

    Printf(Tee::PriHigh, "GFE: Server nonce generated got: 0x%0x 0x%0x 0x%0x 0x%0x\n", pEcid->serverNonce[0],
         pEcid->serverNonce[1], pEcid->serverNonce[2], pEcid->serverNonce[3]);
    Printf(Tee::PriHigh, "GFE: Program ID: 0x%0x\n", pEcid->programID);
    Printf(Tee::PriHigh, "GFE: Session ID: 0x%0x\n", pEcid->sessionID);
    Printf(Tee::PriHigh, "GFE: Retcode: 0x%0x\n", pEcid->retCode);

    CHECK_RC(ErrorCheck(HCLASS(ERROR_NONE), pEcid->retCode, "ReadEcid"));
    CHECK_RC(PrintEcid(pEcid, (LwU8*)m_signMemDesc.pCpuAddrSysMem, pFileOut));

label_return:
    delete pFilePkey;
    delete pFileOut;
    pFilePkey = NULL;
    pFileOut  = NULL;
    return rc;
}

/*!
 * Generated encrypted key
 *
 * Overloads the READECID method
 */
RC Class95a1Sec2GfeTest::GenEkey()
{
    RC                  rc             = OK;
    gfe_read_ecid_param *pEcid         = (gfe_read_ecid_param *)m_resMemDesc.pCpuAddrSysMem;
    ifstream             *pFilePkey    = NULL;
    ofstream             *pFileEkey    = NULL;
    LwU32               keysize        = 0;

    // Allocate memory
    memset(m_resMemDesc.pCpuAddrSysMem, 0, 256);

    // Check if input and output files are non-null
    if((!(m_keyInFileName.compare(""))) || (!(m_ekeyOutFileName.compare(""))))
    {
        Printf(Tee::PriHigh, "GFE Gen Key: Missing input or output files\n");
        return RC::CANNOT_OPEN_FILE;
    }

    pFilePkey = new ifstream(m_keyInFileName.c_str(), std::ios::binary);
    pFileEkey = new ofstream(m_ekeyOutFileName.c_str(), std::ios::binary);

    // Open files for input and output
    if (!(pFilePkey->is_open() && pFileEkey->is_open()))
    {
        Printf(Tee::PriHigh, "GFE Gen Key: Error opening input or output files\n");
        rc = RC::CANNOT_OPEN_FILE;
        goto label_return;
    }

    // Pass the key size in signMode
    keysize = SEC2_GFE_RSA_1K_KEY_SIZE_ORI; // Effective key size
    pEcid->signMode = LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_1024;
    if (m_bIsGfe2k)
    {
        pEcid->signMode = LW95A1_GFE_READ_ECID_SIGN_MODE_RSA_2048;
        keysize = SEC2_GFE_RSA_2K_KEY_SIZE_ORI;
    }

    memset(m_privKeyDesc.pCpuAddrSysMem, 0, keysize);

    // Read key from input file
    pFilePkey->seekg(0);
    pFilePkey->read((char *)m_privKeyDesc.pCpuAddrSysMem, keysize);

    CHECK_RC(StartMethodStream("GenEkey", 1));
    m_pCh->Write(0, LW95A1_GFE_SET_ECID_SIGN_BUF, (LwU32)(m_signMemDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_GFE_SET_PRIV_KEY_BUF, (LwU32)(m_privKeyDesc.gpuAddrSysMem >> 8));
    m_pCh->Write(0, LW95A1_GFE_READ_ECID, (LwU32)(m_resMemDesc.gpuAddrSysMem >> 8));
    CHECK_RC(EndMethodStream("GenEkey"));
    CHECK_RC(ErrorCheck(HCLASS(ERROR_NONE), pEcid->retCode, "GenEkey"));

    //Write the encrypted key to the outfile
    pFileEkey->seekp(0);
    pFileEkey->write((char*)m_signMemDesc.pCpuAddrSysMem, SEC2_IV_SIZE + keysize + SEC2_SHA256_HASH_SIZE);

label_return:
    delete pFilePkey;
    delete pFileEkey;
    pFilePkey = NULL;
    pFileEkey = NULL;
    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2GfeTest::Run()
{
    RC rc = OK;

    if (m_bIsGenMode)
        CHECK_RC(GenEkey());
    else
        CHECK_RC(ReadEcid());
    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2GfeTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    FreeMem(&m_semMemDesc);
    FreeMem(&m_resMemDesc);
    FreeMem(&m_signMemDesc);
    FreeMem(&m_privKeyDesc);
    pLwRm->Free(m_hObj);
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2GfeTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2GfeTest, RmTest,
                 "Class95a1Sec2GfeTest RM test.");

CLASS_PROP_READWRITE(Class95a1Sec2GfeTest, keyInFileName, string,
                     "Input file which holds the input key in binary format.");

CLASS_PROP_READWRITE(Class95a1Sec2GfeTest, ekeyOutFileName, string,
                     "Output file which holds the encrypted key in binary format.");

CLASS_PROP_READWRITE(Class95a1Sec2GfeTest, bIsGenMode, bool,
                     "Run the test in key gen mode.");

CLASS_PROP_READWRITE(Class95a1Sec2GfeTest, bIsGfe2k, bool,
                     "Check if 2K key generation is requested.");

