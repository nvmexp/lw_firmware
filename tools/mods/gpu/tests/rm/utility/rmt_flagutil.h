/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _RMT_FLAGUTIL_H
#define _RMT_FLAGUTIL_H

#include "core/include/utility.h"

// TODO: These must already exist somewhere...?
#define CONCAT(x, y) x##y
#define CONCAT2(x, y) CONCAT(x, y)

namespace rmt
{
    //! Utility to describe and manipulate a set of bitmask flags.
    class BitFlags
    {
    public:
        BitFlags();

        void RegisterFlag(const UINT32 flag, const char *name, const double prob);
        UINT32 GetRandom(Random *pRand) const;
        void Print(const INT32 pri, const char *prefix, const UINT32 flags) const;

        //! Utility to statically register a flag.
        class Flag
        {
        public:
            Flag(BitFlags *pFlags, const UINT32 flag, const char *name,
                 const double prob)
            {
                pFlags->RegisterFlag(flag, name, prob);
            }
        };
        //! DRY wrapper macro to statically register a flag.
        #define STATIC_BIT_FLAG(pFlags, prob, flag) \
            static BitFlags::Flag CONCAT2(bitflag, __LINE__) \
                (pFlags, flag, #flag, prob)

    private:
        const char *m_names[32];
        double      m_probs[32];
    };

    //! Utility to describe and manipulate a set of DRF bit-fields.
    class BitFields
    {
    public:
        class Field;
        class Value;

        BitFields();

        void RegisterField(const UINT32 index, Field *field);
        void RegisterValue(const UINT32 index, const Value *value, const double prob);
        void PrepareFields();
        UINT32 GetRandom(Random *pRand) const;
        void Print(const INT32 pri, const char *prefix, const UINT32 data) const;

        //! Bit-field descriptor.
        class Field
        {
        public:
            Field(BitFields *pFields, const UINT32 index, const UINT32 width,
                  const char *name) :
                m_width(width), m_name(name)
            {
                pFields->RegisterField(index, this);
            }
            const UINT32             m_width;
            const char              *m_name;
            vector<const Value*>     m_values;
            vector<Random::PickItem> m_picks;
        };
        //! DRY wrapper macro to statically register a field.
        #define STATIC_BIT_FIELD(pFields, fld) \
            static BitFields::Field CONCAT2(bitvalue, __LINE__) \
                (pFields, DRF_SHIFT(fld), DRF_SIZE(fld), #fld)

        //! Bit-field value descriptor.
        class Value
        {
        public:
            Value(BitFields *pFields, const UINT32 index, const UINT32 value,
                  const char *name, const double prob) :
                m_value(value), m_name(name)
            {
                pFields->RegisterValue(index, this, prob);
            }
            const UINT32 m_value;
            const char  *m_name;
        };
        //! DRY wrapper macro to statically register a field value.
        #define STATIC_BIT_VALUE(pFields, fld, prob, val) \
            static BitFields::Value CONCAT2(bitfield, __LINE__) \
                (pFields, DRF_SHIFT(fld), fld##val, #val, prob)

        //! Utility to prepare a set of fields statically.
        class Prepper
        {
        public:
            Prepper(BitFields *pFields)
            {
                pFields->PrepareFields();
            }
        };
        //! DRY wrapper macro to statically prepare a field set.
        #define STATIC_BIT_FIELD_PREP(pFields) \
            static BitFields::Prepper CONCAT2(bitprep, __LINE__)(pFields)

    private:
        Field *m_fields[32];
        bool   m_bPrepared;
    };
}

#endif
