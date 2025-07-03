/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010,2013,2016,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndLighting         GL state randomizer for traditional light & material stuff.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"   // declaration of our namespace
#include "core/include/massert.h"
#include "random.h"
#include <math.h>

using namespace GLRandom;

//------------------------------------------------------------------------------
RndLighting::RndLighting(GLRandomTest * pglr)
: GLRandomHelper(pglr, RND_LIGHT_NUM_PICKERS, "Lighting")
{
    memset(&m_LightSources, 0, sizeof(m_LightSources));
    memset(&m_Lighting, 0, sizeof(m_Lighting));
}

//------------------------------------------------------------------------------
RndLighting::~RndLighting()
{
}

//------------------------------------------------------------------------------
RC RndLighting::SetDefaultPicker(int picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_LIGHT_ENABLE:
            // enable/not lighting
            m_Pickers[RND_LIGHT_ENABLE].ConfigRandom();
            m_Pickers[RND_LIGHT_ENABLE].AddRandItem(19, GL_TRUE);
            m_Pickers[RND_LIGHT_ENABLE].AddRandItem( 1, GL_FALSE);
            m_Pickers[RND_LIGHT_ENABLE].CompileRandom();
            break;

        case RND_LIGHT_SHADE_MODEL:
            // glShadeModel()
            m_Pickers[RND_LIGHT_SHADE_MODEL].ConfigRandom();
            m_Pickers[RND_LIGHT_SHADE_MODEL].AddRandItem( 19, GL_SMOOTH );
            m_Pickers[RND_LIGHT_SHADE_MODEL].AddRandItem(  1, GL_FLAT );
            m_Pickers[RND_LIGHT_SHADE_MODEL].CompileRandom();
            break;

        case RND_LIGHT_TYPE:
            // what kind of light?
            m_Pickers[RND_LIGHT_TYPE].ConfigRandom();
            m_Pickers[RND_LIGHT_TYPE].AddRandItem(1, RND_LIGHT_TYPE_Directional);
            m_Pickers[RND_LIGHT_TYPE].AddRandItem(1, RND_LIGHT_TYPE_PointSource);
            m_Pickers[RND_LIGHT_TYPE].AddRandItem(1, RND_LIGHT_TYPE_SpotLight);
            m_Pickers[RND_LIGHT_TYPE].CompileRandom();
            break;

        case RND_LIGHT_SPOT_LWTOFF:
            // angle (in degrees) at which spotlight lwts off
            m_Pickers[RND_LIGHT_SPOT_LWTOFF].ConfigRandom();
            m_Pickers[RND_LIGHT_SPOT_LWTOFF].AddRandItem(1, 180);         // no cutoff
            m_Pickers[RND_LIGHT_SPOT_LWTOFF].AddRandRange(1, 0, 90);     // narrow beam
            m_Pickers[RND_LIGHT_SPOT_LWTOFF].CompileRandom();
            break;

        case RND_LIGHT_NUM_ON:
            // How many lights should be enabled?
            m_Pickers[RND_LIGHT_NUM_ON].ConfigRandom();
            m_Pickers[RND_LIGHT_NUM_ON].AddRandItem(2, 2);
            m_Pickers[RND_LIGHT_NUM_ON].AddRandRange(1, 1, 8);
            m_Pickers[RND_LIGHT_NUM_ON].CompileRandom();
            break;

        case RND_LIGHT_BACK:
            // does this light shine on the front or back of polygons?
            m_Pickers[RND_LIGHT_BACK].ConfigRandom();
            m_Pickers[RND_LIGHT_BACK].AddRandItem(9, GL_FALSE);
            m_Pickers[RND_LIGHT_BACK].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_LIGHT_BACK].CompileRandom();
            break;

        case RND_LIGHT_MODEL_LOCAL_VIEWER:
            // should we callwlate spelwlar highlights with a custom eye angle for each vertex?
            m_Pickers[RND_LIGHT_MODEL_LOCAL_VIEWER].ConfigRandom();
            m_Pickers[RND_LIGHT_MODEL_LOCAL_VIEWER].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_LIGHT_MODEL_LOCAL_VIEWER].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_LIGHT_MODEL_LOCAL_VIEWER].CompileRandom();
            break;

        case RND_LIGHT_MODEL_TWO_SIDE:
            // should we light only front, or both front and back sides of polygons?
            m_Pickers[RND_LIGHT_MODEL_TWO_SIDE].ConfigRandom();
            m_Pickers[RND_LIGHT_MODEL_TWO_SIDE].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_LIGHT_MODEL_TWO_SIDE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_LIGHT_MODEL_TWO_SIDE].CompileRandom();
            break;

        case RND_LIGHT_MODEL_COLOR_CONTROL:
            // should spelwlar highlight color be callwlated independently of texturing?
            m_Pickers[RND_LIGHT_MODEL_COLOR_CONTROL].ConfigRandom();
            m_Pickers[RND_LIGHT_MODEL_COLOR_CONTROL].AddRandItem(1, GL_SINGLE_COLOR);
            m_Pickers[RND_LIGHT_MODEL_COLOR_CONTROL].AddRandItem(2, GL_SEPARATE_SPELWLAR_COLOR);
            m_Pickers[RND_LIGHT_MODEL_COLOR_CONTROL].CompileRandom();
            break;

        case RND_LIGHT_MAT_IS_EMISSIVE:
            // should this material "glow"?
            m_Pickers[RND_LIGHT_MAT_IS_EMISSIVE].ConfigRandom();
            m_Pickers[RND_LIGHT_MAT_IS_EMISSIVE].AddRandItem(9, GL_FALSE);
            m_Pickers[RND_LIGHT_MAT_IS_EMISSIVE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_LIGHT_MAT_IS_EMISSIVE].CompileRandom();
            break;

        case RND_LIGHT_MAT:
            // which material color property should glColor() control?
            m_Pickers[RND_LIGHT_MAT].ConfigRandom();
            m_Pickers[RND_LIGHT_MAT].AddRandItem(10, GL_AMBIENT_AND_DIFFUSE);
            m_Pickers[RND_LIGHT_MAT].AddRandItem(7, GL_DIFFUSE);
            m_Pickers[RND_LIGHT_MAT].AddRandItem(1, GL_AMBIENT);
            m_Pickers[RND_LIGHT_MAT].AddRandItem(1, GL_SPELWLAR);
            m_Pickers[RND_LIGHT_MAT].AddRandItem(1, GL_EMISSION);
            m_Pickers[RND_LIGHT_MAT].CompileRandom();
            break;

        case RND_LIGHT_SHININESS:
            // shininess (spelwlar exponent) for lighting
            m_Pickers[RND_LIGHT_SHININESS].ConfigRandom();
            m_Pickers[RND_LIGHT_SHININESS].AddRandRange(1, 0, 1024);
            m_Pickers[RND_LIGHT_SHININESS].CompileRandom();
            break;

        case RND_LIGHT_COLOR:
            // light color (spelwlar & diffuse) float, 3 calls per light
            m_Pickers[RND_LIGHT_COLOR].FConfigRandom();
            m_Pickers[RND_LIGHT_COLOR].FAddRandRange(1, 0.5f, 1.0f);
            m_Pickers[RND_LIGHT_COLOR].FAddRandRange(1, 0.0f, 1.0f);
            m_Pickers[RND_LIGHT_COLOR].CompileRandom();
            break;

        case RND_LIGHT_AMBIENT:
            // light color (ambient) float, 3 calls per light
            m_Pickers[RND_LIGHT_AMBIENT].FConfigRandom();
            m_Pickers[RND_LIGHT_AMBIENT].FAddRandRange(2, 0.0f, 0.3f);
            m_Pickers[RND_LIGHT_AMBIENT].FAddRandRange(1, 0.0f, 0.6f);
            m_Pickers[RND_LIGHT_AMBIENT].CompileRandom();
            break;

        case RND_LIGHT_ALPHA:
            // light alpha value (float)
            m_Pickers[RND_LIGHT_ALPHA].FConfigRandom();
            m_Pickers[RND_LIGHT_ALPHA].FAddRandRange(1, 0.0f, 1.0f);
            m_Pickers[RND_LIGHT_ALPHA].CompileRandom();
            break;

        case RND_LIGHT_SPOT_EXPONENT:
            // spotlight falloff exponent
            m_Pickers[RND_LIGHT_SPOT_EXPONENT].ConfigRandom();
            m_Pickers[RND_LIGHT_SPOT_EXPONENT].AddRandRange(3, 16, 64);
            m_Pickers[RND_LIGHT_SPOT_EXPONENT].AddRandRange(1, 0, 128);
            m_Pickers[RND_LIGHT_SPOT_EXPONENT].AddRandRange(1, 0, 1024);
            m_Pickers[RND_LIGHT_SPOT_EXPONENT].CompileRandom();
            break;
    }
    return OK;
}

string LightIdxToString(uint32 index)
{
    return Utility::StrPrintf("LightIdx_%d", index);
}
//------------------------------------------------------------------------------
RC RndLighting::Restart()
{
    m_Pickers.Restart();

    // Use the Log feature to verify that we generate lights repeatably.
    m_pGLRandom->LogBegin(8, LightIdxToString);

    int L;
    for (L = GL_LIGHT0; L <= GL_LIGHT7; L++)
    {
        LightSource * pL = m_LightSources + (L - GL_LIGHT0);

        pL->CompressedIndex  = L - GL_LIGHT0;
        pL->Type             = m_Pickers[RND_LIGHT_TYPE].Pick();
        pL->IsBackLight      = m_Pickers[RND_LIGHT_BACK].Pick();
        pL->SpotDirection[0] =  0.0;
        pL->SpotDirection[1] =  0.0;
        pL->SpotDirection[2] = -1.0;
        pL->SpotDirection[3] =  0.0;
        pL->SpotExponent     = 0;
        pL->SpotLwtoff       = 180;
        pL->Attenuation[0]   = 1.0;
        pL->Attenuation[1]   = 0.0;
        pL->Attenuation[2]   = 0.0;

        pL->ColorDS[0] = m_Pickers[RND_LIGHT_COLOR].FPick();
        pL->ColorDS[1] = m_Pickers[RND_LIGHT_COLOR].FPick();
        pL->ColorDS[2] = m_Pickers[RND_LIGHT_COLOR].FPick();
        pL->ColorDS[3] = m_Pickers[RND_LIGHT_ALPHA].FPick();

        pL->ColorA[0] = m_Pickers[RND_LIGHT_AMBIENT].FPick();
        pL->ColorA[1] = m_Pickers[RND_LIGHT_AMBIENT].FPick();
        pL->ColorA[2] = m_Pickers[RND_LIGHT_AMBIENT].FPick();
        pL->ColorA[3] = m_Pickers[RND_LIGHT_ALPHA].FPick();

        // Is this light parallel directional, a local point-source, or
        // a local spot-light?

        switch ( pL->Type )
        {
            case RND_LIGHT_TYPE_Directional:

                // Has direction, but not position or attenuation.
                // Force direction to have Z>= 0, so we light the
                // sides of polygons facing the screen.

                m_pGLRandom->PickNormalfv(pL->Vec, !pL->IsBackLight);
                pL->Vec[3] = 0.0;   // direction, not position
                break;

            case RND_LIGHT_TYPE_PointSource:

                // Has position and attenuation, but is omni-directional.
                // Force position to be within or near the view frustum in XY,
                // and between the camera position and the middle of the frustum
                // in Z.

                pL->Vec[0] = (GLfloat) m_pFpCtx->Rand.GetRandomFloat(-.55*RND_GEOM_WorldWidth, .55*RND_GEOM_WorldWidth);
                pL->Vec[1] = (GLfloat) m_pFpCtx->Rand.GetRandomFloat(-.55*RND_GEOM_WorldHeight, .55*RND_GEOM_WorldHeight);
                pL->Vec[2] = (GLfloat) m_pFpCtx->Rand.GetRandomFloat(-.55*RND_GEOM_WorldDepth, 0.0);

                if ( pL->IsBackLight )
                    pL->Vec[2] += .5*RND_GEOM_WorldDepth;

                pL->Vec[3] = 1.0;   // position, not direction.

                pL->Attenuation[0] = (GLfloat)m_pFpCtx->Rand.GetRandomFloat(0.5, 2.0);
                pL->Attenuation[1] = (GLfloat)m_pFpCtx->Rand.GetRandomFloat(0.0, 2.0/RND_GEOM_WorldWidth);
                pL->Attenuation[2] = (GLfloat)m_pFpCtx->Rand.GetRandomFloat(0.0, 2.0/RND_GEOM_WorldWidth/RND_GEOM_WorldWidth);
                break;

            case RND_LIGHT_TYPE_SpotLight:

                // Has position, direction, attenuation, and cone "focus" parameters.
                // Force position as in a PointSource light.

                pL->Vec[0] = (GLfloat) m_pFpCtx->Rand.GetRandomFloat(-.55*RND_GEOM_WorldWidth, .55*RND_GEOM_WorldWidth);
                pL->Vec[1] = (GLfloat) m_pFpCtx->Rand.GetRandomFloat(-.55*RND_GEOM_WorldHeight, .55*RND_GEOM_WorldHeight);
                pL->Vec[2] = (GLfloat) m_pFpCtx->Rand.GetRandomFloat(-.55*RND_GEOM_WorldDepth, 0.0);

                if ( pL->IsBackLight )
                    pL->Vec[2] += .5*RND_GEOM_WorldDepth;

                pL->Vec[3] = 1.0;   // position, not direction.

                pL->Attenuation[0] = (GLfloat)m_pFpCtx->Rand.GetRandomFloat(0.5, 2.0);
                pL->Attenuation[1] = (GLfloat)m_pFpCtx->Rand.GetRandomFloat(0.0, 1.0/RND_GEOM_WorldWidth);
                pL->Attenuation[2] = (GLfloat)m_pFpCtx->Rand.GetRandomFloat(0.0, 1.0/RND_GEOM_WorldWidth/RND_GEOM_WorldWidth);

                // Spot direction should shine at or near the bbox being drawn,
                // so leave this callwlation for later.
                pL->SpotDirection[0] = 0.0;
                pL->SpotDirection[1] = 0.0;
                pL->SpotDirection[2] = 1.0;
                pL->SpotDirection[3] = 0.0;   // direction, not position

                pL->SpotExponent     = m_Pickers[RND_LIGHT_SPOT_EXPONENT].Pick();
                if (pL->SpotExponent >= m_pGLRandom->MaxSpotExponent())
                    pL->SpotExponent = 16;

                pL->SpotLwtoff       = m_Pickers[RND_LIGHT_SPOT_LWTOFF].Pick();
                break;

            default: MASSERT(0);  // shouldn't get here!
        }

        // PrintLightSource(Tee::PriDebug, L - GL_LIGHT0);
        m_pGLRandom->LogData(L - GL_LIGHT0, pL, sizeof(*pL));
    }

    return m_pGLRandom->LogFinish("Light");
}

//------------------------------------------------------------------------------
void RndLighting::Pick()
{
    GLuint L;
    GLuint NumSources;

    // First, pick the random state:

    // Note that we do the same random picks, whether or not lighting
    // is enabled.  This helps when we are debugging lighting problems,
    // since the rest of the test continues to get the same random sequence.

    m_Lighting.ShadeModel      = m_Pickers[RND_LIGHT_SHADE_MODEL].Pick();
    m_Lighting.Enabled         = m_Pickers[RND_LIGHT_ENABLE].Pick();
    m_Lighting.LocalViewer     = m_Pickers[RND_LIGHT_MODEL_LOCAL_VIEWER].Pick();
    m_Lighting.TwoSided        = m_Pickers[RND_LIGHT_MODEL_TWO_SIDE].Pick();
    m_Lighting.ColorControl    = m_Pickers[RND_LIGHT_MODEL_COLOR_CONTROL].Pick();
    NumSources                 = m_Pickers[RND_LIGHT_NUM_ON].Pick();
    m_Lighting.Shininess       = m_Pickers[RND_LIGHT_SHININESS].Pick();
    m_Lighting.ColorMaterial   = m_Pickers[RND_LIGHT_MAT].Pick();

    m_Lighting.GlobalAmbientColor[0] = m_Pickers[RND_LIGHT_AMBIENT].FPick();
    m_Lighting.GlobalAmbientColor[1] = m_Pickers[RND_LIGHT_AMBIENT].FPick();
    m_Lighting.GlobalAmbientColor[2] = m_Pickers[RND_LIGHT_AMBIENT].FPick();
    m_Lighting.GlobalAmbientColor[3] = m_Pickers[RND_LIGHT_ALPHA].FPick();

    m_Lighting.MaterialColor[0] = m_Pickers[RND_LIGHT_COLOR].FPick();
    m_Lighting.MaterialColor[1] = m_Pickers[RND_LIGHT_COLOR].FPick();
    m_Lighting.MaterialColor[2] = m_Pickers[RND_LIGHT_COLOR].FPick();
    m_Lighting.MaterialColor[3] = m_Pickers[RND_LIGHT_ALPHA].FPick();

    if ( m_Pickers[RND_LIGHT_MAT_IS_EMISSIVE].Pick() )
    {
        m_Lighting.EmissiveMaterialColor[0] = m_Pickers[RND_LIGHT_COLOR].FPick();
        m_Lighting.EmissiveMaterialColor[1] = m_Pickers[RND_LIGHT_COLOR].FPick();
        m_Lighting.EmissiveMaterialColor[2] = m_Pickers[RND_LIGHT_COLOR].FPick();
        m_Lighting.EmissiveMaterialColor[3] = m_Pickers[RND_LIGHT_ALPHA].FPick();
    }
    else
    {
        m_Lighting.EmissiveMaterialColor[0] = 0.0f;
        m_Lighting.EmissiveMaterialColor[1] = 0.0f;
        m_Lighting.EmissiveMaterialColor[2] = 0.0f;
        m_Lighting.EmissiveMaterialColor[3] = 1.0f;
    }

    // Pick which light sources to enable:
    GLuint numEnabled = 0;

    for ( L = 0; L < 8; L++ )
    {
        if (m_Lighting.CompressLights)
            m_LightSources[L].CompressedIndex = numEnabled;
        else
            m_LightSources[L].CompressedIndex = L;

        if ( (NumSources-numEnabled) > m_pFpCtx->Rand.GetRandom(0, 7-L) )
        {
            m_Lighting.EnabledSources[L] = 1;
            numEnabled++;

            // Pick the spot direction for spotlights, force it to shine towards
            // the bbox that we are drawing.
            if (m_LightSources[L].Type == RND_LIGHT_TYPE_SpotLight)
            {
                LightSource * pL = m_LightSources + L;

                RndGeometry::BBox bbox;
                m_pGLRandom->m_Geometry.GetBBox(&bbox);

                pL->SpotDirection[0] = bbox.left   + bbox.width/2  - pL->Vec[0] /*- bbox.left   */;
                pL->SpotDirection[1] = bbox.bottom + bbox.height/2 - pL->Vec[1] /*- bbox.bottom */;
                pL->SpotDirection[2] = bbox.deep   + bbox.depth/2  - pL->Vec[2] /*- bbox.deep   */;

                float norm = 1.0f /
                             sqrt(
                                pL->SpotDirection[0] * pL->SpotDirection[0] +
                                pL->SpotDirection[1] * pL->SpotDirection[1] +
                                pL->SpotDirection[2] * pL->SpotDirection[2]
                                );
                pL->SpotDirection[0] *= norm;
                pL->SpotDirection[1] *= norm;
                pL->SpotDirection[2] *= norm;
            }
        }
        else
        {
            m_Lighting.EnabledSources[L] = 0;
        }
    }
    if (m_Lighting.Shininess < 0)
        m_Lighting.Shininess = 0;
    if (m_Lighting.Shininess > m_pGLRandom->MaxShininess())
        m_Lighting.Shininess = m_pGLRandom->MaxShininess();

    if (m_pGLRandom->m_GpuPrograms.VxProgEnabled())
        m_Lighting.Enabled         = GL_FALSE;

    m_pGLRandom->LogData(RND_LIGHT, &m_Lighting, sizeof(m_Lighting));

    // Special processing for Transform Feedback
    if ( m_Lighting.Enabled )
        m_pGLRandom->m_Misc.XfbPlaybackNotPossible();

}

//------------------------------------------------------------------------------
void RndLighting::Print(Tee::Priority pri)
{
    // Only print when we are drawing vertexes, not pixels.
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return;
    if (m_pGLRandom->m_GpuPrograms.VxProgEnabled())
    {
        Printf(pri, "Lighting: vxprog, %s\n",mglEnumToString(m_Lighting.ShadeModel,"BadShadeModel",true));
        return;
    }

    if ( m_Lighting.Enabled )
    {
        Printf(pri, "Lighting: ENABLED, %s %d-sided %s %s mat:%s shin:%d\n",
            mglEnumToString(m_Lighting.ShadeModel,"BadShadeModel",true),
            m_Lighting.TwoSided ? 2 : 1,
            m_Lighting.LocalViewer ? "local-viewer" : "",
            m_Lighting.ColorControl == GL_SINGLE_COLOR ? "GL_SINGLE_COLOR" : "GL_SEPARATE_SPELWLAR_COLOR",
            StrMaterial(m_Lighting.ColorMaterial),
            m_Lighting.Shininess
            );

        for ( GLint L = 0; L < 8; L++ )
        {
            if ( m_Lighting.EnabledSources[L] )
            {
                Printf(pri, "  ");
                PrintLightSource(pri, L);
            }
        }
    }
    else
    {
        Printf(pri, "Lighting: DISABLED, glShadeModel(%s)\n",
            mglEnumToString(m_Lighting.ShadeModel,"BadShadeModel",true) );
    }
}

GLboolean RndLighting::IsEnabled() const
{
    return (m_Lighting.Enabled != 0);
}

//------------------------------------------------------------------------------
RC RndLighting::Send()
{
    // Set options and enable things.
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return OK; //!< we are drawing pixels in ths round.

    glShadeModel( m_Lighting.ShadeModel );

    if ( m_Lighting.Enabled )
    {
        GLint L;
        for ( L = 0; L < 8; L++ )
        {
            if ( m_Lighting.EnabledSources[L] )
            {
                LightSource * pL     = m_LightSources + L;
                GLenum        Light  = GL_LIGHT0 + pL->CompressedIndex;

                glEnable(Light);

                // @@@ It seems like we ought to be able to send this info once
                // @@@ in GenerateLights, then never again, but doing it that
                // @@@ way causes repeatability problems.  GL driver bug?

                glLightfv(Light, GL_DIFFUSE,                pL->ColorDS);
                glLightfv(Light, GL_SPELWLAR,               pL->ColorDS);
                glLightfv(Light, GL_AMBIENT,                pL->ColorA);
                glLightfv(Light, GL_POSITION,               pL->Vec);
                glLightf (Light, GL_CONSTANT_ATTENUATION,   pL->Attenuation[0]);
                glLightf (Light, GL_LINEAR_ATTENUATION,     pL->Attenuation[1]);
                glLightf (Light, GL_QUADRATIC_ATTENUATION,  pL->Attenuation[2]);
                glLightfv(Light, GL_SPOT_DIRECTION,         pL->SpotDirection);
                glLighti (Light, GL_SPOT_EXPONENT,          pL->SpotExponent);
                glLighti (Light, GL_SPOT_LWTOFF,            pL->SpotLwtoff);
            }
        }

        glLightModelfv(GL_LIGHT_MODEL_AMBIENT,       m_Lighting.GlobalAmbientColor);
        glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,   m_Lighting.LocalViewer);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,       m_Lighting.TwoSided);
        glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,  m_Lighting.ColorControl);
        glColorMaterial( GL_FRONT_AND_BACK, m_Lighting.ColorMaterial );

        glEnable(GL_LIGHTING);

        if (m_pGLRandom->m_VertexFormat.Size(att_COL0))
            glEnable(GL_COLOR_MATERIAL);

        glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, m_Lighting.Shininess);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, m_Lighting.MaterialColor );
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, m_Lighting.MaterialColor );
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPELWLAR, m_Lighting.MaterialColor );
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, m_Lighting.EmissiveMaterialColor );
    }
    return OK;
}

//------------------------------------------------------------------------------
RC RndLighting::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
void RndLighting::CompressLights(bool b)
{
    m_Lighting.CompressLights = b;
}

//------------------------------------------------------------------------------
void RndLighting::PrintLightSource(Tee::Priority pri, int L) const
{
    Printf(pri, "light%d %s %s D,S [%3.1f %3.1f %3.1f %3.1f] A [%3.1f %3.1f %3.1f %3.1f]\n",
        m_LightSources[L].CompressedIndex,
        m_LightSources[L].Type == RND_LIGHT_TYPE_Directional ? "Directional" :
        m_LightSources[L].Type == RND_LIGHT_TYPE_PointSource ? "PointSource" :
           "SpotLight",
        m_LightSources[L].IsBackLight ? "BACK" : "FRONT",
        m_LightSources[L].ColorDS[0],
        m_LightSources[L].ColorDS[1],
        m_LightSources[L].ColorDS[2],
        m_LightSources[L].ColorDS[3],
        m_LightSources[L].ColorA[0],
        m_LightSources[L].ColorA[1],
        m_LightSources[L].ColorA[2],
        m_LightSources[L].ColorA[3] );

    Printf(pri, "\tdir/pos [ %3.1f %3.1f %3.1f %3.1f ] atten [ %3.1f %3.1f %3.1f ] spotDir [ %3.1f %3.1f %3.1f ] spotEx %d spotLwt %d\n",
        m_LightSources[L].Vec[0],
        m_LightSources[L].Vec[1],
        m_LightSources[L].Vec[2],
        m_LightSources[L].Vec[3],
        m_LightSources[L].Attenuation[0],
        m_LightSources[L].Attenuation[1],
        m_LightSources[L].Attenuation[2],
        m_LightSources[L].SpotDirection[0],
        m_LightSources[L].SpotDirection[1],
        m_LightSources[L].SpotDirection[2],
        m_LightSources[L].SpotExponent,
        m_LightSources[L].SpotLwtoff );
}

//------------------------------------------------------------------------------
const char * RndLighting::StrMaterial(GLenum mat) const
{
    return mglEnumToString(mat,"BadMaterial",true);
}
GLenum RndLighting::GetShadeModel() const
{
    return m_Lighting.ShadeModel;
}

