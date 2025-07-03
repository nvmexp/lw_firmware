/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/coverage.h"
#include "core/include/rc.h"
#include <map>
#include <set>
namespace TestCoverage {

//--------------------------------------------------------------------------------------------------
// Coverage object to track hardware ISA instructions.
// All shaders (vertex, geometry, tessellation, fragment, & compute) will generate a series of
// ISA (Instruction Set Architecture) opcodes. This coverage object tracks the usage of all these
// opcodes.
// Note: It does not track the attributes
class HwIsaCoverageObject: public CoverageObject
{
public:
//    static CoverageObject * Instance();
    virtual         ~HwIsaCoverageObject();
                    HwIsaCoverageObject();

private:
    virtual string  ComponentEnumToString(UINT32 component);
    virtual RC      Initialize();
    // Non-copyable
                    HwIsaCoverageObject(const HwIsaCoverageObject&) {}
                    HwIsaCoverageObject & operator=(const HwIsaCoverageObject*){return *this;}

//   static HwIsaCoverageObject * s_Instance;
};

//--------------------------------------------------------------------------------------------------
// Coverage object to track hardware texture formats.
// OpenGL provides several different types of texture formats. These formats have internal and
// external representations. When the texture image is created the internal format is used however
// when the texture is rendered to each pixel in the display surface it is colwerted to the external
// format. This colwersion is performed by a small part of the hardware.
// This object tracks the different types of internal formats used by the OpenGL based tests.
class HwTextureCoverageObject: public CoverageObject
{
public:
//    static CoverageObject * Instance();
    virtual             ~HwTextureCoverageObject();
                        HwTextureCoverageObject();

private:
    virtual string      ComponentEnumToString(UINT32 component);
    // Non-copyable
                        HwTextureCoverageObject(const HwTextureCoverageObject&) {}
                        HwTextureCoverageObject & operator=(const HwTextureCoverageObject*)
                            {return *this;}
    virtual RC          Initialize();

//    static HwTextureCoverageObject * s_Instance;
};

//--------------------------------------------------------------------------------------------------
// Coverage object to track OpenGL extensions supported in mods.
class SwOpenGLExtCoverageObject: public CoverageObject
{
public:
    static CoverageObject * Instance();
    virtual             ~SwOpenGLExtCoverageObject();
                        SwOpenGLExtCoverageObject();

private:
    virtual string      ComponentEnumToString(UINT32 component);
    // Non-copyable
                        SwOpenGLExtCoverageObject(const SwOpenGLExtCoverageObject&){}
                        SwOpenGLExtCoverageObject & operator=(const SwOpenGLExtCoverageObject*)
                        { return *this;}
    virtual RC          Initialize();

//   static SwOpenGLExtCoverageObject * s_Instance;
   vector<string>    m_Ext;
};

}// namespace TestCoverage

