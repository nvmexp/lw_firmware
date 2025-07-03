/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014, 2018 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <cstddef>
#include <vector>

#include "h264decodedpicturesrc.h"

namespace H264
{
class Picture;

//! An element of Decoded Pictures Buffer (DPB). It refers to a picture by its
//! `m_picIdx`. `PicNum`, `LongTermPicNum` and `LongTermFrameIdx` are ITU-T
//! H.264 syntax elements. See sections 8.2.4.1 and 8.2.5 of ITU-T H.264.
class DPBElement
{
public:
    explicit DPBElement(std::size_t picIdx)
      : m_usedForReference(false)
      , m_isLongTerm(false)
      , m_isOutput(false)
      , PicNum(-1)
      , LongTermPicNum(-1)
      , LongTermFrameIdx(-1)
      , m_picIdx(picIdx)
    {}

    Picture* GetPicture(DecodedPictureSrc *src)
    {
        return src->GetPicture(m_picIdx);
    }

    const Picture* GetPicture(const DecodedPictureSrc *src) const
    {
        return src->GetPicture(m_picIdx);
    }

    bool IsUsedForReference() const
    {
        return m_usedForReference;
    }

    void SetUsedForReference(bool val)
    {
        m_usedForReference = val;
    }

    bool IsLongTerm() const
    {
        return m_isLongTerm;
    }

    void SetLongTerm(bool val)
    {
        m_isLongTerm = val;
    }

    bool IsOutput() const
    {
        return m_isOutput;
    }

    void SetIsOutput(bool val)
    {
        m_isOutput = val;
    }

    int GetPicNum() const
    {
        return PicNum;
    }

    void SetPicNum(int val)
    {
        PicNum = val;
    }

    int GetLongTermPicNum() const
    {
        return LongTermPicNum;
    }

    void SetLongTermPicNum(int val)
    {
        LongTermPicNum = val;
    }

    int GetLongTermFrameIdx() const
    {
        return LongTermFrameIdx;
    }

    void SetLongTermFrameIdx(int val)
    {
        LongTermFrameIdx = val;
    }

    std::size_t GetPicIdx() const
    {
        return m_picIdx;
    }

private:
    bool        m_usedForReference;
    bool        m_isLongTerm;
    bool        m_isOutput;

    int         PicNum;
    int         LongTermPicNum;
    int         LongTermFrameIdx;

    std::size_t m_picIdx;
};

//! Decoded Pictures Buffer (DPB). It doesn't contain the pictures themselves,
//! only their indices and a description of what every picture at the moment is
//! for the purpose of building the reference lists. Every picture contains an
//! instance of this buffer. This buffer represents DPB to decode the picture.
class DPB
{
public:
    typedef std::vector<DPBElement>::value_type      value_type;
    typedef std::vector<DPBElement>::size_type       size_type;
    typedef std::vector<DPBElement>::iterator        iterator;
    typedef std::vector<DPBElement>::const_iterator  const_iterator;
    typedef std::vector<DPBElement>::reference       reference;
    typedef std::vector<DPBElement>::const_reference const_reference;

    iterator begin()
    {
        return m_impl.begin();
    }

    const_iterator begin() const
    {
        return m_impl.begin();
    }

    iterator end()
    {
        return m_impl.end();
    }

    const_iterator end() const
    {
        return m_impl.end();
    }

    void clear()
    {
        m_impl.clear();
    }

    size_type size() const
    {
        return m_impl.size();
    }

    iterator erase(iterator where)
    {
        return m_impl.erase(where);
    }

    iterator erase(iterator start, iterator finish)
    {
        return m_impl.erase(start, finish);
    }

    void push_back(const value_type& v)
    {
        return m_impl.push_back(v);
    }

    bool empty() const
    {
        return m_impl.empty();
    }

    const_reference operator[](size_type pos) const
    {
        return m_impl[pos];
    }

private:
    std::vector<DPBElement> m_impl;
};

//! An abstract interface to access Decoded Pictures Buffer (DPB).
class DPBSrc
{
public:
    virtual ~DPBSrc() {}

    const DPB& GetDPB() const
    {
        return DoGetDPB();
    }

private:
    virtual const DPB& DoGetDPB() const = 0;
};

} // namespace H264
