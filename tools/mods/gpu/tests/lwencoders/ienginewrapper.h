/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015, 2018 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <cstddef>
#include <string>

#include <boost/core/noncopyable.hpp>

#include "core/include/channel.h"
#include "core/include/memtypes.h"
#include "core/include/lwrm.h"
#include "core/include/rc.h"
#include "core/include/tar.h"
#include "core/include/types.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surf2d.h"

#include "ctr64encryptor.h"
#include "h264parser.h"
#include "h265parser.h"

enum class MemLayout
{
    LW12 = 0, LW14 = 1
};

enum class OutputSize
{
    _8bits, _16bits
};

enum class LwDecCodec
{
    H264, H265
};

namespace LWDEC
{
    class Engine : boost::noncopyable
    {
    public:
        INTERFACE_HEADER(Engine);

        virtual ~Engine()
        {}

        RC InitFromH264File(
            const char *fileName,
            const TarFile *tarFile,
            GpuDevice *dev,
            GpuSubdevice *subDev,
            LwRm::Handle chHandle
        )
        {
            return DoInitFromH264File(fileName, tarFile, dev, subDev, chHandle);
        }

        RC InitFromH265File(
            const char *fileName,
            const TarFile *tarFile,
            GpuDevice *dev,
            GpuSubdevice *subDev,
            LwRm::Handle chHandle
        )
        {
            return DoInitFromH265File(fileName, tarFile, dev, subDev, chHandle);
        }

        RC InitFromAnother
        (
            GpuDevice *dev,
            GpuSubdevice *subDev,
            LwRm::Handle chHandle,
            Engine &another
        )
        {
            return DoInitFromAnother(dev, subDev, chHandle, another);
        }

        RC InitOutput(GpuDevice *dev, GpuSubdevice *subDev, LwRm::Handle chHandle, FLOAT64 skTimeout)
        {
            return DoInitOutput(dev, subDev, chHandle, skTimeout);
        }

        RC UpdateInDataGoldens(GpuGoldenSurfaces *goldSurfs, Goldelwalues *goldValues)
        {
            return DoUpdateInDataGoldens(goldSurfs, goldValues);
        }

        RC UpdateOutPicsGoldens(GpuGoldenSurfaces *goldSurfs, Goldelwalues *goldValues)
        {
            return DoUpdateOutPicsGoldens(goldSurfs, goldValues);
        }

        RC UpdateHistGoldens(GpuGoldenSurfaces *goldSurfs, Goldelwalues *goldValues)
        {
            return DoUpdateHistGoldens(goldSurfs, goldValues);
        }

        RC Free()
        {
            return DoFree();
        }

        size_t GetNumPics() const
        {
            return DoGetNumPics();
        }

        bool GetEncryption() const
        {
            return DoGetEncryption();
        }

        void SetEncryption(bool encrypt)
        {
            DoSetEncryption(encrypt);
        }

        RC InitChannel()
        {
            return DoInitChannel();
        }

        RC DecodeOneFrame(FLOAT64 timeOut)
        {
            return DoDecodeOneFrame(timeOut);
        }

        void SetPrintPri(Tee::Priority val)
        {
            DoSetPrintPri(val);
        }

        RC PrintInputData()
        {
            return DoPrintInputData();
        }

        RC PrintHistogram()
        {
            return DoPrintHistogram();
        }

        string GetStreamGoldSuffix() const
        {
            return DoGetStreamGoldSuffix();
        }

        void SetStreamGoldSuffix(string val)
        {
            return DoSetStreamGoldSuffix(val);
        }

        Engine* Clone() const
        {
            return DoClone();
        }

    protected:
        Engine()
        {}

        Engine(const Engine &)
        {}

        RC SetupContentKey(GpuSubdevice *subDev, FLOAT64 skTimeout)
        {
            return DoSetupContentKey(subDev, skTimeout);
        }

        const UINT32* GetWrappedSessionKey() const
        {
            return DoGetWrappedSessionKey();
        }

        void SetWrappedSessionKey(const UINT32 wsk[AES::DW])
        {
            DoSetWrappedSessionKey(wsk);
        }

        const UINT32* GetWrappedContentKey() const
        {
            return DoGetWrappedContentKey();
        }

        void SetWrappedContentKey(const UINT32 wck[AES::DW])
        {
            DoSetWrappedContentKey(wck);
        }

        const UINT32* GetContentKey() const
        {
            return DoGetContentKey();
        }

        void SetContentKey(const UINT32 ck[AES::DW])
        {
            DoSetContentKey(ck);
        }

        const UINT32* GetInitVector() const
        {
            return DoGetInitVector();
        }

        void SetInitVector(const UINT32 iv[AES::DW])
        {
            DoSetInitVector(iv);
        }

        UINT08 GetKeyIncrement() const
        {
            return DoGetKeyIncrement();
        }

        void SetKeyIncrement(UINT08 ki)
        {
            DoSetKeyIncrement(ki);
        }

        virtual RC DoSetupContentKey(GpuSubdevice *subDev, FLOAT64 skTimeout) = 0;

        template <size_t N, size_t Prev = 4 * N>
        struct Sqrt
        {
            static const size_t next = (Prev + 4 * N / Prev) / 2;
            static const size_t Result()
            {
                return (Prev < next ? (next - Prev <= 1) : (Prev - next <= 1)) ?
                       (next + 1) / 2 : Sqrt<N, next>::Result();
            }
        };

    private:
        virtual RC     DoInitFromH264File(const char *fileName, const TarFile *tarFile, GpuDevice *dev,
                                          GpuSubdevice *subDev, LwRm::Handle chHandle) = 0;
        virtual RC     DoInitFromH265File(const char *fileName, const TarFile *tarFile, GpuDevice *dev,
                                          GpuSubdevice *subDev, LwRm::Handle chHandle) = 0;
        virtual RC     DoInitFromAnother(GpuDevice *dev, GpuSubdevice *subDev, LwRm::Handle chHandle,
                                         Engine &another) = 0;
        virtual RC     DoInitOutput(GpuDevice * dev, GpuSubdevice * subDev, LwRm::Handle chHandle,
                                    FLOAT64 skTimeout) = 0;
        virtual RC     DoUpdateInDataGoldens(GpuGoldenSurfaces * goldSurfs, Goldelwalues * goldValues) = 0;
        virtual RC     DoUpdateOutPicsGoldens(GpuGoldenSurfaces *goldSurfs, Goldelwalues *goldValues) = 0;
        virtual RC     DoUpdateHistGoldens(GpuGoldenSurfaces * goldSurfs, Goldelwalues * goldValues) = 0;
        virtual RC     DoFree() = 0;
        virtual size_t DoGetNumPics() const = 0;
        virtual bool   DoGetEncryption() const = 0;
        virtual void   DoSetEncryption(bool encrypt) = 0;
        virtual RC     DoInitChannel() = 0;
        virtual RC     DoDecodeOneFrame(FLOAT64 timeOut) = 0;
        virtual void   DoSetPrintPri(Tee::Priority val) = 0;
        virtual RC     DoPrintInputData() = 0;
        virtual RC     DoPrintHistogram() = 0;
        virtual string DoGetStreamGoldSuffix() const = 0;
        virtual void   DoSetStreamGoldSuffix(string val) = 0;

        virtual const UINT32* DoGetWrappedSessionKey() const = 0;
        virtual void          DoSetWrappedSessionKey(const UINT32 wsk[AES::DW]) = 0;
        virtual const UINT32* DoGetWrappedContentKey() const = 0;
        virtual void          DoSetWrappedContentKey(const UINT32 wck[AES::DW]) = 0;
        virtual const UINT32* DoGetContentKey() const = 0;
        virtual void          DoSetContentKey(const UINT32 ck[AES::DW]) = 0;
        virtual const UINT32* DoGetInitVector() const = 0;
        virtual void          DoSetInitVector(const UINT32 iv[AES::DW]) = 0;
        virtual UINT08        DoGetKeyIncrement() const = 0;
        virtual void          DoSetKeyIncrement(UINT08 ki) = 0;

        virtual Engine* DoClone() const = 0;
    };

    inline
    Engine* new_clone(const Engine& a)
    {
        return a.Clone();
    }

    class Saver
    {
    public:
        INTERFACE_HEADER(Saver);

        virtual ~Saver()
        {}

        RC SaveFrames(const string &nameTemplate, UINT32 instNum)
        {
            return DoSaveFrames(nameTemplate, instNum);
        }

    private:
        virtual RC DoSaveFrames(const string &nameTemplate, UINT32 instNum) = 0;
    };
}
