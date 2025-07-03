/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <lwmisc.h>
#include <lwtypes.h>

#include <dev_master.h>
#include <dev_pwr_pri.h>
#include "gpu-io.h"
#include "engine-io.h"
#include "brom.h"
#include "misc.h"

void bar(void)
{
    printf("-----------------------------------------------------------\n");
}

#define FILE_BASE "sample-test-"

int main(int argc, char *argv[])
{
    const char *textFile = FILE_BASE ENGINE_NAME ".text.encrypt.bin";
    const char *dataFile = FILE_BASE ENGINE_NAME ".data.encrypt.bin";
    const char *manifestFile = FILE_BASE ENGINE_NAME ".manifest.encrypt.bin.out.bin";
    Gpu *pGpu;
    Engine *pEngine;

    if (geteuid())
    {
        printf("Run me as root (to access /dev/mem).\n");
        return -1;
    }

    if (argc > 1)
    {
        textFile = argv[1];
        if (argc > 2)
        {
            dataFile = argv[2];
            if (argc > 3)
                manifestFile = argv[3];
        }
    }
    printf("\n\n");
    printf("===========================================================\n");
    printf("Code: %s\n", textFile);
    printf("Data: %s\n", dataFile);
    printf("Manifest: %s\n", manifestFile);
    bar();

    printf("Opening gpu... ");
    unsigned deviceWhitelist[] = {0x2294, 0x222f, 0x223f, 0x24af, 0x2482, 0};
    int i = 0;
    while (1)
    {
        printf("Trying %x\n", deviceWhitelist[i]);
        pGpu = gpuOpen(deviceWhitelist[i]);
        if (pGpu)
            break;
        ++i;
        if (deviceWhitelist[i] == 0)
            break;
    }

    if (!pGpu)
    {
        return -1;
    }

    if (!gpuIsAlive(pGpu))
    {
        printf("GPU seems dead...\n");
        return -1;
    }

    if (!gpuIsDevinitCompleted(pGpu))
    {
        bar();
        printf("\n*** WARNING ***\n\n");
        printf("Devinit has not been exelwted on GPU...\n");
        printf("Bootrom will most likely hang doing fence.\n");
        printf("\n*** WARNING ***\n\n");
        bar();
    }

    printf("Done.\n");
    printf("Platform: %s\n", gpuPlatformType(pGpu));
    pEngine = engineGet(pGpu, ENGINE_NAME);
    bromDumpStatus(pEngine);
    engineReset(pEngine);
    //setup access to PMB
    bromConfigurePmbBoot(pEngine);

    bromDumpStatus(pEngine);

    bar();
    printf("Trying noob-boot...\n");
    void *pImg;
    LwLength imgSize;

    if (!imageRead(manifestFile, &pImg, &imgSize))
        return -1;
    printf("Writing manifest @%llx, size %lld\n", pEngine->dmemSize - imgSize, imgSize);
    engineWriteDmem(pEngine, pEngine->dmemSize - imgSize, imgSize, pImg);

    free(pImg);

    if (!imageRead(dataFile, &pImg, &imgSize))
        return -1;
    if (imgSize > 0)
    {
        printf("Writing monitor data @%x, size %lld\n", 0, imgSize);
        engineWriteDmem(pEngine, 0, imgSize, pImg);
        free(pImg);
    }

    if (!imageRead(textFile, &pImg, &imgSize))
        return -1;
    printf("Writing monitor code @%x, size %lld\n", 0, imgSize);
    engineWriteImem(pEngine, 0, imgSize, pImg);
    free(pImg);

    bar();
    printf("Images programmed. Booting...\n");

    bromBoot(pEngine);
    bar();
    bromDumpStatus(pEngine);

    printf("Waiting for completion...\n");
    LwBool bootRes;
    bootRes = bromWaitForCompletion(pEngine, 5000);
    if (!bootRes)
    {
        printf("* * * T I M E O U T * * *\n");
        // Check if we did devinit, otherwise BROM hangs on fence...

        if (!gpuIsDevinitCompleted(pGpu))
        {
            printf("\nTimeout may be caused because you haven't run devinit.\n\n");
            usleep(500 * 1000);
            printf("Friendly advice: run devinit.\n\n");
            usleep(500 * 1000);
            printf("Tip of the day: running devinit helps testing risc-v.\n\n");
            usleep(500 * 1000);
            printf("Trivia: people that skip devinit hang bootrom.\n\n");
        }
    }
    bromDumpStatus(pEngine);

    // wait for icd
    while (!engineIsInIcd(pEngine))
    {
        usleep(1000);
    }
    dumpDmesg(pEngine);

    enginePut(pEngine);
    gpuClose(pGpu);

    return 0;
}
