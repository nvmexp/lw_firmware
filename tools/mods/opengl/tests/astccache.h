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

 /**
  * @file   astccache.h
  * @brief  Declare class AstcCache.
  *
  */

#pragma once
#ifndef INCLUDED_ASTCCACHE_H
#define INCLUDED_ASTCCACHE_H

#include "core/include/rc.h"
#include "opengl/modsgl.h"
#include "core/include/tar.h"

#include <vector>

namespace Utility
{
    struct ASTCData;
}

/**
 * @class AstcCache
 *
 * @brief Singleton for maintained read ASTC file data inbetween GLrandom runs
 *
 * The AstcCache class is used to open and read ASTC compressed image files, and
 * store their information throughout the lifetime of the application. This
 * maintains a list of known and supported ASTC image files along with
 * information on the data contained in each of those files.
 *
 * As of Sept 15, 2015, 3D textures are not yet fully supported by the ASTC
 * format, however placeholders exist for future support. Therefore, this class
 * lwrrently only supports 2D texture images.
 *
 */
class AstcCache
{
public:

    static AstcCache& GetInstance()
    {
        static AstcCache instance;
        return instance;
    }

    ~AstcCache();

    /**
     * GetData attempts to read ASTC data from its cache, provided it has been
     * previously loaded. If not, it will attempt to load the data from a .astc
     * file. This function may return an error if the provided fileId does not
     * match a known file from the m_SupFilesInfo vector, or if there was an
     * error reading a .astc file. The memory within **astcData is owned by this
     * class. In case of error, there is no guarantee on whether astcData will
     * be modified or not.
     */
    RC GetData(GLenum internalFormat, Utility::ASTCData** astcData);

    /**
     * GetImageSizes returns the width, height, and depth sizes of the
     * uncompressed image for an ASTC file that is known to the AstcCache.
     */
    RC GetImageSizes(GLenum internalFormat,
                     GLuint *width, GLuint *height, GLuint *depth) const;

private:

    /**
     * Disallow calling these operators
     */
    AstcCache();
    AstcCache(const AstcCache&);              // Copy constructor
    AstcCache& operator=(const AstcCache&);   // Copy assignment operator

    enum SupFiles {
        spFestival,
        spLwbes,
        spBird,
        spClouds,
        spSplats,
        spForest,
        spLogo,
        spSquares,
        spDrops,
        spLwrves,
        spFractals,
        spLines,
        spTieDie,
        spSmudges,
        NumSupFiles
    };

    struct SupportedFilesInfo
    {
        const char* FileName;
        UINT32 BaseImageWidth;
        UINT32 BaseImageHeight;
        UINT32 BaseImageDepth;
        UINT08 BlockX;
        UINT08 BlockY;
        UINT08 BlockZ;

        SupportedFilesInfo(const char* fileName,
                           UINT32 baseImageWidth,
                           UINT32 baseImageHeight,
                           UINT32 baseImageDepth,
                           UINT08 blockX,
                           UINT08 blockY,
                           UINT08 blockZ)
        : FileName(fileName)
        , BaseImageWidth(baseImageWidth)
        , BaseImageHeight(baseImageHeight)
        , BaseImageDepth(baseImageDepth)
        , BlockX(blockX)
        , BlockY(blockY)
        , BlockZ(blockZ)
        {}
    };

    UINT32 GetImageIndex(GLenum internalFormat) const;

    bool                         m_bAstcsInitialized;
    TarArchive                   m_Astcs;
    vector< SupportedFilesInfo > m_SupFilesInfo;
    vector< Utility::ASTCData >  m_CachedData;
};

#endif // INCLUDED_ASTC_H

