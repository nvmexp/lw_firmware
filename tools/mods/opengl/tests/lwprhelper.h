/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
/**
 * @file   lwprhelper.h
 */
#ifndef INCLUDED_LWPRHELPER_H
#define INCLUDED_LWPRHELPER_H
#include "opengl/modsgl.h"
#define ILWALIDPATHID 0

namespace ColourHelper
{
    enum
    {
        //Colour code
        BG_BLACK,
        BG_WHITE,
        BG_RED,
        BG_GREEN,
        BG_BLUE,
        BG_OCHRE,
        BG_MIX0,
        BG_MIX1,
        BG_MAXCOLOURS
    };
    struct Rgba
    {
        GLfloat r;
        GLfloat g;
        GLfloat b;
        GLfloat a;
    };
    extern const Rgba s_ColourTable[BG_MAXCOLOURS];
};

/*
 * Class for 2d coordinates. PathCommands depend upon this
*/
class Coordpair
{
    public:
        Coordpair(GLfloat x, GLfloat y) {m_X = x; m_Y = y;}
        Coordpair() {m_X = 0; m_Y = 0;}; //def origin

        //Access
        GLfloat X() const{ return m_X;}
        GLfloat Y() const{ return m_Y;}

        //Modify
        void Assign(GLfloat x, GLfloat y) {m_X =x; m_Y = y;}

        Coordpair & operator+=(const Coordpair & C)
        {
            if (this != &C)
            {
                m_X += C.X();
                m_Y += C.Y();
            }
            return *this;
        }
        Coordpair & operator-=(const Coordpair & C)
        {
            if (this != &C)
            {
                m_X -= C.X();
                m_Y -= C.Y();
            }
            return *this;
        }
        const Coordpair operator+ (const Coordpair C) const
        {
            Coordpair retval(m_X + C.X(), m_Y + C.Y());
            return retval;
        }
        const Coordpair operator- (const Coordpair C) const
        {
            Coordpair retval(m_X - C.X(), m_Y - C.Y());
            return retval;
        }

        const Coordpair operator* (const UINT32 scale) const
        {
            Coordpair retval(m_X * scale, m_Y * scale);
            return retval;
        }

        //Compare
        bool operator==(const Coordpair &C) const
        {
            return ( m_X == C.X()) && (m_Y == C.Y());
        }
    private:
        GLfloat m_X;
        GLfloat m_Y;
};

class PathStyle
{
    public:
        enum EndCap {CAP_FLAT, CAP_TRIANGLE, CAP_SQUARE, CAP_ROUND};
        enum JoinStyle {JOIN_NONE, JOIN_ROUND, JOIN_BEVEL,
           JOIN_MITER_REVERT, JOIN_MITER_TRUNCATE};
        PathStyle();
        void SelectPathStyle(EndCap style)
            {m_EndInit = style; m_Stroking = true;}
        void SelectPathStyle(EndCap Capping, JoinStyle Join, float width);
        void SetStrokeWidth(GLfloat StrokeWd)
            {m_StrokeWd = StrokeWd; m_Stroking = true;}
    protected:
        GLenum JoinToLWformat();
        GLenum CapToLWformat();

        EndCap m_EndInit, m_EndTerm;
        JoinStyle m_Join;
        float m_StrokeWd;
        bool m_Stroking;
};

class SimplifyPathCommands: public PathStyle
{
    //glPathCommandsLW(GLuint path, GLsizei numCommands, const GLubyte *commands, GLsizei numCoords, GLenum coordType, const GLvoid *coords)
    public:
        SimplifyPathCommands();
        explicit SimplifyPathCommands(GLuint id);
        ~SimplifyPathCommands();
        void SetPathid(GLuint id);
        UINT32 GetPathid() { return m_Path; };
        void SetPathColor(UINT32 idx) { m_ColourIdx = idx; }
        void cmdSetProj(GLdouble lt, GLdouble Rt, GLdouble Bt, GLdouble top,
                GLfloat scaleXi =1, GLfloat scaleY = 1);
        RC SubmitPath(UINT32 logPath = Tee::PriNone);

        void Clear(){m_Commands.clear(); m_Coords.clear();}
        void cmdM(const Coordpair C);
        void cmdM(GLfloat x, GLfloat y);
        void cmdL(const Coordpair C);
        void cmdL(GLfloat x, GLfloat y);
        void cmdC(const Coordpair C[3]);
        void cmdC(const Coordpair C0,
                  const Coordpair C1,
                  const Coordpair C2)
        {
            cmdC(C0.X(), C0.Y(), C1.X(), C1.Y(), C2.X(), C2.Y());
        }
        void cmdC(GLfloat x, GLfloat y,
                GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
        void cmdQ(const Coordpair C[2]);
        void cmdQ(Coordpair C0, Coordpair C1)
        {
            cmdQ(C0.X(), C0.Y(), C1.X(), C1.Y());
        }
        void cmdQ(GLfloat x, GLfloat y, GLfloat x1, GLfloat y1);
        void cmdZ();
        void TestSharedEdge(bool flag)
        {m_FlagSharedEdge = (flag) ? GL_SHARED_EDGE_LW : 0;}

        RC cmdDrawPath(bool fill, bool stroke);
       /*Path style*/
        RC SubmitPathStyleSettings();
        void ClearPathCommands(){m_Coords.clear(); m_Commands.clear();}
    private:
        RC CoverAndFill(GLenum countingMode, GLuint mask=~0);
        RC PathStroking(GLint reference, GLuint mask=~0);
        void DumpPathCommand(UINT32 prilevel);
        UINT32 m_ColourIdx;
        GLdouble m_Left;
        GLdouble m_Right;
        GLdouble m_Bottom;
        GLdouble m_Top;
        GLfloat m_ScaleX;
        GLfloat m_ScaleY;
        GLuint m_Path;
        vector<GLubyte> m_Commands;
        vector<GLfloat> m_Coords;
        UINT32 m_FlagSharedEdge;
};

class SimplifyGlyphCommands
{
    public:
        SimplifyGlyphCommands();
        ~SimplifyGlyphCommands();

        //Generate glyph paths, Draw, delete paths
        RC Setupglyphs(string text);
        RC Drawglyphs();
        RC CleanCommands();

        //Draw Design
        void cmdUnderline();
        void cmdStroke();
        void cmdFill();
        void cmdProjection();
        void SetText(string val) {m_Text = val;}

        //Enable side features
        void SetAspectRatio(float val) {m_Aspectratio = val;}
        void SetZRotation(float degrees) {m_ZRotateDeg = degrees;}
        void SetupTranslation(float x, float y, float z)
            {m_transx = x; m_transy = y; m_transz = x;}
        void EnableUnderline() {m_Underline = true;}
        void DisableUnderline() {m_Underline = false;}
        void EnableStroking() {m_Stroking = true;}
        void DisableStroking() {m_Stroking = false;}
        void EnableFilling() {m_Filling = true;}
        void DisableFilling() {m_Filling = false;}
        void UseColourGradient() {m_Usegradient = true;}
        void DisableColourGradient() {m_Usegradient = false;}

    private:

        bool m_IsInitialized;
        GLuint m_glyphbase;    //Most important: Range of glyphs
        string m_Text;         //Message
        const int m_Emscale;   //Truetype convention
        const GLuint m_pathtemplate; //Non-existant path object
        const int m_charrange; //ISO/IEC 8859-1 8-bit character range

        GLfloat m_yMin;        //Verticle min and max
        GLfloat m_yMax;
        GLfloat m_Underlinepos; //Position and width of character underline
        GLfloat m_UnderlineWd;
        GLfloat *m_xtranslate;  //Kerning separation between 2 characters
        GLfloat m_totalAdvance;
        GLfloat m_initialShift;

        //Design flags
        bool m_Stroking, m_Filling, m_Usegradient, m_Underline;
        float m_Aspectratio, m_ZRotateDeg; //Projection and View
        float m_transx, m_transy, m_transz; //view translate

        void QueryGlyphMetrics();
        void QuerySpacingInfo();
};
#endif //#ifndef INCLUDED_LWPRHELPER_H
