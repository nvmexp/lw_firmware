/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef FIELDIAG_H
#define FIELDIAG_H

#include <string>
#include <vector>

#include "rc.h"
#include "types.h"

#include "restarter.h"

void FieldiagRestartCheck(ProcCtrl::Performer *performer, INT32 rc);

#endif
