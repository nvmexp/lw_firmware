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
// mdiag_xml.h -- define the MDiag_XML state object

#ifndef MDIAG_XML_H
#define MDIAG_XML_H

#include <list>
#include <string>

#include "XMLlogging.h"

class CSysSpec;

class Mdiag_XML
{
public:

    typedef list<string> stringList;

    // Constructor is intialized with the output file name

    Mdiag_XML(const string &logFileName, bool incSpecified, stringList &specifier_list, bool compressed_output);

    long GetRuntimeTimeInMilliSeconds();

    virtual ~Mdiag_XML();

    bool                    creation_succeeded; // to tell acf.cpp all is well

    XMLWriter              *xml_writer;

private:

    double                  startClock; // time since start of run
};

extern Mdiag_XML* gXML;

#endif
