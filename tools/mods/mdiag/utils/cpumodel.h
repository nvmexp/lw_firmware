/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 *
 * Namespace defines queries and operations on CPU Model (an LWPU
 * developed model of a generic NCI-compatible CPU and cache
 * see //hw/doc/gpu/volta/volta/design/testplan/FD_testplans/Volta_LWLink2_Arch_Infrastructure.docx
 * for details)
 */

#ifndef CPUMODEL_H
#define CPUMODEL_H

namespace CPUModel
{
    // returns true if CPU Model is enabled in chiplib, false otherwise
    bool Enabled();
}
#endif
