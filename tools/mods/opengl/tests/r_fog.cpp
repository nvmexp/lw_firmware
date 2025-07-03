/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010,2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndFog: OpenGL Randomizer for vertex data formats.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"   // declaration of our namespace
#include "core/include/massert.h"

using namespace GLRandom;

//------------------------------------------------------------------------------
RndFog::RndFog(GLRandomTest * pglr)
: GLRandomHelper(pglr, RND_FOG_NUM_PICKERS, "Fog")
{
    memset(&m_Fog, 0, sizeof(m_Fog));
}

//------------------------------------------------------------------------------
RndFog::~RndFog()
{
}

//------------------------------------------------------------------------------
RC RndFog::SetDefaultPicker(int picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_FOG_ENABLE:
            // enable/disable depth fog
            m_Pickers[RND_FOG_ENABLE].ConfigRandom();
            m_Pickers[RND_FOG_ENABLE].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_FOG_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_FOG_ENABLE].CompileRandom();
            break;

        case RND_FOG_DIST_MODE:
            // fog distance mode (eye-plane, eye-plane-abs, eye-radial)
            m_Pickers[RND_FOG_DIST_MODE].ConfigRandom();
            m_Pickers[RND_FOG_DIST_MODE].AddRandItem(1, GL_EYE_RADIAL_LW);
            m_Pickers[RND_FOG_DIST_MODE].AddRandItem(1, GL_EYE_PLANE);
            m_Pickers[RND_FOG_DIST_MODE].AddRandItem(1, GL_EYE_PLANE_ABSOLUTE_LW);
            m_Pickers[RND_FOG_DIST_MODE].CompileRandom();
            break;

        case RND_FOG_MODE:
            // fog intensity callwlation mode: linear/exp/exp2
            m_Pickers[RND_FOG_MODE].ConfigRandom();
            m_Pickers[RND_FOG_MODE].AddRandItem(1, GL_LINEAR);
            m_Pickers[RND_FOG_MODE].AddRandItem(2, GL_EXP);
            m_Pickers[RND_FOG_MODE].AddRandItem(3, GL_EXP2);
            m_Pickers[RND_FOG_MODE].CompileRandom();
            break;

        case RND_FOG_DENSITY:
            // coefficient for exp/exp2 modes
            m_Pickers[RND_FOG_DENSITY].FConfigRandom();
            m_Pickers[RND_FOG_DENSITY].FAddRandItem(2, 1.0f/(2.0f*RND_GEOM_WorldDepth));
            m_Pickers[RND_FOG_DENSITY].FAddRandRange(1, 1.0f/(0.9f*RND_GEOM_WorldDepth), 1.0f/(6.0f*RND_GEOM_WorldDepth));
            m_Pickers[RND_FOG_DENSITY].CompileRandom();
            break;

        case RND_FOG_START:
            // starting eye-space Z depth for linear mode
            m_Pickers[RND_FOG_START].FConfigRandom();
            m_Pickers[RND_FOG_START].FAddRandRange(1, 0.8f*RND_GEOM_WorldNear, 1.2f*RND_GEOM_WorldNear);
            m_Pickers[RND_FOG_START].CompileRandom();
            break;

        case RND_FOG_END:
            // ending eye-space Z depth for linear mode
            m_Pickers[RND_FOG_END].FConfigRandom();
            m_Pickers[RND_FOG_END].FAddRandItem(2, 2.0f*RND_GEOM_WorldFar);
            m_Pickers[RND_FOG_END].FAddRandRange(1, 0.8f*RND_GEOM_WorldFar, 6.0f*RND_GEOM_WorldFar);
            m_Pickers[RND_FOG_END].CompileRandom();
            break;

        case RND_FOG_COORD:
            // where does fog distance come from?
            m_Pickers[RND_FOG_COORD].ConfigRandom();
            m_Pickers[RND_FOG_COORD].AddRandItem(1, GL_FOG_COORDINATE_EXT);  // explicit per-vertex fog coord
            m_Pickers[RND_FOG_COORD].AddRandItem(1, GL_FRAGMENT_DEPTH_EXT);
            m_Pickers[RND_FOG_COORD].CompileRandom();
            break;

        case RND_FOG_COLOR:
            // Fog color (float, called 3 times per loop)
            m_Pickers[RND_FOG_COLOR].FConfigRandom();
            m_Pickers[RND_FOG_COLOR].FAddRandRange(1, 0.0f, 1.0f);
            m_Pickers[RND_FOG_COLOR].CompileRandom();
            break;
    }
    return OK;
}

//------------------------------------------------------------------------------
void RndFog::Pick()
{
    m_Fog.Enabled = m_Pickers[RND_FOG_ENABLE].Pick();
    m_Fog.Mode    = m_Pickers[RND_FOG_MODE].Pick();
    m_Fog.Density = m_Pickers[RND_FOG_DENSITY].FPick();
    m_Fog.Start   = m_Pickers[RND_FOG_START].FPick();
    m_Fog.End     = m_Pickers[RND_FOG_END].FPick();
    m_Fog.CoordSource = m_Pickers[RND_FOG_COORD].Pick();
    m_Fog.DistMode = m_Pickers[RND_FOG_DIST_MODE].Pick();
    m_Fog.Color[0] = m_Pickers[RND_FOG_COLOR].FPick();
    m_Fog.Color[1] = m_Pickers[RND_FOG_COLOR].FPick();
    m_Fog.Color[2] = m_Pickers[RND_FOG_COLOR].FPick();
    m_Fog.Color[3] = 1.0f;

    // Fixup the picks to be self-consistent, and increase the
    // odds of something interesting showing up on the screen.

    if ( ! m_pGLRandom->HasExt(GLRandomTest::ExtLW_fog_distance) )
        m_Fog.DistMode = GL_EYE_PLANE_ABSOLUTE_LW;

    if ( ! m_pGLRandom->HasExt(GLRandomTest::ExtEXT_fog_coord) )
        m_Fog.CoordSource = GL_FRAGMENT_DEPTH_EXT;

    if ( m_Fog.DistMode == GL_EYE_PLANE )
    {
        m_Fog.Start = -m_Fog.Start;
        m_Fog.End   = -m_Fog.End;

        m_Fog.Mode = GL_LINEAR;
    }

    if ( m_Fog.Enabled != 0 && m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture))
    {
        // When fog is enabled and we are using Transfer Feedback we must
        // set the FOG_COORD_SRC to use FRAGMENT_DEPTH
        m_Fog.CoordSource = GL_FRAGMENT_DEPTH_EXT;
    }
    m_pGLRandom->LogData(RND_FOG, &m_Fog, sizeof(m_Fog));
}

//------------------------------------------------------------------------------
void RndFog::Print(Tee::Priority pri)
{
    // Only print when we are drawing vertexes, not pixels.
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return;

    if ( ! m_Fog.Enabled )
        Printf(pri, "FOG: DISABLED\nFOG color: %f, %f, %f\n",
            m_Fog.Color[0],
            m_Fog.Color[1],
            m_Fog.Color[2] );

    else
        Printf(pri, "FOG: %s %s %s  color: %f,%f,%f start:%f end:%f density:%f\n",
            StrFogMode(m_Fog.Mode),
            StrFogCoordSource(m_Fog.CoordSource),
            StrFogDistMode(m_Fog.DistMode),
            m_Fog.Color[0],
            m_Fog.Color[1],
            m_Fog.Color[2],
            m_Fog.Start,
            m_Fog.End,
            m_Fog.Density );
}

//------------------------------------------------------------------------------
RC RndFog::Send()
{
    if (m_Fog.Enabled &&
        (RND_TSTCTRL_DRAW_ACTION_vertices == m_pGLRandom->m_Misc.DrawAction()))
    {
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, m_Fog.Mode);
        glFogf(GL_FOG_START, m_Fog.Start);
        glFogf(GL_FOG_END, m_Fog.End);
        glFogf(GL_FOG_DENSITY, m_Fog.Density);

        if ( m_pGLRandom->HasExt(GLRandomTest::ExtLW_fog_distance) )
            glFogi(GL_FOG_DISTANCE_MODE_LW, m_Fog.DistMode);

        if ( m_pGLRandom->HasExt(GLRandomTest::ExtEXT_fog_coord) )
            glFogi(GL_FOG_COORDINATE_SOURCE_EXT, m_Fog.CoordSource);
    }

    // Combiner programs might use fogC w/o enabling fog, so always send it.
    glFogfv(GL_FOG_COLOR, m_Fog.Color);

    return OK;
}

//------------------------------------------------------------------------------
RC RndFog::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
const char * RndFog::StrFogMode(GLenum fm) const
{
    return mglEnumToString(fm,"IlwalidFogMode",true);
}

//------------------------------------------------------------------------------
const char * RndFog::StrFogCoordSource(GLenum fc) const
{
    return mglEnumToString(fc,"IlwalidFogCoordSource",true);
}

//------------------------------------------------------------------------------
const char * RndFog::StrFogDistMode(GLenum fdm) const
{
    return mglEnumToString(fdm,"IlwalidFogDistMode",true);
}
GLboolean RndFog::IsEnabled(void) const
{
    return (m_Fog.Enabled != 0);
}

