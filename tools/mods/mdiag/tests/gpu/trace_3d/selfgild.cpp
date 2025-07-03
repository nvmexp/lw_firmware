/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <string>
#include <map>
#include <set>
#include "selfgild.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/raster_types.h"
#include "core/include/cmdline.h"
#include "teegroups.h"
#include "core/include/massert.h"
#include "core/include/utility.h"

#define MSGID() T3D_MSGID(Gild)

/*------------------------------------------------------------------------
 *  SelfgildState
 *------------------------------------------------------------------------
 */
SelfgildState::SelfgildState() : m_version(false), m_keyDelims(":"), m_valueDelims(" \t")
{
}

//expected input:
//  parameter name : param_value
//shall we allow only alpha-numerical symbols in a parameter name?
bool SelfgildState::AddParam(const string& str)
{
    if (!m_version)
    {
        //the first line should contain "Version 0.2"... maybe need to get revisited
        m_version = true;
        return ValidateVersion(str);
    }
    else
    {
        vector<string> tokens;
        if (Utility::Tokenizer(str, m_keyDelims, &tokens) != OK)
        {
            return false;
        }

        if (tokens.empty())
        {
            return false;
        }
        else if (tokens.size() == 1)
        {
            // if ":" is not be found, it is a continuation line
            if (m_lwrrentKey.empty())
            {
                ErrPrintf("Malformed syntax in selfgild.txt!");
                return false;
            }
            m_params[m_lwrrentKey].push_back(tokens[0]);
        }
        else
        {
            if(m_params.count(tokens[0]) != 0)
            {
                ErrPrintf("Duplicated key in selfgild.txt!");
                return false;
            }
            if (tokens.size() > 2)
            {
                WarnPrintf("More than one \":\" oclwrred in one line of selfgild.txt!");
            }

            vector<char> key(tokens[0].begin(), tokens[0].end());
            key.push_back('\0');
            m_lwrrentKey = Utility::RemoveHeadTailWhitespace(&key[0]);
            m_params[m_lwrrentKey].push_back(tokens[1]);
        }

        return true;
    }
}

bool SelfgildState::ValidateVersion(const string& str) const
{
    if (str.find("Version 0.2") == string::npos)
    {
        return false;
    }
    else
    {
        return true;
    }
}

SelfgildStrategy* SelfgildState::GetSelfgildStrategy(const ArgReader* params)
{
    ValueMap::const_iterator i = m_params.find("Strategy");
    if (i != m_params.end())
    {
        const list<string>& strategy = i->second;
        MASSERT((strategy.size() == 1) && "More than one Strategy!");
        SelfgildStrategy *ret = StrategyFactory(this, strategy.front(), params);
        if (ret && ret->CompatibleMode(params))
            return ret;
    }
    return 0;
}

string SelfgildState::GetString (const string& name) const
{
    ValueMap::const_iterator i = m_params.find(name);
    if (i != m_params.end())
    {
        const list<string>& values = i->second;
        MASSERT((values.size() == 1) && ("More than one line!"));
        return values.front();
    }
    return string("");
}

UINT32 SelfgildState::GetUINT32 (const string& name) const
{
    ValueMap::const_iterator i = m_params.find(name);
    if (i != m_params.end())
    {
        const list<string>& values = i->second;
        MASSERT((values.size() == 1) && ("More than one line!"));
        return strtoul(values.front().c_str(), 0, 0);
    }
    return 0;
}

SelfgildState::ColorList SelfgildState::GetColors (const string& name) const
{
    ValueMap::const_iterator i = m_params.find(name);
    if (i != m_params.end())
    {
        const list<string>& colors = i->second;
        MASSERT((colors.size() == 1) && ("More than one line which contains colors!"));
        return GetColorList(colors.front());
    }
    return ColorList();
}

SelfgildState::MultipleColorList SelfgildState::GetMultipleColors (const string& name) const
{
    MultipleColorList ret;

    ValueMap::const_iterator i = m_params.find(name);
    if (i != m_params.end())
    {
        const list<string>& multipleColorList = i->second;
        for (list<string>::const_iterator j = multipleColorList.begin();
             j != multipleColorList.end(); ++j)
        {
            ret.push_back(GetColorList(*j, true));
        }
    }
    return ret;
}

SelfgildState::ColorList SelfgildState::GetColorList (
    const string& line,
    bool ignoreLeadingNumber /*
                              * = false (by default, all the tokens are parts of Color)
                              * = true  (means that first token is number of Color)
                              */
    ) const
{
    ColorList ret;
    UINT32 colors[RGBAFloat::NUM_COLORS];
    UINT32 i = 0, j = 0;
#ifdef DEBUG
    UINT32 colorNumber = 0;
#endif
    vector<string> tokens;

    if (Utility::Tokenizer(line, m_valueDelims, &tokens) != OK) {
        MASSERT(!"Failed to parse colors!");
    }

    if (ignoreLeadingNumber)
    {
#ifdef DEBUG
        colorNumber = strtoul(tokens[0].c_str(), 0, 0);
#endif
        ++i;
    }

    for (; i < tokens.size(); ++i)
    {
        colors[j++] = strtoul(tokens[i].c_str(), 0, 0);
        if (j == RGBAFloat::NUM_COLORS)
        {
            ret.push_back(RGBAFloat(colors));
            j = 0;
        }
    }

    MASSERT(!j && "Not a valid RGBA color format!");
    MASSERT((!ignoreLeadingNumber || (ret.size() == colorNumber)) && "Number of color is not correct!");

    return ret;
}

SelfgildState::RequiredColorsList SelfgildState::GetRequiredColors (const string& name) const
{
    RequiredColorsList ret;
    ValueMap::const_iterator i = m_params.find(name);
    if (i != m_params.end())
    {
        const list<string>& lines = i->second;
        MASSERT((lines.size() == 1) && ("Required colors format is not correct!"));
        vector<string> tokens;
        if (Utility::Tokenizer(lines.front(), m_valueDelims, &tokens) != OK)
        {
            MASSERT(!"Failed to parse required colors!");
        }
        for (vector<string>::iterator j = tokens.begin(); j != tokens.end(); ++j)
        {
            ret.push_back(strtoul(j->c_str(), 0, 0));
        }
    }
    return ret;
}

/*
FP32 SelfgildState::GetFloat (const string& name) const
{
    return 0;
}
*/

/*------------------------------------------------------------------------
 *  SelfgildStrategy
 *------------------------------------------------------------------------
 */

bool SelfgildStrategy::IsImageBlack(LW50Raster* raster, UINT32 width, UINT32 height,
    UINT32 depth, UINT32 array_size)
{
    //make sure thre is at least one non-black pixel in the image
    if (raster->ZFormat())
        return false;

    UINT32 bl_base = 0;
    for (UINT32 a = 0; a < array_size; ++a)
        for (UINT32 d = 0; d < depth; ++d)
        {
            for (UINT32 j = 0; j < height; ++j)
                for (UINT32 i = 0; i < width; ++i)
                {
                    if (!raster->IsBlack(bl_base + i + j*width))
                        return false;
                }
            bl_base += width*height;
        }

    return true;
}

/*------------------------------------------------------------------------
 *  SelfgildMatchTriangles
 *------------------------------------------------------------------------
 */

class SelfgildMatchTriangles : public SelfgildStrategy
{
public:
    SelfgildMatchTriangles(const string& name, UINT32 threshold, UINT32 triangles) :
        SelfgildStrategy(name), m_threshold(&threshold), m_num_triangles(triangles) {}
    virtual bool Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const;
    virtual bool CompatibleMode(const ArgReader *params) const;

private:
    bool GildOneLayer(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 bl_base) const;
    bool checkTriangleCorners(UINT32 x1, UINT32 y1, UINT32 x2, UINT32 y2, UINT32 width,
        bool checkBlack, PixelFB bgColor, UINT32 bl_base, LW50Raster* r) const;

    FloatArray<1> m_threshold;
    UINT32 m_num_triangles;
};

//this strategy is incompatible with any AA mode
bool SelfgildMatchTriangles::CompatibleMode(const ArgReader *params) const
{
    if ( params->ParamPresent("-aamode"))
        return false;
    else
        return true;
}

bool SelfgildMatchTriangles::Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const
{
    if (raster->ZFormat())
    //this strategy does not support Z directly to test Z test should be
    //written such that Z is reflected and gildable as color
    {
        InfoPrintf("Selfgild matching triangles strategy is not supported for ZFormat, this test will pass by default\n");
        return true;
    }

    UINT32 bl_base = 0;

    // Iterate through all layers
    for (UINT32 a = 0; a < array_size; ++a)
        for (UINT32 d = 0; d < depth; ++d)
        {
            if (!GildOneLayer(raster, width, height, bl_base))
                return false;
            bl_base += width*height;
        }
    return true;
}

bool SelfgildMatchTriangles::GildOneLayer(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 bl_base) const
{
    UINT32 numTriangles = 0;

    // Callwlate the background color. If two edges are all the same color, that's the background color.
    // If the condition is not met, default to black.
    //
    //             0
    //    +-------------------+
    // 3  |                   | 1
    //    |                   |
    //    +-------------------+
    //              2
    PixelFB edgeColor[4];
    bool    edgeConst[4];

    edgeConst[0] = edgeConst[2] = true;
    edgeColor[0] = raster->GetPixel(bl_base);
    edgeColor[2] = raster->GetPixel(bl_base + (height - 1) * width);
    for (UINT32 x = 1; x < width; x++)
    {
        if(edgeConst[0] && !(raster->GetPixel(bl_base + x)                        == edgeColor[0]))
            edgeConst[0] = false;
        if(edgeConst[2] && !(raster->GetPixel(bl_base + (height - 1) * width + x) == edgeColor[2]))
            edgeConst[2] = false;
    }

    edgeConst[3] = edgeConst[1] = true;
    edgeColor[3] = raster->GetPixel(bl_base);
    edgeColor[1] = raster->GetPixel(bl_base + width - 1);
    for (UINT32 y = 0; y < height; y++)
    {
        if(edgeConst[3] && !(raster->GetPixel(bl_base + width * y)             == edgeColor[3]))
            edgeConst[3] = 0;
        if(edgeConst[1] && !(raster->GetPixel(bl_base + width * y + width - 1) == edgeColor[1]))
            edgeConst[1] = 0;
    }

    // If two edges are constant color and match, that's the background color. Otherwise
    // default to checking for black.
    PixelFB bgColor(0, 0, 0, 0);
    bool checkBlack = true;

    for (int i = 0; i < 4 && checkBlack; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if(i == j) continue;
            if(!edgeConst[i] || !edgeConst[j]) continue;

            if(edgeColor[i] == edgeColor[j])
            {
                bgColor = edgeColor[i];
                checkBlack = false;
                break;
            }
        }
    }

    if(bgColor.Red() == 0 && bgColor.Green() == 0 && bgColor.Blue() == 0 && bgColor.Alpha() == 0)
        checkBlack = true;

    InfoPrintf("Selfgild matching triangles strategy: default black background = %d, backgroundColor = (%08X %08X %08X %08X)\n",
        checkBlack, bgColor.Red(), bgColor.Green(), bgColor.Blue(), bgColor.Alpha());

    for (UINT32 x = 0, y = 0, lwrrentRow = 0, ty = 0; y < height; y = lwrrentRow, ++ty)
    {
        // Keep moving until we get a row that's not all blank
        for (; y < height; ++y)
        {
            for (x = 0; x < width; ++x)
            {
                if (checkBlack ? (!raster->IsBlack(bl_base + x + y*width)) : (!(raster->GetPixel(bl_base + x + y*width) == bgColor)))
                    break;
            }
            if (x < width)
                break;
        }

        // Save the location
        UINT32 top = y;

        // Keep moving until we get a row that is all blank
        for (; y < height; ++y)
        {
            for (x = 0; x < width; ++x)
            {
                if (checkBlack ? (!raster->IsBlack(bl_base + x + y*width)) : (!(raster->GetPixel(bl_base + x + y*width) == bgColor)))
                    break;
            }
            if (x == width)
                break;
        }

        // Save the location minus one
        UINT32 bottom = y - 1;

        // If we just ran off the edge, that doesn't count
        if(bottom <= top)
            break;

        // Save the current value of y (will be restored at start of next iteration)
        lwrrentRow = y;

        // We now know a row of triangles is contained in y = [top, bottom].
        // Now move across horizontally to isolate individual triangles

        for (UINT32 tx = 0, x = 0; x < width; ++tx)
        {
            // Keep moving until we get a column that's not all black
            for (; x < width; ++x)
            {
                for (y = top; y <= bottom; ++y)
                {
                    if (checkBlack ? (!raster->IsBlack(bl_base + x + y*width)) : (!(raster->GetPixel(bl_base + x + y*width) == bgColor)))
                        break;
                }
                if (y <= bottom)
                    break;
            }

            // Save the location
            UINT32 left = x;

            // Keep moving until we get a column that is all black
            for (; x < width; ++x)
            {
                for (y = top; y <= bottom; ++y)
                {
                    if (checkBlack ? (!raster->IsBlack(bl_base + x + y*width)) : (!(raster->GetPixel(bl_base + x + y*width) == bgColor)))
                        break;
                }
                if (y > bottom)
                    break;
            }

            // Save the location minus one
            UINT32 right = x - 1;

            // If we just got to the side, that doesn't count
            if(right <= left)
                break;

            numTriangles++;

            // We now have a pair of triangles that occupy (left, top) - (right, bottom).
            // We want to make sure that the two triangles are solid colors and within
            // one LSB of each other. (Or some other threshold.) We compute a histogram
            // of the colors in the region and make sure that it contains at most two
            // colors, and those colors are within the threshold.
            // Note we're only checking the color image.

            DebugPrintf(MSGID(), "Triangle #%d: (%d, %d)-(%d, %d)\n ",
                    numTriangles, left, top, right, bottom);

            if(!checkTriangleCorners(left, top, right, bottom, width, checkBlack, bgColor, bl_base, raster))
            {
                ErrPrintf("Centroid too far from center of bounding rectangle, ");
                RawPrintf("probably missing one of the two triangles\n");
                ErrPrintf("Can't find two triangles in (%d, %d) (%d, %d)-(%d, %d)\n",
                        tx, ty, left, top, right, bottom);

                return false;
            }

            DebugPrintf(MSGID(), "Triangle #%d\n",numTriangles);
            for (y = top; y <= bottom; ++y)
            {
                for (x = left; x <= right; ++x)
                {
                    UINT32 xm=right+left-x;
                    UINT32 ym=top+bottom-y;
                    UINT32 offset1 = bl_base + x + y*width;
                    UINT32 offset2 = bl_base + xm + ym*width;
                    if (!(raster->ComparePixels(offset1,offset2 , m_threshold)))
                    {
                        PixelFB pix1=raster->GetPixel(offset1);
                        PixelFB pix2=raster->GetPixel(offset2);
                        ErrPrintf("Triangle pair (%d, %d) with upper left corner at pixel (%d, %d) and lower right corner at pixel (%d, %d) fails SELFGILD_MATCHING_TRIANGLES.\n", tx, ty, left, top, right, bottom);
                        ErrPrintf("Pixels at (%d,%d) and (%d,%d) differ by more than the threshold of %f.\n", x, y, xm,ym, m_threshold.getFloat32(0));
                        InfoPrintf("Pixel1: (%08X %08X %08X %08X)\n", pix1.Red(), pix1.Green(), pix1.Blue(), pix1.Alpha());
                        InfoPrintf("Pixel2: (%08X %08X %08X %08X)\n", pix2.Red(), pix2.Green(), pix2.Blue(), pix2.Alpha());
                        return false;
                    }
                }
            }

        }
        DebugPrintf(MSGID(), "\n");
    }

    if (numTriangles != m_num_triangles)
    {
        ErrPrintf("Found %d non-background color triangles, expecting %d. Test fails\n",
                   numTriangles, m_num_triangles);
        return false;
    }

    InfoPrintf("Processed %d triangles - all passed. Expected %d triangles.\n",
                 numTriangles, m_num_triangles);
    return true;
}

bool SelfgildMatchTriangles::checkTriangleCorners(UINT32 x1, UINT32 y1, UINT32 x2, UINT32 y2, UINT32 width,
    bool checkBlack, PixelFB bgColor, UINT32 bl_base, LW50Raster* r) const
{
    UINT32 offset;

    // Compute the centroid of the region
    UINT32 x, y, ctr;
    double xs, ys;

    ctr = 0;
    xs = 0.0; ys = 0.0;

    for(y = y1; y <= y2; ++y)
    {
        for(x = x1; x <= x2; ++x)
        {
            offset = bl_base + x + y * width;

            if (checkBlack ? (!r->IsBlack(offset)) : (!(r->GetPixel(offset) == bgColor)))
            {
                ctr++;

                xs += (float)x;
                ys += (float)y;
            }
        }
    }
    xs = xs / (float)ctr;
    ys = ys / (float)ctr;

    xs = (xs - (float)x1) * (100.0 / (float)(x2 - x1));
    ys = (ys - (float)y1) * (100.0 / (float)(y2 - y1));

    DebugPrintf(MSGID(), " centroid at (%3.3f, %3.3f)\n", xs, ys);

    // If the centroid is too far off, odds are we're missing one of the triangle halves.
    if(xs > 40.0 && xs < 60.0 && ys > 40.0 && xs < 60.0) {
        return true;
    }
    else {
        return false;
    }
}

/*------------------------------------------------------------------------
 *  SelfgildByColor
 *------------------------------------------------------------------------
 */

class SelfgildByColor : public SelfgildStrategy
{
public:
    typedef SelfgildState::ColorList ColorList;

    SelfgildByColor(const string& name, UINT32 threshold, SelfgildState::ColorList colors) :
        SelfgildStrategy(name), m_threshold(&threshold), m_colors(colors) {}
    virtual bool Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const;

private:
    FloatArray<1> m_threshold;
    ColorList m_colors;
};

bool SelfgildByColor::Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const
{
    if (raster->ZFormat())
    {
       InfoPrintf("Selfgild by color strategy is not supported for ZFormat, this test will pass by default\n");
        return true;
    }

    typedef map<PixelFB, UINT32> Colors;
    Colors colors;

    // Get a list of all unique colors in the image
    for (UINT32 a = 0; a < array_size; ++a)
        for (UINT32 d = 0; d < depth; ++d)
        {
            UINT32 bl_base = 0;
            for (UINT32 j = 0; j < height; ++j)
                for (UINT32 i = 0; i < width; ++i)
                {
                    UINT32 offset = bl_base + i + j*width;
                    colors[raster->GetPixel(offset)] = offset;
                }
            bl_base += width*height;
        }

    // Make sure they contain the same elements (within tolerance)
    // Doing this in two passes to show a miningful error
    //
    // Pass 1. Is any found pixel really specified by the test?
    for(Colors::const_iterator i = colors.begin(); i != colors.end(); ++i)
    {
        bool match = false; // Assume no match
        for (ColorList::const_iterator j = m_colors.begin(); j != m_colors.end(); ++j)
        {
            if (raster->ComparePixelsRGBA(i->second, *j, m_threshold))
                match = true;
        }
        if (!match)
        {
            ErrPrintf("Color {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d,%d) is not specified by selfgild\n",
                      i->first.Red(),
                      i->first.Green(),
                      i->first.Blue(),
                      i->first.Alpha(),
                      i->second%width, i->second/width);
            ErrPrintf("Only the following colors with tolerence %10.6f(0x%08x) should be in produced image:\n",
                       m_threshold.getFloat32(0), m_threshold.getRaw(0));
            for (ColorList::const_iterator k = m_colors.begin(); k != m_colors.end(); ++k)
                RawPrintf("\t {%12.5e(0x%08x),%12.5e(0x%08x),%12.5e(0x%08x),%12.5e(0x%08x)}\n",
                          k->GetRFloat32f(), k->GetRFloat32(),
                          k->GetGFloat32f(), k->GetGFloat32(),
                          k->GetBFloat32f(), k->GetBFloat32(),
                          k->GetAFloat32f(), k->GetAFloat32());
            return false;
        }
    }

    //Pass 2. Are all specifed colors found in the image?
    for(ColorList::const_iterator i = m_colors.begin(); i != m_colors.end(); ++i)
    {
        bool match = false; // Assume no match
        for (Colors::const_iterator j = colors.begin(); j != colors.end(); ++j)
        {
            if (raster->ComparePixelsRGBA(j->second, *i, m_threshold))
                match = true;
        }
        if (!match)
        {
            ErrPrintf("Image does not contain color {%12.5e(0x%08x), %12.5e(0x%08x), %12.5e(0x%08x), %12.5e(0x%08x)} with tolerence %f\n",
                      i->GetRFloat32f(), i->GetRFloat32(),
                      i->GetGFloat32f(), i->GetGFloat32(),
                      i->GetBFloat32f(), i->GetBFloat32(),
                      i->GetAFloat32f(), i->GetAFloat32(),
                      m_threshold.getFloat32(0));

            return false;
        }
    }

    return true;
}

/*------------------------------------------------------------------------
 *  SelfgildByColorset
 *------------------------------------------------------------------------
 */

class SelfgildByColorset : public SelfgildStrategy
{
public:
    typedef SelfgildState::ColorList ColorList;
    typedef SelfgildState::MultipleColorList MultipleColorList;
    typedef SelfgildState::RequiredColorsList RequiredColorsList;

    SelfgildByColorset(const string& name, UINT32 threshold,
        SelfgildState::MultipleColorList colorset, SelfgildState::RequiredColorsList requiredColorsList) :
        SelfgildStrategy(name), m_threshold(&threshold), m_colorset(colorset),
        m_requiredColorsList(requiredColorsList) {}
    virtual bool Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const;

private:
    FloatArray<1> m_threshold;
    MultipleColorList m_colorset;
    RequiredColorsList m_requiredColorsList;
};

bool SelfgildByColorset::Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const
{
    if (raster->ZFormat())
    {
        InfoPrintf("Selfgild by colorset strategy is not supported for ZFormat, this test will pass by default\n");
        return true;
    }

    MASSERT(m_requiredColorsList.size() == m_colorset.size());

    typedef map<PixelFB, UINT32> Colors;
    Colors colors;

    // Get a list of all unique colors in the image
    for (UINT32 a = 0; a < array_size; ++a)
        for (UINT32 d = 0; d < depth; ++d)
        {
            UINT32 bl_base = 0;
            for (UINT32 j = 0; j < height; ++j)
                for (UINT32 i = 0; i < width; ++i)
                {
                    UINT32 offset = bl_base + i + j*width;
                    colors[raster->GetPixel(offset)] = offset;
                }
            bl_base += width*height;
        }

    // Make sure they contain the necessary elements (within tolerance)
    // Different from SELFGILD_MULTI_COLOR, not all the elements defined must be present
    // Instead only required number of colors are necessary. Seeing details in bug 1558886.
    // Doing this in two passes to show a miningful error
    //
    // Pass 1. Is any found pixel really specified by the test?
    for(Colors::const_iterator i = colors.begin(); i != colors.end(); ++i)
    {
        bool match = false; // Assume no match
        for (MultipleColorList::const_iterator j = m_colorset.begin(); j != m_colorset.end(); ++j)
        {
            for (ColorList::const_iterator k = j->begin(); k != j->end(); ++k)
            {
                if (raster->ComparePixelsRGBA(i->second, *k, m_threshold))
                {
                    match = true;
                }
            }
        }
        if (!match)
        {
            ErrPrintf("Color {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d,%d) is not specified by selfgild\n",
                      i->first.Red(),
                      i->first.Green(),
                      i->first.Blue(),
                      i->first.Alpha(),
                      i->second%width, i->second/width);
            ErrPrintf("Only the following colors with tolerance %10.6f(0x%08x) should be in produced image:\n",
                       m_threshold.getFloat32(0), m_threshold.getRaw(0));
            for (MultipleColorList::const_iterator j = m_colorset.begin(); j != m_colorset.end(); ++j)
            {
                for (ColorList::const_iterator k = j->begin(); k != j->end(); ++k)
                {
                    RawPrintf("\t {%12.5e(0x%08x),%12.5e(0x%08x),%12.5e(0x%08x),%12.5e(0x%08x)}\n",
                              k->GetRFloat32f(), k->GetRFloat32(),
                              k->GetGFloat32f(), k->GetGFloat32(),
                              k->GetBFloat32f(), k->GetBFloat32(),
                              k->GetAFloat32f(), k->GetAFloat32());
                }
            }
            return false;
        }
    }

    //Pass 2. Are all requred number of colors found in the image?
    RequiredColorsList::const_iterator requiredColorIter = m_requiredColorsList.begin();
    UINT32 setNumber = 1;
    for (MultipleColorList::const_iterator i = m_colorset.begin(); i != m_colorset.end(); ++i)
    {
        UINT32 matchedColor = 0;
        ColorList matchedColorList, unmatchedColorList;
        for (ColorList::const_iterator j = i->begin(); j != i->end(); ++j)
        {
            for (Colors::const_iterator k = colors.begin(); k != colors.end(); ++k)
            {
                if (raster->ComparePixelsRGBA(k->second, *j, m_threshold))
                {
                    ++matchedColor;
                    matchedColorList.push_back(*j);
                }
                else
                {
                    unmatchedColorList.push_back(*j);
                }
            }
        }

        if (*requiredColorIter != matchedColor)
        {
            ErrPrintf("(%d) Number of contained color in image does not number of (%d) number of required color in color set %d\n",
                      matchedColor, *requiredColorIter, setNumber);

            ErrPrintf("The following colors with tolerance %10.6f(0x%08x) are in produced image:\n",
                       m_threshold.getFloat32(0), m_threshold.getRaw(0));
            for (ColorList::const_iterator j = matchedColorList.begin(); j != matchedColorList.end(); ++j)
            {
                RawPrintf("\t {%12.5e(0x%08x),%12.5e(0x%08x),%12.5e(0x%08x),%12.5e(0x%08x)}\n",
                          j->GetRFloat32f(), j->GetRFloat32(),
                          j->GetGFloat32f(), j->GetGFloat32(),
                          j->GetBFloat32f(), j->GetBFloat32(),
                          j->GetAFloat32f(), j->GetAFloat32());
            }

            ErrPrintf("The following colors with tolerance %10.6f(0x%08x) are NOT in produced image:\n",
                       m_threshold.getFloat32(0), m_threshold.getRaw(0));
            for (ColorList::const_iterator j = unmatchedColorList.begin(); j != unmatchedColorList.end(); ++j)
            {
                RawPrintf("\t {%12.5e(0x%08x),%12.5e(0x%08x),%12.5e(0x%08x),%12.5e(0x%08x)}\n",
                          j->GetRFloat32f(), j->GetRFloat32(),
                          j->GetGFloat32f(), j->GetGFloat32(),
                          j->GetBFloat32f(), j->GetBFloat32(),
                          j->GetAFloat32f(), j->GetAFloat32());
            }

            return false;
        }

        ++requiredColorIter;
        ++setNumber;
    }

    return true;
}

/*------------------------------------------------------------------------
 *  SelfgildMatchingSides
 *------------------------------------------------------------------------
 */

class SelfgildMatchingSides : public SelfgildStrategy
{
public:
    SelfgildMatchingSides(const string& name, UINT32 threshold, bool mirrored) :
        SelfgildStrategy(name), m_threshold(&threshold), m_mirrored(mirrored) {}
    virtual bool Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const;

private:

    FloatArray<1> m_threshold;
    bool m_mirrored;
};

bool SelfgildMatchingSides::Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const
{
    bool    single_color = true;

    for (UINT32 a = 0; a < array_size; ++a)
        for (UINT32 d = 0; d < depth; ++d)
        {
            UINT32 bl_base = 0;
            for (UINT32 x = 0, xx = width/2, off1 = 0, off2 = 0; x < width/2; ++x, ++xx)
            {
                for (UINT32 y = 0; y < height; ++y)
                {
                    off1 = bl_base + x + y*width;
                    if (!m_mirrored)
                        off2 = bl_base + xx + y*width;
                    else
                        off2 = bl_base + (width - 1 - x) + y*width;

                    if (!raster->ComparePixels(off1, off2, m_threshold))
                    {
                        PixelFB p1 = raster->GetPixel(off1);
                        PixelFB p2 = raster->GetPixel(off2);
                        if (raster->ZFormat())
                        {
                            ErrPrintf("ZFormat error %s: pixel {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d, %d) doesn't match pixel {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d, %d) to within a difference of %f per component.\n",
                                m_mirrored ? "SelfgildMirrorSides" : "SelfgildMatchingSides",
                                p1.Red(), p1.Green(), p1.Blue(), p1.Alpha(),
                                x, y,
                                p2.Red(), p2.Green(), p2.Blue(), p2.Alpha(),
                                off2%width, y, m_threshold.getFloat32(0));
                        }
                        else
                        {
                            ErrPrintf("%s: pixel {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d, %d) doesn't match pixel {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d, %d) to within a difference of %f per component.\n",
                                m_mirrored ? "SelfgildMirrorSides" : "SelfgildMatchingSides",
                                p1.Red(), p1.Green(), p1.Blue(), p1.Alpha(),
                                x, y,
                                p2.Red(), p2.Green(), p2.Blue(), p2.Alpha(),
                                off2%width, y, m_threshold.getFloat32(0));
                        }
                        return false;
                    }
                    if( single_color && !raster->ZFormat() )
                    {
                        single_color = raster->ComparePixels( off1, 0, m_threshold );
                    }
                }
            }
            bl_base += width*height;
        }

    if( single_color && !raster->ZFormat() )
    {
        ErrPrintf("Color buffer in one color is not allowed for SelfgildMirrorSides or SelfgildMatchingSides\n");
        return false;
    }
    return true;
}

/*------------------------------------------------------------------------
 *  SelfgildMatchingVertSides
 *------------------------------------------------------------------------
 */

class SelfgildMatchingVertSides : public SelfgildStrategy
{
public:
    SelfgildMatchingVertSides(const string& name, UINT32 threshold, bool mirrored) :
        SelfgildStrategy(name), m_threshold(&threshold), m_mirrored(mirrored) {}
    virtual bool Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const;

private:

    FloatArray<1> m_threshold;
    bool m_mirrored;
};

bool SelfgildMatchingVertSides::Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const
{
    for (UINT32 a = 0; a < array_size; ++a)
        for (UINT32 d = 0; d < depth; ++d)
        {
            UINT32 bl_base = 0;
            for (UINT32 y = 0, yy = height/2, off1 = 0, off2 = 0; y < height/2; ++y, ++yy)
            {
                for (UINT32 x = 0; x < width; ++x)
                {
                    off1 = bl_base + x + y*width;
                    if (!m_mirrored)
                        off2 = bl_base + x + yy*width;
                    else
                        off2 = bl_base + x + (height - 1 - y) * width;

                    if (!raster->ComparePixels(off1, off2, m_threshold))
                    {
                        PixelFB p1 = raster->GetPixel(off1);
                        PixelFB p2 = raster->GetPixel(off2);
                        if (raster->ZFormat())
                        {
                            ErrPrintf("ZFormat error %s: pixel {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d, %d) doesn't match pixel {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d, %d) to within a difference of %f per component.\n",
                                m_mirrored ? "SelfgildMirrorVertSides" : "SelfgildMatchingVertSides",
                                p1.Red(), p1.Blue(), p1.Green(), p1.Alpha(),
                                x, y,
                                p2.Red(), p2.Blue(), p2.Green(), p2.Alpha(),
                                off2%width, off2/width, m_threshold.getFloat32(0));
                        }
                        else
                        {
                            ErrPrintf("%s: pixel {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d, %d) doesn't match pixel {0x%08x, 0x%08x, 0x%08x, 0x%08x} at (%d, %d) to within a difference of %f per component.\n",
                                m_mirrored ? "SelfgildMirrorVertSides" : "SelfgildMatchingVertSides",
                                p1.Red(), p1.Blue(), p1.Green(), p1.Alpha(),
                                x, y,
                                p2.Red(), p2.Blue(), p2.Green(), p2.Alpha(),
                                off2%width, off2/width, m_threshold.getFloat32(0));
                        }
                        return false;
                    }
                }
            }
            bl_base += width*height;
        }
    return true;
}

/*------------------------------------------------------------------------
 *  SelfgildLwstom
 *------------------------------------------------------------------------
 */

class SelfgildLwstom : public SelfgildStrategy
{
public:
    SelfgildLwstom(const string& name) : SelfgildStrategy(name) {}
    virtual bool Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const;
};

bool SelfgildLwstom::Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const
{
    ErrPrintf("MODS does not support custom strategy\n");
    return false;
}

SelfgildStrategy *StrategyFactory(const SelfgildState *state, const string &name, const ArgReader* params)
{
    UINT32 threshold = 0;
    if (state)
    {
        threshold = state->GetUINT32("Color Tolerance");
    }
    if ( params->ParamPresent("-selfgild_threshold"))
    {
        float thresholdFloat = params->ParamFloat("-selfgild_threshold");
        FloatArray<1> tempArray = FloatArray<1>(&thresholdFloat);
        threshold = tempArray.getRaw(0);
    }
    if (name.find("SELFGILD_MATCHING_TRIANGLES") != string::npos)
        return new SelfgildMatchTriangles(name,
                                          threshold,
                                          state->GetUINT32("intArg1"));
    // SELFGILD_MULTI_COLORSET must be placed before SELFGILD_MULTI_COLOR.
    // Otherwise SELFGILD_MULTI_COLOR will never be called.
    if (name.find("SELFGILD_MULTI_COLORSET") != string::npos)
        return new SelfgildByColorset(name,
                                      threshold,
                                      state->GetMultipleColors("Colorset"),
                                      state->GetRequiredColors("Colors Required"));
    if (name.find("SELFGILD_MULTI_COLOR")  != string::npos)
        return new SelfgildByColor(name,
                                   threshold,
                                   state->GetColors("Colors"));
    if (name.find("SELFGILD_MATCHING_SIDES") != string::npos)
        return new SelfgildMatchingSides(name,
                                         threshold,
                                         false);
    if (name.find("SELFGILD_MIRROR_SIDES") != string::npos)
        return new SelfgildMatchingSides(name,
                                         threshold,
                                         true);
    if (name.find("SELFGILD_MATCHING_VERT_SIDES") != string::npos)
        return new SelfgildMatchingVertSides(name,
                                         threshold,
                                         false);
    if (name.find("SELFGILD_MIRROR_VERT_SIDES") != string::npos)
        return new SelfgildMatchingVertSides(name,
                                         threshold,
                                         true);
    if (name.find("SELFGILD_LWSTOM_STRATEGY") != string::npos)
        return new SelfgildLwstom(name);

    ErrPrintf("Unrecognized selfgild strategy: %s\n", name.c_str());
    return NULL;
}

