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

// Secure Dma Copy Test.
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/utility/aes_gcm/gcm.h"
#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl0070.h"       // LW01_MEMORY_SYSTEM_DYNAMIC
#include "class/clc8b5.h"       // HOPPER_DMA_COPY_A
#include "class/clc56f.h"       // AMPERE_DMA_COPY_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_COPY_ENGINE
#include "class/clcb33.h"       // LW_CONFIDENTIAL_COMPUTE class
#include "ctrl/ctrlc56f.h"      // AMPERE_DMA_COPY_A, CC_AES_256_GCM_KEY_SIZE_BYTES
#include "ctrl/ctrlcb33.h"      // LW_CONFIDENTIAL_COMPUTE controls
#include "selwrecpy.h"

#define CE_BUF_SIZE_BYTES                   0x3000
#define CE_SEMA_BUF_SIZE_BYTES              0x4
#define CE_AUTH_TAG_BUF_SIZE_BYTES          0x10

#define CE_NUM_CHANNELS_PER_CHGRP           2
#define CE_NUM_CHANNEL_GROUPS_PER_ENGINE    2
#define FILL_PATTERN                        0xbad0cafe
#define CE_SEMA_PAYLOAD                     0x12345678

//=================================================================================================
// CeMemDesc Implementation
//=================================================================================================

//-------------------------------------------------------------------------------------------------
// Allocate & map memory
RC CeMemDesc::AllocateMemory(GpuDevice *gpudev, UINT32 size, Memory::Location location)
{
    RC rc;
    m_Size = size;
    m_Surf2d.SetForceSizeAlloc(true);
    m_Surf2d.SetArrayPitch(1);
    m_Surf2d.SetArraySize(size);
    m_Surf2d.SetColorFormat(ColorUtils::VOID32);
    m_Surf2d.SetAddressModel(Memory::Paging);
    m_Surf2d.SetLayout(Surface2D::Pitch);

    INT32 locOverride = m_Surf2d.GetLocationOverride();
    m_Surf2d.SetLocationOverride(Surface2D::NO_LOCATION_OVERRIDE);
    m_Surf2d.SetLocation(location);

    CHECK_RC(m_Surf2d.Alloc(gpudev));
    m_AllocDone = true;
    m_Surf2d.SetLocationOverride(locOverride);

    CHECK_RC(m_Surf2d.Map());
    m_GpuAddrSysMem = m_Surf2d.GetCtxDmaOffsetGpu();
    m_pCpuAddrSysMem = (UINT32*)m_Surf2d.GetAddress();

    return rc;
}

//-------------------------------------------------------------------------------------------------
// Fill the SYS memory with a user pattern
RC CeMemDesc::Fill(UINT32 pat)
{
    UINT32 dwsize = m_Size/4;
    if (m_pCpuAddrSysMem)
    {
        for (UINT32 i = 0; i < dwsize; i++)
        {
            m_pCpuAddrSysMem[i] = pat;
        }
    }
    return RC::OK;
}

//-------------------------------------------------------------------------------------------------
// Initialize the SYS memory with a distinct pattern
void CeMemDesc::InitializeSrc()
{
    UINT32 dwsize = m_Size/4;
    UINT32 tmp    = 0;
    if (m_pCpuAddrSysMem)
    {
        for (UINT32 i = 0; i < dwsize; i++)
        {
            tmp = 0xdead0000 + i;
            Platform::VirtualWr32(&(m_pCpuAddrSysMem[i]), tmp);
        }
    }
}

//-------------------------------------------------------------------------------------------------
RC CeMemDesc::Compare(CeMemDesc *pDstMemDesc)
{
    UINT32 *pSrcAddr = m_pCpuAddrSysMem;
    UINT32 *pDstAddr = (UINT32*)pDstMemDesc->m_pCpuAddrSysMem;
    UINT32 dwsize = m_Size/4;
    UINT32 src = 0;
    UINT32 dst = 0;
    for (UINT32 i = 0; i < dwsize; i++)
    {
        src = MEM_RD32(&(pSrcAddr[i]));
        dst = MEM_RD32(&(pDstAddr[i]));
        if (src != dst)
        {
            Printf(Tee::PriError,
                   "SELWRECOPY: Compare fail at index %u value 0x%0x expected 0x%0x \n",
                   i, src, dst);
            return RC::BUFFER_MISMATCH;
        }
    }
    return RC::OK;
}

//-------------------------------------------------------------------------------------------------
// A helper routine to dump memdesc in SYS memory
void CeMemDesc::Dump(const char *pStr)
{
    UINT32 *pAddr   = m_pCpuAddrSysMem;
    UINT32 dwsize   = m_Size/4;
    Printf(Tee::PriNormal, "Dumping %s: \n", pStr);

    for (UINT32 i = 0; i < dwsize; i += 8)
    {
        for (UINT32 j = i; ((j < i+8) && (j < dwsize)); j++)
        {
            Printf(Tee::PriNormal, "0x%08x ", MEM_RD32(&(pAddr[j])));
        }
        Printf(Tee::PriNormal, "\n");
    }
}

//=================================================================================================
// SelwreCopy Implementation
//=================================================================================================
//-------------------------------------------------------------------------------------------------
bool SelwreCopy::IsSupported()
{
    RC              rc;
    LwRmPtr         pLwRm;  // self-initializing singleton
    LwRm::Handle    hCCObj = 0;
    LwRm::Handle    hClient = pLwRm->GetClientHandle();
    if (!pLwRm->IsClassSupported(HOPPER_DMA_COPY_A, GetBoundGpuDevice()) ||
        !pLwRm->IsClassSupported(LW_CONFIDENTIAL_COMPUTE, GetBoundGpuDevice()))
    {
        return false;
    }
    
    LW_CONFIDENTIAL_COMPUTE_ALLOC_PARAMS                        params = { 0 };
    LW_CONF_COMPUTE_CTRL_CMD_SYSTEM_GET_CAPABILITIES_PARAMS capsParams = { 0 };

    params.hClient = hClient;
    rc = pLwRm->Alloc(hClient, &hCCObj, LW_CONFIDENTIAL_COMPUTE, &params);
    if (rc != RC::OK)
    {
        Printf(Tee::PriDebug, "LW_CONFIDENTIAL_COMPUTE alloc failed: 0x%x \n", rc.Get());
        return false;
    }
    DEFER
    {
        // Free CC Obj here, not required anymore
        pLwRm->Free(hCCObj);
    };

    rc = pLwRm->Control(hCCObj, LW_CONF_COMPUTE_CTRL_CMD_SYSTEM_GET_CAPABILITIES,
                        &capsParams, sizeof (capsParams));
    if (rc != RC::OK)
    {
        Printf(Tee::PriDebug,
               "CONF_COMPUTE_CTRL_CMD_SYSTEM_GET_CAPABILITIES failed: 0x%x\n", rc.Get());
        return false;
    }

    if ((capsParams.gpusCapability != LW_CONF_COMPUTE_SYSTEM_GPUS_CAPABILITY_HCC) ||
        (capsParams.ccFeature != LW_CONF_COMPUTE_SYSTEM_FEATURE_HCC_ENABLED))
    {
        return false;
    }

    return true;
}

//-------------------------------------------------------------------------------------------------
RC SelwreCopy::FreeChannels()
{
    LwRmPtr pLwRm;
    RC rc;
    for (UINT32 i = 0; i < CHANNEL_GROUPS_PER_ENGINE; i++)
    {
        CeChanGroup * pCeCh  = &(m_CeChanGroup[i]);
        for (UINT32 j = 0; j < CHANNELS_PER_GROUP; j++)
        {
            if (pCeCh->hObj[j])
            {
                pLwRm->Free(pCeCh->hObj[j]);
            }
            Channel *pLocalCh = pCeCh->pCh[j];
            if (pCeCh->pChGrp && pLocalCh)
            {
                pCeCh->pChGrp->FreeChannel(pLocalCh);
            }
        }
        if (pCeCh->pChGrp)
        {
            pCeCh->pChGrp->Free();
            delete pCeCh->pChGrp;
        }
        memset(pCeCh, 0, sizeof(CeChanGroup));
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Alloc channel groups & channels
RC SelwreCopy::SetupChannels(UINT32 engineType)
{
    LwRmPtr pLwRm;
    UINT32  flags    = 0;
    RC      rc;

    flags = FLD_SET_DRF(OS04, _FLAGS, _CC_SELWRE, _TRUE, flags);
    for (UINT32 i = 0; i < CHANNEL_GROUPS_PER_ENGINE; i++)
    {
        LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = { 0 };
        chGrpParams.engineType = engineType;
        CeChanGroup * pCeCh  = &(m_CeChanGroup[i]);
        ChannelGroup *pChGrp = NULL;

        pCeCh->pChGrp = new ChannelGroup(GetTestConfiguration(), &chGrpParams);
        pChGrp = pCeCh->pChGrp;
        pChGrp->SetUseVirtualContext(false);

        CHECK_RC(pChGrp->Alloc());

        for (UINT32 j = 0; j < CHANNELS_PER_GROUP; j++)
        {
            Channel **pLocalCh = &(pCeCh->pCh[j]);
            CHECK_RC(pChGrp->AllocChannel(pLocalCh, flags));
            CHECK_RC(pLwRm->Alloc((*pLocalCh)->GetHandle(), &(pCeCh->hObj[j]),
                                  HOPPER_DMA_COPY_A, NULL));

            LWC56F_CTRL_CMD_GET_KMB_PARAMS getKmbParams;
            memset (&getKmbParams, 0, sizeof(LWC56F_CTRL_CMD_GET_KMB_PARAMS));

            CHECK_RC_MSG(pLwRm->Control((*pLocalCh)->GetHandle(), LWC56F_CTRL_CMD_GET_KMB,
                                        &getKmbParams, sizeof (LWC56F_CTRL_CMD_GET_KMB_PARAMS)),
                                        "SELWRECOPY: Get KMB Failed.\n");

            memcpy(&(pCeCh->kmb[j]), &(getKmbParams.kmb), sizeof(CC_KMB));
        }
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
bool SelwreCopy::PollSemaPayload(void * Args)
{
    PollSemaPayloadArgs * args = static_cast<PollSemaPayloadArgs *>(Args);
    UINT32 data = Platform::VirtualRd32(args->pAddr);
    return (data == args->value);
}

//-------------------------------------------------------------------------------------------------
// A typical semaphore polling routine
RC SelwreCopy::PollSemaMethod(CeMemDesc *pMemDesc)
{
    RC rc;
    PollSemaPayloadArgs args;

    args.value  = CE_SEMA_PAYLOAD;
    args.pAddr  = (UINT32*)(pMemDesc->m_pCpuAddrSysMem);

    CHECK_RC(POLLWRAP(PollSemaPayload, &args, m_TestConfig.TimeoutMs()));

    return rc;
}

//-------------------------------------------------------------------------------------------------
// Decrypts a given block of cipher text using software algorith in gcm
RC SelwreCopy::SwDecrypt
(
    CC_KMB *pKmb,
    CeMemDesc *pInMemDesc,
    CeMemDesc *pAuthTag,
    CeMemDesc *pOutMemDesc
)
{
    RC rc;
    UINT32 iv[3];
    gcm_context ctx;
    gcm_initialize();

    vector<UINT08> input(pInMemDesc->m_Size, 0);
    vector<UINT08> output(pOutMemDesc->m_Size, 0);
    vector<UINT08> auth(pAuthTag->m_Size, 0);
    // Copy to temporary memory
    Platform::MemCopy(input.data(), pInMemDesc->m_pCpuAddrSysMem, pInMemDesc->m_Size);
    Platform::MemCopy(auth.data(), pAuthTag->m_pCpuAddrSysMem, pAuthTag->m_Size);

    gcm_setkey(&ctx, (const UINT08 *)pKmb->decryptBundle.key, CC_AES_256_GCM_KEY_SIZE_BYTES);

    pKmb->decryptBundle.iv[0] = pKmb->decryptBundle.iv[0] + 1;
    iv[0] = pKmb->decryptBundle.iv[0] ^ pKmb->decryptBundle.ivMask[0];
    iv[1] = pKmb->decryptBundle.iv[1] ^ pKmb->decryptBundle.ivMask[1];
    iv[2] = pKmb->decryptBundle.iv[2] ^ pKmb->decryptBundle.ivMask[2];

    INT32 err = gcm_crypt_and_tag(&ctx,
                                  DECRYPT,
                                  (UINT08 *)iv,
                                  CC_AES_256_GCM_IV_SIZE_BYTES,
                                  NULL,
                                  0,
                                  input.data(),
                                  output.data(),
                                  pInMemDesc->m_Size,
                                  auth.data(),
                                  CE_AUTH_TAG_BUF_SIZE_BYTES);
    gcm_zero_ctx(&ctx);
    if (!err)
    {
        Platform::MemCopy(pOutMemDesc->m_pCpuAddrSysMem, output.data(), pInMemDesc->m_Size);
    }
    else
    {
        Printf(Tee::PriError, "SelwreCopy::SwDecrypt(), gcm_crypt_and_tag() failed with 0x%x\n", err);
        rc = RC::HW_ERROR;
    }

    return rc;
}
//-------------------------------------------------------------------------------------------------
RC SelwreCopy::SwEncrypt
(
    CC_KMB *pKmb,
    CeMemDesc *pInMemDesc,
    CeMemDesc *pAuthTag,
    CeMemDesc *pOutMemDesc
)
{
    RC      rc;
    UINT32  iv[3];
    gcm_context ctx;
    gcm_initialize();

    vector<UINT08> input(pInMemDesc->m_Size, 0);
    vector<UINT08> output(pOutMemDesc->m_Size, 0);
    vector<UINT08> auth(pAuthTag->m_Size,0);
    Platform::MemCopy(input.data(),pInMemDesc->m_pCpuAddrSysMem, pInMemDesc->m_Size);

    gcm_setkey(&ctx, (const UINT08 *)pKmb->encryptBundle.key, CC_AES_256_GCM_KEY_SIZE_BYTES);
    pKmb->encryptBundle.iv[0] = pKmb->encryptBundle.iv[0] + 1;
    iv[0] = pKmb->encryptBundle.iv[0] ^ pKmb->encryptBundle.ivMask[0];
    iv[1] = pKmb->encryptBundle.iv[1] ^ pKmb->encryptBundle.ivMask[1];
    iv[2] = pKmb->encryptBundle.iv[2] ^ pKmb->encryptBundle.ivMask[2];

    INT32 err = gcm_crypt_and_tag(&ctx,
                                  ENCRYPT,
                                  (unsigned char *)iv,
                                  CC_AES_256_GCM_IV_SIZE_BYTES,
                                  NULL,
                                  0,
                                  input.data(),
                                  output.data(),
                                  pInMemDesc->m_Size,
                                  auth.data(),
                                  CE_AUTH_TAG_BUF_SIZE_BYTES);
    gcm_zero_ctx(&ctx);
    if (!err)
    {
        Platform::MemCopy(pOutMemDesc->m_pCpuAddrSysMem, output.data(), pInMemDesc->m_Size);
        Platform::MemCopy(pAuthTag->m_pCpuAddrSysMem, auth.data(), pAuthTag->m_Size);
    }
    else
    {
        Printf(Tee::PriError, "SelwreCopy::SwEncrypt(),gcm_crypt_and_tag() failed with 0x%x\n", err);
        rc = RC::HW_ERROR;
    }

    return rc;
}

//-------------------------------------------------------------------------------------------------
// Push CE SwEncrypt & SwDecrypt methods
RC SelwreCopy::PushCeMethods
(
    Channel *pCh,
    LwRm::Handle hObj,
    CeBuffers * pCeBufs,
    LwU32 semPayload,
    bool bIsModeEncrypt,
    bool bFlushAndWait
)
{
    RC rc;
    Surface2D *pSrc  = &(pCeBufs->dmaSrc.m_Surf2d);
    Surface2D *pDst  = &(pCeBufs->dmaDest.m_Surf2d);
    Surface2D *pSema = &(pCeBufs->sema.m_Surf2d);
    Surface2D *pAuth = &(pCeBufs->authTag.m_Surf2d);
    Surface2D *pIv   = &(pCeBufs->iv.m_Surf2d);
    CHECK_RC(pDst->Fill(FILL_PATTERN));

    CHECK_RC(pCh->SetObject(0, hObj));

    CHECK_RC(pCh->Write(0, LWC8B5_OFFSET_IN_UPPER, LwU64_HI32(pSrc->GetCtxDmaOffsetGpu())));
    CHECK_RC(pCh->Write(0, LWC8B5_OFFSET_IN_LOWER, LwU64_LO32(pSrc->GetCtxDmaOffsetGpu())));

    CHECK_RC(pCh->Write(0, LWC8B5_OFFSET_OUT_UPPER, LwU64_HI32(pDst->GetCtxDmaOffsetGpu())));
    CHECK_RC(pCh->Write(0, LWC8B5_OFFSET_OUT_LOWER, LwU64_LO32(pDst->GetCtxDmaOffsetGpu())));

    CHECK_RC(pCh->Write(0, LWC8B5_PITCH_IN, pSrc->GetPitch()));
    CHECK_RC(pCh->Write(0, LWC8B5_PITCH_OUT, pDst->GetPitch()));

    CHECK_RC(pCh->Write(0, LWC8B5_LINE_COUNT, 1));
    CHECK_RC(pCh->Write(0, LWC8B5_LINE_LENGTH_IN, LwU64_LO32(pSrc->GetSize())));

    CHECK_RC(pCh->Write(0, LWC8B5_SET_SEMAPHORE_PAYLOAD, semPayload));

    CHECK_RC(pCh->Write(0, LWC8B5_SET_SEMAPHORE_A,
             DRF_NUM(C8B5, _SET_SEMAPHORE_A, _UPPER, LwU64_HI32(pSema->GetCtxDmaOffsetGpu()))));
    CHECK_RC(pCh->Write(0, LWC8B5_SET_SEMAPHORE_B, LwU64_LO32(pSema->GetCtxDmaOffsetGpu())));

    if (bIsModeEncrypt)
    {
        CHECK_RC(pCh->Write(0, LWC8B5_SET_SELWRE_COPY_MODE, LWC8B5_SET_SELWRE_COPY_MODE_MODE_ENCRYPT));
        CHECK_RC(pCh->Write(0, LWC8B5_SET_ENCRYPT_AUTH_TAG_ADDR_UPPER, LwU64_HI32(pAuth->GetCtxDmaOffsetGpu())));
        CHECK_RC(pCh->Write(0, LWC8B5_SET_ENCRYPT_AUTH_TAG_ADDR_LOWER, LwU64_LO32(pAuth->GetCtxDmaOffsetGpu())));
    }
    else
    {
        CHECK_RC(pCh->Write(0, LWC8B5_SET_SELWRE_COPY_MODE, LWC8B5_SET_SELWRE_COPY_MODE_MODE_DECRYPT));
        CHECK_RC(pCh->Write(0, LWC8B5_SET_DECRYPT_AUTH_TAG_COMPARE_ADDR_UPPER, LwU64_HI32(pAuth->GetCtxDmaOffsetGpu())));
        CHECK_RC(pCh->Write(0, LWC8B5_SET_DECRYPT_AUTH_TAG_COMPARE_ADDR_LOWER, LwU64_LO32(pAuth->GetCtxDmaOffsetGpu())));
    }

    CHECK_RC(pCh->Write(0, LWC8B5_SET_ENCRYPT_IV_ADDR_UPPER, LwU64_HI32(pIv->GetCtxDmaOffsetGpu())));
    CHECK_RC(pCh->Write(0, LWC8B5_SET_ENCRYPT_IV_ADDR_LOWER, LwU64_LO32(pIv->GetCtxDmaOffsetGpu())));

    CHECK_RC(pCh->Write(0, LWC8B5_LAUNCH_DMA,
                    DRF_DEF(C8B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED) |
                    DRF_DEF(C8B5, _LAUNCH_DMA, _COPY_TYPE, _SELWRE)));

    if (bFlushAndWait)
    {
        CHECK_RC(pCh->Flush());
        CHECK_RC(pCh->WaitIdle(GetTestConfiguration()->TimeoutMs()));
        CHECK_RC(PollSemaMethod(&(pCeBufs->sema)));
    }
    return rc;
}

//------------------------------------------------------------------------------------------------
// Alloc all buffers required for CE encrypt/decrypt
RC SelwreCopy::AllocCeBuffers(CeBuffers *pCeBuf, LwU32 srcSizeBytes, Memory::Location srcLocation)
{
    RC rc;
    pCeBuf->sizeInBytes = srcSizeBytes;

    Memory::Location destLocation;
    if (srcLocation == Memory::Fb)
    {
        destLocation = Memory::NonCoherent;
    }
    else
    {
        destLocation = Memory::Fb;
    }
    GpuDevice *gpudev = GetBoundGpuDevice();
    CHECK_RC(pCeBuf->dmaSrc.AllocateMemory(gpudev, srcSizeBytes, srcLocation));
    CHECK_RC(pCeBuf->dmaDest.AllocateMemory(gpudev, srcSizeBytes, destLocation));
    CHECK_RC(pCeBuf->sema.AllocateMemory(gpudev, CE_SEMA_BUF_SIZE_BYTES, Memory::Fb));
    CHECK_RC(pCeBuf->authTag.AllocateMemory(gpudev, CE_AUTH_TAG_BUF_SIZE_BYTES,
                                            Memory::NonCoherent));
    CHECK_RC(pCeBuf->iv.AllocateMemory(gpudev, CC_AES_256_GCM_IV_SIZE_BYTES,
                                       Memory::NonCoherent));

    return rc;
}

//------------------------------------------------------------------------------------------------
// Verifies if CE encryption & decryption produces the expected value
RC SelwreCopy::VerifyOutput
(
    CeBuffers* pCeBuf,
    CC_KMB kmb,
    bool bCeEncrypt,
    CeMemDesc *pGenMemDesc
)
{
    RC rc;
    if (bCeEncrypt)
    {
        // Decrypt DEST and compare with SRC
        CHECK_RC(SwDecrypt(&kmb, &(pCeBuf->dmaDest), &(pCeBuf->authTag), pGenMemDesc));
        CHECK_RC(pCeBuf->dmaSrc.Compare(pGenMemDesc));
    }
    else
    {
        // Compare DEST and golden Mem Desc
        CHECK_RC(pCeBuf->dmaDest.Compare(pGenMemDesc));
    }
    return rc;
}

//------------------------------------------------------------------------------------------------
RC SelwreCopy::Setup()
{
    RC rc;
    memset(&m_CeChanGroup, 0, sizeof(m_CeChanGroup));
    
    CHECK_RC(InitFromJs());
    m_TestConfig.SetAllowMultipleChannels(true);
    m_TestConfig.SetPushBufferLocation(Memory::Fb);
    CHECK_RC(SetupChannels(LW2080_ENGINE_TYPE_COPY2));
    return rc;
}

//------------------------------------------------------------------------------------------------
// Verify that the CE HW encryption/decryption is identical to the SW encryption/decryption.
// Use the following process:
// 1. HW Encrypt from d2hCeBuf.dmaSrc to d2hCeBuf.dmaDest
// 2. SW Decrypt from d2hCeBuf.dmaDest to genMemDesc
// 3. SW Encrypt from genMemDesc to h2dCeBuf.dmaSrc
// 4. HW Decrypt from h2dCeBuf.dmaSrc to h2dCeBuf.dmaDest
// 5. SW verify h2dCeBuf.dmaDest with genMemDesc.
// Note: We could also verify d2hCeBuf.dmaDest with h2dCeBuf.dmaSrc, but we indirectly do that
// by using the encryption/decryption process as a round trip process.
RC SelwreCopy::Run()
{
    RC rc;
    CeMemDesc genMemDesc;
    CeBuffers d2hCeBuf; // device to host
    CeBuffers h2dCeBuf; // host to device

    CHECK_RC(genMemDesc.AllocateMemory(GetBoundGpuDevice(), CE_BUF_SIZE_BYTES,
                                       Memory::NonCoherent));
    // CE Encrypt from Device to Host
    CHECK_RC(AllocCeBuffers(&d2hCeBuf, CE_BUF_SIZE_BYTES, Memory::Fb));

    // CE Decrypt from Host to Device
    CHECK_RC(AllocCeBuffers(&h2dCeBuf, CE_BUF_SIZE_BYTES, Memory::NonCoherent));

    // Initialize the golden surface
    genMemDesc.InitializeSrc();

    for (UINT32 i = 0; i < CHANNEL_GROUPS_PER_ENGINE; i++)
    {
        CeChanGroup  *pCeCh  = &(m_CeChanGroup[i]);
        ChannelGroup *pChGrp = pCeCh->pChGrp;
        pChGrp->Schedule();

        for (UINT32 j = 0; j < CHANNELS_PER_GROUP; j++)
        {
            d2hCeBuf.dmaSrc.InitializeSrc();
            // Use CE to Encrypt from Device to Host
            CHECK_RC(PushCeMethods(pCeCh->pCh[j], pCeCh->hObj[j], &d2hCeBuf, CE_SEMA_PAYLOAD,
                                   true, true));
            // Use software to decrypt the output and compare against the golden surface.
            CHECK_RC_MSG(VerifyOutput(&d2hCeBuf, pCeCh->kmb[j], true, &genMemDesc),
                                      "SECURE COPY:d2h Verification FAILED\n");

            pChGrp->Schedule();
            // Encrypt the golden surface and place it in the dmaSrc buffer.
            CHECK_RC(SwEncrypt(&(pCeCh->kmb[j]), &genMemDesc, &(h2dCeBuf.authTag),
                             &(h2dCeBuf.dmaSrc)));

            // Use CE to Decrypt from Host to Device
            CHECK_RC(PushCeMethods(pCeCh->pCh[j], pCeCh->hObj[j], &h2dCeBuf, CE_SEMA_PAYLOAD,
                                   false, true));
            CHECK_RC_MSG(VerifyOutput(&h2dCeBuf, pCeCh->kmb[j], false, &genMemDesc),
                                      "SECURE COPY:h2d Verification FAILED\n");
        }
    }
    return rc;
}

//------------------------------------------------------------------------------------------------
RC SelwreCopy::Cleanup()
{
    return FreeChannels();
}

//------------------------------------------------------------------------------------------------
void SelwreCopy::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    // No specific properties yet
    return;
}

JS_CLASS_INHERIT(SelwreCopy, PexTest, "GPU dma encryption/decryption test");

