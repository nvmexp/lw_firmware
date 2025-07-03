/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "SurfaceDumper.h"
#include "mdiag/utils/GZFileIO.h"

#include "mdiag/gpu/surfmgr.h"
#include "mdiag/sysspec.h"
#include <vector>

static uintptr_t castToUintptr(UINT08 *p) { return reinterpret_cast<uintptr_t>(p); }

SurfaceDumper::SurfaceDumper(
    SurfaceReader* surfaceReader,
    IGpuSurfaceMgr *surfMgr,
    const ArgReader* params) :
    m_pSurfaceReader(surfaceReader),
    m_surfMgr(surfMgr),
    m_pBuf(0),
    m_params(params)
{
}

SurfaceDumper::~SurfaceDumper()
{
}

/* Figure out output file name
   Algorithm is to use the input file directory as the "output file name".tga
   Directory is chosen by the following example:
   -i directoryfilename/test.hdr
   puts the output image in:
   IMG_A8R8G8B8.../directoryfilename.tga
   when doing crcdump where the directory where IMG is located is parallel to
   the directory of the input file.

   If not doing crcdump the output goes into the following IMG directory based
   on what is given in the "-o" option. If no -o option is given, the current
   directory is used as the start of the path
*/

RC SurfaceDumper::DumpSurface(const char *filename, MdiagSurf *surface,
        UINT08* surface_base, const SurfaceAssembler::Rectangle &rect, const char* profileKey,
        UINT32 gpuSubdevice, bool rawCrc)
{
    // The dump_images_never option overrides all other image dumping options.
    if (m_params->ParamPresent("-dump_images") &&
        m_params->ParamUnsigned("-dump_images") == 1)
    {
        return OK;
    }

    vector<UINT08> tempbuf;
    if (surface_base == 0)
    {
        RC rc = m_pSurfaceReader->ReadSurface(&tempbuf, surface, gpuSubdevice, rawCrc, rect);

        if( rc != OK ) {
            ErrPrintf("Could not read back %s buffer: %s\n",
                surface->GetName().c_str(), rc.Message());
            return rc;
        }

        surface_base = &tempbuf[0];
    }

    UINT32 depth = surface->GetBytesPerPixel();
    int width = surface->GetWidth();
    int height = surface->GetHeight()*surface->GetDepth()*surface->GetArraySize();
    UINT32 pitch = surface->GetWidth()*surface->GetBytesPerPixel();
    RasterFormat rasformat = RASTER_LW50;
    LW50Raster* pRaster = m_surfMgr->GetRaster(surface->GetColorFormat());
    if(pRaster == 0)
    {
        ErrPrintf("Failed to dump surface: %s for unsupported format: %d\n",
                  surface->GetName().c_str(), surface->GetColorFormat());
        return RC::UNSUPPORTED_COLORFORMAT;
    }

    if (!rect.IsNull())
    {
        // If a rect is used, we only need to dump the area in rect,
        // so we should adjust the dimension info
        width = rect.width;
        height = rect.height * surface->GetDepth() * surface->GetArraySize();
        pitch = rect.width * surface->GetBytesPerPixel();
    }

    int ret = m_pBuf->SaveBufTGA(filename, width, height, castToUintptr(surface_base), pitch,
        depth, CTRaster(rasformat, pRaster), 0, profileKey);

    if (!ret)
    {
        ErrPrintf("Failed to dump color tga output to %s\n", filename);
        return RC::FILE_WRITE_ERROR;
    }

    return OK;
}

RC SurfaceDumper::DumpDmaBuf(const char *filename, CheckDmaBuffer *pCheck,
        UINT08 *bufferData, bool use_gz, UINT32 gpuSubdevice,
        UINT32 chunkSize)
{
    // The dump_images_never option overrides all other image dumping options.
    if (m_params->ParamPresent("-dump_images") &&
        m_params->ParamUnsigned("-dump_images") == 1)
    {
        return OK;
    }

    RC rc = OK;
    UINT08* pDMABuffer = 0;
    UINT32 fullSize  = pCheck->GetUntiledSize();
    if (chunkSize == 0) chunkSize = fullSize;
    // pDMABuffer walks bufferData if bufferData != 0
    if (bufferData) pDMABuffer = bufferData;
    UINT32 saveOffset = pCheck->Offset;

    if (use_gz)
    {
        GZFileIO gzf;
        if (gzf.FileOpen(filename, "wb"))
            return RC::CANNOT_OPEN_FILE;
        while (fullSize > 0)
        {
            UINT32 readSize = min(fullSize, chunkSize);
            if (bufferData == 0)
            {
                rc = m_pSurfaceReader->ReadDmaBuffer(&pDMABuffer, pCheck, gpuSubdevice, readSize);
                if (rc != OK)
                {
                    ErrPrintf("Could not read back %s: %s\n",
                        pCheck->Filename.c_str(), rc.Message());
                    //delete[] pDMABuffer; // This line has been called in ReadDmaBuffer()
                                           // but I still leave it here as a reminder
                    return false;
                }
            }
            if (readSize != gzf.FileWrite(pDMABuffer, readSize, 1))
            {
                gzf.FileClose();
                return RC::FILE_WRITE_ERROR;
            }
            // pDMABuffer walks bufferData if bufferData != 0
            pDMABuffer += readSize;
            pCheck->Offset += readSize;
            fullSize -= readSize;
        }
        gzf.FileClose();
    }
    else
    {
        FILE* file = fopen(filename, "wb");
        if (!file)
            return RC::CANNOT_OPEN_FILE;

        while (fullSize > 0)
        {
            UINT32 readSize = min(fullSize, chunkSize);
            if (bufferData == 0)
            {
                rc = m_pSurfaceReader->ReadDmaBuffer(&pDMABuffer, pCheck, gpuSubdevice, readSize);
                if (rc != OK)
                {
                    ErrPrintf("Could not read back %s: %s\n",
                        pCheck->Filename.c_str(), rc.Message());
                    //delete[] pDMABuffer; // This line has been called in ReadDmaBuffer()
                                           // but I still leave it here as a reminder
                    return false;
                }
            }
            if (readSize != fwrite(pDMABuffer, sizeof(unsigned char), readSize, file))
            {
                fclose(file);
                return RC::FILE_WRITE_ERROR;
            }
            // pDMABuffer walks bufferData if bufferData != 0
            pDMABuffer += readSize;
            pCheck->Offset += readSize;
            fullSize -= readSize;
        }
        fclose(file);
    }
    pCheck->Offset = saveOffset;
    return OK;
}

