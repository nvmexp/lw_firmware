/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013,2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_VGPU_H
#define INCLUDED_VGPU_H

#include "core/include/types.h"
#include "core/include/rc.h"

/// Aids in communication with linsim or Fmodel over the VGPU interface.
class Vgpu
{
    public:
        enum BufferType
        {
            sendBuffer,
            recvBuffer,
            msgBuffer,
            shrBuffer,
            numBuffers
        };

        Vgpu();
        ~Vgpu();

        static bool IsFSF();
        static bool IsVdk();

        /// Returns global pointer to the VGPU singleton object.
        ///
        /// The object is initialized on first invocation.
        /// If VGPU is not supported on this platform, returns null pointer.
        static Vgpu* GetPtr();
        /// Shuts down VGPU object if it was initialized.
        static void Shutdown();

        static bool IsSupported();

        bool Initialize();
        RC EscapeRead(const char* path, UINT32 index, UINT32 size, UINT32* pValue);
        RC EscapeWrite(const char* path, UINT32 index, UINT32 size, UINT32 value);
        UINT32 ReadReg(UINT32 offset)
        {
            UINT32 value = ~0U;
            ReadReg(offset, &value, sizeof(value));
            return value;
        }
        void WriteReg(UINT32 offset, UINT32 value)
        {
            WriteReg(offset, &value, sizeof(value));
        }

        void ReadBuffer(BufferType type, UINT32 offset, void* data, UINT32 size);
        void WriteBuffer(BufferType type, UINT32 offset, const void* data, UINT32 size);

    private:
        bool InitRpcRings();
        void ReadReg(UINT32 offset, void* buf, UINT32 size);
        void WriteReg(UINT32 offset, const void* buf, UINT32 size);
        void WriteHeader(UINT32 offset, UINT32 size);
        void WriteParam(UINT32 offset, UINT32 value)
        {
            WriteParam(offset, &value, sizeof(UINT32));
        }
        void WriteParam(UINT32 offset, const void* buf, UINT32 size);
        void ReadParam(UINT32 offset, void* buf, UINT32 size);
        void WriteMsg(UINT32 offset, UINT32 value);
        bool IssueAndWait();
        static string ParseTegraPlatformFile();
        static bool s_bSupported;

        bool m_bInitialized;
        void* m_Mutex;
        UINT08* m_pAperture;
        void* m_BufDesc[numBuffers];
        UINT08* m_pBuf[numBuffers];
        size_t m_BufferSize;
        UINT32 m_Sequence;
        UINT32 m_SendPut;
        bool m_bGpInRing;
};

#endif // INCLUDED_VGPU_H
