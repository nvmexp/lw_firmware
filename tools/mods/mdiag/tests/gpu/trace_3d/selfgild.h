/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef SELFGILD_H
#define SELFGILD_H

#include <map>
#include <string>
#include <list>
#include "mdiag/utils/types.h"
#include "mdiag/utils/raster_types.h"

/*----------------------------------------------------------------------
 * Strategy is an algorithm which is used to test if produced image is
 * correct. This is a new approach used instead of an old one, where gold
 * CRC is used. Each strategy requires support in FakeGL and in MODS.
 * Here is a description of how to create a new strategy in MODS.
 *
 * To create a new strategy:
 * - derive class from SelfgildStrategy and override virtual Gild() function.
 *   Put your gilding algorithm there.
 * - add your new strategy to StrategyFactory() function. That's how MODS
 *   knows what strategy to create depending on what string is found in
 *   Selfgild file.
 * - if you need to pass more parameters from from FakeGL to strategy in
 *   MODS, create data members for them in your class and pass their values
 *   in constructor. Use string name of extracted parameter to extract it
 *   from Selfgild file, i.e. state->GetUINT32("Color Tolerance").
 *
 * In your strategy you can create Pixel object. It represents colors of the
 * pixel. Do not use its components in strategy directly (since you don't
 * what format it is). You can do two things with Pixel object:
 * 1. Compare if colors of two pixels are equal exactly (to compare with
 *    tolerance use LW50Raster::ComparePixels())
 * 2. Use them as a key value in maps (i.e. create color histograms).
 *
 * Image analysis should be done using interface of LW50Raster. That ensures
 * your strategy works with any of supported formats.
 * ----------------------------------------------------------------------
 */

class ArgReader;

class SelfgildStrategy
{
public:
    SelfgildStrategy(const string& name) : m_name(name) {}
    virtual bool Gild(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size) const = 0;
    virtual bool CompatibleMode(const ArgReader *params) const {return true;}
    virtual ~SelfgildStrategy(){}

    static bool IsImageBlack(LW50Raster* raster, UINT32 width, UINT32 height, UINT32 depth, UINT32 array_size);

    const string& Name() const {return m_name;};

protected:
    string m_name;
};

class SelfgildState
{
public:

    SelfgildState();
    bool AddParam(const string& p);

    SelfgildStrategy* GetSelfgildStrategy(const ArgReader* run_params);

    friend SelfgildStrategy* StrategyFactory(const SelfgildState* state, const string&, const ArgReader* params);
    typedef list<RGBAFloat> ColorList;
    typedef list<ColorList> MultipleColorList;
    typedef list<UINT32> RequiredColorsList;
protected:

    UINT32              GetUINT32     (const string& name) const;
    string              GetString   (const string& name) const;
    ColorList           GetColors   (const string& name) const;
    MultipleColorList   GetMultipleColors (const string& name) const;
    RequiredColorsList  GetRequiredColors (const string& name) const;
    //float             GetFloat    (const string& name) const;

private:
    ColorList GetColorList (const string& line, bool ignoreLeadingNumber = false) const;
    bool ValidateVersion(const string& str) const;

    typedef map<string, list<string> >  ValueMap;

    bool m_version;
    const string m_keyDelims;
    const string m_valueDelims;
    ValueMap m_params;
    string m_lwrrentKey;
};

SelfgildStrategy* StrategyFactory(const SelfgildState* state, const string&, const ArgReader* params);

#endif /* SELFGILD_H */

