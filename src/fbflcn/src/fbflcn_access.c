/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


#include "fbflcn_access.h"



/*!
 * @brief Extract content from a packed data structure
 *
 * This function reads coentent from a packed data structure where content fields
 * are not aligned to the size of the field.
 *
 * @param[in]   adr    The adress of data field
 *
 * @param[in]   size   Size of the data field.
 *                     This should come from a sizeof(field) of the table definition
 *                     and not be hard coded into the access command.
 *
 * @return      val    Properly aligned data field from from table
 *
 */

LwU32
getData
(
        LwU8 *adr,
        LwU8 size
)
{
    LwU8 i;
    LwU32 val = 0x00000000;
    for (i=0; i<size; i++) {
        LwU8 part = (*adr++);
        val = val + (part << (i*8));
    }
    return val;
}


/*!
 * @brief Force content into a packed data structure
 *
 * This is a debug function that forces a value into a packed data structure
 *
 *
 * @param[in]   adr    The adress of data field
 *
 * @param[in]   size   Size of the data field.
 *                     This should come from a sizeof(field) of the table definition
 *                     and not be hard coded into the access command.
 *
 * @param[in]   data   data to be written into the packed structure
 *
 */

void
forceData
(
        LwU8 *adr,
        LwU8 size,
        LwU32 data
)
{
    LwU8 i;
    LwU32 val = data;
    for (i=0; i<size; i++) {
        (*adr++) = (LwU8)(val & 0xFF);
        val = val >> 8;
    }
}

void
extractData
(
        LwU8 *adr,
        LwU16 size,
        LwU8 *dest
)
{
    LwU16 i;
    LwU8* pDest = (LwU8*) dest;
    for (i=0; i<size; i++) {
        (*pDest++) = (*adr++);
    }
}
