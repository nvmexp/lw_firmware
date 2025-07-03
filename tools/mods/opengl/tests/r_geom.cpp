/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010,2013,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndGeometry: OpenGL Randomizer for matrices, bbox, vertex programs, etc.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"   // declaration of our namespace
#include "core/include/massert.h"
#include "random.h"
#include <stdio.h>      // sprintf()

using namespace GLRandom;

//------------------------------------------------------------------------------
RndGeometry::RndGeometry(GLRandomTest * pglr)
: GLRandomHelper(pglr, RND_GEOM_NUM_PICKERS, "Geometry")
{
    memset(&m_Geom, 0, sizeof(m_Geom));
}

//------------------------------------------------------------------------------
RndGeometry::~RndGeometry()
{
}

//------------------------------------------------------------------------------
// get picker tables from JavaScript
RC RndGeometry::SetDefaultPicker(int picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_GEOM_BBOX_WIDTH:
            // X width of bounding-box for each group of primitives
            m_Pickers[RND_GEOM_BBOX_WIDTH].ConfigRandom();
            m_Pickers[RND_GEOM_BBOX_WIDTH].AddRandItem (80, RND_GEOM_WorldWidth/2);
            m_Pickers[RND_GEOM_BBOX_WIDTH].AddRandRange(19, 1, RND_GEOM_WorldWidth/2);
            m_Pickers[RND_GEOM_BBOX_WIDTH].AddRandItem ( 1, RND_GEOM_WorldWidth);
            m_Pickers[RND_GEOM_BBOX_WIDTH].CompileRandom();
            break;

        case RND_GEOM_BBOX_HEIGHT:
            // Y height of bounding-box for each group of primitives
            m_Pickers[RND_GEOM_BBOX_HEIGHT].ConfigRandom();
            m_Pickers[RND_GEOM_BBOX_HEIGHT].AddRandItem (80, RND_GEOM_WorldHeight/2);
            m_Pickers[RND_GEOM_BBOX_HEIGHT].AddRandRange(19, 1, RND_GEOM_WorldHeight/2);
            m_Pickers[RND_GEOM_BBOX_HEIGHT].AddRandItem ( 1, RND_GEOM_WorldHeight);
            m_Pickers[RND_GEOM_BBOX_HEIGHT].CompileRandom();
            break;

        case RND_GEOM_BBOX_XOFFSET:
            // X offset of bbox from previous bbox within row
            m_Pickers[RND_GEOM_BBOX_XOFFSET].ConfigRandom();
            m_Pickers[RND_GEOM_BBOX_XOFFSET].AddRandItem(1, RND_GEOM_WorldWidth/2);
            m_Pickers[RND_GEOM_BBOX_XOFFSET].CompileRandom();
            break;

        case RND_GEOM_BBOX_YOFFSET:
            // Y offset of bbox from previous bbox within row
            m_Pickers[RND_GEOM_BBOX_YOFFSET].ConfigRandom();
            m_Pickers[RND_GEOM_BBOX_YOFFSET].AddRandItem(1, RND_GEOM_WorldHeight/2);
            m_Pickers[RND_GEOM_BBOX_YOFFSET].CompileRandom();
            break;

        case RND_GEOM_BBOX_Z_BASE:
            // Z center of BBOX (offset from max depth)
            m_Pickers[RND_GEOM_BBOX_Z_BASE].ConfigRandom();
            m_Pickers[RND_GEOM_BBOX_Z_BASE].AddRandRange(1, 0, RND_GEOM_WorldDepth);
            m_Pickers[RND_GEOM_BBOX_Z_BASE].CompileRandom();
            break;

        case RND_GEOM_BBOX_DEPTH:
            // Z depth of bbox (near-far)
            m_Pickers[RND_GEOM_BBOX_DEPTH].ConfigRandom();
            m_Pickers[RND_GEOM_BBOX_DEPTH].AddRandRange(9, 0, RND_GEOM_WorldDepth/20);
            m_Pickers[RND_GEOM_BBOX_DEPTH].AddRandRange(1, 0, RND_GEOM_WorldDepth/2);
            m_Pickers[RND_GEOM_BBOX_DEPTH].CompileRandom();
            break;

        case RND_GEOM_FRONT_FACE:
            // is a clockwise poly front, or CCWise?
            m_Pickers[RND_GEOM_FRONT_FACE].ConfigRandom();
            m_Pickers[RND_GEOM_FRONT_FACE].AddRandItem(1, GL_CW);
            m_Pickers[RND_GEOM_FRONT_FACE].AddRandItem(1, GL_CCW);
            m_Pickers[RND_GEOM_FRONT_FACE].CompileRandom();
            break;

        case RND_GEOM_VTX_NORMAL_SCALING:
            // how should vertex normals be forced to length 1.0?
            m_Pickers[RND_GEOM_VTX_NORMAL_SCALING].ConfigRandom();
            m_Pickers[RND_GEOM_VTX_NORMAL_SCALING].AddRandItem(2, RND_GEOM_VTX_NORMAL_SCALING_Unscaled);
            m_Pickers[RND_GEOM_VTX_NORMAL_SCALING].AddRandItem(1, RND_GEOM_VTX_NORMAL_SCALING_Scaled);
            m_Pickers[RND_GEOM_VTX_NORMAL_SCALING].AddRandItem(1, RND_GEOM_VTX_NORMAL_SCALING_Corrected);
            m_Pickers[RND_GEOM_VTX_NORMAL_SCALING].CompileRandom();
            break;

        case RND_GEOM_LWLL_FACE:
            // what faces to lwll? (none, front, back, both)
            m_Pickers[RND_GEOM_LWLL_FACE].ConfigRandom();
            m_Pickers[RND_GEOM_LWLL_FACE].AddRandItem(80, GL_FALSE);
            m_Pickers[RND_GEOM_LWLL_FACE].AddRandItem(15, GL_BACK);
            m_Pickers[RND_GEOM_LWLL_FACE].AddRandItem( 5, GL_FRONT);
            m_Pickers[RND_GEOM_LWLL_FACE].AddRandItem( 1, GL_FRONT_AND_BACK);
            m_Pickers[RND_GEOM_LWLL_FACE].CompileRandom();
            break;

        case RND_GEOM_BBOX_FACES_FRONT:
            // does this group of polygons face front?
            m_Pickers[RND_GEOM_BBOX_FACES_FRONT].ConfigRandom();
            m_Pickers[RND_GEOM_BBOX_FACES_FRONT].AddRandItem(8, GL_TRUE);
            m_Pickers[RND_GEOM_BBOX_FACES_FRONT].AddRandItem(2, GL_FALSE);
            m_Pickers[RND_GEOM_BBOX_FACES_FRONT].CompileRandom();
            break;

        case RND_GEOM_FORCE_BIG_SIMPLE:
            // Force polygons to be big and simple (for simplifying debugging)
            m_Pickers[RND_GEOM_FORCE_BIG_SIMPLE].ConfigConst(GL_FALSE);
            break;

        case RND_GEOM_TXU_GEN_ENABLED:
            // Is texture coordinate generation enabled for this texture unit?
            m_Pickers[RND_GEOM_TXU_GEN_ENABLED].ConfigRandom();
            m_Pickers[RND_GEOM_TXU_GEN_ENABLED].AddRandItem(2, GL_FALSE);
            m_Pickers[RND_GEOM_TXU_GEN_ENABLED].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_GEOM_TXU_GEN_ENABLED].CompileRandom();
            break;

        case RND_GEOM_TXU_GEN_MODE:
            // How should texture coordinate generation be done for this texture unit?
            m_Pickers[RND_GEOM_TXU_GEN_MODE].ConfigRandom();
            m_Pickers[RND_GEOM_TXU_GEN_MODE].AddRandItem(1, GL_OBJECT_LINEAR);
            m_Pickers[RND_GEOM_TXU_GEN_MODE].AddRandItem(1, GL_EYE_LINEAR);
            m_Pickers[RND_GEOM_TXU_GEN_MODE].AddRandItem(1, GL_SPHERE_MAP);
            m_Pickers[RND_GEOM_TXU_GEN_MODE].AddRandItem(1, GL_NORMAL_MAP_EXT);
            m_Pickers[RND_GEOM_TXU_GEN_MODE].AddRandItem(1, GL_REFLECTION_MAP_EXT);
            m_Pickers[RND_GEOM_TXU_GEN_MODE].CompileRandom();
            break;

        case RND_GEOM_TXMX_OPERATION:
            // what should we do with the texture coordinate matrix?
            m_Pickers[RND_GEOM_TXMX_OPERATION].ConfigRandom();
            m_Pickers[RND_GEOM_TXMX_OPERATION].AddRandItem(5, RND_GEOM_TXMX_OPERATION_Identity);   // texture matrix leaves texture coords unchanged
            m_Pickers[RND_GEOM_TXMX_OPERATION].AddRandItem(1, RND_GEOM_TXMX_OPERATION_2dRotate);   // Rotate S,T around the R axis
            m_Pickers[RND_GEOM_TXMX_OPERATION].AddRandItem(1, RND_GEOM_TXMX_OPERATION_2dTranslate);// Offset S,T slightly
            m_Pickers[RND_GEOM_TXMX_OPERATION].AddRandItem(1, RND_GEOM_TXMX_OPERATION_2dScale);    // Scale S,T
            m_Pickers[RND_GEOM_TXMX_OPERATION].AddRandItem(2, RND_GEOM_TXMX_OPERATION_2dAll);      // Rotate, translate, and scale
            m_Pickers[RND_GEOM_TXMX_OPERATION].AddRandItem(2, RND_GEOM_TXMX_OPERATION_Project);    // use a perspective transform, to "project" textures onto geometry
            m_Pickers[RND_GEOM_TXMX_OPERATION].CompileRandom();
            break;

        case RND_GEOM_TXU_SPRITE_ENABLE_MASK:
            // mask of texture units for which point sprite coord replacement should be enabled
            m_Pickers[RND_GEOM_TXU_SPRITE_ENABLE_MASK].ConfigRandom();
            m_Pickers[RND_GEOM_TXU_SPRITE_ENABLE_MASK].AddRandRange(1, 0, 0xff);
            m_Pickers[RND_GEOM_TXU_SPRITE_ENABLE_MASK].CompileRandom();
            break;

        case RND_GEOM_USE_PERSPECTIVE:
            // Use perspective projection (else orthographic).
            // NOTE: perspective is prettier, but ortho is a better HW test.
            m_Pickers[RND_GEOM_USE_PERSPECTIVE].ConfigConst(GL_FALSE);
            break;

        case RND_GEOM_USER_CLIP_PLANE_MASK:
            // which user clip planes to enable? (bit mask)
            m_Pickers[RND_GEOM_USER_CLIP_PLANE_MASK].ConfigRandom();
            m_Pickers[RND_GEOM_USER_CLIP_PLANE_MASK].AddRandItem(3, 0);
            m_Pickers[RND_GEOM_USER_CLIP_PLANE_MASK].AddRandRange(1, 0, 0xff);
            m_Pickers[RND_GEOM_USER_CLIP_PLANE_MASK].CompileRandom();
            break;

        case RND_GEOM_MODELVIEW_OPERATION:
            // what should we do with the modelview matrix?
            m_Pickers[RND_GEOM_MODELVIEW_OPERATION].ConfigRandom();
            m_Pickers[RND_GEOM_MODELVIEW_OPERATION].AddRandItem (1, RND_GEOM_MODELVIEW_OPERATION_Identity );
            m_Pickers[RND_GEOM_MODELVIEW_OPERATION].AddRandItem (1, RND_GEOM_MODELVIEW_OPERATION_Translate);
            m_Pickers[RND_GEOM_MODELVIEW_OPERATION].AddRandItem (1, RND_GEOM_MODELVIEW_OPERATION_Scale    );
            m_Pickers[RND_GEOM_MODELVIEW_OPERATION].AddRandItem (1, RND_GEOM_MODELVIEW_OPERATION_Rotate   );
            m_Pickers[RND_GEOM_MODELVIEW_OPERATION].CompileRandom();
            break;

    }
    return OK;
}

//------------------------------------------------------------------------------
// Do once-per-restart picks, prints, & sends.
RC RndGeometry::Restart()
{
    RC rc;

    m_Pickers.Restart();

    // Pick a new starting x,y and directions for the new screen.
    ResetBBox();

    // Set projection to view this world:
    //   X from -10k to +10k
    //   Y from  -8k to  +8k
    //   Z from  +8k to +24k (distance from eye)

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( RND_GEOM_WorldLeft,   RND_GEOM_WorldRight,
        RND_GEOM_WorldBottom, RND_GEOM_WorldTop,
        RND_GEOM_WorldNear,   RND_GEOM_WorldFar  );

    // Shift the modelview matrix so that Z seems to run from -8k to +8k.

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslated(0.0, 0.0, -(RND_GEOM_WorldFar+RND_GEOM_WorldNear)/2);

    rc = mglGetRC();

    return rc;
}

//------------------------------------------------------------------------------
// Pick new random state.
void RndGeometry::Pick()
{
    // Make random picks.

    m_Geom.FaceFront         = (GL_FALSE != m_Pickers[RND_GEOM_BBOX_FACES_FRONT].Pick());
    m_Geom.CwIsFront         = (GL_CW    == m_Pickers[RND_GEOM_FRONT_FACE].Pick());
    m_Geom.UsePerspective    = (GL_FALSE != m_Pickers[RND_GEOM_USE_PERSPECTIVE].Pick());
    m_Geom.LwllFace          = m_Pickers[RND_GEOM_LWLL_FACE].Pick();
    m_Geom.Normalize         = m_Pickers[RND_GEOM_VTX_NORMAL_SCALING].Pick();
    m_Geom.SpriteEnableMask  = m_Pickers[RND_GEOM_TXU_SPRITE_ENABLE_MASK].Pick();
    m_Geom.ForceBigSimple    = m_Pickers[RND_GEOM_FORCE_BIG_SIMPLE].Pick();
    m_Geom.UserClipPlaneMask = m_Pickers[RND_GEOM_USER_CLIP_PLANE_MASK].Pick();
    m_Geom.ModelViewOperation = m_Pickers[RND_GEOM_MODELVIEW_OPERATION].Pick();

    if ((m_Geom.ModelViewOperation < RND_GEOM_MODELVIEW_OPERATION_Identity) ||
        (m_Geom.ModelViewOperation > RND_GEOM_MODELVIEW_OPERATION_Rotate))
        m_Geom.ModelViewOperation = RND_GEOM_MODELVIEW_OPERATION_Identity;

    NextBBox(); // Note: uses picked value of m_Geom.ForceBigSimple.

    // Fix up the random picks to be self-consistent, and to match the HW.
    if (!m_pGLRandom->HasExt(GLRandomTest::ExtLW_point_sprite))
        m_Geom.SpriteEnableMask = 0;

    m_Geom.UserClipPlaneMask &= ((1 << m_pGLRandom->MaxUserClipPlanes()) - 1);

    switch (m_Geom.Normalize)
    {
        case RND_GEOM_VTX_NORMAL_SCALING_Unscaled:
        case RND_GEOM_VTX_NORMAL_SCALING_Scaled:
        case RND_GEOM_VTX_NORMAL_SCALING_Corrected:
            break;
        default:
            m_Geom.Normalize = RND_GEOM_VTX_NORMAL_SCALING_Unscaled;
    }
    if (m_pGLRandom->m_GpuPrograms.VxProgEnabled())
        m_Geom.Normalize = RND_GEOM_VTX_NORMAL_SCALING_Unscaled;

    //
    // Pick texture coord gen and matrix stuff for each texture unit.
    //
    memset(m_Geom.Tx, 0, sizeof(TxGeometryStatus)*MaxTxCoords);

    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
    {
        if (0 == m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(txu))
            continue;

        TxGeometryStatus * pTxgs = m_Geom.Tx + txu;

        pTxgs->GenEnabled      = m_Pickers[RND_GEOM_TXU_GEN_ENABLED].Pick();
        pTxgs->GenMode[0]      = m_Pickers[RND_GEOM_TXU_GEN_MODE].Pick();
        pTxgs->GenMode[1]      = m_Pickers[RND_GEOM_TXU_GEN_MODE].Pick();
        pTxgs->GenMode[2]      = m_Pickers[RND_GEOM_TXU_GEN_MODE].Pick();
        pTxgs->GenMode[3]      = m_Pickers[RND_GEOM_TXU_GEN_MODE].Pick();
        pTxgs->MatrixOperation = m_Pickers[RND_GEOM_TXMX_OPERATION].Pick();
        pTxgs->MxRotateDegrees = m_pFpCtx->Rand.GetRandomFloat(-180.0,180.0);
        pTxgs->MxRotateAxis[0] = m_pFpCtx->Rand.GetRandomFloat(-1.0, 1.0);
        pTxgs->MxRotateAxis[1] = m_pFpCtx->Rand.GetRandomFloat(-1.0, 1.0);
        pTxgs->MxRotateAxis[2] = m_pFpCtx->Rand.GetRandomFloat(-1.0, 1.0);
        pTxgs->MxTranslate[0]  = m_pFpCtx->Rand.GetRandomFloat(-0.5, 0.5);
        pTxgs->MxTranslate[1]  = m_pFpCtx->Rand.GetRandomFloat(-0.5, 0.5);
        pTxgs->MxScale[0]      = (GLfloat) EXP( m_pFpCtx->Rand.GetRandomFloat(-2.3, 2.3) );
        pTxgs->MxScale[1]      = (GLfloat) EXP( m_pFpCtx->Rand.GetRandomFloat(-2.3, 2.3) );

        // Make tex-gen modes compatible with the texture coord requirements.

        GLint minTxCoords = m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(txu);

        if (m_pGLRandom->m_GpuPrograms.VxProgEnabled() || (minTxCoords == 0))
            pTxgs->GenEnabled = GL_FALSE;

        if (m_pGLRandom->m_GpuPrograms.VxProgEnabled())
            pTxgs->MatrixOperation = RND_GEOM_TXMX_OPERATION_Identity;

        if (pTxgs->GenEnabled)
        {
            // Copy gen mode to each enabled TX coordinate,
            // set up plane-equations for linear modes.

            GLfloat Sscale, Tscale, Rscale, Qscale;

            m_pGLRandom->m_Texturing.ScaleSTRQ(txu, &Sscale, &Tscale, &Rscale, &Qscale);

            Rscale *= (GLfloat)(EXP(m_pFpCtx->Rand.GetRandomFloat(0.7F, 3.0F))/m_Geom.bbox.depth);
            Tscale *= (GLfloat)(EXP(m_pFpCtx->Rand.GetRandomFloat(0.7F, 3.0F))/m_Geom.bbox.width);
            Sscale *= (GLfloat)(EXP(m_pFpCtx->Rand.GetRandomFloat(0.7F, 3.0F))/m_Geom.bbox.height);

            // R can't be sphere map, Q must be linear mode
            if (pTxgs->GenMode[2] == GL_SPHERE_MAP)
                pTxgs->GenMode[2] = GL_OBJECT_LINEAR;
            if (pTxgs->GenMode[3] != GL_EYE_LINEAR)
                pTxgs->GenMode[3] = GL_OBJECT_LINEAR;

            pTxgs->GenPlane[0][0] = Sscale;   // S equals scaled X (if linear mode)
            pTxgs->GenPlane[0][1] = 0.0;
            pTxgs->GenPlane[0][2] = 0.0;
            pTxgs->GenPlane[0][3] = 0.0;

            pTxgs->GenPlane[1][0] = 0.0;
            pTxgs->GenPlane[1][1] = Tscale;   // T equals scaled Y (if linear mode)
            pTxgs->GenPlane[1][2] = 0.0;
            pTxgs->GenPlane[1][3] = 0.0;

            pTxgs->GenPlane[2][0] = 0.0;
            pTxgs->GenPlane[2][1] = 0.0;
            pTxgs->GenPlane[2][2] = Rscale;   // R equals scaled Z (if linear mode)
            pTxgs->GenPlane[2][3] = 0.0;

            pTxgs->GenPlane[3][0] = 0.0;
            pTxgs->GenPlane[3][1] = 0.0;
            pTxgs->GenPlane[3][2] = 0.0;
            pTxgs->GenPlane[3][3] = Qscale;  // Q equals W
        }
    } // for each txu (texture coord options)

    // Special Transform Feedback considerations
    // The second pass is using the captured clip coords as position.  To be
    // sure the same values go down in the 2nd pass, you need to stub out your
    // modelview and projection matrices (making that stage a NOP).  However,
    // the clipping computations on the first pass use eye coordinates.  The
    // clipping computations on the second pass have no choice but to use clip
    // coordinates (what's fed back in).  So the computations aren't the same.
    //
    // If we use programmable shaders, we can configure the pipe the way you
    // want, and implement clipping via clip distance outputs from the shader.
    // With fixed-function, you're SOL unless you use the identity matrix and
    // that is not an option in the 1st pass.
    if ( m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) )
    {
        m_Geom.UserClipPlaneMask = 0;
    }
    if (m_pGLRandom->m_GpuPrograms.VxProgEnabled() ||
        m_pGLRandom->m_GpuPrograms.FrProgEnabled())
    {
        // Don't mess with user-clip-planes when we're using programs, it
        // causes repeatability problems and is duplicate HW coverage anyway.
        m_Geom.UserClipPlaneMask = 0;
    }

    m_pGLRandom->LogData(RND_GEOM, &m_Geom, sizeof(m_Geom));
}

//------------------------------------------------------------------------------
// Print current picks to screen & logfile.
void RndGeometry::Print(Tee::Priority pri)
{
    // Only print when we are drawing vertexes, not pixels.
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return;
    Printf(pri, "Geometry: %s (=%s) modelview=%s lwll=%s normal=%s %s  ClpPlnMsk=%x Sprite=%x\n",
        DrawClockWise() ? "clockwise" : "counterclockwise",
        FaceFront() ? "front" : "back",
        StrModelViewOperation(m_Geom.ModelViewOperation),
        m_Geom.LwllFace == GL_BACK ? "back" :
            m_Geom.LwllFace == GL_FRONT ? "front" :
                m_Geom.LwllFace == GL_FALSE ? "disabled" :
                    "front&back",
        m_Geom.Normalize == RND_GEOM_VTX_NORMAL_SCALING_Unscaled ? "Unscaled" :
            m_Geom.Normalize == RND_GEOM_VTX_NORMAL_SCALING_Scaled ? "Scaled" :
                "Corrected",
        m_Geom.UsePerspective ? "perspective" : "orthographic",
        m_Geom.UserClipPlaneMask,
        m_Geom.SpriteEnableMask
        );

    if (!m_pGLRandom->m_GpuPrograms.VxProgEnabled())
    {
        Printf(pri, "\t TxGen,mtx:");
        GLint txu;
        for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
        {
            if (0 == m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(txu))
                continue;
            Printf(pri, " t%d=%s/%s/%s/%s,%s",
                txu,
                m_Geom.Tx[txu].GenEnabled ? StrTxGenMode(m_Geom.Tx[txu].GenMode[0]) : "off",
                m_Geom.Tx[txu].GenEnabled ? StrTxGenMode(m_Geom.Tx[txu].GenMode[1]) : "off",
                m_Geom.Tx[txu].GenEnabled ? StrTxGenMode(m_Geom.Tx[txu].GenMode[2]) : "off",
                m_Geom.Tx[txu].GenEnabled ? StrTxGenMode(m_Geom.Tx[txu].GenMode[3]) : "off",
                StrTxMxOperation(m_Geom.Tx[txu].MatrixOperation)
                );
        }
        Printf(pri, "\n");
    }
    PrintBBox(pri);
}

//------------------------------------------------------------------------------
// Send current picks to library/HW.
RC RndGeometry::Send()
{
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
    {
        return OK; //!< we are drawing pixels in ths round.
    }

    if ( m_Geom.UsePerspective )
    {
        // Set projection to view this world:
        //   X from -10k to +10k
        //   Y from  -8k to  +8k
        //   Z from  +8k to +24k (distance from eye)

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glFrustum(  RND_GEOM_WorldLeft,   RND_GEOM_WorldRight,
                    RND_GEOM_WorldBottom, RND_GEOM_WorldTop,
                    RND_GEOM_WorldNear,   RND_GEOM_WorldFar  );
    }

    for (GLuint i = 0; i < m_pGLRandom->MaxUserClipPlanes(); i++)
    {
        if (m_Geom.UserClipPlaneMask & (1 << i))
        {
            //
            // Plane equation is {x,y,z,w} where
            //   x,y,z = plane normal,
            //   w     = distance along normal from plane to origin.
            //
            // We handle up to 8 clip planes, each lwtting off one of the 8
            // corners of the bounding box.
            //
            GLdouble plane[4];
            GLdouble corner[3];

            if (0 == (i & 1))
            {  // clip planes 0,2,4,6 cut off the left corners of the bbox.
                plane[0]  = 1.0;
                corner[0] = m_Geom.bbox.left + 0.2 * m_Geom.bbox.width;
            }
            else
            {  // clip planes 1,3,5,7 cut off the right corners of the bbox.
                plane[0]  = -1;
                corner[0] = m_Geom.bbox.left + 0.8 * m_Geom.bbox.width;
            }

            if (0 == (i & 2))
            {  // clip planes 0,1,4,5 cut off the bottom corners of the bbox.
                plane[1]  = 1.0;
                corner[1] = m_Geom.bbox.bottom + 0.2 * m_Geom.bbox.height;
            }
            else
            {  // clip planes 2,3,6,7 cut off the top corners of the bbox.
                plane[1]  = -1;
                corner[1] = m_Geom.bbox.bottom + 0.8 * m_Geom.bbox.height;
            }

            if (0 == (i & 4))
            {  // clip planes 0,1,2,3 cut off the far corners of the bbox.
                plane[2]  = 1.0;
                corner[2] = m_Geom.bbox.deep + 0.2 * m_Geom.bbox.depth;
            }
            else
            {  // clip planes 4,5,6,7 cut off the near corners of the bbox.
                plane[2]  = -1;
                corner[2] = m_Geom.bbox.deep + 0.8 * m_Geom.bbox.depth;
            }

            // Callwlate distance as -normal DOT (any point on the plane).
            plane[3] =  corner[0]*-plane[0] +
                        corner[1]*-plane[1] +
                        corner[2]*-plane[2];

            glClipPlane(GL_CLIP_PLANE0 + i, plane);
            glEnable(GL_CLIP_PLANE0 + i);
        }
    }

    if (m_Geom.ModelViewOperation != RND_GEOM_MODELVIEW_OPERATION_Identity)
    {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslated(0.0, 0.0, -(RND_GEOM_WorldFar+RND_GEOM_WorldNear)/2);

        switch (m_Geom.ModelViewOperation)
        {
            case RND_GEOM_MODELVIEW_OPERATION_Translate:
            // RndVertexes generated data as if the bbox was at 0.
            // Translate to the correct spot.
            glTranslatef(
                (GLfloat)m_Geom.bbox.left,
                (GLfloat)m_Geom.bbox.bottom,
                (GLfloat)m_Geom.bbox.deep);
                break;

            case RND_GEOM_MODELVIEW_OPERATION_Scale    :
                // RndVertexes generated data as if the bbox was an 8k lwbe.
                // Scale to the correct size.
                glTranslatef(
                (GLfloat)m_Geom.bbox.left,
                (GLfloat)m_Geom.bbox.bottom,
                (GLfloat)m_Geom.bbox.deep);
                glScalef(
                m_Geom.bbox.width/(4*1024.0f),
                m_Geom.bbox.height/(4*1024.0f),
                m_Geom.bbox.depth/(4*1024.0f));
                break;

            case RND_GEOM_MODELVIEW_OPERATION_Rotate   :
                // RndVertexes generated data in a bbox 90d ccw from the real
                // bbox location, rotate it back by 90d clockwise (about Z axix).
                glRotatef(-90.0, 0.0, 0.0, 1.0);
                break;

            default:
                MASSERT(0); // missing enum value in case.
        }
    }

    if (m_Geom.CwIsFront)
        glFrontFace(GL_CW);

    if (m_Geom.LwllFace != GL_FALSE)
    {
        glEnable(GL_LWLL_FACE);
        switch ( m_Geom.LwllFace )
        {
            case GL_BACK:
            glLwllFace(GL_BACK);
            break;

        case GL_FRONT:
            glLwllFace(GL_FRONT);
            break;

        case GL_FRONT_AND_BACK:
            glLwllFace(GL_FRONT_AND_BACK);
            break;

        default: MASSERT(0);   // shouldn't get here!
        }
    }

    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
    {
        glActiveTexture(GL_TEXTURE0_ARB + txu);
        if (m_Geom.SpriteEnableMask & (1 << txu))
        {
            glTexElwi(GL_POINT_SPRITE_LW, GL_COORD_REPLACE_LW, GL_TRUE);
        }
    }

    if (!m_pGLRandom->m_GpuPrograms.VxProgEnabled())
    {
        if (m_Geom.Normalize != RND_GEOM_VTX_NORMAL_SCALING_Unscaled)
        {
            switch ( m_Geom.Normalize )
            {
                case RND_GEOM_VTX_NORMAL_SCALING_Scaled:
                glEnable(GL_RESCALE_NORMAL);
                    break;

                case RND_GEOM_VTX_NORMAL_SCALING_Corrected:
                glEnable(GL_NORMALIZE);
                    break;

                default: MASSERT(0); // shouldn't get here!
            }
        }

        // Update texture units:
        for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
        {
            if (0 == m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(txu))
                continue;
            TxGeometryStatus * pTxgs = m_Geom.Tx + txu;

            glActiveTexture(GL_TEXTURE0_ARB + txu);

            // texture matrix:

            glMatrixMode(GL_TEXTURE);
            glLoadIdentity();

            if (pTxgs->MatrixOperation != RND_GEOM_TXMX_OPERATION_Identity)
            {
                switch ( pTxgs->MatrixOperation )
                {
                    case RND_GEOM_TXMX_OPERATION_2dRotate:
                        // Rotate S,T around the R axis
                        glRotatef(pTxgs->MxRotateDegrees, 0.0, 0.0, 1.0);
                        break;

                    case RND_GEOM_TXMX_OPERATION_2dTranslate:
                        // Offset S,T slightly
                        glTranslatef(pTxgs->MxTranslate[0], pTxgs->MxTranslate[1], 0.0);
                        break;

                    case RND_GEOM_TXMX_OPERATION_2dScale:
                        // Scale S,T: shrink or expand by up to 10x.  ( log of 10 is 2.3).
                        glScalef(pTxgs->MxScale[0], pTxgs->MxScale[1], 1.0);
                        break;

                    case RND_GEOM_TXMX_OPERATION_2dAll:
                        // Rotate, translate, and scale
                        glRotatef(pTxgs->MxRotateDegrees, 0.0, 0.0, 1.0);
                        glScalef(pTxgs->MxScale[0], pTxgs->MxScale[1], 1.0);
                        glTranslatef(pTxgs->MxTranslate[0], pTxgs->MxTranslate[1], 0.0);
                        break;

                    case RND_GEOM_TXMX_OPERATION_Project:
                        // use a perspective transform, to "project" textures onto geometry
                        glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 3.0);
                        glRotatef(
                            pTxgs->MxRotateDegrees,
                            pTxgs->MxRotateAxis[0],
                            pTxgs->MxRotateAxis[1],
                            pTxgs->MxRotateAxis[2]);
                        glTranslatef(0.0, 0.0, -2);
                        break;

                    default: MASSERT(0); break;
                }
                MASSERT_NO_GLERROR();
            }

            // Texture coord generation:

            GLint numCoordsToGen = m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(txu);

            if (! pTxgs->GenEnabled)
                numCoordsToGen = 0;

            if (numCoordsToGen >= 4)
            {
                glEnable(GL_TEXTURE_GEN_Q);
                glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE, pTxgs->GenMode[3]);
                if ( pTxgs->GenMode[3] == GL_OBJECT_LINEAR )
                    glTexGenfv(GL_Q, GL_OBJECT_PLANE, pTxgs->GenPlane[3]);
                if ( pTxgs->GenMode[3] == GL_EYE_LINEAR )
                    glTexGenfv(GL_Q, GL_EYE_PLANE, pTxgs->GenPlane[3]);
            }
            if (numCoordsToGen >= 3)
            {
                glEnable(GL_TEXTURE_GEN_R);
                glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, pTxgs->GenMode[2]);
                if ( pTxgs->GenMode[2] == GL_OBJECT_LINEAR )
                    glTexGenfv(GL_R, GL_OBJECT_PLANE, pTxgs->GenPlane[2]);
                if ( pTxgs->GenMode[2] == GL_EYE_LINEAR )
                    glTexGenfv(GL_R, GL_EYE_PLANE, pTxgs->GenPlane[2]);
            }
            if (numCoordsToGen >= 2)
            {
                glEnable(GL_TEXTURE_GEN_T);
                glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, pTxgs->GenMode[1]);
                if ( pTxgs->GenMode[1] == GL_OBJECT_LINEAR )
                    glTexGenfv(GL_T, GL_OBJECT_PLANE, pTxgs->GenPlane[1]);
                if ( pTxgs->GenMode[1] == GL_EYE_LINEAR )
                    glTexGenfv(GL_T, GL_EYE_PLANE, pTxgs->GenPlane[1]);
            }
            if (numCoordsToGen >= 1)
            {
                glEnable(GL_TEXTURE_GEN_S);
                glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, pTxgs->GenMode[0]);
                if ( pTxgs->GenMode[0] == GL_OBJECT_LINEAR )
                    glTexGenfv(GL_S, GL_OBJECT_PLANE, pTxgs->GenPlane[0]);
                if ( pTxgs->GenMode[0] == GL_EYE_LINEAR )
                    glTexGenfv(GL_S, GL_EYE_PLANE, pTxgs->GenPlane[0]);
            }
            MASSERT_NO_GLERROR();
        } // for each TX unit
    } // vx pgms disabled

    return OK;
}

//------------------------------------------------------------------------------
RC RndGeometry::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
void RndGeometry::GetBBox(BBox* p) const
{
    MASSERT(p);
    memcpy(p, &m_Geom.bbox, sizeof(BBox));

    switch (m_Geom.ModelViewOperation)
    {
        default:
            MASSERT(0); // missing enum value in case.

        case RND_GEOM_MODELVIEW_OPERATION_Identity :
            break;

        case RND_GEOM_MODELVIEW_OPERATION_Translate:
            // Tell RndVertexes to generate data as if the bbox was at 0.
            p->left = p->bottom = p->deep = 0;
            break;

        case RND_GEOM_MODELVIEW_OPERATION_Scale    :
            // Tell RndVertexes to generate data as if the bbox was a 4k lwbe at 0.
            p->left = p->bottom = p->deep = 0;
            p->width = p->height = p->depth = 4*1024;
            break;

        case RND_GEOM_MODELVIEW_OPERATION_Rotate   :
            // Tell RndVertexes to generate data as if the bbox was rotated 90 d
            // to the left about the Z axis.
            p->left    = -m_Geom.bbox.bottom;
            p->bottom  =  m_Geom.bbox.left;
            p->width   = -m_Geom.bbox.height;
            p->height  =  m_Geom.bbox.width;
            break;
    }
}

//------------------------------------------------------------------------------
// normal scaling to 1.0 in HW enabled
bool RndGeometry::NormalizeEnabled() const
{
    if (m_Geom.Normalize == RND_GEOM_VTX_NORMAL_SCALING_Corrected)
        return true;
    // else
    return false;
}

//------------------------------------------------------------------------------
// texture coords will be generated by HW
bool RndGeometry::TxGenEnabled(GLint txu) const
{
    MASSERT(txu >= 0);

    if (txu >= m_pGLRandom->NumTxCoords())
        return false;

    return (0 != m_Geom.Tx[txu].GenEnabled);
}

//------------------------------------------------------------------------------
// true if normals (and polygons) should be drawn front facing
bool RndGeometry::FaceFront() const
{
    return (0 != m_Geom.FaceFront);
}

//------------------------------------------------------------------------------
// true if polygons should be drawn with vx in clockwise order
bool RndGeometry::DrawClockWise() const
{
    return (m_Geom.FaceFront == m_Geom.CwIsFront);
}

//------------------------------------------------------------------------------
bool RndGeometry::IsCCW() const
{
    return (! m_Geom.CwIsFront);
}

//------------------------------------------------------------------------------
bool RndGeometry::ForceBigSimple() const
{
    return (0 != m_Geom.ForceBigSimple);
}

//------------------------------------------------------------------------------
//
// Divide the world up into irregular, overlapping X/Y bounding boxes,
// and draw some random stuff within each rectangular area.
//
//    We use world coordinates here.
//    Starting row and row order is random.
//    Starting column and column order is random.
//    The center of each bbox is within the view frustum.
//    The edges may be outside the view frustum.
//
//------------------------------------------------------------------------------
void RndGeometry::ResetBBox()
{
    // YStart:  set new starting Y (vertical center of first row of bboxes).
    // Ydir:    pick bottom to top(1) or top to bottom(-1).
    // Yoffset: we haven't completed any rows of bboxes yet.

    m_Geom.Ystart   = RND_GEOM_WorldBottom + (GLint)m_pFpCtx->Rand.GetRandom(0, (UINT32) RND_GEOM_WorldHeight);
    m_Geom.Ydir     = m_pFpCtx->Rand.GetRandom(0,1)*2 - 1;
    m_Geom.Yoffset  = 0;

    // Xstart:  set starting X for the first row of bboxes.
    // Xdir:    pick left to right(1) or right to left(-1) for the first row.
    // Xoffset: we haven't completed any bboxes in this row yet.

    m_Geom.Xstart   = RND_GEOM_WorldLeft + (GLint) m_pFpCtx->Rand.GetRandom(0, (UINT32)RND_GEOM_WorldWidth);
    m_Geom.Xdir     = m_pFpCtx->Rand.GetRandom(0,1)*2  - 1;
    m_Geom.Xoffset  = 0;
}

//------------------------------------------------------------------------------
void RndGeometry::NextBBox()
{
    //
    // Random size (w,h,d) of this bounding box.
    // Must be > 0, no limit on max size.
    //
    m_Geom.bbox.width  = (GLint) m_Pickers[RND_GEOM_BBOX_WIDTH].Pick();
    m_Geom.bbox.height = (GLint) m_Pickers[RND_GEOM_BBOX_HEIGHT].Pick();
    m_Geom.bbox.depth  = (GLint) m_Pickers[RND_GEOM_BBOX_DEPTH].Pick();

    if ( m_Geom.bbox.width < 1 )
        m_Geom.bbox.width = 1;
    if ( m_Geom.bbox.height < 1 )
        m_Geom.bbox.height = 1;
    if ( m_Geom.bbox.depth < 1 )
        m_Geom.bbox.depth = 1;

    //
    // Random Z location of the back (far side) of the bounding box.
    // The Z center of the bbox is guaranteed to be inside the view frustum.
    //
    GLint Zcenter = (GLint) m_Pickers[RND_GEOM_BBOX_Z_BASE].Pick() - RND_GEOM_WorldDepth/2;
    if ( Zcenter > RND_GEOM_WorldDepth/2 )
        Zcenter = RND_GEOM_WorldDepth/2;
    if ( Zcenter < -RND_GEOM_WorldDepth/2 )
        Zcenter = -RND_GEOM_WorldDepth/2;
    m_Geom.bbox.deep   = Zcenter - m_Geom.bbox.depth/2;

    if (! m_Geom.ForceBigSimple)
    {
        //
        // Random X location of the left side of the bounding box.
        //
        GLint Xcenter = m_Geom.Xstart + m_Geom.Xdir*m_Geom.Xoffset;

        if ( Xcenter > RND_GEOM_WorldRight )
            Xcenter -= RND_GEOM_WorldWidth;
        if ( Xcenter < RND_GEOM_WorldLeft )
            Xcenter += RND_GEOM_WorldWidth;

        m_Geom.bbox.left   = Xcenter - m_Geom.bbox.width/2;

        //
        // Random Y location of the bottom side of the bounding box.
        //
        GLint Ycenter = m_Geom.Ystart + m_Geom.Ydir*m_Geom.Yoffset;

        if ( Ycenter > RND_GEOM_WorldTop )
            Ycenter -= RND_GEOM_WorldHeight;
        if ( Ycenter < RND_GEOM_WorldBottom )
            Ycenter += RND_GEOM_WorldHeight;

        m_Geom.bbox.bottom = Ycenter - m_Geom.bbox.height/2;
    }
    else
    {
        //
        // For debugging, force a very simple layout for the bounding boxes.
        // Left to right, top to bottom, lower left corner always in the frustum.
        //
        m_Geom.bbox.left   = RND_GEOM_WorldLeft + m_Geom.Xoffset;
        m_Geom.bbox.bottom = RND_GEOM_WorldTop - m_Geom.Yoffset - m_Geom.bbox.height;
    }

    // Advance to the next position in this row.

    m_Geom.Xoffset += (GLint) m_Pickers[RND_GEOM_BBOX_XOFFSET].Pick();

    if ( m_Geom.Xoffset >= RND_GEOM_WorldWidth )
    {
        // Done with this row.  Start a new row.

        m_Geom.Xstart   = RND_GEOM_WorldLeft + (GLint) m_pFpCtx->Rand.GetRandom(0, (UINT32)RND_GEOM_WorldWidth);
        m_Geom.Xdir     = m_pFpCtx->Rand.GetRandom(0,1)*2  - 1;
        m_Geom.Xoffset  = 0;

        // Advance Y to next row.

        m_Geom.Yoffset += (GLint) m_Pickers[RND_GEOM_BBOX_YOFFSET].Pick();

        if ( m_Geom.Yoffset >= RND_GEOM_WorldHeight )
        {
            // Done with this screen.  Start over.

            m_Geom.Ystart   = RND_GEOM_WorldBottom + (GLint)m_pFpCtx->Rand.GetRandom(0, (UINT32) RND_GEOM_WorldHeight);
            m_Geom.Ydir     = m_pFpCtx->Rand.GetRandom(0,1)*2 - 1;
            m_Geom.Yoffset  = 0;
        }
    }
}

//------------------------------------------------------------------------------
void RndGeometry::PrintBBox(Tee::Priority pri) const
{
    Printf(pri, "BoundingBox: x %d:%d  y %d:%d  z %d:%d\n",
        m_Geom.bbox.left,   m_Geom.bbox.left   + m_Geom.bbox.width,
        m_Geom.bbox.bottom, m_Geom.bbox.bottom + m_Geom.bbox.height,
        m_Geom.bbox.deep,   m_Geom.bbox.deep   + m_Geom.bbox.depth
        );
}

//------------------------------------------------------------------------------
const char * RndGeometry::StrTxMxOperation(int op) const
{
    switch (op)
    {
        case RND_GEOM_TXMX_OPERATION_Identity   : return "Identity";
        case RND_GEOM_TXMX_OPERATION_2dRotate   : return "2dRotate";
        case RND_GEOM_TXMX_OPERATION_2dTranslate: return "2dTranslate";
        case RND_GEOM_TXMX_OPERATION_2dScale    : return "2dScale";
        case RND_GEOM_TXMX_OPERATION_2dAll      : return "2dAll";
        case RND_GEOM_TXMX_OPERATION_Project    : return "Project";
        default: MASSERT(0);                      return "IlwalidTxMxOp";
    }
}

//------------------------------------------------------------------------------
const char * RndGeometry::StrTxGenMode(GLenum gm) const
{
    return mglEnumToString(gm,"IlwalidGenMode",true);
}

//------------------------------------------------------------------------------
const char * RndGeometry::StrModelViewOperation (GLint op) const
{
    const char * OpNames[] =
    {
        "Identity",
        "Translate",
        "Scale",
        "Rotate"
    };
    if ((op < 0) || (op >= (GLint)(sizeof(OpNames)/sizeof(const char *))))
    {
        MASSERT(0); // table is out of date?
        return "ilwalidModelViewOp";
    }

    return OpNames[op];
}

