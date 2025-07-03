/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <memory>
#include <stdio.h>
#include "../utils/lwdiagutils.h"

namespace Encryptor
{
    class LogSink
    {
        public:
            LogSink();
            ~LogSink();

            LogSink(const LogSink&) = delete;
            LogSink(LogSink&&) = delete;
            LogSink& operator=(const LogSink&) = delete;
            LogSink& operator=(LogSink&&) = delete;

            enum WhatFile
            {
                NEW_FILE,
                APPEND
            };
            LwDiagUtils::EC Initialize(FILE* pFile, WhatFile what);

            void Append(const char* str, size_t size);

        private:
            struct Impl;
            std::unique_ptr<Impl> m_pImpl;
            FILE* m_pFile = nullptr;
    };
}
