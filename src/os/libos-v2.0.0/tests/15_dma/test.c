#include <stdbool.h>

#include "../common/test-common.h"
#include "libos.h"
#include "task_test.h"

DEFINE_LOG_TEST()

LwU8 compSrc[0x10000];
LwU8 compDst[0x10000];

#define RM_PAGE_SHIFT 12

static void MemCopy(LwU8 *pDest, LwU8 *pSrc, LwU32 len)
{
    LwU32 i;

    for (i = 0; i < len; i++)
    {
        pDest[i] = pSrc[i];
    }
}

static void DumpMem(LwU64 *pDump, int nLength)
{
    int i;
    nLength = (nLength + 3) >> 2;

    for (i = 0; i < nLength; i += 4)
    {
        LOG_TEST(
            LOG_LEVEL_INFO, "%p: 0x%016llx 0x%016llx 0x%016llx 0x%016llx\n", &pDump[i], pDump[i + 0],
            pDump[i + 1], pDump[i + 2], pDump[i + 3]);
    }
}

static void CheckSrcBuffer(void)
{
    LwU64 *pTcmBuffer = (LwU64 *)TASK_RM_TEST_TCM_SRC;
    LwU32 i;
    LwU32 mismatchIndex = 0;
    LwBool bMismatch    = LW_FALSE;

    for (i = 0; i < 0x2000; i++)
    {
        if (pTcmBuffer[i] != (LwU64)TASK_RM_TEST_TCM_SRC + (i << 3))
        {
            if (!bMismatch)
            {
                mismatchIndex = i;
                bMismatch     = LW_TRUE;
            }
        }
        else
        {
            if (bMismatch)
            {
                LOG_TEST(
                    LOG_LEVEL_INFO, "src mismatch at offsets %p - %p (0x%016llx != 0x%016llx)\n",
                    &pTcmBuffer[mismatchIndex], (LwU8 *)&pTcmBuffer[i] - 1, pTcmBuffer[mismatchIndex],
                    TASK_RM_TEST_TCM_SRC + (mismatchIndex << 3));

                DumpMem(&pTcmBuffer[mismatchIndex], i - mismatchIndex);
                bMismatch = LW_FALSE;
            }
        }
    }

    if (bMismatch)
    {
        LOG_TEST(
            LOG_LEVEL_INFO, "src mismatch at offsets %p - %p (0x%016llx != 0x%016llx)\n",
            &pTcmBuffer[mismatchIndex], (LwU8 *)&pTcmBuffer[i] - 1, pTcmBuffer[mismatchIndex],
            TASK_RM_TEST_TCM_SRC + (mismatchIndex << 3));

        DumpMem(&pTcmBuffer[mismatchIndex], i - mismatchIndex);
        bMismatch = LW_FALSE;
    }
}

static void CompareDstBuffer(LwU64 *pCmpBuffer)
{
    LwU64 *pTcmBuffer = (LwU64 *)TASK_RM_TEST_TCM_DEST;
    LwU32 i;
    int mismatchIndex = 0;
    LwBool bMismatch  = LW_FALSE;

    for (i = 0; i < 0x2000; i++)
    {
        if (pTcmBuffer[i] != pCmpBuffer[i])
        {
            if (!bMismatch)
            {
                mismatchIndex = i;
                bMismatch     = LW_TRUE;
            }
        }
        else
        {
            if (bMismatch)
            {
                LOG_TEST(
                    LOG_LEVEL_INFO, "%s: dst mismatch at offsets %p - %p (0x%016llx != 0x%016llx)\n",
                    __FUNCTION__, &pTcmBuffer[mismatchIndex], (LwU8 *)&pTcmBuffer[i] - 1,
                    pTcmBuffer[mismatchIndex], pCmpBuffer[mismatchIndex]);

                DumpMem(&pTcmBuffer[mismatchIndex], i - mismatchIndex);
                bMismatch = LW_FALSE;
            }
        }
    }

    if (bMismatch)
    {
        LOG_TEST(
            LOG_LEVEL_INFO, "%s: dst mismatch at offsets %p - %p (0x%016llx != 0x%016llx)\n", __FUNCTION__,
            &pTcmBuffer[mismatchIndex], (LwU8 *)&pTcmBuffer[i] - 1, pTcmBuffer[mismatchIndex],
            pCmpBuffer[mismatchIndex]);

        DumpMem(&pTcmBuffer[mismatchIndex], i - mismatchIndex);
        bMismatch = LW_FALSE;
    }
}

void task_test_entry()
{
    LibosStatus libosStatus;
    LwU32 i, j, k, z;
    LwU8 *pDest, *pSrc;

    construct_log_test();

    for (int super_pass = 0; super_pass < 2; super_pass++)
    {
        // fill tcm and compare buffers with pattern
        for (i = 0; i < 0x10000; i += 8)
        {
            *(LwU64 *)(TASK_RM_TEST_TCM_SRC + i)  = TASK_RM_TEST_TCM_SRC + i;
            *(LwU64 *)(compSrc + i)               = TASK_RM_TEST_TCM_SRC + i;
            *(LwU64 *)(TASK_RM_TEST_TCM_DEST + i) = TASK_RM_TEST_TCM_DEST + i;
            *(LwU64 *)(compDst + i)               = TASK_RM_TEST_TCM_DEST + i;
        }

        for (j = 0; j < 20; j++)
        {
            k = j % 10;

            LwU64 offset = (k << RM_PAGE_SHIFT);

            LwU64 tcmSrc = offset + TASK_RM_TEST_TCM_SRC;
            LwU64 tcmDst = offset + TASK_RM_TEST_TCM_DEST;
            LwU64 dmaOffset;
            LwU32 size = 8 << k;

            if (super_pass == 0)
                dmaOffset = offset + TASK_RM_TEST_DMA;
            else
                dmaOffset = offset + TASK_RM_TEST_DMA_SYS;

            LOG_AND_VALIDATE(LOG_TEST, libosDmaCopy(dmaOffset, tcmSrc, size) == LIBOS_OK);
            LOG_AND_VALIDATE(LOG_TEST, libosDmaCopy(tcmDst, dmaOffset, size) == LIBOS_OK);

            MemCopy(compDst + offset, compSrc + offset, size);
        }

        libosDmaFlush();

        CheckSrcBuffer();
        CompareDstBuffer((LwU64 *)compDst);
        LOG_TEST(LOG_LEVEL_INFO, "DMA pass %d complete\n", super_pass);
    }
}
