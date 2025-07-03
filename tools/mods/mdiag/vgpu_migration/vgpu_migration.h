/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef VGPU_MIGRATION_H
#define VGPU_MIGRATION_H

#include <list>
#include "core/include/lwrm.h"
#include "core/include/fileholder.h"

//
// Confluence page for reference:
// https://confluence.lwpu.com/display/ArchMods/MODS+FB+Copy+Implementation
//
#include "mdiag/vgpu_migration/vm_gpu.h"
#include "mdiag/vgpu_migration/vm_stream.h"

// A tag shows up in log prints for finding out related debugging info easily.
#define vGpuMigrationTag        "[vGpu Migration]"

class ArgReader;
class GpuSubdevice;
class LwRm;
class LWGpuResource;
class LWGpuChannel;
class SmcEngine;
class GpuPartition;
class SmcResourceController;

#define FB_COPY_ILW_ADDR        (~0ULL)

namespace MDiagVmScope
{
    // Use MdiagSurf to alloc/map FB mem, or directly map FB mem on BAR1.
    enum AllocType
    {
        // AllocBa1 will map the fb segment directly to BAR1.
        AllocBar1,
        // AllocSurf will use a MdiagSurf object to represent the FB segment.
        AllocSurf
    };
    
    // vGpu migration FB copy Task, abstract base type.
    // Derived concrete type could be StreamTask for save/restore or
    // DmaCopyTask for FB DMA copy.
    class Task
    {
    public:
        class DescCreator;

        // Task descriptor.
        class Desc
        {
        public:
            Desc(DescCreator* pCreator, UINT64 id)
                : m_pCreator(pCreator),
                m_Id(id)
            {
                MASSERT(m_pCreator != nullptr);
                m_pCreator->AddDesc(this);
            }
            virtual ~Desc() {}
            virtual const MemDescs* GetMemDescs() const { return &m_MemDescs; }
            // Most tasks needn't to set up Alloc. 
            virtual void SetupMemAlloc(MemAlloc* pAlloc) {}
            // If has meta data to save or load.
            virtual bool HasMeta() const { return true; }

            // Most tasks needn't local mem data
            virtual const MemData* GetMemData() const { return nullptr; }
            // Most tasks needn't to set data
            virtual void SetMemData(const MemData& data) {}
            // If need to save data to local buffer but not to file or to file only.
            virtual bool HasLocalMemData() const { return false; }

        public:
            MemDescs& GetMemDescsRef() { return m_MemDescs; }
            const MemDescs& GetMemDescsRefConst() const { return m_MemDescs; }
            UINT64 GetId() const { return m_Id; }
            void SetCreator(DescCreator* pCreator)
            {
                MASSERT(pCreator != nullptr);
                m_pCreator = pCreator;
            }
            DescCreator* GetCreator() { return m_pCreator; }

        protected:
            void SetMemDescs(const MemDescs& descs) { m_MemDescs = descs; }
            void QueryFbMemSize(UINT64* pSize);

        private:
            MemDescs m_MemDescs;
            DescCreator* m_pCreator = nullptr;
            const UINT64 m_Id;
        };

        class DescCreator
        {
        public:
            virtual ~DescCreator() {}
            void FreeDesc(UINT64 id);
            void FreeDesc(Desc* pDesc) { FreeDesc(pDesc->GetId()); }
            Desc* GetDesc(UINT64 id);
            void AddDesc(Desc* pDesc);

        private:
            map<UINT64, unique_ptr<Desc> > m_List;
        };

    public:
        virtual ~Task() {}

    public:
        void SetDstTaskDesc(Desc* pDesc);
        Desc* GetDstTaskDesc() { return m_pTaskDescDst; }
        const Desc* GetDstTaskDescConst() const { return m_pTaskDescDst; }

        void SetDstMemAlloc(MemAlloc* pAlloc) { m_pAllocDst.reset(pAlloc); }
        MemAlloc* GetDstMemAlloc() { return m_pAllocDst.get(); }
        MemAlloc* ReleaseDstMemAlloc()
        {
            return m_pAllocDst.release();
        }

    private:
        Desc* m_pTaskDescDst = nullptr;
        unique_ptr<MemAlloc> m_pAllocDst;
    };

    // FB mem stream save and restore Task
    class StreamTask : public Task
    {
    public:
        // Use MdiagSurf to alloc/map FB mem, but not map on BAR1.
        class Surf : public MemAlloc
        {
        public:
            virtual ~Surf() {}
            virtual RC Map();
            virtual RC Unmap();

        public:
            MdiagSurf* GetSurf() { return &m_Surf; }
            const MdiagSurf* GetSurfConst() const { return &m_Surf; }
            void SetMemLocation(const Memory::Location loc) { m_MemLoc = loc; }
            void Print() const;

        protected:
            PHYSADDR GetAllocGpuPa() const;
            UINT64 GetGpuVa() const;
            void* GetCpuVa() const;
            PHYSADDR GetCpuPa() const;
            UINT64 GetSize() const
            {
                MASSERT(GetFbMemSegConst() != nullptr);
                return GetFbMemSegConst()->GetSize();
            }
            void SetNoCpuMap() { m_bNeedCpuMap  = false; }

        private:
            void SetupSurf(const Memory::Location loc);
            RC CreateSurf();
            void Free();
            RC RemapPa();

            MdiagSurf m_Surf;
            Memory::Location m_MemLoc = Memory::Fb;
            bool m_bNeedCpuMap = true;
        };

        typedef list<Surf*> SurfList;

    public:
        StreamTask();
        virtual ~StreamTask() {}

        void SetHexFileNumData(size_t num)
        {
            m_HexFileNumData = num;
        }
        size_t GetHexFileNumData() const
        {
            return m_HexFileNumData;
        }

        void SetAllocType(const AllocType type);

        // Save/Restore requires a file.
        RC SaveMemToFile(const string& file);
        RC RestoreMemFromFile(const string& file);

        // Save/Restore needn't a file, might use local buffer instead.
        RC ReadMem(MemData* pOut);
        RC WriteMem(const MemData& data);
        RC WriteSavedMem();

    private:
        // Use BAR1 to map FB mem, but not use surface.
        class Bar1Alloc : public MemAlloc
        {
        public:
            virtual ~Bar1Alloc() {}

            virtual RC Map();
            virtual RC Unmap();
        };

        MemDescs& GetMemDescsRef()
        {
            MASSERT(GetDstTaskDesc() != nullptr);
            return GetDstTaskDesc()->GetMemDescsRef();
        }
        const MemDescs& GetMemDescsRefConst() const
        {
            MASSERT(GetDstTaskDescConst() != nullptr);
            return GetDstTaskDescConst()->GetMemDescsRefConst();
        }
        RC StartReadFile(const string& file);
        RC StartWriteFile(const string& file);
        RC VerifySrcSize();
        void AddSrcMemDescs()
        {
            GetMemDescsRef().AddSrcDescs(m_SrcMemDescs);
            m_SrcMemDescs.Clear();
        }

        // Prepare for saving data.
        RC Prepare();
        
        MemDescs m_SrcMemDescs;
        FbCopyStream m_Stream;
        FileHolder m_fh;
        size_t m_HexFileNumData;
    };

    // Verify mem data, e.g. compare local buffer to mem data.
    struct DataVerif
    {
    public:
        DataVerif(UINT64 addr, UINT64 size)
        {
            m_Seg.SetAddr(addr);
            m_Seg.SetSize(size);
        }
        DataVerif(const MemSegDesc& seg)
            : m_Seg(seg)
        {}

        // Compare two data buffers to see if matching.
        bool Compare(const void* pSrc, const void* pDst) const;

    private:
        MemSegDesc m_Seg;
    };

    // 1-step DMA Copy:
    // FB mem direct copy from mem to mem without using a file
    // or local buffer for save and then restore purpose.
    //
    // 2-step DMA Copy:
    // FB mem direct copy from mem to mem using a 
    // temp vidmem surface for save and then restore purpose.
    //
    // DMA copy uses MdiagSurf + DMA-channel, or DmaReader
    class CopyTask : public Task
    {
    public:
        virtual ~CopyTask() {}

    public:
        CopyTask();
        void SetSurfList(StreamTask::SurfList* pList) { m_pSurfList = pList; }
        void SetDstMemLocation(Memory::Location loc)
        {
            GetDstSurf()->SetMemLocation(loc);
        }
        void SetSrcTaskDesc(Desc* pDesc);
        Desc* GetSrcTaskDesc() { return m_pTaskDescSrc; }
        // 1-step DMA Copy
        RC Copy();

        // 2-step DMA Copy, save and restore.
        RC Save(bool bSysmem);
        RC Restore();

    private:
        class DmaSurf : public StreamTask::Surf
        {
        public:
            virtual ~DmaSurf() {}
            static DmaSurf* Create()
            {
                DmaSurf* pSurf = new DmaSurf;
                pSurf->Init();
                return pSurf;
            }
            void Init() { SetNoCpuMap(); }

            UINT64 GetCopySize() const { return GetSize(); }
            RC DmaCopy(DmaSurf* pDst);

        private:
            DmaSurf() = default;
            DmaSurf(const DmaSurf& surf) = default;
        };

        void SetSrcMemAlloc(MemAlloc* pAlloc) { m_pAllocSrc.reset(pAlloc); }
        MemAlloc* GetSrcMemAlloc() { return m_pAllocSrc.get(); }

        DmaSurf* GetSrcSurf()
        {
            DmaSurf* pSurf = dynamic_cast<DmaSurf*>(GetSrcMemAlloc());
            MASSERT(pSurf != nullptr);
            return pSurf;
        }
        const DmaSurf* GetSrcSurfConst() const
        {
            const DmaSurf* pSurf = dynamic_cast<const DmaSurf*>(m_pAllocSrc.get());
            MASSERT(pSurf != nullptr);
            return pSurf;
        }
        DmaSurf* GetDstSurf()
        {
            DmaSurf* pSurf = dynamic_cast<DmaSurf*>(GetDstMemAlloc());
            MASSERT(pSurf != nullptr);
            return pSurf;
        }
        UINT64 GetCopySize() const
        {
            return GetSrcSurfConst()->GetCopySize();
        }
        void SaveSurf();
        void LoadSurf();

    private:
        Desc* m_pTaskDescSrc = nullptr;
        unique_ptr<MemAlloc> m_pAllocSrc;
        StreamTask::SurfList* m_pSurfList = nullptr;
    };

    // SRIOV related resources.
    // As the VMMU segments save/restore TaskDescriptor creator as well.
    class SriovResources : public Task::DescCreator
    {
    public:
        // Construct SRIOV VMMU segments related TaskDescriptor.
        class TaskDesc : public Task::Desc
        {
        public:
            TaskDesc(SriovResources* pSriov, UINT32 gfid)
                : Task::Desc(pSriov, gfid)
            {}
            virtual ~TaskDesc() {}
            virtual bool HasMeta() const { return true; }

        public:
            // Figure out required VMMU segments to be copied.
            RC Setup(UINT32 gfid);
        };

    public:
        RC InitVmmuSegs(UINT32 gfid);
        // Get SRIOV VF FB VMMU segments.
        const MemDescs* GetVmmuSegs(UINT32 gfid) const;
        UINT32 GetSwizzId(UINT32 gfid) const;

        TaskDesc* CreateTaskDesc(UINT32 gfid)
        {
            RC rc;
            TaskDesc* pData = new TaskDesc(this, gfid);
            rc = pData->Setup(gfid);
            if (rc != OK)
            {
                pData->GetCreator()->FreeDesc(pData);
                pData = nullptr;
            }
            return pData;
        }

    private:
        void GetVfVmmuSetting(UINT32 gfid, vector<UINT64>* pSetting) const;

        map<UINT32, MemDescs> m_VmmuSegs;
    };

    // SMC related resources.
    // As the SMC partition save/restore TaskDescriptor creator as well.
    class SmcResources : public Task::DescCreator
    {
    public:
        // Construct SMC partition related TaskDescriptor.
        class TaskDesc : public Task::Desc
        {
        public:
            // VM ID could be syspipe# or GFID#.
            TaskDesc(SmcResources* pSmc, UINT32 vmId)
                : Task::Desc(pSmc, vmId)
            {}
            virtual ~TaskDesc() {}
            virtual bool HasMeta() const { return true; }

        public:
            // Figure out SMC partition FB mem.
            // Use syspipe# by default in case SRIOV not enabled.
            RC Setup(UINT32 syspipe);
            // Use GFID# mapping to GpuPartition in case SRIOV enabled.
            RC SetupSriovSmc(UINT32 gfid);
        };

    public:
        void AddSmcEngine(SmcEngine* pEng);
        SmcEngine* GetSmcEngine(UINT32 syspipe) const;
        GpuSubdevice* GetGpuSubdevice(UINT32 syspipe) const;
        LwRm* GetEngineLwRmPtr(UINT32 syspipe) const;
        void AddGpuPartition(GpuPartition* pPart);
        void GetGpuPartitions(vector<GpuPartition*>* pParts) const;
        GpuPartition* GetGpuPartition(UINT32 swizzId) const;

        RC GetPartitionInfo(LwRm* pLwRm, GpuSubdevice* pSubdev, MemSegDesc* pSeg) const;
        RC GetAllPartitionInfo(MemDescs* pMem) const;

        // Create by syspipe# by default.
        TaskDesc* CreateTaskDesc(UINT32 syspipe)
        {
            RC rc;
            TaskDesc* pData = new TaskDesc(this, syspipe);
            rc = pData->Setup(syspipe);
            if (rc != OK)
            {
                pData->GetCreator()->FreeDesc(pData);
                pData = nullptr;
            }
            return pData;
        }
        // Create by GFID# mapping to GpuPartition.
        TaskDesc* CreateTaskDescSriovSmc(UINT32 gfid)
        {
            RC rc;
            TaskDesc* pData = new TaskDesc(this, gfid);
            rc = pData->SetupSriovSmc(gfid);
            if (rc != OK)
            {
                pData->GetCreator()->FreeDesc(pData);
                pData = nullptr;
            }
            return pData;
        }

        void PrintEngines() const;

        // Get first SmcEngine found.
        SmcEngine* GetDefaultSmcEngine() const;
        // Get LwGpu by default SmcEngine/GpuParition, if SMC.
        LWGpuResource* GetDefaultLwGpu() const;
        // Create LwGpuChannel by default SmcEngine/GpuParition, if SMC.
        LWGpuChannel* CreateDefaultChannel() const;

    private:
        SmcResourceController* GetDefaultSmcResCtrl() const;

        map<UINT32, SmcEngine*> m_SmcEngines;
        vector<GpuPartition*> m_GpuPartitions;
    };

    // Runlist related resources.
    // As the Runlist FB mem save/restore TaskDescriptor creator as well.
    class Runlists : public Task::DescCreator
    {
    public:
        class Resources : public Channels::ChRes
        {
        public:
            void GetFbInfo(MemSegDesc* pSeg) const;

        };
        
        // Construct Runlist TaskDescriptor.
        class TaskDesc : public Task::Desc
        {
        public:
            TaskDesc(Runlists* pRunlists, UINT32 engId)
                : Task::Desc(pRunlists, engId)
            {}
            virtual ~TaskDesc() {}
            virtual bool HasMeta() const { return false; }
            virtual void SetupMemAlloc(MemAlloc* pAlloc) const;
            virtual const MemData* GetMemData() const { return &m_Data; }
            virtual void SetMemData(const MemData& data) { m_Data = data; }
            virtual bool HasLocalMemData() const { return true; }

        public:
            // Figure out Runlist to be saved/restored
            void Setup(const Runlists::Resources& res);

        private:
            MemData m_Data;
        };

    public:
        TaskDesc* CreateTaskDesc(const Runlists::Resources& res)
        {
            TaskDesc* pData = new TaskDesc(this, res.GetEngineId());
            pData->Setup(res);
            return pData;
        }

    private:
        // Mapping engine ID to runlist.
        map<UINT32, FbMemSeg> m_Runlists;
    };

    // Shorten access for SriovResources
    SriovResources* GetSriovResources();

    // Shorten access for SmcResources
    SmcResources* GetSmcResources();

    // Shorten access for Runlists
    Runlists* GetRunlists();

} // MDiagVmScope

// MDiagVmObjs: A opaque container to be passed across external data classes or
// modules, e.g. PmActions, while the end-to-end FB copy code knows the
// MDiagVmObjs internal stuff.
struct MDiagVmObjs
{
    MDiagVmScope::Task* m_pTask;

    MDiagVmScope::StreamTask* GetStreamTask()
    {
        MDiagVmScope::StreamTask* pTask =
            dynamic_cast<MDiagVmScope::StreamTask*>(m_pTask);
        MASSERT(pTask != nullptr);
        return pTask;
    }

    MDiagVmScope::CopyTask* GetCopyTask()
    {
        MDiagVmScope::CopyTask* pTask =
            dynamic_cast<MDiagVmScope::CopyTask*>(m_pTask);
        MASSERT(pTask != nullptr);
        return pTask;
    }
};

// All possible vGpu Migration requied resources.
// As a general FB copy TaskDescriptor creator as well.
class MDiagVmResources : public MDiagVmScope::Task::DescCreator
{
public:
    // General FB copy task descriptor.
    class TaskDesc : public MDiagVmScope::Task::Desc
    {
    public:
        TaskDesc(MDiagVmResources* pRes, UINT64 addr)
            : MDiagVmScope::Task::Desc(pRes, addr)
        {}
        virtual ~TaskDesc() {}
        virtual bool HasMeta() const { return m_bHasMeta; }

        virtual bool HasLocalMemData() const { return !m_bUseFile; }
        virtual const MDiagVmScope::MemData* GetMemData() const { return &m_Data; }
        virtual void SetMemData(const MDiagVmScope::MemData& data)
        {
            if (HasLocalMemData())
            {
                m_Data = data;
            }
        }

    public:
        // Save/restore whole FB.
        void SetupWholeFb();
        // Save/restore single segment using file.
        void SetupSingleSeg(const MDiagVmScope::MemSegDesc& seg);
        // Read/write single segment, not use file to save/restore.
        void SetupSegRW(const MDiagVmScope::MemSegDesc& seg);
        // SRIOV+SMC case SMC partition save/restore.
        void SetupSriovSmc(UINT32 gfid);

    private:
        void SetHasMeta(bool bHasMeta) { m_bHasMeta = bHasMeta; }
        void SetUseFile(bool bUseFile) { m_bUseFile = bUseFile; }

        MDiagVmScope::MemData m_Data;
        bool m_bHasMeta = true;
        bool m_bUseFile = false;
    };
    
public:
    TaskDesc* CreateTaskDescWholeFb()
    {
        TaskDesc* pData = new TaskDesc(this, 0);
        pData->SetupWholeFb();
        return pData;
    }
    TaskDesc* CreateTaskDescSingleSeg(const MDiagVmScope::MemSegDesc& seg)
    {
        TaskDesc* pData = new TaskDesc(this, seg.GetAddr());
        pData->SetupSingleSeg(seg);
        return pData;
    }
    TaskDesc* CreateTaskDescSegRW(const MDiagVmScope::MemSegDesc& seg)
    {
        TaskDesc* pData = new TaskDesc(this, seg.GetAddr());
        pData->SetupSegRW(seg);
        return pData;
    }

    MDiagVmScope::Task::Desc* CreateTaskDescByGfid(UINT32 gfid);

    RC Init();
    const ArgReader* GetArgs() const { return m_pArgs.get(); }

    MDiagVmScope::StreamTask::SurfList* GetSurfList() { return &m_SurfList; }
    MDiagVmScope::GpuUtils* GetGpuUtils() { return &m_GpuUtils; }
    MDiagVmScope::SriovResources* GetSriovResources() { return &m_Sriov; }
    MDiagVmScope::SmcResources* GetSmcResources() { return &m_Smc; }
    MDiagVmScope::Runlists* GetRunlists() { return &m_Runlists; }

    MDiagVmScope::MemStream::GlobConfig* GetMemStreamGlobConfig() { return &m_MemStreamCfg; }
    const MDiagVmScope::MemStream::GlobConfig* GetMemStreamGlobConfigConst() const { return &m_MemStreamCfg; }

    GpuPartition* GetGpuPartitionByGfid(UINT32 gfid);
    FLOAT64 GetTimeoutMs() const { return m_TimeoutMs; }

private:
    void SetArgs();

    MDiagVmScope::GpuUtils m_GpuUtils;
    MDiagVmScope::SriovResources m_Sriov;
    MDiagVmScope::SmcResources m_Smc;
    MDiagVmScope::Runlists m_Runlists;
    MDiagVmScope::MemStream::GlobConfig m_MemStreamCfg;
    MDiagVmScope::StreamTask::SurfList m_SurfList;
    unique_ptr<ArgReader> m_pArgs;
    FLOAT64 m_TimeoutMs = 0;
};

// Shorten access for VmResources
MDiagVmResources* GetMDiagVmResources();

class MDiagvGpuMigration
{
public:
    // Need called to check if MODS vGpu Migration functions enabled
    // from command line before call below Init().
    void ParseCmdLineArgs();
    RC Init();
    // Called to enable MODS vGpu Migration functions.
    void Enable();
    void SetBaseRun();
    void SetMigrationRun();
    // This determines if need to init vGpu Migration required resources.
    bool IsEnabled() const { return m_bEnabled; }
    bool IsBaseRun() const;
    bool IsMigrationRun() const;

    MDiagVmResources* GetVmResources() { return m_pVmRes.get(); }
    const MDiagVmResources* GetVmResourcesConst() const { return m_pVmRes.get(); }
    void SetupMemStream(const MDiagVmScope::FbMemSeg& fbSeg, bool bRead, size_t bandwidth);
    void SetFbCopyVerif(bool bVerif)
    {
        Enable();
        GetVmResources()->GetMemStreamGlobConfig()->SetVerif(bVerif);
    }
    bool FbCopyNeedVerif() const
    {
        if (GetVmResourcesConst())
        {
            return GetVmResourcesConst()->GetMemStreamGlobConfigConst()->NeedVerif();
        }
        return false;
    }

    // FB mem save/restore progress prints
    void ConfigFbCopyProgressPrintGran(UINT64 fbLen);
    void PrintFbCopyProgress(UINT64 off) const;

private:

    // The resources object needed only when vGpu Migration functions enabled.
    unique_ptr<MDiagVmResources> m_pVmRes;
    UINT64 m_FbCopyProgressPrintGran = 32ULL << 20;
    bool m_bEnabled = false;
    bool m_bIsBaseRun = false;
    bool m_bIsMigrationRun = false;
};

MDiagvGpuMigration* GetMDiagvGpuMigration();

#endif

