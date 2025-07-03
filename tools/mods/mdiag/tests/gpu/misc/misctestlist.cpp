/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#define MAKE_TEST_LIST
#include "mdiag/tests/test.h"
#include "misctestlist.h"
#undef MAKE_TEST_LIST

#include "core/include/platform.h"

// ---------------------------------------------------------------------------
// Assign final value of TEST_LIST_HEAD to misctest_list_head so that linked list
// of TestListEntry objects is accessible.
// ---------------------------------------------------------------------------
TestListEntryPtr misctest_list_head = TEST_LIST_HEAD; // it's exported to testdir
