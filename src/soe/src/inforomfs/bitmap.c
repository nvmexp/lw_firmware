#include "inforom/bitmap.h"

#define BM_OP_SET   0
#define BM_OP_CLEAR 1

void ifsBitmapOp(LwU64 *pBitmap, LwU32 start, LwU32 count, int op)
{
    LwU32 page = start / 64;
    LwU32 index = start % 64;

    for ( ; count > 0; page++) {
        LwU64 val = pBitmap[page];

        for ( ; index < 64; index++) {
            if (op == BM_OP_SET)
                val |= (1llu << index);
            else
                val &= ~(1llu << index);

            count--;
            if (count == 0) break;
        }

        pBitmap[page] = val;
        index = 0;
    }
}

void ifsBitmapSet(LwU64 *pBitmap, LwU32 start, LwU32 count)
{
    ifsBitmapOp(pBitmap, start, count, BM_OP_SET);
}

void ifsBitmapClear(LwU64 *pBitmap, LwU32 start, LwU32 count)
{
    ifsBitmapOp(pBitmap, start, count, BM_OP_CLEAR);
}

LwBool ifsBitmapIsSet(LwU64 *pBitmap, LwU32 index)
{
    LwU64 val = pBitmap[index / 64];
    return (val & (1ull << (index % 64))) ? LW_TRUE : LW_FALSE;
}