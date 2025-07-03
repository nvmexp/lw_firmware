/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010,2013,2016,2019 by LWPU Corporation.  All rights reserved.  All
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

using namespace GLRandom;

//------------------------------------------------------------------------------
RndVertexFormat::RndVertexFormat(GLRandomTest * pglr)
    : GLRandomHelper(pglr, RND_VXFMT_NUM_PICKERS, "VertexFormat")
    , m_IndexType(0)
{
}

//------------------------------------------------------------------------------
RndVertexFormat::~RndVertexFormat()
{
}

//------------------------------------------------------------------------------
GLint RndVertexFormat::Size(VxAttribute attr)
{
    MASSERT((attr >= 0) && (attr < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG));

    return m_Size[attr];
}

//------------------------------------------------------------------------------
GLint RndVertexFormat::TxSize(GLint txu)
{
    MASSERT((txu >= 0) && (txu < MaxTxCoords));

    return m_Size[att_TEX0 + txu];
}

GLint RndVertexFormat::TxType(GLint txu)
{
    MASSERT((txu >= 0) && (txu < MaxTxCoords));
    return m_Type[att_TEX0 + txu];
}
//------------------------------------------------------------------------------
GLenum   RndVertexFormat::Type(VxAttribute attr)
{
    MASSERT((attr >= 0) && (attr < att_NUM_ATTRIBUTES_COUNTING_EDGE_FLAG));

    return m_Type[attr];
}

//------------------------------------------------------------------------------
GLenum RndVertexFormat::IndexType()
{
    return m_IndexType;
}

//------------------------------------------------------------------------------
RC RndVertexFormat::SetDefaultPicker(int picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_VXFMT_COORD_SIZE:
            // xy, xyz, or xyzw coordinates
            m_Pickers[RND_VXFMT_COORD_SIZE].ConfigRandom();
            m_Pickers[RND_VXFMT_COORD_SIZE].AddRandItem(1, 2);
            m_Pickers[RND_VXFMT_COORD_SIZE].AddRandItem(5, 3);
            m_Pickers[RND_VXFMT_COORD_SIZE].AddRandItem(1, 4);
            m_Pickers[RND_VXFMT_COORD_SIZE].CompileRandom();
            break;

        case RND_VXFMT_COORD_DATA_TYPE:
            // double, float, short, etc.
            m_Pickers[RND_VXFMT_COORD_DATA_TYPE].ConfigRandom();
            m_Pickers[RND_VXFMT_COORD_DATA_TYPE].AddRandItem(1, GL_SHORT);
            m_Pickers[RND_VXFMT_COORD_DATA_TYPE].AddRandItem(4, GL_FLOAT);
#if defined(GL_LW_half_float)
            m_Pickers[RND_VXFMT_COORD_DATA_TYPE].AddRandItem(1, GL_HALF_FLOAT_LW);
#else
            m_Pickers[RND_VXFMT_COORD_DATA_TYPE].AddRandItem(1, GL_FLOAT);
#endif
            m_Pickers[RND_VXFMT_COORD_DATA_TYPE].CompileRandom();
            break;

        case RND_VXFMT_HAS_NORMAL:
            // has/not got a normal vector per vertex
            m_Pickers[RND_VXFMT_HAS_NORMAL].ConfigRandom();
            m_Pickers[RND_VXFMT_HAS_NORMAL].AddRandItem(4, GL_TRUE);
            m_Pickers[RND_VXFMT_HAS_NORMAL].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_VXFMT_HAS_NORMAL].CompileRandom();
            break;

        case RND_VXFMT_NORMAL_DATA_TYPE:
            // double, float, short, etc.
            m_Pickers[RND_VXFMT_NORMAL_DATA_TYPE].ConfigRandom();
            m_Pickers[RND_VXFMT_NORMAL_DATA_TYPE].AddRandItem(1, GL_SHORT);
            m_Pickers[RND_VXFMT_NORMAL_DATA_TYPE].AddRandItem(4, GL_FLOAT);
#if defined(GL_LW_half_float)
            m_Pickers[RND_VXFMT_NORMAL_DATA_TYPE].AddRandItem(1, GL_HALF_FLOAT_LW);
#else
            m_Pickers[RND_VXFMT_NORMAL_DATA_TYPE].AddRandItem(1, GL_FLOAT);
#endif
            m_Pickers[RND_VXFMT_NORMAL_DATA_TYPE].CompileRandom();
            break;

        case RND_VXFMT_HAS_PRICOLOR:
            // has/not got a color per vertex
            m_Pickers[RND_VXFMT_HAS_PRICOLOR].ConfigRandom();
            m_Pickers[RND_VXFMT_HAS_PRICOLOR].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_VXFMT_HAS_PRICOLOR].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_VXFMT_HAS_PRICOLOR].CompileRandom();
            break;

        case RND_VXFMT_HAS_EDGE_FLAG:
            // has/not got edge flag per vertex
            m_Pickers[RND_VXFMT_HAS_EDGE_FLAG].ConfigRandom();
            m_Pickers[RND_VXFMT_HAS_EDGE_FLAG].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_VXFMT_HAS_EDGE_FLAG].AddRandItem(4, GL_FALSE);
            m_Pickers[RND_VXFMT_HAS_EDGE_FLAG].CompileRandom();
            break;

        case RND_VXFMT_TXU_COORD_SIZE:
            // number of texture coordinates: 1,2,3,4 == s, st, str, strq
            m_Pickers[RND_VXFMT_TXU_COORD_SIZE].ConfigRandom();
            m_Pickers[RND_VXFMT_TXU_COORD_SIZE].AddRandRange(1, 1,4);
            m_Pickers[RND_VXFMT_TXU_COORD_SIZE].CompileRandom();
            break;

        case RND_VXFMT_HAS_SECCOLOR:
            // do/don't send secondary color
            m_Pickers[RND_VXFMT_HAS_SECCOLOR].ConfigRandom();
            m_Pickers[RND_VXFMT_HAS_SECCOLOR].AddRandItem(1, GL_FALSE);
            m_Pickers[RND_VXFMT_HAS_SECCOLOR].AddRandItem(2, GL_TRUE);
            m_Pickers[RND_VXFMT_HAS_SECCOLOR].CompileRandom();
            break;

        case RND_VXFMT_HAS_FOG_COORD:
            // do/don't send per-vertex fog coord
            m_Pickers[RND_VXFMT_HAS_FOG_COORD].ConfigRandom();
            m_Pickers[RND_VXFMT_HAS_FOG_COORD].AddRandItem(2, GL_FALSE);
            m_Pickers[RND_VXFMT_HAS_FOG_COORD].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_VXFMT_HAS_FOG_COORD].CompileRandom();
            break;

        case RND_VXFMT_INDEX_TYPE:
            // data type for vertex indexes
            m_Pickers[RND_VXFMT_INDEX_TYPE].ConfigRandom();
            m_Pickers[RND_VXFMT_INDEX_TYPE].AddRandItem(1, GL_UNSIGNED_SHORT);
            m_Pickers[RND_VXFMT_INDEX_TYPE].AddRandItem(1, GL_UNSIGNED_INT);
            m_Pickers[RND_VXFMT_INDEX_TYPE].CompileRandom();
            break;
    }
    return OK;
}

//------------------------------------------------------------------------------
void RndVertexFormat::Pick()
{
    // Do the random picks.

    m_Size[att_OPOS]     = m_Pickers[RND_VXFMT_COORD_SIZE].Pick();
    m_Size[att_1]        = GL_FALSE;
    m_Size[att_NRML]     = m_Pickers[RND_VXFMT_HAS_NORMAL].Pick();
    m_Size[att_COL0]     = m_Pickers[RND_VXFMT_HAS_PRICOLOR].Pick();
    m_Size[att_COL1]     = m_Pickers[RND_VXFMT_HAS_SECCOLOR].Pick();
    m_Size[att_FOGC]     = m_Pickers[RND_VXFMT_HAS_FOG_COORD].Pick();
    m_Size[att_6]        = GL_FALSE;
    m_Size[att_7]        = GL_FALSE;
    m_Size[att_TEX0]     = m_Pickers[RND_VXFMT_TXU_COORD_SIZE].Pick();
    m_Size[att_TEX1]     = m_Pickers[RND_VXFMT_TXU_COORD_SIZE].Pick();
    m_Size[att_TEX2]     = m_Pickers[RND_VXFMT_TXU_COORD_SIZE].Pick();
    m_Size[att_TEX3]     = m_Pickers[RND_VXFMT_TXU_COORD_SIZE].Pick();
    m_Size[att_TEX4]     = m_Pickers[RND_VXFMT_TXU_COORD_SIZE].Pick();
    m_Size[att_TEX5]     = m_Pickers[RND_VXFMT_TXU_COORD_SIZE].Pick();
    m_Size[att_TEX6]     = m_Pickers[RND_VXFMT_TXU_COORD_SIZE].Pick();
    m_Size[att_TEX7]     = m_Pickers[RND_VXFMT_TXU_COORD_SIZE].Pick();
    m_Size[att_EdgeFlag] = m_Pickers[RND_VXFMT_HAS_EDGE_FLAG].Pick();
    m_Type[att_OPOS]     = m_Pickers[RND_VXFMT_COORD_DATA_TYPE].Pick();
    m_Type[att_NRML]     = m_Pickers[RND_VXFMT_NORMAL_DATA_TYPE].Pick();
    m_IndexType          = m_Pickers[RND_VXFMT_INDEX_TYPE].Pick();

    //
    // Fix up our picks.
    //
    // Since we will set the global value of each of these attributes just
    // before sending the vertexes, all attributes will always be available
    // to the T&L unit or vertex program.
    //
    // All we are deciding here is which are changing per-vertex.
    // We don't really care here, whether the attribute will be used or not.
    //

    //
    // OPOS (object-space position) must always be sent, and GLRandom only
    // supports xy, xyz, or xyzw coordinates in float or short types.
    //

    // First lets filter out the attributes that are not required by the vertex
    // program. There is no sense sending attributes that will not get used.
    if (!m_pGLRandom->m_GpuPrograms.VxAttrNeeded(att_NRML))
        m_Size[att_NRML] = 0;
    if (!m_pGLRandom->m_GpuPrograms.VxAttrNeeded(att_COL0))
        m_Size[att_COL0] = 0;
    if (!m_pGLRandom->m_GpuPrograms.VxAttrNeeded(att_COL1))
        m_Size[att_COL1] = 0;
    if (!m_pGLRandom->m_GpuPrograms.VxAttrNeeded(att_FOGC))
        m_Size[att_FOGC] = 0;

    if (m_Size[att_OPOS] < 2)
        m_Size[att_OPOS] = 2;
    if (m_Size[att_OPOS] > 4)
        m_Size[att_OPOS] = 4;
   switch (m_Type[att_OPOS])
   {
      case GL_SHORT:
      case GL_FLOAT:
         break;

#if defined(GL_LW_half_float)
        case GL_HALF_FLOAT_LW:
            if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_half_float))
                break;
         /* else fall through */
#endif
        default:
            m_Type[att_OPOS] = GL_FLOAT;
            break;
    }

    //
    // NRML (vertex normal for lighting) must be xyz only,
    // GLRandom suports float or short.
    //
    if (m_Size[att_NRML] != 0)
        m_Size[att_NRML] = 3;
    switch (m_Type[att_NRML])
    {
        case GL_SHORT:
        case GL_FLOAT:
            break;

#if defined(GL_LW_half_float)
        case GL_HALF_FLOAT_LW:
            if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_half_float))
                break;
         /* else fall through */
#endif
        default:
            m_Type[att_NRML] = GL_FLOAT;
            break;
    }

    //
    // COL0 (primary color or material color) must be xyzw (really rgba),
    // GLRandom supports only ubyte.
    //
    if (m_Size[att_COL0] != 0)
        m_Size[att_COL0] = 4;
    m_Type[att_COL0] = GL_UNSIGNED_BYTE;

    //
    // COL1 (secondary color, i.e. spelwlar hightlight) must be xyz (really rgb),
    // GLRandom supports only ubyte.
    //
    if (m_Size[att_COL1] != 0)
        m_Size[att_COL1] = 3;
    m_Type[att_COL1] = GL_UNSIGNED_BYTE;

    //
    // FOGC (fog coordinate) is a scalar (x only), GLRandom supports only float.
    // If Fog and XFB are enabled we must pull fog coordinates from the vertex
    // array.
    if ((m_pGLRandom->m_Fog.IsEnabled() &&
            m_pGLRandom->m_Misc.XfbEnabled(xfbModeCapture)) ||
            (m_Size[att_FOGC] != 0))
        m_Size[att_FOGC] = 1;
    m_Type[att_FOGC] = GL_FLOAT;

    //
    // att_6 and att_7 have no standard T&L meaning.  They might be used by
    // a vertex program, but their meaning depends entirely on the program.
    // GLRandom supports only 4 floats.
    //
    m_Size[att_1] = 0;            // used to be att_WGHT for lw10 "skinning"
    m_Type[att_1] = GL_FLOAT;
    m_Size[att_6] = 0;
    m_Type[att_6] = GL_FLOAT;
    m_Size[att_7] = 0;
    m_Type[att_7] = GL_FLOAT;

    //
    // Texture coords are handled specially.
    //
    // We'll try to match up with exactly what the texture units require.
    // This is because generating unused tx coords is computationally expensive,
    // and not sending per-vertex tx coords that are needed doesn't exercise
    // the texture units well.
    //
    // GLRandom supports 0 to 4 floats.
    //
    GLint txu;
    for (txu = 0; txu < MaxTxCoords; txu++)
    {
        GLint txuRqd = m_pGLRandom->m_GpuPrograms.TxCoordComponentsNeeded(txu);

        if ((txuRqd == 0) || m_pGLRandom->m_Geometry.TxGenEnabled(txu))
        {
            m_Size[att_TEX0 + txu] = 0;
        }
        else
        {
            if (m_Size[att_TEX0 + txu] < txuRqd)
                m_Size[att_TEX0 + txu] = txuRqd;
        }
        m_Type[att_TEX0 + txu] = GL_FLOAT;
    }

    //
    // Edge flag isn't really a vertex parameter, it is used per-primitive
    // to determine which edges to draw when rendering polygons
    // in GL_LINE or GL_POINT mode.
    // No LW hardware so far (up to lw2A) implements this in HW.
    //
    // GLRandom supports 1 ubyte.
    //
    if (m_Size[att_EdgeFlag] != 0)
        m_Size[att_EdgeFlag] = 1;
    m_Type[att_EdgeFlag] = GL_UNSIGNED_BYTE;

    // Vertex index type.
    // Hardware only supports 16 or 32 unsigned ints, although various GL
    // entry points allow other formats.
    if (m_IndexType != GL_UNSIGNED_SHORT)
        m_IndexType = GL_UNSIGNED_INT;

    m_pGLRandom->LogData(RND_VXFMT, &m_Size[0], sizeof(m_Size));
    m_pGLRandom->LogData(RND_VXFMT, &m_Type[0], sizeof(m_Type));
    m_pGLRandom->LogData(RND_VXFMT, &m_IndexType, sizeof(m_IndexType));
}

//------------------------------------------------------------------------------
void RndVertexFormat::Print(Tee::Priority pri)
{
}

//------------------------------------------------------------------------------
RC RndVertexFormat::Send()
{
    return OK;
}

//------------------------------------------------------------------------------
RC RndVertexFormat::Cleanup()
{
    return OK;
}

RC RndVertexFormat::Playback()
{
    if (RND_TSTCTRL_DRAW_ACTION_vertices != m_pGLRandom->m_Misc.DrawAction())
        return OK; //!< we are drawing pixels in ths round.

    if (m_Size[att_FOGC])
        glFogi(GL_FOG_COORD_SRC, GL_FOG_COORD);

    return mglGetRC();

}

