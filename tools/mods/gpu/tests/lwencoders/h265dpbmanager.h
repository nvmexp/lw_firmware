/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016,2019-2020 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef H265DPBMANAGER_H
#define H265DPBMANAGER_H

#include <algorithm>
#include <set>
#include <vector>

#include "core/include/types.h"

#include "h265parser.h"
#include "h26xparser.h"

class H265DPBManager
{
private:
    class DPBEntry
    {
    public:
        DPBEntry(UINT08 lwdecIdx, H265::PicIdx h265Idx, INT32 poc)
          : m_lwdecIdx(lwdecIdx)
          , m_h265Idx(h265Idx)
          , m_poc(poc)
          , m_usedForReference(true) // new pictures are marked as "used for short-term reference"
          , m_usedForLtReference(false)
          , m_isOutput(false)
        {}

        bool IsOutput() const
        {
            return m_isOutput;
        }

        void SetIsOutput(bool val)
        {
            m_isOutput = val;
        }

        bool IsLongTerm() const
        {
            return m_usedForLtReference;
        }

        void SetLongTerm(bool val)
        {
            m_usedForLtReference = val;
        }

        bool IsUsedForReference() const
        {
            return m_usedForReference;
        }

        void SetUsedForReference(bool val)
        {
            m_usedForReference = val;
        }

        UINT08 GetLwdecIdx() const
        {
            return m_lwdecIdx;
        }

        void SetLwdecIdx(UINT08 val)
        {
            m_lwdecIdx = val;
        }

        INT32 PicOrderCnt() const
        {
            return m_poc;
        }

        void PicOrderCnt(INT32 val)
        {
            m_poc = val;
        }

        H265::PicIdx GetPicIdx() const
        {
            return m_h265Idx;
        }

        void SetPicIdx(H265::PicIdx val)
        {
            m_h265Idx = val;
        }

    private:
        UINT08       m_lwdecIdx;
        H265::PicIdx m_h265Idx;
        INT32        m_poc;
        bool         m_usedForReference;
        bool         m_usedForLtReference;
        bool         m_isOutput;
    };

    typedef vector<DPBEntry> DPB;

public:
    H265DPBManager()
      : m_size(0),
        m_ltMask(0)
    {}

    H265DPBManager(UINT08 size) : m_ltMask(0)
    {
        SetSize(size);
    }

    void SetSize(UINT08 size)
    {
        m_size = size;
        m_surfIndicesPool.clear();
        for (UINT08 i = 0; size > i; ++i)
        {
            m_surfIndicesPool.insert(i);
        }
    }

    UINT08 GetSize() const
    {
        return m_size;
    }

    UINT16 GetLtMask() const
    {
        return m_ltMask;
    }

    //! This is the central method of this class. It searches for a free output
    //! surface to place a new decoded picture to. If all surfaces are already
    //! oclwpied with decoded pictures, it looks for surfaces that can be output
    //! and writes their indices to the supplied output iterator. Finally it
    //! returns the surface index that can be used for decoding of the new
    //! picture.
    template <class OutputIt>
    INT08 GetSlotForNewPic(const H265::Picture &pic, OutputIt outIt)
    {
        if (pic.IsIDR())
        {
            Clear(outIt);
        }

        for (DPB::iterator it = m_dpb.begin(); m_dpb.end() != it; ++it)
            it->SetUsedForReference(false);

        const H265::RefPicSet &rps = pic.GetRps();
        DPB::iterator it;
        for (UINT08 i = 0; rps.NumPocStLwrrBefore > i; ++i)
        {
            it = find_if(m_dpb.begin(), m_dpb.end(), IdxEqPred(rps.RefPicSetStLwrrBefore[i]));
            if (m_dpb.end() != it)
            {
                it->SetUsedForReference(true);
                it->SetLongTerm(false);
            }
        }
        for (UINT08 i = 0; rps.NumPocStLwrrAfter > i; ++i)
        {
            it = find_if(m_dpb.begin(), m_dpb.end(), IdxEqPred(rps.RefPicSetStLwrrAfter[i]));
            if (m_dpb.end() != it)
            {
                it->SetUsedForReference(true);
                it->SetLongTerm(false);
            }
        }
        for (UINT08 i = 0; rps.NumPocStFoll > i; ++i)
        {
            it = find_if(m_dpb.begin(), m_dpb.end(), IdxEqPred(rps.RefPicSetStFoll[i]));
            if (m_dpb.end() != it)
            {
                it->SetUsedForReference(true);
                it->SetLongTerm(false);
            }
        }
        m_ltMask = 0;
        for (UINT08 i = 0; rps.NumPocLtLwrr > i; ++i)
        {
            it = find_if(m_dpb.begin(), m_dpb.end(), IdxEqPred(rps.RefPicSetLtLwrr[i]));
            if (m_dpb.end() != it)
            {
                it->SetUsedForReference(true);
                it->SetLongTerm(true);
                m_ltMask |= 0x8000 >> it->GetLwdecIdx();
            }
        }
        for (UINT08 i = 0; rps.NumPocLtFoll > i; ++i)
        {
            it = find_if(m_dpb.begin(), m_dpb.end(), IdxEqPred(rps.RefPicSetLtFoll[i]));
            if (m_dpb.end() != it)
            {
                it->SetUsedForReference(true);
                it->SetLongTerm(true);
            }
        }

        // first remove all unused frames (were output and are not used for reference)
        if (m_surfIndicesPool.empty())
        {
            DPB::iterator it = find_if(m_dpb.begin(), m_dpb.end(), H26X::UnusedPred<DPB>());
            if (m_dpb.end() != it)
            {
                m_surfIndicesPool.insert(it->GetLwdecIdx());
                DPB::iterator newEnd = it;
                for (++it; m_dpb.end() != it; ++it)
                {
                    if (!it->IsOutput() || it->IsUsedForReference())
                    {
                        *newEnd++ = *it;
                    }
                    else
                    {
                        m_surfIndicesPool.insert(it->GetLwdecIdx());
                    }
                }
                m_dpb.erase(newEnd, m_dpb.end());
            }
        }

        // if is still empty, output frames until we can remove a picture from DPB
        while (m_surfIndicesPool.empty())
        {
            DPB::iterator minIt = FindMinPOCNotOutput();
            MASSERT(m_dpb.end() != minIt);
            *outIt++ = minIt->GetLwdecIdx();
            minIt->SetIsOutput(true);

            if (!minIt->IsUsedForReference())
            {
                m_surfIndicesPool.insert(minIt->GetLwdecIdx());
                m_dpb.erase(minIt);
            }
        }

        UINT08 lwdecIdx = *m_surfIndicesPool.begin();
        m_surfIndicesPool.erase(lwdecIdx);
        m_dpb.push_back(DPBEntry(lwdecIdx, pic.GetPicIdx(), pic.PicOrderCntVal));

        return lwdecIdx;
    }

    template <class OutputIt>
    void Clear(OutputIt outIt)
    {
        for (;;)
        {
            DPB::iterator minIt = FindMinPOCNotOutput();
            if (m_dpb.end() == minIt)
                break;
            *outIt++ = minIt->GetLwdecIdx();
            minIt->SetIsOutput(true);
        }
        m_dpb.clear();
        SetSize(m_size);
    }

    INT08 GetLwdecIdx(H265::PicIdx idx) const
    {
        DPB::const_iterator it;
        it = find_if(m_dpb.begin(), m_dpb.end(), IdxEqPred(idx));
        MASSERT(m_dpb.end() != it);

        return it->GetLwdecIdx();
    }

    pair<INT32, bool> GetPoc(UINT08 lwdecIdx) const
    {
        DPB::const_iterator it;
        it = find_if(m_dpb.begin(), m_dpb.end(), LwdecIdxEqPred(lwdecIdx));
        if (m_dpb.end() == it) return make_pair(0, false);

        return make_pair(it->PicOrderCnt(), true);
    }

private:
    DPB::iterator FindMinPOCNotOutput()
    {
        DPB::iterator minIt;
        DPB::iterator it =
            find_if(m_dpb.begin(), m_dpb.end(), not1(H26X::IsOutputPred<DPB>()));
        if (m_dpb.end() == it)
            return it;
        minIt = it;
        for (; m_dpb.end() != ++it;)
        {
            if (!it->IsOutput() && (it->PicOrderCnt() < minIt->PicOrderCnt()))
            {
                minIt = it;
            }
        }

        return minIt;
    }

    struct IdxEqPred
    {
        IdxEqPred(const H265::PicIdx &picIdx)
          : m_picIdx(picIdx)
        {}

        bool operator()(const DPBEntry &e) const
        {
            return m_picIdx == e.GetPicIdx();
        }

        const H265::PicIdx &m_picIdx;
    };

    struct LwdecIdxEqPred
    {
        LwdecIdxEqPred(const UINT08 &lwdecIdx)
          : m_lwdecIdx(lwdecIdx)
        {}

        bool operator()(const DPBEntry &e) const
        {
            return m_lwdecIdx == e.GetLwdecIdx();
        }

        const UINT08 &m_lwdecIdx;
    };

    struct ReturnIndexToPool
    {
        ReturnIndexToPool(set<UINT08> &_set)
          : m_set(_set)
        {}

        void operator()(const DPB::value_type &dpbEntry)
        {
            m_set.insert(dpbEntry.GetLwdecIdx());
        }

        set<UINT08> &m_set;
    };

    static
    bool PocOrder(const DPBEntry &e1, const DPBEntry &e2)
    {
        return e1.PicOrderCnt() < e2.PicOrderCnt();
    }

    DPB         m_dpb;
    set<UINT08> m_surfIndicesPool;
    UINT08      m_size;
    UINT16      m_ltMask;
};

#endif
