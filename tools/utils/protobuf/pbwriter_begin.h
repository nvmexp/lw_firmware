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
// necessary for writing protobuf messages
//
// See pbcommon.h for a description of a simple usage of the library

using namespace ProtobufWriter;

#define BEGIN_MESSAGE(name)                                 \
    struct name: public ProtobufWriter::ProtobufPusher      \
    {                                                       \
        using reference = name&;                            \
        name(name&&)            = default;                  \
        name& operator=(name&&) = default;                  \
        explicit name(ByteStream* pBytes)                   \
        : ProtobufPusher(pBytes)                            \
        {                                                   \
        }

#define DEFINE_FIELD(name, type, field, isFieldPublic)      \
        reference name(const type& fieldVal,                \
                       Output how = Output::Normal)         \
        {                                                   \
            if (isFieldPublic || !IsFieldVisibilityEnforced())  \
            {                                               \
                PushField(field, fieldVal, how);            \
            }                                               \
            return *this;                                   \
        }                                                   \
        Dumper<type, field, isFieldPublic> name()           \
        {                                                   \
            auto pBytes = ProtobufWriter::ProtobufPusher::GetByteStream();  \
            return Dumper<type, field, isFieldPublic>(      \
                    pBytes, DumpPos(pBytes->size()));       \
        }                                                   \
        static Dumper<type, field, isFieldPublic> name(ByteStream* pBytes, \
                                                       DumpPos dumpPos)    \
        {                                                   \
            return Dumper<type, field, isFieldPublic>(      \
                    pBytes, dumpPos);                       \
        }                                                   \
        static Dumper<type, field, isFieldPublic> name(ByteStream* pBytes) \
        {                                                   \
            return Dumper<type, field, isFieldPublic>(      \
                    pBytes, DumpPos(pBytes->size()));       \
        }                                                   \
        static Deductor<type, field, isFieldPublic> name(reference) \
        {                                                   \
            return Deductor<type, field, isFieldPublic>();  \
        }
#define DEFINE_REPEATED_FIELD(name, type, field, isFieldPublic) \
        template<typename T,                                                    \
        enable_if_t<                                                            \
            (sizeof(T) >= sizeof(type))                                         \
            &&                                                                  \
                (is_integral<T>::value == is_integral<T>::value &&              \
                 is_signed<T>::value == is_signed<type>::value)                 \
                ||                                                              \
                (is_floating_point<T>::value == is_floating_point<type>::value) \
                ||                                                              \
                (is_enum<T>::value && is_same<T, type>::value)                  \
            , int> = 0>                                                         \
        reference name(const vector<T>& values)             \
        {                                                   \
            if (isFieldPublic || !IsFieldVisibilityEnforced())  \
            {                                               \
                PushRepeatedField(field, values);           \
            }                                               \
            return *this;                                   \
        }                                                   \
        template<typename T,                                \
                 enable_if_t<is_same<T, type>::value &&     \
                             !is_arithmetic<T>::value, int> \
                            = 0>                            \
        reference name(const T& value)                      \
        {                                                   \
            if (isFieldPublic || !IsFieldVisibilityEnforced())  \
            {                                               \
                PushField(field, value, Output::Normal);    \
            }                                               \
            return *this;                                   \
        }                                                   \
        Dumper<type, field, isFieldPublic> name()           \
        {                                                   \
            LWDASSERT((is_base_of<ProtobufWriter::ProtobufPusher, type>::value)); \
            auto pBytes = ProtobufWriter::ProtobufPusher::GetByteStream();        \
            return Dumper<type, field, isFieldPublic>(      \
                    pBytes, DumpPos(pBytes->size()));       \
        }                                                   \
        static Dumper<type, field, isFieldPublic> name(ByteStream* pBytes, \
                                        DumpPos dumpPos)    \
        {                                                   \
            return Dumper<type, field, isFieldPublic>(      \
                    pBytes, dumpPos);                       \
        }                                                   \
        static Dumper<type, field, isFieldPublic> name(ByteStream* pBytes) \
        {                                                   \
            return Dumper<type, field, isFieldPublic>(      \
                    pBytes, DumpPos(pBytes->size()));       \
        }                                                   \
        static Deductor<type, field, isFieldPublic> name(reference) \
        {                                                   \
            return Deductor<type, field, isFieldPublic>();  \
        }
#define END_MESSAGE };
#define BEGIN_ENUM(name) enum name : pb_uint32 {
#define DEFINE_ENUM_VALUE(key, value) key = value,
#define END_ENUM };
