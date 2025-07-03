/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015, 2017 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef H264LAYOUTER_H
#define H264LAYOUTER_H

#include <vector>

#include "ctr64encryptor.h"
#include "core/include/types.h"
#include "core/include/utility.h"

#include "h264parser.h"
#include "offsetsstorage.h"

//! This class places pieces of the input data and control structures into an
//! input buffer and stores the offset of the each piece.
template <
    class WriterTraits
  , template <
        unsigned int
      , unsigned int
      > class OffsetsStoreTraits
  , class EncryptorTraits
  >
class H264Layouter
{
private:
    enum SingleOffsets
    {
        endOfStreamOffset,
        historyOffset,
        colocOffset,
        mbHistOffset,
        numSingleOffsets
    };

    enum MultipleOffsets
    {
        frameOffsets,
        sliceOffsets,
        picSetupOffsets,
        numMultipleOffsets
    };

public:
    typedef typename OffsetsStoreTraits<
        numSingleOffsets
      , numMultipleOffsets
      >::const_iterator const_iterator;

    H264Layouter()
      : m_sliceAlign(0)
      , m_controlAlign(0)
    {}

    void SetStartAddress(UINT08 *startPtr)
    {
        m_writer.SetStartAddress(startPtr);
        m_offsetsStore.template Clear<frameOffsets>();
        m_offsetsStore.template Clear<picSetupOffsets>();
    }

    void SetSliceAlign(UINT32 val)
    {
        m_sliceAlign = val;
    }

    void SetControlAlign(UINT32 val)
    {
        m_controlAlign = val;
    }

    void SetContentKey(const UINT32 key[AES::DW])
    {
        m_encryptor.SetContentKey(key);
    }

    const UINT32* GetContentKey() const
    {
        return m_encryptor.GetContentKey();
    }

    void SetInitializatiolwector(const UINT32 iv[AES::DW])
    {
        m_encryptor.SetInitializatiolwector(iv);
    }

    const UINT32* GetInitializatiolwector() const
    {
        return m_encryptor.GetInitializatiolwector();
    }

    UINT32 AddPicture(
        const H264::Picture::slice_const_iterator &slicesStart,
        const H264::Picture::slice_const_iterator &slicesFinish
    )
    {
        H264::Picture::slice_const_iterator slIt;
        vector<UINT08> slicesData;
        for (slIt = slicesStart; slicesFinish != slIt; ++slIt)
        {
            const UINT08 *start = slIt->GetEbspPayload();
            const UINT08 *finish = start + slIt->GetEbspSize();

            slicesData.insert(slicesData.end(), start, finish);
        }

        vector<UINT08> encSlices(
            Utility::AlignUp<EncryptorTraits::blockSize>(slicesData.size())
        );
        m_encryptor.StartOver();
        m_encryptor.Encrypt(
            &slicesData[0],
            &encSlices[0],
            static_cast<UINT32>(slicesData.size())
        );

        AddFrameOffsets(m_writer.Write(
            &encSlices[0],
            static_cast<UINT32>(encSlices.size()),
            m_sliceAlign
        ));

        return static_cast<UINT32>(encSlices.size());
    }

    void AddSliceOffsets(void const *what, size_t dataSize)
    {
        AddSliceOffsets(m_writer.Write(
            what, static_cast<UINT32>(dataSize), m_controlAlign
        ));
    }

    void AddPicSetup(void const *what, size_t dataSize)
    {
        AddPicSetupOffsets(m_writer.Write(
            what, static_cast<UINT32>(dataSize), m_controlAlign
        ));
    }

    void AddHistBuffer(size_t bufSize)
    {
        SetHistBufOffset(m_writer.Reserve(
            static_cast<UINT32>(bufSize), m_controlAlign
        ));
    }

    void AddColocBuffer(size_t bufSize)
    {
        SetColocBufOffset(m_writer.Reserve(
            static_cast<UINT32>(bufSize), m_controlAlign
        ));
    }

    void AddMBHistBuffer(size_t bufSize)
    {
        SetMBHistBufOffset(m_writer.Reserve(
            static_cast<UINT32>(bufSize), m_controlAlign
        ));
    }

    UINT32 SizeInBytes() const
    {
        return m_writer.GetOffset();
    }

    const_iterator frames_begin() const
    {
        return m_offsetsStore.template Begin<frameOffsets>();
    }

    const_iterator frames_end() const
    {
        return m_offsetsStore.template End<frameOffsets>();
    }

    const_iterator sliceofs_begin() const
    {
        return m_offsetsStore.template Begin<sliceOffsets>();
    }

    const_iterator sliceofs_end() const
    {
        return m_offsetsStore.template End<sliceOffsets>();
    }

    const_iterator pic_set_begin() const
    {
        return m_offsetsStore.template Begin<picSetupOffsets>();
    }

    const_iterator pic_set_end() const
    {
        return m_offsetsStore.template End<picSetupOffsets>();
    }

    UINT32 GetHistBufOffset() const
    {
        return m_offsetsStore.template Get<historyOffset>();
    }

    UINT32 GetColocBufOffset() const
    {
        return m_offsetsStore.template Get<colocOffset>();
    }

    UINT32 GetMBHistBufOffset() const
    {
        return m_offsetsStore.template Get<mbHistOffset>();
    }

private:
    void SetEOSOffset(UINT32 v)
    {
        m_offsetsStore.template Set<endOfStreamOffset>(v);
    }

    void SetHistBufOffset(UINT32 v)
    {
        m_offsetsStore.template Set<historyOffset>(v);
    }

    void SetColocBufOffset(UINT32 v)
    {
        m_offsetsStore.template Set<colocOffset>(v);
    }

    void SetMBHistBufOffset(UINT32 v)
    {
        m_offsetsStore.template Set<mbHistOffset>(v);
    }

    size_t GetFrameOffsetsSize() const
    {
        return m_offsetsStore.template Size<frameOffsets>();
    }

    void AddFrameOffsets(UINT32 v)
    {
        m_offsetsStore.template Add<frameOffsets>(v);
    }

    void AddSliceOffsets(UINT32 v)
    {
        m_offsetsStore.template Add<sliceOffsets>(v);
    }

    void AddPicSetupOffsets(UINT32 v)
    {
        m_offsetsStore.template Add<picSetupOffsets>(v);
    }

    OffsetsStoreTraits<
        numSingleOffsets
      , numMultipleOffsets
      >             m_offsetsStore;
    WriterTraits    m_writer;
    EncryptorTraits m_encryptor;

    UINT32 m_sliceAlign;
    UINT32 m_controlAlign;
};

#endif
