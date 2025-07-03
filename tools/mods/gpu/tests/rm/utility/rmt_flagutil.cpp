/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwmisc.h"
#include "core/include/utility.h"
#include "rmt_flagutil.h"
#include "core/include/memcheck.h"

using namespace rmt;

//! Construct empty bitflags descriptor.
//-----------------------------------------------------------------------------
BitFlags::BitFlags()
{
    memset(m_names, 0, sizeof(m_names));
    memset(m_probs, 0, sizeof(m_probs));
}

//! Associate a name and probability with a single-bit flag value.
//-----------------------------------------------------------------------------
void BitFlags::RegisterFlag(const UINT32 flag, const char *name, const double prob)
{
    // Assert flag is a single bit.
    MASSERT(flag && !(flag & (flag - 1)));
    const UINT32 i = BIT_IDX_32(flag);

    // Assert no duplicates.
    MASSERT(m_names[i] == NULL);

    // Keep track in table.
    m_names[i] = name;
    m_probs[i] = prob;
}

//! Generate a random bitmask of flags that have been registered.
//-----------------------------------------------------------------------------
UINT32 BitFlags::GetRandom(Random *pRand) const
{
    UINT32 data = 0;
    for (UINT32 i = 0; i < 32; ++i)
    {
        // Only attempt flags that are registered (and possible).
        if (m_probs[i] != 0.0)
        {
            if (m_probs[i] > pRand->GetRandomDouble(0.0, 1.0))
            {
                data |= BIT(i);
            }
        }
    }
    return data;
}

//! Pretty-print a bitmask of flags (whether registered or not).
//-----------------------------------------------------------------------------
void BitFlags::Print(const INT32 pri, const char *prefix, const UINT32 flags) const
{
    // First print prefix to determine vertical alignment.
    const INT32 indent = Printf(pri, "%s", prefix);

    // If no flags are set just print 0.
    if (flags == 0)
    {
        Printf(pri, "0\n");
        return;
    }

    // Loop through flags that are set.
    for (UINT32 i = 0; i < 32; ++i)
    {
        if (flags & BIT(i))
        {
            // If available, print registered name.
            if (m_names[i] != NULL)
            {
                Printf(pri, "%s", m_names[i]);
            }
            // Otherwise fallback to BIT(i) notation.
            else
            {
                Printf(pri, "BIT(%u)", i);
            }

            // If we still have more flags set, print separator.
            if ((flags >> i) != 1)
            {
                Printf(pri, " |\n");

                // Also vertically align for next flag.
                for (INT32 j = 0; j < indent; ++j)
                {
                    Printf(pri, " ");
                }
            }
            // Otherwise finish with newline.
            else
            {
                Printf(pri, "\n");
            }
        }
    }
}

//! Construct empty bitfields descriptor.
//-----------------------------------------------------------------------------
BitFields::BitFields() : m_bPrepared(false) // we're not eagle scouts yet
{
    memset(m_fields, 0, sizeof(m_fields));
}

//! Associate a field with a bit index.
//-----------------------------------------------------------------------------
void BitFields::RegisterField(const UINT32 index, BitFields::Field *field)
{
    // Assert bounds of field.
    const UINT32 limit = index + field->m_width;
    MASSERT(field->m_width != 0);
    MASSERT(limit <= 32);

    // Assert not overlapping with existing fields.
    for (UINT32 i = 0; i < limit; ++i)
    {
        if (m_fields[i] != NULL)
        {
            MASSERT((i + m_fields[i]->m_width) <= index);
        }
    }

    // Track field in table.
    m_fields[index] = field;

    // Fields updated so need to call PrepareFields().
    m_bPrepared = false;
}

//! Associate a field value and probability with the field at a bit index.
//-----------------------------------------------------------------------------
void BitFields::RegisterValue(const UINT32 index, const BitFields::Value *value,
                              const double prob)
{
    // Assert valid field.
    MASSERT(index < 32);
    MASSERT(m_fields[index] != NULL);

    // Add value to field.
    Field *pField = m_fields[index];
    pField->m_values.push_back(value);

    // Add relative probability of value, scaled and truncated to integer.
    size_t p = pField->m_picks.size();
    pField->m_picks.resize(p + 1);
    pField->m_picks[p].RelProb = (UINT32)(100 * prob);
    pField->m_picks[p].Min = value->m_value;
    pField->m_picks[p].Max = value->m_value;

    // Fields updated so need to call PrepareFields().
    m_bPrepared = false;
}

//! Normalize field probabilities - call this before generating random fields.
//-----------------------------------------------------------------------------
void BitFields::PrepareFields()
{
    // Foreach valid field, normalize relative value probabilities.
    for (UINT32 i = 0; i < 32; ++i)
    {
        Field *pField = m_fields[i];
        if (pField != NULL && !pField->m_picks.empty())
        {
            Random::PreparePickItems((INT32)pField->m_picks.size(), &pField->m_picks[0]);

            // WAR unsupported 0% probability.
            UINT32 lastProb = 0;
            for (UINT32 j = 0; j < pField->m_picks.size(); ++j)
            {
                if (pField->m_picks[j].RelProb == 0)
                {
                    pField->m_picks[j].ScaledProb = lastProb;
                }
                lastProb = pField->m_picks[j].ScaledProb;
            }
        }
    }
    m_bPrepared = true; // got our last merit badge
}

//! Generate a random set of registered field values.
//-----------------------------------------------------------------------------
UINT32 BitFields::GetRandom(Random *pRand) const
{
    // Assert PrepareFields() has been called since last update.
    MASSERT(m_bPrepared);

    // Pick random value for each field.
    UINT32 data = 0;
    for (UINT32 i = 0; i < 32; ++i)
    {
        const Field *pField = m_fields[i];
        if (pField != NULL && !pField->m_picks.empty())
        {
            const UINT32 val = pRand->Pick(&pField->m_picks[0]);
            data |= (val & (BIT(pField->m_width) - 1)) << i;
        }
    }
    return data;
}

//! Pretty-print a set of fields (whether registered or not).
//-----------------------------------------------------------------------------
void BitFields::Print(const INT32 pri, const char *prefix, const UINT32 data) const
{
    // Assert PrepareFields() has been called since last update.
    MASSERT(m_bPrepared);

    // First print prefix to determine vertical alignment.
    const INT32 indent = Printf(pri, "%s", prefix);

    // Print raw value for visibility into unregistered fields.
    Printf(pri, "0x%08X\n", data);

    // Loop through registered fields.
    for (UINT32 i = 0; i < 32; ++i)
    {
        const Field *pField = m_fields[i];
        if (pField != NULL)
        {
            // Vertically align.
            for (INT32 j = 0; j < indent; ++j)
            {
                Printf(pri, " ");
            }

            // Print field name.
            Printf(pri, "%s = ", pField->m_name);

            // Search for registered value.
            const UINT32 val = (data >> i) & (BIT(pField->m_width) - 1);
            const Value *pValue = NULL;
            for (size_t j = 0; j < pField->m_values.size(); ++j)
            {
                if (pField->m_values[j]->m_value == val)
                {
                    pValue = pField->m_values[j];
                    break;
                }
            }

            // If registered, use registred name.
            if (pValue != NULL)
            {
                Printf(pri, "%s", pValue->m_name);
            }
            // Otherwise fallback to raw value.
            else
            {
                Printf(pri, "0x%X", val);
            }

            // Newline after each registered field.
            Printf(pri, "\n");
        }
    }
}

