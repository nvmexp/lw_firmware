/*
 * Copyright (c) 2020-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file   context_mem.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2019
 *
 * CCC memory object management.
 */
#ifndef INCLUDED_CONTEXT_MEM_H
#define INCLUDED_CONTEXT_MEM_H

#ifndef CCC_TRACE_CMEM_OPS
#define CCC_TRACE_CMEM_OPS 0
#endif

/* Backwards compatibility: separate DMA and KEY memories forced
 * on by subsystem macro setup.
 *
 * Also possible to define explicitly CCC_WITH_CONTEXT_KEY_MEMORY
 * and CCC_WITH_CONTEXT_DMA_MEMORY settings for require/ignore these.
 */
#if HAVE_CONTEXT_MEMORY
/* If the subsystem defined this => it means it wants to use
 * a separate memory for the KEY allocations.
 *
 * Mandate CMEM KEY buffer in this case as well.
 */
#ifdef GET_KEY_MEMORY
#ifndef CCC_WITH_CONTEXT_KEY_MEMORY
#define CCC_WITH_CONTEXT_KEY_MEMORY 1
#endif
#endif /* GET_KEY_MEMORY */

#ifndef HAVE_CONTEXT_MEM_INIT_CALL
#define HAVE_CONTEXT_MEM_INIT_CALL 0
#endif

/* If the subsystem defined this => it means it wants to use
 * a separate memory for the DMA allocations.
 *
 * Mandate CMEM DMA buffer in this case as well.
 */
#ifdef GET_ALIGNED_DMA_MEMORY
#ifndef CCC_WITH_CONTEXT_DMA_MEMORY
#define CCC_WITH_CONTEXT_DMA_MEMORY 1
#endif
#endif /* GET_ALIGNED_DMA_MEMORY */
#endif /* HAVE_CONTEXT_MEMORY */

#if FORCE_CONTEXT_MEMORY

#if HAVE_CONTEXT_MEMORY == 0
#error "HAVE_CONTEXT_MEMORY must be set for FORCE_CONTEXT_MEMORY"
#endif

#else

/** @def GET_KEY_MEMORY(msize)
 * @brief Request msize byte key memory object from subsystem.
 *
 * Subsystem may override this to e.g. allocate key memory from
 * internal SRAM or for some other reason.
 */
#ifndef GET_KEY_MEMORY
#define GET_KEY_MEMORY(msize)		GET_ZEROED_MEMORY(msize)
#endif

/** @def RELEASE_KEY_MEMORY(kmem, msize)
 * @brief Release key memory requested earlier with GET_KEY_MEMORY.
 *
 * Subsystem may override this to handle different release
 * mechanisms for key memory objects. Size is sizeof_u32(te_crypto_key_t).
 *
 * <kmem,msize> object is already cleared by the caller.
 */
#ifndef RELEASE_KEY_MEMORY
#define RELEASE_KEY_MEMORY(kmem, msize) RELEASE_MEMORY(kmem)
#endif

/* If the system needs to allocate DMA accessed buffers with device
 * memory attributes => the subsystem must redefine these macros to do
 * so. Otherwise they will get aligned with GET_CONTIGUOUS_ZEROED_MEMORY.
 *
 * Other structures/objects in CCC can not be allocated with device memory
 * attibutes because the size alignment is not guaranteed to be correct.
 *
 * If you anyway would do so you may lwrrently get trigger a data
 * abort when CCC in some cases uses struct assignments to copy setup
 * data from client arguments to internal state objects.
 */
#ifndef GET_ALIGNED_DMA_MEMORY
#define GET_ALIGNED_DMA_MEMORY(align,size) GET_CONTIGUOUS_ZEROED_MEMORY(align,size)
#endif

#ifndef RELEASE_ALIGNED_DMA_MEMORY
#define RELEASE_ALIGNED_DMA_MEMORY(addr) RELEASE_CONTIGUOUS_MEMORY(addr)
#endif

#endif /* FORCE_CONTEXT_MEMORY */

#ifndef HAVE_USE_CMEM_RELEASE
/* If a subsystem needs to release a reserved context memory slice explicitly then
 * this can be set in config file. CCC does not use this call internally, this is
 * a client interface call.
 */
#define HAVE_USE_CMEM_RELEASE 0
#endif

#ifndef HAVE_USE_CMEM_FROM_BUFFER
/* Set this in config file if need to colwert memory buffer into CMEM object
 * to for CCC use.
 *
 * If your system does support mempool (or heap) operations then you
 * do not really need this since the object can be obtained from there.
 * If you do not have heap support, you can use the context memory (cmem)
 * supporty in CCC and colwert buffer space to CMEM space with this call.
 *
 * In such scenario without mempool implementation in subsystem,
 * e.g. T19X RSA key load operations need memory for the Montgomery colwersions
 * which use this call.
 */
#define HAVE_USE_CMEM_FROM_BUFFER (HAVE_PKA1_LOAD && FORCE_CONTEXT_MEMORY)
#endif

enum cmtag_e {
	CMTAG_NONE = 0,
	CMTAG_PKA_DATA,
	CMTAG_BUFFER,
	CMTAG_ECONTEXT,
	CMTAG_KOBJECT,
	CMTAG_API_STATE,
	CMTAG_ALIGNED_BUFFER,
	CMTAG_ALIGNED_DMA_BUFFER,
	CMTAG_ALIGNED_CONTEXT,
};

#if HAVE_CONTEXT_MEMORY
#define MEM_SLICE_COUNT_MAX 7

/* 8 bytes per entry */
struct mem_slice_s {
	enum cmtag_e	sli_tag;    /**< Slice allocation tag (CMTAG_NONE: not in use) */
	uint16_t	sli_offset; /**< Slice offset from aligned base */
	uint16_t	sli_len;    /**< Slice length */
};

/* (8U + (MEM_SLICE_COUNT_MAX * 8U))
 * Needs to be 2^x. This size is selected because it is the
 * DMA cache line size for Arm V8. Other systems may have
 * CACHE_LINE size of 32 bytes, which is aligned with this as well.
 *
 * No operations in CCC use more than 7 active memory slices at
 * the same time.
 *
 * Needs to be a power of two now.
 */
#define CMEM_DESCRIPTOR_SIZE 64U

/* Key object size is fixed and only one key object is used lwrrently
 * by any CCC calls.
 *
 * sizeof_u32(te_crypto_key_t) == 1076,
 *  gets rounded up to 64 mult == 1088 and add a descriptor
 */
#define CMEM_KEY_OBJECT_SIZE_MAX 1076U /* sizeof(te_crypto_key_t)
					*
					* But static initializers can not use
					* sizeof().
					*/

/* This is 1152 bytes when key object size is 1076
 * Used in static initializers, can not use sizeof()
 * If this is too small it will get trapped at runtime
 * when requesting key mem slice (if it matters).
 */
#define CMEM_KEY_MEMORY_STATIC_SIZE					\
	(CMEM_DESCRIPTOR_SIZE +						\
	 CMEM_MEM_SIZE_ALIGN(CMEM_KEY_OBJECT_SIZE_MAX))

/* Key object size is fixed (this is 1076 bytes, until key object
 * changed)
 */
#define CMEM_KEY_OBJECT_SIZE sizeof_u32(te_crypto_key_t)

/* Use this when possible, at runtime */
#define CMEM_KEY_MEMORY_SIZE						\
	(CMEM_DESCRIPTOR_SIZE +						\
	 CMEM_MEM_SIZE_ALIGN(CMEM_KEY_OBJECT_SIZE))

/* This is just an estimate for max DMA allocations,
 * for the IMPLICIT context memory reserved for the DMA
 * buffer space.
 *
 * Max is SHA3 DMA is ALIGN(200)=256, about 2 times md buffer and a descriptor,
 * is 576. This is ok for most algos.
 *
 * AES-CCM needs larger due to large work buffer (512), needs 640 bytes.
 * This value is too large for general use in sig verifications
 * so this needs to be decided at runtime.
 */
#define CMEM_DMA_MAX_IMPLICIT_MEMORY_SIZE_LONGEST 640U
#define CMEM_DMA_MAX_IMPLICIT_MEMORY_SIZE	  576U

struct cmem_memory_descriptor_s {
	uint16_t	  d_size;  /**< Lwrrently used */
	uint16_t	  d_max;   /**< Max mem usage */
	uint16_t	  d_free_space; /**< after descriptor to end of cmem */
	uint16_t	  d_entries_used; /**< entries in use */
	struct mem_slice_s d_ent[ MEM_SLICE_COUNT_MAX ];
};

struct cmem_desc_s {
	/* 8 + (MEM_SLICE_COUNT_MAX*8) == 64
	 */
	struct cmem_memory_descriptor_s cde;

	/* The data bytes immediately follow after the CDE field
	 * in all struct cmem_desc_s *ptr referenced objects.
	 *
	 * So the code will access cmem_desc_s objects past the cde
	 * field, this is intentional but diffilwlt to express in a
	 * Misra compatible manner.
	 *
	 * E.g. using flex arrays to access those bytes does not work.
	 */
};

/* CMEM objects sizes are 64 byte multiples (descriptor is 64 bytes) */
#define CMEM_MEM_SIZE_ALIGN(sz)						\
	(((sz) + (CMEM_DESCRIPTOR_SIZE - 1U)) & (~(CMEM_DESCRIPTOR_SIZE-1U)))

/* Round slice size to 64 byte multiple (ceil) */
#define CMEM_SLICE_ALIGN_SIZE(a) CMEM_MEM_SIZE_ALIGN(a)

/* For using aligned memory objects from static memory
 *
 * NOTE: Compiler dependent directive for aligning memory objects.
 */
#define CMEM_MEM_ALIGNED(x) __attribute__((aligned(x)))

/* Largest CMEM buffer size supported by the system.
 *
 * All algorithms need to be able to operate with this CMEM size or
 * less of cmem memory buffer (per algorithm  call instance).
 *
 * In some systems the max size of MEMORY BUFFERS for CMEM may be
 * larger than PAGE_SIZE (e.g. some systems use 1KB pages).
 *
 * Even if the PAGE_SIZE would be smaller than 4096 bytes and the CMEM
 * buffer contains multiple pages they MUST BE contiguous in physical
 * memory when used as CMEM buffer.
 *
 * SUBSYSTEM must ensure this is the case.
 *
 * Value must fit in uint16_t, descriptor offsets and sizes are 16 bit
 * values in current implementation.
 *
 * For any 4096 byte PAGE_SIZE system providing one page per active
 * call is sufficient with max size 4096.
 */
#define CMEM_MAX_SIZE (uint16_t)4096

/* Forward declaration */
struct context_mem_s;

/* Each algorithm class (crypto API classes) defines
 * a layout for the memory provided by caller
 * in the context.
 */
struct cm_layout_s {
	uint32_t spare; /* Unused for now */

	/* these lo sizes and buffers must include CMEM_DESCRIPTOR_SIZE
	 * for the descriptor.
	 */
	uint32_t cm_lo_size;			/**< Required size for generic buffer */
	uint32_t cm_lo_key_size;		/**< Required size for key memory */
	uint32_t cm_lo_dma_size;		/**< Required size for DMA buffers */
};

#if HAVE_CONTEXT_MEM_INIT_CALL
/* Do not export context_memory_init() call unless required,
 * CCC uses context_memory_get_init() instead (it calls init).
 */
status_t context_memory_init(struct context_mem_s *cmem,
			     const struct cm_layout_s *cm);
#endif

bool context_memory_is_used(const struct context_mem_s *cmem);

/* Get cmem object ref or NULL, INITIALIZE the mem descriptors!
 */
status_t context_memory_get_init(struct context_mem_s *chk_mem,
				 struct context_mem_s **cmem_p,
				 const struct cm_layout_s *cm);

/* For colwerting buf to a managed cmem
 */
enum cmem_type_e {
	CMEM_TYPE_NONE,
	CMEM_TYPE_GENERIC, /* Used for all types if no DMA or KEY defined */
	CMEM_TYPE_KEY, /* Used for KEY objects */
	CMEM_TYPE_DMA, /* Used for DMA access buffers */
};

#if HAVE_USE_CMEM_FROM_BUFFER
/* Colwert a linear memory region to a CMEM buffer.
 *
 * This may be needed e.g. by functions which do not
 * know which cmem to request memory from (e.g. no cmem known)
 * in case when memory pool calls are not available.
 * [ If they are, just use the memory pool ]
 *
 * In that case andy buffer can be colwerted into a CMEM by this
 * call and passed down the call chain as a cmem.
 */
status_t cmem_from_buffer(struct context_mem_s *cmem,
			  enum cmem_type_e cmtype,
			  void *buf, uint32_t blen);
#endif /* HAVE_USE_CMEM_FROM_BUFFER */

#if HAVE_USE_CMEM_RELEASE
/* Uses subsystem memory pool for CMEM for alloc/release
 *
 * Do not use these unless your system contains some "heap like"
 * system in any case and you wish to use CCC CMEM internally
 * instead of that heap.
 *
 * *ptr_p  value points to 64 bit aligned memory area,
 * cast it to (uint8_t *) when required.
 *
 * Note: request_id is lwrrently not really used.
 */
status_t cmem_alloc(struct context_mem_s *cmem,
		    enum cmtag_e cmt,
		    uint32_t request_id,
		    void **ptr_p,
		    uint32_t len_arg);

/* Exported function for releasing an allocated CMEM slice
 * with address *ptr
 *
 * If needed by subsystem add HAVE_USE_CMEM_RELEASE to config file.
 */
status_t cmem_release(struct context_mem_s *cmem,
		      enum cmtag_e cmt,
		      const void *ptr);
#endif /* HAVE_USE_CMEM_RELEASE */
#endif /* HAVE_CONTEXT_MEMORY */

/* Get cmem object ref (chk_mem value) or NULL
 */
struct context_mem_s *context_memory_get(struct context_mem_s *chk_mem);

void *cm_tag_mem_get(struct context_mem_s *cmem, enum cmtag_e cmt,
		     uint32_t align, uint32_t msize);

void cm_tag_mem_release(struct context_mem_s *cmem, enum cmtag_e cmt, void *ptr);

#if HAVE_CONTEXT_MEMORY && CCC_TRACE_CMEM_OPS
void *cm_tag_mem_get_fl(struct context_mem_s *cmem, enum cmtag_e cmt,
			uint32_t align, uint32_t msize,
			const char *file, uint32_t line);

void cm_tag_mem_release_fl(struct context_mem_s *cmem, enum cmtag_e cmt,
			   void *p, const char *file, uint32_t line);

#define CMTAG_MEM_GET_BUFFER(cmem, cm_tag, align, bsize)		\
	(uint8_t *)cm_tag_mem_get_fl(cmem, cm_tag, align, bsize, __FILE__, __LINE__)

#define CMTAG_MEM_GET_OBJECT(cmem, cm_tag, align, basetype, ptr_to_basetype) \
	(ptr_to_basetype)cm_tag_mem_get_fl(cmem, cm_tag, align, sizeof_u32(basetype), \
					__FILE__, __LINE__)

#define CMTAG_MEM_GET_TYPED_BUFFER(cmem, cm_tag, align, ptr_to_basetype, size) \
	(ptr_to_basetype)cm_tag_mem_get_fl(cmem, cm_tag, align, size,	\
					__FILE__, __LINE__)

#define CMTAG_MEM_RELEASE(cmem, cm_tag, cm_obj)				\
	do {								\
		cm_tag_mem_release_fl(cmem, cm_tag, cm_obj,		\
				      __FILE__, __LINE__);		\
		(cm_obj) = NULL;					\
	} while (CFALSE)
#else

#define CMTAG_MEM_GET_BUFFER(cmem, cm_tag, align, bsize)	\
	(uint8_t *)cm_tag_mem_get(cmem, cm_tag, align, bsize)

#define CMTAG_MEM_GET_OBJECT(cmem, cm_tag, align, basetype, ptr_to_basetype) \
	(ptr_to_basetype)cm_tag_mem_get(cmem, cm_tag, align, sizeof_u32(basetype))

#define CMTAG_MEM_GET_TYPED_BUFFER(cmem, cm_tag, align, ptr_to_basetype, size) \
	(ptr_to_basetype)cm_tag_mem_get(cmem, cm_tag, align, size)

#define CMTAG_MEM_RELEASE(cmem, cm_tag, cm_obj)				\
	do {								\
		cm_tag_mem_release(cmem, cm_tag, cm_obj);	\
		(cm_obj) = NULL;					\
	} while (CFALSE)
#endif /* HAVE_CONTEXT_MEMORY && CCC_TRACE_CMEM_OPS */

#endif /* INCLUDED_CONTEXT_MEM_H */
