/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2009, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifdef _WIN32
#pragma warning(disable:4786)
#define GZ_FILE_OPEN_MODE "wb"
#else
#define GZ_FILE_OPEN_MODE "w"
#endif
#define XML_GROUP "LibWriter"
#define XD NULL
#include "mdiag/utils/types.h"
#include "mdiag/utils/XMLDump.h"
#include "mdiag/utils/XMLWriter.h"
#include "core/include/zlib.h"

#include <stdio.h>

#ifndef INCLUDED_STL_MEMORY
#include <memory>
#define INCLUDED_STL_MEMORY
#endif

#define XML_TAB_SIZE 4

// NOTE:- XML_LINE_LENGTH is fairly large as some Perl scripts used to post-process XML
// output need each element on a separate line
//
#define XML_LINE_LENGTH 1024

// XMLFileWriter class -
//
// This is the XMLDump class which is passed to the cmodel

static const char    *lotsOfBlanks = "                                                       " ;

static int now = 0;
static int active = 1;

class XMLFileWriter: public XMLWriter
{

    private:

        // types of XML terminators

        enum XMLTerminatorType
        {
            XML_TERM_ATTRS,      // attributes inline terminated with />
            XML_TERM_BODY,       // term with </name>
            XML_TERM_XML,        // the <?xml> terminator
            XML_TERM_ATTRS_BODY  //attributes then normal term
        };

        class XMLTerminator
        {
            XMLTerminatorType m_TermType;
            const char* m_XmlElementName;

        public:
            XMLTerminator(XMLFileWriter* xdf, const char *ename, XMLTerminatorType tType)
            {
                m_TermType = tType;

                // if the element name begins with a "<", it's a flag saying always output
                //  this element; not part of name, wouldn't be legal anyway

                if (*ename == '<')
                    m_XmlElementName = ename + 1;
                else
                    m_XmlElementName = ename;

                // output to start the element

                switch (tType)
                {
                    case XML_TERM_XML:

                        xdf->outputChars("<?xml version=\"1.0\"?>");
//                      xdf->lwrrIndent = -XML_TAB_SIZE;
                        xdf->newLine();
                        break;

                    case XML_TERM_ATTRS_BODY:
                    case XML_TERM_ATTRS:

                        if (xdf->cOutputPosition > xdf->lwrrIndent)
                            xdf->newLine();
                        if (xdf->cOutputPosition == 0)
                            xdf->indentLine();
                        if( tType == XML_TERM_ATTRS_BODY )
                            xdf->lwrrIndent += XML_TAB_SIZE;
                        xdf->outputChars("<");
                        xdf->outputChars(m_XmlElementName);
                        break;

                    case XML_TERM_BODY:

                        xdf->lwrrIndent += XML_TAB_SIZE;
                        if (xdf->cOutputPosition > xdf->lwrrIndent)
                            xdf->newLine();
                        if (xdf->cOutputPosition == 0)
                            xdf->indentLine();
                        xdf->outputChars("<");
                        xdf->outputChars(m_XmlElementName);
                        xdf->outputChars(">");
                        xdf->newLine();
                        xdf->indentLine();
                        break;

                    default:

                        fprintf(stderr, "Unuspported XMLTerminatorType for XMLTerminator initialization\n");
                        exit(24);
                }
            }

            void Close(XMLFileWriter* xdf)
            {
                switch (m_TermType)
                {
                    case XML_TERM_ATTRS:

                        // don't start a new line for the terminator

//                      xdf->cOutputPosition -= 4;
                        xdf->outputChars(" />");
                        xdf->newLine();
                        break;

                    case XML_TERM_BODY:
                    case XML_TERM_ATTRS_BODY:

                        if (xdf->cOutputPosition > xdf->lwrrIndent)
                            xdf->newLine();
                        xdf->lwrrIndent -= XML_TAB_SIZE;
                        if (xdf->cOutputPosition == 0)
                            xdf->indentLine();
                        xdf->outputChars("</");
                        xdf->outputChars(m_XmlElementName);
                        xdf->outputChars(">");
//                      xdf->lwrrIndent -= XML_TAB_SIZE;
//                      xdf->newLine();
//                      xdf->indentLine();
                        break;

                    case XML_TERM_XML:
                        break;

                    default:

                        fprintf(stderr, "Unuspported XMLTerminatorType for XMLTerminator destructor\n");
                        exit(24);
                }
            }
        };
        friend class XMLTerminator;

        bool                compressOutput;
        int                 cOutputPosition;    // current output position
        int                 lwrrIndent;         // current indentation
        list<XMLTerminator> m_terminators;
        FILE*               xmlFile;
        gzFile              xmlgzFile;

    public:
        XMLFileWriter() :
            compressOutput(false),
            cOutputPosition(0),
            lwrrIndent(0),
            xmlFile(NULL),
            xmlgzFile(NULL)
        {
            nowAddr = &now;
            frameActiveAddr = &active;
        }

        bool XMLOpen(const char *fName, bool compressed_output)
        {
            compressOutput = compressed_output;
            if ((strcmp(fName, "stdout")==0) || (strcmp(fName, "STDOUT")==0))
            {
                xmlFile = stdout;
            }
            else if ( compressed_output )
            {
                xmlgzFile = gzopen (fName, GZ_FILE_OPEN_MODE);
                if (xmlgzFile == NULL)
                {
                    fprintf(stderr, "Failed to open gzipped XML output file %s\n", fName);
                    return initializedSuccessfully;
                }
            }
            else
            {
                fprintf(stderr, "Opening file '%s' in mode 'w'\n", fName);
                xmlFile = fopen(fName, "w");
                if (xmlFile == NULL)
                {
                    fprintf(stderr, "Failed to open XML output file %s\n", fName);
                    return initializedSuccessfully;
                }
            }
            lwrrIndent = 0;
            cOutputPosition = 0;

            // start the entry.

            m_terminators.push_back(XMLTerminator(this, "xml", XML_TERM_XML));

            initializedSuccessfully = true;
            return initializedSuccessfully;
        }

        // the destructor is responsible for completing any unclosed XML entries
        //  in the output and closing the file.

        virtual ~XMLFileWriter()
        {
            while ( ! m_terminators.empty() )
            {
                m_terminators.back().Close(this);
                m_terminators.pop_back();
            }
            if ( compressOutput )
            {
                gzclose(xmlgzFile);
            }
            else
            {
                fclose(xmlFile);
            }
        }

        virtual void endAttributes()
        {
            outputChars(">", 1);
            newLine();
            indentLine();
        }

        virtual void indentLine()
        {
            if (cOutputPosition < lwrrIndent)
                outputChars(lotsOfBlanks, lwrrIndent - cOutputPosition);
            cOutputPosition = lwrrIndent;
        }

        virtual void newLine()
        {
            cOutputPosition = 0;
            if ( compressOutput )
            {
                const char *nl = "\n";
                gzwrite(xmlgzFile, const_cast<char*>(nl), 1);
            }
            else if (!fwrite("\n", 1, 1, xmlFile))
            {
                fprintf(stderr, "ERROR: Unable to write end of line");
            }
        }

        virtual void outputAttribute(const char *aName, int aval)
        {
            char buff[256];
            int  hiHexWord;

            int len = 0;
            switch (integer_output_mode)
            {
                case decimal:

                    len = sprintf(buff, " %s=\"%d\"", aName, aval);
                    break;

                case hex:

                    len = sprintf(buff, " %s=\"0x%08x\"", aName, aval);
                    break;

                case mixed:
                    hiHexWord = aval >> 16;
                    if ((hiHexWord == 0) || (hiHexWord == -1) ||
                            (strncmp(aName + 3, "clk", 3) == 0))
                        len = sprintf(buff, " %s=\"%d\"", aName, aval);
                    else
                        len = sprintf(buff, " %s=\"0x%08x\"", aName, aval);
            }

            outputChars(buff, len);
        }

        virtual void outputAttribute(const char *aName, unsigned aval)
        {
            char buff[256];

            int len = 0;
            switch (integer_output_mode) {
                case decimal:
                    len = sprintf(buff, " %s=\"%u\"", aName, aval);
                    break;

                case hex:
                    len = sprintf(buff, " %s=\"0x%x\"", aName, aval);
                    break;

                case mixed:
                    unsigned hiHexWord = aval >> 16;
                    if ((hiHexWord == 0) || (hiHexWord == 0xFFFF) ||
                            (strncmp(aName + 3, "clk", 3) == 0))
                        len = sprintf(buff, " %s=\"%u\"", aName, aval);
                    else
                        len = sprintf(buff, " %s=\"0x%x\"", aName, aval);
            }

            outputChars(buff, len);
        }

        virtual void outputAttribute(const char *aName, long aval)
        {
            char buff[256];

            int len = 0;
            switch (integer_output_mode)
            {
                case decimal:
                    len = sprintf(buff, " %s=\"%ld\"", aName, aval);
                    break;

                case hex:
                    len = sprintf(buff, " %s=\"0x%lx\"", aName, aval);
                    break;

                case mixed:
                    long hiHexWord = aval >> 16;
                    if ((hiHexWord == 0) || (hiHexWord == -1) ||
                            (strncmp(aName + 3, "clk", 3) == 0))
                        len = sprintf(buff, " %s=\"%ld\"", aName, aval);
                    else
                        len = sprintf(buff, " %s=\"0x%lx\"", aName, aval);
            }

            outputChars(buff, len);
        }

        virtual void outputAttribute(const char *aName, UINT64 aval)
        {
            char                    buff[256];
            int                     len;

            len = 0;
            switch (integer_output_mode)
            {
                case decimal:
                    len = sprintf(buff, " %s=\"%llu\"", aName, aval);
                    break;

                case hex:
                    len = sprintf(buff, " %s=\"0x%llx\"", aName, aval);
                    break;

                case mixed:
                    INT64 hiHexWord = aval >> 32;
                    if ((hiHexWord == 0) || (hiHexWord == -1) ||
                            (strncmp(aName + 3, "clk", 3) == 0))
                        len = sprintf(buff, " %s=\"%llu\"", aName, aval);
                    else
                        len = sprintf(buff, " %s=\"0x%llx\"", aName, aval);
            }

            outputChars(buff, len);
        }

        virtual void outputAttribute(const char *aName, int aval, bool ign)
        {
            char buff[256];
            const int len = sprintf(buff, " %s=\"%08x\"", aName, aval);
            outputChars(buff, len);
        }

        virtual void outputAttribute(const char *aName, const char *sVal)
        {
            unsigned char buff[256];
            unsigned char lwrChar;
            int           slen = (int) strlen(sVal);

            // we have to make this legal XML, per the XML specification for quoting

            int len = sprintf((char *) buff, " %s=\"", aName);
            outputChars((char *) buff, len);
            for (int i=0; i<slen; i++)
            {
                lwrChar = (unsigned char) sVal[i];
                if ((lwrChar < 32) || (lwrChar > 126))
                {
                    len = sprintf((char *) buff, "&#x%02X;", lwrChar);
                    outputChars((char *) buff, len);
                }
                else
                {
                    switch (lwrChar)
                    {

                        case '&':
                            outputChars("&amp;", 5);
                            break;

                        case '<':
                            outputChars("&lt;", 4);
                            break;

                        case '>':
                            outputChars("&gt;", 4);
                            break;

                        case '\'':
                            outputChars("&apos;", 6);
                            break;

                        case '"':
                            outputChars("&quot;", 6);
                            break;

                        default:
                            buff[0] = lwrChar;
                            outputChars((char *) buff, 1);
                    }
                }
            }
            outputChars("\"", 1);
        }

        virtual void outputChars(const char *chars, int len)
        {
            if ((cOutputPosition + len) > XML_LINE_LENGTH)
            {
                newLine();
                indentLine();
            }
            if ( compressOutput )
            {   // zlib, unfortunately, does not use a const type for gzwrite's buffer...
                gzwrite(xmlgzFile, const_cast<char *>(chars), len);
            }
            else
            {
                len = len * static_cast<int>(fwrite(chars, len, 1, xmlFile));
            }
            cOutputPosition += len;
        }

        virtual void outputChars(const char *chars)
        {
            outputChars(chars, (int) strlen(chars));
        }

        virtual void XMLStartElement(const char *eType)
        {
            m_terminators.push_back(XMLTerminator(this, eType, XML_TERM_ATTRS_BODY));
        }

        bool Find(const string groupName)
        {
#if 0 // debug version of code to iterate through group elements explicitly
            for ( stringList::iterator ig = xmlGroups.begin(); ig != xmlGroups.end(); ++ig ) {
                if (*ig == groupName) {
                    return true;
                }
            }
            return false;
#else
            return find(xmlGroups.begin(), xmlGroups.end(), groupName) != xmlGroups.end();
#endif
        }

        virtual bool XMLDumping(const char *specifier)
        {
            const char                   *dotpos;

            // If the specifier is the special value "CMODEL_END", this
            //  is informing us that the cmodel has finished exelwtion,
            //  including output of summary statistics as interfaces complete.
            //  In that case, we want to close the dump

            if (!strcmp(specifier, "CMODEL_END"))
            {
                XMLStartStdInline("EndOfDump");
                XMLEndLwrrent();
                if ( ! compressOutput )
                {
                    fflush(xmlFile);
                }
                return false;
            }

            if (specifier != NULL)
            {
                dotpos = strchr(specifier, '.');
                if (dotpos != NULL)
                {
                    if (xmlGroupsAreIncludes)
                    {
                        return Find(string(specifier, dotpos-specifier));
                    }
                    else
                    {
                        return !Find(string(specifier, dotpos-specifier));
                    }
                }
            }
            return true;  // dev hack
        }

        // "smart" version of XMLDumping
        //
        // This version maintains a cached status word that can tell the caller that no generation
        //  is needed very easily.  The code generated by XINLINE(elementName) is:
        //
        //      if ((xmlDump != NULL) &&
        //          (cachedState != neverOutput) &&
        //          ((cachedState == output) || xmlDump->XMLDumping(cachedState, elementName)
        //

        virtual bool XMLDumping(cachedSelectionState &cState, const char *specifier)
        {
            switch (cState) {

                case untouched:

                    if (XMLDumping(specifier))
                    {
                        cState = outputIfFrameActive;       // note - fall through to next case!
                    }
                    else
                    {
                        cState = neverOutput;
                        return false;
                    }

                case outputIfFrameActive:

                    if (strncmp(specifier, "Standard", 7) == 0)
                    {
                        cState = output;
                        return true;
                    }
                    if (*frameActiveAddr)
                        cState = output;
                    return (*frameActiveAddr != 0);

                default:

                    fprintf(stderr, "ERROR: Unexpected cached state value given to XMLDumping");
            }
            return false;

        }

        virtual void XMLEndLwrrent()
        {
            setMixedIntegerOutputMode();                // reset to default mode
            m_terminators.back().Close(this);
            m_terminators.pop_back();

            if ( forceFlush && ! compressOutput )
            {
                fflush(xmlFile);
            }
        }

        virtual void XMLStartStdNonInline(const char *eType)
        {
            m_terminators.push_back(XMLTerminator(this, eType, XML_TERM_BODY));
        }

        virtual void XMLStartStdInline(const char *eType)
        {
            m_terminators.push_back(XMLTerminator(this, eType, XML_TERM_ATTRS));
            outputAttribute("simclk", *nowAddr);
        }
};

// defined here to match previous chips
XMLDump::~XMLDump()
{
}

XMLWriter *XMLWriter::newWriter(void)
{
    return new XMLFileWriter;
}
