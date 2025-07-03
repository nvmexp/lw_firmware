/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Sec2WorkLaunchTest test.
// Tests Sec2 Hopper CC work launch decryption APIs by sending encrypted
// buffers to SEC2 for decryption and validates that the decrypted output is
// correct.
//

#include "lwos.h"
#include "lwRmApi.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/clcba2.h"
#include "cc_drv.h"
#include "class/clc56f.h"
#include "ctrl/ctrlc56f.h"
#include "gpu/utility/aes_gcm/gcm.h"
#include "gpu/utility/rmctrlutils.h"
#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "core/include/memcheck.h"

#define SEC2_SEMAPHORE_INITIAL_VALUE 0x8765ABCD
#define SEC2_SEMAPHORE_RELEASE_VALUE 0x1234FEDC
#define SEC2_NUM_CHANNELS_PER_CHGRP           1
#define SEC2_NUM_CHANNEL_GROUPS               3

#define PLAIN_TEXT_BYTES (32) // must be multiple of 16B
#define GCM_IV_SIZE_BYTES (12)
#define SEC2_AUTH_TAG_BUF_SIZE_BYTES (0x10)
#define SEC2_SEMA_BUF_SIZE_BYTES (0x4)
#define SEC2_BUF_SIZE_BYTES (PLAIN_TEXT_BYTES)

typedef struct _Sec2MemDesc
{
    Surface2D    surf2d;
    LwU32        size;
    LwRm::Handle hSysMem;
    LwRm::Handle hSysMemDma;
    LwRm::Handle hDynSysMem;
    LwU64        gpuAddrSysMem;
    LwU32       *pCpuAddrSysMem;
} Sec2MemDesc;

typedef struct
{
    LwU32       sizeInBytes;
    Sec2MemDesc dmaSrc;
    Sec2MemDesc dmaDest;
    Sec2MemDesc authTag;
    Sec2MemDesc sema;
} Sec2Buffers, *pSec2Buffers;

typedef struct
{
    ChannelGroup *pChGrp;
    Channel *     pCh[SEC2_NUM_CHANNELS_PER_CHGRP];
    LwRm::Handle  hObj[SEC2_NUM_CHANNELS_PER_CHGRP];
    CC_KMB        kmb[SEC2_NUM_CHANNELS_PER_CHGRP];
    Sec2Buffers   pBuf[SEC2_NUM_CHANNELS_PER_CHGRP];
} Sec2ChGrp, *PSec2ChGrp;

class Sec2WorkLaunchTest: public RmTest
{
public:
    Sec2WorkLaunchTest();
    virtual ~Sec2WorkLaunchTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    FLOAT64      m_TimeoutMs  = Tasker::NO_TIMEOUT;
    RC AllocateMem(Sec2MemDesc *pMemDesc, LwU32 size, Memory::Location location);
    RC AllocSec2Buffers(pSec2Buffers pSec2Buf, Memory::Location srcLocation);
    RC FreeSec2Buffers(pSec2Buffers pSec2Buf);
    RC FreeMem(Sec2MemDesc*);
    RC Sec2DecryptCopy(Channel *pCh, LwRm::Handle* pHObj, CC_KMB *pkmb, pSec2Buffers pBuf);

    Sec2ChGrp    m_Sec2ChGrp[SEC2_NUM_CHANNEL_GROUPS];
};

//------------------------------------------------------------------------------
Sec2WorkLaunchTest::Sec2WorkLaunchTest()
{
    SetName("Sec2WorkLaunchTest");
}

//------------------------------------------------------------------------------
Sec2WorkLaunchTest::~Sec2WorkLaunchTest()
{
}

//------------------------------------------------------------------------
string Sec2WorkLaunchTest::IsTestSupported()
{
    if(IsClassSupported(HOPPER_SEC2_WORK_LAUNCH_A))
        return RUN_RMTEST_TRUE;
    return "HOPPER_SEC2_WORK_LAUNCH_A is not supported on current platform";
}

//------------------------------------------------------------------------------
RC Sec2WorkLaunchTest::Setup()
{

    LwRmPtr                                   pLwRm;
    RC                                        rc;
    LwU32 i;
    LW_STATUS status;

    CHECK_RC(InitFromJs());
    m_TimeoutMs = m_TestConfig.TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);

    for (i = 0; i < SEC2_NUM_CHANNEL_GROUPS; i++)
    {
        LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = {0};
        chGrpParams.engineType = LW2080_ENGINE_TYPE_SEC2;

        Sec2ChGrp *pSec2Ch = &(m_Sec2ChGrp[i]);
        ChannelGroup *pChGrp = NULL;

        pSec2Ch->pChGrp = new ChannelGroup(&m_TestConfig, &chGrpParams);
        pChGrp = pSec2Ch->pChGrp;
        pChGrp->SetUseVirtualContext(false);

        CHECK_RC(pChGrp->Alloc());

        LwU32 j;
        for (j = 0; j < SEC2_NUM_CHANNELS_PER_CHGRP; j++)
        {
            LwU32 flags = FLD_SET_DRF(OS04, _FLAGS, _CC_SELWRE, _TRUE, 0);
            Channel **pLocalCh = &(pSec2Ch->pCh[j]);
            CHECK_RC(pChGrp->AllocChannel(pLocalCh, flags));
            CHECK_RC(pLwRm->Alloc((*pLocalCh)->GetHandle(), &(pSec2Ch->hObj[j]), HOPPER_SEC2_WORK_LAUNCH_A, NULL));

            LWC56F_CTRL_CMD_GET_KMB_PARAMS getKmbParams;
            memset (&getKmbParams, 0, sizeof(LWC56F_CTRL_CMD_GET_KMB_PARAMS));

            status = LwRmControl(pLwRm->GetClientHandle(), (*pLocalCh)->GetHandle(),
                                 LWC56F_CTRL_CMD_GET_KMB,
                                 &getKmbParams, sizeof (LWC56F_CTRL_CMD_GET_KMB_PARAMS));
            if (status != LW_OK)
            {
                Printf(Tee::PriHigh, "SELWRECOPY: Get KMB Failed with status 0x%0x\n", status);
                return RC::SOFTWARE_ERROR;
            }
            memcpy(&(pSec2Ch->kmb[j]), &(getKmbParams.kmb), sizeof(CC_KMB));
        }
    }

    return rc;
}

//------------------------------------------------------------------------
RC Sec2WorkLaunchTest::Run()
{
    RC            rc;
    LwU32 i, j;

    for (i = 0; i < SEC2_NUM_CHANNEL_GROUPS; i++)
    {
        Sec2ChGrp      *pSec2Ch  = &(m_Sec2ChGrp[i]);
        ChannelGroup *pChGrp = pSec2Ch->pChGrp;
        pChGrp->Schedule();
        for (j = 0; j < SEC2_NUM_CHANNELS_PER_CHGRP; j++)
        {
            Channel *pCh = pSec2Ch->pCh[j];
            AllocSec2Buffers(&(pSec2Ch->pBuf[j]), Memory::Fb);
            Sec2DecryptCopy(pCh, &(pSec2Ch->hObj[j]), &(pSec2Ch->kmb[j]), &(pSec2Ch->pBuf[j]));
        }
    }
    return rc;
}

RC Sec2WorkLaunchTest::Sec2DecryptCopy(Channel *pCh, LwRm::Handle* pHObj, CC_KMB *pKmb, pSec2Buffers pBuf)
{
    RC rc;
    PollArguments args;
    LwU8 plainText[PLAIN_TEXT_BYTES] =
    {0x46, 0x05, 0x5f, 0x82, 0x75, 0x69, 0xb4, 0xe2, 0x63, 0xba, 0xbb, 0x98, 0x3e, 0x4f, 0x34, 0xe2, 0xf1, 0x85, 0x17, 0x3d, 0x9f, 0x83, 0x69, 0xc1, 0x03, 0x9d, 0xc6, 0xd2, 0x7a, 0x97, 0x8a, 0xde,
    };
    LwU8 decryptedText[PLAIN_TEXT_BYTES] = {0};
    LwU8 encryptedText[PLAIN_TEXT_BYTES];
    LwU8 authTag[SEC2_AUTH_TAG_BUF_SIZE_BYTES];    
    LwU32 iv[GCM_IV_SIZE_BYTES/4];

    memcpy(iv, pKmb->encryptBundle.iv, GCM_IV_SIZE_BYTES);

    for (LwU32 counter = 0; counter < 2; counter++)
    {
        gcm_context ctx;
        gcm_initialize();
        gcm_setkey(&ctx, (const UINT08 *)pKmb->encryptBundle.key, CC_AES_256_GCM_KEY_SIZE_BYTES);

        // pre-increment IV.
        iv[0] += 1;

        // encrypt the plain text
        int ret = gcm_crypt_and_tag( &ctx, ENCRYPT, (unsigned char *)iv, CC_AES_256_GCM_IV_SIZE_BYTES, NULL, 0,
                                             ((LwU8*)plainText), ((LwU8*)encryptedText), PLAIN_TEXT_BYTES, authTag, SEC2_AUTH_TAG_BUF_SIZE_BYTES);

        if (ret!=0)
        {
            return RC::SOFTWARE_ERROR;
        }

        gcm_zero_ctx( &ctx );

        gcm_initialize();
        gcm_setkey(&ctx, (const UINT08 *)pKmb->encryptBundle.key, CC_AES_256_GCM_KEY_SIZE_BYTES);

        // decrypt the encrypted text (for comparison with SEC2 output)
        ret = gcm_crypt_and_tag( &ctx, DECRYPT, (unsigned char *)iv, CC_AES_256_GCM_IV_SIZE_BYTES, NULL, 0,
                                             ((LwU8*)encryptedText), ((LwU8*)decryptedText), PLAIN_TEXT_BYTES, authTag, SEC2_AUTH_TAG_BUF_SIZE_BYTES);
        if (ret!=0)
        {
            return RC::SOFTWARE_ERROR;
        }

        gcm_zero_ctx( &ctx );

        if (memcmp(decryptedText, plainText, PLAIN_TEXT_BYTES) != 0)
            return RC::SOFTWARE_ERROR;

        for (UINT32 i = 0; i < (PLAIN_TEXT_BYTES); i++)
        {
            MEM_WR08((LwU8 *)pBuf->dmaSrc.pCpuAddrSysMem + i, encryptedText[i]);
        }

        for (UINT32 i = 0; i < (SEC2_AUTH_TAG_BUF_SIZE_BYTES); i++)
        {
            MEM_WR08((LwU8 *)pBuf->authTag.pCpuAddrSysMem + i, authTag[i]);
        }

        MEM_WR32((UINT32 *)((LwU8 *)pBuf->sema.pCpuAddrSysMem), SEC2_SEMAPHORE_INITIAL_VALUE);

        pCh->SetObject(0, *pHObj);

        // test decrypt copy. initialize with garbage
        for (UINT32 i = 0; i < (PLAIN_TEXT_BYTES / 4); i++)
        {
            MEM_WR32(((LwU32 *)pBuf->dmaDest.pCpuAddrSysMem + i), 0xaaaa5555);
        }        

        pCh->Write(0, LWCBA2_DECRYPT_COPY_SRC_ADDR_HI, (UINT32)(pBuf->dmaSrc.gpuAddrSysMem >> 32LL));
        pCh->Write(0, LWCBA2_DECRYPT_COPY_SRC_ADDR_LO, (UINT32)(pBuf->dmaSrc.gpuAddrSysMem & 0xffffffffLL));
        pCh->Write(0, LWCBA2_DECRYPT_COPY_DST_ADDR_HI, (UINT32)(pBuf->dmaDest.gpuAddrSysMem >> 32LL));
        pCh->Write(0, LWCBA2_DECRYPT_COPY_DST_ADDR_LO, (UINT32)(pBuf->dmaDest.gpuAddrSysMem & 0xffffffffLL));

        pCh->Write(0, LWCBA2_DECRYPT_COPY_AUTH_TAG_ADDR_HI, (UINT32)(pBuf->authTag.gpuAddrSysMem >> 32LL));
        pCh->Write(0, LWCBA2_DECRYPT_COPY_AUTH_TAG_ADDR_LO, (UINT32)(pBuf->authTag.gpuAddrSysMem & 0xffffffffLL));
        pCh->Write(0, LWCBA2_DECRYPT_COPY_SIZE, PLAIN_TEXT_BYTES);

        pCh->Write(0, LWCBA2_SEMAPHORE_A, (UINT32)(pBuf->sema.gpuAddrSysMem >> 32LL));
        pCh->Write(0, LWCBA2_SEMAPHORE_B, (UINT32)(pBuf->sema.gpuAddrSysMem & 0xffffffffLL));
        pCh->Write(0, LWCBA2_SET_SEMAPHORE_PAYLOAD_LOWER, (UINT32)(SEC2_SEMAPHORE_RELEASE_VALUE));

        pCh->Write(0, LWCBA2_EXELWTE, 0x1);

        pCh->Flush();

        // Poll on the semaphore release
        args.value  = SEC2_SEMAPHORE_RELEASE_VALUE;

        args.pAddr = (UINT32 *)pBuf->sema.pCpuAddrSysMem;
        Printf(Tee::PriHigh, "Waiting for semaphore release...\n");
        CHECK_RC(PollVal(&args, m_TimeoutMs));
        Printf(Tee::PriHigh, "Semaphore released\n");

        if (memcmp((LwU32 *)pBuf->dmaDest.pCpuAddrSysMem, plainText, PLAIN_TEXT_BYTES) != 0)
            return RC::SOFTWARE_ERROR;

        Printf(Tee::PriHigh, "decryption pass!\n");
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Sec2WorkLaunchTest::Cleanup()
{
    RC rc = OK;
    LwRmPtr pLwRm; 
    LwU32   i;

    for (i = 0; i < SEC2_NUM_CHANNEL_GROUPS; i++)
    {
        Sec2ChGrp      *pSec2Ch  = &(m_Sec2ChGrp[i]);
        LwU32 j = 0;
        for (j = 0; j < SEC2_NUM_CHANNELS_PER_CHGRP; j++)
        {
            Channel *pLocalCh = pSec2Ch->pCh[j];
            FreeSec2Buffers(&(pSec2Ch->pBuf[j]));
            pLwRm->Free(pSec2Ch->hObj[j]);
            pSec2Ch->pChGrp->FreeChannel(pLocalCh);
        }
        pSec2Ch->pChGrp->Free();
        delete pSec2Ch->pChGrp;
    }

    return rc;
}

// Allocate & map memory
RC Sec2WorkLaunchTest::AllocateMem(Sec2MemDesc *pMemDesc, LwU32 size, Memory::Location location)
{
    RC rc = OK;
    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(true);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetColorFormat(ColorUtils::VOID32);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetLayout(Surface2D::Pitch);

    INT32 locOverride = pVidMemSurface->GetLocationOverride();
    pVidMemSurface->SetLocationOverride(Surface2D::NO_LOCATION_OVERRIDE);

    pVidMemSurface->SetLocation(location);
    CHECK_RC_CLEANUP(pVidMemSurface->Alloc(GetBoundGpuDevice()));

    pVidMemSurface->SetLocationOverride(locOverride);

    CHECK_RC_CLEANUP(pVidMemSurface->Map());

    pMemDesc->gpuAddrSysMem = pVidMemSurface->GetCtxDmaOffsetGpu();
    pMemDesc->pCpuAddrSysMem = (LwU32*)pVidMemSurface->GetAddress();
    pMemDesc->size = size;

Cleanup:
    return rc;
}

// Alloc all buffers required for CE encrypt/decrypt
RC Sec2WorkLaunchTest::AllocSec2Buffers(pSec2Buffers pSec2Buf, Memory::Location srcLocation)
{
    RC rc = OK;
    pSec2Buf->sizeInBytes = SEC2_BUF_SIZE_BYTES;

    Memory::Location destLocation;
    if (srcLocation == Memory::Fb)
        destLocation = Memory::NonCoherent;
    else
        destLocation = Memory::Fb;

    AllocateMem(&(pSec2Buf->dmaSrc), SEC2_BUF_SIZE_BYTES, srcLocation);
    AllocateMem(&(pSec2Buf->dmaDest), SEC2_BUF_SIZE_BYTES, destLocation);
    AllocateMem(&(pSec2Buf->sema), SEC2_SEMA_BUF_SIZE_BYTES, Memory::Fb);
    AllocateMem(&(pSec2Buf->authTag), SEC2_AUTH_TAG_BUF_SIZE_BYTES, Memory::NonCoherent);

    return rc;
}

RC Sec2WorkLaunchTest::FreeMem(Sec2MemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

// Free buffers
RC Sec2WorkLaunchTest::FreeSec2Buffers(pSec2Buffers pSec2Buf)
{
    FreeMem(&(pSec2Buf->dmaSrc));
    FreeMem(&(pSec2Buf->dmaDest));
    FreeMem(&(pSec2Buf->authTag));
    FreeMem(&(pSec2Buf->sema));

    return OK;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Sec2WorkLaunchTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Sec2WorkLaunchTest, RmTest,
                 "Sec2WorkLaunchTest RM test.");
