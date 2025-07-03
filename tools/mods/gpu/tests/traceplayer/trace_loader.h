/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/fileholder.h"
#include "core/include/rc.h"
#include "core/include/tar.h"

#include <string>
#include <vector>

class Surface2D;

namespace MfgTracePlayer
{
    class SurfaceAccessor;

    class Loader
    {
        public:
            Loader() = default;
            RC SetTraceLocation(const string& path);
            RC GetFileSize(const string& filename, size_t* pSize);
            RC LoadFile(const string& filename, vector<UINT08>* pBuffer);
            RC LoadFile(const string& filename, string* pBuffer);
            RC LoadFile(const string& filename, Surface2D* pSurf);
            RC LoadFile
            (
                const string& filename,
                Surface2D* pSurf,
                SurfaceAccessor *pSurfaceAccessor
            );

            RC GetEncryptedFileSize(const string& filename, size_t* pSize);
            RC LoadEncryptedFile(const string& filename, vector<UINT08>* pBuffer);
            RC LoadEncryptedFile(const string& filename, string* pBuffer);
            RC LoadEncryptedFile(const string& filename, Surface2D* pSurf);
            RC LoadEncryptedFile
            (
                const string& filename,
                Surface2D* pSurf,
                SurfaceAccessor *pSurfaceAccessor
            );
            RC Exists(const string& filename) const;

        private:
            template<typename LoadHandler>
            RC LoadFile(const string& filename, LoadHandler& handler, bool bEncrypted);

            bool       m_bInitialized = false;
            bool       m_bTar         = false;
            string     m_Path;
            string     m_TraceBasename;
            TarArchive m_Archive;
    };
}
