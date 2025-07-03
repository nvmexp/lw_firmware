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
// XMLlogging.h
//
// Include this file when generating XML output

#ifndef XMLLOGGING_H
#define XMLLOGGING_H

#include "XMLWriter.h"

#define XD   reinterpret_cast<XMLDump*>(gXML ? gXML->xml_writer : 0)

#include "mdiag_xml.h"

#endif
