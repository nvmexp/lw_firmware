/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _DMA_LOADER_H_
#define _DMA_LOADER_H_

#include "mdiag/utils/types.h"

class ArgReader;
class TraceChannel;
class MdiagSurf;
class LWGpuChannel;
class RC;

// Load buffer via dma (CE)
class DmaLoader
{
public:
    //! virtual d'tor
    virtual ~DmaLoader();

    //! Factory function to create loader objects
    static DmaLoader* CreateDmaLoader
    (
        const ArgReader *params,
        TraceChannel *channel
    );

    //! Allocates dma notifier and handles
    virtual RC AllocateNotifier(UINT32 subdev) = 0;

    //! Every time we bind a new source buffer, we have to write the data
    //! to a temp surface which is in system
    RC BindSrcBuf(void *srcBuf, size_t size, UINT32 subdev);

    //! Load binded surface data to target surface
    virtual RC LoadSurface(MdiagSurf *dstSurf, UINT64 offset, UINT32 subdev) = 0;

    virtual RC FillSurface(MdiagSurf *dstSurf, UINT64 offset, UINT32 filledValue, UINT64 size, UINT32 subdev) = 0;
    //! Set the max chunk size, by default it's 1M
    void SetChunkSize(UINT32 size) { m_ChunkSize = size; }
    UINT32 GetChunkSize() const { return m_ChunkSize; }

protected:
    //! c'tor
    DmaLoader();
    //! Gets the dma loader class id
    virtual UINT32 GetClassId() const = 0;
    //! Allocates appropriate dma object via RM
    virtual bool AllocateObject();
    //! check if the allocated dma engine matches
    virtual bool ClassMatched(UINT32 classId) = 0;

    TraceChannel *m_TraceChannel;
    LWGpuChannel *m_GpuChannel;
    UINT32        m_DmaObjHandle;
    bool          m_IsNewObjCreated;

    //! dma notifier
    UINT32    m_NotifierHandle;
    uintptr_t m_NotifierBase;
    UINT64    m_NotifierGpuOffset;

    //! temp surface to hold the data
    MdiagSurf *m_SrcSurf;

    //! class number for the subch
    UINT32 m_Class;
    //! max chunk size for each launch, tesla only supports 1M at most
    UINT32 m_ChunkSize;

    const ArgReader* params;

private:
    //! Prevent object copying
    DmaLoader(const DmaLoader&);
    DmaLoader& operator=(const DmaLoader&);
};

class DmaLoaderGF100 : public DmaLoader
{
public:
    DmaLoaderGF100();
    ~DmaLoaderGF100();

    virtual UINT32 GetClassId() const;
    virtual bool ClassMatched(UINT32 classId);
    virtual RC AllocateNotifier(UINT32 subdev);
    RC LoadSurface(MdiagSurf *dstSurf, UINT64 offset, UINT32 subdev);
    RC FillSurface(MdiagSurf *dstSurf, UINT64 offset, UINT32 filledValue, UINT64 size, UINT32 subdev);
    virtual RC SendDstBasicKindMethod(MdiagSurf *dstSurf);

private:
    //! Prevent copying
    DmaLoaderGF100(const DmaLoaderGF100&);
    DmaLoaderGF100& operator=(const DmaLoaderGF100&);
};

class DmaLoaderVolta : public DmaLoaderGF100
{
public:
    DmaLoaderVolta();
    ~DmaLoaderVolta();

    virtual RC SendDstBasicKindMethod(MdiagSurf *dstSurf);

private:
    //! Prevent copying
    DmaLoaderVolta(const DmaLoaderVolta&);
    DmaLoaderVolta& operator=(const DmaLoaderVolta&);
};

#endif
