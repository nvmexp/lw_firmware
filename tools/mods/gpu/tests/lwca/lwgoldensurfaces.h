/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011, 2016 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file glgoldensurfaces.h
//! \brief Declares the LwdaGoldenSurfaces class.

#ifndef INCLUDED_GOLDEN_H
#include "core/include/golden.h"
#endif

#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif

//------------------------------------------------------------------------------
//! \brief Mods LWCA-specific derived class of GoldenSurfaces.
//!
//! This class is a very thin wrapper for the GoldenSurfaces interface. Most of
//! the work is unique to the LwdaRandom class so it has been implemented their.
//
class LwdaGoldenSurfaces : public GoldenSurfaces
{
public:
    virtual ~LwdaGoldenSurfaces();

    //! \brief Get the starting address for a sub-surface tile
    //!
    //! Should be called after DescribeSurface.
    virtual void * GetCachedAddress(
        int SurfNum,
        Goldelwalues::BufferFetchHint bufFetchHint,
        UINT32 SubdevNum,
        vector<UINT08> *surfDumpBuffer
    );

    virtual void Ilwalidate();

    //! \brief Not used, only provided for implementation requirements
    virtual RC CheckAndReportDmaErrors(
        UINT32 SubdevNum
    );

    //! \brief Describe surfaces to be read back.
    void DescribeSurface(
        UINT32 Width,               //!< The width in elements of the surface.
        UINT32 Height,              //!< The height in elements of the surface.
        UINT32 Pitch,               //!< The pitch in bytes of the surface
        void * MemAddr,             //!< Starting address for this surface
        int    SurfNum,             //!< The psuedo surface to describe
        const char * Name           //!< The name of this surface
    );

    //! \brief Return the display for this surface
    virtual UINT32 Display(
        int SurfNum
    ) const;

    //! \brief Return the color surface format for this surface
    virtual ColorUtils::Format  Format(
        int SurfNum
    ) const;

    //! \brief Return the height of the current tile for this surface
    virtual UINT32 Height(
        int SurfNum
    ) const;

    //! \brief Return the name of the current tile for this surface
    virtual const string & Name(
        int SurfNum
    ) const;

    //! \brief Return the number of surfaces
    virtual int NumSurfaces() const;

    //! \brief Return the row pitch for this surface
    virtual INT32 Pitch(
        int SurfNum
    ) const;

    //! \brief Set the number of psuedo surfaces to generate golden values for
    virtual void SetNumSurfaces(
        int NumSurf
    );

    //! \brief Return the width of the current tile for this surface
    virtual UINT32 Width(
        int SurfNum
    ) const;

    void SetRC(int surfIdx, RC rc);
    RC GetRC(int surfIdx);

private:
    virtual RC GetPitchAlignRequirement(UINT32 *pitch)
    {
        MASSERT(pitch);

        *pitch = 1;

        return OK;
    }

    struct surface {
        UINT32      width;
        UINT32      height;
        UINT32      pitch;
        void *      memAddr;
        string      name;
        RC          rc;
    };
    vector <surface> m_Surf;

    int m_LwrSurfNum = 0;   // to keep track of the surface being checked

};

