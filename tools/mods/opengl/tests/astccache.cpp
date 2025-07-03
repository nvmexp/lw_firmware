/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "astccache.h"
#include "opengl/modsgl.h"
#include "core/include/utility.h"

AstcCache::AstcCache()
: m_bAstcsInitialized(false)
{
    m_SupFilesInfo.reserve(NumSupFiles);
    m_SupFilesInfo.push_back(SupportedFilesInfo("festival.astc", 516,  344,  1, 4,  4,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("lwbes.astc",    540,  360,  1, 5,  4,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("bird.astc",     800,  534,  1, 5,  5,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("clouds.astc",   1000, 452,  1, 6,  5,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("splats.astc",   800,  600,  1, 6,  6,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("forest.astc",   900,  600,  1, 8,  5,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("logo.astc",     1024, 1024, 1, 8,  6,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("squares.astc",  1280, 1024, 1, 8,  8,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("drops.astc",    1024, 1024, 1, 10, 5,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("lwrves.astc",   1024, 1024, 1, 10, 6,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("fractals.astc", 1600, 1200, 1, 10, 8,  1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("lines.astc",    1920, 1080, 1, 10, 10, 1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("tiedie.astc",   2048, 1024, 1, 12, 10, 1));
    m_SupFilesInfo.push_back(SupportedFilesInfo("smudges.astc",  1920, 1200, 1, 12, 12, 1));
    MASSERT(m_SupFilesInfo.size() == NumSupFiles);

    m_CachedData.resize(NumSupFiles);
}

//------------------------------------------------------------------------------
AstcCache::~AstcCache()
{
}

//------------------------------------------------------------------------------
RC AstcCache::GetData(GLenum internalFormat, Utility::ASTCData** ppAstcData)
{
    RC rc;

    MASSERT(ppAstcData);

    UINT32 idx = GetImageIndex(internalFormat);
    if(idx > NumSupFiles)
    {
        Printf(Tee::PriError, "Trying to load an unsupported ASTC file %u!\n", internalFormat);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    Utility::ASTCData *pAstcData = &m_CachedData[idx];

    // If the cached data is empty, read it from an .astc compressed file
    if (pAstcData->imageData.empty())
    {
        if (!m_bAstcsInitialized)
        {
            const char*  astcBin  = "astc.bin";
            const string fullPath = Utility::DefaultFindFile(astcBin, true);
            if (!m_Astcs.ReadFromFile(fullPath, true))
            {
                Printf(Tee::PriError, "Failed to load %s\n", astcBin);
                return RC::FILE_DOES_NOT_EXIST;
            }
            m_bAstcsInitialized = true;
        }

        const string&        filename = m_SupFilesInfo[idx].FileName;
        const TarFile* const pTarFile = m_Astcs.Find(filename);
        const unsigned       size     = pTarFile ? pTarFile->GetSize() : 0;
        if (!size)
        {
            Printf(Tee::PriError, "ASTC file %s not found", filename.c_str());
            return RC::FILE_DOES_NOT_EXIST;
        }

        vector<char> buf(size, 0);
        pTarFile->Seek(0);
        const unsigned numRead = pTarFile->Read(&buf[0], size);
        if (numRead != size)
        {
            Printf(Tee::PriError, "Failed to read ASTC file %s", filename.c_str());
            return RC::FILE_READ_ERROR;
        }

        CHECK_RC(Utility::ReadAstcFile(buf, pAstcData));
        if (pAstcData->imageData.empty() ||
            pAstcData->imageWidth != m_SupFilesInfo[idx].BaseImageWidth ||
            pAstcData->imageHeight != m_SupFilesInfo[idx].BaseImageHeight ||
            pAstcData->imageDepth != m_SupFilesInfo[idx].BaseImageDepth )
        {
            Printf(Tee::PriError,
                   "Astc file data does not match expected data! size=%zu, w=%u, h=%u, d=%u\n",
                   pAstcData->imageData.size(),
                   pAstcData->imageWidth,
                   pAstcData->imageHeight,
                   pAstcData->imageDepth);

            rc = RC::SOFTWARE_ERROR;
            return rc;
        }
    }

    *ppAstcData = pAstcData;

    return rc;
}

//------------------------------------------------------------------------------
RC AstcCache::GetImageSizes
(
    GLenum internalFormat,
    GLuint *width,
    GLuint *height,
    GLuint *depth
) const
{
    MASSERT(width);
    MASSERT(height);
    MASSERT(depth);

    UINT32 idx = GetImageIndex(internalFormat);
    if(idx > NumSupFiles)
    {
        return RC::SOFTWARE_ERROR;
    }

    *width = m_SupFilesInfo[idx].BaseImageWidth;
    *height = m_SupFilesInfo[idx].BaseImageHeight;
    *depth = m_SupFilesInfo[idx].BaseImageDepth;

    return OK;
}

//------------------------------------------------------------------------------
UINT32 AstcCache::GetImageIndex(GLenum internalFormat) const
{
    switch(internalFormat)
    {
        case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
            return spFestival;

        case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
            return spLwbes;

        case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
            return spBird;

        case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
            return spClouds;

        case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
            return spSplats;

        case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
            return spForest;

        case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
            return spLogo;

        case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
            return spSquares;

        case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
            return spDrops;

        case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
            return spLwrves;

        case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
            return spFractals;

        case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
            return spLines;

        case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
            return spTieDie;

        case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
            return spSmudges;

        default:
            Printf(Tee::PriError, "Trying to query an unsupported ASTC format %u!\n", internalFormat);
            MASSERT(0);
    }

    return _UINT32_MAX;
}

