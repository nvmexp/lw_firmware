/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/types.h"

#include "vc1syntax.h"
#include "vc1parser.h"

namespace VC1
{

RC Parser::ParseStream(const UINT08 *buf, size_t bufSize)
{
    RC rc;

    size_t startOffset = 0;
    bool haveMore = true;
    BDU bdu;

    while (haveMore)
    {
        haveMore = bdu.Read(buf, startOffset, bufSize);
        switch (bdu.GetBduType())
        {
            case BDU_TYPE_SEQUENCE:
            {
                BitIArchive<BDU::Iterator> ia(bdu.RBDUBegin(), bdu.RBDUEnd());
                ia >> m_sequenceLayer;

                break;
            }
            case BDU_TYPE_ENTRY_POINT:
            {
                BitIArchive<BDU::Iterator> ia(bdu.RBDUBegin(), bdu.RBDUEnd());
                ia >> m_entryPointLayer;

                break;
            }
            case BDU_TYPE_FRAME:
            {
                PictureLayer pic(&m_sequenceLayer, &m_entryPointLayer);
                BitIArchive<BDU::Iterator> ia(bdu.RBDUBegin(), bdu.RBDUEnd());
                ia >> pic;
                break;
            }
            case BDU_TYPE_FIELD:
            {
                return RC::ILWALID_FILE_FORMAT;
            }
            case BDU_TYPE_END_OF_SEQ:
            case BDU_TYPE_SLICE:
            case BDU_TYPE_SLICE_LVL_USER_DATA:
            case BDU_TYPE_FIELD_LVL_USER_DATA:
            case BDU_TYPE_FRAME_LVL_USER_DATA:
            case BDU_TYPE_ENTRY_POINT_LVL_USER_DATA:
            case BDU_TYPE_SEQ_LVL_USER_DATA:
            {
                // not implemented yet
                break;
            }
        }
        startOffset += bdu.GetEbduDataSize();
    }

    return rc;
}

} // namespace H264
