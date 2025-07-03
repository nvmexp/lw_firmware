/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2014,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_BITROT_H
#define INCLUDED_BITROT_H

#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif
#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif
#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

//! \file bitrot.h
//! \brief Utility for finding best-match bit-rotation of FB patterns.

class FrameBuffer;

namespace FbBitRotation
{
    //! Holds one bit-rotation.
    struct Rot
    {
        INT32   shift;  //!< Data-clocks delayed vs. original (may be negative)
        FLOAT32 corr;   //!< Correlation coefficient (1.0 for perfect match).
    };

    //! Holds best-match bit-rotation data for whole FrameBuffer interface.
    //!
    //! Size of Rots will be partitions * subpartitions * 32.
    //! Where subpartitions == lanes == channels.
    typedef vector<Rot> Rots;

    //! Given a pair of buffers (original, corrupted) and the data required to
    //! decode offsets in (corrupted) down to individual FBIO data-bits, find
    //! the best-matching data-delays per FBIO data-bit.
    //!
    //! Used for calibrating the read or write delay controls for a GDDR5
    //! ram interface.
    //!
    //! For best results, use a data-pattern that is highly self-dissimilar
    //! vs. rotated versions of itself.  I.e. an alternating {0, ~0} pattern
    //! will match perfectly at every even bit delay, and would be a very bad
    //! choice!
    //!
    //! The pattern must be a whole number of bursts long.
    //! If it is longer than one burst, it must be a multiple of
    //! partitions*channels*bursts so that each channel sees the same pattern.
    //!
    RC FindBest
    (
        FrameBuffer *  pFb,         //!< For the Decode function
        UINT64         fbOffset,    //!< Fb offset of buffer (for Decode)
        UINT32         pteKind,     //!< gpu PTE kind (for Decode)
        UINT32         pageSizeKB,  //!< page size in KB (for Decode)
        const UINT32 * pPat,        //!< Original data.
        UINT32         patLen,      //!< Num UINT32s in pattern.
        UINT32         perChanPatLen,//!< Pattern repeat length in one channel.
        const UINT32 * pBuf,        //!< Result data.
        UINT32         bufLen,      //!< Num UINT32s in buffer.
        Rots *         pRtnRots     //!< Returned results.
    );
};

#endif // INCLUDED_BITROT_H

