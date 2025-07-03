/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "misc.h"

void dump_hex(char *data, unsigned size, unsigned offs)
{
    int i;
    char line[256];

    for (i = 0; i < size; ++i)
    {
        char *lp = line;
        line[0] = 0;

        if (i % 16 == 0) // first line
            lp += sprintf(lp, "%8x: ", i+offs);
        
        lp += sprintf(lp, "%02x ", (unsigned)data[i] & 0xff);
        
        if (i % 16 == 15) {
            int j;
            
            lp += sprintf(lp, " ");
            for (j = i - 15; j <= i; j += 4) {
                lp += sprintf(lp, "%08x ", ((uint32_t*)data)[j/4]);
            }
            
            lp += sprintf(lp, " ");
            for (j = i - 15; j <= i; ++j) {
                if (isprint(data[j]))
                    lp += sprintf(lp, "%c", data[j]);
                else
                    lp += sprintf(lp, ".");
            }
            
            lp += sprintf(lp, "\n");
        }
        
        DBG_PRINT("%s", line);
    }
    
    if (size % 16 != 0)
        DBG_PRINT("\n");
}

LwBool imageRead(const char *filename, void **ppBuf, LwLength *pSize)
{
    LwBool ret = LW_FALSE;
    FILE *fd;
    long size;
    void *pBuf;

    if (!filename || !ppBuf || !pSize)
        return LW_FALSE;

    fd = fopen(filename, "rb");
    if (!fd)
    {
        printf("Failed opening image file: '%s'\n", filename);
        return LW_FALSE;
    }

    fseek(fd, 0L, SEEK_END);
    size = ftell(fd);
    rewind(fd);

    if (size == 0) // empty file is OK.
    {
        ret = LW_TRUE;
        goto out_fd;
    }

    if (size > 1024*1024) // Sanity check, 1MiB should be fine
    {
        printf("Image too big.\n");
        ret = LW_FALSE;
        goto out_fd;
    }

    pBuf = calloc(1, size);
    if (!pBuf)
        goto out_fd;

    if (fread(pBuf, size, 1, fd) != 1)
    {
        if (feof(fd))
            printf("File seems empty.\n");
        else if (ferror(fd))
            printf("Error: %d\n", ferror(fd));
        else
            printf("Error reading file %s.\n", filename);
        goto out_free;
    }

    *ppBuf = pBuf;
    *pSize = size;

    ret = LW_TRUE;
    goto out_fd;

out_free:
    free(pBuf);
out_fd:
    fclose(fd);
    return ret;
}

// really naive dumping
typedef struct {
    uint32_t read_ofs;
    uint32_t write_ofs;
    uint32_t buffer_size;
    uint32_t magic;
} RiscvDbgDmesgHdr;

#define RISCV_DMESG_MAGIC    0xF007BA11U

LwBool dumpDmesg(Engine *pEngine)
{
    RiscvDbgDmesgHdr hdr;

    if (!engineReadDmem(pEngine, pEngine->dmemSize - sizeof(hdr), sizeof(hdr), &hdr))
    {
        printf("Failed reading dmesg metadata.\n");
        return LW_FALSE;
    }

    if (hdr.magic != RISCV_DMESG_MAGIC)
    {
        printf("Dmesg magic mismatch.\n");
        return LW_FALSE;
    }

    if (hdr.buffer_size > pEngine->dmemSize + sizeof(hdr))
    {
        printf("Dmesg size invalid.\n");
        return LW_FALSE;
    }

    if (hdr.read_ofs > hdr.buffer_size || hdr.write_ofs > hdr.buffer_size)
    {
        printf("Dmesg offsets invalid.\n");
        return LW_FALSE;
    }

    // buffer empty
    if (hdr.read_ofs == hdr.write_ofs)
    {
        printf("Debug buffer is empty.\n");
        return LW_FALSE;
    }

    {
        char buf[hdr.buffer_size];
        LwU32 bufferBase = pEngine->dmemSize - sizeof(hdr) - hdr.buffer_size;

        memset(buf, 0, sizeof(buf));

        if (hdr.write_ofs > hdr.read_ofs)
        {
            engineReadDmem(pEngine, bufferBase + hdr.read_ofs, hdr.write_ofs - hdr.read_ofs, buf);
        } else
        {
//            TODO
        }
        printf("-----------------------------------------------\n");
        printf("%s", buf);
        printf("-----------------------------------------------\n");
    }
}
