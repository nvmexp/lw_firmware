/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "pbcommon.h"
#include "inc/bytestream.h"

using namespace std;

namespace ProtobufReader
{
    // Parsed protobuf field header
    struct FieldHdr
    {
        size_t               pos;
        int                  index;
        ProtobufCommon::Wire wire;
        FieldHdr() : pos(0), index(0), wire(ProtobufCommon::Wire::VarInt) { }
    };

    // Used for parsing protobuf structured data.  This structure only tracks
    // the parsing context, but it does not copy the container containing the
    // parsed protobuf data nor any portion of it.
    class PBInput
    {
    public:

        // Constructor for data already in a bytestream
        explicit PBInput
        (
            PBInput*              pParent,
            const ByteStream *    pMessageData,
            size_t                pos,
            size_t                endPos,
            LwDiagUtils::Priority errorPri
        )
        : pParent(pParent),
          pMessageData(pMessageData),
          pos(pos),
          endPos(endPos),
          errorPri(errorPri)
        {
        }

        // Constructor for data in a string
        explicit PBInput
        (
            PBInput*              pParent,
            const string *        pFileData,
            size_t                pos,
            size_t                endPos,
            LwDiagUtils::Priority errorPri
        )
        : pParent(pParent),
          pFileData(pFileData),
          pos(pos),
          endPos(endPos),
          errorPri(errorPri)
        {
        }

        // Construct PBInput for a message containing submessages.
        // The new PBInput constructed this way spans only the message
        // which will be parsed.  The contructor also updated the source
        // PBInput and skips the current message.
        PBInput(PBInput& input, size_t hdrPos);

        virtual ~PBInput();

        // Boiler plate for iterating over input with range for loop
        class Iterator
        {
            public:
                explicit Iterator(PBInput* pInput);

                FieldHdr& operator*() { return m_Hdr; }

                void operator++();

                bool operator!=(const Iterator& other) const;

            private:
                PBInput* m_pInput;
                FieldHdr m_Hdr;
        };
        Iterator begin() { return Iterator(this);    }
        Iterator end()   { return Iterator(nullptr); }

        virtual void SetParsingError(size_t errorPos);
        virtual size_t GetPBSize(size_t initialPos);

        PBInput*              pParent      = nullptr;
        const ByteStream *    pMessageData = nullptr;
        const string *        pFileData    = nullptr;
        size_t                pos          = 0;
        size_t                endPos       = 0;
        UINT64                unknown      = 0;
        UINT64                corrupted    = 0;
        bool                  error        = false;
        LwDiagUtils::Priority errorPri     = LwDiagUtils::PriError;
    };

    // Nasty (but concise) template magic to deduce protobuf wire type
    // from data type used during protobuf message parsing.
    template<typename T, enable_if_t<is_integral<T>::value, int> = 0>
    constexpr ProtobufCommon::Wire TypeToWireInteger()
    {
        return ProtobufCommon::Wire::VarInt;
    }

    template<typename T>
    constexpr ProtobufCommon::Wire TypeToWire()
    {
        return TypeToWireInteger<T>();
    }

    template<>
    constexpr ProtobufCommon::Wire TypeToWire<float>()
    {
        return ProtobufCommon::Wire::Float;
    }

    template<>
    constexpr ProtobufCommon::Wire TypeToWire<string>()
    {
        return ProtobufCommon::Wire::Bytes;
    }

    // Extract one data item of primitive type (VarInt/Bytes/Float) from protobuf byte stream
    template<typename T>
    T GetPBData(PBInput& input)
    {
        T value = { };
        const size_t initialPos = input.pos;
        LwDiagUtils::EC ec = LwDiagUtils::OK;
        if (input.pMessageData != nullptr)
        {
            ec = ByteStream::UnpackPBRange(input.pMessageData->data(),
                                           input.endPos, &input.pos, &value);
        }
        else
        {
            ec = ByteStream::UnpackPBRange(reinterpret_cast<const UINT08 *>(input.pFileData->data()),
                                           input.endPos, &input.pos, &value);
        }

        if (ec != LwDiagUtils::OK)
        {
            input.SetParsingError(initialPos);
        }
        return value;
    }

    template<typename ParseType,
             typename ElementType>
    void ParseField(PBInput& input, ElementType* pElement)
    {
        auto value = static_cast<ElementType>(GetPBData<ParseType>(input));
        if (!input.error)
        {
            *pElement = move(value);
        }
    }

    template<typename ParseType,
             typename ElementType,
             enable_if_t<is_arithmetic<ParseType>::value, int> = 0>
    void ParseRepeatedField
    (
        PBInput& input,
        size_t                   pos,
        vector<ElementType>*     pContainer
    )
    {
        PBInput entryInput(input, pos);

        while ((entryInput.pos < entryInput.endPos) && !entryInput.error)
        {
            auto value = static_cast<ElementType>(GetPBData<ParseType>(entryInput));
            if (!entryInput.error)
            {
                pContainer->push_back(move(value));
            }
        }
    }

    template<typename ParseType,
             typename ElementType,
             enable_if_t<is_same<ParseType, string>::value, int> = 0>
    void ParseRepeatedField
    (
        PBInput& input,
        size_t,
        vector<ElementType>*  pContainer
    )
    {
        auto value = static_cast<ElementType>(GetPBData<ParseType>(input));
        if (!input.error)
        {
            pContainer->push_back(move(value));
        }
    }

    FieldHdr GetPBFieldHdr(PBInput& input);

    // Check wire type of the current entry and report error if it's incorrect
    bool CheckWire
    (
        const char*          functionName,
        const char*          parentName,
        PBInput&             input,
        const FieldHdr&      hdr,
        ProtobufCommon::Wire expected
    );
}
