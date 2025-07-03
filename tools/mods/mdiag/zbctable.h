/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef ZBCTABLE_H
#define ZBCTABLE_H

#include "sysspec.h"
#include "core/include/color.h"
#include "mdiag/utils/raster_types.h"
#include "core/include/lwrm.h"
#include "ctrl/ctrl9096.h"

class GpuSubdevice;
class LWGpuResource;

// ZBC stands for Zero-Bandwidth Clears.  It is a hardware feature that
// allows the GPU to clear a compressed ROP tile without FB traffic.
// In order for a GPU clear to use this feature, the value to which
// a ROP tile is being cleared must be one of a subset of values that
// are stored in ZBC tables.  There are three ZBC tables; one for
// color clear values, one for depth clear values, and one for stencil clear
// values.  (Note: pre-Pascal chips do not have a stencil ZBC table.)

// See /hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/FermiFBCompression_FD.doc
// for more information (partilwlarly the section titled Zero-Bandwidth Clears.

// The ZbcTable class represents an interface layer between mdiag and the
// RM ZBC API.  The RM API handles the actual register settings for the ZBC
// tables in hardware.  Because RM does not handle certain ZBC table
// restrictions, the ZbcTable class handles the restrictions instead.
// For example, some depth formats require that the corresponding depth
// value must be in the lowest entries of the ZBC depth table.  RM is
// unaware of the depth formats, so it will put depth values in any
// of the available entries in the table.  Thus the ZbcTable class needs
// to account for this limitation of the RM API.

// There are two primary ways that mdiag adds entries to the ZBC tables.
// The first way mdiag adds entries to the ZBC tables is via command-line
// arguments that specify specific color, depth, or stencil values.
// These values are added independently of one anther.
// The second way mdiag adds entries to the ZBC tables is an automatic
// ZBC entry method.  If the user specifies a compressed, ZBC-enabled
// rendertarget, mdiag will automatically add a three-part ZBC table entry
// (color, depth, stencil) based on the clear values from the trace header.
// These two methods of adding ZBC table entries are mutually exclusive,
// and the first method takes precedence.

// Miscellaneous notes:
//
//   It is illegal for the ZBC depth and stencil tables to have duplicate
//     values.

class ZbcTable
{
public:
    ZbcTable(LWGpuResource *lwgpu, LwRm* pLwRm);
    ~ZbcTable();

    RC AddArgumentEntries(const ArgReader *params, LwRm* pLwRm);

    RC AddColorToZbcTable(GpuSubdevice *gpuSubdevice,
        ColorUtils::Format colorFormat, const RGBAFloat &colorClear, LwRm* pLwRm, bool skipL2Table);
    RC AddDepthToZbcTable(GpuSubdevice *gpuSubdevice,
        ColorUtils::Format zetaFormat, const ZFloat &depthClear, LwRm* pLwRm, bool skipL2Table);
    RC AddStencilToZbcTable(GpuSubdevice *gpuSubdevice,
        ColorUtils::Format zetaFormat, const Stencil &stencilClear, LwRm* pLwRm, bool skipL2Table);

    bool AllowAutoZbc() const { return m_AllowAutoZbc; }

    void ClearTables(GpuSubdevice *gpuSubdevice, LwRm* pLwRm);

    // This function is for debugging only and shouldn't be
    // called in production code.
    RC PrintZbcTable(GpuSubdevice *gpuSubdevice, LwRm* pLwRm);

private:
    // This class is used to allocate the ZBC object and automatically
    // free it when an object of this class goes out of scope.
    class ZbcTableHandle
    {
    public:
        ZbcTableHandle(GpuSubdevice *gpuSubdevice, LwRm* pLwRm);
        ~ZbcTableHandle();
        operator LwRm::Handle() const { return m_Handle; }
    private:
        LwRm* m_pLwRm;
        LwRm::Handle m_Handle;
    };

    void ColwertColor(ColorUtils::Format colorFormat,
        const RGBAFloat &colorClear, UINT32 *colorFB, UINT32 *colorDS,
        UINT32 *clearFormat);

    RC GetTableIndex(const ZbcTableHandle &zbcHandle, LwRm* pLwRm, LW9096_CTRL_ZBC_CLEAR_TABLE_TYPE type,
        UINT32 *indexStart, UINT32 *indexEnd);
    UINT32 GetMaxDepthIndex(ColorUtils::Format zetaFormat);
    UINT32 GetMaxStencilIndex(ColorUtils::Format zetaFormat);
    RC ReadZbcColorTable(const ZbcTableHandle &zbcHandle, LwRm* pLwRm);
    RC ReadZbcDepthTable(const ZbcTableHandle &zbcHandle, LwRm* pLwRm);
    RC ReadZbcStencilTable(const ZbcTableHandle &zbcHandle, LwRm* pLwRm);

    LWGpuResource *m_Lwgpu;
    bool m_AllowAutoZbc;
    map<UINT32, LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS> m_ColorTable;
    map<UINT32, LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS> m_DepthTable;
    map<UINT32, LW9096_CTRL_GET_ZBC_CLEAR_TABLE_ENTRY_PARAMS> m_StencilTable;

    typedef struct
    {
        UINT32 indexStart;
        UINT32 indexEnd;
    } ZbcTableIndex;

    ZbcTableIndex m_ColorTableIndex;
    ZbcTableIndex m_DepthTableIndex;
    ZbcTableIndex m_StencilTableIndex;
};

#endif
