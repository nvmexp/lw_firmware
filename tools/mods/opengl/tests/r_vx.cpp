/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndVertexes: OpenGL Randomizer for vertex data.
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"     // declaration of our namespace
#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "random.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "lwos.h"         // for RM memory alloc
#include <math.h>         // atan()

using namespace GLRandom;

#define LOG_GOLDEN_VALUES 1
#define DONT_LOG_GOLDEN_VALUES 0

//------------------------------------------------------------------------------
RndVertexes::RndVertexes(GLRandomTest * pglr)
: GLRandomHelper(pglr, RND_VX_NUM_PICKERS, "Vertexes")
{
    memset(&m_VxLayout, 0, sizeof(m_VxLayout));
    memset(&m_VxLayoutOrder, 0, sizeof(m_VxLayoutOrder));
    m_CompactedVxSize = 0;
    m_Vertexes = NULL;
    m_LwrrentVertexBuf = NULL;
    m_PciVertexHandle = 0;
    m_VxBufBytes = 0;
    m_PciVertexes = NULL;
    m_VertexIndexes = NULL;
    m_LwrrentIndexBuf = NULL;
    m_UsePrimRestart = false;
    m_Method = 0;
    m_Primitive = 0;
    m_UseVBO = 0;
    m_VertexesPerSend = 0;
    m_NumVertexes = 0;
    m_NumIndexes = 0;
    m_XfbPrintRequest = false;
    m_XfbVbo[0] = 0;
    m_XfbVbo[1] = 0;
    m_XfbQuery[0] = 0;
    m_XfbQuery[1] = 0;
    m_NumXfbAttribs = 0;
    m_XfbPrintPri = Tee::PriLow;
}

//------------------------------------------------------------------------------
RndVertexes::~RndVertexes()
{
}

//------------------------------------------------------------------------------
// get picker tables from JavaScript
//
RC RndVertexes::SetDefaultPicker(GLint picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_VX_ACTIVATE_BUBBLE:
            m_Pickers[RND_VX_ACTIVATE_BUBBLE].ConfigRandom();
            m_Pickers[RND_VX_ACTIVATE_BUBBLE].AddRandItem(10, GL_TRUE);
            m_Pickers[RND_VX_ACTIVATE_BUBBLE].AddRandItem(90, GL_FALSE);
            m_Pickers[RND_VX_ACTIVATE_BUBBLE].CompileRandom();
            break;

        case RND_VX_PRIMITIVE:
            // which primitive to draw: GL_POINTS, GL_TRIANGLES, etc.
            m_Pickers[RND_VX_PRIMITIVE].ConfigRandom();
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem( 10, GL_TRIANGLES );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  7, GL_QUADS );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  5, GL_TRIANGLE_STRIP );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  5, GL_TRIANGLE_FAN );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  4, GL_QUAD_STRIP );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  2, GL_POLYGON );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  3, RND_VX_PRIMITIVE_Solid );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  1, GL_POINTS );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  1, GL_LINES );
            // There was a SW problem in the GL driver with LINE_LOOP. It may be fixed
            // but lets leave it disabled until we can test it on a seperate CL.
            //m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  1, GL_LINE_LOOP );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  1, GL_LINE_STRIP );
            m_Pickers[RND_VX_PRIMITIVE].AddRandItem(  1, GL_PATCHES );
            m_Pickers[RND_VX_PRIMITIVE].CompileRandom();
            break;

        case RND_VX_SEND_METHOD:
            // how should vertexes be sent?
            m_Pickers[RND_VX_SEND_METHOD].ConfigRandom();
            m_Pickers[RND_VX_SEND_METHOD].AddRandItem( 1, RND_VX_SEND_METHOD_DrawElements );
            m_Pickers[RND_VX_SEND_METHOD].AddRandItem( 2, RND_VX_SEND_METHOD_DrawElementsInstanced );
            m_Pickers[RND_VX_SEND_METHOD].AddRandItem( 2, RND_VX_SEND_METHOD_DrawRangeElements );
            m_Pickers[RND_VX_SEND_METHOD].AddRandItem( 1, RND_VX_SEND_METHOD_DrawArrays );
            m_Pickers[RND_VX_SEND_METHOD].AddRandItem( 2, RND_VX_SEND_METHOD_DrawArraysInstanced );
            m_Pickers[RND_VX_SEND_METHOD].AddRandItem( 2, RND_VX_SEND_METHOD_Array );
            m_Pickers[RND_VX_SEND_METHOD].AddRandItem( 1, RND_VX_SEND_METHOD_Vector );
            m_Pickers[RND_VX_SEND_METHOD].AddRandItem( 1, RND_VX_SEND_METHOD_Immediate );
            m_Pickers[RND_VX_SEND_METHOD].CompileRandom();
            break;

        case RND_VX_PER_LOOP:
            // number of random vertexes to generate within each bbox
            m_Pickers[RND_VX_PER_LOOP].ConfigRandom();
            m_Pickers[RND_VX_PER_LOOP].AddRandRange(1, 25, 100);
            m_Pickers[RND_VX_PER_LOOP].AddRandRange(1,  1, MaxVertexes);
            m_Pickers[RND_VX_PER_LOOP].CompileRandom();
            break;

        case RND_VX_W:
            // vertex W (float)
            m_Pickers[RND_VX_W].FConfigRandom();
            m_Pickers[RND_VX_W].FAddRandRange(1, 0.95f, 1.05f);
            m_Pickers[RND_VX_W].CompileRandom();
            break;

        case RND_VX_EDGE_FLAG:
            // is this an edge or not (for line or point polygon-mode)
            m_Pickers[RND_VX_EDGE_FLAG].ConfigRandom();
            m_Pickers[RND_VX_EDGE_FLAG].AddRandItem( 9, GL_TRUE );
            m_Pickers[RND_VX_EDGE_FLAG].AddRandItem( 1, GL_FALSE );
            m_Pickers[RND_VX_EDGE_FLAG].CompileRandom();
            break;

        case RND_VX_VBO_ENABLE:
            // enable LW_vertex_array_range extension
            m_Pickers[RND_VX_VBO_ENABLE].ConfigRandom();
            m_Pickers[RND_VX_VBO_ENABLE].AddRandItem(9, GL_TRUE);
            m_Pickers[RND_VX_VBO_ENABLE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_VX_VBO_ENABLE].CompileRandom();
            break;

        case RND_VX_SECCOLOR_BYTE:
            // secondary (spelwlar) color r, g, or b
            m_Pickers[RND_VX_SECCOLOR_BYTE].ConfigRandom();
            m_Pickers[RND_VX_SECCOLOR_BYTE].AddRandRange(1, 0,255);
            m_Pickers[RND_VX_SECCOLOR_BYTE].CompileRandom();
            break;

        case RND_VX_COLOR_BYTE:
            // color 0-255
            m_Pickers[RND_VX_COLOR_BYTE].ConfigRandom();
            m_Pickers[RND_VX_COLOR_BYTE].AddRandRange(1, 0,255);
            m_Pickers[RND_VX_COLOR_BYTE].CompileRandom();
            break;

        case RND_VX_ALPHA_BYTE:
            // transparency, 0-255
            m_Pickers[RND_VX_ALPHA_BYTE].ConfigRandom();
            m_Pickers[RND_VX_ALPHA_BYTE].AddRandItem(1, 255);
            m_Pickers[RND_VX_ALPHA_BYTE].AddRandRange(1, 0,255);
            m_Pickers[RND_VX_ALPHA_BYTE].CompileRandom();
            break;

        case RND_VX_NEW_GROUP:
            // called 17 times for laying out vertex array data: if always 0, all vertexes in one struct, max stride.
            m_Pickers[RND_VX_NEW_GROUP].ConfigRandom();
            m_Pickers[RND_VX_NEW_GROUP].AddRandItem(19, GL_FALSE);
            m_Pickers[RND_VX_NEW_GROUP].AddRandItem( 1, GL_TRUE);
            m_Pickers[RND_VX_NEW_GROUP].CompileRandom();
            break;

        case RND_VX_DO_SHUFFLE_STRUCT:
            // if false, (if if not vxarray), don't shuffle the order of vx attribs in vertex struct.
            m_Pickers[RND_VX_DO_SHUFFLE_STRUCT].ConfigConst(GL_TRUE);
            break;

        case RND_VX_PRIM_RESTART:
            // do/don't use LW_primitive_restart with vertex arrays.
            m_Pickers[RND_VX_PRIM_RESTART].ConfigRandom();
            m_Pickers[RND_VX_PRIM_RESTART].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_VX_PRIM_RESTART].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_VX_PRIM_RESTART].CompileRandom();
            break;

        case RND_VX_COLOR_WHITE:
            // do/don't force color==white
            m_Pickers[RND_VX_COLOR_WHITE].ConfigRandom();
            m_Pickers[RND_VX_COLOR_WHITE].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_VX_COLOR_WHITE].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_VX_COLOR_WHITE].CompileRandom();
            break;

        case RND_VX_MAXTOSEND:
            // (debugging) if < VX_PER_LOOP, rest of vertexes are not sent.
            m_Pickers[RND_VX_MAXTOSEND].ConfigConst(MaxIndexes);
            break;

        case RND_VX_INSTANCE_COUNT:
            m_Pickers[RND_VX_INSTANCE_COUNT].ConfigRandom();
            m_Pickers[RND_VX_INSTANCE_COUNT].AddRandRange(3, 1, 5);
            m_Pickers[RND_VX_INSTANCE_COUNT].AddRandRange(1, 1, 15);
            m_Pickers[RND_VX_INSTANCE_COUNT].CompileRandom();

    }
    return OK;
}

//------------------------------------------------------------------------------
// Check library/HW capabilities, etc.
//
RC RndVertexes::InitOpenGL()
{
    LwRmPtr pLwRm;
    RC rc;

    m_PciVertexes     = 0;
    m_Vertexes        = 0;
    m_VertexIndexes   = 0;

    // Allocate some data arrays:

    m_Vertexes        = new Vertex [MaxVertexes];
    m_VertexIndexes   = new GLuint [MaxIndexes];

    // Make the buffer big enough for:
    //   max vertex size * max num vertexes PLUS
    //   GLuint array of vertex indexes for LW_element_array.
    //   Plus extra indexes for LW_primitive_restart.

    m_VxBufBytes = MaxVertexes * MaxCompactedVertexSize +
                   MaxIndexes  * sizeof(GLuint);

    rc = pLwRm->AllocSystemMemory
    (
        &m_PciVertexHandle,
         DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS)
        |  DRF_DEF(OS02, _FLAGS, _LOCATION,    _PCI)
        |  DRF_DEF(OS02, _FLAGS, _COHERENCY,   _CACHED),
        m_VxBufBytes,
        m_pGLRandom->GetBoundGpuDevice()
    );
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PCI vertex array memory alloc failed\n");
        return rc;
    }

    rc = pLwRm->MapMemory(
        m_PciVertexHandle,
        0,
        m_VxBufBytes,
        (void **)&m_PciVertexes,
        0, // flags
        m_pGLRandom->GetBoundGpuSubdevice());
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "PCI vertex array memory map failed\n");
        return rc;
    }

    m_LwrrentVertexBuf = m_PciVertexes;

    CHECK_RC(AllocateXfbObjects());

    return rc;
}

//------------------------------------------------------------------------------
// Pick a primitive that is suitable for the enabled geometry program.
//
GLenum RndVertexes::AdjustPrimForPgm(GLenum prim)
{
    bool validPrim = false;
    int retry;

    if (prim == GL_PATCHES && (
             !m_pGLRandom->HasExt(GLRandomTest::ExtLW_tessellation_program5) ||
             !m_pGLRandom->m_GpuPrograms.TessProgEnabled()))
    {
        prim = GL_TRIANGLES;
    }

    switch (m_pGLRandom->m_GpuPrograms.ExpectedPrim())
    {
        default:
            // placeholder for "anything will do, thanks".
            break;

        case GL_POINTS:
            prim = GL_POINTS;
            break;

        case GL_LINES:
            for (retry = 0; retry < 5 && !validPrim; retry++)
            {
                switch (prim)
                {
                    case GL_LINES:
                    case GL_LINE_LOOP:
                    case GL_LINE_STRIP:
                        validPrim = true;
                        break;
                    default:
                        prim = m_Pickers[RND_VX_PRIMITIVE].Pick();
                        break;
                }
            }
            if (!validPrim)
                prim = GL_LINES;
            break;

        case GL_TRIANGLES:
            for (retry = 0; retry < 5 && !validPrim; retry++)
            {
                switch (prim)
                {
                    case GL_TRIANGLE_FAN:
                    case GL_POLYGON:
                    case GL_TRIANGLES:
                    case GL_TRIANGLE_STRIP:
                    case GL_QUADS:
                    case GL_QUAD_STRIP:
                        validPrim = true;
                        break;

                    default:
                        prim = m_Pickers[RND_VX_PRIMITIVE].Pick();
                        break;
                }
            }
            if (!validPrim)
                prim = GL_TRIANGLES;
            break;

        case GL_LINES_ADJACENCY_EXT:
            for (retry = 0; retry < 5 && !validPrim; retry++)
            {
                switch (prim)
                {
                    case GL_LINES_ADJACENCY_EXT:
                    case GL_LINE_STRIP_ADJACENCY_EXT:
                        validPrim = true;
                        break;
                    default:
                        prim = m_Pickers[RND_VX_PRIMITIVE].Pick();
                        break;
                }
            }
            if (!validPrim)
                prim = GL_LINES_ADJACENCY_EXT;
            break;

        case GL_TRIANGLES_ADJACENCY_EXT:
            for (retry = 0; retry < 5 && !validPrim; retry++)
            {
                switch (prim)
                {
                    case GL_TRIANGLES_ADJACENCY_EXT:
                    case GL_TRIANGLE_STRIP_ADJACENCY_EXT:
                        validPrim = true;
                        break;
                    default:
                        prim = m_Pickers[RND_VX_PRIMITIVE].Pick();
                        break;
                }
            }
            if (!validPrim)
                prim = GL_TRIANGLES_ADJACENCY_EXT;
            break;

        case GL_PATCHES:
            prim = GL_PATCHES;
            break;
    }
    return prim;
}

//------------------------------------------------------------------------------
// Pick new random state.
//
void RndVertexes::Pick()
{
    m_Pickers[RND_VX_ACTIVATE_BUBBLE].Pick();
    m_Method          = m_Pickers[RND_VX_SEND_METHOD].Pick();
    m_Primitive       = m_Pickers[RND_VX_PRIMITIVE].Pick();
    m_NumVertexes     = m_Pickers[RND_VX_PER_LOOP].Pick();
    m_UseVBO          = m_Pickers[RND_VX_VBO_ENABLE].Pick();
    m_VertexesPerSend = m_NumVertexes;
    m_UsePrimRestart  = (m_Pickers[RND_VX_PRIM_RESTART].Pick() != 0);
    m_Pickers[RND_VX_MAXTOSEND].Pick();
    m_InstanceCount   = m_Pickers[RND_VX_INSTANCE_COUNT].Pick();

    // Make necessary adjustments to the input primitive.
    m_Primitive = AdjustPrimForPgm(m_Primitive);

    if (m_Method == RND_VX_SEND_METHOD_DrawArraysInstanced ||
        m_Method == RND_VX_SEND_METHOD_DrawElementsInstanced)
    {
        if (!m_pGLRandom->HasExt(GLRandomTest::ExtARB_draw_instanced))
        {
            m_Method = RND_VX_SEND_METHOD_DrawArrays;
            m_InstanceCount = 1;
        }
    }
    else
    {
        m_InstanceCount = 1;
    }

    // Fix up NumVertexes to be in a reasonable range:
    const GLuint milwx = 4; // Want to get at least one patch or quad.
    m_NumVertexes = min<GLuint>(MaxVertexes, m_NumVertexes);
    m_NumVertexes = max(m_NumVertexes, milwx);

    // Tessellation, geometry programs, and draw_instanced can all multiply
    // the starting vertex count greatly, leading to drawing more stuff
    // than the GPU can complete in a reasonable amount of time.
    // We also use this MaxOutputVxPerDraw limit to stay within buffersize
    // when capturing final vertexes (vertex feedback).

    const float pgmMult = m_pGLRandom->m_GpuPrograms.GetVxMultiplier();
    const float maxMult = static_cast<float>(m_pGLRandom->GetMaxVxMultiplier());
    if (pgmMult * m_InstanceCount > maxMult)
    {
        m_InstanceCount = static_cast<UINT32>(max(1.0F, maxMult / pgmMult));
    }

    const int maxOutputVx = m_pGLRandom->GetMaxOutputVxPerDraw();

    while (m_NumVertexes * pgmMult * m_InstanceCount > maxOutputVx)
    {
        if (m_InstanceCount > 1)
        {
            // Try reducing instance count for DrawInstanced.
            m_InstanceCount = static_cast<UINT32>(
                max(1.0F, maxOutputVx / (m_NumVertexes * pgmMult)));
        }
        else if (m_NumVertexes > milwx)
        {
            // Try reducing how many vertexes we send.
            m_NumVertexes = max(milwx, m_NumVertexes * 2 / 3);
        }
        else
        {
            break; //nothing else we can do here.
        }
        Printf(Tee::PriLow,
               "MaxOutputVxPerDraw limit: pgmMult=%.1f inst %d numVx %d\n",
               pgmMult, m_InstanceCount, m_NumVertexes);
    }

    // Don't use the vertex buffer object if we aren't using vertex arrays.
    if ((m_Method == RND_VX_SEND_METHOD_Immediate) ||
        (m_Method == RND_VX_SEND_METHOD_Vector) ||
        (m_Method == RND_VX_SEND_METHOD_Array) ||
        !m_pGLRandom->HasExt(GLRandomTest::ExtARB_vertex_buffer_object) )
    {
        m_UseVBO      = false;
    }

    m_LwrrentIndexBuf = 0;

    // Pick vertex data.

    switch ( m_Primitive )
    {
        case GL_POINTS:               PickVertexesForPoints();           break;
        case GL_LINES_ADJACENCY_EXT:
        case GL_LINES:                PickVertexesForLines();            break;
        case GL_LINE_LOOP:            PickVertexesForLineLoops();        break;
        case GL_LINE_STRIP_ADJACENCY_EXT:
        case GL_LINE_STRIP:           PickVertexesForLineStrips();       break;
        case GL_TRIANGLES_ADJACENCY_EXT:
        case GL_TRIANGLES:            PickVertexesForTriangles();        break;
        case GL_TRIANGLE_STRIP_ADJACENCY_EXT:
        case GL_TRIANGLE_STRIP:       PickVertexesForTriangleStrips();   break;
        case GL_TRIANGLE_FAN:         PickVertexesForTriangleFans();     break;
        case GL_QUADS:                PickVertexesForQuads();            break;
        case GL_QUAD_STRIP:           PickVertexesForQuadStrips();       break;
        case GL_POLYGON:              PickVertexesForPolygons();         break;
        case GL_PATCHES:              PickVertexesForPatches();          break;

        case RND_VX_PRIMITIVE_Solid:
            PickVertexesForSolid();
            m_Primitive = GL_TRIANGLE_STRIP;
            break;

        default: MASSERT(0); // shouldn't get here!
    }
    MASSERT(m_NumVertexes <= MaxVertexes);
    MASSERT(m_NumIndexes  <= MaxIndexes);
    MASSERT(m_NumVertexes <= m_NumIndexes);
    MASSERT(0 == m_NumIndexes % m_VertexesPerSend);

    CleanupVertexes();

    if ((! m_pGLRandom->HasExt(GLRandomTest::ExtLW_primitive_restart) ||
       (m_Method != RND_VX_SEND_METHOD_DrawElementArray)))
    {
        m_UsePrimRestart  = GL_FALSE;
    }
    if (m_Method == RND_VX_SEND_METHOD_DrawArrays ||
        m_Method == RND_VX_SEND_METHOD_DrawArraysInstanced)
    {
        // Vertexes must be in order, with no double-indexing
        // or skipped vertexes.
        // This requires duplicating any vertex that is referenced
        // twice in m_VertexIndexes, so we may have a problem with fit,
        // since m_VertexIndexes is longer than m_Vertexes...
        if (m_NumIndexes > MaxVertexes)
            m_Method = RND_VX_SEND_METHOD_Immediate;
        else
            UnShuffleVertexes();

        // Warning: do this BEFORE PickVertexLayout, as m_NumVertexes changes,
        // and the compacted vx layout depends on m_NumVertexes (if there are
        // more than one group).
    }

    PickVertexLayout();

    switch ( m_Method )
    {
        default: // bad picker!
            m_Method = RND_VX_SEND_METHOD_Immediate;
            /* fall through */

        case RND_VX_SEND_METHOD_Immediate:
        case RND_VX_SEND_METHOD_Vector:
            break;

        case RND_VX_SEND_METHOD_Array:
        case RND_VX_SEND_METHOD_DrawArrays:
        case RND_VX_SEND_METHOD_DrawArraysInstanced:
        case RND_VX_SEND_METHOD_DrawElements:
        case RND_VX_SEND_METHOD_DrawElementsInstanced:
        case RND_VX_SEND_METHOD_DrawRangeElements:
            CopyVertexes(LOG_GOLDEN_VALUES);
            break;
    }

    // Special processing for Transform Feedback Feature
    // Each RndHelper must declare feedback unfeasable during the Pick() process.
    if ( m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) )
    {
        // XFB is designed to pull data from a vertex buffer object(vbo). These
        // send methods don't use vbos... So capture and/or playback is not
        //possible.
        if ((m_Method == RND_VX_SEND_METHOD_Immediate) ||
            (m_Method == RND_VX_SEND_METHOD_Vector) ||
            (m_Method == RND_VX_SEND_METHOD_Array) )
        {
            m_pGLRandom->m_Misc.XfbCaptureNotPossible();
        }

        // With Geometry & Tessellation programs enabled the final primitive is
        // determined by the PRIMITIVE_OUT statement of the most downstream
        // enabled shader.
        GLenum primitive = m_Primitive;
        if (m_pGLRandom->m_GpuPrograms.GetFinalPrimOut() != (GLenum)-1)
            primitive = m_pGLRandom->m_GpuPrograms.GetFinalPrimOut();

        // There is a HW counter that starts with zero at the beginning
        // of each GL_LINE_STRIP or GL_LINE_LOOP and increments with each
        // pixel touched. When XFB is capturing, these LINE_STRIP/LINE_LOOPs
        // get colwerted to individual line segments and the counter gets
        // reset with each segment. The end result is you will not get
        // inconsistent output between standard processing and XFB.
        if ((m_pGLRandom->m_PrimitiveModes.LineStippleEnabled() == GL_TRUE) &&
           ((primitive == GL_LINE_LOOP) || (primitive == GL_LINE_STRIP)) )
        {
            m_pGLRandom->m_Misc.XfbPlaybackNotPossible();
        }

        // When drawing QUADs directly, each indivdual quad is split into two
        // triangles. However, this splitting sets the hardware-internal edge
        // flags that tells the hardware not to draw the internal diagonal. In
        // the picture below, we are drawing the quad ABCD. First we draw
        // triangle ABC - the internal edge flag tells the hardware not to draw
        // CA. Then we draw ACD, and the internal edge flag tells the hardware
        // not to draw AC.
        // When doing XFB, the quad is also split into triangle, but the edge
        // flags are not available in the streamed-out data. So when you pass
        // the triangles back, there's nothing that prevents the hardware from
        // drawing the internal edges. The XFB spec. doesn't guarantee a
        // specific triangulation for QUADS, QUAD_STRIP, and POLYGON, so
        // anything we do isn't guaranteed to work.
        // B     C
        // +-----+
        // |    /|
        // |  /  |
        // |/    |
        // +-----+
        // A     D
        if(primitive == GL_QUADS || primitive == GL_QUAD_STRIP ||
           primitive == GL_POLYGON)
        {
            GLenum backMode = m_pGLRandom->m_PrimitiveModes.PolygonBackMode();
            GLenum frontMode = m_pGLRandom->m_PrimitiveModes.PolygonFrontMode();
            if( backMode  == GL_LINE || backMode == GL_POINT ||
                frontMode == GL_LINE || frontMode == GL_POINT )
            {
                m_pGLRandom->m_Misc.XfbPlaybackNotPossible();
            }
            // During a single-pass operation, a QUAD_STRIP looks like this:
            //
            // 1  3  5
            // +--+--+
            // |  |  |
            // +--+--+
            // 0  2  4
            //
            // It will draw quads 0123 and 2345.  When using FLAT, the two quads
            // will be drawn with the color of vertices 3 and 5.
            //
            // XFB doesn't support direct capture of QUADS, QUAD_STRIP,or
            // POLYGON. Instead, we capture triangles. What gets captured is
            // four triangles:
            //
            //  013, 032, 235, and 254
            //
            // With flat shading, the triangles will be color according to
            // vertices 3, 2, 5, and 4.  So we don't match the QUAD_STRIP pass.
            //
            // In practice, QUAD_STRIP primitives are also drawn as four
            // triangles. However, the hardware has enough internal support to
            // know that triangles 032 and 254 have provoking/flat-shading
            // vertices 3 and 5, respectively. (Effectively, they are
            // four-vertex triangles, with the fourth one used only for flat
            // shading.)
            //
            // One other note: For some primitive types, XFB capture returns
            // different triangles on G8X/G9X triangles vs. GT200 and above.
            if (m_pGLRandom->m_GpuPrograms.VxProgEnabled() &&
                m_pGLRandom->m_Lighting.GetShadeModel() == GL_FLAT)
            {
                m_pGLRandom->m_Misc.XfbPlaybackNotPossible();
            }
        }
    }

    if(m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) &&
       m_pGLRandom->m_Misc.XfbIsCapturePossible())
    {
        if (m_pGLRandom->PrintXfbBuffer() == m_pFpCtx->LoopNum)
            m_XfbPrintPri = Tee::PriNormal;
        SetupXfbAttributes();
    }

}

//------------------------------------------------------------------------------
// Print current picks to screen & logfile.
//
void RndVertexes::Print(Tee::Priority pri)
{
    // Only print when we are drawing vertexes, not pixels.
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return;
    Printf(pri, "Vertexes: Fog coord:%f %s %dx%d %s%s%s ",
        m_Vertexes[0].FogCoord,
        StrPrimitive(m_Primitive),
        m_NumIndexes/m_VertexesPerSend,
        m_VertexesPerSend,
        StrSendDrawMethod(m_Method),
        m_UseVBO ? " VBOs" : "",
        m_UsePrimRestart ? " PrimRestart" : ""
        );
    if (m_Method == RND_VX_SEND_METHOD_DrawArraysInstanced ||
        m_Method == RND_VX_SEND_METHOD_DrawElementsInstanced)
    {
        Printf(pri, "instCt %d ", m_InstanceCount);
    }
    Printf(pri, "\n    ");
    // Print out the layout of the vertex array data.

    GLuint group = att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG;
    GLuint i;
    for (i = 0; i < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG; i++)
    {
        GLuint attr = m_VxLayoutOrder[i];
        const VertexLayout * pvlo = m_VxLayout + attr;

        if (group != pvlo->Group)
        {
            // Start a new group structure.

            if (group != att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG)
            {
                // Finish the old group structure.
                Printf(pri, " }");
            }
            group = pvlo->Group;

            if (group == att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG)
                break;
            Printf(pri, "{");
        }
        Printf(pri,
            " %s %s[%d];",
            StrTypeName(pvlo->Type),
            StrAttrName(attr),
            pvlo->NumElements
            );
    }
    if (group != att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG)
    {
        // Finish the old group structure.
        Printf(pri, " }");
    }

    Printf(pri, "\n");

    if ((m_Method == RND_VX_SEND_METHOD_Immediate) ||
        (m_Method == RND_VX_SEND_METHOD_Vector))
    {
        // Below we print out the "compacted" vertex data.
        // If we are in one of the immediate send modes, we never actually built
        // this data.  Build it now, as it (won't be/wasn't) built by Send.
        CopyVertexes(DONT_LOG_GOLDEN_VALUES);
    }

    GLuint ii;
    GLuint v;
    for (ii = 0; ii < m_NumIndexes; ii++)
    {
        v = m_VertexIndexes[ii];

        Printf(pri, " %03d { ", v); //$

        group = 0;
        GLuint attridx;
        for (attridx = 0; attridx < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG; attridx++)
        {
            GLuint attr = m_VxLayoutOrder[attridx];
            const VertexLayout * pvlo = m_VxLayout + attr;

            if (pvlo->Group != group)
            {
                group = pvlo->Group;
                if (group == att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG)
                    break;
                Printf(pri, "}{ ");
            }

            const GLubyte * src = m_LwrrentVertexBuf + pvlo->Offset + v * pvlo->Stride;

            GLint comma = ' ';
            GLuint validx;
            for (validx = 0; validx < pvlo->NumElements; validx++)
            {
                switch (pvlo->Type)
                {
                    case GL_BYTE:
                    {
                        const GLbyte srcData = MEM_RD08(src);
                        Printf(pri, "%c%d", comma, srcData);
                        src += 1;
                        break;
                    }
                    case GL_UNSIGNED_BYTE:
                    {
                        const GLubyte srcData = MEM_RD08(src);
                        Printf(pri, "%c%u", comma, srcData);
                        src += 1;
                        break;
                    }
                    case GL_SHORT:
                    {
                        const GLshort srcData = MEM_RD16(src);
                        Printf(pri, "%c%d", comma, srcData);
                        src += 2;
                        break;
                    }
                    case GL_UNSIGNED_SHORT:
                    {
                        const GLushort srcData = MEM_RD16(src);
                        Printf(pri, "%c%u", comma, srcData);
                        src += 2;
                        break;
                    }
                    case GL_INT:
                    {
                        const GLint srcData = MEM_RD32(src);
                        Printf(pri, "%c%d", comma, srcData);
                        src += 4;
                        break;
                    }
                    case GL_UNSIGNED_INT:
                    {
                        const GLuint srcData = MEM_RD32(src);
                        Printf(pri, "%c%u", comma, srcData);
                        src += 4;
                        break;
                    }
                    case GL_FLOAT:
                    {
                        const UINT32 srcData32 = MEM_RD32(src);
                        const GLfloat srcData =
                            *reinterpret_cast<const GLfloat*>(&srcData32);
                        if (fabs(srcData) < 2.0)
                            Printf(pri, "%c%.2f", comma, srcData);
                        else
                            Printf(pri, "%c%.0f", comma, srcData);
                        src += 4;
                        break;
                    }
#if defined(GL_LW_half_float)
                    case GL_HALF_FLOAT_LW:
                    {
                        const UINT16 srcData = MEM_RD16(src);
                        GLfloat tmp = Utility::Float16ToFloat32(srcData);

                        if (fabs(tmp) < 2.0)
                            Printf(pri, "%c%.2f", comma, tmp);
                        else
                            Printf(pri, "%c%.0f", comma, tmp);
                        src += 2;
                        break;
                    }
#endif
                    default:
                        MASSERT(0);
                }
                comma = ',';
            } // value
            Printf(pri, " ");
        } // attr
        Printf(pri, "}\n");
    } // vertex

    const char *szMode[] =
    {   "disabled", "capture", "playback", "capture+playback" };
    // Now print out the Transform Feedback info
    Printf(pri,"Transform Feedback Mode:%s, Capture:%s, Playback:%s\n",
        szMode[m_pGLRandom->m_Misc.GetXfbMode()],
        m_pGLRandom->m_Misc.XfbIsCapturePossible() ? "possible" : "not possible",
        m_pGLRandom->m_Misc.XfbIsPlaybackPossible() ? "possible" : "not possible");

    if (m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) &&
        m_pGLRandom->m_Misc.XfbIsCapturePossible())
    {
        GLenum prim = GetXfbPrimitive(m_Primitive);
        // read the print pri if user wants to see the decoded XFB buffer
        if (m_pGLRandom->PrintXfbBuffer() == m_pFpCtx->LoopNum)
            pri = Tee::PriNormal;

        Printf(pri,"XFB primitives:%s\n",StrPrimitive(prim));
        Printf(pri,"XFB attributes:\n");
        for(ii = 0; ii < m_NumXfbAttribs; ii++)
        {
            Printf(pri, "%d {%s,%d,%d}\n",
                        ii,
                        mglEnumToString(m_XfbAttribs[ii].attrib),
                        m_XfbAttribs[ii].numComponents,
                        m_XfbAttribs[ii].index
                   );
        }
        Printf(pri,"\n");
        // we can't actually print the streamed data until the send command. So set a flag
        // to indicate a print request was issued.
        m_XfbPrintRequest = true;
    }
    Tee::FlushToDisk();
}

//------------------------------------------------------------------------------
// Report what primitive we will be drawing.
GLenum RndVertexes::Primitive() const
{
    return m_Primitive;
}

string TransformFeedbackIndexToString(uint32 index)
{
    return Utility::StrPrintf("XFB_DATA_Idx_%d", index);
}
//------------------------------------------------------------------------------
// Send current picks to library/HW.
//
RC RndVertexes::Send()
{
    RC rc = OK;

    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return OK; //!< we are drawing pixels in ths round.

    GLuint tmpVertexesPerSend = m_VertexesPerSend;

    switch (m_Method)
    {
        default:
            MASSERT(0);
            /* fall through */

        case RND_VX_SEND_METHOD_Immediate:
        case RND_VX_SEND_METHOD_Vector:
            break;

        case RND_VX_SEND_METHOD_Array:
        case RND_VX_SEND_METHOD_DrawArrays:
        case RND_VX_SEND_METHOD_DrawArraysInstanced:
        case RND_VX_SEND_METHOD_DrawElements:
        case RND_VX_SEND_METHOD_DrawElementsInstanced:
        case RND_VX_SEND_METHOD_DrawRangeElements:
        {
            //Setup for vertex pulling.
            SetupVertexArrays(m_XfbVbo[0]);
#ifdef GL_LW_primitive_restart
            if (m_UsePrimRestart)
            {
                glEnableClientState(GL_PRIMITIVE_RESTART_LW);

                if (m_pGLRandom->m_VertexFormat.IndexType() == GL_UNSIGNED_SHORT)
                    glPrimitiveRestartIndexLW(0x0000ffff);
                else
                    glPrimitiveRestartIndexLW(0xffffffff);

                tmpVertexesPerSend = m_NumIndexes + (m_NumIndexes / m_VertexesPerSend) - 1;

                // Only the DrawElementArray case below understands tmpVertexesPerSend...
                MASSERT(m_Method == RND_VX_SEND_METHOD_DrawElementArray);
            }
#endif

            if (m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) && m_pGLRandom->m_Misc.XfbIsCapturePossible())
            {
                glTransformFeedbackAttribsLW(m_NumXfbAttribs,&m_XfbAttribs[0].attrib,GL_INTERLEAVED_ATTRIBS_LW);

                glBindBufferBaseLW(GL_TRANSFORM_FEEDBACK_BUFFER_LW, // target
                                    0,                              // index should be zero
                                    m_XfbVbo[1]);                      // vbo

                if (m_pGLRandom->GetLogMode() != GLRandomTest::Skip)
                {
                    // Initialize the buffer before each capture when we will be CRCing it as
                    // an array of bytes, to prevent false miscompares.
                    GLvoid * p = glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW, GL_READ_WRITE);
                    Platform::MemSet(p, 0, MaxXfbCaptureBytes);
                    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW);
                }

                glBeginTransformFeedbackLW(GetXfbPrimitive(m_Primitive));

                glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_LW, m_XfbQuery[0]);
                glBeginQuery(GL_PRIMITIVES_GENERATED_LW, m_XfbQuery[1]);

                // Don't send the vertex data directly to the rasterizer, instead
                // use the transform feedback vbo data.
                // This is to prove the vertex data going to the feedback vbo is
                // identical to what would normally go to the rasterizer.
                if( m_pGLRandom->m_Misc.XfbEnabled(xfbModePlayback) &&
                    m_pGLRandom->m_Misc.XfbIsPlaybackPossible())
                {
                    glEnable(GL_RASTERIZER_DISCARD_LW);
                }
            }
        }
    }

    // Set a global value for all the vertex attributes.
    // The PickVertexes() function nicely filled in a value for ALL ATTRIBUTES
    // on vertex 0 for us to use here.
    //
    // Not doing this causes nasty repeatability bugs...

    glNormal3fv(m_Vertexes[0].Normal);

    glColor4ubv(m_Vertexes[0].Color);

    if (m_pGLRandom->HasExt(GLRandomTest::ExtEXT_secondary_color))
        glSecondaryColor3ubv(m_Vertexes[0].SecColor);

    if (m_pGLRandom->HasExt(GLRandomTest::ExtEXT_fog_coord))
        glFogCoordf(m_Vertexes[0].FogCoord);

    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_vertex_program))
    {
        glVertexAttrib4ubvLW(1, m_Vertexes[0].Color);
        glVertexAttrib4ubvLW(6, m_Vertexes[0].Color);
        glVertexAttrib4ubvLW(7, m_Vertexes[0].Color);
    }

    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
    {
        glMultiTexCoord4fv(GL_TEXTURE0_ARB + txu, m_Vertexes[0].Tx[txu]);
    }

    glEdgeFlag(m_Vertexes[0].EdgeFlag);

    if (att_COL0 != m_pGLRandom->m_GpuPrograms.Col0Alias())
        glVertexAttrib4ubvLW(m_pGLRandom->m_GpuPrograms.Col0Alias(), m_Vertexes[0].Color);

    // Let the tessellation primitive generator in hw know how many vertices
    // per patch.
    if (m_pGLRandom->m_GpuPrograms.TessProgEnabled())
    {
        glPatchParameteri(GL_PATCH_VERTICES, (GLint)m_VertexesPerSend);
    }

    // Send down the vertexes.
    GLuint vx;
    for ( vx = 0; vx < m_NumIndexes; vx += tmpVertexesPerSend )
    {
        switch ( m_Method )
        {
            default:
                MASSERT(0);
                /* fall through */

            case RND_VX_SEND_METHOD_Immediate:
            case RND_VX_SEND_METHOD_Vector:
            case RND_VX_SEND_METHOD_Array:
                SendDrawBeginEnd(vx);
                break;

            case RND_VX_SEND_METHOD_DrawElements:
                glDrawElements(
                    m_Primitive,
                    m_VertexesPerSend,
                    GL_UNSIGNED_INT,
                    m_VertexIndexes + vx );
                break;

            case RND_VX_SEND_METHOD_DrawElementsInstanced:
                glDrawElementsInstancedEXT(
                    m_Primitive,
                    m_VertexesPerSend,
                    GL_UNSIGNED_INT,
                    m_VertexIndexes + vx,
                    m_InstanceCount);
                break;

            case RND_VX_SEND_METHOD_DrawRangeElements:
                glDrawRangeElements(
                    m_Primitive,
                    0,
                    m_NumVertexes - 1,
                    m_VertexesPerSend,
                    GL_UNSIGNED_INT,
                    m_VertexIndexes + vx );
                break;

            case RND_VX_SEND_METHOD_DrawArrays:
                glDrawArrays(
                    m_Primitive,
                    vx,
                    m_VertexesPerSend );
                break;

            case RND_VX_SEND_METHOD_DrawArraysInstanced:
                glDrawArraysInstancedEXT(
                    m_Primitive,
                    vx,
                    m_VertexesPerSend,
                    m_InstanceCount);
                break;
        }
        if(ActivateBubble() && (vx == 0))
        {
            CHECK_RC(m_pGLRandom->ActivateBubble());
        }
    }

    if( m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) &&
        m_pGLRandom->m_Misc.XfbIsCapturePossible() )
    {
        GLuint prim_written = 0;
        GLuint prim_generated = 0;

        glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_LW);
        glEndQuery(GL_PRIMITIVES_GENERATED_LW);

        glGetQueryObjectuiv(m_XfbQuery[0], GL_QUERY_RESULT, &prim_written);
        glGetQueryObjectuiv(m_XfbQuery[1], GL_QUERY_RESULT, &prim_generated);

        glEndTransformFeedbackLW();

        if( m_pGLRandom->m_Misc.XfbEnabled(xfbModePlayback) &&
            m_pGLRandom->m_Misc.XfbIsPlaybackPossible())
        {
            glDisable(GL_RASTERIZER_DISCARD_LW);
        }

        // If prim_generated > prim_written then the buffer is full.
        if (prim_written != prim_generated)
        {
            Printf(Tee::PriHigh,
                   "Loop:%d XFB prim_written:%d != prim_generated:%d\n",
                   m_pFpCtx->LoopNum, prim_written, prim_generated);
            MASSERT(false);
        }

        m_XfbVerticesWritten = GetXfbVertices(m_Primitive, prim_written);

        if (m_XfbPrintRequest || (m_pGLRandom->GetLogMode() != GLRandomTest::Skip))
        {
            // map the vbo2 back to user memory and log the data
            GLvoid * pVertexData = glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW,
                                               GL_READ_WRITE);
            MASSERT( pVertexData != NULL);

            m_pGLRandom->LogBegin(1, TransformFeedbackIndexToString);
            m_pGLRandom->LogData(0, pVertexData,
                                 (m_XfbVerticesWritten * m_NumXfbAttribs *
                                  sizeof(GLfloat)));
            rc = m_pGLRandom->LogFinish("XFB");

            if (RC::SOFTWARE_ERROR == rc || m_XfbPrintRequest ||
                (m_pGLRandom->PrintXfbBuffer() == m_pFpCtx->LoopNum))
            {
                PrintVBO(m_XfbPrintPri, pVertexData,m_XfbVerticesWritten,
                         prim_written,m_NumXfbAttribs,m_XfbAttribs);
                m_XfbPrintRequest = false; // clear the flag.
            }
            glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER_LW);
        }
    }

    return rc;
}

RC RndVertexes::Playback()
{
    if ((RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction()) || //!< we are drawing pixels in this round.
        !m_pGLRandom->m_Misc.XfbIsPlaybackPossible() ||
        !m_pGLRandom->m_Misc.XfbEnabled(xfbModePlayback) )
        return OK;

    //disable clientStates and reset pointers for m_XfbVbo[0]
    TurnOffVertexArrays();

    // remove the internal bind point to the Transform feedback
    glBindBufferBaseLW(GL_TRANSFORM_FEEDBACK_BUFFER_LW,0,0);

    glBindBuffer(GL_ARRAY_BUFFER,m_XfbVbo[1]);

    SetupXfbPointers(); // for m_XfbVbo[1]

    // send out the streamed vertices
    glDrawArrays(GetXfbPrimitive(m_Primitive),0,m_XfbVerticesWritten );

    // glFinish(); // Debug

    return mglGetRC();
}

//------------------------------------------------------------------------------
// Decode and print the contents of the Vertex Buffer Object based on the
// Attributes sent down with the vertices. The contents of the feedback vbo
// follow the Attributes array passed in during the glTransformFeedbackAttribsLW()
// call. All values are 32bit floating point.
void RndVertexes::PrintVBO( Tee::Priority pri,
                            GLvoid * Data,
                            GLint vertWritten,
                            GLint primWritten,
                            GLint numAttribs,
                            XfbAttrRec * pAttribs)
{
    GLfloat * pData = (GLfloat*)Data;
    GLint comp;
    GLfloat vp[4];    //!< The default full-screen viewport.

    static const char *colAttrNames[] =
    {
        "Col0",
        "Col1",
        "BkCol0",
        "BkCol1"
    };
    UINT32 colAttrNamesIdx = 0;

    glGetFloatv(GL_VIEWPORT, vp); //!< Get the viewport params, x,y,w,h

    Printf(pri,"VBO Data: PrimType:%s TFPrimType:%s prims:%3.3d verts:%3.3d  attrs:%3.3d\n",
        StrPrimitive(m_Primitive),
        StrPrimitive(GetXfbPrimitive(m_Primitive)),
        primWritten,
        vertWritten,
        numAttribs);

    for (int i = 0; i < vertWritten; i++)
    {
        Printf(pri,"%5.5d: ", i);
        const UINT32 * pLine = reinterpret_cast<UINT32*>(pData);
        for (int ii = 0; ii < numAttribs; ii++)
        {
            colAttrNamesIdx = 0;
            switch ( pAttribs[ii].attrib )
            {
                case GL_POSITION:
                {
                    // The window coordinate space is a continuous floating-point space.
                    // Pixels are centered around (X.5, Y.5), where X and Y are integers.
                    // The origin is in the lower-left corner. So the coordinate space in
                    // a 1x1 window looks like:
                    //
                    //  (0.0,1.0) (1.0,1,0)             Clip coordinates are in a space
                    //      +---------+                 before we account for perspective.
                    //      |         |                 For normal projections, the W clip
                    //      |         |                 coordinate holds the distance from
                    //      |    X <--|-- (0.5, 0.5)    the eye along the view direction.
                    //      |         |                 The Z clip coordinate doesnt have
                    //      |         |                 any real-world meaning  it is used
                    //      +---------+                 to distribute the view volume over
                    //  (0.0,0,0) (1.0,0.0)             the range of Z buffer values  its
                    //                                  chosen so that Z == -W at the front
                    // clip plane and Z == +W at the back. You need to divide through by W to
                    // figure out the (X,Y) location on the screen.
                    //
                    // The transformation of clip coordinates has two parts. First, we divide
                    // through by W. This yields normalized device coordinates:
                    //
                    //  (X_ndc, Y_ndc, Z_ndc) = (X_clip/W_clip, Y_clip/W_clip, Z_clip/W_clip)
                    //
                    // The view volume in normalized device coordinates is the regular lwbe from
                    // (-1,-1,-1) to (+1,+1,+1). Then, we project to window coordinates using the
                    // glViewport and glDepthRange state. This one centers the NDC lwbe into the
                    // viewport and depth ranges
                    //
                    //  X_window = X_ndc * (0.5*viewport.w)+ (viewport.x + viewport.w/2)
                    //  Y_window = Y_ndc * (0.5*viewport.h) + (viewport.y + viewport.h/2)
                    //  Z_window = Z_ndc * (0.5*(depth_range.f-depth_range.n)) + 0.5*(depth_range.f+depth_range.n)
                    //
                    // Finally, if you want to test a screenshot that has (0,0) at the upper left,
                    // you need to account for the lower-left origin:
                    //
                    //  Y_screen = window.h - Y_window
                    //
                    GLfloat w = (pAttribs[ii].numComponents == 4) ? pData[3] : 1.0f;
                    GLint xWin = (GLint)((pData[0]/w) * (0.5f* vp[2])+(vp[0]+ vp[2]/2.0f));
                    GLint yWin = (GLint)(vp[3] - ((pData[1]/w) * (0.5f*vp[3])+(vp[1]+vp[3]/2.0f)));

                    Printf(pri,"Win(x:%d y:%d) ", xWin,yWin);
                    Printf(pri, "Pos(");
                    for (comp = 0; comp < pAttribs[ii].numComponents; comp++,pData++)
                    {
                        switch(comp)
                        {
                            case 0: Printf(pri,"x:%.2f", *pData); break;
                            case 1: Printf(pri," y:%.2f", *pData); break;
                            case 2: Printf(pri," z:%.2f", *pData); break;
                            case 3: Printf(pri," w:%.2f", *pData); break;
                        }
                    }
                    Printf(pri,")");
                    break;
                }
                case GL_TEXTURE_COORD_LW:   // Texture coordinates only support GL_FLOAT
                case GL_GENERIC_ATTRIB_LW:
                    Printf(pri, "%s%d(",
                        (pAttribs[ii].attrib == GL_TEXTURE_COORD_LW) ? "Txu" : "GenAttr",
                        pAttribs[ii].index);

                    for (comp = 0; comp < pAttribs[ii].numComponents; comp++,pData++)
                    {
                        switch(comp)
                        {
                            case 0: Printf(pri,"x:%.2f", *pData); break;
                            case 1: Printf(pri," y:%.2f", *pData); break;
                            case 2: Printf(pri," z:%.2f", *pData); break;
                            case 3: Printf(pri," w:%.2f", *pData); break;
                        }
                    }
                    Printf(pri,")");
                    break;
                case GL_BACK_SECONDARY_COLOR_LW:  // colAttrNamesIdx = 3, label = "BkCol1"
                    colAttrNamesIdx++;
                    /* fall through */
                case GL_BACK_PRIMARY_COLOR_LW:    // colAttrNamesIdx = 2, label = "BkCol0"
                    colAttrNamesIdx++;
                    /* fall through */
                case GL_SECONDARY_COLOR_LW:       // colAttrNamesIdx = 1, label = "Col1"
                    colAttrNamesIdx++;
                    /* fall through */
                case GL_PRIMARY_COLOR:            // colAttrNamesIdx = 0, label = "Col0"
                    Printf(pri, "%s(",colAttrNames[colAttrNamesIdx]);
                    for (comp = 0; comp < pAttribs[ii].numComponents; comp++,pData++)
                    {
                        switch(comp)
                        {
                            case 0: Printf(pri,"r:%.2f", *pData); break;
                            case 1: Printf(pri," g:%.2f", *pData); break;
                            case 2: Printf(pri," b:%.2f", *pData); break;
                            case 3: Printf(pri," a:%.2f", *pData); break;
                        }
                    }
                    Printf(pri,")");
                    break;

                case GL_FOG_COORDINATE:     // Fog only supports GL_FLOAT
                    Printf(pri, "Fog(%.2f) ",*pData);
                    pData++;
                    break;

                case GL_POINT_SIZE:
                    Printf(pri, "PtSz(%.2f) ",*pData);
                    pData++;
                    break;

                case GL_PRIMITIVE_ID_LW:
                    Printf(pri, "PrimId(%.2f) ",*pData);
                    pData++;
                    break;

                case GL_VERTEX_ID_LW:
                    Printf(pri, "VertId(%.2f) ",*pData);
                    pData++;
                    break;

                case GL_CLIP_DISTANCE_LW:
                    Printf(pri, "ClpDst(%.2f) ",*pData);
                    pData++;
                    break;

                default:
                    break;
            }
        }
        Printf(pri, "\n");
        Printf(pri, "(raw): ");
        const UINT32 * pLineEnd = reinterpret_cast<UINT32*>(pData);
        while (pLine < pLineEnd)
            Printf(pri, " %08x", *pLine++);
        Printf(pri,"\n");
    }
    Printf(pri,"\n");
}

//------------------------------------------------------------------------------
// Free all allocated resources.
//
RC RndVertexes::Cleanup()
{
    RC rc;
    LwRmPtr pLwRm;
    if ( 0 != m_PciVertexes )
        pLwRm->Free(m_PciVertexHandle);

    if ( m_Vertexes )
        delete [] m_Vertexes;
    if ( m_VertexIndexes )
        delete []m_VertexIndexes;

    m_PciVertexes  = 0;
    m_VertexIndexes = 0;
    m_Vertexes     = 0;

    if (m_pGLRandom->IsGLInitialized())
    {
        if (m_XfbVbo[0])
            glDeleteBuffers(1, &m_XfbVbo[0]);

        if (m_XfbVbo[1])
            glDeleteBuffers(1, &m_XfbVbo[1]);

        if (m_XfbQuery[0])
            glDeleteQueries(1, &m_XfbQuery[0]);

        if (m_XfbQuery[1])
            glDeleteQueries(1, &m_XfbQuery[1]);

        rc = mglGetRC();
    }
    return rc;
}

//------------------------------------------------------------------------------
// Fill in m_Vertexes[] with _something_, may be overwritten later.
//
void RndVertexes::FillVertexData()
{
    RndGeometry::BBox box;
    m_pGLRandom->m_Geometry.GetBBox(&box);

    // For some reason box width or height can be negative.
    // Make sure we correctly request the numbers from the bounding box's range
    // from the RNG, otherwise we would get some garbage.
    const auto leftMin   = min(box.left, box.left + box.width - 1);
    const auto leftMax   = max(box.left, box.left + box.width - 1);
    const auto bottomMin = min(box.bottom, box.bottom + box.height - 1);
    const auto bottomMax = max(box.bottom, box.bottom + box.height - 1);
    const auto depthMin  = min(box.deep, box.deep + box.depth - 1);
    const auto depthMax  = max(box.deep, box.deep + box.depth - 1);

    // Always fill in all attributes of vertex 0, since we send them
    //   as the global non-changing values for attributes that aren't changing
    //   per-vertex.

    m_Vertexes[0].Coords[0] = m_pFpCtx->Rand.GetRandomFloat(leftMin, leftMax);
    m_Vertexes[0].Coords[1] = m_pFpCtx->Rand.GetRandomFloat(bottomMin, bottomMax);
    m_Vertexes[0].Coords[2] = m_pFpCtx->Rand.GetRandomFloat(depthMin, depthMax);
    m_Vertexes[0].Coords[3] = m_Pickers[RND_VX_W].FPick();

    m_pGLRandom->PickNormalfv(m_Vertexes[0].Normal, m_pGLRandom->m_Geometry.FaceFront());

    m_Vertexes[0].FogCoord = (GLfloat) RND_GEOM_WorldDepth - box.deep;

    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
    {
        m_Vertexes[0].Tx[txu][0] = m_pFpCtx->Rand.GetRandomFloat(-.5, 1.5);
        m_Vertexes[0].Tx[txu][1] = m_pFpCtx->Rand.GetRandomFloat(-.5, 1.5);
        m_Vertexes[0].Tx[txu][2] = m_pFpCtx->Rand.GetRandomFloat(-.5, 1.5);
        m_Vertexes[0].Tx[txu][3] = m_Pickers[RND_VX_W].FPick();
    }

    if (m_Pickers[RND_VX_COLOR_WHITE].Pick())
    {
        m_Vertexes[0].Color[0] = 255;
        m_Vertexes[0].Color[1] = 255;
        m_Vertexes[0].Color[2] = 255;
        m_Vertexes[0].Color[3] = 255;
        m_Vertexes[0].SecColor[0] = 255;
        m_Vertexes[0].SecColor[1] = 255;
        m_Vertexes[0].SecColor[2] = 255;
    }
    else
    {
        m_Vertexes[0].Color[0] = m_Pickers[RND_VX_COLOR_BYTE].Pick();
        m_Vertexes[0].Color[1] = m_Pickers[RND_VX_COLOR_BYTE].Pick();
        m_Vertexes[0].Color[2] = m_Pickers[RND_VX_COLOR_BYTE].Pick();
        m_Vertexes[0].Color[3] = m_Pickers[RND_VX_ALPHA_BYTE].Pick();
        m_Vertexes[0].SecColor[0] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
        m_Vertexes[0].SecColor[1] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
        m_Vertexes[0].SecColor[2] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
    }

    m_Vertexes[0].EdgeFlag = (GLbyte) m_Pickers[RND_VX_EDGE_FLAG].Pick();

    Vertex * pV    = m_Vertexes + 1;
    Vertex * pVEnd = m_Vertexes + m_NumVertexes;

    for (/**/; pV < pVEnd; pV++)
    {
        pV->Coords[0] = m_pFpCtx->Rand.GetRandomFloat(leftMin, leftMax);
        pV->Coords[1] = m_pFpCtx->Rand.GetRandomFloat(bottomMin, bottomMax);
        pV->Coords[2] = m_pFpCtx->Rand.GetRandomFloat(depthMin, depthMax);
        pV->Coords[3] = m_Pickers[RND_VX_W].FPick();

        if (m_pGLRandom->m_VertexFormat.Size(att_NRML))
        {
            m_pGLRandom->PickNormalfv(pV->Normal, m_pGLRandom->m_Geometry.FaceFront());
        }
        else
        {
            pV->Normal[0] = 0.0F;
            pV->Normal[1] = 0.0F;
            pV->Normal[2] = 1.0F;
        }

        pV->FogCoord = RND_GEOM_WorldDepth - pV->Coords[2];

        GLuint tx;
        for (tx = 0; tx < (GLuint)m_pGLRandom->NumTxCoords(); tx++)
        {
            pV->Tx[tx][0] = pV->Normal[0];
            pV->Tx[tx][1] = pV->Normal[1];
            pV->Tx[tx][2] = pV->Normal[2];
            pV->Tx[tx][3] = pV->Coords[3];
        }

        if (m_pGLRandom->m_VertexFormat.Size(att_COL0) &&
            !m_Pickers[RND_VX_COLOR_WHITE].Pick())
        {
            pV->Color[0] = m_Pickers[RND_VX_COLOR_BYTE].Pick();
            pV->Color[1] = m_Pickers[RND_VX_COLOR_BYTE].Pick();
            pV->Color[2] = m_Pickers[RND_VX_COLOR_BYTE].Pick();
            pV->Color[3] = m_Pickers[RND_VX_ALPHA_BYTE].Pick();
        }
        else
        {
            pV->Color[0] = 255;
            pV->Color[1] = 255;
            pV->Color[2] = 255;
            pV->Color[3] = 255;
        }

        if (m_pGLRandom->m_VertexFormat.Size(att_COL1) &&
            !m_Pickers[RND_VX_COLOR_WHITE].Pick())
        {
            pV->SecColor[0] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
            pV->SecColor[1] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
            pV->SecColor[2] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
        }
        else
        {
            pV->SecColor[0] = 255;
            pV->SecColor[1] = 255;
            pV->SecColor[2] = 255;
        }
        if (m_pGLRandom->m_VertexFormat.Size(att_EdgeFlag))
            pV->EdgeFlag = (GLbyte) m_Pickers[RND_VX_EDGE_FLAG].Pick();
        else
            pV->EdgeFlag = GL_TRUE;
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForPoints()
{
    m_NumIndexes = m_NumVertexes + (m_NumVertexes>>2);
    m_VertexesPerSend = m_NumIndexes;

    SetupShuffledIndexArray();
    FillVertexData();
    RandomFillTxCoords();

    if ( m_pGLRandom->m_Geometry.ForceBigSimple() )
    {
        m_NumVertexes = 1;
        m_NumIndexes = 1;
        m_VertexesPerSend = 1;
        SetupInorderIndexArray();

        RndGeometry::BBox box;
        m_pGLRandom->m_Geometry.GetBBox(&box);
        m_Vertexes[0].Coords[0] = (GLfloat) (box.left + box.width/2.0);
        m_Vertexes[0].Coords[1] = (GLfloat) (box.bottom + box.height/2.0);
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForLines()
{
    m_NumIndexes = (m_NumVertexes + (m_NumVertexes>>2)) & ~1;
    m_VertexesPerSend = m_NumIndexes;

    SetupShuffledIndexArray();
    FillVertexData();
    RandomFillTxCoords();
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForLineLoops()
{
    m_NumIndexes = m_NumVertexes + (m_NumVertexes>>2);

    if ( (m_NumIndexes % 4) == 0 )
        m_VertexesPerSend = 4;
    else if ( (m_NumIndexes %3) == 0 )
        m_VertexesPerSend = 3;
    else
        m_VertexesPerSend = m_NumIndexes;

    SetupShuffledIndexArray();
    FillVertexData();
    RandomFillTxCoords();
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForLineStrips()
{
    m_NumIndexes = m_NumVertexes + (m_NumVertexes>>2);

    if ( (m_NumIndexes % 4) == 0 )
        m_VertexesPerSend = 4;
    else if ( (m_NumIndexes % 3) == 0 )
        m_VertexesPerSend = 3;
    else
        m_VertexesPerSend = m_NumIndexes;

    SetupShuffledIndexArray();
    FillVertexData();
    RandomFillTxCoords();
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForTriangles()
{
    if ( m_pGLRandom->m_Geometry.ForceBigSimple() )
    {
        // One triangle, with vertexes at 3 of the 4 XyBox corners.

        m_NumVertexes = 3;
        m_NumIndexes  = 3;
        m_VertexesPerSend = m_NumIndexes;

        SetupInorderIndexArray();
        FillVertexData();

        UINT32 XyBoxCorners[] = {0,1,2,3};   // lowLeft, lowRight, upLeft, upRight
        RndGeometry::BBox box;
        m_pGLRandom->m_Geometry.GetBBox(&box);

        GLuint vx;
        for (vx = 0; vx < m_VertexesPerSend; ++vx)
        {
            m_Vertexes[vx].Coords[0] = (GLfloat) box.left;
            m_Vertexes[vx].Coords[1] = (GLfloat) box.bottom;

            if ( XyBoxCorners[vx] & 1 )
                m_Vertexes[vx].Coords[0] += (GLfloat) (box.width - 1);
            if ( XyBoxCorners[vx] & 2 )
                m_Vertexes[vx].Coords[1] += (GLfloat) (box.height - 1);
        }
    }
    else
    {
        m_NumVertexes = (m_NumVertexes / 3) * 3;
        m_NumIndexes  = m_NumVertexes;
        m_VertexesPerSend = m_NumVertexes;

        SetupShuffledIndexArray();
        FillVertexData();
    }

    // Make all the triangles CW or CCW, to match m_pGLRandom->m_Geometry.DrawClockWise().
    // (Determines front/back facing, see r_geom.cpp).

    GLuint tri;
    for ( tri = 0; tri < (m_NumVertexes/3); tri++ )
    {
        GLuint vx0 = m_VertexIndexes[ tri*3 ];
        GLuint vx1 = m_VertexIndexes[ tri*3 + 1 ];
        GLuint vx2 = m_VertexIndexes[ tri*3 + 2 ];

        if ( TriangleIsClockWise(vx0, vx1, vx2) != m_pGLRandom->m_Geometry.DrawClockWise() )
        {
            FixFacingBySwappingVx12(vx0, vx1, vx2);
        }

        GenerateTexCoords(true, vx0, vx1, vx2);
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForTriangleStrips()
{
    // Decide how many triangles per strip:

    if ( (m_NumVertexes % 19) == 0 )
        m_VertexesPerSend = 19;         // 17 triangles/strip
    else if ( (m_NumVertexes % 17) == 0 )
        m_VertexesPerSend = 17;         // 15 triangles/strip
    else if ( (m_NumVertexes % 13) == 0 )
        m_VertexesPerSend = 13;         // 11 triangles/strip
    else if ( (m_NumVertexes % 11) == 0 )
        m_VertexesPerSend = 11;         // 9 triangles/strip
    else if ( (m_NumVertexes % 7) == 0 )
        m_VertexesPerSend = 7;          // 5 triangles/strip
    else if ( (m_NumVertexes % 5) == 0 )
        m_VertexesPerSend = 5;          // 3 triangles/strip
    else if ( (m_NumVertexes % 4) == 0 )
        m_VertexesPerSend = 4;          // 2 triangles/strip
    else if ( (m_NumVertexes % 3) == 0 )
        m_VertexesPerSend = 3;          // 1 triangle/strip
    else if ( m_NumVertexes > 32 )
        m_VertexesPerSend = 32;         // 30 triangles/strip
    else
        m_VertexesPerSend = m_NumVertexes;  // all in one strip.

    m_NumVertexes -= (m_NumVertexes % m_VertexesPerSend);
    m_NumIndexes = m_NumVertexes;

    SetupShuffledIndexArray();
    FillVertexData();

    // Force all the triangles in each strip to be clockwise or ccwise.

    GLuint strip;
    GLuint numStrips = m_NumVertexes / m_VertexesPerSend;
    GLuint tri;
    GLuint vx0;
    GLuint vx1;
    GLuint vx2;

    for (strip = 0; strip < numStrips; strip++)
    {
        for (tri = 0; tri < (m_VertexesPerSend-2); tri++)
        {
            // GL tri-strips use vertexes in a funny order, so that the whole
            // strip can be front-facing or back-facing:
            //
            //    Triangle 0 uses indexes 0,1,2
            //    Triangle 1 uses indexes 2,1,3
            //    Triangle 2 uses indexes 2,3,4
            //    Triangle 3 uses indexes 4,3,5
            //    etc

            vx0 = m_VertexIndexes[ strip*m_VertexesPerSend + tri ];
            vx1 = m_VertexIndexes[ strip*m_VertexesPerSend + tri + 1 ];
            vx2 = m_VertexIndexes[ strip*m_VertexesPerSend + tri + 2 ];

            if ( tri & 1)
            {
                GLuint tmp = vx0;
                vx0 = vx1;
                vx1 = tmp;
            }

            if (TriangleIsClockWise(vx0, vx1, vx2) != m_pGLRandom->m_Geometry.DrawClockWise())
            {
                if ( tri == 0 )
                {
                    FixFacingBySwappingVx12(vx0, vx1, vx2);
                }
                else
                {
                    // This version is slower, but we can't change vx1 at this
                    // point without screwing up the earlier triangles in the strip.
                    FixFacingByTransformingVx2(vx0, vx1, vx2);
                }
            }
            GenerateTexCoords(tri==0, vx0, vx1, vx2);
        }
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForTriangleFans()
{
    // Decide how many vertices per triangle fan (and how many fans this loop).
    for (m_VertexesPerSend = 16; m_VertexesPerSend > 3; m_VertexesPerSend--)
    {
        if (0 == m_NumVertexes % m_VertexesPerSend)
            break;
    }

    m_NumVertexes -= (m_NumVertexes % m_VertexesPerSend);
    m_NumIndexes = m_NumVertexes;

    SetupShuffledIndexArray();
    FillVertexData();

    const GLuint numFans = m_NumVertexes / m_VertexesPerSend;

    for (GLuint fan = 0; fan < numFans; fan++)
    {
        GLuint * pVxIdx = m_VertexIndexes + fan * m_VertexesPerSend;

        GenerateColwexPolygon(pVxIdx, m_VertexesPerSend, true);

        const GLint vxCenter = pVxIdx[0];

        for (GLuint tri = 0; tri < (m_VertexesPerSend-2); tri++)
        {
            GenerateTexCoords(tri==0, vxCenter, pVxIdx[tri+1], pVxIdx[tri+2]);
        }
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForQuads()
{
    if ( m_pGLRandom->m_Geometry.ForceBigSimple() )
    {
        // One quad, with vertexes at 3 of the 4 BBox corners.

        m_NumVertexes = 4;
        m_NumIndexes  = 4;
        m_VertexesPerSend = m_NumVertexes;

        SetupInorderIndexArray();
        FillVertexData();

        // Decide on clockwise (1) or counter-clockwise(-1).
        // Default: assume CCW==front and back-facing, so quad should be CW.

        GLint direction;

        if (m_pGLRandom->m_Geometry.DrawClockWise())
            direction = 1;
        else
            direction = -1;

        RndGeometry::BBox box;
        m_pGLRandom->m_Geometry.GetBBox(&box);

        GLuint startCorner = 0;
        GLuint vx;

        for (vx = 0; vx < m_VertexesPerSend; ++vx)
        {
            m_Vertexes[vx].Coords[0] = (GLfloat) box.left;
            m_Vertexes[vx].Coords[1] = (GLfloat) box.bottom;

            GLint corner = startCorner + direction * vx;
            if ( corner > 3 )
                corner -= 4;
            if ( corner < 0 )
                corner += 4;

            switch ( corner )
            {
                case 0:  // lower left
                    break;

                case 1:  // upper left
                    m_Vertexes[vx].Coords[1] += box.height - 1;
                    break;

                case 2:  // upper right
                    m_Vertexes[vx].Coords[0] += box.width - 1;
                    m_Vertexes[vx].Coords[1] += box.height - 1;
                    break;

                case 3:  // lower right
                    m_Vertexes[vx].Coords[0] += box.width - 1;
                    break;
            }
        }

        // Extrapolate a planar Z for vertex 3.
        // This is pretty easy, given that we know we have a rectangle.

        double vx0Z = m_Vertexes[0].Coords[2];
        double vx1Z = m_Vertexes[1].Coords[2];
        double vx2Z = m_Vertexes[2].Coords[2];
        double centerZ = (vx0Z + vx2Z)/2;

        m_Vertexes[3].Coords[2] = (GLfloat) (centerZ + (centerZ - vx1Z));

        GenerateTexCoords(true, 0, 1, 2, 3);

    }
    else
    {
        m_NumVertexes = (m_NumVertexes & ~3);
        m_NumIndexes = m_NumVertexes;
        m_VertexesPerSend = m_NumVertexes;

        SetupShuffledIndexArray();
        FillVertexData();

        // Force all the quads to face front or back...
        //
        // Also, force the quads to be colwex and planar, as required by OpenGL.

        int quad;
        int numQuads = m_NumVertexes / 4;

        for (quad = 0; quad < numQuads; quad++)
        {
            int vx0 = m_VertexIndexes[quad*4];
            int vx1 = m_VertexIndexes[quad*4+1];
            int vx2 = m_VertexIndexes[quad*4+2];
            int vx3 = m_VertexIndexes[quad*4+3];

            // Make the first 3 vertexes into a triangle with the correct facing.

            if ( TriangleIsClockWise(vx0, vx1, vx2) != m_pGLRandom->m_Geometry.DrawClockWise() )
            {
                FixFacingBySwappingVx12(vx0, vx1, vx2);
            }

            // Make the quad colwex and planar by generating a legal
            // 4th vertex based on the other three.

            FixQuadVx3(vx0, vx1, vx2, vx3);

            GenerateTexCoords(true, vx0, vx1, vx2, vx3);

            // During the tesselation process the original color of the quad
            // is not preserved which causes inconsistencies. So fix up the
            // color component if its being sent.
            if(m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) &&
               (m_pGLRandom->m_Lighting.GetShadeModel() == GL_FLAT) &&
               m_pGLRandom->m_VertexFormat.Size(att_COL0))
            {
                m_Vertexes[vx0].Color[0] = m_Vertexes[vx3].Color[0];
                m_Vertexes[vx0].Color[1] = m_Vertexes[vx3].Color[1];
                m_Vertexes[vx0].Color[2] = m_Vertexes[vx3].Color[2];
                m_Vertexes[vx0].Color[3] = m_Vertexes[vx3].Color[3];

                m_Vertexes[vx1].Color[0] = m_Vertexes[vx3].Color[0];
                m_Vertexes[vx1].Color[1] = m_Vertexes[vx3].Color[1];
                m_Vertexes[vx1].Color[2] = m_Vertexes[vx3].Color[2];
                m_Vertexes[vx1].Color[3] = m_Vertexes[vx3].Color[3];

                m_Vertexes[vx2].Color[0] = m_Vertexes[vx3].Color[0];
                m_Vertexes[vx2].Color[1] = m_Vertexes[vx3].Color[1];
                m_Vertexes[vx2].Color[2] = m_Vertexes[vx3].Color[2];
                m_Vertexes[vx2].Color[3] = m_Vertexes[vx3].Color[3];
            }
        }
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForQuadStrips()
{
    // Decide how many quads per strip:

    m_NumVertexes = m_NumVertexes & ~1;

    if ( (m_NumVertexes % 24) == 0 )
        m_VertexesPerSend = 24;         // 11 quads/strip
    else if ( (m_NumVertexes % 16) == 0 )
        m_VertexesPerSend = 16;         // 7 quads/strip
    else if ( (m_NumVertexes % 8) == 0 )
        m_VertexesPerSend = 8;          // 3 quads/strip
    else if ( (m_NumVertexes % 6) == 0 )
        m_VertexesPerSend = 6;          // 2 quads/strip
    else if ( (m_NumVertexes % 4) == 0 )
        m_VertexesPerSend = 4;          // 1 quad/strip
    else if ( m_NumVertexes > 32)
        m_VertexesPerSend = 32;         // 15 quads/strip
    else
        m_VertexesPerSend = m_NumVertexes; // one big strip.

    m_NumVertexes -= (m_NumVertexes % m_VertexesPerSend);
    m_NumIndexes = m_NumVertexes;

    SetupShuffledIndexArray();
    FillVertexData();

    // Force all the quads to be clockwise or CCwise.
    //
    // Also, force the quads to be colwex and planar, as required by OpenGL.

    int strip;
    int numStrips = m_NumVertexes / m_VertexesPerSend;

    for (strip = 0; strip < numStrips; strip++)
    {
        int quad;
        int numQuads = (m_VertexesPerSend-2) / 2;

        for (quad = 0; quad < numQuads; quad++)
        {
            int vx0 = m_VertexIndexes[ strip*m_VertexesPerSend + quad*2 + 0];
            int vx1 = m_VertexIndexes[ strip*m_VertexesPerSend + quad*2 + 1 ];
            int vx2 = m_VertexIndexes[ strip*m_VertexesPerSend + quad*2 + 3 ];   // not a bug, sent in 0,1,3,2 order!
            int vx3 = m_VertexIndexes[ strip*m_VertexesPerSend + quad*2 + 2 ];   // not a bug, sent in 0,1,3,2 order!

            // Make the first 3 vertexes into a triangle with the correct facing.

            if ( TriangleIsClockWise(vx0, vx1, vx2) != m_pGLRandom->m_Geometry.DrawClockWise() )
            {
                if ( quad == 0 )
                    FixFacingBySwappingVx12(vx0, vx1, vx2);
                else
                    FixFacingByTransformingVx2(vx0, vx1, vx2);
            }

            // Make the quad colwex and planar by generating a legal
            // 4th vertex based on the other three.

            FixQuadVx3(vx0, vx1, vx2, vx3);

            GenerateTexCoords(quad==0, vx0, vx1, vx2, vx3);
        }
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForPolygons()
{
    // Decide how many vertices per polygon (and how many polys this loop).
    for (m_VertexesPerSend = 15; m_VertexesPerSend > 3; m_VertexesPerSend--)
    {
        if (0 == m_NumVertexes % m_VertexesPerSend)
            break;
    }

    m_NumVertexes -= (m_NumVertexes % m_VertexesPerSend);
    m_NumIndexes = m_NumVertexes;

    SetupShuffledIndexArray();
    FillVertexData();

    const GLuint numPolys = m_NumVertexes / m_VertexesPerSend;

    for (GLuint poly = 0; poly < numPolys; poly++)
    {
        GLuint * pVxIdx = m_VertexIndexes + poly * m_VertexesPerSend;

        GenerateColwexPolygon(pVxIdx, m_VertexesPerSend, false);

        // Treat the polygon as a triangle-strip for purposes of texture coords.
        for (GLuint tri = 0; tri < m_VertexesPerSend-2; tri++)
        {
            GenerateTexCoords(tri==0, pVxIdx[tri], pVxIdx[tri+1], pVxIdx[tri+2]);
        }
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForPatches()
{
    // Decide how many vertices per patch (and how many patches this loop).
    m_VertexesPerSend = m_pGLRandom->m_GpuPrograms.ExpectedVxPerPatch();

    m_NumVertexes -= (m_NumVertexes % m_VertexesPerSend);
    m_NumIndexes = m_NumVertexes;

    SetupShuffledIndexArray();
    FillVertexData();

    const GLuint numPatches = m_NumVertexes / m_VertexesPerSend;

    for (GLuint patch = 0; patch < numPatches; patch++)
    {
        GLuint * pVxIdx = m_VertexIndexes + patch * m_VertexesPerSend;

        GenerateColwexPolygon(pVxIdx, m_VertexesPerSend, false);

        // Treat the patch as a triangle-strip for purposes of texture coords.
        for (GLuint tri = 0; tri < m_VertexesPerSend-2; tri++)
        {
            GenerateTexCoords(tri==0, pVxIdx[tri], pVxIdx[tri+1], pVxIdx[tri+2]);
        }
    }
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexesForSolid()
{
    // We're drawing a square mesh wrapped around a sphere.
    //
    // There are tessRate horizontal tri-strips in the mesh.
    // Each strip has tessRate*2 triangles.
    // The mesh wraps around at the left and right.
    // The mesh draws to a point at the top and bottom (causing degenerate tris).
    //
    // Example for tessRate = 3:
    //
    //    0 0 0 0
    //    1 2 3 1
    //    4 5 6 4
    //    7 7 7 7
    //
    //    top strip: vx 01020301  tris (010) 012 (020) 023 (030) 031
    //    middle:    vx 14253614  tris  142  245  253  356  361  164
    //    bottom:    vx 47576747  tris  475 (577) 576 (677) 674 (477)
    //
    // tessRate of 1 gets a line with no volume or area.
    // tessRate of 2 gets a flat diamond shape with no volume.
    // tessRate of 3 (above) gets a solid with 8 triangular faces.
    // As tessRate increases, we approximate a sphere.
    //
    //    numStrips  = tr
    //    vxPerStrip = tr*2 + 2
    //    numTris    = tr*tr*2
    //    numTris    = tr*(tr-1)*2 + tr*2   (not counting degenerate triangles)
    //    numVx      = tr*(tr-1) + 2        (not counting reused vertexes)

    // Callwlate tessRate based on m_NumVertexes.
    GLuint tessRate;
    if (m_NumVertexes < 9)
        tessRate = 3;
    else
        tessRate = (GLuint)(sqrt((double)m_NumVertexes));

    m_NumVertexes     = tessRate * (tessRate-1) + 2;
    m_VertexesPerSend = tessRate * 2 + 2;
    m_NumIndexes      = tessRate * (tessRate*2 + 2);

    // Fill in vertex indexes for the strips:

    GLuint ccw0 = 0;
    GLuint ccw1 = 1;
    if (!m_pGLRandom->m_Geometry.IsCCW())
    {
        // We are drawing inside-out (i.e. clockwise is front), so we
        // swap pairs of vertex indexes in our strips.
        ccw0 = 1;
        ccw1 = 0;
    }

    GLuint s;
    GLuint t;
    GLuint * idxPtr = m_VertexIndexes;

    // top strip:
    for (t = 0; t < tessRate; t++)
    {
        idxPtr[ccw0] = 0;
        idxPtr[ccw1] = t+1;
        idxPtr += 2;
    }
    idxPtr[ccw0] = 0; // wrap at right.
    idxPtr[ccw1] = 1;
    idxPtr += 2;

    // center strips:
    for (s = 1; s < (tessRate-1); s++)
    {
        for (t = 0; t < tessRate; t++)
        {
            idxPtr[ccw0] = (s-1) * tessRate + t + 1;
            idxPtr[ccw1] = idxPtr[ccw0] + tessRate;
            idxPtr += 2;
        }
        idxPtr[ccw0] = (s-1) * tessRate + 1; // wrap at right.
        idxPtr[ccw1] = idxPtr[ccw0] + tessRate;
        idxPtr += 2;
    }

    // bottom strip:
    GLuint bottomIdx = tessRate * (tessRate-1) + 1;
    for (t = 0; t < tessRate; t++)
    {
        idxPtr[ccw0] = tessRate * (tessRate-2) + t + 1;
        idxPtr[ccw1] = bottomIdx;
        idxPtr += 2;
    }
    idxPtr[ccw0] = tessRate * (tessRate-2) + 1; // wrap at right.
    idxPtr[ccw1] = bottomIdx;
    idxPtr += 2;

    // Fill in the vertex positions, normals, texture coords, etc.
    // We make one "circle of latitude" at each of tessRate slices of the sphere.

    double alt;   // altitude in degrees: 0..180, 0 at s=0 (top, +y axis)
    double az;    // azimuth in degrees:  0..360, 0 at t=0 (front, +z axis)

    RndGeometry::BBox bbox;
    m_pGLRandom->m_Geometry.GetBBox(&bbox);

    double xScale  = bbox.width / 2.0;
    double xCenter = bbox.left + xScale;
    double yScale  = bbox.height / 2.0;
    double yCenter = bbox.bottom + yScale;
    double zScale  = bbox.depth / 2.0;
    double zCenter = bbox.deep + zScale;

    Vertex * pv = m_Vertexes;
    memset(pv, 0, sizeof(Vertex)*m_NumVertexes);

    for (s = 0; s <= tessRate; s++)
    {
        alt           = s*LOW_PREC_PI/tessRate;
        double cosAlt = cos(alt);
        double sinAlt = sin(alt);

        for (t = 0; t < tessRate; t++)
        {
            az = t*2.0*LOW_PREC_PI/tessRate;

            pv->Normal[0]     = (GLfloat)( sin(az) * sinAlt );
            pv->Normal[1]     = (GLfloat)( cosAlt );
            pv->Normal[2]     = (GLfloat)( cos(az) * sinAlt );

            pv->Coords[0]     = (GLfloat) (xCenter + xScale * pv->Normal[0]);
            pv->Coords[1]     = (GLfloat) (yCenter + yScale * pv->Normal[1]);
            pv->Coords[2]     = (GLfloat) (zCenter + zScale * pv->Normal[2]);
            pv->Coords[3]     = m_Pickers[RND_VX_W].FPick();

            pv->FogCoord      = (GLfloat) RND_GEOM_WorldDepth - pv->Coords[2];

            GLuint txc;
            for (txc = 0; txc < (GLuint)m_pGLRandom->NumTxCoords(); txc++)
            {
                // Fill in defaults by copying from Coords and Normal.
                pv->Tx[txc][0] = pv->Normal[0];
                pv->Tx[txc][1] = pv->Normal[1];
                pv->Tx[txc][2] = pv->Normal[2];
                pv->Tx[txc][3] = pv->Coords[3];

                UINT32 txAttr = m_pGLRandom->m_GpuPrograms.GetInTxAttr(ppTxCd0+txc);
                if (txAttr == 0)
                {
                    switch (m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(txc))
                    {
                        case 4: txAttr  = IsArrayLwbe; break;
                        case 3: txAttr  = Is3d;        break;
                        default: txAttr = Is2d;        break;
                    }
                }
                                                        // Coordinates used
                switch (txAttr)                         // s t r  layer  shadow  sample
                {                                       // ------ -----  ------  ------
                    case Is3d:                          // x y z    -      -
                    case IsLwbe:                        // x y z    -      -
                    case IsShadowLwbe:                  // x y z    -      w
                    case IsArrayLwbe:                   // x y z    w      -
                        // unused, 3d or normal-map texture, use the default values.
                        break;

                    case Is1d:                          // x - -    -      -
                    case IsArray1d:                     // x - -    y      -
                    case IsShadow1d:                    // x - -    -      z

                        // By using both s and t for 1d tx coord, we get a spiral.
                        pv->Tx[txc][0] = (t + s) / (GLfloat)tessRate - 0.5f;
                        break;

                    case Is2d:                          // x y -    -      -
                    case Is2dMultisample:               // x y -    -      -       w
                    case IsArray2d:                     // x y -    z      -
                    case IsShadow2d:                    // x y -    -      z
                    case IsShadowArray2d:               // x y -    z      w
                    {
                        // 2d texture.
                        // Put the 0,1 range at front center.
                        // Make the sphere 2 textures high and 4 textures around.

                        // tmp ranges from 0->1, with 0.5 at t==0 in front of sphere.
                        GLfloat tmp = ((t + tessRate/2) % tessRate) / (GLfloat)tessRate;

                        // Shift and scale tmp from 0->1 to -1.5 to +2.5,
                        // with 0.5 at t==0 in front of sphere.
                        pv->Tx[txc][0] = 4.0f*tmp - 1.5f;

                        // tmp ranges from 0->1, with 0.5 at "equator" of sphere.
                        tmp = s / (GLfloat)tessRate;

                        // Shift and scale tmp from 0->1 to -0.5 to 1.5,
                        // with 0.5 at equator.
                        pv->Tx[txc][1] = 2.0f*tmp - 0.5f;
                        break;
                    }
                }
                int index = GetLayerIndex(txAttr);
                if (index > 0)
                {
                    pv->Tx[txc][index] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                }
                index = GetShadowIndex(txAttr);
                if (index > 0)
                {
                    pv->Tx[txc][index] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                }
                if (txAttr == Is2dMultisample)
                {
                    pv->Tx[txc][3] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                }
            }

            if ((m_pGLRandom->m_VertexFormat.Size(att_COL0) &&
                !m_Pickers[RND_VX_COLOR_WHITE].Pick()) ||
                (s == 0))
            {
                pv->Color[0]   = m_Pickers[RND_VX_COLOR_BYTE].Pick();
                pv->Color[1]   = m_Pickers[RND_VX_COLOR_BYTE].Pick();
                pv->Color[2]   = m_Pickers[RND_VX_COLOR_BYTE].Pick();
                pv->Color[3]   = m_Pickers[RND_VX_ALPHA_BYTE].Pick();
            }
            else
            {
                pv->Color[0]   = 255;
                pv->Color[1]   = 255;
                pv->Color[2]   = 255;
                pv->Color[3]   = 255;
            }
            if ((m_pGLRandom->m_VertexFormat.Size(att_COL1) &&
                !m_Pickers[RND_VX_COLOR_WHITE].Pick()) ||
                (s == 0))
            {
                pv->SecColor[0] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
                pv->SecColor[1] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
                pv->SecColor[2] = m_Pickers[RND_VX_SECCOLOR_BYTE].Pick();
            }
            else
            {
                pv->SecColor[0] = 255;
                pv->SecColor[1] = 255;
                pv->SecColor[2] = 255;
            }
            if ((m_pGLRandom->m_VertexFormat.Size(att_EdgeFlag)) || (s == 0))
            {
                pv->EdgeFlag   = (GLbyte) m_Pickers[RND_VX_EDGE_FLAG].Pick();
            }
            else
            {
                pv->EdgeFlag   = GL_TRUE;
            }

            ++pv;  // Done with this vertex.

            if ((s == 0) || (s == tessRate))
            {
                // North or south pole, we have just one vertex at this latitude.
                break;
            }
        }
    }
}

//------------------------------------------------------------------------------
void RndVertexes::GenerateColwexPolygon
(
    GLuint * pVx,
    GLuint   lwx,
    bool     isTriangleFan
)
{
    // Generating a fully general N-pointed random polygon is hard.
    // The problem is making it colwex and non-self-intersecting.
    //
    // So instead we'll generate a randomly skewed ellipse and place
    // vertices at irregular points around it.
    //
    // If drawing a triangle-fan, avoid "pac-man" shapes (which are not colwex)
    // by either:
    //  - repeating the last outside vertex to effectively draw a closed polygon
    //  - or keep all points on one side of the center.

    // Center and scale the ellipse based on the bounding box.
    RndGeometry::BBox box;
    m_pGLRandom->m_Geometry.GetBBox(&box);
    const double cX = box.left + box.width/2.0;
    const double cY = box.bottom + box.height/2.0;
    const double cZ = box.deep + box.depth/2.0;
    const double major = max(box.width, box.height);
    const double minor = min(box.width, box.height);

    // Choose a random angle (in radians) for the major axis.
    const double a = m_pFpCtx->Rand.GetRandomFloat(0, 2*LOW_PREC_PI);
    const double a_sin = sin(a);
    const double a_cos = cos(a);

    if (isTriangleFan)
    {
        // Steal the first vertex as the center.
        GLuint vxC = pVx[0];
        pVx++;
        lwx--;

        m_Vertexes[vxC].Coords[0] = (GLfloat) cX;
        m_Vertexes[vxC].Coords[1] = (GLfloat) cY;
        m_Vertexes[vxC].Coords[2] = (GLfloat) cZ;
    }

    // Direction of step is -1 for clockwise, because radians increase
    // in the counter-clockwise direction.
    const bool wantClockWise = m_pGLRandom->m_Geometry.DrawClockWise();
    const double dir = wantClockWise ? -1.0 : 1.0;

    // Walk around outside of ellipse starting from the major-axis angle.
    double t = 0.0;
    for (GLuint i = 0; i < lwx; i++)
    {
        // First vertex is always at the right-hand "point" of the ellipse.

        if (0 != i)
        {
            // Choose a random point further along the outside of the ellipse.
            // Points must be less than than 180deg apart, and we must wrap
            // around the ellipse only once.
            float max_tStep = (float) min(2.0*LOW_PREC_PI - t, 0.99*LOW_PREC_PI);
            float tStep = m_pFpCtx->Rand.GetRandomFloat(0, max_tStep);

            if ((lwx-1 == i) && isTriangleFan)
            {
                // Last vertex of triangle fan -- "pac-man" elimination.
                // If we've gone more than halfway around the ellipse already,
                // close the polygon by repeating the t==0.0 vertex.
                // Otherwise, limit t to 180deg.
                if (t > LOW_PREC_PI)
                    t = 0.0;
                else
                    t = min(LOW_PREC_PI, t + tStep);
            }
            else
            {
                t = t + tStep;
            }
        }

        const double t_sin = sin(t*dir);
        const double t_cos = cos(t*dir);

        // From the wikipedia article on "Ellipse".
        GLfloat x = (GLfloat)(cX + major*t_cos*a_cos - minor*t_sin*a_sin);
        GLfloat y = (GLfloat)(cY + major*t_cos*a_sin + minor*t_sin*a_cos);

        m_Vertexes[pVx[i]].Coords[0] = x;
        m_Vertexes[pVx[i]].Coords[1] = y;

        // Here we could also generate Z values that make the polygon planar.
        // But that doesn't seem to be required in glrandom, because our
        // modelview matrix is never set to rotate stuff except around the Z
        // axis (with RND_GEOM_MODELVIEW_OPERATION_Rotate).
        //
        // So the polygon won't be "looked at" from an angle where it projects
        // onto the screen as non-colwex.
        //
        // We can safely leave the current random-within-bbox Z values.
    }
}

//------------------------------------------------------------------------------
void RndVertexes::CleanupVertexes()
{
    Vertex * pV    = m_Vertexes;
    Vertex * pVEnd = m_Vertexes + m_NumVertexes;

    for (/**/; pV < pVEnd; pV++)
    {
        // SHORT values may only be in the range  -32768 to 32767.
        // HALF values may only be in the range   -65504 to 65504.
        // To keep approximate repeatability between the data types, and to
        // prevent INF from showing up in HALF values, clamp our floats before
        // doing the data colwersion.
        int i;
        for (i = 0; i < 4; i++)
        {
            if (pV->Coords[i] > 32767.0)
                pV->Coords[i] = 32767.0;
            else if (pV->Coords[i] < -32768.0)
                pV->Coords[i] = -32768.0;
        }

        switch (m_pGLRandom->m_VertexFormat.Type(att_OPOS))
        {
            case GL_FLOAT:
                break;
            case GL_SHORT:
                pV->sCoords[0] = (GLshort) pV->Coords[0];
                pV->sCoords[1] = (GLshort) pV->Coords[1];
                pV->sCoords[2] = (GLshort) pV->Coords[2];
                pV->sCoords[3] = 1;
                break;
#if defined(GL_LW_half_float)
            case GL_HALF_FLOAT_LW:
                pV->sCoords[0] = (GLshort) Utility::Float32ToFloat16( pV->Coords[0]);
                pV->sCoords[1] = (GLshort) Utility::Float32ToFloat16( pV->Coords[1]);
                pV->sCoords[2] = (GLshort) Utility::Float32ToFloat16( pV->Coords[2]);
                pV->sCoords[3] = (GLshort) Utility::Float32ToFloat16( pV->Coords[3]);
                break;
#endif
        }

        if (m_pGLRandom->m_VertexFormat.Size(att_NRML))
        {
            switch (m_pGLRandom->m_VertexFormat.Type(att_NRML))
            {
                case GL_FLOAT:
                    break;
                case GL_SHORT:
                    // Here, floats ranging from -1.0 to +1.0 must be mapped to
                    // shorts ranging from -32768 to 32767.
                    //
                    pV->sNormal[0] = (GLshort)(32767.0 * pV->Normal[0]);
                    pV->sNormal[1] = (GLshort)(32767.0 * pV->Normal[1]);
                    pV->sNormal[2] = (GLshort)(32767.0 * pV->Normal[2]);
                    break;
#if defined(GL_LW_half_float)
                case GL_HALF_FLOAT_LW:
                    pV->sNormal[0] = (GLshort) Utility::Float32ToFloat16(pV->Normal[0]);
                    pV->sNormal[1] = (GLshort) Utility::Float32ToFloat16(pV->Normal[1]);
                    pV->sNormal[2] = (GLshort) Utility::Float32ToFloat16(pV->Normal[2]);
                    break;
#endif
            }
        }
    }

    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
    {
        GLfloat scaleS;
        GLfloat scaleT;
        GLfloat scaleR;
        GLfloat scaleQ;

        m_pGLRandom->m_Texturing.ScaleSTRQ(txu, &scaleS, &scaleT, &scaleR, &scaleQ);

        if ((scaleS != 1.0f) || (scaleT != 1.0f) || (scaleR != 1.0f) || (scaleQ != 1.0f))
        {
            for (pV = m_Vertexes; pV < pVEnd; pV++)
            {
                pV->Tx[txu][0] = fabs(scaleS * pV->Tx[txu][0]);
                pV->Tx[txu][1] = fabs(scaleT * pV->Tx[txu][1]);
                pV->Tx[txu][2] = fabs(scaleR * pV->Tx[txu][2]);
                pV->Tx[txu][3] = fabs(scaleQ * pV->Tx[txu][3]);
                // some texcoords have value >1.0 and if the texture target is
                //TEXTURE_RENDERBUFFER_LW(which means the S&T scale values will
                // not be 1.0), we can't exceed the size of the texture.
                if (scaleS != 1.0f)
                    pV->Tx[txu][0] = min(scaleS-1.0f, pV->Tx[txu][0]);
                if (scaleT != 1.0f)
                    pV->Tx[txu][1] = min(scaleT-1.0f, pV->Tx[txu][1]);
            }
        }
    }
}

//------------------------------------------------------------------------------
void RndVertexes::SetupInorderIndexArray()
{
    GLuint * pi    = m_VertexIndexes;
    GLuint * piEnd = m_VertexIndexes + m_NumIndexes;
    GLuint   i     = 0;
    while (pi < piEnd)
    {
        *pi++ = i++;
        if (i >= m_NumVertexes)
            i = 0;
    }
}

//------------------------------------------------------------------------------
void RndVertexes::SetupShuffledIndexArray()
{
    SetupInorderIndexArray();

    m_pFpCtx->Rand.Shuffle(m_NumIndexes, (UINT32*)m_VertexIndexes);
}

//------------------------------------------------------------------------------
void RndVertexes::PickVertexLayout()
{
    GLuint numGroups = 0;
    GLuint attrib;
    GLuint i;

    //
    // Do/don't randomly reorder the vertex structure.
    //
    GLboolean doRandomize = m_Pickers[RND_VX_DO_SHUFFLE_STRUCT].Pick();

    //
    // This speed optimization would cause the random sequence
    // to depend on the send method, which makes GL driver bugs hard to
    // track down.  So, don't do it.
    //
    // if ((m_Method == RND_VX_SEND_METHOD_Immediate) ||
    //     (m_Method == RND_VX_SEND_METHOD_Vector))
    // {
    //    doRandomize = false;
    // }

    VxAttribute layout[att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG];
    GLuint usedAttribs = att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG;
    GLuint unusedAttribs = att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG - 1;
    // Shuffle before removing the unused attribs so we always call the same number of
    // random values.
    for (attrib = 0; attrib < usedAttribs; attrib++)
    {
        layout[attrib] = (VxAttribute)attrib;
    }

    if (doRandomize)
    {
        m_pFpCtx->Rand.Shuffle(usedAttribs, (UINT32 *)layout);
    }

    // Squeeze out the unused attribs.
    usedAttribs = 0;
    for (attrib = 0; attrib < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG; attrib++)
    {
        if (m_pGLRandom->m_VertexFormat.Size(layout[attrib]))
            m_VxLayoutOrder[usedAttribs++] = layout[attrib];
        else
            m_VxLayoutOrder[unusedAttribs--] = layout[attrib];
    }

    //
    // First pass: fill in types and size, pick grouping.
    //
    m_CompactedVxSize = 0;
    GLuint newGroup = false;
    for (i = 0; i < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG; i++)
    {
        attrib = m_VxLayoutOrder[i];

        m_VxLayout[attrib].Type        = m_pGLRandom->m_VertexFormat.Type((VxAttribute)attrib);
        m_VxLayout[attrib].NumElements = m_pGLRandom->m_VertexFormat.Size((VxAttribute)attrib);

        // need to always call this picker to keep the random sequence consistent when turning on/off features
        newGroup = m_Pickers[RND_VX_NEW_GROUP].Pick();

        if (m_VxLayout[attrib].NumElements == 0)
        {
            // Unused attribute, add to the dummy group.
            m_VxLayout[attrib].Group    = att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG;
            m_VxLayout[attrib].Offset   = 0;
            m_VxLayout[attrib].Stride   = 0;
            m_VxLayout[attrib].NumBytes = 0;
            m_VxLayout[attrib].PadBytes = 0;
            continue;
        }

        if ((newGroup && doRandomize) || (numGroups == 0))
        {
            // Start a new group.
            m_VxLayout[attrib].Group = numGroups++;
        }
        else
        {
            // Add to current group.
            m_VxLayout[attrib].Group = numGroups-1;
        }

        switch (m_VxLayout[attrib].Type)
        {
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                m_VxLayout[attrib].NumBytes = 1 * m_VxLayout[attrib].NumElements;
                break;

            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
#if defined(GL_LW_half_float)
            case GL_HALF_FLOAT_LW:
#endif
                m_VxLayout[attrib].NumBytes = 2 * m_VxLayout[attrib].NumElements;
                break;

            case GL_INT:
            case GL_UNSIGNED_INT:
            case GL_FLOAT:
                m_VxLayout[attrib].NumBytes = 4 * m_VxLayout[attrib].NumElements;
                break;

            default:
                MASSERT(0);
                m_VxLayout[attrib].NumBytes = 0;
                break;
        }

        const int align = 8;
        if (m_VxLayout[attrib].NumBytes & (align-1))
            m_VxLayout[attrib].PadBytes = align - (m_VxLayout[attrib].NumBytes & (align-1));
        else
            m_VxLayout[attrib].PadBytes = 0;

        m_CompactedVxSize += m_VxLayout[attrib].NumBytes;
        m_CompactedVxSize += m_VxLayout[attrib].PadBytes;
    }

    //
    // Second pass: fill in offset and stride.
    //
    GLuint groupOffset = 0;

    GLuint group;
    for (group = 0; group < numGroups; group++)
    {
        // Pass 2a: find stride for this group, set offset.
        GLuint groupStride = 0;

        for (i = 0; i < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG; i++)
        {
            attrib = m_VxLayoutOrder[i];

            if (m_VxLayout[attrib].Group == group)
            {
                m_VxLayout[attrib].Offset = groupOffset + groupStride;

                groupStride =  groupStride +
                m_VxLayout[attrib].NumBytes +
                m_VxLayout[attrib].PadBytes;
            }
        }
        MASSERT(groupStride <= 255);

        // Pass 2b: fill in stride for each attrib.
        for (attrib = 0; attrib < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG; attrib++)
        {
            if (m_VxLayout[attrib].Group == group)
                m_VxLayout[attrib].Stride = groupStride;
        }

        groupOffset += groupStride * m_NumVertexes;
    }
}

//------------------------------------------------------------------------------
void RndVertexes::UnShuffleVertexes()
{
    Vertex * dst = (Vertex *)m_PciVertexes;
    Vertex * src = m_Vertexes;
    GLuint ii;
    for (ii = 0; ii < m_NumIndexes; ii++)
    {
        memcpy(dst + ii, src + m_VertexIndexes[ii], sizeof(Vertex));
        m_VertexIndexes[ii] = ii;
    }
    m_NumVertexes = m_NumIndexes;

    memcpy(m_Vertexes, m_PciVertexes, m_NumVertexes*sizeof(Vertex));
}

//------------------------------------------------------------------------------
void RndVertexes::CopyVertexes(bool bLogGoldelwalues)
{
    // We require 64-byte alignment.  The buffers should have been allocated that way.
    MASSERT(0 == (0x3F & (size_t)m_LwrrentVertexBuf));

#ifdef GLRANDOM_PICK_LOGGING
    //
    // Since we don't write the pad bytes in the loop below, we'll get
    // repeatability problems when we crc m_LwrrentVertexBuf.
    // Prevent false errors in the GLRandom self checking code.
    //
    Platform::MemSet(m_PciVertexes, 0xcc, m_NumVertexes * m_CompactedVxSize);
#endif

#if defined(APPLE_P80_MEMCOPY_BUG_ILWESTIGATION)
    double PrevDestValue = *(double *)m_LwrrentVertexBuf;
#endif

    //
    // For each attribute:
    //
    GLuint ai;
    for (ai = 0; ai < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG; ai++)
    {
        //
        // Get the layout of this vertex attrib, skip if not enabled.
        //
        VertexLayout * pvlo = m_VxLayout + ai;
        if (pvlo->NumBytes == 0)
            continue;

        //
        // Starting destination address:
        //
        GLubyte * dst = pvlo->Offset + m_PciVertexes;

        //
        // Starting source address:
        //
        const GLubyte * src;
        switch (ai)
        {
            case att_OPOS:
                if (m_pGLRandom->m_VertexFormat.Type(att_OPOS) != GL_FLOAT)
                    src = (const GLubyte *)&(m_Vertexes[0].sCoords[0]);
                else
                    src = (const GLubyte *)&(m_Vertexes[0].Coords[0]);
                break;

            case att_NRML:
                if (m_pGLRandom->m_VertexFormat.Type(att_NRML) != GL_FLOAT)
                    src = (const GLubyte *)&(m_Vertexes[0].sNormal[0]);
                else
                    src = (const GLubyte *)&(m_Vertexes[0].Normal[0]);
                break;

            case att_COL0:
                src = (const GLubyte *)&(m_Vertexes[0].Color[0]);
                break;

            case att_COL1:
                src = (const GLubyte *)&(m_Vertexes[0].SecColor[0]);
                break;

            case att_FOGC:
                src = (const GLubyte *)&(m_Vertexes[0].FogCoord);
                break;

            case att_TEX0:
            case att_TEX1:
            case att_TEX2:
            case att_TEX3:
            case att_TEX4:
            case att_TEX5:
            case att_TEX6:
            case att_TEX7:
                src = (const GLubyte *)&(m_Vertexes[0].Tx[ai - att_TEX0][0]);
                break;

            case att_EdgeFlag:
                src = (const GLubyte *)&(m_Vertexes[0].EdgeFlag);
                break;

            default:
            case att_1:
            case att_6:
            case att_7:
                MASSERT(0);
                src = 0;
                break;
        }

        //
        // Now, scatter this vertex attribute across the current vertex buffer,
        // so it ends up where the HW will look for it.
        //
        GLuint v;
        for (v = 0; v < m_NumVertexes; v++)
        {
            GLubyte *       d = dst + v * pvlo->Stride;
            const GLubyte * s = src + v * sizeof(Vertex);

            UINT32 count = pvlo->NumBytes;

            while (count--)
            {
                MEM_WR08(d, *s);
                d++;
                s++;
            }
        }
    } // each attribute

    //
    // Now that we have the data arranged in m_PciVertexes,
    // copy it to the FB or AGP array if necessary.
    //
    MASSERT(m_CompactedVxSize <= MaxCompactedVertexSize);

    if (m_LwrrentVertexBuf != m_PciVertexes)
    {

#if !defined(APPLE_P80_MEMCOPY_BUG_ILWESTIGATION)
        //
        // Because of a chipset problem on apple P80 systems,
        // this memcpy occasionally fails to correctly copy the data.
        // The alternate byte-by-byte copy below works on all systems.
        //
        // memcpy(m_LwrrentVertexBuf,
        //        m_PciVertexes,
        //        m_CompactedVxSize * m_NumVertexes);
        //
        GLubyte * src = m_PciVertexes;
        GLubyte * dst = m_LwrrentVertexBuf;
        GLubyte * dstEnd = dst + m_CompactedVxSize * m_NumVertexes;

        while (dst < dstEnd)
            *dst++ = *src++;

    #else
        //
        // Code for debugging the P80 issue.
        //
        GLubyte * src = m_PciVertexes;
        GLubyte * dst = m_LwrrentVertexBuf;
        GLubyte * dstEnd = dst + m_CompactedVxSize * m_NumVertexes;

        GLdouble * Dsrc = (GLdouble *)src;
        GLdouble * Ddst = (GLdouble *)dst;
        GLdouble * DdstEnd = (GLdouble *)dstEnd;

        // Copy the first one outside the loop.
        // This first write is the one that most often fails on apple P80 systems.
        *Ddst = *Dsrc;
        if (*Ddst != *Dsrc)
        {
            Xp::BreakPoint();
            GLubyte * pPrevDst = (GLubyte *)&PrevDestValue;

            Printf(Tee::PriHigh, "\nFirst 8-byte copy from temp vertex array to video memory failed!\n");
            Printf(Tee::PriHigh, "src @%p = %02x %02x %02x %02x  %02x %02x %02x %02x\n",
                src,
                src[0], src[1], src[2], src[3],
                src[4], src[5], src[6], src[7]
                );
            Printf(Tee::PriHigh, "dst @%p = %02x %02x %02x %02x  %02x %02x %02x %02x (before)\n",
                dst,
                pPrevDst[0], pPrevDst[1], pPrevDst[2], pPrevDst[3],
                pPrevDst[4], pPrevDst[5], pPrevDst[6], pPrevDst[7]
                );
            Printf(Tee::PriHigh, "dst @%p = %02x %02x %02x %02x  %02x %02x %02x %02x (after)\n",
                dst,
                dst[0], dst[1], dst[2], dst[3],
                dst[4], dst[5], dst[6], dst[7]
                );
        }
        ++Ddst;
        ++Dsrc;
        while (Ddst < DdstEnd)
            *Ddst++ = *Dsrc++;
#endif
    }
    Platform::FlushCpuWriteCombineBuffer();

    if (bLogGoldelwalues)
        m_pGLRandom->LogData(RND_VX, m_LwrrentVertexBuf, m_NumVertexes * m_CompactedVxSize);

}

//------------------------------------------------------------------------------
void RndVertexes::CopyIndexes()
{
    m_LwrrentIndexBuf =
        m_LwrrentVertexBuf + MaxCompactedVertexSize * m_NumVertexes;

    GLuint restartInterval = m_UsePrimRestart ? m_VertexesPerSend : m_NumIndexes;
    GLuint i;

    if (m_pGLRandom->m_VertexFormat.IndexType() == GL_UNSIGNED_SHORT)
    {
        GLushort * pDst    = (GLushort*)m_LwrrentIndexBuf;

        for (i = 0; i < m_NumIndexes; i++)
        {
            if (i && (0 == i % restartInterval))
                *pDst++ = 0xffff;

            *pDst++ = (GLushort) m_VertexIndexes[i];
        }
    }
    else
    {
        MASSERT(m_pGLRandom->m_VertexFormat.IndexType() == GL_UNSIGNED_INT);

        GLuint * pDst    = (GLuint*)m_LwrrentIndexBuf;

        for (i = 0; i < m_NumIndexes; i++)
        {
            if (i && (0 == i % restartInterval))
                *pDst++ = 0xffffffff;

            *pDst++ = m_VertexIndexes[i];
        }
    }
    Platform::FlushCpuWriteCombineBuffer();
}

//------------------------------------------------------------------------------
void RndVertexes::SetupVertexArrays(GLuint vboId)
{
    // Tell the GL library where the data is, ie. setup for vertex pulling.
    GLubyte * base = m_LwrrentVertexBuf;
    if ((m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture) &&
        m_pGLRandom->m_Misc.XfbIsCapturePossible()) || m_UseVBO)
    {
        //base is now an offset into the vbo.
        base = 0;
        // We will use this vbo for vertex pulling during the capture mode of XFB
        // or if simply using VBOs.
        glBindBuffer(GL_ARRAY_BUFFER,vboId);
        glBufferSubData(GL_ARRAY_BUFFER,
                        0, // offset
                        m_NumVertexes * m_CompactedVxSize, // size
                        m_LwrrentVertexBuf);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(  m_pGLRandom->m_VertexFormat.Size(att_OPOS),
        m_pGLRandom->m_VertexFormat.Type(att_OPOS),
        m_VxLayout[att_OPOS].Stride,
        base + m_VxLayout[att_OPOS].Offset );

    if (m_pGLRandom->m_VertexFormat.Size(att_NRML))
    {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(  m_pGLRandom->m_VertexFormat.Type(att_NRML),
            m_VxLayout[att_NRML].Stride,
            base + m_VxLayout[att_NRML].Offset );
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_COL0))
    {
        if (att_COL0 == m_pGLRandom->m_GpuPrograms.Col0Alias())
        {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(   m_pGLRandom->m_VertexFormat.Size(att_COL0),
                m_pGLRandom->m_VertexFormat.Type(att_COL0),
                m_VxLayout[att_COL0].Stride,
                base + m_VxLayout[att_COL0].Offset );
        }
        else
        {
            glEnableClientState(GL_VERTEX_ATTRIB_ARRAY0_LW + m_pGLRandom->m_GpuPrograms.Col0Alias());
            glVertexAttribPointerLW(
                m_pGLRandom->m_GpuPrograms.Col0Alias(),
                m_pGLRandom->m_VertexFormat.Size(att_COL0),
                m_pGLRandom->m_VertexFormat.Type(att_COL0),
                m_VxLayout[att_COL0].Stride,
                base + m_VxLayout[att_COL0].Offset );
        }
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_EdgeFlag))
    {
        glEnableClientState(GL_EDGE_FLAG_ARRAY);
        glEdgeFlagPointer(m_VxLayout[att_EdgeFlag].Stride,
            base + m_VxLayout[att_EdgeFlag].Offset );
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_COL1))
    {
        glEnableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);
        glSecondaryColorPointer(
            m_pGLRandom->m_VertexFormat.Size(att_COL1),
            m_pGLRandom->m_VertexFormat.Type(att_COL1),
            m_VxLayout[att_COL1].Stride,
            base + m_VxLayout[att_COL1].Offset );
    }

    GLint txu;
    for ( txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++ )
    {
        if (m_pGLRandom->m_VertexFormat.TxSize(txu))
        {
            glClientActiveTexture(GL_TEXTURE0_ARB + txu);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(
                m_pGLRandom->m_VertexFormat.TxSize(txu),
                m_pGLRandom->m_VertexFormat.TxType(txu),
                m_VxLayout[att_TEX0 + txu].Stride,
                base + m_VxLayout[att_TEX0 + txu].Offset );
        }
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_FOGC))
    {
        glEnableClientState(GL_FOG_COORDINATE_ARRAY_EXT);
        glFogCoordPointer(m_pGLRandom->m_VertexFormat.Type(att_FOGC),
            m_VxLayout[att_FOGC].Stride,
            base + m_VxLayout[att_FOGC].Offset );
    }
}

//------------------------------------------------------------------------------
void RndVertexes::TurnOffVertexArrays()
{

    glVertexPointer(4,GL_FLOAT,0,NULL);
    glDisableClientState(GL_VERTEX_ARRAY);

    if (m_pGLRandom->m_VertexFormat.Size(att_NRML))
    {
        glNormalPointer(GL_FLOAT,0,NULL);
        glDisableClientState(GL_NORMAL_ARRAY);
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_COL0))
    {
        if (att_COL0 == m_pGLRandom->m_GpuPrograms.Col0Alias())
        {
            glColorPointer(4,GL_FLOAT,0,NULL);
            glDisableClientState(GL_COLOR_ARRAY);
        }
        else
        {
            glVertexAttribPointerLW(m_pGLRandom->m_GpuPrograms.Col0Alias(),4,GL_FLOAT,0,NULL);
            glDisableClientState(GL_VERTEX_ATTRIB_ARRAY0_LW + m_pGLRandom->m_GpuPrograms.Col0Alias());
        }
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_EdgeFlag))
    {
        glEdgeFlagPointer(0,NULL);
        glDisableClientState(GL_EDGE_FLAG_ARRAY);
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_COL1))
    {
        glSecondaryColorPointer(3,GL_FLOAT,0,NULL);
        glDisableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);
    }

    // For XFB we stream all textures if just one is enabled, or TexGen is enabled.
    // For std processing we use the m_VertexFormat.Size(). In either case the GL
    // knows what is actually enabled for any given point in time. So just ask it.
    GLint txu;
    for ( txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++ )
    {
        glClientActiveTexture(GL_TEXTURE0_ARB + txu);
        if (glIsEnabled(GL_TEXTURE_COORD_ARRAY))
        {
            glTexCoordPointer(4,GL_FLOAT,0,NULL);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        }
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_FOGC))
    {
        glFogCoordPointer(GL_FLOAT,0,NULL);
        glDisableClientState(GL_FOG_COORDINATE_ARRAY_EXT);
    }
}

//------------------------------------------------------------------------------
// Called by SendDrawMethod, for one-vertex-at-a-time draws.
//
void RndVertexes::SendDrawBeginEnd(int firstVertex)
{
    // Set up our send functions:
    void (RndVertexes::*pfSndColor)(GLint)       = 0;
    void (RndVertexes::*pfSndNormal)(GLint)      = 0;
    void (RndVertexes::*pfSndEdgeFlag)(GLint)    = 0;
    void (RndVertexes::*pfSndSecColor)(GLint)    = 0;
    void (RndVertexes::*pfSndTxc)(GLint)         = 0;
    void (RndVertexes::*pfSndVtx)(GLint)         = 0;
    void (RndVertexes::*pfSndFog)(GLint)         = 0;

    bool hasTexCoords = false;

    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
    {
        if (m_pGLRandom->m_VertexFormat.TxSize(txu))
        {
            hasTexCoords = true;
            break;
        }
    }

    switch ( m_Method )
    {
        default:
        MASSERT(0); // shouldn't get here.

        case RND_VX_SEND_METHOD_Array:
        {
            pfSndVtx = &RndVertexes::SndArrayElement;
            break;
        }
        case RND_VX_SEND_METHOD_Vector:
        {
            switch (m_pGLRandom->m_VertexFormat.Type(att_OPOS))
            {
                case GL_FLOAT:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_OPOS))
                    {
                        case 2: pfSndVtx = &RndVertexes::SndV2fv; break;
                        case 3: pfSndVtx = &RndVertexes::SndV3fv; break;
                        case 4: pfSndVtx = &RndVertexes::SndV4fv; break;
                        default: MASSERT(0); break;
                    }
                    break;
                case GL_SHORT:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_OPOS))
                    {
                        case 2: pfSndVtx = &RndVertexes::SndV2sv; break;
                        case 3: pfSndVtx = &RndVertexes::SndV3sv; break;
                        case 4: pfSndVtx = &RndVertexes::SndV4sv; break;
                        default: MASSERT(0); break;
                    }
                    break;
#if defined(GL_LW_half_float)
                case GL_HALF_FLOAT_LW:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_OPOS))
                    {
                        case 2: pfSndVtx = &RndVertexes::SndV2hv; break;
                        case 3: pfSndVtx = &RndVertexes::SndV3hv; break;
                        case 4: pfSndVtx = &RndVertexes::SndV4hv; break;
                        default: MASSERT(0); break;
                    }
                    break;
#endif
                default:
                MASSERT(0);
                break;
            }
            switch (m_pGLRandom->m_VertexFormat.Type(att_NRML))
            {
                case GL_FLOAT:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_NRML))
                    {
                        case 3: pfSndNormal = &RndVertexes::SndN3fv; break;
                        default: MASSERT(0);
                        case 0:  break;
                    }
                    break;
                case GL_SHORT:
                switch (m_pGLRandom->m_VertexFormat.Size(att_NRML))
                {
                    case 3: pfSndNormal = &RndVertexes::SndN3sv; break;
                    default: MASSERT(0);
                    case 0:  break;
                }
                break;
#if defined(GL_LW_half_float)
                case GL_HALF_FLOAT_LW:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_NRML))
                    {
                        case 3: pfSndNormal = &RndVertexes::SndN3hv; break;
                        default: MASSERT(0);
                        case 0:  break;
                    }
                    break;
#endif
                default:
                    MASSERT(0);
                    break;
            }

            if (m_pGLRandom->m_VertexFormat.Size(att_COL0))
                pfSndColor = &RndVertexes::SndC4bv;

            if (m_pGLRandom->m_VertexFormat.Size(att_EdgeFlag))
                pfSndEdgeFlag = &RndVertexes::SndE1bv;

            if (m_pGLRandom->m_VertexFormat.Size(att_COL1))
                pfSndSecColor = &RndVertexes::SndSecC3bv;

            if (hasTexCoords)
                pfSndTxc = &RndVertexes::SndTxcv;

            if (m_pGLRandom->m_VertexFormat.Size(att_FOGC))
                pfSndFog = &RndVertexes::SndFogfv;

            break;
        }

        case RND_VX_SEND_METHOD_Immediate:
        {
            switch (m_pGLRandom->m_VertexFormat.Type(att_OPOS))
            {
                case GL_FLOAT:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_OPOS))
                    {
                        case 2: pfSndVtx = &RndVertexes::SndV2f; break;
                        case 3: pfSndVtx = &RndVertexes::SndV3f; break;
                        case 4: pfSndVtx = &RndVertexes::SndV4f; break;
                        default: MASSERT(0); break;
                    }
                    break;
                case GL_SHORT:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_OPOS))
                    {
                        case 2: pfSndVtx = &RndVertexes::SndV2s; break;
                        case 3: pfSndVtx = &RndVertexes::SndV3s; break;
                        case 4: pfSndVtx = &RndVertexes::SndV4s; break;
                        default: MASSERT(0); break;
                    }
                    break;
#if defined(GL_LW_half_float)
                case GL_HALF_FLOAT_LW:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_OPOS))
                    {
                        case 2: pfSndVtx = &RndVertexes::SndV2h; break;
                        case 3: pfSndVtx = &RndVertexes::SndV3h; break;
                        case 4: pfSndVtx = &RndVertexes::SndV4h; break;
                        default: MASSERT(0); break;
                    }
                    break;
#endif
                default:
                    MASSERT(0);
                    break;
            }
            switch (m_pGLRandom->m_VertexFormat.Type(att_NRML))
            {
                case GL_FLOAT:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_NRML))
                    {
                        case 3: pfSndNormal = &RndVertexes::SndN3f; break;
                        default: MASSERT(0);
                        case 0:  break;
                    }
                    break;
                case GL_SHORT:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_NRML))
                    {
                        case 3: pfSndNormal = &RndVertexes::SndN3s; break;
                        default: MASSERT(0);
                        case 0:  break;
                    }
                    break;
#if defined(GL_LW_half_float)
                case GL_HALF_FLOAT_LW:
                    switch (m_pGLRandom->m_VertexFormat.Size(att_NRML))
                    {
                        case 3: pfSndNormal = &RndVertexes::SndN3h; break;
                        default: MASSERT(0);
                        case 0:  break;
                    }
                    break;
#endif
                default:
                    MASSERT(0);
                    break;
            }

            if (m_pGLRandom->m_VertexFormat.Size(att_COL0))
                pfSndColor = &RndVertexes::SndC4b;

            if (m_pGLRandom->m_VertexFormat.Size(att_EdgeFlag))
                pfSndEdgeFlag = &RndVertexes::SndE1b;

            if (m_pGLRandom->m_VertexFormat.Size(att_COL1))
                pfSndSecColor = &RndVertexes::SndSecC3b;

            if (hasTexCoords)
                pfSndTxc = &RndVertexes::SndTxc;

            if (m_pGLRandom->m_VertexFormat.Size(att_FOGC))
                pfSndFog = &RndVertexes::SndFogf;

            break;
        }
    }

    glBegin( m_Primitive );

    // For each vertex:

    GLuint vx;
    GLuint vxEnd = firstVertex + m_VertexesPerSend;

    if (vxEnd > m_Pickers[RND_VX_MAXTOSEND].GetPick())
    {
        // Debugging: don't send all the vertices we generated.
        vxEnd = m_Pickers[RND_VX_MAXTOSEND].GetPick();
    }

    for ( vx = firstVertex; vx < vxEnd; ++vx )
    {
        // Send the vertex data using the function pointers we set up before.

        GLint vxi = m_VertexIndexes[vx];

        if ( pfSndColor )    (this->*pfSndColor)( vxi );
        if ( pfSndSecColor ) (this->*pfSndSecColor)( vxi );
        if ( pfSndNormal )   (this->*pfSndNormal)( vxi );
        if ( pfSndEdgeFlag ) (this->*pfSndEdgeFlag)( vxi );
        if ( pfSndTxc )      (this->*pfSndTxc)( vxi );
        if ( pfSndFog )      (this->*pfSndFog)( vxi );

        // This one must be last because it provokes the vertex programs!
        if ( pfSndVtx )      (this->*pfSndVtx)( vxi );
    }
    glEnd();
}

//------------------------------------------------------------------------------
// Helper functions for SendDrawBeginEnd().
//
void RndVertexes::SndArrayElement (GLint vx)
{
    glArrayElement(vx);
}

void RndVertexes::SndV2fv (GLint vx)
{
    glVertex2fv( &m_Vertexes[vx].Coords[0] );
}
void RndVertexes::SndV3fv (GLint vx)
{
    glVertex3fv( &m_Vertexes[vx].Coords[0] );
}
void RndVertexes::SndV4fv (GLint vx)
{
    glVertex4fv( &m_Vertexes[vx].Coords[0] );
}

void RndVertexes::SndV2sv (GLint vx)
{
    glVertex2sv( &m_Vertexes[vx].sCoords[0] );
}
void RndVertexes::SndV3sv (GLint vx)
{
    glVertex3sv( &m_Vertexes[vx].sCoords[0] );
}
void RndVertexes::SndV4sv (GLint vx)
{
    glVertex4sv( &m_Vertexes[vx].sCoords[0] );
}

#if defined(GL_LW_half_float)
void RndVertexes::SndV2hv (GLint vx)
{
    glVertex2hvLW( (GLhalf *)&m_Vertexes[vx].sCoords[0] );
}
void RndVertexes::SndV3hv (GLint vx)
{
    glVertex3hvLW( (GLhalf *)&m_Vertexes[vx].sCoords[0] );
}
void RndVertexes::SndV4hv (GLint vx)
{
    glVertex4hvLW( (GLhalf *)&m_Vertexes[vx].sCoords[0] );
}
#endif

void RndVertexes::SndV2f  (GLint vx)
{
    glVertex2f( m_Vertexes[vx].Coords[0], m_Vertexes[vx].Coords[1] );
}
void RndVertexes::SndV3f  (GLint vx)
{
    glVertex3f( m_Vertexes[vx].Coords[0], m_Vertexes[vx].Coords[1], m_Vertexes[vx].Coords[2] );
}
void RndVertexes::SndV4f  (GLint vx)
{
    glVertex4f( m_Vertexes[vx].Coords[0], m_Vertexes[vx].Coords[1], m_Vertexes[vx].Coords[2], m_Vertexes[vx].Coords[3] );
}

void RndVertexes::SndV2s  (GLint vx)
{
    glVertex2s( m_Vertexes[vx].sCoords[0], m_Vertexes[vx].sCoords[1] );
}
void RndVertexes::SndV3s  (GLint vx)
{
    glVertex3s( m_Vertexes[vx].sCoords[0], m_Vertexes[vx].sCoords[1], m_Vertexes[vx].sCoords[2] );
}
void RndVertexes::SndV4s  (GLint vx)
{
    glVertex4s( m_Vertexes[vx].sCoords[0], m_Vertexes[vx].sCoords[1], m_Vertexes[vx].sCoords[2], m_Vertexes[vx].sCoords[3] );
}

#if defined(GL_LW_half_float)
void RndVertexes::SndV2h  (GLint vx)
{
    glVertex2hLW( ((GLhalf*)m_Vertexes[vx].sCoords)[0], ((GLhalf*)m_Vertexes[vx].sCoords)[1] );
}
void RndVertexes::SndV3h  (GLint vx)
{
    glVertex3hLW( ((GLhalf*)m_Vertexes[vx].sCoords)[0], ((GLhalf*)m_Vertexes[vx].sCoords)[1], ((GLhalf*)m_Vertexes[vx].sCoords)[2] );
}
void RndVertexes::SndV4h  (GLint vx)
{
    glVertex4hLW( ((GLhalf*)m_Vertexes[vx].sCoords)[0], ((GLhalf*)m_Vertexes[vx].sCoords)[1], ((GLhalf*)m_Vertexes[vx].sCoords)[2], ((GLhalf*)m_Vertexes[vx].sCoords)[3] );
}
#endif

void RndVertexes::SndN3fv (GLint vx)
{
    glNormal3fv( &m_Vertexes[vx].Normal[0] );
}
void RndVertexes::SndN3sv (GLint vx)
{
    glNormal3sv( &m_Vertexes[vx].sNormal[0] );
}
#if defined(GL_LW_half_float)
void RndVertexes::SndN3hv (GLint vx)
{
    glNormal3hvLW( (GLhalf *)&m_Vertexes[vx].sNormal[0] );
}
#endif

void RndVertexes::SndN3f  (GLint vx)
{
    glNormal3f( m_Vertexes[vx].Normal[0], m_Vertexes[vx].Normal[1], m_Vertexes[vx].Normal[2] );
}
void RndVertexes::SndN3s  (GLint vx)
{
    glNormal3s( m_Vertexes[vx].sNormal[0], m_Vertexes[vx].sNormal[1], m_Vertexes[vx].sNormal[2] );
}

#if defined(GL_LW_half_float)
void RndVertexes::SndN3h  (GLint vx)
{
    glNormal3hLW( ((GLhalf *)m_Vertexes[vx].sNormal)[0], ((GLhalf*)m_Vertexes[vx].sNormal)[1], ((GLhalf*)m_Vertexes[vx].sNormal)[2] );
}
#endif

void RndVertexes::SndC4bv (GLint vx)
{
    if (att_COL0 == m_pGLRandom->m_GpuPrograms.Col0Alias())
        glColor4ubv( &m_Vertexes[vx].Color[0] );
    else
        glVertexAttrib4ubvLW( m_pGLRandom->m_GpuPrograms.Col0Alias(), &m_Vertexes[vx].Color[0] );
}
void RndVertexes::SndC4b  (GLint vx)
{
    if (att_COL0 == m_pGLRandom->m_GpuPrograms.Col0Alias())
        glColor4ub( m_Vertexes[vx].Color[0], m_Vertexes[vx].Color[1], m_Vertexes[vx].Color[2], m_Vertexes[vx].Color[3] );
    else
        glVertexAttrib4ubvLW( m_pGLRandom->m_GpuPrograms.Col0Alias(), &m_Vertexes[vx].Color[0] );
}

void RndVertexes::SndSecC3bv (GLint vx)
{
    glSecondaryColor3ubv(m_Vertexes[vx].SecColor);
}
void RndVertexes::SndSecC3b (GLint vx)
{
    glSecondaryColor3ub(m_Vertexes[vx].SecColor[0], m_Vertexes[vx].SecColor[1], m_Vertexes[vx].SecColor[2]);
}

void RndVertexes::SndE1bv (GLint vx)
{
    glEdgeFlagv( &m_Vertexes[vx].EdgeFlag );
}
void RndVertexes::SndE1b  (GLint vx)
{
    glEdgeFlag( m_Vertexes[vx].EdgeFlag );
}

void RndVertexes::SndTxcv(GLint vx)
{
    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
    {
        switch (m_pGLRandom->m_VertexFormat.TxSize(txu))
        {
            case 0:  break;
            case 1:  glMultiTexCoord1fv(GL_TEXTURE0_ARB + txu, m_Vertexes[vx].Tx[txu]); break;
            case 2:  glMultiTexCoord2fv(GL_TEXTURE0_ARB + txu, m_Vertexes[vx].Tx[txu]); break;
            case 3:  glMultiTexCoord3fv(GL_TEXTURE0_ARB + txu, m_Vertexes[vx].Tx[txu]); break;
            case 4:  glMultiTexCoord4fv(GL_TEXTURE0_ARB + txu, m_Vertexes[vx].Tx[txu]); break;

            default: MASSERT(0);
        }
    }
}

void RndVertexes::SndTxc(GLint vx)
{
    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++)
    {
        switch (m_pGLRandom->m_VertexFormat.TxSize(txu))
        {
            case 0:  break;

            case 1:  glMultiTexCoord1f(GL_TEXTURE0_ARB + txu,
                                    m_Vertexes[vx].Tx[txu][0] ); break;

            case 2:  glMultiTexCoord2f(GL_TEXTURE0_ARB + txu,
                                    m_Vertexes[vx].Tx[txu][0],
                                    m_Vertexes[vx].Tx[txu][1] ); break;

            case 3:  glMultiTexCoord3f(GL_TEXTURE0_ARB + txu,
                                    m_Vertexes[vx].Tx[txu][0],
                                    m_Vertexes[vx].Tx[txu][1],
                                    m_Vertexes[vx].Tx[txu][2] ); break;

            case 4:  glMultiTexCoord4f(GL_TEXTURE0_ARB + txu,
                                    m_Vertexes[vx].Tx[txu][0],
                                    m_Vertexes[vx].Tx[txu][1],
                                    m_Vertexes[vx].Tx[txu][2],
                                    m_Vertexes[vx].Tx[txu][3] ); break;

            default: MASSERT(0);
        }
    }
}

void RndVertexes::SndFogfv(GLint vx)
{
    glFogCoordfv(&m_Vertexes[vx].FogCoord);
}
void RndVertexes::SndFogf(GLint vx)
{
    glFogCoordf(m_Vertexes[vx].FogCoord);
}

//------------------------------------------------------------------------------
bool RndVertexes::TriangleIsClockWise(int vx0, int vx1, int vx2)
{
    //---------------------------------------------------------------------------
    // Triangle has vertexes A,B,C:
    //   slope of A->B is (B.y - A.y) / (B.x - A.x)
    //   slope of A->C is (C.y - A.y) / (C.x - A.x)
    //
    // If slope of A->B is greater than slope of A->C,
    //   then this triangle has its vertexes in clockwise order.
    //
    // Equivalently, if (B.y - A.y)*(C.x - A.x) - (C.y - A.y)*(B.x - A.x) > 0,
    //   then clockwise.
    //   If < 0, counterclockwise.
    //   If == 0, both slopes are the same, triangle is edge-on, zero area.
    //---------------------------------------------------------------------------

    double dX_b, dX_c, dY_b, dY_c;
    double deltaSlope;

    dX_b = m_Vertexes[vx1].Coords[0] - m_Vertexes[vx0].Coords[0];
    dX_c = m_Vertexes[vx2].Coords[0] - m_Vertexes[vx0].Coords[0];
    dY_b = m_Vertexes[vx1].Coords[1] - m_Vertexes[vx0].Coords[1];
    dY_c = m_Vertexes[vx2].Coords[1] - m_Vertexes[vx0].Coords[1];

    deltaSlope = (dY_b * dX_c) - (dY_c * dX_b);

    return (deltaSlope > 0.0);
}

//------------------------------------------------------------------------------
void RndVertexes::FixFacingBySwappingVx12(int vx0, int vx1, int vx2)
{
    //---------------------------------------------------------------------------
    // vx2 is in the wrong place.
    //
    // This makes the triangle formed by vx0,vx1,vx2 be clockwise
    // when it should be counter-clock-wise, or vice-versa.
    //
    // Fix vx2 by swapping its x,y with vx1.
    //
    // WARNING: if vx1 is shared, you have just screwed up the facing
    //          for the other triangle using vx1!
    //          Use FixFacingByTransformingVx2() instead.
    //---------------------------------------------------------------------------
    GLfloat tmp;

    tmp                       = m_Vertexes[vx1].Coords[0];
    m_Vertexes[vx1].Coords[0] = m_Vertexes[vx2].Coords[0];
    m_Vertexes[vx2].Coords[0] = tmp;

    tmp                       = m_Vertexes[vx1].Coords[1];
    m_Vertexes[vx1].Coords[1] = m_Vertexes[vx2].Coords[1];
    m_Vertexes[vx2].Coords[1] = tmp;
}

//------------------------------------------------------------------------------
void RndVertexes::FixFacingByTransformingVx2(int vx0, int vx1, int vx2)
{
    //---------------------------------------------------------------------------
    // vx2 is in the wrong place.
    //
    // This makes the triangle formed by vx0,vx1,vx2 be clockwise
    // when it should be counter-clock-wise, or vice-versa.
    //
    // Fix vx2 by treating it as a point in a different coordinate system:
    //
    //   origin at vx0
    //   +Y axis runs through vx1
    //
    // In this system, if vx2X is >0, the triangle will be clockwise.
    // if vx2X is <0, the triangle will be counter-clock-wise.
    //
    // So, here we do the rotation and translation transform that
    // turns vx2 from the vx0->vx1 coordinate system to the regular world system.
    //---------------------------------------------------------------------------

    RndGeometry::BBox box;
    m_pGLRandom->m_Geometry.GetBBox(&box);

    double vx2Xoffset = m_Vertexes[vx2].Coords[0] - box.left;
    double vx2Yoffset = m_Vertexes[vx2].Coords[1] - box.bottom;

    if (! m_pGLRandom->m_Geometry.DrawClockWise())
        vx2Xoffset *= -1.0;

    GLfloat vx0X = m_Vertexes[vx0].Coords[0];
    GLfloat vx0Y = m_Vertexes[vx0].Coords[1];
    GLfloat vx1X = m_Vertexes[vx1].Coords[0];
    GLfloat vx1Y = m_Vertexes[vx1].Coords[1];

    double dX = vx1X - vx0X;
    double dY = vx1Y - vx0Y;

    if ( (dX == 0.0) && (dY == 0.0) )
    {
        // Flat triangle, no theta.  Don't rotate.
        m_Vertexes[vx2].Coords[0] = (GLfloat) (vx0X + vx2Xoffset);
        m_Vertexes[vx2].Coords[1] = (GLfloat) (vx0Y + vx2Yoffset);
    }
    else
    {
        double theta = LowPrecAtan2(dY, dX) - LOW_PREC_PI/2.0;

        m_Vertexes[vx2].Coords[0] = (GLfloat)(vx2Xoffset*cos(theta) - vx2Yoffset*sin(theta) + vx0X);
        m_Vertexes[vx2].Coords[1] = (GLfloat)(vx2Xoffset*sin(theta) + vx2Yoffset*cos(theta) + vx0Y);
    }
}

//------------------------------------------------------------------------------
void RndVertexes::FixQuadVx3(int vx0, int vx1, int vx2, int vx3)
{
    //---------------------------------------------------------------------------
    // Manipulate the last vertex's X,Y to force a colwex quad:
    //
    // Reinterpret vx3Y as "distance along line segment vx2->vx0".
    // Reinterpret vx3X as "distance along line from vx1->(vx3y pt)".
    //
    // This forces vx3 to be a colwex quad with the same facing as
    // the triangle formed by vx0,1,2.
    //
    // If we linearly interpolate the Z across from vx0 through pt to vx3,
    // the quad will be planar also.
    //---------------------------------------------------------------------------
    RndGeometry::BBox box;
    m_pGLRandom->m_Geometry.GetBBox(&box);

    GLfloat vx0X = m_Vertexes[vx0].Coords[0];
    GLfloat vx0Y = m_Vertexes[vx0].Coords[1];
    GLfloat vx1X = m_Vertexes[vx1].Coords[0];
    GLfloat vx1Y = m_Vertexes[vx1].Coords[1];
    GLfloat vx2X = m_Vertexes[vx2].Coords[0];
    GLfloat vx2Y = m_Vertexes[vx2].Coords[1];

    double vx20dX = vx0X - vx2X;
    double vx20dY = vx0Y - vx2Y;
    double vx20dZ = m_Vertexes[vx0].Coords[2] - m_Vertexes[vx2].Coords[2];

    double dist20;
    double dist1p = m_Vertexes[vx3].Coords[0] - box.left;

    if ( box.height )
        dist20 = (m_Vertexes[vx3].Coords[1] - box.bottom) / box.height;
    else
        dist20 = 0.5;

    double PtX = vx2X + dist20 * vx20dX;
    double PtY = vx2Y + dist20 * vx20dY;
    double PtZ = m_Vertexes[vx2].Coords[2] + dist20 * vx20dZ;

    double vx1PdX = PtX - vx1X;
    double vx1PdY = PtY - vx1Y;
    double vx1PdZ = PtZ - m_Vertexes[vx1].Coords[2];

    double theta = LowPrecAtan2(vx1PdY, vx1PdX);

    m_Vertexes[vx3].Coords[0] = (GLfloat)(PtX + dist1p*cos(theta));
    m_Vertexes[vx3].Coords[1] = (GLfloat)(PtY + dist1p*sin(theta));

    double Zslope;
    if ( (vx1PdX == 0.0) && (vx1PdY == 0.0))
        Zslope = 0.0;
    else
        Zslope = vx1PdZ / sqrt(vx1PdX*vx1PdX + vx1PdY*vx1PdY);

    m_Vertexes[vx3].Coords[2] = (GLfloat)(PtZ + dist1p * Zslope);
}

//------------------------------------------------------------------------------
// Given the vertexes of a triangle or quad, fill in texture coordinates
// for vertexes 2 (and 3 for quads).
//
// For solitary polygons, or the first polygons of a strip or fan,
// forces the bottom edge of the texture to match the vx0->vx1 edge.
//
// Does not stretch or flip the texture, makes shared edges in fans or strips
// match up.
//
// ASSUMES: polygon facing is correct, and quads are planar.
//
void RndVertexes::Generate2dTexCoords(bool IsFirst, GLint txu, GLint vx0, GLint vx1, GLint vx2, GLint vx3 /* = -1 */)
{
    if (m_pGLRandom->m_Geometry.DrawClockWise())
    {
        // Swap vx0 and vx1, so that the texture is always right-side-out
        // on front faces, and flipped on back faces.

        GLint tmp = vx0;
        vx0 = vx1;
        vx1 = tmp;
    }

    if ( IsFirst )
    {
        m_Vertexes[vx0].Tx[txu][0] = 0.0;
        m_Vertexes[vx0].Tx[txu][1] = 0.0;
        m_Vertexes[vx0].Tx[txu][2] = 0.0;
        m_Vertexes[vx0].Tx[txu][3] = 1.0;

        m_Vertexes[vx1].Tx[txu][0] = 1.0;
        m_Vertexes[vx1].Tx[txu][1] = 0.0;
        m_Vertexes[vx1].Tx[txu][2] = 0.0;
        m_Vertexes[vx1].Tx[txu][3] = 1.0;
    }

    // force R to 0, Q to 1.

    m_Vertexes[vx2].Tx[txu][2] = 0.0;
    m_Vertexes[vx2].Tx[txu][3] = 1.0;

    if (vx3 >= 0)
    {
        m_Vertexes[vx3].Tx[txu][2] = 0.0;
        m_Vertexes[vx3].Tx[txu][3] = 1.0;
    }

    // Make a temporary copy of the polygon's vertexes, translated so that
    // vx0 is at the origin.

    double v[4][3];  // four vertexes, 3 coords each (assume Q coord is 1).

    v[0][0] = 0.0;
    v[0][1] = 0.0;
    v[0][2] = 0.0;

    v[1][0] = m_Vertexes[vx1].Coords[0] - m_Vertexes[vx0].Coords[0];
    v[1][1] = m_Vertexes[vx1].Coords[1] - m_Vertexes[vx0].Coords[1];
    v[1][2] = m_Vertexes[vx1].Coords[2] - m_Vertexes[vx0].Coords[2];

    v[2][0] = m_Vertexes[vx2].Coords[0] - m_Vertexes[vx0].Coords[0];
    v[2][1] = m_Vertexes[vx2].Coords[1] - m_Vertexes[vx0].Coords[1];
    v[2][2] = m_Vertexes[vx2].Coords[2] - m_Vertexes[vx0].Coords[2];

    if ( vx3 >= 0 )
    {
        v[3][0] = m_Vertexes[vx3].Coords[0] - m_Vertexes[vx0].Coords[0];
        v[3][1] = m_Vertexes[vx3].Coords[1] - m_Vertexes[vx0].Coords[1];
        v[3][2] = m_Vertexes[vx3].Coords[2] - m_Vertexes[vx0].Coords[2];
    }
    else
    {
        v[3][0] = 0;
        v[3][1] = 0;
        v[3][2] = 0;
    }

    // Find the orthonormal basis for the polygon's own coordinate system:
    //
    //   S axis runs from vx0 to vx1
    //   T axis runs from vx0 towards vx2 (exactly to it, if angle vx2->vx0->vx1 is a right angle)
    //   R axis sticks out perpendilwlar to the surface, in the facing direction.
    //      (i.e. R is the face normal of the polygon)
    //
    //  basis vectors for CCW front triangles:
    //  S = (normalized) vx0->vx1
    //  T = (normalized) R cross S
    //  R = (normalized) S cross vx0->vx2
    //
    //  basis vectors for CCW back triangles:
    //  S = (normalized) vx1->vx0    (reversed S!)
    //  T = (normalized) R cross S
    //  R = (normalized) S cross vx0->vx2
    //
    //  basis vectors for CW front triangles: treat as CCW back
    //  basis vectors for CW back triangles: treat as CCW front

    double S[3];
    double T[3];
    double R[3];

    S[0] = v[1][0];
    S[1] = v[1][1];
    S[2] = v[1][2];

    R[0] = S[1]*v[2][2] - S[2]*v[2][1];
    R[1] = S[2]*v[2][0] - S[0]*v[2][2];
    R[2] = S[0]*v[2][1] - S[1]*v[2][0];

    T[0] = R[1]*S[2] - R[2]*S[1];
    T[1] = R[2]*S[0] - R[0]*S[2];
    T[2] = R[0]*S[1] - R[1]*S[0];

    double SdotS = S[0]*S[0] + S[1]*S[1] + S[2]*S[2];
    double TdotT = T[0]*T[0] + T[1]*T[1] + T[2]*T[2];
    double RdotR = R[0]*R[0] + R[1]*R[1] + R[2]*R[2];

    // If v0->v1 and v0->v2 are approximately parallel,
    // their cross-product (R) will be near 0.0, and
    // we will get floating-point exceptions or rounding problems.

    if ( RdotR > 0.001 )
    {
        // It is safe to do divisions -- normalize the basis vectors.

        SdotS = sqrt(SdotS);
        S[0] = S[0]/SdotS;
        S[1] = S[1]/SdotS;
        S[2] = S[2]/SdotS;

        TdotT = sqrt(TdotT);
        T[0] = T[0]/TdotT;
        T[1] = T[1]/TdotT;
        T[2] = T[2]/TdotT;

        RdotR = sqrt(RdotR);
        R[0] = R[0]/RdotR;
        R[1] = R[1]/RdotR;
        R[2] = R[2]/RdotR;

        // Now transform the vertex x,y,z into vertex s,t,r.
        //
        // v[0] will be 0,0,0 (already is, actually).
        //
        // v[1] will be N,0,0 (where N is length of v0->v1 in world coordinates),
        // since we chose a basis set with the X (i.e. S) axis along v0->v1.
        //
        // v[*][2] will be 0, since we chose a basis set that puts the polygon
        // in the z=0 plane.

        v[1][0] = v[1][0]*S[0] + v[1][1]*S[1] + v[1][2]*S[2];
        v[1][1] = 0.0;
        v[1][2] = 0.0;

        double tmpX = v[2][0];
        double tmpY = v[2][1];
        v[2][0] = tmpX*S[0] + tmpY*S[1] + v[2][2]*S[2];
        v[2][1] = tmpX*T[0] + tmpY*T[1] + v[2][2]*T[2];
        v[2][2] = 0.0;

        if ( vx3 >= 0 )
        {
            tmpX = v[3][0];
            tmpY = v[3][1];
            v[3][0] = tmpX*S[0] + tmpY*S[1] + v[3][2]*S[2];
            v[3][1] = tmpX*T[0] + tmpY*T[1] + v[3][2]*T[2];
            v[3][2] = 0.0;
        }

        // scale so that v0->v1 is exactly 1.0 texture width.
        //
        // If this polygon happens to be a square, the texture will
        // fit it exactly.

        double scaler = v[1][0];
        v[1][0] = 1.0;

        v[2][0] = v[2][0] / scaler;
        v[2][1] = v[2][1] / scaler;

        if ( vx3 >= 0 )
        {
            v[3][0] = v[3][0] / scaler;
            v[3][1] = v[3][1] / scaler;
        }

        if ( ! IsFirst )
        {
            // This is not the first polygon of a strip or fan, so we must adjust
            // our S,T's to make a matching seam with the previous polygon.
            //
            // scale, rotate, translate

            double inDeltaS = m_Vertexes[vx1].Tx[txu][0] - m_Vertexes[vx0].Tx[txu][0];
            double inDeltaT = m_Vertexes[vx1].Tx[txu][1] - m_Vertexes[vx0].Tx[txu][1];

            double scaler = sqrt(inDeltaS*inDeltaS + inDeltaT*inDeltaT);

            double theta = LowPrecAtan2(inDeltaT, inDeltaS);

            double sinTheta = sin(theta);
            double cosTheta = cos(theta);

            v[2][0] *= scaler;
            v[2][1] *= scaler;

            tmpX = v[2][0];
            tmpY = v[2][1];
            m_Vertexes[vx2].Tx[txu][0] = (GLfloat)(tmpX * cosTheta - tmpY*sinTheta + m_Vertexes[vx0].Tx[txu][0]);
            m_Vertexes[vx2].Tx[txu][1] = (GLfloat)(tmpX * sinTheta + tmpY*cosTheta + m_Vertexes[vx0].Tx[txu][1]);

            if ( vx3 >= 0 )
            {
                v[3][0] *= scaler;
                v[3][1] *= scaler;

                tmpX = v[3][0];
                tmpY = v[3][1];
                m_Vertexes[vx3].Tx[txu][0] = (GLfloat)(tmpX * cosTheta - tmpY*sinTheta + m_Vertexes[vx0].Tx[txu][0]);
                m_Vertexes[vx3].Tx[txu][1] = (GLfloat)(tmpX * sinTheta + tmpY*cosTheta + m_Vertexes[vx0].Tx[txu][1]);
            }
        }
        else
        {
            // Is first -- we don't have to align with any previous polygon.

            m_Vertexes[vx2].Tx[txu][0] = (GLfloat) v[2][0];
            m_Vertexes[vx2].Tx[txu][1] = (GLfloat) v[2][1];

            if ( vx3 >= 0 )
            {
                m_Vertexes[vx3].Tx[txu][0] = (GLfloat) v[3][0];
                m_Vertexes[vx3].Tx[txu][1] = (GLfloat) v[3][1];
            }
        }

        // Since we have a correct surface normal (in R) for free, let's store it
        // in place of the crappy random normal we have now.
        if (IsFirst)
        {
            m_Vertexes[vx0].Normal[0] = (GLfloat) R[0];
            m_Vertexes[vx0].Normal[1] = (GLfloat) R[1];
            m_Vertexes[vx0].Normal[2] = (GLfloat) R[2];

            m_Vertexes[vx1].Normal[0] = (GLfloat) R[0];
            m_Vertexes[vx1].Normal[1] = (GLfloat) R[1];
            m_Vertexes[vx1].Normal[2] = (GLfloat) R[2];
        }

        m_Vertexes[vx2].Normal[0] = (GLfloat) R[0];
        m_Vertexes[vx2].Normal[1] = (GLfloat) R[1];
        m_Vertexes[vx2].Normal[2] = (GLfloat) R[2];

        if (vx3 >= 0)
        {
            m_Vertexes[vx3].Normal[0] = (GLfloat) R[0];
            m_Vertexes[vx3].Normal[1] = (GLfloat) R[1];
            m_Vertexes[vx3].Normal[2] = (GLfloat) R[2];
        }

    }
    else
    {
        // RdotR is very small or 0, which means that vx0->vx1 and vx0->vx2 are
        // parallel or nearly so: a flat polygon.

        m_Vertexes[vx2].Tx[txu][0] = m_Vertexes[vx1].Tx[txu][0];
        m_Vertexes[vx2].Tx[txu][1] = m_Vertexes[vx1].Tx[txu][1];

        if (vx3 >= 0)
        {
            m_Vertexes[vx3].Tx[txu][0] = m_Vertexes[vx0].Tx[txu][0];
            m_Vertexes[vx3].Tx[txu][1] = m_Vertexes[vx0].Tx[txu][1];
        }
    }
}

//------------------------------------------------------------------------------
// Generate completely random texture coords, for points and lines.
//
void RndVertexes::RandomFillTxCoords()
{
    GLint    tx;
    GLint    txNum;
    GLubyte  txSize[MaxTxCoords];

    txNum = m_pGLRandom->NumTxCoords();

    for (tx = 0; tx < txNum; tx++)
        txSize[tx] = m_pGLRandom->m_VertexFormat.TxSize(tx);

    Vertex * pV    = m_Vertexes;
    Vertex * pVEnd = m_Vertexes + m_NumVertexes;

    for (/**/; pV < pVEnd; pV++)
    {
        for (tx = 0; tx < txNum; tx++)
        {
            switch (txSize[tx])
            {
                case 4: pV->Tx[tx][3] = m_Pickers[RND_VX_W].FPick();
                case 3: pV->Tx[tx][2] = m_pFpCtx->Rand.GetRandomFloat(-.5, 1.5);
                case 2: pV->Tx[tx][1] = m_pFpCtx->Rand.GetRandomFloat(-.5, 1.5);
                case 1: pV->Tx[tx][0] = m_pFpCtx->Rand.GetRandomFloat(-.5, 1.5);
                case 0: break;
            }

            int txAttr = m_pGLRandom->m_GpuPrograms.GetInTxAttr(ppTxCd0+tx);
            if (txAttr)
            {
                int index = GetLayerIndex(txAttr);
                if (index > 0)
                {
                    pV->Tx[tx][index] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                }
                index = GetShadowIndex(txAttr);
                if (index > 0)
                {
                    pV->Tx[tx][index] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                }
                if (txAttr == Is2dMultisample)
                {
                    pV->Tx[tx][3] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
// Fill in texture coords appropriate for the bound texture on each
// texture unit, skipping tx units that are disabled,
// and skipping tx units that have HW tx coord generation enabled.
//
// If IsFirst is true, fills in all 3 vertexes (4 for quads, if vx3 != -1).
//
// Otherwise, assumes that the first 2 vertexes are shared with a previous
// polygon in a strip or fan, and leaves them alone.
//
// Processing texture coordinates has become more complicated. We need to
// generate values based on the following filters:
// Filter 1: Determine if the texcoord is being used with a texture image or simply a
//           as a generic register access.
// Filter 2: Generate proper xyz coordinates based on the "base" texture target
// Filter 3: Modify the yzw coordinates if we have an array target.
// Filter 4: Modify the zw coordinate if we have a shadow target.
//
void RndVertexes::GenerateTexCoords(bool IsFirst, GLint vx0, GLint vx1, GLint vx2, GLint vx3 /*=-1*/)
{
    GLint Txu;                 // texture unit index
    GLint first2dTxu    = -1;  // texture unit index (has 2d texture coords)

    for (Txu = 0; Txu < m_pGLRandom->NumTxCoords(); Txu++)
    {
        if (! m_pGLRandom->m_VertexFormat.TxSize(Txu))
        {
            if ( IsFirst )
            {
                m_Vertexes[vx0].Tx[Txu][0] = 0.0F;
                m_Vertexes[vx0].Tx[Txu][1] = 0.0F;
                m_Vertexes[vx0].Tx[Txu][2] = 0.0F;
                m_Vertexes[vx0].Tx[Txu][3] = 1.0F;

                m_Vertexes[vx1].Tx[Txu][0] = 0.0F;
                m_Vertexes[vx1].Tx[Txu][1] = 0.0F;
                m_Vertexes[vx1].Tx[Txu][2] = 0.0F;
                m_Vertexes[vx1].Tx[Txu][3] = 1.0F;
            }

            m_Vertexes[vx2].Tx[Txu][0] = 0.0F;
            m_Vertexes[vx2].Tx[Txu][1] = 0.0F;
            m_Vertexes[vx2].Tx[Txu][2] = 0.0F;
            m_Vertexes[vx2].Tx[Txu][3] = 1.0F;

            continue;
        }

        // Filter 1 (if this coordinate is not being used in a texture operation
        //           then just create coordinates based on the components accessed).
        int txAttr = m_pGLRandom->m_GpuPrograms.GetInTxAttr(ppTxCd0+Txu);
        if (!txAttr)
        {
            switch (m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(Txu))
            {
                case 4: txAttr  = IsArrayLwbe; break;
                case 3: txAttr  = Is3d;        break;
                default: txAttr = Is2d;        break;
            }
        }

        // Filter 2 (Create coordinates based on the 'base' target.
        switch (txAttr)             // Coordinates used
        {                           // s t r  layer  shadow  sample
            default: MASSERT(0);    // ------ -----  ------  ------
            case Is1d:              // x - -    -      -
            case Is2d:              // x y -    -      -
            case Is2dMultisample:   // x y -    -      -       w
            case IsArray1d:         // x - -    y      -
            case IsArray2d:         // x y -    z      -
            case IsShadowArray1d:   // x - -    y      z
            case IsShadow1d:        // x - -    -      z
            case IsShadow2d:        // x y -    -      z
            case IsShadowArray2d:   // x y -    z      w
            {
                // Generate 2D coords that wrap the textures around the polygon nicely.

                if ( first2dTxu == -1 )
                {
                    Generate2dTexCoords(IsFirst, Txu, vx0, vx1, vx2, vx3);
                    first2dTxu = Txu;
                }
                else
                {
                    if ( IsFirst )
                    {
                        m_Vertexes[vx0].Tx[Txu][0] = m_Vertexes[vx0].Tx[first2dTxu][0];
                        m_Vertexes[vx0].Tx[Txu][1] = m_Vertexes[vx0].Tx[first2dTxu][1];
                        m_Vertexes[vx0].Tx[Txu][2] = m_Vertexes[vx0].Tx[first2dTxu][2];
                        m_Vertexes[vx0].Tx[Txu][3] = m_Vertexes[vx0].Tx[first2dTxu][3];

                        m_Vertexes[vx1].Tx[Txu][0] = m_Vertexes[vx1].Tx[first2dTxu][0];
                        m_Vertexes[vx1].Tx[Txu][1] = m_Vertexes[vx1].Tx[first2dTxu][1];
                        m_Vertexes[vx1].Tx[Txu][2] = m_Vertexes[vx1].Tx[first2dTxu][2];
                        m_Vertexes[vx1].Tx[Txu][3] = m_Vertexes[vx1].Tx[first2dTxu][3];
                    }

                    m_Vertexes[vx2].Tx[Txu][0] = m_Vertexes[vx2].Tx[first2dTxu][0];
                    m_Vertexes[vx2].Tx[Txu][1] = m_Vertexes[vx2].Tx[first2dTxu][1];
                    m_Vertexes[vx2].Tx[Txu][2] = m_Vertexes[vx2].Tx[first2dTxu][2];
                    m_Vertexes[vx2].Tx[Txu][3] = m_Vertexes[vx2].Tx[first2dTxu][3];

                    if (vx3 >= 0)
                    {
                        m_Vertexes[vx3].Tx[Txu][0] = m_Vertexes[vx3].Tx[first2dTxu][0];
                        m_Vertexes[vx3].Tx[Txu][1] = m_Vertexes[vx3].Tx[first2dTxu][1];
                        m_Vertexes[vx3].Tx[Txu][2] = m_Vertexes[vx3].Tx[first2dTxu][2];
                        m_Vertexes[vx3].Tx[Txu][3] = m_Vertexes[vx3].Tx[first2dTxu][3];
                    }
                }
                break;
            }
            case Is3d:              // x y z    -      -
            {
                // 3D texture access.

                // Scale and copy vertex x,y,z,w into tex coords.
                RndGeometry::BBox box;
                m_pGLRandom->m_Geometry.GetBBox(&box);

                GLfloat depthScale = (GLfloat) box.depth;
                if (depthScale < 1.0)
                    depthScale = 1.0;

                m_Vertexes[vx0].Tx[Txu][0] = (m_Vertexes[vx0].Coords[0] - box.left) / box.width;
                m_Vertexes[vx0].Tx[Txu][1] = (m_Vertexes[vx0].Coords[1] - box.bottom) / box.height;
                m_Vertexes[vx0].Tx[Txu][2] = (m_Vertexes[vx0].Coords[2] - box.deep) / depthScale;
                m_Vertexes[vx0].Tx[Txu][3] = m_Vertexes[vx0].Coords[3];
                m_Vertexes[vx1].Tx[Txu][0] = (m_Vertexes[vx1].Coords[0] - box.left) / box.width;
                m_Vertexes[vx1].Tx[Txu][1] = (m_Vertexes[vx1].Coords[1] - box.bottom) / box.height;
                m_Vertexes[vx1].Tx[Txu][2] = (m_Vertexes[vx1].Coords[2] - box.deep) / depthScale;
                m_Vertexes[vx1].Tx[Txu][3] = m_Vertexes[vx1].Coords[3];
                m_Vertexes[vx2].Tx[Txu][0] = (m_Vertexes[vx2].Coords[0] - box.left) / box.width;
                m_Vertexes[vx2].Tx[Txu][1] = (m_Vertexes[vx2].Coords[1] - box.bottom) / box.height;
                m_Vertexes[vx2].Tx[Txu][2] = (m_Vertexes[vx2].Coords[2] - box.deep) / depthScale;
                m_Vertexes[vx2].Tx[Txu][3] = m_Vertexes[vx2].Coords[3];

                if (vx3 >= 0)
                {
                    m_Vertexes[vx3].Tx[Txu][0] = (m_Vertexes[vx3].Coords[0] - box.left) / box.width;
                    m_Vertexes[vx3].Tx[Txu][1] = (m_Vertexes[vx3].Coords[1] - box.bottom) / box.height;
                    m_Vertexes[vx3].Tx[Txu][2] = (m_Vertexes[vx3].Coords[2] - box.deep) / depthScale;
                    m_Vertexes[vx3].Tx[Txu][3] = m_Vertexes[vx3].Coords[3];
                }
                break;
            }
            case IsLwbe:            // x y z    -      -
            case IsShadowLwbe:      // x y z    -      w
            case IsArrayLwbe:       // x y z    w      -
            case IsShadowArrayLwbe: // x y z    w      *
            {
                // Required of 4 comes from the dot-product texture shader ops,
                // or from a lwbe-map texture.  Generate a direction vector.

                if ( IsFirst )
                {
                    m_pGLRandom->PickNormalfv(m_Vertexes[vx0].Tx[Txu], 0 != m_pFpCtx->Rand.GetRandom(0,1));
                    m_Vertexes[vx0].Tx[Txu][3] = 1.0;
                    m_pGLRandom->PickNormalfv(m_Vertexes[vx1].Tx[Txu], 0 != m_pFpCtx->Rand.GetRandom(0,1));
                    m_Vertexes[vx1].Tx[Txu][3] = 1.0;
                }

                m_pGLRandom->PickNormalfv(m_Vertexes[vx2].Tx[Txu], 0 != m_pFpCtx->Rand.GetRandom(0,1));
                m_Vertexes[vx2].Tx[Txu][3] = 1.0;

                if (vx3 >= 0)
                {
                    m_pGLRandom->PickNormalfv(m_Vertexes[vx3].Tx[Txu], 0 != m_pFpCtx->Rand.GetRandom(0,1));
                    m_Vertexes[vx3].Tx[Txu][3] = 1.0;
                }
                break;
            }
        } // end switch

        // Filter 3 (modify yzw components base on "array" property)
        //          We need to keep the layer value
        int layer = GetLayerIndex(txAttr);
        if (layer > 0)
        {
            if ( IsFirst )
            {
                m_Vertexes[vx0].Tx[Txu][layer] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                m_Vertexes[vx1].Tx[Txu][layer] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
            }

            m_Vertexes[vx2].Tx[Txu][layer] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);

            if (vx3 >= 0)
            {
                m_Vertexes[vx3].Tx[Txu][layer] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
            }
        }

        // Filter 4 (modify zw components based on "shadow" property
        int shadow = GetShadowIndex(txAttr);
        if (shadow > 0)
        {
            if ( IsFirst )
            {
                m_Vertexes[vx0].Tx[Txu][shadow] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                m_Vertexes[vx1].Tx[Txu][shadow] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
            }

            m_Vertexes[vx2].Tx[Txu][shadow] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);

            if (vx3 >= 0)
            {
                m_Vertexes[vx3].Tx[Txu][shadow] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
            }
        }
        // Filter 5: Modify the w component if this texture is a multisampled target
        if (txAttr == Is2dMultisample)
        {
            if ( IsFirst )
            {
                m_Vertexes[vx0].Tx[Txu][3] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
                m_Vertexes[vx1].Tx[Txu][3] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
            }

            m_Vertexes[vx2].Tx[Txu][3] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);

            if (vx3 >= 0)
            {
                m_Vertexes[vx3].Tx[Txu][3] = m_pFpCtx->Rand.GetRandomFloat(0.0, 0.99);
            }
        }

    } // end for each txu
}

//------------------------------------------------------------------------------
int RndVertexes::GetLayerIndex(UINT32 TxAttr) const
{
    switch (TxAttr)             // Coordinates used
    {                           // s t r  layer  shadow  sample
                                // ------ -----  ------  ------
        case IsArray1d:         // x - -    y      -
        case IsShadowArray1d:   // x - -    y      z
            return 1;
        case IsArray2d:         // x y -    z      -
        case IsShadowArray2d:   // x y -    z      w
            return 2;
            break;

        case IsArrayLwbe:       // x y z    w      -
        case IsShadowArrayLwbe: // x y z    w      * (requires a second texture coordinate)
            return 3;
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
int RndVertexes::GetShadowIndex(UINT32 TxAttr) const
{
    switch (TxAttr)             // Coordinates used
    {                           // s t r  layer  shadow  sample
                                // ------ -----  ------  ------
        case IsShadowArray1d:   // x - -    y      z
        case IsShadow1d:        // x - -    -      z
        case IsShadow2d:        // x y -    -      z
            return 2;

        case IsShadowArray2d:   // x y -    z      w
            return  3;

        case IsShadowArrayLwbe: // x y z    w      * (requires a second texture coordinate)
            return 0;           //                   (can't do anything about this here.)
    }
    return 0;
}
//------------------------------------------------------------------------------
const char * RndVertexes::StrPrimitive(GLenum prim) const
{
    if (prim == RND_VX_PRIMITIVE_Solid)
        return "solid";

    const char * names[] =
    {
        "GL_POINTS",
        "GL_LINES",
        "GL_LINE_LOOP",
        "GL_LINE_STRIP",
        "GL_TRIANGLES",
        "GL_TRIANGLE_STRIP",
        "GL_TRIANGLE_FAN",
        "GL_QUADS",
        "GL_QUAD_STRIP",
        "GL_POLYGON",
        "GL_LINES_ADJACENCY_EXT",
        "GL_LINE_STRIP_ADJACENCY_EXT",
        "GL_TRIANGLES_ADJACENCY_EXT",
        "GL_TRIANGLE_STRIP_ADJACENCY_EXT",
        "GL_PATCHES"
    };
    if (prim >= NUMELEMS(names))
    {
        MASSERT(0);
        return "BadPrimitiveValue";
    }

    return names[prim];
}

//------------------------------------------------------------------------------
const char * RndVertexes::StrSendDrawMethod(GLenum meth) const
{
    const char * strs[RND_VX_SEND_METHOD_NUM_METHODS] =
    {
        "Immediate",
        "Vector",
        "Array",
        "DrawArrays",
        "DrawArraysInstanced",
        "DrawElements",
        "DrawElementsInstanced",
        "DrawRangeElements",
        "DrawElementArray"
    };
    if (meth >= NUMELEMS(strs))
    {
        MASSERT(0);
        return "UnknownSendMethod";
    }
    return strs[meth];
}

//------------------------------------------------------------------------------
const char * RndVertexes::StrTypeName(GLenum tn) const
{
    return mglEnumToString(tn,"UnknownTypeName",true);
}
//------------------------------------------------------------------------------
const char * RndVertexes::StrAttrName(GLuint at) const
{
    const char * strs[] =
    {
        "opos",
        "att1",
        "nrml",
        "col0",
        "col1",
        "fogc",
        "att6",
        "att7",
        "tex0",
        "tex1",
        "tex2",
        "tex3",
        "tex4",
        "tex5",
        "tex6",
        "tex7",
        "edge"
    };
    if (at >= NUMELEMS(strs))
    {
        MASSERT(0);
        return "UnknownAttrName";
    }
    return strs[at];
}

RC RndVertexes::AllocateXfbObjects()
{
    glGenBuffers( 2, m_XfbVbo);
    glGenQueries( 2, m_XfbQuery);

    // allocate enough space to hold MaxVertexes worth of attributes but don't
    // initialize the data.

    // All vertex attribute info.
    glBindBuffer(GL_ARRAY_BUFFER, m_XfbVbo[0]);
    glBufferData(GL_ARRAY_BUFFER,  // target
                 MaxCompactedVertexSize * MaxVertexes, // size
                 NULL, // data
                 GL_DYNAMIC_DRAW); // hint

    // Allocate some space on the GLServer side memory for Transform feedback primitives
    glBindBuffer(GL_ARRAY_BUFFER, m_XfbVbo[1]);

    glBufferData(GL_ARRAY_BUFFER,  // target
                 MaxXfbCaptureBytes, // size
                 NULL, // data
                 GL_DYNAMIC_DRAW); // hint

    // release the current binding so we can operate on vertex arrays or raw primitives.
    glBindBuffer(GL_ARRAY_BUFFER,0);

    return mglGetRC();
}

void RndVertexes::SetupXfbPointers()
{
    // Setup the offsets within the vbo based on the attributes that were
    // streamed to the vbo.
    // Note: There are no vertex array pointers for GL_CLIP_DISTANCE_LW, GL_POINT_SIZE,
    //       GL_BACK_PRIMARY_COLOR_LW, or GL_BACK_SECONDARY_COLOR_LW.

    GLuint attr;
    GLint offset = 0;
    GLint stride = 0;

    // All streamed attributes are floats.
    for (attr = 0; attr < m_NumXfbAttribs; attr++)
        stride += sizeof(GLfloat) * m_XfbAttribs[attr].numComponents;

    for (attr = 0; attr < m_NumXfbAttribs; attr++)
    {
        switch (m_XfbAttribs[attr].attrib)
        {
            case GL_POSITION:
                glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(m_XfbAttribs[attr].numComponents,GL_FLOAT,stride,
                                reinterpret_cast<void*>((size_t)offset));
                break;

            case GL_PRIMARY_COLOR:
                glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(m_XfbAttribs[attr].numComponents,GL_FLOAT,stride,
                               reinterpret_cast<void*>((size_t)offset));
                break;

            case GL_SECONDARY_COLOR_LW:
                glEnableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);
                glSecondaryColorPointer(m_XfbAttribs[attr].numComponents,GL_FLOAT,
                                        stride,reinterpret_cast<void*>((size_t)offset));
                break;

            case GL_FOG_COORDINATE:
                glEnableClientState(GL_FOG_COORDINATE_ARRAY_EXT);
                glFogCoordPointer(GL_FLOAT,stride, reinterpret_cast<void*>((size_t)offset));
                break;

            case GL_TEXTURE_COORD_LW:
                glClientActiveTexture(GL_TEXTURE0_ARB + m_XfbAttribs[attr].index);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(m_XfbAttribs[attr].numComponents,GL_FLOAT,stride,
                                reinterpret_cast<void*>((size_t)offset));
                break;

            case GL_GENERIC_ATTRIB_LW:
                glEnableClientState(GL_VERTEX_ATTRIB_ARRAY0_LW + m_XfbAttribs[attr].index);
                glVertexAttribPointerLW(m_XfbAttribs[attr].index,
                        m_XfbAttribs[attr].numComponents, GL_FLOAT, stride,
                        reinterpret_cast<void*>((size_t)offset));
                break;

            // These attributes don't have client enable states or array pointers to set.
            // However the vertex programs may be streaming them out. So ignore them.
            case GL_BACK_PRIMARY_COLOR_LW:      break;
            case GL_BACK_SECONDARY_COLOR_LW:    break;
            case GL_POINT_SIZE:                 break;
            case GL_VERTEX_ID_LW:               break;
            case GL_PRIMITIVE_ID_LW:            break;
            case GL_CLIP_DISTANCE_LW:           break;

            default:
                MASSERT(false); // probably not insync with the vertex program output
                break;

        }
        // all streamed attributes are GLfloat
        offset += sizeof(GLfloat) * m_XfbAttribs[attr].numComponents;
    }
}

RC RndVertexes::SetupXfbAttributes()
{
    // Note1:
    // There aren't any array pointers for GL_CLIP_DISTANCE_LW, GL_POINT_SIZE,
    // GL_BACK_PRIMARY_COLOR_LW, or GL_BACK_SECONDARY_COLOR_LW. You can stream
    // them to the vbo but you won't get consistent results if you try to use
    // the playback feature.
    // Note2:
    // When enabling a particular attribute you must specify the maximum number
    // of components for that attribute or you will get inconsistencies between
    // the feedback data and the stardard processing data.
    RC rc;
    m_NumXfbAttribs = 0;
    UINT32  StdAttr = 0;
    UINT32  UsrAttr = 0;

    m_pGLRandom->m_GpuPrograms.GetXfbAttrFlags( &StdAttr, &UsrAttr);
    if( !StdAttr && !UsrAttr)
    {
        MASSERT(!"StdAttr == 0 && UsrAttr == 0");
        rc = RC::SOFTWARE_ERROR;
    }
    CHECK_RC(rc);

    INT32   txu = -1, clp = -1;
    UINT32  bit;
    GLint   maxComponents = 0;
    INT32   numComponents = 0;
    bool    bCountComponents = false;
    // It turns out that the hardwares 64 limit isnt quite the same as
    // writing 64 outputs from the shader  some outputs might die because
    // the next shader doesnt need them, others might be added because
    // theyre needed by other state (XFB/clipping/etc).  We cant enforce
    // the hardware limit when a program is assembled, so we fall back if
    // you end up writing <= 64 in your vertex program but needing to pass
    // down > 64 for other reasons.
    glGetProgramivARB(GL_VERTEX_PROGRAM_LW,
                      GL_MAX_PROGRAM_RESULT_COMPONENTS_LW,&maxComponents);
    for(bit = 0; (bit < 32) && (numComponents < maxComponents); bit++)
    {
        bCountComponents = true;
        switch( StdAttr & (1<<bit))
        {
            case RndGpuPrograms::Pos   :
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_POSITION;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 4;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                break;

            case RndGpuPrograms::Col0  :
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_PRIMARY_COLOR;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 4;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                break;

            case RndGpuPrograms::Col1  :
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_SECONDARY_COLOR_LW;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 4;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                break;

            case RndGpuPrograms::BkCol0:
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_BACK_PRIMARY_COLOR_LW;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 4;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                m_pGLRandom->m_Misc.XfbPlaybackNotPossible();  // see note above.
                break;

            case RndGpuPrograms::BkCol1:
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_BACK_SECONDARY_COLOR_LW;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 4;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                m_pGLRandom->m_Misc.XfbPlaybackNotPossible();  // see note above.
                break;

            case RndGpuPrograms::Fog   :
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_FOG_COORDINATE;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 1;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                break;

            case RndGpuPrograms::PtSz  :
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_POINT_SIZE;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 1;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                m_pGLRandom->m_Misc.XfbPlaybackNotPossible();  // see note above.
                break;

            case RndGpuPrograms::VertId:
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_VERTEX_ID_LW;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 1;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                m_pGLRandom->m_Misc.XfbPlaybackNotPossible();  // see note above.
                break;

            case RndGpuPrograms::PrimId:
                m_XfbAttribs[m_NumXfbAttribs].attrib = GL_PRIMITIVE_ID_LW;
                m_XfbAttribs[m_NumXfbAttribs].numComponents = 1;
                m_XfbAttribs[m_NumXfbAttribs].index = 0;
                m_NumXfbAttribs++;
                m_pGLRandom->m_Misc.XfbPlaybackNotPossible();  // see note above.
                break;

            case RndGpuPrograms::Tex0  :   txu = 0; break;
            case RndGpuPrograms::Tex1  :   txu = 1; break;
            case RndGpuPrograms::Tex2  :   txu = 2; break;
            case RndGpuPrograms::Tex3  :   txu = 3; break;
            case RndGpuPrograms::Tex4  :   txu = 4; break;
            case RndGpuPrograms::Tex5  :   txu = 5; break;
            case RndGpuPrograms::Tex6  :   txu = 6; break;
            case RndGpuPrograms::Tex7  :   txu = 7; break;

            case RndGpuPrograms::Clp0  :   clp = 0; break;
            case RndGpuPrograms::Clp1  :   clp = 1; break;
            case RndGpuPrograms::Clp2  :   clp = 2; break;
            case RndGpuPrograms::Clp3  :   clp = 3; break;
            case RndGpuPrograms::Clp4  :   clp = 4; break;
            case RndGpuPrograms::Clp5  :   clp = 5; break;

            case 0: bCountComponents = false;   break; // bit is not set

            // undefined bit is set, should never get here
            default:
                MASSERT(!"Undefined Xfb Attribute");
                rc = RC::SOFTWARE_ERROR;
                break;
        }
        if(txu >= 0) // common texture processing
        {
            m_XfbAttribs[m_NumXfbAttribs].attrib = GL_TEXTURE_COORD_LW;
            m_XfbAttribs[m_NumXfbAttribs].numComponents = 4;
            m_XfbAttribs[m_NumXfbAttribs].index = txu;
            m_NumXfbAttribs++;
            txu = -1;
        }
        if(clp >= 0) // common clip plane processing
        {
            m_XfbAttribs[m_NumXfbAttribs].attrib = GL_CLIP_DISTANCE_LW;
            m_XfbAttribs[m_NumXfbAttribs].numComponents = 1;
            m_XfbAttribs[m_NumXfbAttribs].index = clp;
            m_NumXfbAttribs++;
            clp = -1;
            m_pGLRandom->m_Misc.XfbPlaybackNotPossible();  // see note1 above.
        }
        if( bCountComponents)
            numComponents += m_XfbAttribs[m_NumXfbAttribs-1].numComponents;
    } // next StdAttr bit

    // Check for generic attributes
    for(bit = 0; (bit < 32) && (rc == OK); bit++)
    {
        if( UsrAttr & (1<<bit))
        {
            m_XfbAttribs[m_NumXfbAttribs].attrib = GL_GENERIC_ATTRIB_LW;
            m_XfbAttribs[m_NumXfbAttribs].numComponents = 4;
            m_XfbAttribs[m_NumXfbAttribs].index = bit;
            m_NumXfbAttribs++;
        }
    }

    return rc;

}

//----------------------------------------------------------------------------
// Return the XFB attributes based on what is lwrrently setup in the vertex
// format array.
void RndVertexes::GetXfbAttrFlags(UINT32 *pXfbStdAttr, UINT32 *pXfbUsrAttr)
{
    UINT32 stdAttr;
    // Position is always available
    stdAttr = RndGpuPrograms::Pos;

    if (m_pGLRandom->m_VertexFormat.Size(att_COL0))
    {
       stdAttr |=  RndGpuPrograms::Col0;
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_COL1))
    {
       stdAttr |=  RndGpuPrograms::Col1;
    }

    if (m_pGLRandom->m_VertexFormat.Size(att_FOGC))
    {
       stdAttr |=  RndGpuPrograms::Fog;
    }

    // Streaming texture coordinates is a bit weird. If you have texture
    // generation enabled, depending on the texture mode you can be pulling
    // from multiple texture coordinates. So to play it safe we will stream
    // out all 8 texture coords. if texture generation is enabled or we are
    // supplying even one texture from the vertex arrays.
    GLint txu;
    for (txu = 0; txu < m_pGLRandom->NumTxCoords(); txu++ )
    {
        if (m_pGLRandom->m_VertexFormat.TxSize(txu) ||
            m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(txu) )
        {
            stdAttr |= RndGpuPrograms::AllTex;
            break;
        }
    }

    *pXfbStdAttr = stdAttr;
    *pXfbUsrAttr = 0;
}

GLenum RndVertexes::GetXfbPrimitive(GLenum primitive) const
{
    // Geometry & Tessellation programs add another level of complexity. The
    // capture/feedback mechanism oclwrs just before the clipping stage. So
    // we have to take into account how the primitives are transformed by the
    // previously enabled shaders. Vertex transformation can occur from
    // a Vertex, TessCtrl, TessEval, or Geometry shader.
    GLenum prim = m_pGLRandom->m_GpuPrograms.GetFinalPrimOut();
    if (prim != (GLenum)-1)
        primitive = prim;

    switch (primitive)
    {
        case GL_PATCHES:
            return GL_PATCHES;
        case GL_POINTS:
            return GL_POINTS;
        case GL_LINES:
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
            return GL_LINES;
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_QUADS:
        case GL_QUAD_STRIP:
        case GL_POLYGON:
            return GL_TRIANGLES;
    }
    MASSERT(false); // should never get here
    return GL_POINTS;
}
GLint RndVertexes::GetXfbVertices( GLenum primitive, GLint primsWritten)
{
    switch (GetXfbPrimitive(primitive))
    {
        case GL_POINTS:     return primsWritten;
        case GL_LINES:      return primsWritten * 2;
        case GL_TRIANGLES:  return primsWritten * 3;
        case GL_PATCHES:
            return primsWritten *
                    m_pGLRandom->m_GpuPrograms.ExpectedVxPerPatch();
    }
    MASSERT(false); // should never get here
    return 0;
}

