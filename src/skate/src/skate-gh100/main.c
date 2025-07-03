/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#if defined LWRISCV_IS_ENGINE_gsp
#include <dev_gsp.h>
#elif defined LWRISCV_IS_ENGINE_pmu
#include <dev_pwr_pri.h>
#elif defined SAMPLE_PROFILE_sec
#include <dev_sec_pri.h>
#endif

#include <dev_top.h>

#include <lw_riscv_address_map.h>
#include <lwtypes.h>
#include <lwmisc.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/peregrine.h>
#include "dev_bus.h"
#include "dev_fuse.h"
#include "dev_se_seb.h"
#include "liblwriscv/io_dio.h"

#include "lwriscv/csr.h"
//#include "dev_riscv_csr_64.h"
#include "dev_gc6_island.h"
#include "scp_crypt.h"
#include "lwriscv/manifest_gh100.h"
LwU32 pSrcText[4] __attribute__((aligned(16)));
LwU32 pDstText[4] __attribute__((aligned(16)));
LwU32 pIvBuf[4] __attribute__((aligned(16)));


static void init(void)
{
    LwU32 dmemSize = 0;

    // Turn on tracebuffer
    localWrite(LW_PRGNLCL_RISCV_TRACECTL,
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MODE, _STACK) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _SMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _UMODE_ENABLE, _TRUE));

    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

    // configure print to the start of DMEM
    printInit((LwU8*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x1000, 0x1000, 0, 0);

    // Configure FBIF mode
    localWrite(LW_PRGNLCL_FBIF_CTL,
                (DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE) |
                 DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
    localWrite(LW_PRGNLCL_FBIF_CTL2,
                LW_PRGNLCL_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);

    // Unlock IMEM and DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, 0x0);

    // Release priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));
}

#if defined LWRISCV_IS_ENGINE_pmu

LwU32 endian(LwU32 data)
{
    LwU32 byte0 = data&0xff;
    LwU32 byte1 = (data>>8) &0xff;
    LwU32 byte2 = (data>>16) &0xff;
    LwU32 byte3 = (data >> 24) &0xff;

    LwU32 out = 0;

    out = (byte3) | (byte2<<8) | (byte1 << 16) | (byte0 << 24);

    return out;

}
void kcv(LwU32 slotId, char * str)
{
    pSrcText[0] = 0;
    pSrcText[1] = 0;
    pSrcText[2] = 0;
    pSrcText[3] = 0;

    pIvBuf[0] = 0;
    pIvBuf[1] = 0;
    pIvBuf[2] = 0;
    pIvBuf[3] = 0;

    pDstText[0] = 0x11111111;
    pDstText[1] = 0x11111111;
    pDstText[2] = 0x11111111;
    pDstText[3] = 0x11111111;

    scpCbcCryptWithKmem (
        (LwU8*)pSrcText,
        16,
        (LwU8*)pDstText,
        (LwU8*)pIvBuf,
        LW_TRUE,
        slotId
    );
/*
    for(int i = 0; i < 4; i++) {
         priWrite(LW_PBUS_SW_SCRATCH(k), pDstText[i]);
        k++;
    }

*/
    printf("KCV for %s\n", str);
    for(int i = 0; i < 4; i++) {
        printf("%08x", endian(pDstText[i]));
    }
    printf("\n--------------------------\n");
}
void encrypt_data(LwU32 slotId, char *str)
{
    pSrcText[0] = 0x008a3cec;
    pSrcText[1] = 0x617248ba;
    pSrcText[2] = 0xdf9e796f;
    pSrcText[3] = 0x39a0bf62;

    pIvBuf[0] = 0xbcf3419e;
    pIvBuf[1] = 0xf188793d;
    pIvBuf[2] = 0x80f1b6fd;
    pIvBuf[3] = 0xa2a3253d;

    pDstText[0] = 0x11111111;
    pDstText[1] = 0x11111111;
    pDstText[2] = 0x11111111;
    pDstText[3] = 0x11111111;

    scpCbcCryptWithKmem (
        (LwU8*)pSrcText,
        16,
        (LwU8*)pDstText,
        (LwU8*)pIvBuf,
        LW_TRUE,
        slotId
    );

    // for(int i = 0; i < 4; i++) {
    //      priWrite(LW_PBUS_SW_SCRATCH(k), pDstText[i]);
    //     k++;
    // }

    printf("Encrypted data for %s\n", str);
    for(int i = 0; i < 4; i++) {
        printf("%08x", endian(pDstText[i]));
    }
    printf("\n--------------------------\n");
}

void useKMEM(void)
{
    csr_write(LW_RISCV_CSR_MSPM, (0xf<<12) | (0x1<<31));
    csr_write(LW_RISCV_CSR_MRSP, (0x3<<6) | (1<<11));

    priWrite(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_39_PRIV_LEVEL_MASK, 0xffffffff);
    priWrite(LW_PGC6_AON_SELWRE_SCRATCH_GROUP_39, 0x1234);

    // Enabling access of keyslot for EK
    priWrite(LW_PPWR_FALCON_KMEM_SLOT_MASK, 0xffffffff);


    //
    // Keyslot for EK is 254
    // Each slot is 16 bytes
    // so, offset in KMEM is 254*16 = 0xFE0
    // Set autoincr true
    //
    priWrite(LW_PPWR_FALCON_KMEMC(0), (1<<25)|0xFE0);
    printf("LWRISCV: KMEM is :0x%x\n", priRead(LW_PPWR_FALCON_KMEMC(0)));


    // Read KMEM in scratch registers
    // for(int i = 0; i < 8; i++) {
    //      priWrite(LW_PBUS_SW_SCRATCH(i), priRead(LW_PPWR_FALCON_KMEMD(0)));
    // }


    // Read KMEM in scratch registers
    printf("Plaintext Private EK is: \n");
    for(int i = 0; i < 8; i++) {
         printf("%08x ", priRead(LW_PPWR_FALCON_KMEMD(0)));
         if(i == 3) printf("\n");
    }
    printf("\n");
}
#endif


int main(void)
{
    init();
    csr_write(LW_RISCV_CSR_MSPM, (0xf<<12) | (0x1<<31));
    csr_write(LW_RISCV_CSR_MRSP, (0x3<<6) | (1<<11));

    printf("Started 233 \n");
    // PKC_VERIFICATION_MANIFEST *pManifest = (PKC_VERIFICATION_MANIFEST *)(LW_RISCV_AMAP_DMEM_START + 0x2C800 - sizeof(PKC_VERIFICATION_MANIFEST));

    // printf("Manifest: = 0x%x\n",pManifest->version);
    // printf("Manifest: = 0x%x\n",pManifest->ucodeId);
    // printf("Manifest: = 0x%x\n",pManifest->bKDF);

    printf("Started 331 \n");
#if defined LWRISCV_IS_ENGINE_gsp
    printf("GSP 0x111ECD  \n");
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_HEADER_PLAINTEXT,  0x00010301);
/*priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_0,  0x4c604ff1);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_1,  0x1289df4d);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_2,  0x6b39fbb8);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_3,  0x61c047da);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_4,  0x90374ca2);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_5,  0xe78363ad);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_6,  0x114a2a96);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_7,  0xf2abe85c);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_8,  0xb61b6e81);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_9,  0xe271517a);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_10,  0x0b667a76);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_11,  0x28e41dde);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_12,  0x3b93c6d2);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_13,  0x925214d1);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_14,  0xfb2f4539);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_15,  0x1e9130e1);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_16,  0x35a343eb);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_17,  0xcf3f3d79);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_18,  0x3c6531c2);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_KEYS_CIPHERTEXT_19,  0x41f7a73f);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_PROTECTINFO_PLAINTEXT_0, 0x00000000);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_PROTECTINFO_PLAINTEXT_1, 0x00000000);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_PROTECTINFO_PLAINTEXT_2, 0x00000000);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_PAYLOAD_PROTECTINFO_PLAINTEXT_3, 0xFFFF84c0);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_SIGNATURE_0,  0x64370277);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_SIGNATURE_1,  0xa526c324);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_SIGNATURE_2,  0x2ea98dc1);
priWrite(LW_FUSE_OPT_ENC_FUSE_KEY_SIGNATURE_3,  0xeba1ef74);*/

    priWrite(LW_FUSE_OPT_PDI_0, 0x3b0d5fb6);
    priWrite(LW_FUSE_OPT_PDI_1, 0xb2f07cce);

    printf("GSP Done with fuse programming  \n");

//  priWrite(LW_FUSE_OPT_PRIV_SFK_0, 0x04f2bef5);
//  priWrite(LW_FUSE_OPT_PRIV_SFK_1, 0x68eb9583);
//  priWrite(LW_FUSE_OPT_PRIV_SFK_2, 0xb313b16f);
//  priWrite(LW_FUSE_OPT_PRIV_SFK_3, 0x6fc40064);

    printf("GSP Programming PDI  \n");
    DIO_PORT dioPort;
    dioPort.dioType = DIO_TYPE_SE;
    dioPort.portIdx = 0;
    LWRV_STATUS status = LWRV_OK;
    LwU32 val = (1<<2);
    LwU32 ppp = 0xdead;


    status = dioReadWrite(dioPort,DIO_OPERATION_RD, LW_SSE_SE_PDI_AUTOLOAD_CTL, &val);

    printf("GSP LW_SSE_SE_PDI_AUTOLOAD_CTL 0x%x\n", val);
    val = (1<<2);
    status = dioReadWrite(dioPort,DIO_OPERATION_WR, LW_SSE_SE_PDI_AUTOLOAD_CTL, &val);
    printf("GSP DIO status DIO_OPERATION_WR, LW_SSE_SE_PDI_AUTOLOAD_CTL = 0x%x\n", status);
    printf("GSP programmed AUTOLOAD  \n");
    do {
        priWrite(LW_PGSP_FALCON_MAILBOX0, 0x11223344);
        status = dioReadWrite(dioPort,DIO_OPERATION_RD, LW_SSE_SE_PDI_AUTOLOAD_CTL, &ppp);
        printf("GSP DIO status 2 = 0x%x\n", ppp);
        priWrite(LW_PGSP_FALCON_MAILBOX1, ppp );
    }while(ppp != 1);


    priWrite(LW_FUSE_DEBUGCTRL, (1<<11));
    priWrite(LW_FUSE_DEBUGCTRL, (1<<12));
    priWrite(LW_FUSE_DEBUGCTRL, (1<<13));
    priWrite(LW_FUSE_DEBUGCTRL, (1<<17));
    int i = 0;
    for (i = 0 ; i <= 28; i++) {
        priWrite(LW_PGSP_FALCON_KEYGLOB_CTRL, ((1<<20)|i));
        printf("keyblob %d = 0x%x\n", i,priRead(LW_PGSP_FALCON_KEYGLOB_DATA));      
    }
    
    i = 0;
    val = 0xdead;
    status = dioReadWrite(dioPort,DIO_OPERATION_RD, LW_SSE_SE_PDI_AUTOLOAD_VALUE(i), &val);
    printf("GSP DIO LW_SSE_SE_PDI_AUTOLOAD_VALUE(0)= 0x%x\n", val);
    priWrite(LW_PGSP_MAILBOX(i), val);

    i++;
    status = dioReadWrite(dioPort,DIO_OPERATION_RD,LW_SSE_SE_PDI_AUTOLOAD_VALUE(i), &val);
    printf("GSP DIO LW_SSE_SE_PDI_AUTOLOAD_VALUE(1) = 0x%x\n", val);
    priWrite(LW_PGSP_MAILBOX(i), val);
    priWrite(LW_PGSP_FALCON_MAILBOX0, 0x110942);


    printf("GSP Done with PDI \n");

#elif defined LWRISCV_IS_ENGINE_pmu
    // Test code
    priWrite(LW_PPWR_FALCON_MAILBOX0, 0x11223344);

    // EK dumping code
    useKMEM();

    kcv(96, "KMEM SLOT 96\0");
    kcv(127, "KMEM SLOT 127\0");
    kcv(128, "KMEM SLOT 128\0");
    kcv(159, "KMEM SLOT 159\0");
    kcv(160, "KMEM SLOT 160\0");
    kcv(191, "KMEM SLOT 191\0");


    encrypt_data(96, "KMEM SLOT 96\0");
    encrypt_data(127, "KMEM SLOT 127\0");
    encrypt_data(128, "KMEM SLOT 128\0");
    encrypt_data(159, "KMEM SLOT 159\0");
    encrypt_data(160, "KMEM SLOT 160\0");
    encrypt_data(191, "KMEM SLOT 191\0");

    // Enabling access of keyslot for EK
    priWrite(LW_PPWR_FALCON_KMEM_SLOT_MASK, 0x00000000);
    priWrite(LW_PPWR_FALCON_MAILBOX0, 0x12342);
#elif defined SAMPLE_PROFILE_sec
    priWrite(LW_PSEC_FALCON_MAILBOX0, 0xabcd1234);
#endif

    riscvShutdown();
    return 0;
}
