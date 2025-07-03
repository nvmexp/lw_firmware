/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// XMLDump.h
//
// WARNING:
//
// The base XMLDump class defines the interface that is used for writing XML
// in moth mdiag and the cmodel. The base class does nothing and must be
// subclassed to provide actual writing of XML data. Due to problems in our
// build system and because of compatibility with older chips the definition
// of XMLDump should not be modified in any way.
//
// Due to further limitations and the structure of our build environment this
// file has been copied to ///hw/lwdiag/multidiag/ifspec. These two versions
// MUST be kept in sync.

#ifndef XML_WRITER_H
#define XML_WRITER_H

#ifdef _WIN32
#pragma warning(disable:4786)
#endif

#include <algorithm>
#include <set>
#include <list>
#include <string>
#include "XMLDump.h"

#ifndef XML_GROUP
#define XML_GROUP "Undefined"
#endif

class XMLWriter: public XMLDump
{
protected:

    int                     *nowAddr;                      // cmodel addr of now
    int                     *frameActiveAddr;              // is frame active
    bool                    forceFlush;
    bool                    initializedSuccessfully;

public:
    typedef std::list<std::string>    stringList; // rename to xmlGroups

    bool                    xmlGroupsAreIncludes;          // specifier list is includes, not excludes, xmlGroupsAreIncludes
    stringList              xmlGroups;

    // visible methods

    XMLWriter() :
        nowAddr(NULL),
        frameActiveAddr(NULL),
        forceFlush(false),
        initializedSuccessfully(false),
        xmlGroupsAreIncludes(false)
    {}

    virtual ~XMLWriter(){}

    virtual void endAttributes(){}                          // end the attributes section of a non-inline element
    virtual void indentLine(){}                             // indent the current line by indent count
    virtual void newLine(){}                                // start a new output line
    virtual void outputAttribute(const char *, int){}
                                                            // outputs an integer attribute
    virtual void outputAttribute(const char *, const char *){}
    virtual void outputAttribute(const char *, int, bool){}
                                                            // outputs an integer attribute in hex, bool ignored
    virtual void outputChars(const char *, int){}
    virtual void outputChars(const char *){}
                                                            // output, if current line would overflow, start a new line
    void setClock(int *now)             { nowAddr = now; }
    void setFrameActiveAddr(int *active){ frameActiveAddr = active; }
    void setGroupsAreIncludes(bool inc) { xmlGroupsAreIncludes = true; }
    void addGroup(const char *name)     { xmlGroups.push_back(name); }

    void addGroups(stringList &specifier_list) {
        for ( stringList::iterator ig = specifier_list.begin(); ig != specifier_list.end(); ++ig ) {
            xmlGroups.push_back(*ig);
        }
    }

    // enable XML output to specified file (could be "stdout"), return true if successful otherwise false.
    virtual bool XMLOpen(const char *fName, bool compressed_output = false){return false;}
    virtual bool XMLDumping(const char *){return false;} // test dumping by class.item
    virtual bool XMLDumping(cachedSelectionState&, const char *) {return false;} // fast/smart version
    virtual void XMLEndLwrrent(){}                        // end output of the current item
    virtual void XMLStartElement(const char *){}          // start a non-inline element - attributes comine
    virtual void XMLStartStdInline(const char *){}       // start a standard non-inline dump
    virtual void XMLStartStdNonInline(const char *){}    // start a standard inline dump, which includes simclk

    static XMLWriter *newWriter(void);
};

#endif
