/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef H264DECODEDPICTURESRC_H
#define H264DECODEDPICTURESRC_H

#include <cstddef>

namespace H264
{
class Picture;

//! An abstract interface to access decoded pictures in whatever high level
//! class that holds them.
class DecodedPictureSrc
{
public:
    //! Gets the last decoded picture
    Picture* GetLastPicture()
    {
        return DoGetLastPicture();
    }

    //! Gets the last decoded picture
    const Picture* GetLastPicture() const
    {
        return DoGetLastPicture();
    }

    //! Gets the last decoded reference picture
    Picture* GetLastRefPicture()
    {
        return DoGetLastRefPicture();
    }

    //! Gets the last decoded reference picture
    const Picture* GetLastRefPicture() const
    {
        return DoGetLastRefPicture();
    }

    //! DPB stores some abstract indices of pictures. This function retrieves
    //! a picture by its index for DPB purposes. What this index really means is
    //! up to the implementation of this interface.
    Picture* GetPicture(std::size_t idx)
    {
        return DoGetPicture(idx);
    }

    //! DPB stores some abstract indices of pictures. This function retrieves
    //! a picture by its index for DPB purposes. What this index really means is
    //! up to the implementation of this interface.
    const Picture* GetPicture(std::size_t idx) const
    {
        return DoGetPicture(idx);
    }

private:
    virtual Picture* DoGetLastPicture() = 0;
    virtual const Picture* DoGetLastPicture() const = 0;
    virtual Picture* DoGetLastRefPicture() = 0;
    virtual const Picture* DoGetLastRefPicture() const = 0;
    virtual Picture* DoGetPicture(std::size_t idx) = 0;
    virtual const Picture* DoGetPicture(std::size_t idx) const = 0;
};

} // namespace H264

#endif
