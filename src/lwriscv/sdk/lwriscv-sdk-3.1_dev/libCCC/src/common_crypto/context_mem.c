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
#include <crypto_system_config.h>

#include <crypto_ops.h>
#include <context_mem.h>
#include <ccc_safe_arith.h>

#if HAVE_CONTEXT_MEMORY
#include <crypto_context.h>
#endif

#ifndef CCC_TRACE_CMEM_USAGE
#define CCC_TRACE_CMEM_USAGE  0
#endif

#ifndef LOCAL_TRACE
#if defined(MODULE_TRACE)
#define LOCAL_TRACE MODULE_TRACE
#else
/* Special conditioning of CMEM tracing */
#define LOCAL_TRACE CCC_TRACE_CMEM_OPS
#endif
#endif

#if CCC_TRACE_CMEM_OPS || CCC_TRACE_CMEM_USAGE
#define CMEM_TRACE_LOG LOG_ERROR
#else
#define CMEM_TRACE_LOG(...)
#endif

/*
 * In CMEM (context memory) all objects are
 * aligned to CMEM_DECRIPTOR_SIZE (64 bytes).
 */
#define IS_CMEM_ALIGNED(value) ((((uintptr_t)(value)) & (CMEM_DESCRIPTOR_SIZE - 1U)) == 0U)

#if FORCE_CONTEXT_MEMORY == 0

#ifndef CCC_MINIMUM_MEMPOOL_BYTE_ALIGN
/* This needs to be power of two:
 *  N-1 mask is used for checking objects.
 */
#define CCC_MINIMUM_MEMPOOL_BYTE_ALIGN BYTES_IN_WORD
#endif

static void *cm_tag_mem_get_mempool(enum cmtag_e cmt,
				    uint32_t align_arg,
				    uint32_t msize)
{
	void *p = NULL;
	uint32_t align = align_arg;

	LTRACEF("entry\n");

	/* Minimum alignment, for all objects
	 */
	if (align < CCC_MINIMUM_MEMPOOL_BYTE_ALIGN) {
		align = CCC_MINIMUM_MEMPOOL_BYTE_ALIGN;
	}

	switch (cmt) {
	case CMTAG_KOBJECT:
		p = GET_KEY_MEMORY(sizeof_u32(te_crypto_key_t));
		break;

	case CMTAG_API_STATE:
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		/* use separate DMA buffer */
		p = GET_ZEROED_MEMORY(msize);
#else
		/* use contiguous caller aligned (This should normally be
		 * CACHE_LINE aligned)
		 */
		p = GET_CONTIGUOUS_ZEROED_MEMORY(align, msize);
#endif
		break;

	case CMTAG_PKA_DATA:
	case CMTAG_BUFFER:
	case CMTAG_ECONTEXT:
		p = GET_ZEROED_MEMORY(msize);
		break;

	case CMTAG_ALIGNED_DMA_BUFFER:
		p = GET_ALIGNED_DMA_MEMORY(align, msize);
		break;

	case CMTAG_ALIGNED_CONTEXT:
	case CMTAG_ALIGNED_BUFFER:
		p = GET_CONTIGUOUS_ZEROED_MEMORY(align, msize);
		break;

	default:
		/* Nothing, p stays NULL as an error */
		break;
	}

	LTRACEF("exit\n");
	return p;
}

static void cm_tag_mem_release_mempool(enum cmtag_e cmt, void *p)
{
	LTRACEF("entry\n");

	switch (cmt) {
	case CMTAG_BUFFER:
	case CMTAG_ECONTEXT:
	case CMTAG_PKA_DATA:
		RELEASE_MEMORY(p);
		break;

	case CMTAG_API_STATE:
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		RELEASE_MEMORY(p);
#else
		RELEASE_CONTIGUOUS_MEMORY(p);
#endif
		break;

	case CMTAG_KOBJECT:
		RELEASE_KEY_MEMORY(p, sizeof_u32(te_crypto_key_t));
		break;

	case CMTAG_ALIGNED_DMA_BUFFER:
		RELEASE_ALIGNED_DMA_MEMORY(p);
		break;

	case CMTAG_ALIGNED_CONTEXT:
	case CMTAG_ALIGNED_BUFFER:
		RELEASE_CONTIGUOUS_MEMORY(p);
		break;

	default:
		/* nothing */
		break;
	}
	LTRACEF("exit\n");
}
#endif /* FORCE_CONTEXT_MEMORY == 0 */

#if HAVE_CONTEXT_MEMORY

#if LOCAL_TRACE || CCC_TRACE_CMEM_OPS

static const struct cname_s {
	enum cmtag_e ct;
	const char *ctn;
} cns[] = {
	{ CMTAG_PKA_DATA, "CMTAG_PKA_DATA" },
	{ CMTAG_BUFFER, "CMTAG_BUFFER" },
	{ CMTAG_ECONTEXT, "CMTAG_ECONTEXT" },
	{ CMTAG_KOBJECT, "CMTAG_KOBJECT" },
	{ CMTAG_API_STATE, "CMTAG_API_STATE" }, /* includes CONTEXT object */
	{ CMTAG_ALIGNED_BUFFER, "CMTAG_ALIGNED_BUFFER" },
	{ CMTAG_ALIGNED_DMA_BUFFER, "CMTAG_ALIGNED_DMA_BUFFER" },
	{ CMTAG_ALIGNED_CONTEXT, "CMTAG_ALIGNED_CONTEXT" },

	{ CMTAG_NONE, "CMTAG_NONE" },
};

static const char *get_cmname(enum cmtag_e ctin)
{
	uint32_t inx = 0U;
	const char *rv = "CMTAG not found";

	while (true) {
		if (cns[inx].ct == ctin) {
			rv = cns[inx].ctn;
			break;
		}

		if (cns[inx].ct == CMTAG_NONE) {
			break;
		}
		inx++;
	}
	return rv;
}

#define CMEM_GET_CMNAME(cmt) get_cmname(cmt)

#endif /* LOCAL_TRACE || CCC_TRACE_CMEM_OPS */

#ifndef CMEM_GET_CMNAME
#define CMEM_GET_CMNAME(cmt) ""
#endif

#if CCC_TRACE_CMEM_OPS

static void trace_dump_descriptor(const struct cmem_desc_s *mdesc)
{
	LTRACEF("entry\n");

	if (NULL == mdesc) {
		CMEM_TRACE_LOG("D-DESC: NULL\n");
	} else {
		uint32_t inx = 0U;

		CMEM_TRACE_LOG("D-DESC(%p): s:%u, mx:%u, fr:%u, ent:%u\n",
			       mdesc,
			       mdesc->cde.d_size, mdesc->cde.d_max,
			       mdesc->cde.d_free_space, mdesc->cde.d_entries_used);

		for (inx = 0U; inx < MEM_SLICE_COUNT_MAX; inx++) {
			CMEM_TRACE_LOG(" => S(%u) %s, [o:%u, l:%u]\n",
				       inx,
				       CMEM_GET_CMNAME(mdesc->cde.d_ent[inx].sli_tag),
				       mdesc->cde.d_ent[inx].sli_offset,
				       mdesc->cde.d_ent[inx].sli_len);
		}
	}

	LTRACEF("exit\n");
}

void *cm_tag_mem_get_fl(struct context_mem_s *cmem, enum cmtag_e cmt,
			uint32_t align, uint32_t msize,
			const char *file, uint32_t line)
{
	void *p = cm_tag_mem_get(cmem, cmt, align, msize);
#if CCC_SOC_FAMILY_ON(CCC_SOC_ON_VDK)
	CMEM_TRACE_LOG("Alloc(%s)\n", CMEM_GET_CMNAME(cmt));
	CMEM_TRACE_LOG("=> %p s:%u (0x%x) a:0x%x\n",
		  p, msize, msize, align);
	CMEM_TRACE_LOG("@[%s:%u]\n", file, line);
#else
	CMEM_TRACE_LOG("Alloc(%s) => %p s:%u (0x%x) a:0x%x [%s:%u]\n",
		  CMEM_GET_CMNAME(cmt),
		  p, msize, msize, align,
		  file, line);
#endif
	return p;
}

void cm_tag_mem_release_fl(struct context_mem_s *cmem, enum cmtag_e cmt,
			   void *p, const char *file, uint32_t line)
{
#if CCC_SOC_FAMILY_ON(CCC_SOC_ON_VDK)
	CMEM_TRACE_LOG("Release(%s)\n", CMEM_GET_CMNAME(cmt));
	CMEM_TRACE_LOG("=> %p @[%s:%u]\n", p, file, line);
#else
	CMEM_TRACE_LOG("Release(%s) => %p [%s:%u]\n",
		  CMEM_GET_CMNAME(cmt), p, file, line);
#endif
	cm_tag_mem_release(cmem, cmt, p);
}
#endif /* CCC_TRACE_CMEM_OPS */

#if CCC_TRACE_CMEM_OPS || CCC_TRACE_CMEM_USAGE
static void cmem_descriptor_mem_use(struct cmem_desc_s *mdesc)
{
	uint32_t max = mdesc->cde.d_max + CMEM_DESCRIPTOR_SIZE;

	CMEM_TRACE_LOG("DESC(%p) usage: current %u, total 0x%x (%u), free %u\n",
		       mdesc, mdesc->cde.d_size, mdesc->cde.d_max,
		       mdesc->cde.d_max, mdesc->cde.d_free_space);
	CMEM_TRACE_LOG("Cmem usage: total (incl descriptor) ==> 0x%x (%u)\n",
		       max, max);
}
#endif

static uint32_t cmem_memtype_slices(enum cmtag_e cmt)
{
	uint32_t slices = 0U;

	LTRACEF("entry\n");

	switch (cmt) {
	case CMTAG_PKA_DATA:
	case CMTAG_BUFFER:
	case CMTAG_ECONTEXT:
	case CMTAG_API_STATE:
	case CMTAG_ALIGNED_BUFFER:
	case CMTAG_ALIGNED_CONTEXT:
		slices = MEM_SLICE_COUNT_MAX;
		break;

	case CMTAG_ALIGNED_DMA_BUFFER:
#if CCC_WITH_CONTEXT_DMA_MEMORY
		slices = MEM_SLICE_COUNT_MAX; /* likely less is ok */
#else
		slices = MEM_SLICE_COUNT_MAX;
#endif
		break;

	case CMTAG_KOBJECT:
#if CCC_WITH_CONTEXT_KEY_MEMORY
		// Only one key object per call allocated
		slices = 1U;
#else
		slices = MEM_SLICE_COUNT_MAX;
#endif
		break;

	default:
		slices = 0U;
		break;
	}

	LTRACEF("exit\n");

	return slices;
}

/* Released slice [rev], add the current len to free space
 * Set slice [rev] length 0.
 */
static status_t cmem_gc_slice_to_free_space(struct cmem_desc_s *mdesc,
					    uint32_t rev)
{
	status_t ret = NO_ERROR;
	uint32_t free_len = mdesc->cde.d_free_space;
	uint32_t sli_len = mdesc->cde.d_ent[rev].sli_len;

	LTRACEF("entry\n");

	if ((free_len > CMEM_MAX_SIZE) ||
	    (sli_len > CMEM_MAX_SIZE)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid sizes in gc\n"));
	}

	free_len = free_len + sli_len;

	if (free_len > 0xFFFFU) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}
	mdesc->cde.d_free_space = (uint16_t)(free_len & 0xFFFFU);

	mdesc->cde.d_ent[rev].sli_len = 0U;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmem_gc_slices(struct cmem_desc_s *mdesc,
			       uint32_t memtype_max_slices)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	uint32_t rev = 0U;

	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	CCC_LOOP_BEGIN {
		if (inx >= memtype_max_slices) {
			CCC_LOOP_STOP;
		}

		rev = memtype_max_slices - inx -  1U;

		if (CMTAG_NONE != mdesc->cde.d_ent[rev].sli_tag) {
			break;
		}

		if (0U != mdesc->cde.d_ent[rev].sli_offset) {
			LTRACEF(">>> CMEM slice %u gc [o: 0x%x, s:%d]\n",
				rev, mdesc->cde.d_ent[rev].sli_offset,
				mdesc->cde.d_ent[rev].sli_len);

			if (0U == rev) {
#if CCC_TRACE_CMEM_OPS || CCC_TRACE_CMEM_USAGE
				cmem_descriptor_mem_use(mdesc);
#endif
				if (0U != mdesc->cde.d_size) {
					CCC_ERROR_END_LOOP_WITH_ECODE(ERR_BAD_STATE,
								      LTRACEF("CMEM memory corruption\n"));
				}
			}
		}
		mdesc->cde.d_ent[rev].sli_offset = 0U;

		ret = cmem_gc_slice_to_free_space(mdesc, rev);
		CCC_ERROR_END_LOOP(ret);

		inx++;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t release_slice_get_offset16(const struct cmem_desc_s *mdesc,
					   const void *ptr,
					   uint16_t *offset16_p)
{
	status_t ret = NO_ERROR;

	uintptr_t mdesc_val = (uintptr_t)mdesc;
	uintptr_t ptr_val = (uintptr_t)(const uint8_t *)ptr;
	uintptr_t diff = (uintptr_t)0;
	uint16_t offset16 = 0U;

	LTRACEF("entry\n");

	if ((NULL == mdesc) || (NULL == ptr)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ptr_val <= mdesc_val) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Data not after descriptor, p(%p) <= m(%p)\n",
					     ptr, mdesc));
	}

	diff = ptr_val - mdesc_val;

	/* Data objects must be after descriptor */
	if (diff < CMEM_DESCRIPTOR_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Data not after descriptor, p(%p) <= m(%p)\n",
					     ptr, mdesc));
	}

	/* Data objects must be inside CMEM (data size not known here)
	 * so compare just start pointer.
	 */
	if (diff > CMEM_MAX_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Out of range pointer: %p (mdesc %p)\n",
					     ptr, mdesc));
	}

	offset16 = (uint16_t)(ptr_val - mdesc_val);

	*offset16_p = offset16;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmem_release_slice(struct cmem_desc_s *mdesc,
				   enum cmtag_e cmt,
				   const void *ptr)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	const uint32_t memtype_max_slices = cmem_memtype_slices(cmt);
	uint16_t offset16 = 0U;

	LTRACEF("entry\n");

	ret = release_slice_get_offset16(mdesc, ptr, &offset16);
	CCC_ERROR_CHECK(ret);

	for (; inx < memtype_max_slices; inx++) {
		if (CMTAG_NONE == mdesc->cde.d_ent[inx].sli_tag) {
			continue;
		}

		if (mdesc->cde.d_ent[inx].sli_offset == offset16) {
			break;
		}
	}

	if (memtype_max_slices == inx) {
		/* Pointer not allocated from this CMEM */
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("<<< NOT a CMEM REF: %p (mdesc %p)\n",
					     ptr, mdesc));
	}

	LTRACEF("CMEM release %s slice[%u]\n",
		CMEM_GET_CMNAME(mdesc->cde.d_ent[inx].sli_tag), inx);
	LTRACEF(" => desc %p [o: 0x%x, s: %d]\n",
		mdesc, offset16, mdesc->cde.d_ent[inx].sli_len);

	mdesc->cde.d_ent[inx].sli_tag = CMTAG_NONE;
	{
		uint32_t size = 0U;
		uint32_t d_size = mdesc->cde.d_size;
		uint32_t slice_len = mdesc->cde.d_ent[inx].sli_len;

		if (d_size < slice_len) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
		}

		size = d_size - slice_len;
		mdesc->cde.d_size = (uint16_t)(size & 0xFFFFU);
	}

	if (0U == mdesc->cde.d_entries_used) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* number of slices in use from this context mdesc
	 * This is not an "index" it is just "number of used".
	 */
	mdesc->cde.d_entries_used--;

	/* "Garbage collect" CMEM slices
	 */
	ret = cmem_gc_slices(mdesc, memtype_max_slices);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmem_slice_at_end(struct cmem_desc_s *mdesc,
				  uint16_t len16,
				  uint32_t inx)
{
	status_t ret = NO_ERROR;
	uint32_t tmp  = 0U;

	/* Enough free space at the end?
	 */
	if (mdesc->cde.d_free_space < len16) {
#if CCC_TRACE_CMEM_OPS
		trace_dump_descriptor(mdesc);
#endif
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LTRACEF("CMEM %p: no memory, areq=%u (free %u)\n",
					     mdesc, len16,
					     mdesc->cde.d_free_space));
	}

	/* Yes, this slice size now set, decrease free space at end
	 */
	mdesc->cde.d_ent[inx].sli_len = len16;

	tmp = mdesc->cde.d_free_space;
	tmp = tmp - len16;

	mdesc->cde.d_free_space = (uint16_t)(tmp & 0xFFFFU);

	if (mdesc->cde.d_size >= CMEM_MAX_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Track object request sizes for this context */
	tmp = mdesc->cde.d_size;
	tmp = tmp + len16;

	if (tmp > 0xFFFFU) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}
	mdesc->cde.d_size = (uint16_t)(tmp & 0xFFFFU);

	if (mdesc->cde.d_size > mdesc->cde.d_max) {
		mdesc->cde.d_max = mdesc->cde.d_size;
	}

fail:
	return ret;
}

static bool cmem_use_this_slice(const struct cmem_desc_s *mdesc,
				uint32_t inx,
				uint16_t len16)
{
	bool slice_select = false;

	if (CMTAG_NONE == mdesc->cde.d_ent[inx].sli_tag) {
		/* free slice found. Big enough? */
		if ((mdesc->cde.d_ent[inx].sli_len > len16) ||
		    (mdesc->cde.d_ent[inx].sli_len == (uint16_t)0U)) {
			/* This slice is big enough (was used before
			 * when sli_len is nonzero).
			 *
			 * If the slice is after all used slices (or
			 * the first slice) size gets checked by caller.
			 */
			LTRACEF("CMEM reserving slice %u @ %p\n",
				inx, mdesc);
			slice_select = true;
		}
	}
	return slice_select;
}

static status_t cmem_slice_find_free(const struct cmem_desc_s *mdesc,
				     uint32_t memtype_max_slices,
				     const uint16_t len16,
				     uint32_t *inx_p,
				     uint32_t *prev_slice_end_p)
{
	status_t ret = NO_ERROR;
	/* First slice starts after fixed size cmem descriptor */
	uint32_t prev_offset = (uint16_t)CMEM_DESCRIPTOR_SIZE;
	uint32_t prev_len = 0U;
	uint32_t inx = 0U;

	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	CCC_LOOP_BEGIN {
		if (inx >= memtype_max_slices) {
			break;
		}

		if (BOOL_IS_TRUE(cmem_use_this_slice(mdesc, inx, len16))) {
			CCC_LOOP_STOP;
		}

		/* Need to look at next slice... */
		prev_offset = mdesc->cde.d_ent[inx].sli_offset;
		prev_len    = mdesc->cde.d_ent[inx].sli_len;

		if ((prev_offset >= CMEM_MAX_SIZE) ||
		    (prev_len >= CMEM_MAX_SIZE)) {
#if CCC_TRACE_CMEM_OPS
			trace_dump_descriptor(mdesc);
#endif
			CCC_ERROR_END_LOOP_WITH_ECODE(ERR_BAD_STATE,
						      LTRACEF("inconsistent slice %u info\n",
							      inx));
		}

		inx++;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

	if (memtype_max_slices == inx) {
#if CCC_TRACE_CMEM_OPS
		trace_dump_descriptor(mdesc);
#endif
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LTRACEF("no memory, slices fragmented\n"));
	}

	*prev_slice_end_p = prev_offset + prev_len;
	*inx_p = inx;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmem_select_slice(struct cmem_desc_s *mdesc,
				  enum cmtag_e cmt,
				  uint16_t len16,
				  uint8_t **ptr_p)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	const uint32_t memtype_max_slices = cmem_memtype_slices(cmt);

	uint32_t tmp = 0U;
	uint8_t *ptr = NULL;
	uint8_t *ptr_cde_bytes = (uint8_t *)(&mdesc->cde);
	uint32_t prev_slice_end = 0U;

	*ptr_p = NULL;

	if (memtype_max_slices == mdesc->cde.d_entries_used) {
#if CCC_TRACE_CMEM_OPS
		trace_dump_descriptor(mdesc);
#endif
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LTRACEF("***** CMEM out of memory, no free slices ******\n"));
	}

	/* Lookup next free slot */
	ret = cmem_slice_find_free(mdesc, memtype_max_slices,
				   len16, &inx, &prev_slice_end);
	CCC_ERROR_CHECK(ret);

	/* If sli_len is not zero, this slice was unused and selected above,
	 * since it was free and large enough to hold the object.
	 *
	 * If zero => no slice was selected, use slice at inx (current
	 * last slice at end)
	 */
	if (0U == mdesc->cde.d_ent[inx].sli_len) {
		ret = cmem_slice_at_end(mdesc, len16, inx);
		CCC_ERROR_CHECK(ret);
	}

	/* Slice selected is at [inx], it is large enough (or bigger)
	 * that is needed for this request.
	 *
	 * Start offset at end of previous slice.
	 */
	mdesc->cde.d_ent[inx].sli_tag = cmt;

	if (prev_slice_end > 0xFFFFU) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
				     LTRACEF("Slice offset over 16 bits\n"));
	}
	mdesc->cde.d_ent[inx].sli_offset = (uint16_t)(prev_slice_end & 0xFFFFU);

	/* Here ptr_cde_bytes points to the begining of the context
	 * memory desciptor.
	 *
	 * The related context memory bytes managed with the
	 * descriptor are right after the cde object, at address
	 * &ptr_cde_bytes[CMEM_DESCRIPTOR_SIZE].
	 *
	 * CMEM_DESCRIPTOR_SIZE is the size of the mdesc->cde field.
	 *
	 * The managed CMEM buffer bytes always follow immediately
	 * after the cde descriptor field in *mdesc.
	 *
	 * First data slice immediately follows the descriptor (which
	 * is 64 byte aligned by address and size, like all data
	 * slices).
	 *
	 * For first data slice the sli_offset was set to
	 * CMEM_DESCRIPTOR_SIZE above with prev_slice_end value.
	 *
	 * For the subsequent data slices the sli_offset was set to
	 * the first byte after the last reserved slice bytes,
	 * i.e. they are always adjacent to previous reserved slice.
	 *
	 * Since all slices are 64 byte aligned in size, all slices
	 * are 64 byte aligned by address.
	 */
	ptr = &ptr_cde_bytes[mdesc->cde.d_ent[inx].sli_offset];

	/* Clear the slice before using it.
	 */
	tmp = mdesc->cde.d_ent[inx].sli_len;
	se_util_mem_set(ptr, 0U, tmp);

	LTRACEF("CMEM get %s slice[%u]\n",
		CMEM_GET_CMNAME(cmt), inx);
	LTRACEF(" =>  desc(%p) -> %p [o: 0x%x, s: %d])\n",
		mdesc, ptr, mdesc->cde.d_ent[inx].sli_offset, len16);

	FI_BARRIER(status, ret);

	/* Slice selected, memory reserved */
	*ptr_p = ptr;
	ret = NO_ERROR;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Only 64 byte aligned slices supported
 */
static status_t cmem_get_slice(struct cmem_desc_s *mdesc,
			       uint32_t request_id,
			       uint32_t len_arg,
			       enum cmtag_e cmt,
			       uint8_t **ptr_p)
{
	status_t ret = NO_ERROR;
	uint32_t len = 0U;
	uint16_t len16 = 0U;
	uint8_t *ptr = NULL;
	const uint32_t memtype_max_slices = cmem_memtype_slices(cmt);

	/* XXX identify request origin somehow. Unused for now. */
	(void)request_id;

	LTRACEF("entry, origreq=%u\n", len_arg);

	if ((NULL == mdesc) || (NULL == ptr_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*ptr_p = NULL;

	if (len_arg >= CMEM_MAX_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (memtype_max_slices == mdesc->cde.d_entries_used) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY, LTRACEF("All slices in use\n"));
	}

	len = CMEM_SLICE_ALIGN_SIZE(len_arg);
	if (len > 0xFFFFU) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}
	len16 = (uint16_t)(len & 0xFFFFU);

	ret = cmem_select_slice(mdesc, cmt, len16, &ptr);
	CCC_ERROR_CHECK(ret);

	if (NULL == ptr) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Pointer is aligned 64 byte boundary */
	if (!IS_CMEM_ALIGNED(ptr)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("CMEM(%p): Context memory slice %p not aligned\n",
					     mdesc, ptr));
	}

	/* Return ref to the slice memory start to caller.
	 *
	 * This points to a 64 byte aligned contiguous buffer
	 * (Max CACHE_LINE size in our systems).
	 */
	*ptr_p = ptr;

	mdesc->cde.d_entries_used++;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Return CMEM descriptor for the object type request or NULL on error
 */
static struct cmem_desc_s *cmem_get_descriptor(struct context_mem_s *cmem,
					       enum cmtag_e cmt)
{
	struct cmem_desc_s *desc = NULL;
	void *cmbuf = NULL;

	LTRACEF("entry\n");

	/* XXX Does struct pointer casts, but all addresses are verified
	 * correctly aligned for this operation on cmem init.
	 */
	switch (cmt) {
	case CMTAG_PKA_DATA:
	case CMTAG_BUFFER:
	case CMTAG_ECONTEXT:
	case CMTAG_ALIGNED_BUFFER:
	case CMTAG_ALIGNED_CONTEXT:
	case CMTAG_API_STATE:
		cmbuf = &cmem->cmem_buf[0];
		break;

	case CMTAG_ALIGNED_DMA_BUFFER:
#if CCC_WITH_CONTEXT_DMA_MEMORY
		if (NULL != cmem->cmem_dma_buf) {
			cmbuf = &cmem->cmem_dma_buf[0];
		} else {
			LOG_ERROR("DMA request without dbuf\n");
		}
#else
		cmbuf = &cmem->cmem_buf[0];
#endif
		break;

	case CMTAG_KOBJECT:
#if CCC_WITH_CONTEXT_KEY_MEMORY
		if (NULL != cmem->cmem_key_buf) {
			cmbuf = &cmem->cmem_key_buf[0];
		} else {
			LOG_ERROR("KOBJECT request without kbuf\n");
		}
#else
		cmbuf = &cmem->cmem_buf[0];
#endif
		break;

	default:
		LTRACEF("unknown CMTAG not supported: %u\n", cmt);
		break;
	}

	if (!IS_CMEM_ALIGNED((uint8_t *)cmbuf)) {
		LTRACEF("CMEM(%p): Context memory not aligned for descriptor\n", cmbuf);
	} else {
		desc = (struct cmem_desc_s *)cmbuf;
	}

	LTRACEF("exit\n");
	return desc;
}

/* This is only used locally if release is also not exported.
 * So keep static if not exported.
 */
#if HAVE_USE_CMEM_RELEASE == 0
static
#endif
status_t cmem_alloc(struct context_mem_s *cmem,
		    enum cmtag_e cmt,
		    uint32_t request_id,
		    void **ptr_p,
		    uint32_t len_arg)
{
	status_t ret = NO_ERROR;
	struct cmem_desc_s *mdesc = cmem_get_descriptor(cmem, cmt);
	uint8_t *ptr = NULL;

	LTRACEF("entry\n");

	if (NULL == ptr_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == cmem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	*ptr_p = NULL;

	ret = cmem_get_slice(mdesc, request_id,
			     len_arg, cmt, &ptr);
	CCC_ERROR_CHECK(ret);

	/* Memory manager returns a (void *) type.
	 *
	 * The pointer target memory is 64 BYTE aligned
	 * contiguous buffer.
	 */
	*ptr_p = (void *)ptr;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_USE_CMEM_RELEASE
status_t cmem_release(struct context_mem_s *cmem,
		      enum cmtag_e cmt,
		      const void *ptr)
{
	status_t ret = NO_ERROR;
	struct cmem_desc_s *mdesc = cmem_get_descriptor(cmem, cmt);

	LTRACEF("CMEM(%p) release slice from %p -> %p\n", cmem, mdesc, ptr);
	ret = cmem_release_slice(mdesc, cmt, ptr);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_USE_CMEM_RELEASE */

bool context_memory_is_used(const struct context_mem_s *cmem)
{
	bool rval = false;

	LTRACEF("entry\n");

	if (NULL != cmem) {
		/* In each CMEM the GENERIC memory is mandatory
		 * and will not be active without it.
		 *
		 * The optional KEY and DMA portions get used
		 * if defined with non-zero size.
		 */
		rval = ((0U != cmem->cmem_size) && (NULL != cmem->cmem_buf));
	}

	LTRACEF("exit ==> %u\n", rval);
	return rval;
}

static status_t cmem_init_desc(enum cmem_type_e cmtype,
			       void *cmbuf,
			       uint32_t cmbuf_size_in,
			       uint32_t smallest_size)
{
	status_t ret = NO_ERROR;
	uint32_t min_size = 0U;
	struct cmem_desc_s *mdesc = NULL;
	uint32_t tmp = 0U;
	uint32_t msize = 0U;
	uint32_t cmbuf_size = cmbuf_size_in;

	(void)cmtype;

	LTRACEF("entry\n");

	if ((NULL == cmbuf) || (cmbuf_size <= CMEM_DESCRIPTOR_SIZE)) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LTRACEF("CMEM(%u): No memory provided in context\n", cmtype));
	}

	if (smallest_size > CMEM_MAX_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LTRACEF("CMEM(%u): requested size %u too large for CMEM\n",
					     cmtype, smallest_size));
	}

	if (cmbuf_size > CMEM_MAX_SIZE) {
		cmbuf_size = CMEM_MAX_SIZE;
		LTRACEF("CMEM(%u): Only using %u bytes of memory buffer %p\n",
			cmtype, cmbuf_size, cmbuf);
	}

	msize = (cmbuf_size / CMEM_DESCRIPTOR_SIZE) * CMEM_DESCRIPTOR_SIZE;

	/* Includes descriptor size (round up to 64 multiple)
	 */
	min_size = CMEM_MEM_SIZE_ALIGN(smallest_size);

	if (msize < min_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LOG_ERROR("CMEM(%u): insufficient memory (%u < %u)\n",
					       cmtype,
					       msize,
					       min_size));
	}

	/* NOTE: If we would here assign "msize = min_size" then that
	 * would force the minimum boundaries from the crypto class
	 * setting to the available cmem for the descptor being
	 * handed. This checks that the values set in the crypto
	 * classes are sufficient to complete the used algorithms.
	 * Also note that these values are the minimum acceptable cmem
	 * memory sizes, it is recommended that a full aligned page is
	 * passed when ever it is possible.
	 */

	/* Check alignment (for casting to mdesc) and for possible
	 * DMA
	 */
	if (!IS_CMEM_ALIGNED((uint8_t *)cmbuf)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
				     LTRACEF("CMEM(%u): Context memory not aligned\n", cmtype));
	}

	/* Init the descriptor */
	se_util_mem_set((uint8_t *)cmbuf, 0U, CMEM_DESCRIPTOR_SIZE);

	/* Cast ok, alignment verified above */
	mdesc = (struct cmem_desc_s *)cmbuf;

	tmp = msize - CMEM_DESCRIPTOR_SIZE;

	if (tmp > 0xFFFFU) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
				     LTRACEF("Released slice size over 16 bits\n"));
	}
	mdesc->cde.d_free_space = (uint16_t)(tmp & 0xFFFFU);

	LTRACEF("CMEM(%u) %p size %u\n", cmtype, mdesc, mdesc->cde.d_free_space);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CONTEXT_MEM_INIT_CALL == 0
static
#endif
status_t context_memory_init(struct context_mem_s *cmem,
			     const struct cm_layout_s *cm)
{
	status_t ret = NO_ERROR;
	uint32_t generic_size = 0U;

	LTRACEF("entry\n");

	if ((NULL == cmem) || (NULL == cm)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	generic_size = cm->cm_lo_size;

#if SE_RD_DEBUG
	/* Just check this for the sake of it
	 */
	if (sizeof_u32(struct cmem_memory_descriptor_s) != CMEM_DESCRIPTOR_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("CMEM descriptor size invalid: %u\n",
					       sizeof_u32(struct cmem_memory_descriptor_s)));
	}
#endif

	/* Generic portion of CMEM must exist to use CMEM */
	if ((NULL == cmem->cmem_buf) || (0U == cmem->cmem_size)) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LTRACEF("No memory provided to context\n"));
	}

#if CCC_WITH_CONTEXT_KEY_MEMORY
	/* Algorithm class does not use key memory if zero.
	 * For example message digests.
	 */
	if (cm->cm_lo_key_size > 0U) {
		ret = cmem_init_desc(CMEM_TYPE_KEY, cmem->cmem_key_buf,
				     cmem->cmem_key_size, cm->cm_lo_key_size);
		CCC_ERROR_CHECK(ret, LTRACEF("CMEM key memory init failed\n"));

		/* Key buffer not reserved from generic, so it can be smaller */
		generic_size = generic_size - cm->cm_lo_key_size;
	} else {
		cmem->cmem_key_buf  = NULL;
		cmem->cmem_key_size = 0U;
	}
#endif /* CCC_WITH_CONTEXT_KEY_MEMORY */

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* Algorithm class does not use dma memory if zero.
	 */
	if (cm->cm_lo_dma_size > 0U) {
		ret = cmem_init_desc(CMEM_TYPE_DMA, cmem->cmem_dma_buf,
				     cmem->cmem_dma_size, cm->cm_lo_dma_size);
		CCC_ERROR_CHECK(ret, LTRACEF("CMEM dma memory init failed\n"));

		/* Dma buffer not reserved from generic, so it can be smaller */
		generic_size = generic_size - cm->cm_lo_dma_size;
	} else {
		cmem->cmem_dma_buf  = NULL;
		cmem->cmem_dma_size = 0U;
	}
#endif

	ret = cmem_init_desc(CMEM_TYPE_GENERIC, cmem->cmem_buf,
			     cmem->cmem_size, generic_size);
	CCC_ERROR_CHECK(ret, LTRACEF("CMEM gen memory init failed\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_USE_CMEM_FROM_BUFFER
/* Colwert a (linear) aligned buffer to managed CMEM of cmtype
 *
 * In case accessed with phys address DMA the CALLER is responsible
 * for providing a linear contiguous buffer in BUF.
 */
status_t cmem_from_buffer(struct context_mem_s *cmem,
			  enum cmem_type_e cmtype,
			  void *buf, uint32_t blen)
{
	status_t ret = NO_ERROR;
	uint32_t tmp = 0U;

	LTRACEF("entry\n");

	if ((NULL == cmem) || (NULL == buf) ||
	    ((blen / CMEM_DESCRIPTOR_SIZE) <= 1U)) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LOG_ERROR("Buffer can not be colwerted to CMEM\n"));
	} else {
		struct cmem_desc_s *mdesc = NULL;

		/* Use 64 byte aligned size */
		uint32_t msize = (blen / CMEM_DESCRIPTOR_SIZE) * CMEM_DESCRIPTOR_SIZE;

		/* TODO: Check buf aligned to 64 bytes... */
		se_util_mem_set((uint8_t *)buf, 0U, CMEM_DESCRIPTOR_SIZE);

		switch (cmtype) {
		case CMEM_TYPE_GENERIC:
			cmem->cmem_buf = buf;
			cmem->cmem_size = msize;
			break;
#if CCC_WITH_CONTEXT_KEY_MEMORY
		case CMEM_TYPE_KEY:
			cmem->cmem_key_buf = buf;
			cmem->cmem_key_size = msize;
			break;
#endif
#if CCC_WITH_CONTEXT_DMA_MEMORY
		case CMEM_TYPE_DMA:
			cmem->cmem_dma_buf = buf;
			cmem->cmem_dma_size = msize;
			break;
#endif
		default:
			LTRACEF("Unsupported CMEM region type %u\n", cmtype);
			ret = SE_ERROR(ERR_NOT_SUPPORTED);
			break;
		}
		CCC_ERROR_CHECK(ret);

		if (!IS_CMEM_ALIGNED((uint8_t *)buf)) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
					     LTRACEF("CMEM(%u): Context memory not aligned\n", cmtype));
		}

		mdesc = (struct cmem_desc_s *)buf;

		CCC_SAFE_SUB_U32(tmp, msize, CMEM_DESCRIPTOR_SIZE);

		if (tmp > 0xFFFFU) {
			LTRACEF("Cmem buffer size limited to 65535\n");
			tmp = 0xFFFFU;
		}
		mdesc->cde.d_free_space = (uint16_t)(tmp & 0xFFFFU);

		LTRACEF("CMEM(%u) size %u\n",
			cmtype, mdesc->cde.d_free_space);
	}

fail:
	LTRACEF("exit\n");
	return ret;
}
#endif /* HAVE_USE_CMEM_FROM_BUFFER */

status_t context_memory_get_init(struct context_mem_s *chk_mem,
				 struct context_mem_s **cmem_p,
				 const struct cm_layout_s *cm)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == cmem_p) || (NULL == cm)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*cmem_p = NULL;

	if (BOOL_IS_TRUE(context_memory_is_used(chk_mem))) {
		ret = context_memory_init(chk_mem, cm);
		CCC_ERROR_CHECK(ret,
				LTRACEF("CMEM check failed\n"));

		LTRACEF("CMEM enabled for call context\n");
		*cmem_p = chk_mem;
	}

#if FORCE_CONTEXT_MEMORY
	if (NULL == *cmem_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("CMEM object not configured (when forced)\n"));
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CONTEXT_MEMORY */

void *cm_tag_mem_get(struct context_mem_s *cmem, enum cmtag_e cmt,
		     uint32_t align, uint32_t msize)
{
	void *rv = NULL;

	(void)cmem;
	(void)align; /* CMEM aligns all to 64 bytes */

	do {
#if HAVE_CONTEXT_MEMORY
		if (NULL != cmem) {
			status_t ret = NO_ERROR;

			ret = cmem_alloc(cmem, cmt, 42U, &rv, msize);
			if (NO_ERROR != ret) {
				LOG_ERROR("cmem_alloc error %d\n", ret);
				rv = NULL;
			}
			break;
		}
#endif

#if FORCE_CONTEXT_MEMORY
		LOG_ERROR("** ERROR: CCC *MEMPOOL* get [t:%u a:%u s:%u]\n",
			  cmt, align, msize);
#else
		{
			void *p = cm_tag_mem_get_mempool(cmt, align, msize);
			if (NULL != p) {
				if ((((uintptr_t)(uint8_t *)p) & (CCC_MINIMUM_MEMPOOL_BYTE_ALIGN - 1U)) != 0U) {
					LOG_ERROR("MEMPOOL alignment: align(%p) != %u\n",
						  p, CCC_MINIMUM_MEMPOOL_BYTE_ALIGN);
					cm_tag_mem_release_mempool(cmt, p);
					rv = NULL;
				} else {
					rv = p;
				}
			}
		}
#endif /* FORCE_CONTEXT_MEMORY */

	} while (false);

	return rv;
}

/* ptr can not be const as it may be passed to mempool release.
 * That API is not in CCC control.
 * [ misra 8.13 ]
 */
void cm_tag_mem_release(struct context_mem_s *cmem, enum cmtag_e cmt,
			void *ptr)
{
	(void)cmem;
	(void)cmt;

	do {
#if HAVE_CONTEXT_MEMORY
		if (NULL != cmem) {
			status_t ret = NO_ERROR;
			struct cmem_desc_s *mdesc = cmem_get_descriptor(cmem, cmt);
			ret = cmem_release_slice(mdesc, cmt, ptr);
			LTRACEF("CMEM release slice: ->%d\n", ret);
			(void)ret;
			break;
		}
#endif

#if FORCE_CONTEXT_MEMORY
		LOG_ERROR("** ERROR: CCC *MEMPOOL* release [t:%u p:%p]\n",
			  cmt, ptr);
#else
		cm_tag_mem_release_mempool(cmt, ptr);
#endif /* FORCE_CONTEXT_MEMORY */
	} while (false);
}

/* Returns NULL if context memory is not configured OR
 * the context_mem_s object is not properly set up (cmem_size is zero
 * or cmem_buf is NULL).
 */
struct context_mem_s *context_memory_get(struct context_mem_s *chk_mem)
{
	/* Override with NULL to use param for misra */
	struct context_mem_s *cmem = chk_mem;

	LTRACEF("entry\n");

#if HAVE_CONTEXT_MEMORY
	if (BOOL_IS_TRUE(context_memory_is_used(cmem))) {
		LTRACEF("CMEM used for call context\n");
	} else {
		cmem = NULL;
	}

#if FORCE_CONTEXT_MEMORY
	if (NULL == cmem) {
		CCC_ERROR_MESSAGE("CMEM object not configured (when forced)\n");
	}
#endif

#else
	cmem = NULL;
#endif /* HAVE_CONTEXT_MEMORY */

	LTRACEF("exit\n");
	return cmem;
}
