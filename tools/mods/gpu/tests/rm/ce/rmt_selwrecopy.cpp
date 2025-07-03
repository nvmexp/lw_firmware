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
#include "lwos.h"
#include "lwRmApi.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl0070.h" // LW01_MEMORY_SYSTEM_DYNAMIC

#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "class/clc8b5.h"       // HOPPER_DMA_COPY_A
#include "class/clc8b5.h"       // HOPPER_DMA_COPY_A
#include "class/clc56f.h"       // AMPERE_DMA_COPY_A

#include "class/cla06f.h"       // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/clcb33.h"       // LW_CONFIDENTIAL_COMPUTE class
#include "ctrl/ctrlcb33.h"      // LW_CONFIDENTIAL_COMPUTE controls
#include "ctrl/ctrlc56f.h"      // AMPERE_DMA_COPY_A
#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "core/include/memcheck.h"
#include "gpu/utility/aes_gcm/gcm.h"
#include "random.h"
#include "cc_drv.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>

typedef struct _CeMemDesc
{
    Surface2D    surf2d;
    LwU32        size;
    LwRm::Handle hSysMem;
    LwRm::Handle hSysMemDma;
    LwRm::Handle hDynSysMem;
    LwU64        gpuAddrSysMem;
    LwU32       *pCpuAddrSysMem;
} CeMemDesc;

typedef struct
{
    LwU32     sizeInBytes;
    CeMemDesc dmaSrc;
    CeMemDesc dmaDest;
    CeMemDesc authTag;
    CeMemDesc iv;
    CeMemDesc sema;
} CeBuffers, *PCeBuffers;

#define CE_BUF_SIZE_BYTES                   0x3000
#define CE_SEMA_BUF_SIZE_BYTES              0x4
#define CE_AUTH_TAG_BUF_SIZE_BYTES          0x10 

#define CE_NUM_CHANNELS_PER_CHGRP           2
#define CE_NUM_CHANNEL_GROUPS_PER_ENGINE    2
#define FILL_PATTERN                        0xbad0cafe
#define CE_SEMA_PAYLOAD                     0x12345678
typedef struct
{
    ChannelGroup *pChGrp;
    Channel *     pCh[CE_NUM_CHANNELS_PER_CHGRP];
    LwRm::Handle  hObj[CE_NUM_CHANNELS_PER_CHGRP];
    CC_KMB        kmb[CE_NUM_CHANNELS_PER_CHGRP];
} CeChGrp, *PCeChGrp;

class SelwreCopyTest: public RmTest
{
    public:
        SelwreCopyTest();
        virtual ~SelwreCopyTest();
        virtual string IsTestSupported();

        virtual RC Setup();
        virtual RC SetupChannels(LwU32);
        virtual RC FreeChannels();
        virtual RC Run();
        virtual RC Cleanup();
        virtual RC FreeMem(CeMemDesc*);
        virtual RC PollSemaMethod(CeMemDesc *);    
        virtual RC WriteMethod(Channel *, LwU32, LwU32);
        virtual void InitializeSrc(CeMemDesc *pMemDesc);
        virtual RC CompareDest(CeMemDesc *pSrcMemDesc, CeMemDesc *pDstMemDesc);
        virtual RC CopyMemDesc(CeMemDesc *pSrcMemDesc, CeMemDesc *pDstMemDesc);
        virtual RC MemsetMemDesc(CeMemDesc *pSrcMemDesc, LwU32 pat);
        virtual void DumpSrc(CeMemDesc *pMemDesc, char *pStr);
        virtual RC Decrypt(CC_KMB*, CeMemDesc*, CeMemDesc*, CeMemDesc*);
        virtual RC Encrypt(CC_KMB*, CeMemDesc*, CeMemDesc*, CeMemDesc*);
        virtual RC PushCeMethods(Channel *pCh, LwRm::Handle, PCeBuffers pCeBufs, LwU32 semPayload, LwBool bIsModeEncrypt, LwBool bWait);
        virtual RC AllocateMem(CeMemDesc *pMemDesc, LwU32 size, Memory::Location location);
        virtual RC AllocCeBuffers(PCeBuffers pCeBuf, LwU32 srcSizeBytes, Memory::Location srcLocation);
        virtual RC FreeCeBuffers(PCeBuffers pCeBuf);
        virtual RC VerifyOutput(PCeBuffers pCeBuf, CC_KMB kmb, LwBool bCeEncrypt, CeMemDesc *pGenMemDesc);

    private:
        FLOAT64 m_TimeoutMs = Tasker::NO_TIMEOUT;
        LwU32         m_hClient;
        LwU32         m_hDevice;
        GpuSubdevice *m_pSubdev;
        LwRm::Handle  m_hObj;
        LwRm::Handle  m_hCCObj;
        ChannelGroup *pChGrp;
        CC_KMB        m_kmb;
        CeChGrp       m_CeChGrp[CE_NUM_CHANNEL_GROUPS_PER_ENGINE];
};

SelwreCopyTest::SelwreCopyTest() 
{
    SetName("SelwreCopyTest");
}

SelwreCopyTest::~SelwreCopyTest() 
{

}

string SelwreCopyTest::IsTestSupported()
{
    LwRmPtr    pLwRm;
    RC         rc;
    INT32      status = 0;

    if( IsClassSupported(HOPPER_DMA_COPY_A)  && IsClassSupported(LW_CONFIDENTIAL_COMPUTE))
    {
        LW_CONFIDENTIAL_COMPUTE_ALLOC_PARAMS                        params = {0};
        LW_CONF_COMPUTE_CTRL_CMD_SYSTEM_GET_CAPABILITIES_PARAMS capsParams = {0};

        m_hClient = pLwRm->GetClientHandle();
        params.hClient = m_hClient;
        if ((status = pLwRm->Alloc(m_hClient, &m_hCCObj, LW_CONFIDENTIAL_COMPUTE, &params)) != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "%d: LW_CONFIDENTIAL_COMPUTE allocation failed: 0x%x \n", __LINE__,status);
            goto fail;
        }

        status = LwRmControl(m_hClient, m_hCCObj,
                             LW_CONF_COMPUTE_CTRL_CMD_SYSTEM_GET_CAPABILITIES,
                            &capsParams, sizeof (capsParams));
        // Free CC Obj here, not required anymore
        pLwRm->Free(m_hCCObj);
        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                   "%d:CONF_COMPUTE_CTRL_CMD_SYSTEM_GET_CAPABILITIES failed: 0x%x \n", __LINE__,status);
            goto fail;
        }

        if ((capsParams.gpusCapability == LW_CONF_COMPUTE_SYSTEM_GPUS_CAPABILITY_HCC) &&
            (capsParams.ccFeature == LW_CONF_COMPUTE_SYSTEM_FEATURE_HCC_ENABLED))
        {
            return RUN_RMTEST_TRUE;
        }
        goto fail;
    }
    
fail:
    return "HOPPER_DMA_COPY_A class is not supported on current platform";
}

// Free all the allocated channels & channel groups
RC SelwreCopyTest::FreeChannels()
{
    LwRmPtr pLwRm;
    RC rc = OK;
    LwU32   i     = 0;
    for (i = 0; i < CE_NUM_CHANNEL_GROUPS_PER_ENGINE; i++)
    {
        CeChGrp      *pCeCh  = &(m_CeChGrp[i]);
        LwU32 j = 0;
        for (j = 0; j < CE_NUM_CHANNELS_PER_CHGRP; j++)
        {
            Channel *pLocalCh = pCeCh->pCh[j];
            pLwRm->Free(pCeCh->hObj[j]);
            pCeCh->pChGrp->FreeChannel(pLocalCh);
        }
        pCeCh->pChGrp->Free();
        delete pCeCh->pChGrp;
    }

    return rc;
}

// Alloc channel groups & channels
RC SelwreCopyTest::SetupChannels(LwU32 engineType)
{
    LwRmPtr pLwRm;
    RC      rc    = OK;
    LwU32   i     = 0;
    LwU32   flags = 0;
    LwU32 status;

    flags = FLD_SET_DRF(OS04, _FLAGS, _CC_SELWRE, _TRUE, flags);
    for (i = 0; i < CE_NUM_CHANNEL_GROUPS_PER_ENGINE; i++)
    {
        LwU32 j = 0;
        LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = {0};
        chGrpParams.engineType = engineType;
        CeChGrp      *pCeCh  = &(m_CeChGrp[i]);
        ChannelGroup *pChGrp = NULL;

        pCeCh->pChGrp = new ChannelGroup(&m_TestConfig, &chGrpParams);
        pChGrp = pCeCh->pChGrp;
        pChGrp->SetUseVirtualContext(false);

        CHECK_RC(pChGrp->Alloc());

        for (j = 0; j < CE_NUM_CHANNELS_PER_CHGRP; j++)
        {
            Channel **pLocalCh = &(pCeCh->pCh[j]);
            CHECK_RC(pChGrp->AllocChannel(pLocalCh, flags));
            CHECK_RC(pLwRm->Alloc((*pLocalCh)->GetHandle(), &(pCeCh->hObj[j]), HOPPER_DMA_COPY_A, NULL));

            LWC56F_CTRL_CMD_GET_KMB_PARAMS getKmbParams;
            memset (&getKmbParams, 0, sizeof(LWC56F_CTRL_CMD_GET_KMB_PARAMS));

            status = LwRmControl(m_hClient, (*pLocalCh)->GetHandle(),
                                 LWC56F_CTRL_CMD_GET_KMB,
                                 &getKmbParams, sizeof (LWC56F_CTRL_CMD_GET_KMB_PARAMS));
            if (status != LW_OK)
            {
                Printf(Tee::PriHigh, "SELWRECOPY: Get KMB Failed with status 0x%0x\n", status);
                return RC::SOFTWARE_ERROR;
            }
            memcpy(&(pCeCh->kmb[j]), &(getKmbParams.kmb), sizeof(CC_KMB));
        }
    }
    return rc;
}

// Allow multiple channels in a TSG and allocate all necessary channels/groups
RC SelwreCopyTest::Setup() 
{
    RC           rc = OK;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);
    m_TestConfig.SetPushBufferLocation(Memory::Fb);
    SetupChannels(LW2080_ENGINE_TYPE_COPY2);
    return rc;
}


// A standard semaphore polling routine
RC SelwreCopyTest::PollSemaMethod(CeMemDesc *pMemDesc)
{
    RC              rc = OK;
    PollArguments args;

    args.value  = CE_SEMA_PAYLOAD;
    args.pAddr  = (LwU32*)(pMemDesc->pCpuAddrSysMem);
    rc = PollVal(&args, m_TimeoutMs);

    args.value = MEM_RD32(args.pAddr);

    return rc;
}

// Allocate & map memory
RC SelwreCopyTest::AllocateMem(CeMemDesc *pMemDesc, LwU32 size, Memory::Location location)
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

/*!
 *  * FreeMem
 *   *
 *    * Frees the above allocated memory
 *     */
RC SelwreCopyTest::FreeMem(CeMemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

RC SelwreCopyTest::Cleanup() 
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    FreeChannels();

    return firstRc;
}

RC SelwreCopyTest::WriteMethod(Channel *pCh, LwU32 mthdId, LwU32 mthdData)
{
    return pCh->Write(0, mthdId, mthdData);
}

// Initialize source with a known pattern, useful for verification after decryption
void SelwreCopyTest::InitializeSrc(CeMemDesc *pMemDesc)
{
    LwU32 *pAddr = (LwU32*)pMemDesc->pCpuAddrSysMem;
    LwU32 dwsize = pMemDesc->size/4;
    LwU32 i = 0;
    LwU32 tmp = 0;

    while ( i < dwsize)
    {
        tmp = 0xdead0000 + i;
        MEM_WR32(&(pAddr[i]), tmp);
        i++;
    }
}

// A helper routine to dump memdesc
void SelwreCopyTest::DumpSrc(CeMemDesc *pMemDesc, char *pStr)
{
    LwU32 *pAddr = (LwU32*)pMemDesc->pCpuAddrSysMem;
    LwU32 dwsize = pMemDesc->size/4;
    LwU32 i = 0;
    LwU32 tmp = 0;
    Printf(Tee::PriHigh, "Dumping %s: \n", pStr);
    while ( i < dwsize)
    {
        LwU32 j = i;
        while ((j < i+8) && (j < dwsize))
        {
            tmp = MEM_RD32(&(pAddr[j]));
            Printf(Tee::PriHigh, "0x%08x ", tmp);
            j++;
        }
        Printf(Tee::PriHigh, "\n");
        
        i = i + 8;
    }
}

// Helper to copy from SRC -> Dest
RC SelwreCopyTest::CopyMemDesc(CeMemDesc *pSrcMemDesc, CeMemDesc *pDstMemDesc)
{
    LwU32 *pSrcAddr = (LwU32*)pSrcMemDesc->pCpuAddrSysMem;
    LwU32 *pDstAddr = (LwU32*)pDstMemDesc->pCpuAddrSysMem;
    LwU32 dwsize = pSrcMemDesc->size/4;
    LwU32 i = 0;
    LwU32 src = 0;
    while ( i < dwsize)
    {
        src = MEM_RD32(&(pSrcAddr[i]));
        pDstAddr[i] = src;
        i++;
    }
    return OK;
}

// Helper to memset memdesc memory
RC SelwreCopyTest::MemsetMemDesc(CeMemDesc *pSrcMemDesc, LwU32 pat)
{
    LwU32 *pSrcAddr = (LwU32*)pSrcMemDesc->pCpuAddrSysMem;
    LwU32 dwsize = pSrcMemDesc->size/4;
    LwU32 i = 0;
    while ( i < dwsize)
    {
        pSrcAddr[i] = pat;
        i++;
    }

    return OK;
}

// Compares if two memdescs holds the same data in their memory
RC SelwreCopyTest::CompareDest(CeMemDesc *pSrcMemDesc, CeMemDesc *pDstMemDesc)
{
    LwU32 *pSrcAddr = (LwU32*)pSrcMemDesc->pCpuAddrSysMem;
    LwU32 *pDstAddr = (LwU32*)pDstMemDesc->pCpuAddrSysMem;
    LwU32 dwsize = pSrcMemDesc->size/4;
    LwU32 i = 0;
    LwU32 src = 0;
    LwU32 dst = 0;
    while ( i < dwsize)
    {
        src = MEM_RD32(&(pSrcAddr[i]));
        dst = MEM_RD32(&(pDstAddr[i]));
        if (src != dst)
        {
            Printf(Tee::PriHigh, "SELWRECOPY: Compare fail at index %u value 0x%0x expected 0x%0x \n", 
                  i, src, dst);
            return RC::SOFTWARE_ERROR;
        }

        if (i %64 == 0)
        {
            //Printf(Tee::PriHigh, "SELWRECOPY: Print %u value 0x%0x expected 0x%0x \n", 
            //      i, src, dst);
        }
        i++;
    }

    //Printf(Tee::PriHigh, "SELWRECOPY: Compare pass value index %u \n", i);
    return OK;
}

// Decrypts a given block of cipher text.
RC SelwreCopyTest::Decrypt(CC_KMB *pKmb, CeMemDesc *pInMemDesc, CeMemDesc *pAuthTag, CeMemDesc *pOutMemDesc)
{
    RC rc = OK;
    LwU32 iv[3];
    LwU32 i =0;
    gcm_context ctx;
    gcm_initialize();    

    UINT08 *pIn   = (UINT08 *)malloc(pInMemDesc->size);
    UINT08 *pOut  = (UINT08 *)malloc(pOutMemDesc->size);
    UINT08 *pAuth = (UINT08 *)malloc(pAuthTag->size);

    // Copy to temporary memory
    while (i < pInMemDesc->size)
    {
        pIn[i] = MEM_RD08(&(((UINT08*)pInMemDesc->pCpuAddrSysMem)[i]));
        i++;
    }

    i=0;
    while (i < pAuthTag->size)
    {
        pAuth[i] = MEM_RD08(&(((UINT08*)pAuthTag->pCpuAddrSysMem)[i]));
        i++;
    }

    gcm_setkey(&ctx, (const UINT08 *)pKmb->decryptBundle.key, CC_AES_256_GCM_KEY_SIZE_BYTES);

    pKmb->decryptBundle.iv[0] = pKmb->decryptBundle.iv[0] + 1;
    iv[0] = pKmb->decryptBundle.iv[0] ^ pKmb->decryptBundle.ivMask[0];
    iv[1] = pKmb->decryptBundle.iv[1] ^ pKmb->decryptBundle.ivMask[1];
    iv[2] = pKmb->decryptBundle.iv[2] ^ pKmb->decryptBundle.ivMask[2];

    int ret = gcm_crypt_and_tag( &ctx, DECRYPT, (unsigned char *)iv, CC_AES_256_GCM_IV_SIZE_BYTES, NULL, 0,
                                         pIn, pOut, pInMemDesc->size, pAuth, CE_AUTH_TAG_BUF_SIZE_BYTES);
    gcm_zero_ctx( &ctx );
    if (ret!=0)
    {
        rc = RC::SOFTWARE_ERROR;
        goto _decryptRet;
    }


    i = 0;
    while (i < pInMemDesc->size)
    {
        MEM_WR08(&(((UINT08*)pOutMemDesc->pCpuAddrSysMem)[i]), pOut[i]);
        i++;
    }

_decryptRet:
    free(pIn);
    free(pOut);
    free(pAuth);

    return rc; 
}

// Encrypt data with GCM
RC SelwreCopyTest::Encrypt(CC_KMB *pKmb, CeMemDesc *pInMemDesc, CeMemDesc *pAuthTag, CeMemDesc *pOutMemDesc)
{
    RC rc = OK;
    LwU32 iv[3];
    LwU32 i =0;
    gcm_context ctx;
    gcm_initialize();    

    UINT08 *pIn   = (UINT08 *)malloc(pInMemDesc->size);
    UINT08 *pOut  = (UINT08 *)malloc(pOutMemDesc->size);
    UINT08 *pAuth = (UINT08 *)malloc(pAuthTag->size);

    while (i < pInMemDesc->size)
    {
        pIn[i] = MEM_RD08(&(((UINT08*)pInMemDesc->pCpuAddrSysMem)[i]));
        i++;
    }

    gcm_setkey(&ctx, (const UINT08 *)pKmb->encryptBundle.key, CC_AES_256_GCM_KEY_SIZE_BYTES);
    pKmb->encryptBundle.iv[0] = pKmb->encryptBundle.iv[0] + 1;
    iv[0] = pKmb->encryptBundle.iv[0] ^ pKmb->encryptBundle.ivMask[0];
    iv[1] = pKmb->encryptBundle.iv[1] ^ pKmb->encryptBundle.ivMask[1];
    iv[2] = pKmb->encryptBundle.iv[2] ^ pKmb->encryptBundle.ivMask[2];

    int ret = gcm_crypt_and_tag( &ctx, ENCRYPT, (unsigned char *)iv, CC_AES_256_GCM_IV_SIZE_BYTES, NULL, 0,
                                         pIn, pOut, pInMemDesc->size, pAuth, CE_AUTH_TAG_BUF_SIZE_BYTES);
    gcm_zero_ctx( &ctx );
    if (ret!=0)
    {
        rc = RC::SOFTWARE_ERROR;
        goto _encryptRet;
    }

    i = 0;
    while (i < pInMemDesc->size)
    {
        MEM_WR08(&(((UINT08*)pOutMemDesc->pCpuAddrSysMem)[i]), pOut[i]);
        i++;
    }

    i=0;
    while (i < pAuthTag->size)
    {
        MEM_WR08(&(((UINT08*)pAuthTag->pCpuAddrSysMem)[i]), pAuth[i]);
        i++;
    }

_encryptRet:
    free(pIn);
    free(pOut);
    free(pAuth);

    return rc; 
}

// Push CE Encrypt & Decrypt methods
RC SelwreCopyTest::PushCeMethods(Channel *pCh, LwRm::Handle hObj, PCeBuffers pCeBufs, LwU32 semPayload, LwBool bIsModeEncrypt, LwBool bFlush_Wait)
{
    RC rc = OK;
    Surface2D *pSrc  = &(pCeBufs->dmaSrc.surf2d);
    Surface2D *pDst  = &(pCeBufs->dmaDest.surf2d);
    Surface2D *pSema = &(pCeBufs->sema.surf2d);
    Surface2D *pAuth = &(pCeBufs->authTag.surf2d);
    Surface2D *pIv   = &(pCeBufs->iv.surf2d);
    pDst->Fill(FILL_PATTERN);

    pCh->SetObject(0, hObj);


    CHECK_RC(WriteMethod(pCh, LWC8B5_OFFSET_IN_UPPER, LwU64_HI32(pSrc->GetCtxDmaOffsetGpu())));
    CHECK_RC(WriteMethod(pCh, LWC8B5_OFFSET_IN_LOWER, LwU64_LO32(pSrc->GetCtxDmaOffsetGpu())));

    CHECK_RC(WriteMethod(pCh, LWC8B5_OFFSET_OUT_UPPER, LwU64_HI32(pDst->GetCtxDmaOffsetGpu())));
    CHECK_RC(WriteMethod(pCh, LWC8B5_OFFSET_OUT_LOWER, LwU64_LO32(pDst->GetCtxDmaOffsetGpu())));

    CHECK_RC(WriteMethod(pCh, LWC8B5_PITCH_IN, pSrc->GetPitch()));
    CHECK_RC(WriteMethod(pCh, LWC8B5_PITCH_OUT, pDst->GetPitch()));

    CHECK_RC(WriteMethod(pCh, LWC8B5_LINE_COUNT, 1));
    CHECK_RC(WriteMethod(pCh, LWC8B5_LINE_LENGTH_IN, LwU64_LO32(pSrc->GetSize())));

    CHECK_RC(WriteMethod(pCh, LWC8B5_SET_SEMAPHORE_PAYLOAD, semPayload));

    CHECK_RC(WriteMethod(pCh, LWC8B5_SET_SEMAPHORE_A,
                DRF_NUM(C8B5, _SET_SEMAPHORE_A, _UPPER, LwU64_HI32(pSema->GetCtxDmaOffsetGpu()))));

    CHECK_RC(WriteMethod(pCh, LWC8B5_SET_SEMAPHORE_B, LwU64_LO32(pSema->GetCtxDmaOffsetGpu())));

    if (bIsModeEncrypt == LW_TRUE)
    {
        CHECK_RC(WriteMethod(pCh, LWC8B5_SET_SELWRE_COPY_MODE, LWC8B5_SET_SELWRE_COPY_MODE_MODE_ENCRYPT));
        CHECK_RC(WriteMethod(pCh, LWC8B5_SET_ENCRYPT_AUTH_TAG_ADDR_UPPER, LwU64_HI32(pAuth->GetCtxDmaOffsetGpu())));
        CHECK_RC(WriteMethod(pCh, LWC8B5_SET_ENCRYPT_AUTH_TAG_ADDR_LOWER, LwU64_LO32(pAuth->GetCtxDmaOffsetGpu())));
    }
    else
    {
        CHECK_RC(WriteMethod(pCh, LWC8B5_SET_SELWRE_COPY_MODE, LWC8B5_SET_SELWRE_COPY_MODE_MODE_DECRYPT));
        CHECK_RC(WriteMethod(pCh, LWC8B5_SET_DECRYPT_AUTH_TAG_COMPARE_ADDR_UPPER, LwU64_HI32(pAuth->GetCtxDmaOffsetGpu())));
        CHECK_RC(WriteMethod(pCh, LWC8B5_SET_DECRYPT_AUTH_TAG_COMPARE_ADDR_LOWER, LwU64_LO32(pAuth->GetCtxDmaOffsetGpu())));
    }

    CHECK_RC(WriteMethod(pCh, LWC8B5_SET_ENCRYPT_IV_ADDR_UPPER, LwU64_HI32(pIv->GetCtxDmaOffsetGpu())));
    CHECK_RC(WriteMethod(pCh, LWC8B5_SET_ENCRYPT_IV_ADDR_LOWER, LwU64_LO32(pIv->GetCtxDmaOffsetGpu())));

    CHECK_RC(WriteMethod(pCh, LWC8B5_LAUNCH_DMA,
                    DRF_DEF(C8B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _COPY_TYPE, _SELWRE)));


    if (bFlush_Wait == LW_TRUE)
    {
        pCh->Flush();
        pCh->WaitIdle(m_TimeoutMs);
        PollSemaMethod(&(pCeBufs->sema));
    }

    return rc;

}

// Alloc all buffers required for CE encrypt/decrypt
RC SelwreCopyTest::AllocCeBuffers(PCeBuffers pCeBuf, LwU32 srcSizeBytes, Memory::Location srcLocation)
{
    RC rc = OK;
    pCeBuf->sizeInBytes = srcSizeBytes;

    Memory::Location destLocation;
    if (srcLocation == Memory::Fb)
        destLocation = Memory::NonCoherent;
    else
        destLocation = Memory::Fb;

    AllocateMem(&(pCeBuf->dmaSrc), srcSizeBytes, srcLocation);
    AllocateMem(&(pCeBuf->dmaDest), srcSizeBytes, destLocation);
    AllocateMem(&(pCeBuf->sema), CE_SEMA_BUF_SIZE_BYTES, Memory::Fb);
    AllocateMem(&(pCeBuf->authTag), CE_AUTH_TAG_BUF_SIZE_BYTES, Memory::NonCoherent);
    AllocateMem(&(pCeBuf->iv), CC_AES_256_GCM_IV_SIZE_BYTES, Memory::NonCoherent);

    return rc;
}

// Free buffers
RC SelwreCopyTest::FreeCeBuffers(PCeBuffers pCeBuf)
{
    FreeMem(&(pCeBuf->dmaSrc));
    FreeMem(&(pCeBuf->dmaDest));
    FreeMem(&(pCeBuf->authTag));
    FreeMem(&(pCeBuf->iv));
    FreeMem(&(pCeBuf->sema));

    return OK;
}

// Verifies if CE encryption & decryption produces the expected value
RC SelwreCopyTest::VerifyOutput(PCeBuffers pCeBuf, CC_KMB kmb, LwBool bCeEncrypt, CeMemDesc *pGenMemDesc)
{
    RC         rc = OK;

    if (bCeEncrypt == LW_TRUE)
    {
        // Decrypt DEST and compare with SRC
        if ((rc = Decrypt(&kmb, &(pCeBuf->dmaDest), &(pCeBuf->authTag), pGenMemDesc)) != OK)
        {
            Printf(Tee::PriHigh, "SelwreCopyTest:: Decrypt Failed \n");
            goto _verifyOutputRet;
        }
        rc = CompareDest(&(pCeBuf->dmaSrc), pGenMemDesc);
        goto _verifyOutputRet;
    }
    else
    {
        // Compare DEST and golden Mem Desc
        rc = CompareDest(&(pCeBuf->dmaDest), pGenMemDesc);
        goto _verifyOutputRet;
    }
_verifyOutputRet:
    return rc;
}

// Loop through channel groups & channels to issue CE encryption & decryption
RC SelwreCopyTest::Run()
{
    RC rc = OK;
    CeMemDesc genMemDesc;
    CeBuffers d2hCeBuf;
    CeBuffers h2dCeBuf;

    LwU32 i = 0;

    for (i = 0; i < CE_NUM_CHANNEL_GROUPS_PER_ENGINE; i++)
    {
        LwU32        j = 0;
        CeChGrp      *pCeCh  = &(m_CeChGrp[i]);
        ChannelGroup *pChGrp = pCeCh->pChGrp;
        pChGrp->Schedule();

        for (j = 0; j < CE_NUM_CHANNELS_PER_CHGRP; j++)
        {
            // CE Encrypt from Device to Host
            AllocateMem(&genMemDesc, CE_BUF_SIZE_BYTES, Memory::NonCoherent);
            AllocCeBuffers(&d2hCeBuf, CE_BUF_SIZE_BYTES, Memory::Fb);
            InitializeSrc(&(d2hCeBuf.dmaSrc));
            PushCeMethods(pCeCh->pCh[j], pCeCh->hObj[j], &d2hCeBuf, CE_SEMA_PAYLOAD, LW_TRUE, LW_TRUE);
            rc = VerifyOutput(&d2hCeBuf, pCeCh->kmb[j], LW_TRUE, &genMemDesc);
            FreeCeBuffers(&d2hCeBuf);
            if (rc != OK)
            {
                Printf(Tee::PriHigh, "SECURE COPY:Verification FAILED\n");
                goto _runRet;
            }

            MemsetMemDesc(&genMemDesc, 0x0);
            pChGrp->Schedule();

            // CE Decrypt from Host to Device
            AllocCeBuffers(&h2dCeBuf, CE_BUF_SIZE_BYTES, Memory::NonCoherent);
            InitializeSrc(&(genMemDesc));
            Encrypt(&(pCeCh->kmb[j]), &genMemDesc, &(h2dCeBuf.authTag), &(h2dCeBuf.dmaSrc));
            PushCeMethods(pCeCh->pCh[j], pCeCh->hObj[j], &h2dCeBuf, CE_SEMA_PAYLOAD, LW_FALSE, LW_TRUE);
            rc = VerifyOutput(&h2dCeBuf, pCeCh->kmb[j], LW_FALSE, &genMemDesc);
            FreeCeBuffers(&h2dCeBuf);
            FreeMem(&genMemDesc);
            if (rc != OK)
            {
                Printf(Tee::PriHigh, "SECURE COPY:Verification FAILED\n");
                goto _runRet;
            }
        }
    }

_runRet:
    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ SelwreCopyTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(SelwreCopyTest, RmTest,
                 "SelwreCopyTest RM test.");
