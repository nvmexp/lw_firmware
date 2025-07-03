#include "gdma_test.h"

static uint8_t dmemBuf1[BUF_SIZE], dmemBuf2[BUF_SIZE], bufRef[BUF_SIZE];
static volatile uint8_t* bufExt;
static volatile uint32_t* bufPri;


void copyFromPri(uint32_t* dest)
{
    for (unsigned i = 0; i < (BUF_SIZE / 4); i++)
    {
        dest[i] = bufPri[i];
    }
}

void copyToPri(uint32_t* src)
{
    for (unsigned i = 0; i < (BUF_SIZE / 4); i++)
    {
        bufPri[i] = src[i];
    }
}

void createRefBuf(GDMA_ADDR_CFG* cfg, volatile uint8_t* src, volatile uint8_t* dest, uint32_t length)
{
    uint32_t strideSrc;
    uint32_t strideDest;
    uint32_t wrapSrc = (cfg->srcAddrMode == GDMA_ADDR_MODE_WRAP) ? ((cfg->wrapLen + 1u) * 4) : 0xFFFFFFFF;
    uint32_t wrapDest = (cfg->destAddrMode == GDMA_ADDR_MODE_WRAP) ? ((cfg->wrapLen + 1u) * 4) : 0xFFFFFFFF;

    uint32_t srcIdx = 0;
    uint32_t destIdx = 0;

    switch (cfg->srcAddrMode)
    {
    case GDMA_ADDR_MODE_INC:
    case GDMA_ADDR_MODE_WRAP:
        strideSrc = 4;
        break;
    case GDMA_ADDR_MODE_FIX:
        strideSrc = 0;
        break;
    case GDMA_ADDR_MODE_STRIDE:
        strideSrc = (4u << cfg->strideLogLen);
        break;
    default:
        printf("createRefBuf: bad srcAddrMode\n");
        return;
    }

    switch (cfg->destAddrMode)
    {
    case GDMA_ADDR_MODE_INC:
    case GDMA_ADDR_MODE_WRAP:
        strideDest = 4;
        break;
    case GDMA_ADDR_MODE_FIX:
        strideDest = 0;
        break;
    case GDMA_ADDR_MODE_STRIDE:
        strideDest = (4u << cfg->strideLogLen);
        break;
    default:
        printf("createRefBuf: bad srcAddrMode\n");
        return;
    }

    while (length)
    {
        //transfer in 4-byte chunks
        dest[destIdx] = src[srcIdx];
        dest[destIdx + 1] = src[srcIdx + 1];
        dest[destIdx + 2] = src[srcIdx + 2];
        dest[destIdx + 3] = src[srcIdx + 3];

        destIdx += strideDest;
        srcIdx += strideSrc;

        if (destIdx == wrapDest)
        {
            destIdx = 0;
        }
        if (srcIdx == wrapSrc)
        {
            srcIdx = 0;
        }

        length -= 4;
    }
}

int dbg_memcmp(const void* s1, const void* s2, unsigned n)
{
    const unsigned char* d = s1;
    const unsigned char* s = s2;
    while (n--)
    {
        char x = (char)((*s) - (*d));
        if (x) {
            printf("dbg_memcmp@n%d:%02x|%02x.", n, (*s), (*d));
            return x;
        }
        d++;
        s++;
    }
    return 0;
}

void initBuffers(void)
{
    for (unsigned i = 0; i < BUF_SIZE; i++)
    {
        dmemBuf1[i] = (i % 0x100 < 0x100) ? ((uint8_t)i) : ((uint8_t)(0x200 - i));
        dmemBuf2[i] = (i % 0x100 < 0x100) ? ((uint8_t)(0x100 - i)) : ((uint8_t)(i - 0x100));
        bufRef[i] = 0xBB;
        bufExt[i] = 0xEE;
    }

    for (unsigned i = 0; i < (BUF_SIZE / 4); i++)
    {
        // fill it up with the same bytes as bufRef, so that when we do a sparse transfer
        // to bugPri (stride mode used so that not all dest bytes written), the non-written
        // bytes match those in the ref buffer
        bufPri[i] = 0xBBBBBBBB;
    }
}

int gdma_test(void)
{
    LWRV_STATUS status;

    bufExt = (uint8_t*)(LW_RISCV_AMAP_FBGPA_START);
    bufPri = (uint32_t*)(LW_RISCV_AMAP_PRIV_START);

    const GDMA_CHANNEL_PRIV_CFG cfgPriv = {
        .bVirtual = LW_FALSE,
        .asid = 0
    };

    status = gdmaCfgChannelPriv(0, &cfgPriv);
    if (status != LWRV_OK) {
        printf("gdmaCfgChannelPriv failed: %u\n", status);
        return 1;
    }

    const GDMA_CHANNEL_CFG cfg = {
        .rrWeight = 0xF,   //max
        .maxInFlight = 0, //GDMA_CHAN_COMMON_CONFIG_MAX_INFLIGHT_UNLIMITED
        .bIrqEn = LW_FALSE,
        .bMemq = LW_FALSE
    };

    status = gdmaCfgChannel(0, &cfg);
    if (status != LWRV_OK) {
        printf("gdmaCfgChannel failed: %u\n", status);
        return 1;
    }

    //synchronous dmem-to-dmem transfer
    initBuffers();
    status = gdmaXferReg(
        (uint64_t)dmemBuf1,     //src
        (uint64_t)dmemBuf2,     //dest
        BUF_SIZE,         //length
        NULL,             //GDMA_ADDR_CFG *pAddrCfg
        0,                //chanIdx
        0,                //subChanIdx
        LW_TRUE,          //bWaitComplete
        LW_FALSE          //bIrqEn
    );
    if (status != LWRV_OK || dbg_memcmp(dmemBuf1, dmemBuf2, BUF_SIZE))
    {
        printf("dma TCM->TCM failed %x\n", status);
        return 1;
    }
    else
    {
        printf("dma TCM->TCM successful\n");
    }

    //synchronous dmem-to-fb transfer
    initBuffers();
    status = gdmaXferReg(
        (uint64_t)dmemBuf1,     //src
        (uint64_t)bufExt,   //dest
        BUF_SIZE,     //length
        NULL,             //GDMA_ADDR_CFG *pAddrCfg,
        0,                //chanIdx,
        0,                //subChanIdx
        LW_TRUE,          // bWaitComplete,
        LW_FALSE          // bIrqEn
    );
    if (status != LWRV_OK || dbg_memcmp(dmemBuf1, (void*)bufExt, BUF_SIZE))
    {
        printf("dma TCM->FB failed %x\n", status);
        return 1;
    }
    else
    {
        printf("dma TCM->FB successful\n");
    }

    //synchronous fb-to-dmem transfer
    initBuffers();
    status = gdmaXferReg(
        (uint64_t)bufExt,     //src
        (uint64_t)dmemBuf1,   //dest
        BUF_SIZE,     //length
        NULL,             //GDMA_ADDR_CFG *pAddrCfg,
        0,                //chanIdx,
        0,                //subChanIdx
        LW_TRUE,          // bWaitComplete,
        LW_FALSE          // bIrqEn
    );
    if (status != LWRV_OK || dbg_memcmp(dmemBuf1, (void*)bufExt, BUF_SIZE))
    {
        printf("dma FB->TCM failed %d\n", status);
        return 1;
    }
    else
    {
        printf("dma FB->TCM successful\n");
    }

    //transfer to a bad address and recover
    initBuffers();
    status = gdmaXferReg(
        (uint64_t)0xDEADBEEFBEEFDEAD,  //src
        (uint64_t)dmemBuf1,   //dest
        BUF_SIZE,     //length
        NULL,             //GDMA_ADDR_CFG *pAddrCfg,
        0,                //chanIdx,
        0,                //subChanIdx
        LW_TRUE,          // bWaitComplete,
        LW_FALSE          // bIrqEn
    );
    if (status != LWRV_OK)
    {
        printf("dma FB(bad address)->TCM failed as expected %x\n", status);
    }
    else
    {
        printf("dma FB(bad address)->TCM succeeded (UNEXPECTED!)\n");
        return 1;
    }

    status = gdmaGetChannelStatus(0, NULL, NULL, NULL);
    if (status != LWRV_OK)
    {
        uint8_t errorCause;
        uint32_t errorId;
        status = gdmaGetErrorAndResetChannel(0, &errorCause, &errorId);
        if (status != LWRV_OK)
        {
            printf("gdmaGetErrorAndResetChannel failed: %x\n", status);
            return 1;
        }
        else
        {
            printf("errorCause: %02x, errorId: %08x \n", errorCause, errorId);
        }
    }
    else
    {
        printf("gdmaGetChannelStatus returned LWRV_OK after intentional failure (UNEXPECTED!)\n");
        return 1;
    }

    //asynchronous dmem-to-dmem transfer
    initBuffers();
    status = gdmaXferReg(
        (uint64_t)dmemBuf1,
        (uint64_t)dmemBuf2,
        BUF_SIZE,
        NULL,     //GDMA_ADDR_CFG *pAddrCfg,
        0, //chanIdx,
        0,                //subChanIdx
        LW_TRUE, // bWaitComplete,
        LW_FALSE // bIrqEn
    );
    if (status != LWRV_OK)
    {
        printf("async dma FB->TCM failed %d\n", status);
        return 1;
    }

    //poll completion
    uint32_t reqProduce, reqConsume, reqComplete;
    do {
        status = gdmaGetChannelStatus(0, &reqProduce, &reqConsume, &reqComplete);
        if (status != LWRV_OK)
        {
            printf("gdmaGetChannelStatus failed %x\n", status);
            return 1;
        }
        else
        {
            printf("S:%d %d %d\n", reqProduce, reqConsume, reqComplete);
        }
    } while (reqProduce != reqComplete);

    if (dbg_memcmp(dmemBuf1, dmemBuf2, BUF_SIZE))
    {
        printf("async dma FB->TCM failed\n");
        return 1;
    }
    else
    {
        printf("async dma FB->TCM successful\n");
    }

#if ENABLE_GDMA_PRI_XFER
    //synchronous pri-to-dmem transfer, fixed address in src
    GDMA_ADDR_CFG addrCfg = {
        .srcAddrMode = GDMA_ADDR_MODE_FIX,
        .destAddrMode = GDMA_ADDR_MODE_INC,
        .wrapLen = 0,
        .strideLogLen = 0
    };
    initBuffers();
    copyToPri((uint32_t*)dmemBuf1);
    createRefBuf(&addrCfg, dmemBuf1, bufRef, BUF_SIZE);
    status = gdmaXferReg(
        (uint64_t)bufPri,   //src
        (uint64_t)dmemBuf2, //dest
        BUF_SIZE,         //length
        &addrCfg,         //GDMA_ADDR_CFG *pAddrCfg,
        0,                //chanIdx,
        0,                //subChanIdx
        LW_TRUE,          // bWaitComplete,
        LW_FALSE          // bIrqEn
    );
    if (status != LWRV_OK || dbg_memcmp(dmemBuf2, bufRef, BUF_SIZE))
    {
        printf("dma TCM->TCM, fixed address in src failed %d\n", status);
        return 1;
    }
    else
    {
        printf("dma TCM->TCM, fixed address in src successful\n");
    }

    //synchronous pri-to-dmem transfer, wrapping address in src
    initBuffers();
    addrCfg.srcAddrMode = GDMA_ADDR_MODE_WRAP;
    addrCfg.wrapLen = 3; //wrap every 16 bytes
    copyToPri((uint32_t*)dmemBuf1);
    createRefBuf(&addrCfg, dmemBuf1, bufRef, BUF_SIZE);
    status = gdmaXferReg(
        (uint64_t)bufPri,     //src
        (uint64_t)dmemBuf2,     //dest
        BUF_SIZE,     //length
        &addrCfg,         //GDMA_ADDR_CFG *pAddrCfg,
        0,                //chanIdx,
        0,                //subChanIdx
        LW_TRUE,          // bWaitComplete,
        LW_FALSE          // bIrqEn
    );
    if (status != LWRV_OK || dbg_memcmp(dmemBuf2, bufRef, BUF_SIZE))
    {
        printf("dma TCM->TCM, wrap address in src failed %d\n", status);
        dumpBuf(dmemBuf2, BUF_SIZE);
        return 1;
    }
    else
    {
        printf("dma TCM->TCM, wrap address in src successful\n");
    }

    //synchronous dmem-to-pri transfer, wrapping address in dest
    initBuffers();
    addrCfg.srcAddrMode = GDMA_ADDR_MODE_INC;
    addrCfg.destAddrMode = GDMA_ADDR_MODE_WRAP;
    addrCfg.wrapLen = 3; //wrap every 16 bytes
    createRefBuf(&addrCfg, dmemBuf1, bufRef, BUF_SIZE);

    status = gdmaXferReg(
        (uint64_t)dmemBuf1,     //src
        (uint64_t)bufPri,     //dest
        BUF_SIZE,     //length
        &addrCfg,         //GDMA_ADDR_CFG *pAddrCfg,
        0,                //chanIdx,
        0,                //subChanIdx
        LW_TRUE,          // bWaitComplete,
        LW_FALSE          // bIrqEn
    );
    copyFromPri((uint32_t*)dmemBuf2);
    if (status != LWRV_OK || dbg_memcmp(dmemBuf2, bufRef, (addrCfg.wrapLen + 1u) * 4))
    {
        printf("dma TCM->TCM, wrap address in dest failed %d\n", status);
        dumpBuf(dmemBuf2, BUF_SIZE);
        return 1;
    }
    else
    {
        printf("dma TCM->TCM, wrap address in dest successful\n");
    }

    //synchronous pri-to-dmem transfer, stride address in src
    initBuffers();
    addrCfg.srcAddrMode = GDMA_ADDR_MODE_STRIDE;
    addrCfg.destAddrMode = GDMA_ADDR_MODE_INC;
    addrCfg.strideLogLen = 2; //A DWORD every 16 bytes
    copyToPri((uint32_t*)dmemBuf1);
    createRefBuf(&addrCfg, dmemBuf1, bufRef, BUF_SIZE / (1u << addrCfg.strideLogLen));

    status = gdmaXferReg(
        (uint64_t)bufPri,     //src
        (uint64_t)dmemBuf2,     //dest
        BUF_SIZE / (1u << addrCfg.strideLogLen),   //length
        &addrCfg,         //GDMA_ADDR_CFG *pAddrCfg,
        0,                //chanIdx,
        0,                //subChanIdx
        LW_TRUE,          // bWaitComplete,
        LW_FALSE          // bIrqEn
    );
    if (status != LWRV_OK || dbg_memcmp(dmemBuf2, bufRef, BUF_SIZE / (1u << addrCfg.strideLogLen)))
    {
        printf("dma TCM->TCM, stride address in src failed %d\n", status);
        dumpBuf(dmemBuf2, BUF_SIZE);
        return 1;
    }
    else
    {
        printf("dma TCM->TCM, stride address in src successful\n");
    }

    //synchronous dmem-to-pri transfer, stride address in dest
    initBuffers();
    addrCfg.srcAddrMode = GDMA_ADDR_MODE_INC;
    addrCfg.destAddrMode = GDMA_ADDR_MODE_STRIDE;
    addrCfg.strideLogLen = 2; //A DWORD every 16 bytes
    createRefBuf(&addrCfg, dmemBuf1, bufRef, BUF_SIZE / (1u << addrCfg.strideLogLen));

    status = gdmaXferReg(
        (uint64_t)dmemBuf1,     //src
        (uint64_t)bufPri,     //dest
        BUF_SIZE / (1u << addrCfg.strideLogLen),   //length
        &addrCfg,         //GDMA_ADDR_CFG *pAddrCfg,
        0,                //chanIdx,
        0,                //subChanIdx
        LW_TRUE,          // bWaitComplete,
        LW_FALSE          // bIrqEn
    );
    copyFromPri((uint32_t*)dmemBuf2);
    if (status != LWRV_OK || dbg_memcmp(dmemBuf2, bufRef, BUF_SIZE))
    {
        printf("dma TCM->TCM, stride address in dest failed %d\n", status);
        dumpBuf(dmemBuf2, BUF_SIZE);
        return 1;
    }
    else
    {
        printf("dma TCM->TCM, stride address in dest successful\n");
    }
#endif
    return 0;
}
