/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2011,2013,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndVertexFormat: OpenGL Randomizer for vertex data formats.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"   // declaration of our namespace
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"

using namespace GLRandom;

//------------------------------------------------------------------------------
RndPrimitiveModes::RndPrimitiveModes(GLRandomTest * pglr)
: GLRandomHelper(pglr, RND_PRIM_NUM_PICKERS, "PrimitiveModes")
{
    memset(&m_StippleBuf, 0, sizeof(m_StippleBuf));
    memset(&m_Polygon, 0, sizeof(m_Polygon));
    memset(&m_Lines, 0, sizeof(m_Lines));
    memset(&m_Points, 0, sizeof(m_Points));
}

//------------------------------------------------------------------------------
RndPrimitiveModes::~RndPrimitiveModes()
{
}

//------------------------------------------------------------------------------
RC RndPrimitiveModes::SetDefaultPicker(int picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_PRIM_POINT_SPRITE_ENABLE:
            // enable/disable point sprites
            m_Pickers[RND_PRIM_POINT_SPRITE_ENABLE].ConfigRandom();
            m_Pickers[RND_PRIM_POINT_SPRITE_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_PRIM_POINT_SPRITE_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_PRIM_POINT_SPRITE_ENABLE].CompileRandom();
            break;

        case RND_PRIM_POINT_SPRITE_R_MODE:
            // R texture coordinate mode for point sprites
            m_Pickers[RND_PRIM_POINT_SPRITE_R_MODE].ConfigRandom();
            m_Pickers[RND_PRIM_POINT_SPRITE_R_MODE].AddRandItem(1, GL_ZERO);
            m_Pickers[RND_PRIM_POINT_SPRITE_R_MODE].AddRandItem(1, GL_S);
            m_Pickers[RND_PRIM_POINT_SPRITE_R_MODE].AddRandItem(1, GL_R);
            m_Pickers[RND_PRIM_POINT_SPRITE_R_MODE].CompileRandom();
            break;

        case RND_PRIM_POINT_SIZE:
            // point size (float)
            m_Pickers[RND_PRIM_POINT_SIZE].FConfigRandom();
            m_Pickers[RND_PRIM_POINT_SIZE].FAddRandItem(4, 1.0f);
            m_Pickers[RND_PRIM_POINT_SIZE].FAddRandRange(1, 1.0f, 5.0f);
            m_Pickers[RND_PRIM_POINT_SIZE].FAddRandRange(1, 5.0f, 50.0f);
            m_Pickers[RND_PRIM_POINT_SIZE].CompileRandom();
            break;

        case RND_PRIM_POINT_ATTEN_ENABLE:
            // enable the point parameters extension for distance attenuation of points
            m_Pickers[RND_PRIM_POINT_ATTEN_ENABLE].ConfigRandom();
            m_Pickers[RND_PRIM_POINT_ATTEN_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_PRIM_POINT_ATTEN_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_PRIM_POINT_ATTEN_ENABLE].CompileRandom();
            break;

        case RND_PRIM_POINT_SIZE_MIN:
            // min size of point that is distance-shrunk (EXT_point_parameters)
            m_Pickers[RND_PRIM_POINT_SIZE_MIN].FConfigRandom();
            m_Pickers[RND_PRIM_POINT_SIZE_MIN].FAddRandRange(1, 0.5, 4.0);
            m_Pickers[RND_PRIM_POINT_SIZE_MIN].CompileRandom();
            break;

        case RND_PRIM_POINT_THRESHOLD:
            // size of point at which it starts alpha fading (EXT_point_parameters)
            m_Pickers[RND_PRIM_POINT_THRESHOLD].FConfigRandom();
            m_Pickers[RND_PRIM_POINT_THRESHOLD].FAddRandRange(1, 1.0, 10.0);
            m_Pickers[RND_PRIM_POINT_THRESHOLD].CompileRandom();
            break;

        case RND_PRIM_LINE_SMOOTH_ENABLE:
            // enable/not line anti-aliasing
            m_Pickers[RND_PRIM_LINE_SMOOTH_ENABLE].ConfigRandom();
            m_Pickers[RND_PRIM_LINE_SMOOTH_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_PRIM_LINE_SMOOTH_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_PRIM_LINE_SMOOTH_ENABLE].CompileRandom();
            break;

        case RND_PRIM_LINE_WIDTH:
            // line width in pixels (float)
            m_Pickers[RND_PRIM_LINE_WIDTH].FConfigRandom();
            m_Pickers[RND_PRIM_LINE_WIDTH].FAddRandItem(9, 1.0f);
            m_Pickers[RND_PRIM_LINE_WIDTH].FAddRandRange(1, 1.0f, 10.0f);
            m_Pickers[RND_PRIM_LINE_WIDTH].CompileRandom();
            break;

        case RND_PRIM_LINE_STIPPLE_ENABLE:
            // enable/not line stippling
            m_Pickers[RND_PRIM_LINE_STIPPLE_ENABLE].ConfigRandom();
            m_Pickers[RND_PRIM_LINE_STIPPLE_ENABLE].AddRandItem(80, GL_FALSE);
            m_Pickers[RND_PRIM_LINE_STIPPLE_ENABLE].AddRandItem(20, GL_TRUE);
            m_Pickers[RND_PRIM_LINE_STIPPLE_ENABLE].CompileRandom();
            break;

        case RND_PRIM_LINE_STIPPLE_REPEAT:
            // enable/not line stippling
            m_Pickers[RND_PRIM_LINE_STIPPLE_REPEAT].ConfigRandom();
            m_Pickers[RND_PRIM_LINE_STIPPLE_REPEAT].AddRandItem (50,   1);
            m_Pickers[RND_PRIM_LINE_STIPPLE_REPEAT].AddRandRange(30,   1,  10);
            m_Pickers[RND_PRIM_LINE_STIPPLE_REPEAT].AddRandRange(10,  10,  50);
            m_Pickers[RND_PRIM_LINE_STIPPLE_REPEAT].AddRandRange( 5,  50, 100);
            m_Pickers[RND_PRIM_LINE_STIPPLE_REPEAT].AddRandRange( 5, 100, 256);
            m_Pickers[RND_PRIM_LINE_STIPPLE_REPEAT].CompileRandom();
            break;

        case RND_PRIM_POLY_MODE:
            // glPolygonMode() one of point, line, or fill
            m_Pickers[RND_PRIM_POLY_MODE].ConfigRandom();
            m_Pickers[RND_PRIM_POLY_MODE].AddRandItem(197, GL_FILL);
            m_Pickers[RND_PRIM_POLY_MODE].AddRandItem(2,  GL_LINE);
            m_Pickers[RND_PRIM_POLY_MODE].AddRandItem(1,  GL_POINT);
            m_Pickers[RND_PRIM_POLY_MODE].CompileRandom();
            break;

        case RND_PRIM_POLY_STIPPLE_ENABLE:
            // enable/not polygon stippling
            m_Pickers[RND_PRIM_POLY_STIPPLE_ENABLE].ConfigRandom();
            m_Pickers[RND_PRIM_POLY_STIPPLE_ENABLE].AddRandItem(98, GL_FALSE);
            m_Pickers[RND_PRIM_POLY_STIPPLE_ENABLE].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_PRIM_POLY_STIPPLE_ENABLE].CompileRandom();
            break;

        case RND_PRIM_POLY_SMOOTH_ENABLE:
            // do/don't AAlias polygon edges
            m_Pickers[RND_PRIM_POLY_SMOOTH_ENABLE].ConfigRandom();
            m_Pickers[RND_PRIM_POLY_SMOOTH_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_PRIM_POLY_SMOOTH_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_PRIM_POLY_SMOOTH_ENABLE].CompileRandom();
            break;

        case RND_PRIM_POLY_OFFSET_ENABLE:
            // enable/disable polygon Z offset
            m_Pickers[RND_PRIM_POLY_OFFSET_ENABLE].ConfigRandom();
            m_Pickers[RND_PRIM_POLY_OFFSET_ENABLE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_PRIM_POLY_OFFSET_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_PRIM_POLY_OFFSET_ENABLE].CompileRandom();
            break;

        case RND_PRIM_POLY_OFFSET_FACTOR:
            m_Pickers[RND_PRIM_POLY_OFFSET_FACTOR].FConfigRandom();
            m_Pickers[RND_PRIM_POLY_OFFSET_FACTOR].FAddRandRange(40, -10.0f, 10.0f);
            m_Pickers[RND_PRIM_POLY_OFFSET_FACTOR].FAddRandItem (20, -1.0f);
            m_Pickers[RND_PRIM_POLY_OFFSET_FACTOR].FAddRandItem (20,  0.0f);
            m_Pickers[RND_PRIM_POLY_OFFSET_FACTOR].FAddRandItem (20,  1.0f);
            m_Pickers[RND_PRIM_POLY_OFFSET_FACTOR].CompileRandom();
            break;

        case RND_PRIM_POLY_OFFSET_UNITS:
            m_Pickers[RND_PRIM_POLY_OFFSET_UNITS].FConfigRandom();
            m_Pickers[RND_PRIM_POLY_OFFSET_UNITS].FAddRandRange(40, -1000.0f, 1000.0f);
            m_Pickers[RND_PRIM_POLY_OFFSET_UNITS].FAddRandItem (20, -1.0f);
            m_Pickers[RND_PRIM_POLY_OFFSET_UNITS].FAddRandItem (20,  0.0f);
            m_Pickers[RND_PRIM_POLY_OFFSET_UNITS].FAddRandItem (20,  1.0f);
            m_Pickers[RND_PRIM_POLY_OFFSET_UNITS].CompileRandom();
            break;

        case RND_PRIM_STIPPLE_BYTE:
            // polygon stipple pattern
            m_Pickers[RND_PRIM_STIPPLE_BYTE].ConfigRandom();
            m_Pickers[RND_PRIM_STIPPLE_BYTE].AddRandRange(1, 0, 255);
            m_Pickers[RND_PRIM_STIPPLE_BYTE].CompileRandom();
            break;
    }
    return OK;

}

//------------------------------------------------------------------------------
void RndPrimitiveModes::Pick()
{
    m_Points.Size           = m_Pickers[RND_PRIM_POINT_SIZE].FPick();
    m_Points.Sprite         = m_Pickers[RND_PRIM_POINT_SPRITE_ENABLE].Pick();
    m_Points.SpriteRMode    = m_Pickers[RND_PRIM_POINT_SPRITE_R_MODE].Pick();
    m_Points.MinSize        = m_Pickers[RND_PRIM_POINT_SIZE_MIN].FPick();
    m_Points.FadeThreshold  = m_Pickers[RND_PRIM_POINT_THRESHOLD].FPick();
    m_Points.EnableAttenuation = m_Pickers[RND_PRIM_POINT_ATTEN_ENABLE].Pick();

    if ( ! m_pGLRandom->HasExt(GLRandomTest::ExtLW_point_sprite) )
    {
        m_Points.Sprite = GL_FALSE;
        m_Points.SpriteRMode = GL_ZERO;
    }
    else if (m_pGLRandom->GetBoundGpuSubdevice()->HasBug(394858))
    {
        // Before we make PointSprites depend on "x" or "y" components
        // of tex-coords as with the GL_S or GL_R mode check that the components
        // are really available as inputs. This prevents bugs 2582077 and
        // 394858.
        const UINT32 components = m_pGLRandom->m_GpuPrograms.ComponentsReadByAllTxCoords();
        switch (m_Points.SpriteRMode)
        {
            case GL_S:
                if ( (components & compX) == 0)
                {
                    m_Points.SpriteRMode = GL_ZERO;
                }
                break;
            case GL_R:
                if (( components & compY) == 0)
                {
                    m_Points.SpriteRMode = GL_ZERO;
                }
                break;
            case GL_ZERO:
                break;
            default:
                MASSERT(!"Unrecognized SpriteRMode value");
        }
    }

    if ( ! m_pGLRandom->HasExt(GLRandomTest::ExtEXT_point_parameters) )
    {
        m_Points.EnableAttenuation = GL_FALSE;
    }

    m_Lines.Width         = m_Pickers[RND_PRIM_LINE_WIDTH].FPick();
    m_Lines.Smooth        = m_Pickers[RND_PRIM_LINE_SMOOTH_ENABLE].Pick();
    m_Lines.Stipple       = m_Pickers[RND_PRIM_LINE_STIPPLE_ENABLE].Pick();
    m_Lines.StippleRepeat = m_Pickers[RND_PRIM_LINE_STIPPLE_REPEAT].Pick();

    m_Polygon.FrontMode    = m_Pickers[RND_PRIM_POLY_MODE].Pick();
    m_Polygon.BackMode     = m_Pickers[RND_PRIM_POLY_MODE].Pick();
    m_Polygon.Smooth       = m_Pickers[RND_PRIM_POLY_SMOOTH_ENABLE].Pick();
    m_Polygon.Stipple      = m_Pickers[RND_PRIM_POLY_STIPPLE_ENABLE].Pick();
    m_Polygon.OffsetFill   = m_Pickers[RND_PRIM_POLY_OFFSET_ENABLE].Pick();
    m_Polygon.OffsetLine   = m_Pickers[RND_PRIM_POLY_OFFSET_ENABLE].Pick();
    m_Polygon.OffsetPoint  = m_Pickers[RND_PRIM_POLY_OFFSET_ENABLE].Pick();
    m_Polygon.OffsetFactor = m_Pickers[RND_PRIM_POLY_OFFSET_FACTOR].FPick();
    m_Polygon.OffsetUnits  = m_Pickers[RND_PRIM_POLY_OFFSET_UNITS].FPick();

    PickBitmap(m_StippleBuf, 32, 32);

    // Special Transform feedback considerations
    if ( m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) )
    {
        // Point attenuation callwlations are done in Eye Coordinates and
        // XFB captures clip coordinates (after the projection matrix calcs).
        // The only way this would work is if we set the projection matrix to
        // identity in the first pass. So disable this feature until we can
        // get programmable shaders to compensate for this inconsistency.
        m_Points.EnableAttenuation = GL_FALSE;
    }

    if (m_pGLRandom->m_GpuPrograms.ExpectedPrim() == GL_POLYGON &&
        m_pGLRandom->GetBoundGpuSubdevice()->HasBug(750106))
    {
        // Fermi HW bug: when drawing a GL_POLYGON in POINT mode, with
        // program-controlled point size, and a point is lwlled,
        // setup may incorrectly draw a large point.
        // The image varies depending on TPC count.
        // Supposed to be fixed in Kepler, bug 749900.

        if (m_Polygon.FrontMode == GL_POINT)
            m_Polygon.FrontMode = GL_FILL;
        if (m_Polygon.BackMode == GL_POINT)
            m_Polygon.BackMode = GL_FILL;
    }

    m_pGLRandom->LogData(RND_PRIM, &m_Polygon, sizeof(m_Polygon));
    m_pGLRandom->LogData(RND_PRIM, &m_Lines, sizeof(m_Lines));
    m_pGLRandom->LogData(RND_PRIM, &m_Points, sizeof(m_Points));

}

//------------------------------------------------------------------------------
void RndPrimitiveModes::Print(Tee::Priority pri)
{
    // Only print when we are drawing vertexes, not pixels.
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return;

    Printf(pri, "Polygon: %s/%s%s%s%s%s%s",
         m_Polygon.FrontMode == GL_POINT ? "GL_POINT" :
            m_Polygon.FrontMode == GL_LINE ? "GL_LINE" :
               "GL_FILL",
         m_Polygon.BackMode == GL_POINT ? "GL_POINT" :
            m_Polygon.BackMode == GL_LINE ? "GL_LINE" :
               "GL_FILL",
         m_Polygon.Smooth ? " smooth" : "",
         m_Polygon.Stipple ? " stipple" : "",
         m_Polygon.OffsetFill ? " offsetfill" : "",
         m_Polygon.OffsetLine ? " offsetline" : "",
         m_Polygon.OffsetPoint ? " offsetpoint" : "" );

    Printf(pri, " Lines: width %3.1f%s%s",
         m_Lines.Width,
         m_Lines.Smooth ? " smooth" : "",
         m_Lines.Stipple ? " stipple" : "" );

    Printf(pri, " Points: sz %3.1f%s",
         m_Points.Size,
         m_Points.Sprite ? " Sprite" : "" );

    if ( m_Points.EnableAttenuation )
    {
        Printf(pri, " mnsz %3.1f fade %3.1f",
            m_Points.MinSize,
            m_Points.FadeThreshold );
    }

    Printf(pri, "\n");
}

//------------------------------------------------------------------------------
RC RndPrimitiveModes::Send()
{
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return OK; //!< we are drawing pixels in ths round.

    if ( m_pGLRandom->HasExt(GLRandomTest::ExtLW_point_sprite) )
    {
        if (m_Points.Sprite)
        {
            glEnable(GL_POINT_SPRITE_LW);
            glPointParameteri(GL_POINT_SPRITE_R_MODE_LW, m_Points.SpriteRMode);
        }
    }

    if ( m_pGLRandom->HasExt(GLRandomTest::ExtEXT_point_parameters) )
    {
        if ( m_Points.EnableAttenuation )
        {
            glPointParameterf(GL_POINT_SIZE_MIN_EXT, m_Points.MinSize);
            // Leave max size alone
            glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE_EXT, m_Points.FadeThreshold);

            // alpha fade is 1 / (0.1 + dist/worldDepth + (dist*dist)/(worldDepth*WorldDepth)
            const GLfloat distAttenCoeff[] =
            {   0.1f,
                (GLfloat)(1.0/RND_GEOM_WorldDepth),
                (GLfloat)(1.0/(RND_GEOM_WorldDepth*RND_GEOM_WorldDepth))
            };

            glPointParameterfv(GL_DISTANCE_ATTENUATION_EXT, distAttenCoeff);
        }
    }
    glPointSize( m_Points.Size );

    bool isLine = false;
    bool isPoly = false;

    switch (m_pGLRandom->m_Vertexes.Primitive())
    {
        case GL_LINES:
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
            isLine = true;
            break;

        case GL_POINTS:
            break;

        default:
            if ((m_Polygon.FrontMode == GL_LINE) ||
                (m_Polygon.BackMode == GL_LINE))
            {
                isLine = true;
            }
            if ((m_Polygon.FrontMode == GL_FILL) ||
                (m_Polygon.BackMode == GL_FILL))
            {
                isPoly = true;
            }
            break;
    }

    glLineWidth( m_Lines.Width );

    // Don't set lwdqro-slowdown features unnecessarily.
    if (isLine)
    {
        if (m_Lines.Smooth)
            glEnable(GL_LINE_SMOOTH);

        if (m_Lines.Stipple)
        {
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(m_Lines.StippleRepeat, *(GLushort *)m_StippleBuf);
        }
    }

    glPolygonMode(GL_FRONT, m_Polygon.FrontMode);
    glPolygonMode(GL_BACK, m_Polygon.BackMode);

    // Don't set lwdqro-slowdown features unnecessarily.
    if (isPoly)
    {
        if (m_Polygon.Stipple)
        {
            glEnable(GL_POLYGON_STIPPLE);
            glPolygonStipple(m_StippleBuf);
        }

        if (m_Polygon.Smooth)
            glEnable(GL_POLYGON_SMOOTH);
    }

    if (m_Polygon.OffsetPoint)
        glEnable(GL_POLYGON_OFFSET_POINT);
    if (m_Polygon.OffsetLine)
        glEnable(GL_POLYGON_OFFSET_LINE);
    if (m_Polygon.OffsetFill)
        glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(m_Polygon.OffsetFactor, m_Polygon.OffsetUnits);

    return OK;
}

//------------------------------------------------------------------------------
RC RndPrimitiveModes::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Fill a GL bitmap for polygon stipple, bitmap characters, etc.
//
void RndPrimitiveModes::PickBitmap(GLubyte * buf, GLint w, GLint h)
{
    GLubyte * bufEnd = buf + w*h/8;
    while( buf < bufEnd )
    {
        *buf = static_cast<GLubyte>( m_Pickers[RND_PRIM_STIPPLE_BYTE].Pick() );
        ++buf;
    }
}

RC RndPrimitiveModes::Playback()
{
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return OK; //!< we are drawing pixels in ths round.

    if ( m_pGLRandom->HasExt(GLRandomTest::ExtEXT_point_parameters) )
    {
        if( m_Points.EnableAttenuation &&
            m_Points.MinSize < 1.0f )
        {
            glPointParameterf(GL_POINT_SIZE_MIN_EXT, 1.0F);
            // Leave max size alone
            glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE_EXT, 1.0F);

            const GLfloat distAttenCoeff[] ={ 1.0f, 0.0f, 0.0f };

            glPointParameterfv(GL_DISTANCE_ATTENUATION_EXT, distAttenCoeff);
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
// Return the state of line stippling
bool RndPrimitiveModes::LineStippleEnabled() const
{
    return (m_Lines.Stipple != 0);
}

GLenum RndPrimitiveModes::PolygonBackMode() const
{
    return m_Polygon.BackMode;
}

GLenum RndPrimitiveModes::PolygonFrontMode() const
{
    return m_Polygon.FrontMode;
}
