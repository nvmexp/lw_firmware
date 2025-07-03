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
// Mdiag_XML.cpp - routines supporting XML logging in the acf test

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <list>
#include <string>
#include <iostream>

#include "mdiag/sysspec.h"
#include "sim/IChip.h"
#include "sim/IIdList.h"
#include "XMLlogging.h"
#include "mdiag_xml.h"

#include <algorithm>
#include <set>
#include <string>

#include <sys/timeb.h>
static double getLwrrentRealTime() {
#if defined(_WIN32)
    struct _timeb timebuffer;
    _ftime(&timebuffer);
#else
    struct timeb timebuffer;
    ftime(&timebuffer);
#endif
    return (double)timebuffer.time+(((double)timebuffer.millitm)/1000.0);
}

int now = 0;

// Constructor - specifies the logging file name

Mdiag_XML::Mdiag_XML(const string &logFileName, bool incPresent,
                     stringList &specifier_list, bool compressed_output)
{
    creation_succeeded = false;
    xml_writer = NULL;

    // send the XML writer object down to the cmodel

    xml_writer = XMLWriter::newWriter();
    if (!xml_writer->XMLOpen(logFileName.c_str(), compressed_output))
    {
        fprintf(stderr, "ERROR: XML output writer failed to initialized successfully\n");
        return;
    }
    xml_writer->setClock(&now);
    xml_writer->setGroupsAreIncludes(incPresent);
    xml_writer->addGroups(specifier_list);

    startClock = getLwrrentRealTime();

    creation_succeeded = true;
}

long Mdiag_XML::GetRuntimeTimeInMilliSeconds()
{
    double inow = getLwrrentRealTime() - startClock;

    return (long)(1e3*inow);
}

// Destructor

Mdiag_XML::~Mdiag_XML() {
    if(xml_writer)
            delete xml_writer;
}
