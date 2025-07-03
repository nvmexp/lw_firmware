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

#ifndef XML_DUMP_H
#define XML_DUMP_H

#ifdef _WIN32
#pragma warning(disable:4786)
#endif

#ifndef XML_GROUP
#define XML_GROUP "Undefined"
#endif

#define XML

class XMLDump
{
protected:

    enum integer_output_type
    { decimal, mixed, hex }                                 // format for outputting integers
      integer_output_mode;

public:

    enum cachedSelectionState                               // 32 bit word with info about whether this call
    {   untouched = 1,                                      //  should result in output.  This is initial value
        neverOutput = 0,                                    //  don't output
        output = 2,
        outputIfFrameActive = 0x7ffffff                     // force to 32 bits; this means group is selected for output
    };

    // visible methods

    XMLDump() : integer_output_mode(mixed) {}

    virtual ~XMLDump() = 0;                                    // make destructor virtual

    virtual void endAttributes() = 0;                       // end the attributes section of a non-inline element
    virtual void indentLine() = 0;                            // indent the current line by indent count
    virtual void newLine() = 0;                                // start a new output line
    virtual void outputAttribute(const char *, int) = 0;
    virtual void outputAttribute(const char *, unsigned int) = 0;
    virtual void outputAttribute(const char *, long) = 0;
    virtual void outputAttribute(const char *, UINT64) = 0;
                                                            // outputs an integer attribute
    virtual void outputAttribute(const char *, const char *) = 0;
    virtual void outputAttribute(const char *, int, bool) = 0;
                                                            // outputs an integer attribute in hex, bool ignored
    virtual void outputChars(const char *, int) = 0;
    virtual void outputChars(const char *) = 0;
                                                            // output, if current line would overflow, start a new line

    void setDecimalIntegerOutputMode()  { integer_output_mode = decimal; }
    void setHexIntegerOutputMode()      { integer_output_mode = hex; }
    void setMixedIntegerOutputMode()    { integer_output_mode = mixed; }

    virtual bool XMLDumping(const char *) = 0;                // test dumping by class.item
    virtual bool XMLDumping(cachedSelectionState&, const char *) = 0;
                                                            // fast/smart version
    virtual void XMLEndLwrrent() = 0;                        // end output of the current item
    virtual void XMLStartElement(const char *) = 0;         // start a non-inline element - attributes comine
    virtual void XMLStartStdInline(const char *) = 0;       // start a standard non-inline dump
    virtual void XMLStartStdNonInline(const char *) = 0;    // start a standard inline dump, which includes simclk
};

extern "C" bool XMLCheckDumpingEnabled(void);                  // check if XML sumping is enabled
#define XNAT(n,v)               XD->outputAttribute(n, v);
#define XAT(n,v)                XD->outputAttribute(#n, v);
#define XATI(n,v)               XD->outputAttribute(#n, ToInt(v));
#define XA(n)                   XD->outputAttribute(#n, n);
#define XATTR(n,v)              XD->outputAttribute(#n, v);
#define XCS(n)                  XMLDump::n
#define XCSNO                   XCS(neverOutput)
#define XCSO                    XCS(output)
#define XCV                     { static XMLDump::cachedSelectionState cacheState = XMLDump::untouched;
#define XFN(gr, nm)             gr "." #nm
#define XT(gr, nm)              if (XDNN && (cacheState != XCSNO) && ((cacheState == XCSO) || XDM(cacheState, XFN(gr, nm)))) { XSI(#nm);
#define XDM                     XD->XMLDumping
#define XDECIMAL                XD->setDecimalIntegerOutputMode();
#define XDNN                    (XD != NULL)
#define XEE                     XD->XMLEndLwrrent();
#define XEND                    XEE } }
#define XEEND                   XEE } }
#define XHEX                    XD->setHexIntegerOutputMode();
#define XINL(group, n)          XCV XT(#group, n)
#define XINLINE(n)              XCV XT(XML_GROUP, n)
#define XMIXED                  XD->setMixedIntegerOutputMode();
#define XSI                     XD->XMLStartStdInline
#define XSTDINLINE(n)           XCV XT("Standard", n)
#define XELEM(n)                XCV XT(XML_GROUP, n)
#define XSTDELEM(n)             XCV XT("Standard", n)
#define XESTART(cl, n)          XCV XT(#cl, n)

#endif
