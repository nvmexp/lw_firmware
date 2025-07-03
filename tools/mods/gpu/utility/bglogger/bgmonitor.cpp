/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2015,2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   bgmonitor.cpp
 *
 * @brief  Background monitor base class.
 *
 */

#include "bgmonitor.h"

BgMonitor::Sample::SampleItem* BgMonitor::Sample::AllocItem
(
    ValueType type,
    UINT32    numElems
)
{
    MASSERT(type != ANY);

    const size_t hdrSize  = sizeof(SampleItem) - sizeof(ValueType);
    const size_t itemSize = (type == FLOAT)                    ? sizeof(float) :
                            (type == INT || type == INT_ARRAY) ? sizeof(INT64) :
                            (type == STR)                      ? sizeof(char)  :
                            0;
    const size_t elemSize = Utility::AlignUp(hdrSize + itemSize * numElems,
                                             sizeof(SampleItem));

    const size_t oldSize  = data.size();
    data.resize(oldSize + elemSize);

    SampleItem* const pItem = reinterpret_cast<SampleItem*>(&data[oldSize]);
    pItem->type     = type;
    pItem->size     = static_cast<UINT32>(elemSize);
    pItem->numElems = numElems;

    return pItem;
}

void BgMonitor::Sample::Push(float value)
{
    SampleItem* const pItem    = AllocItem(FLOAT, 1);
    pItem->value.floatValue[0] = value;
}

void BgMonitor::Sample::Push(UINT32 value)
{
    SampleItem* const pItem  = AllocItem(INT, 1);
    pItem->value.intValue[0] = static_cast<INT64>(value);
}

void BgMonitor::Sample::Push(INT64 value)
{
    SampleItem* const pItem  = AllocItem(INT, 1);
    pItem->value.intValue[0] = value;
}

void BgMonitor::Sample::Push(const INT64* pValues, UINT32 numElems)
{
    SampleItem* const  pItem = AllocItem(INT_ARRAY, numElems);
    INT64*             pDest = &pItem->value.intValue[0];
    const INT64* const pEnd  = pValues + numElems;
    for ( ; pValues != pEnd; ++pValues) {
        *pDest = *pValues;
        ++pDest;
    }
}

void BgMonitor::Sample::Push(const char* str, UINT32 len)
{
    SampleItem* const pItem = AllocItem(STR, len);
    memcpy(&pItem->value.stringValue[0], str, len);
}

BgMonitor::Sample::ConstIterator::ConstIterator(const vector<char>& data, size_t index)
: m_pData(&data), m_Index(index)
{
    MASSERT(index <= data.size());
    if (index < data.size())
    {
        MASSERT((data.size() - index) >= sizeof(SampleItem));
        MASSERT((*this)->size <= (data.size() - index));
    }
}

BgMonitor::Sample::ConstIterator& BgMonitor::Sample::ConstIterator::operator++()
{
    m_Index += (*this)->size;
    return *this;
}

RC BgMonitor::Sample::FormatAsStrings
(
    vector<string>* pStrings,
    ValueType       type,
    int             precision
) const
{
    if (data.empty())
    {
        return RC::OK;
    }

    for (const SampleItem& item : *this)
    {
        MASSERT(item.type != ANY);

        if (type != ANY)
        {
            MASSERT(type == item.type);
            if (type != item.type)
            {
                Printf(Tee::PriError, "Invalid bg monitor data\n");
                return RC::SOFTWARE_ERROR;
            }
        }

        const SampleItem::Value& value = item.value;
        switch (item.type)
        {
            case INT:
                MASSERT(item.numElems == 1);
                // fall-through
            case INT_ARRAY:
                for (UINT32 i = 0; i < item.numElems; i++)
                {
                    pStrings->push_back(Utility::StrPrintf("%lld", value.intValue[i]));
                }
                break;

            case FLOAT:
                MASSERT(item.numElems == 1);
                pStrings->push_back(Utility::StrPrintf("%.*f", precision, value.floatValue[0]));
                break;

            case STR:
                // A string is one element consisting of many characters.
                // The numElems field specifies how many characters there are in
                // the string, not how many strings there are.
                // So we skip to the end.
                pStrings->emplace_back(&value.stringValue[0], item.numElems);
                break;

            default:
                MASSERT(!"Invalid type");
        }
    }
    return RC::OK;
}

RC BgMonitor::GetPrintable(const Sample& sample, vector<string>* pStrings)
{
    return sample.FormatAsStrings(pStrings, ANY, 4);
}
