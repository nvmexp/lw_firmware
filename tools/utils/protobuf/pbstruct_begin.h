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
// generates from the *.proto file.  Usage of these structs is optional.
//
// Together with those files this file will create C++ structures and enums for each
// of the protobuf messages
//
// See pbcommon.h for a description of a simple usage of the library

// Primitive protobuf types.
// The type names are as defined in protobuf specification.
// The pb_ prefix is prepended by mle.py when generating
// mle.h in order to avoid clashing with C++ names such
// as bool/float/string.
using pb_uint32 = UINT32;
using pb_uint64 = UINT64;
using pb_sint32 = INT32;
using pb_sint64 = INT64;
using pb_bool   = bool;
using pb_float  = float;
using pb_double = double;
using pb_string = string;

#define BEGIN_MESSAGE(name) struct name {
#define DEFINE_FIELD(name, type, value, isPublic) type name;
#define DEFINE_REPEATED_FIELD(name, type, value, isPublic) vector<type> name;
#define END_MESSAGE };
#define BEGIN_ENUM(name) enum name {
#define DEFINE_ENUM_VALUE(key, value) key = value,
#define END_ENUM };
