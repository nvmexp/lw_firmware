/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _SURFACE_ASSEMBLER_H_
#define _SURFACE_ASSEMBLER_H_

#include "mdiag/utils/types.h"
#include <vector>

class RC;
class ScanlineReader;

//! Assembles surface data from one or more rectangles from surfaces.
//!
//! Produces a new array. The produced array has identical width, height,
//! number of layers and number of arrays as the surface passed to Assemble()
//! in SurfaceReader.
class SurfaceAssembler
{
public:
    enum LineOrder
    {
        normalLineOrder,
        reverseLineOrder
    };
    //! \param lineOrder Specified whether the scanlines shall be
    //!                  reversed during assembly.
    SurfaceAssembler(LineOrder lineOrder)
        : m_MirrorY(lineOrder==reverseLineOrder) { }
    //! Rectangle coordinates specification.
    struct Rectangle
    {
        UINT32 x, y, width, height;

        //! Builds rectangle from coordinates
        Rectangle(UINT32 x, UINT32 y, UINT32 width, UINT32 height)
            : x(x), y(y), width(width), height(height) { }
        //! Builds rectangle describing whole surface
        explicit Rectangle(const ScanlineReader& surf);
        bool IsNull() const;
    };
    //! Specifies another source rectangle.
    RC AddRect(
        UINT32 subdev,          //!< Subdevice from which the rectangle will be copied.
        const Rectangle& src,   //!< Source rectangle coordinates specification.
        UINT32 x,               //!< X coordinate within the destination image.
        UINT32 y                //!< Y coordinate within the destination image.
        );
    //! Removes any previously added rectangles.
    void ClearRects() { m_Sources.clear(); }
    //! Assembles a surface image from rectangles specified with AddRect.
    //! \param reader A source of subsequent scanlines.
    RC Assemble(ScanlineReader& reader, UINT32 size = 0);
    //! Returns produced buffer size.
    UINT32 GetOutputBufSize() const { return static_cast<UINT32>(m_Buf.size()); }
    //! Returns produced buffer (constant).
    const UINT08* GetOutputBuf() const { return const_cast<UINT08*>(&m_Buf[0]); }
    //! Returns produced buffer.
    UINT08* GetOutputBuf() { return &m_Buf[0]; }
    //! Returns produced buffer - the caller gets ownership over the buffer.
    void ExtractOutputBuf(vector<UINT08>* v) { m_Buf.swap(*v); }

private:
    struct Source
    {
        UINT32 subdev;
        Rectangle src;
        UINT32 x, y;
    };
    typedef vector<Source> Sources;

    bool m_MirrorY;             //!< Indicates whether order of scanlines shall be reversed.
    Sources m_Sources;          //!< Specification of source rectangles.
    vector<UINT08> m_Buf;       //!< A buffer for assembled surface.
};

extern const SurfaceAssembler::Rectangle g_NullRectangle;
#endif // _SURFACE_ASSEMBLER_H_
