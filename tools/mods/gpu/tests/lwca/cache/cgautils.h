/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * Mostly copied from https://gitlab-master.lwpu.com/dlarch-fastkernels/cask_sdk
 * xmma/include/xmma/utils.h and xmma/include/xmma/arrive_wait.h
 */

namespace CgaUtils
{

////////////////////////////////////////////////////////////////////////////////////////////////////
//  CGA related functions
//
extern "C" __device__ UINT32 __lwvm_get_smem_pointer(void* ptr);
inline __device__ UINT32 get_smem_pointer(const void* ptr)
{
    return __lwvm_get_smem_pointer(const_cast<void*>(ptr));
}

inline __device__ UINT64 get_dsmem_ptr(void* addr, const INT32 cta_id)
{
    return (reinterpret_cast<UINT64>(addr) & 0xFFFFFFFF00FFFFFFULL) | (cta_id << 24);
}

inline __device__ void cga_sync()
{
    asm volatile("barrier.cluster.sync;\n" ::);
}

inline __device__ void cga_arrive()
{
    asm volatile("barrier.cluster.arrive.aligned;\n" ::);
}

inline __device__ void cga_wait()
{
    asm volatile("barrier.cluster.wait.aligned;\n" ::);
}

inline __device__ dim3 cga_grid_dims()
{
    UINT32 x, y, z;
    asm volatile("mov.u32 %0, %nclusterid.x;\n" : "=r"(x) :);
    asm volatile("mov.u32 %0, %nclusterid.y;\n" : "=r"(y) :);
    asm volatile("mov.u32 %0, %nclusterid.z;\n" : "=r"(z) :);
    return {x, y, z};
}

inline __device__ dim3 cga_id_in_grid()
{
    UINT32 x, y, z;
    asm volatile("mov.u32 %0, %clusterid.x;\n" : "=r"(x) :);
    asm volatile("mov.u32 %0, %clusterid.y;\n" : "=r"(y) :);
    asm volatile("mov.u32 %0, %clusterid.z;\n" : "=r"(z) :);
    return {x, y, z};
}

inline __device__ dim3 ctaid_in_cga()
{
    UINT32 x, y, z;
    asm volatile("mov.u32 %0, %cluster_ctaid.x;\n" : "=r"(x) :);
    asm volatile("mov.u32 %0, %cluster_ctaid.y;\n" : "=r"(y) :);
    asm volatile("mov.u32 %0, %cluster_ctaid.z;\n" : "=r"(z) :);
    return {x, y, z};
}

inline __device__ dim3 cga_shape()
{
    UINT32 x, y, z;
    asm volatile("mov.u32 %0, %cluster_nctaid.x;\n" : "=r"(x) :);
    asm volatile("mov.u32 %0, %cluster_nctaid.y;\n" : "=r"(y) :);
    asm volatile("mov.u32 %0, %cluster_nctaid.z;\n" : "=r"(z) :);
    return {x, y, z};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Barrier (arrive-wait) related functions
//
struct mbarrier
{
    public:
        inline __device__ mbarrier(UINT64* bar_base)
        {
            m_BarBase = bar_base;
        }

        inline __device__ void bar_create(const UINT32 id, INT32 init_count)
        {
            UINT64* bar_ptr = m_BarBase + id;
            UINT32 smem_ptr = get_smem_pointer(bar_ptr);
            asm volatile("{\n\t"
                         "mbarrier.init.shared.b64 [%1], %0; \n\t"
                         "}"
                         ::"r"(init_count), "r"(smem_ptr));
        }

        inline __device__ void bar_wait(const INT32 id, const UINT64 state)
        {
            asm volatile(".pragma \"set knob DontInsertYield\";\n" : : : "memory");
            UINT64* bar_ptr = m_BarBase + id;
            UINT32 smem_ptr = get_smem_pointer(bar_ptr);
            asm volatile("{\n\t"
                         ".reg .pred                P1; \n\t"
                         "LAB_WAIT: \n\t"
                         "mbarrier.try_wait.shared.b64 P1, [%0], %1; \n\t"
                         "@P1                       bra.uni DONE; \n\t"
                         "bra.uni                   LAB_WAIT; \n\t"
                         "DONE: \n\t"
                         "}"
                         :: "r"(smem_ptr), "l"(state));
            asm volatile(".pragma \"reset knob DontInsertYield\";\n" : : : "memory");
        }

        inline __device__ void bar_wait(const INT32 id, const bool phase)
        {
            asm volatile(".pragma \"set knob DontInsertYield\";\n" : : : "memory");
            UINT64* bar_ptr = m_BarBase + id;
            UINT32 smem_ptr = get_smem_pointer(bar_ptr);
            asm volatile("{\n\t"
                         ".reg .pred                P1; \n\t"
                         "LAB_WAIT: \n\t"
                         "mbarrier.try_wait.parity.shared.b64 P1, [%0], %1; \n\t"
                         "@P1                       bra.uni DONE; \n\t"
                         "bra.uni                   LAB_WAIT; \n\t"
                         "DONE: \n\t"
                         "}"
                         :: "r"(smem_ptr), "r"(static_cast<UINT32>(phase)));
            asm volatile(".pragma \"reset knob DontInsertYield\";\n" : : : "memory");
        }

        // Sends barrier arrive notification to DSMEM
        // Note this uses a slightly different syntax compared to normal arrive
        // NOTE : Caller has to ensure that set_bar_base_dsmem has been called prior to using this
        // This is done as a compiler optimizations (since set barrier base is independent)
        inline __device__ UINT64 bar_arrive_dsmem(const INT32 id, const INT32 cta_id)
        {
            // TODO use setctarank?
            UINT64* bar_ptr = reinterpret_cast<UINT64*>(get_dsmem_ptr(m_BarBase + id, cta_id));
            UINT64 state = 0;
            asm volatile("{\n\t"
                         "mbarrier.arrive.b64 %0, [%1];\n\t"
                         "}"
                         : "=l"(state) : "l"(bar_ptr));
            return state;
        }
    private:
        // smem barrier base pointer
        UINT64* m_BarBase = nullptr;
};

}
