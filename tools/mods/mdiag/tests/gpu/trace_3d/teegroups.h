/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_TEEGROUPS_H
#define INCLUDED_TEEGROUPS_H

#include "core/include/tee.h"

// Helpers
#define DECLARE_MSG_GROUP_T3D(Group) \
    DECLARE_MSG_GROUP(Mdiag, Trace3d, Group)

#define DEFINE_MSG_GROUP_T3D(Group) \
    DEFINE_MSG_GROUP(Mdiag, Trace3d, Group, false)

#define T3D_MSGID(Group) __MSGID__(Mdiag, Trace3d, Group)

// Groups
DECLARE_MSG_GROUP_T3D(ChannelLog);
DECLARE_MSG_GROUP_T3D(ChipSupport);
DECLARE_MSG_GROUP_T3D(Gild);
DECLARE_MSG_GROUP_T3D(Main);
DECLARE_MSG_GROUP_T3D(Massage);
DECLARE_MSG_GROUP_T3D(Notifier);
DECLARE_MSG_GROUP_T3D(P2P);
DECLARE_MSG_GROUP_T3D(PluginAPI);
DECLARE_MSG_GROUP_T3D(SLI);
DECLARE_MSG_GROUP_T3D(Surface);
DECLARE_MSG_GROUP_T3D(T5d);
DECLARE_MSG_GROUP_T3D(TraceMod);
DECLARE_MSG_GROUP_T3D(TraceOp);
DECLARE_MSG_GROUP_T3D(TraceParser);
DECLARE_MSG_GROUP_T3D(TraceReloc);
DECLARE_MSG_GROUP_T3D(WaitX);
DECLARE_MSG_GROUP_T3D(ContextSync);

#endif
