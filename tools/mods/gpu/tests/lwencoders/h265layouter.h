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

#ifndef H265LAYOUTER_H
#define H265LAYOUTER_H

#include <vector>

#include "ctr64encryptor.h"
#include "core/include/types.h"
#include "core/include/utility.h"

#include "h265parser.h"
#include "offsetsstorage.h"

//! This class places pieces of the input data and control structures into an
//! input buffer and stores the offset of the each piece.
template <
    class WriterTraits
  , template <
        unsigned int
      , unsigned int
      > class OffsetsStoreTraits
  , class EncryptorTraits = NilEncryptor
  >
class H265Layouter
{
private:
    enum SingleOffsets
    {
        colocBufOffset,
        filterBufOffset,
        numSingleOffsets
    };

    enum MultipleOffsets
    {
        frameOffsets,
        picSetupOffsets,
        scalingMtxOffset,
        tilesInfoOffset,
        numMultipleOffsets
    };

public:
    typedef typename OffsetsStoreTraits<
        numSingleOffsets
      , numMultipleOffsets
      >::const_iterator const_iterator;

    H265Layouter()
        : m_sliceAlign(0)
        , m_controlAlign(0)
    {}

    void SetSliceAlign(UINT32 val)
    {
        m_sliceAlign = val;
    }

    void SetControlAlign(UINT32 val)
    {
        m_controlAlign = val;
    }

    void SetStartAddress(UINT08 *startPtr)
    {
        m_writer.SetStartAddress(startPtr);
        m_offsetsStore.template Clear<frameOffsets>();
        m_offsetsStore.template Clear<picSetupOffsets>();
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
        const H265::Picture::slice_const_iterator &slicesStart,
        const H265::Picture::slice_const_iterator &slicesFinish
    )
    {
        using Utility::AlignUp;

        H265::Picture::slice_const_iterator slIt;
        vector<UINT08> slicesData;
        for (slIt = slicesStart; slicesFinish != slIt; ++slIt)
        {
            H265::Slice::segment_const_iterator segIt;
            for (segIt = slIt->segments_begin(); slIt->segments_end() != segIt; ++segIt)
            {
                slicesData.insert(slicesData.end(), segIt->ebsp_begin(), segIt->ebsp_end());
            }
        }
        vector<UINT08> encSlices(
            AlignUp<EncryptorTraits::blockSize>(slicesData.size())
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

    void AddPicSetup(void const *what, size_t dataSize)
    {
        AddPicSetupOffsets(m_writer.Write(
            what, static_cast<UINT32>(dataSize), m_controlAlign
        ));
    }

    void AddScalingMtx(void const *what, size_t dataSize)
    {
        AddScalingMtxOffsets(m_writer.Write(
            what, static_cast<UINT32>(dataSize), m_controlAlign
        ));
    }

    void AddTilesInfo(void const *what, size_t dataSize)
    {
        AddTilesInfoOffsets(m_writer.Write(
            what, static_cast<UINT32>(dataSize), m_controlAlign
        ));
    }

    void AddColocBuffer(size_t bufSize)
    {
        SetColocBufOffset(m_writer.Reserve(
            static_cast<UINT32>(bufSize), m_controlAlign
        ));
    }

    void AddFilterBuffer(size_t bufSize)
    {
        SetFilterBufOffset(m_writer.Reserve(
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

    const_iterator pic_set_begin() const
    {
        return m_offsetsStore.template Begin<picSetupOffsets>();
    }

    const_iterator pic_set_end() const
    {
        return m_offsetsStore.template End<picSetupOffsets>();
    }

    const_iterator scale_mtx_begin() const
    {
        return m_offsetsStore.template Begin<scalingMtxOffset>();
    }

    const_iterator scale_mtx_end() const
    {
        return m_offsetsStore.template End<scalingMtxOffset>();
    }

    const_iterator tiles_info_begin() const
    {
        return m_offsetsStore.template Begin<tilesInfoOffset>();
    }

    const_iterator tiles_info_end() const
    {
        return m_offsetsStore.template End<tilesInfoOffset>();
    }

    UINT32 GetColocBufOffset() const
    {
        return m_offsetsStore.template Get<colocBufOffset>();
    }

    UINT32 GetFilterBufOffset() const
    {
        return m_offsetsStore.template Get<filterBufOffset>();
    }

private:
    void SetColocBufOffset(UINT32 v)
    {
        m_offsetsStore.template Set<colocBufOffset>(v);
    }

    void SetFilterBufOffset(UINT32 v)
    {
        m_offsetsStore.template Set<filterBufOffset>(v);
    }

    size_t GetFrameOffsetsSize() const
    {
        return m_offsetsStore.template Size<frameOffsets>();
    }

    void AddFrameOffsets(UINT32 v)
    {
        m_offsetsStore.template Add<frameOffsets>(v);
    }

    void AddPicSetupOffsets(UINT32 v)
    {
        m_offsetsStore.template Add<picSetupOffsets>(v);
    }

    void AddScalingMtxOffsets(UINT32 v)
    {
        m_offsetsStore.template Add<scalingMtxOffset>(v);
    }

    void AddTilesInfoOffsets(UINT32 v)
    {
        m_offsetsStore.template Add<tilesInfoOffset>(v);
    }

    OffsetsStoreTraits<
        numSingleOffsets
      , numMultipleOffsets
      >             m_offsetsStore;
    WriterTraits    m_writer;
    EncryptorTraits m_encryptor;

    UINT32          m_sliceAlign;
    UINT32          m_controlAlign;
};

#endif
