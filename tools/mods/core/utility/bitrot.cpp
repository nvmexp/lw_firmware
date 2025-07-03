/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2014,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/bitrot.h"
#include "core/include/framebuf.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/tee.h"

//! \file bitrot.cpp
//! \brief Utility for finding best-match bit-rotation of FB patterns.

RC FbBitRotation::FindBest
(
    FrameBuffer *  pFb,         //!< For the Decode function
    UINT64         fbOffset,    //!< Fb offset of buffer (for Decode)
    UINT32         pteKind,     //!< gpu PTE kind (for Decode)
    UINT32         pageSizeKB,  //!< page size in KB (for Decode)
    const UINT32 * pPat,        //!< Original data.
    UINT32         patLen,      //!< Num UINT32s in pattern.
    UINT32         perChanPatLen, //!< Pattern repeat length in one channel.
    const UINT32 * pBuf,        //!< Result data.
    UINT32         bufLen,      //!< Num UINT32s in buffer.
    Rots *         pRtnRots     //!< Returned results.
)
{
    MASSERT(pFb);
    MASSERT(pPat && patLen);
    MASSERT(pBuf && bufLen);
    MASSERT(pRtnRots);

    const UINT32 partitions = pFb->GetFbioCount();
    const UINT32 channels   = pFb->GetSubpartitions();
    const UINT32 burstLen   = pFb->GetBurstSize() / pFb->GetAmapColumnSize();
    RC rc;

    // Pattern must be a multiple of the burst length (i.e. multiple of 8 dwords
    // on gddr5) so that it tiles properly across the different interfaces.
    //
    // If it is longer than 1 burst, it must be a multiple of
    // (bursts * channels * partitions) dwords, so that all channels see the
    // same data.

    if ((patLen % burstLen) ||
        ((patLen > burstLen) &&
         (patLen % (partitions*channels*burstLen))))
    {
        Printf(Tee::PriError,
               "%s patLen is %d, require patLen of %d or N*%d.\n",
                __FUNCTION__, patLen, burstLen, burstLen*partitions*channels);
        return RC::BAD_PARAMETER;
    }
    if ((perChanPatLen % burstLen) ||
        ((perChanPatLen > burstLen) &&
         (perChanPatLen > (patLen / (partitions*channels)))))
    {
        Printf(Tee::PriError,
               "%s perChanPatLen is %d, require %d or <= patLen/%d.\n",
                __FUNCTION__, perChanPatLen, burstLen, partitions*channels);
        return RC::BAD_PARAMETER;
    }

    const UINT32 rotations  = perChanPatLen;

    // Build per-channel patterns that can be "rotated" without mixing in data
    // from other channels.
    vector< vector<UINT32> > perChanPats(partitions*channels);

    if (patLen == burstLen)
    {
        // Pattern is one burst long, each channel gets a copy.
        for (UINT32 ichan = 0; ichan < partitions*channels; ++ichan)
        {
            perChanPats[ichan].resize(patLen);
            Platform::VirtualRd(pPat, &(perChanPats[ichan][0]),
                                patLen * sizeof(UINT32));
        }
    }
    else
    {
        // Pattern is long enough that it has different bursts for different
        // channels.  Divide the data out among the channels.
        for (UINT32 ipat = 0; ipat < patLen; ++ipat)
        {
            FbDecode decode;
            CHECK_RC(pFb->DecodeAddress(&decode,
                                        fbOffset + ipat * sizeof(UINT32),
                                        pteKind, pageSizeKB));

            const UINT32 ichan = (decode.virtualFbio * channels +
                                  decode.subpartition);
            if (perChanPats[ichan].size() < rotations)
            {
                const UINT32 x = MEM_RD32(pPat + ipat);
                perChanPats[ichan].push_back(x);
            }
        }
    }

    // Don't check the whole buffer -- one copy of the pattern for each
    // channel is sufficient and saves time.
    const UINT32 useBufLen = (static_cast<UINT32>(perChanPats[0].size()) *
                              partitions * channels);

    // Count the number of matching bits per fbio-bit, per possible rotation.
    vector<UINT32> matchCounts(partitions * channels * 32 * rotations, 0);

    // Make one pass over buf, decoding addresses and updating matchCounts.
    for (UINT32 ibuf = 0; ibuf < bufLen && ibuf < useBufLen; ++ibuf)
    {
        const UINT32 bufData = MEM_RD32(pBuf + ibuf);

        FbDecode decode;
        CHECK_RC(pFb->DecodeAddress(&decode, fbOffset + ibuf * sizeof(UINT32),
                                    pteKind, pageSizeKB));

        const UINT32 chanIdx = (decode.virtualFbio * channels +
                                decode.subpartition);
        const vector<UINT32>& chanPat = perChanPats[chanIdx];

        for (UINT32 ibit = 0; ibit < 32; ++ibit)
        {
            const UINT32 bitMask = (1u<<ibit);
            const UINT32 bufBit  = bufData & bitMask;

            const UINT32 matchBaseIdx = (chanIdx * 32 + ibit) * rotations;

            for (UINT32 irot = 0; irot < rotations; ++irot)
            {
                const UINT32 ipat = (irot + ibuf) % chanPat.size();

                if (bufBit == (bitMask & chanPat[ipat]))
                {
                    matchCounts[matchBaseIdx + irot]++;
                }
            }
        }
    }

    {
        // Zero-out our return structure.
        const Rot r = {0, 0.0};
        pRtnRots->assign(partitions * channels * 32, r);
    }

    // Now make a pass over matchCounts, finding the best-matching pattern
    // rotation for each FBIO data-bit.
    for (UINT32 ipart = 0; ipart < partitions; ++ipart)
    {
        for (UINT32 ichan = 0; ichan < channels; ++ichan)
        {
            for (UINT32 ibit = 0; ibit < 32; ++ibit)
            {
                const UINT32 matchBaseIdx =
                        (((ipart * channels + ichan) * 32) + ibit) * rotations;

                UINT32 bestMatchCount = 0;
                UINT32 bestRot        = rotations;

                for (UINT32 irot = 0; irot < rotations; ++irot)
                {
                    const UINT32 matchCount = matchCounts[matchBaseIdx + irot];
                    if (matchCount > bestMatchCount)
                    {
                        bestMatchCount = matchCount;
                        bestRot        = irot;
                    }
                }

                INT32 signedRot;
                if (bestRot > rotations/2)
                    signedRot = INT32(bestRot - rotations);
                 else
                    signedRot = INT32(bestRot);

                const Rot r = { signedRot,
                                static_cast<FLOAT32>(bestMatchCount) / patLen };
                (*pRtnRots)[(ipart * channels + ichan)*32 + ibit] = r;
            }
        }
    }
    return OK;
}
