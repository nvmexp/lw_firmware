/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
/**
 * @file   lwprhelper.cpp
 */
#include "opengl/modsgl.h"
#include "core/include/utility.h"
#include "lwprhelper.h"

#define ILWALIDPATHID 0   //Using this value will make gl LwPR Extn API to throw exception

namespace ColourHelper
{
    const Rgba s_ColourTable[BG_MAXCOLOURS] =
    {
        {0.0, 0.0, 0.0, 1.0}    //BG_BLACK
        ,{1.0, 1.0, 1.0, 1.0}   //BG_WHITE
        ,{1.0, 0.0, 0.0, 1.0}   //BG_RED:
        ,{0.0, 1.0, 0.0, 1.0}   //BG_GREEN:
        ,{0.0, 0.0, 1.0, 1.0}   //BG_BLUE:
        ,{0.8f, 0.5, 0.13f, 1.0}  //BG_OCHRE:
        ,{0.5, 0.3f, 0.2f, 1.0}   //BG_MIX0
        ,{0.8f, 0.2f, 0.2f, 1.0}   //BG_MIX1:
    };
};

using namespace ColourHelper;

//-----------------------------------------------------------------------------
PathStyle::PathStyle()
    :m_EndInit (CAP_FLAT),
    m_EndTerm(CAP_FLAT),
    m_Join(JOIN_NONE),
    m_StrokeWd(.5f),
    m_Stroking(true)
{}

//! Describe path design: eg Endcap, line style, stroke width
void PathStyle::SelectPathStyle(EndCap Capping, JoinStyle Join, float width)
{
    m_Stroking = true;
    m_EndInit = Capping;
    m_EndTerm = Capping;
    m_Join = Join;
    m_StrokeWd = width;
}

GLenum PathStyle::JoinToLWformat()
{
    switch (m_Join)
    {
        case JOIN_NONE:
            return GL_NONE;
        case JOIN_ROUND:
            return GL_ROUND_LW;
        case JOIN_BEVEL:
            return GL_BEVEL_LW;
        case JOIN_MITER_REVERT:
            return GL_MITER_REVERT_LW;
        case JOIN_MITER_TRUNCATE:
            return GL_MITER_TRUNCATE_LW;
        default:
            MASSERT(!"Bad join style");
            Printf(Tee::PriHigh, "Bad Join style, continuing with GL_NONE\n");
            return GL_NONE;
    }
}

GLenum PathStyle::CapToLWformat()
{
    switch (m_EndInit)
    {
        case CAP_FLAT:
            return GL_FLAT;
        case CAP_TRIANGLE:
            return GL_TRIANGULAR_LW;
        case CAP_SQUARE:
            return GL_SQUARE_LW;
        case CAP_ROUND:
            return GL_ROUND_LW;
        default:
            MASSERT(!"Bad capping style");
            Printf(Tee::PriHigh, "Bad capping style, continuing with GL_FLAT\n");
            return GL_NONE;
    }
}

//-----------------------------------------------------------------------------
SimplifyPathCommands::SimplifyPathCommands():
    m_ColourIdx(BG_BLUE),
    m_Left(-1),
    m_Right(-1),
    m_Bottom(-1),
    m_Top(-1),
    m_ScaleX(1),
    m_ScaleY(1),
    m_Path(ILWALIDPATHID),
    m_FlagSharedEdge(false)
{}

//-----------------------------------------------------------------------------
SimplifyPathCommands::SimplifyPathCommands(GLuint id):
    m_ColourIdx(BG_BLUE),
    m_Left(-1),
    m_Right(-1),
    m_Bottom(-1),
    m_Top(-1),
    m_ScaleX(1),
    m_ScaleY(1),
    m_Path(id),
    m_FlagSharedEdge(false)
{}

//-----------------------------------------------------------------------------
SimplifyPathCommands::~SimplifyPathCommands()
{}

//-----------------------------------------------------------------------------
void SimplifyPathCommands::SetPathid(GLuint id)
{
    if (m_Path == ILWALIDPATHID)
    {
        m_Path = id;
    }
    else
    {
        Printf(Tee::PriHigh, "Ignoring suggested pathid\n");
        MASSERT(!"Pathid is already set.\n");
    }
}

//-----------------------------------------------------------------------------
void SimplifyPathCommands::cmdM(Coordpair C)
{
    m_Commands.push_back(GL_MOVE_TO_LW);
    m_Coords.push_back(C.X());
    m_Coords.push_back(C.Y());
}

//-----------------------------------------------------------------------------
//! \brief Equivalent to SVG command move to coordinate
//!
//! x,y is the point from where we want to start the design
void SimplifyPathCommands::cmdM(GLfloat x, GLfloat y)
{
    Coordpair tmp(x,y);
    cmdM(tmp);
}

//-----------------------------------------------------------------------------
//! \brief Equivalent to SVG command line to coordinate
//!
//! x,y is the point which should be joined with current point
void SimplifyPathCommands::cmdL(const Coordpair C)
{
    m_Commands.push_back(GL_LINE_TO_LW ^ m_FlagSharedEdge);
    m_Coords.push_back(C.X());
    m_Coords.push_back(C.Y());
}
//-----------------------------------------------------------------------------
void SimplifyPathCommands::cmdL(GLfloat x, GLfloat y)
{
    Coordpair tmp(x,y);
    cmdL(tmp);
}

//-----------------------------------------------------------------------------
//! \brief Equivalent to SVG command lwrve to coordinate
//!
//! Join 2 points with quadratic lwrve
void SimplifyPathCommands::cmdQ(const Coordpair C[2])
{
    m_Commands.push_back(GL_QUADRATIC_LWRVE_TO_LW ^ m_FlagSharedEdge);
    m_Coords.push_back(C[0].X());
    m_Coords.push_back(C[0].Y());
    m_Coords.push_back(C[1].X());
    m_Coords.push_back(C[1].Y());
}
//-----------------------------------------------------------------------------
void SimplifyPathCommands::cmdQ(GLfloat x, GLfloat y, GLfloat x1, GLfloat y1)
{
    Coordpair tmp[2];
    tmp[0].Assign(x, y);
    tmp[1].Assign(x1, y1);
    cmdQ(tmp);
}

//-----------------------------------------------------------------------------
//! \brief Equivalent to SVG command lwrve to coordinate
//!
//! Joins 3 points with cubic lwrve. Middle point is the control point to enable
//! fitting the cubic lwrve between 2 end control points
void SimplifyPathCommands::cmdC(const Coordpair C[3])
{
    m_Commands.push_back(GL_LWBIC_LWRVE_TO_LW ^ m_FlagSharedEdge);
    m_Coords.push_back(C[0].X());
    m_Coords.push_back(C[0].Y());
    m_Coords.push_back(C[1].X());
    m_Coords.push_back(C[1].Y());
    m_Coords.push_back(C[2].X());
    m_Coords.push_back(C[2].Y());
}

//-----------------------------------------------------------------------------
void SimplifyPathCommands::cmdC(GLfloat x, GLfloat y,
        GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
    Coordpair tmp[3];
    tmp[0].Assign(x,y);
    tmp[1].Assign(x1,y1);
    tmp[2].Assign(x2,y2);
    cmdC(tmp);
}

//-----------------------------------------------------------------------------
//! \brief Close command. Joins the last point to the first point (reached with moveto)
//!
void SimplifyPathCommands::cmdZ()
{
    m_Commands.push_back(GL_CLOSE_PATH_LW);
}

//-----------------------------------------------------------------------------
//! \brief Save the submitted path in log file
//!
void SimplifyPathCommands::DumpPathCommand(UINT32 prilevel)
{
    UINT32 c = 0, pts = 0;
    Printf(prilevel, "Path object[%u]: Commands %u, Coordinates %u\n",
            m_Path, static_cast <UINT32> (m_Commands.size()),
            static_cast <UINT32> (m_Coords.size())/2);

    Printf(prilevel, "Projection [%f %f %f %f]\n",
            m_Left, m_Right, m_Bottom, m_Top);
    while (c < m_Commands.size())
    {
        GLubyte command = m_Commands[c];
        if (!c)
        {
            Printf(prilevel, "{\n");
        }
        if (command & GL_SHARED_EDGE_LW)
        {
            Printf(prilevel, "(SHARED EDGE)\t");
            command ^= GL_SHARED_EDGE_LW;
        }
        if (command == GL_CLOSE_PATH_LW)
        {
            Printf(prilevel, "CLOSE_PATH\n");
        }
        else if (command == GL_RELATIVE_LINE_TO_LW)
        {
            Printf(prilevel, "*RELATIVE_LINE_JOIN:\t");
            MASSERT(pts + 1 <= m_Coords.size());
            Printf(prilevel, "(%f, %f)\n", m_Coords[pts], m_Coords[pts+1]);
            pts+=2;
        }
        else if (command == GL_LWBIC_LWRVE_TO_LW)
        {
            Printf(prilevel, "*LWBIC_LWRVE:\n");
            MASSERT(pts + 5 <= m_Coords.size());
            Printf(prilevel, "(%f, %f)  ", m_Coords[pts], m_Coords[pts+1]);
            Printf(prilevel, "(%f, %f)  ", m_Coords[pts+2], m_Coords[pts+3]);
            Printf(prilevel, "(%f, %f)\n", m_Coords[pts+4], m_Coords[pts+5]);
            pts+=6;
        }
        else if (command == GL_QUADRATIC_LWRVE_TO_LW)
        {
            Printf(prilevel, "*QUADRATIC_LWRVE:\n");
            MASSERT(pts + 3 <= m_Coords.size());
            Printf(prilevel, "(%f, %f)  ", m_Coords[pts], m_Coords[pts+1]);
            Printf(prilevel, "(%f, %f)\n", m_Coords[pts+2], m_Coords[pts+3]);
            pts+=4;
        }
        else if (command == GL_LINE_TO_LW)
        {
            Printf(prilevel, "*LINE_JOIN:\t");
            MASSERT(pts + 1 <= m_Coords.size());
            Printf(prilevel, "(%f, %f)\n", m_Coords[pts], m_Coords[pts+1]);
            pts+=2;
        }
        else if (command == GL_MOVE_TO_LW)
        {
            Printf(prilevel, "*MOVE_TO:\t");
            MASSERT(pts + 1 <= m_Coords.size());
            Printf(prilevel, "(%f, %f)\n", m_Coords[pts], m_Coords[pts+1]);
            pts += 2;
        }
        else
        {
            Printf(prilevel, "Unknown command 0x%x\n", command);
        }
        if (c == m_Commands.size() - 1)
        {
            Printf(prilevel, "}\n");
        }
        c++;
    }
}
//-----------------------------------------------------------------------------
//! \brief Submit the completely formed path command for drawing
//!
RC SimplifyPathCommands::SubmitPath(UINT32 logLevel)
{
    if (m_Path == ILWALIDPATHID)
    {
        Printf(Tee::PriHigh, "Path id is not initialized\n");
        MASSERT(0);
        return RC::SOFTWARE_ERROR;
    }
    if (logLevel != Tee::PriNone)
    {
        DumpPathCommand(logLevel);
    }
    glPathCommandsLW(m_Path, GLsizei(m_Commands.size()), &m_Commands[0],
            GLsizei(m_Coords.size()), GL_FLOAT, &m_Coords[0]);
    //do not call glFlush() here. The caller function should handle this
    Printf(Tee::PriDebug, "Path[%d] is initialized for coords %u\n",
            m_Path, (UINT32)m_Coords.size());
    MASSERT(glIsPathLW(m_Path));
    return mglGetRC();
}

//-----------------------------------------------------------------------------
//! \brief Submit commands for setting line characterstics eg. join style
RC SimplifyPathCommands::SubmitPathStyleSettings()
{
    MASSERT(glIsPathLW(m_Path));
    if (m_Stroking && m_Path != ILWALIDPATHID)
    {
        glPathParameteriLW(m_Path, GL_PATH_JOIN_STYLE_LW, JoinToLWformat());
        glPathParameteriLW(m_Path, GL_PATH_END_CAPS_LW, CapToLWformat());
        glPathParameterfLW(m_Path, GL_PATH_STROKE_WIDTH_LW,  m_StrokeWd);
        //Lwrrently Ignore miter limit
    }
    glFlush();
    return mglGetRC();
}

//-----------------------------------------------------------------------------
//! \brief Set orthographic projection coordinates
void SimplifyPathCommands::cmdSetProj(
        GLdouble lt, GLdouble rt,
        GLdouble bt, GLdouble top,
        GLfloat scaleX , GLfloat scaleY)
{
    m_Left = lt;
    m_Right = rt;
    m_Bottom = bt;
    m_Top = top;
    m_ScaleX = scaleX;
    m_ScaleY = scaleY;
    Printf(Tee::PriDebug, "Path[%d] Set Projection boundaries: [%f, %f, %f, %f].\n",
            m_Path, m_Left, m_Right, m_Bottom, m_Top);
}

//-----------------------------------------------------------------------------
//! \brief Path Cover and Fill function
//!
//! \param countingMode Action for setting stencil value. Eg. GL_ILWERT, GL_COU
//!        NT_UP/DOWN_LW
//! \param mask Stencil mask
RC SimplifyPathCommands::CoverAndFill(GLenum countingMode, GLuint mask)
{
    /* Stencil the path: */
    glDisable(GL_STENCIL_TEST);
    glStencilFillPathLW(m_Path, countingMode, mask);
    /*
       The 0x1F mask means the counting uses modulo-32 arithmetic. In
       principle the star's path is simple enough (having a maximum winding
       number of 2) that modulo-4 arithmetic would be sufficient so the mask
       could be 0x3.  Or a mask of all 1's (~0) could be used to count with
       all available stencil bits.
     */
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 0, 0x1F);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glColor3f(0, .8f, .7f);
    glCoverFillPathLW(m_Path, GL_BOUNDING_BOX_LW);
    return mglGetRC();
}

//-----------------------------------------------------------------------------
//! \brief Draw brush strokes on the path suggested. Drawing brush needs to be
//!  setup before. The properties are stroke width, colour, capping style
//! \param reference stencil test reference (bitmask)
//! \param mask      Stencil test mask
RC SimplifyPathCommands::PathStroking(GLint reference, GLuint mask)
{
    RC rc;
    glStencilStrokePathLW(m_Path, reference, mask);
    if (m_ColourIdx > BG_MIX1)
    {
        Printf(Tee::PriHigh, "chosen Colour is out of range. Using colour Black\n");
        m_ColourIdx = BG_BLACK;
    }
    glColor4f(s_ColourTable[m_ColourIdx].r,
                s_ColourTable[m_ColourIdx].g,
                s_ColourTable[m_ColourIdx].b,
                s_ColourTable[m_ColourIdx].a);
    glCoverStrokePathLW(m_Path, GL_BOUNDING_BOX_LW);
    rc = mglGetRC();
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Draw path (stencil test and strokes
RC SimplifyPathCommands::cmdDrawPath(bool fill, bool stroke)
{
    MASSERT(glIsPathLW(m_Path));
    if (m_Path == ILWALIDPATHID)
    {
        Printf(Tee::PriHigh, "Path id is not initialized\n");
        MASSERT(0);
        return RC::SOFTWARE_ERROR;
    }
    RC rc;
    //void GLAPIENTRY glMatrixOrthoEXT
    //(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
    //cartesian system to displayable window
    //Apply projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glScalef(m_ScaleX, m_ScaleY, 1.0f);
    glOrtho(m_Left, m_Right, m_Bottom, m_Top, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    //Draw
    Printf(Tee::PriDebug, "Draw Path id %d, Fill %u, Stroke %u\n",
            m_Path, fill, stroke);

    if (fill)
    {
        CHECK_RC(CoverAndFill(GL_COUNT_UP_LW, ~0));
    }
    if (stroke)
    {
        CHECK_RC(PathStroking(0x1, ~0));
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! Brief Class to help in rendering glyph
//! The default values for m_Emscale and m_pathtemplate are taken from gllwpr
//! demo example
SimplifyGlyphCommands::SimplifyGlyphCommands():
    m_IsInitialized(false),
    m_glyphbase(0),
    m_Emscale(2048),
    m_pathtemplate(~0),
    m_charrange(256), //8-bit character range
    m_yMin(0),
    m_yMax(0),
    m_Underlinepos(0),
    m_UnderlineWd(0),
    m_xtranslate(nullptr),
    m_totalAdvance(0),
    m_initialShift(0),
    m_Stroking(false),
    m_Filling(0),
    m_Usegradient(false),
    m_Underline(false),
    m_Aspectratio(0), m_ZRotateDeg(0),
    m_transx(0), m_transy(0), m_transz(0)
{}

SimplifyGlyphCommands::~SimplifyGlyphCommands()
{
    if (m_xtranslate)
        free( m_xtranslate);
    if (m_IsInitialized == true)
    {
        glDeletePathsLW(m_glyphbase, m_charrange);
    }
}

//-----------------------------------------------------------------------------
//! \brief Get the glyph metrics (which include spacing information and vertical bounds)
void SimplifyGlyphCommands::QueryGlyphMetrics()
{
    //void GLAPIENTRY glGetPathMetricRangeLW(GLbitfield metricQueryMask, GLuint firstPathName, GLsizei numPaths, GLsizei stride, GLfloat *metrics)
    //Get Glyph Metrics
    float fontinfo[4]; //4 is the number of metrics we want

    /* determine the font-wide vertical minimum and maximum for the
       font face by querying the per-font metrics of any one of the glyphs
       from the font face*/
    glGetPathMetricRangeLW(
            GL_FONT_Y_MIN_BOUNDS_BIT_LW | GL_FONT_Y_MAX_BOUNDS_BIT_LW|
            GL_FONT_UNDERLINE_POSITION_BIT_LW | GL_FONT_UNDERLINE_THICKNESS_BIT_LW,
            m_glyphbase+' ',
            1, //count
            static_cast<GLsizei>(NUMELEMS(fontinfo)*sizeof(GLfloat)),
            fontinfo);
    m_yMin = fontinfo[0];
    m_yMax=fontinfo[1];
    m_Underlinepos=fontinfo[2];
    m_UnderlineWd=fontinfo[3];

    Printf(Tee::PriLow,"Y min [%f], max [%f]\n", m_yMin, m_yMax);
    Printf(Tee::PriLow,"underline pos [%f], Width [%f]\n", m_Underlinepos, m_UnderlineWd);
}

//-----------------------------------------------------------------------------
//! \brief Get the glyph metrics (Horizontal spacing)
void SimplifyGlyphCommands::QuerySpacingInfo()
{
    //Get the spacing information
    const char* text = m_Text.c_str();
    size_t Wordlen = m_Text.size();

    GLfloat horizontalAdvance[256];
    glGetPathMetricRangeLW(GL_GLYPH_HORIZONTAL_BEARING_Y_BIT_LW,
            m_glyphbase, m_charrange,
            0,
            horizontalAdvance);
    if(m_xtranslate)
    {
        free(m_xtranslate);
    }
    //Don't know why 2*sizeof is here
    m_xtranslate = (GLfloat *) malloc(2*sizeof(GLfloat) *Wordlen);
    memset(m_xtranslate, 0 , (2*sizeof(GLfloat)*Wordlen));

    /*
       Get the (kerned) separations for the text characters and build
       a set of horizontal translations advancing each successive glyph by
       its kerning distance with the following glyph.
     */
    glGetPathSpacingLW(GL_ACLWM_ADJACENT_PAIRS_LW,
            (GLsizei)Wordlen, GL_UNSIGNED_BYTE, m_Text.c_str(),
            m_glyphbase,
            1.1f, 1.0, GL_TRANSLATE_2D_LW,
            m_xtranslate+2);

    /*Total advance is aclwmulated spacing plus horizontal advance
      of the last glyph*/
    m_totalAdvance = m_xtranslate[2*(Wordlen - 1)] +
        horizontalAdvance[(UINT32)text[Wordlen-1]];
    m_initialShift = m_totalAdvance / Wordlen;
    return;
}

//-----------------------------------------------------------------------------
//! \brief Setup glyph characters
//!
//! Setup characters by setting property for 1 character
RC SimplifyGlyphCommands::Setupglyphs(string disptext)
{
    RC rc;
    m_Text =disptext;

    glPathCommandsLW(m_pathtemplate,
            0, nullptr, 0, GL_FLOAT, nullptr);
    glPathParameterfLW(m_pathtemplate,
            GL_PATH_STROKE_WIDTH_LW, m_Emscale * 0.1f);
    glPathParameteriLW(m_pathtemplate,
            GL_PATH_JOIN_STYLE_LW, GL_MITER_TRUNCATE_LW);
    glPathParameterfLW(m_pathtemplate,
            GL_PATH_MITER_LIMIT_LW, 1.0);

    //Create range of path objects for the character codes in m_charrange
    m_glyphbase = glGenPathsLW(m_charrange);
    Printf(Tee::PriDebug, "Setup Glyphbase id %d\n", m_glyphbase);
    m_IsInitialized = true;

    //Choose built-in font "sans" for drawing
    glPathGlyphRangeLW(m_glyphbase,
            GL_STANDARD_FONT_NAME_LW, "Sans", GL_BOLD_BIT_LW,
            0, m_charrange,
            GL_USE_MISSING_GLYPH_LW, m_pathtemplate,
            m_Emscale * 1.0f);
    QueryGlyphMetrics();
    QuerySpacingInfo();

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 0, ~0);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    rc = mglGetRC();
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Draw underline(quadrilateral)
//!
void SimplifyGlyphCommands::cmdUnderline()
{
    //Draw a rectangle
    float pos = m_Underlinepos;
    float width = m_UnderlineWd;
    glDisable(GL_STENCIL_TEST);
    glColor3f(0, 1, 0);
    glBegin(GL_QUAD_STRIP);
    {
        glVertex2f(0, pos + width);
        glVertex2f(0, pos - width);
        glVertex2f(m_totalAdvance, pos + width);
        glVertex2f(m_totalAdvance, pos - width);
    }
    glEnd();
    glEnable(GL_STENCIL_TEST);
}

//-----------------------------------------------------------------------------
//! \brief Cover stroke
//!
void SimplifyGlyphCommands::cmdStroke()
{
    const char* text = m_Text.c_str();
    const GLsizei Wordlen = (GLsizei) m_Text.size();
    glStencilStrokePathInstancedLW( Wordlen,
            GL_UNSIGNED_BYTE, text, m_glyphbase,
            1, ~0, // Use all stencil bits
            GL_TRANSLATE_2D_LW, m_xtranslate);
    glColor3f(1,1,1);
    glCoverStrokePathInstancedLW(Wordlen,
            GL_UNSIGNED_BYTE, text, m_glyphbase,
            GL_BOUNDING_BOX_OF_BOUNDING_BOXES_LW,
            GL_TRANSLATE_2D_LW, m_xtranslate);

}

//-----------------------------------------------------------------------------
//! \brief Colour filling inside the character edges
//!
void SimplifyGlyphCommands::cmdFill()
{
    const char* text = m_Text.c_str();
    const GLsizei Wordlen = (GLsizei) m_Text.size();
    /* Stencil message into stencil buffer. Results in samples within
       the message's glyphs to have a non zero stencil value
     */
    glStencilFillPathInstancedLW(Wordlen,
            GL_UNSIGNED_BYTE, text, m_glyphbase,
            GL_PATH_FILL_MODE_LW, ~0,
            GL_TRANSLATE_2D_LW, m_xtranslate);
    /*Now cover region of message, colour covered samples (filtered through stencil test)
      and set their stencil back to zero
     */
    if (m_Usegradient)
    {
        GLfloat rgbGen[3][3] = {{0,0,0},
            {0,1,0},
            {0,-1,1}};
        glPathColorGenLW(GL_PRIMARY_COLOR, GL_PATH_OBJECT_BOUNDING_BOX_LW,
                GL_RGB, &rgbGen[0][0]);
    }
    else
    {
        glColor3f(0,0,1); //Stroke colour = blue
    }
    glCoverFillPathInstancedLW(Wordlen,
            GL_UNSIGNED_BYTE, text, m_glyphbase,
            GL_BOUNDING_BOX_OF_BOUNDING_BOXES_LW,
            GL_TRANSLATE_2D_LW, m_xtranslate);
    if (m_Usegradient)
    {
        //Clear gradient
        glPathColorGenLW(GL_PRIMARY_COLOR, GL_NONE, 0, nullptr);
    }
}

//-----------------------------------------------------------------------------
//! \brief Set orhtographic projection coordinates
//!
void SimplifyGlyphCommands::cmdProjection()
{
    /*Follow demo program*/
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if(m_Aspectratio)
    {
        float wd = m_totalAdvance;
        float ht = m_yMax - m_yMin;
        if (ht < wd)
        {
            //Configure calwas so that text is centered with spacing from sides
            float Offset = 0.5f * m_totalAdvance * m_Aspectratio;
            float AvgHeight = 0.5f * (m_yMax + m_yMin);
            glOrtho(-m_initialShift,
                    m_totalAdvance + m_initialShift,
                    AvgHeight - Offset,
                    AvgHeight + Offset,
                    -1.0, 1.0);
        }
        else
        {
            float Offset = 0.5f * ht * m_Aspectratio;
            float AvgWidth = 0.5f * (m_totalAdvance);
            glOrtho(
                    AvgWidth - Offset,
                    AvgWidth + Offset,
                    m_yMin, m_yMax,
                    -1, 1);
        }
    }
    else
    {
        /* Configure calwas coordinate system from (0, yMin) to (totalAdvance, yMax) */
        glOrtho(0,
                m_totalAdvance,
                m_yMin,
                m_yMax,
                -1, 1);
    }
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(m_ZRotateDeg, 0, 0, 1);
    glTranslatef(m_transx, m_transy, m_transz);
}

//-----------------------------------------------------------------------------
//! \brief Public Draw function
//!
RC SimplifyGlyphCommands::Drawglyphs()
{
    Printf(Tee::PriDebug, "Draw Glyphbase id %d\n", m_glyphbase);
    cmdProjection();
    if (m_Underline)
    {
        cmdUnderline();
    }

    if (m_Stroking)
    {
        cmdStroke();
    }
    if (m_Filling)
    {
        cmdFill();
    }
    RC rc = mglGetRC();
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Delete formed paths
//!
RC SimplifyGlyphCommands::CleanCommands()
{
    if(m_IsInitialized)
    {
        glDeletePathsLW(m_glyphbase, m_charrange);
        m_IsInitialized = false;
    }
    RC rc = mglGetRC();
    return rc;
}
