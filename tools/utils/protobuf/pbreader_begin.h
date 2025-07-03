/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// This file is not meant to be used directly but rather is used in files generated
// by protobuf.py in conjuction with pbcommon_end.h and the file that protobuf.py
// generates from the *.proto file
//
// Together with those files this file will create the data structures and enums
// necessary for reading protobuf messages
//
// See pbcommon.h for a description of a simple usage of the library

#define BEGIN_MESSAGE(name) struct name {
#define BEGIN_MESSAGE_CONTAINER(name) BEGIN_MESSAGE(name)
#define DEFINE_FIELD(name, type, value, isPublic) static constexpr unsigned name = value;
#define DEFINE_REPEATED_FIELD(name, type, value, isPublic) static constexpr unsigned name = value;
#define BEGIN_SUBMESSAGES
#define END_SUBMESSAGES
#define END_MESSAGE };
#define END_MESSAGE_CONTAINER END_MESSAGE
#define BEGIN_ENUM(name) enum name {
#define DEFINE_ENUM_VALUE(key, value) key = value,
#define END_ENUM };
