/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Attention:
//  This is a dummy implementation of chiplibtrace dumper for mfg_mods+mdiag
//  to pass compile. Chiplibtrace dumper depends on both sim_mods and mdiag.
//  So it can't work on mfg_mods+mdiag platform.
//  Once mfg_mods does not depend on mdiag, this null implementation can be
//  removed.
//
#ifndef CHIPLIB_TRACE_CAPTURE_STUB_H
#define CHIPLIB_TRACE_CAPTURE_STUB_H
#include "core/include/color.h"
#include "core/include/rc.h"

class BusMemOp;
class CfgPioOp;
class AnnotationOp;
class ChiplibOpBlock;
class EscapeOp;
class RawMemOp;
class ChiplibTraceDumper;

//--------------------------------------------------------------------
//! \brief Abstract base class to represents a single operation between
//!        MODS and Chiplib.
class ChiplibOp
{
public:
    enum OpType
    {
        OPTYPE_BEGIN = 0,
        OPTYPE_LAST
    };

    ChiplibOp(OpType type) {}
    virtual ~ChiplibOp() {}
};

//--------------------------------------------------------------------
//! \brief To manage chiplibtracedumper global information;
//!
class ChiplibTraceDumper
{
public:
    static ChiplibTraceDumper *GetPtr() { return NULL; }

    ChiplibTraceDumper() {}
    ~ChiplibTraceDumper() {}

    bool DumpChiplibTrace() const { return false; }
    bool RawDumpChiplibTrace() const { return false; }
    void SetRawDumpChiplibTrace() {}
    void SetRawDumpChiplibTraceLevel(UINT32) {}

    bool IsChiplibTraceDumpEnabled() { return false; }
    void EnableChiplibTraceDump() {}
    void DisableChiplibTraceDump() {}

    void EnablePramin() {}
    bool IsPraminEnabled() { return false; }

    ChiplibOpBlock* GetLwrrentChiplibOpBlock() { return NULL; }
    void Push(ChiplibOpBlock *pOpBlock) {}
    void Pop() {}

    RC GetGpuSubdevInfo() { return OK; }

    bool IsBar1DumpEnabled() { return false; }
    void SetBar1DumpFlag(bool enabled) {}
    bool IsSysmemDumpEnabled() { return false; }
    void SetSysmemDumpFlag(bool enabled) {}

    void SetTestNamePath(const string& testName, const string& tracePath) {}
    UINT32 GetTestNum() const { return 0; }

    void SetBackdoorArchiveId(const string& archiveId) {}

    void AddArchiveFile(const std::string& dumpFileName, const std::string& archiveFileName) {}
    RC DumpArchiveFiles() { return OK; }
};

//--------------------------------------------------------------------
//! \brief A class to create a new chiplib operation scope
//!
class ChiplibOpScope
{
public:
    enum ScopeType
    {
        SCOPE_COMMON,
        SCOPE_POLL,
        SCOPE_IRQ,
        SCOPE_CRC_SURF_READ,
        SCOPE_OPTIONAL,
    };

    struct Crc_Surf_Read_Info
    {
        string SurfName;
        string SurfFormat;
        UINT32 SurfPitch;
        UINT32 SurfWidth;
        UINT32 SurfHeight;
        UINT32 SurfBytesPerPixel;
        UINT32 SurfNumLayers;
        UINT32 SurfArraySize;
        UINT32 RectSrcX;
        UINT32 RectSrcY;
        UINT32 RectSrcWidth;
        UINT32 RectSrcHeight;
        UINT32 RectDestX;
        UINT32 RectDestY;
    };

    ChiplibOpScope(const string& scopeInfo, UINT08 irq, ScopeType type, void* extraInfo, bool optional = false) {}
    ~ChiplibOpScope() {}

    void CancelOpBlock() {}
};

class GpuSubdevice;
//--------------------------------------------------------------------
//! \brief A block of ChiplibOps produced in a ChiplibOpScope
//!
class ChiplibOpBlock : public ChiplibOp
{
public:
    ~ChiplibOpBlock() {}

    BusMemOp *AddBusMemOp(const char *typeStr, PHYSADDR addr,
                          const void* data, UINT32 count) { return NULL; }
    AnnotationOp *AddAnnotOp(const char* pLinePrefix, const char* pStr) { return NULL; }
    ChiplibOpBlock *AddBlockOp(OpType type, ChiplibOpScope::ScopeType scopeType,
                               const string &blockDes, UINT32 stackDepth,
                               bool optional = false) { return NULL; }
    EscapeOp *AddEscapeOp(const char *typeStr, UINT08 gpuId, const char *path,
                          UINT32 index, UINT32 size, const void *buf) { return NULL; }
    CfgPioOp *AddCfgPioOp(const char *typeStr, INT32 domainNumber, INT32 busNumber,
                          INT32 deviceNumber, INT32 functionNumber,
                          INT32 address, const void* data, UINT32 count) { return NULL; }
    RawMemOp *AddRawMemDumpOp(const string &memAperture,
                              UINT64 physAddress, UINT32 size,
                              const string &fileName) { return NULL; }
    RawMemOp *AddRawMemLoadOp(const string &memAperture,
                              UINT64 physAddress, UINT32 size,
                              const string &fileName) { return NULL; }
    RawMemOp *AddPhysMemDumpOp(const string &memAperture,
                               UINT64 physAddress, UINT32 size,
                               const string &fileName) { return NULL; }
    RawMemOp *AddPhysMemLoadOp(const string &memAperture,
                               UINT64 physAddress, UINT32 size,
                               const string &fileName) { return NULL; }
};

//--------------------------------------------------------------------
//! \brief Busmem read or write operation
//!
class BusMemOp : public ChiplibOp
{
};

//--------------------------------------------------------------------
//! \brief cfg or io space read or write operation
//!
class CfgPioOp : public ChiplibOp
{
};

//--------------------------------------------------------------------
//! \brief AnnotationOp
//!
class AnnotationOp : public ChiplibOp
{
};

//--------------------------------------------------------------------
//! \brief Escape rd/wr
//!
class EscapeOp : public ChiplibOp
{
};

//--------------------------------------------------------------------
//! \brief Chiplib op for dumping raw memory
//!
class RawMemOp : public ChiplibOp
{
};

#define GPU_READ_PRI_ERROR_MASK  0xFFF00000
#define GPU_READ_PRI_ERROR_CODE  0xBAD00000

#define NON_IRQ 0xFF
#define CHIPLIB_CALLSCOPE ChiplibOpScope newScope(__FUNCTION__, NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL)

void WriteDataToBmp(const UINT08* data, UINT32 width, UINT32 height,
                   ColorUtils::Format format, const std::string& surfName);
#endif
