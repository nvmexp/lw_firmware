/*
 * Copyright (c) 2016-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/*
 * This layer takes care of processing and buffering data when several
 * calls feed the inbound data in parts (i.e. zero or more UPDATE()
 * calls followed by one DOFINAL() call).
 *
 * Such a sequence of multiple calls is supported for digests, macs and
 * block/stream cipher functions.
 *
 * Functions in this file are called by the crypto_<md,cipher,mac>.c
 * to process the data, possibly via the crypto_ta_api.c which is dealing
 * with the TA user space addresses (if the crypto call domain is not the
 * secure OS).
 *
 * So this module deals with kernel virtual addresses only. This calls
 * the SE engine HW functions in tegra_se.c, which maps the KV addresses
 * to phys addresses before passing them to the engine.
 */

/**
 * @file   crypto_process_sha.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   Tue Jan 22 15:37:14 2019
 *
 * @brief  Processing layer for SHA operations.
 */

#include <crypto_system_config.h>

#include <crypto_md_proc.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SE_SHA

static status_t sha_digest_check_args(const struct se_data_params *input_params,
				      const te_crypto_algo_t algo)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if ((NULL == input_params) || (NULL == input_params->dst)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(algo) {
	case TE_ALG_SHA1:
	case TE_ALG_SHA224:
	case TE_ALG_SHA256:
	case TE_ALG_SHA384:
	case TE_ALG_SHA512:
#if HAVE_NIST_TRUNCATED_SHA2
	case TE_ALG_SHA512_224:
	case TE_ALG_SHA512_256:
#endif
		break;

	default:
		LTRACEF("Not SHA digest algo: 0x%x\n", algo);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * Callwlate SHA digest. Generic helper for other process functions.
 *
 * input_params->output_size will be modified by the call to contain
 * the number of bytes written to input_params->dst (value is
 * the digest length).
 */
status_t sha_digest(
	struct context_mem_s *cmem,
	te_crypto_domain_t domain,
	struct se_data_params *input_params,
	te_crypto_algo_t sha_algo)
{
	status_t ret = NO_ERROR;
	uint32_t hlen = 0U;
	struct se_sha_context *sha_context = NULL;

	LTRACEF("entry\n");

	ret = sha_digest_check_args(input_params, sha_algo);
	CCC_ERROR_CHECK(ret);

	ret = get_message_digest_size(sha_algo, &hlen);
	CCC_ERROR_CHECK(ret);

	if (input_params->output_size < hlen) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	sha_context = CMTAG_MEM_GET_OBJECT(cmem,
					   CMTAG_ALIGNED_CONTEXT,
					   CCC_DMA_MEM_ALIGN,
					   struct se_sha_context,
					   struct se_sha_context *);
	if (NULL == sha_context) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = sha_context_static_init(domain,
				      CCC_ENGINE_ANY,
				      sha_algo,
				      sha_context,
				      cmem);
	CCC_ERROR_CHECK(ret);

	sha_context->ec.is_last = true;

	/* Call this even with TA addresses. The TA process call
	 * (which may be in sha_context->process if this originates from TA)
	 * would trap the kernel domain address of "hash" object.
	 *
	 * The kernel call (below) can handle also TA addresses if
	 * the ec.domain is TE_CRYPTO_DOMAIN_TA when any of the addresses are
	 * from TA space.
	 */
	ret = se_sha_process_input(input_params, sha_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("SHA digest failed\n"));

fail:
	if (NULL != sha_context) {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		sha_context_dma_buf_release(sha_context);
#else
		se_util_mem_set(sha_context->buf, 0U, CCC_DMA_MEM_SHA_BUFFER_SIZE);
#endif
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_ALIGNED_CONTEXT,
				  sha_context);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_sha_engine_chunk_data_locked(const engine_t *engine,
						struct se_data_params *input,
						struct se_engine_sha_context *econtext,
						uint32_t max_chunk_size,
						uint32_t *out_bytes_p)
{
	status_t ret = NO_ERROR;
	struct se_data_params pinput;
	uint32_t bytes_left = input->input_size;
	uint32_t processed = 0U;
	bool is_last = econtext->is_last;

	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	econtext->is_last = false;

	CCC_LOOP_BEGIN {
		uint32_t bcount = 0U;

		pinput.src = &input->src[processed];
		pinput.dst = NULL;
		pinput.output_size = 0U;

		if (bytes_left <= max_chunk_size) {
			bcount = bytes_left;

			if (BOOL_IS_TRUE(is_last)) {
				pinput.dst         = input->dst;
				pinput.output_size = input->output_size;
				econtext->is_last = true;
			}
		} else {
			bcount = max_chunk_size;
		}

		pinput.input_size = bcount;

		if (!BOOL_IS_TRUE(econtext->is_hw_hmac)) {
			ret = CCC_ENGINE_SWITCH_SHA(engine, &pinput, econtext);
		} else {
#if HAVE_HW_HMAC_SHA
			ret = CCC_ENGINE_SWITCH_HMAC(engine, &pinput, econtext);
#else
			ret = SE_ERROR(ERR_BAD_STATE);
#endif
		}
		CCC_ERROR_END_LOOP(ret);

		ret = CCC_ADD_U32(processed, processed, bcount);
		CCC_ERROR_END_LOOP(ret);

		ret = CCC_SUB_U32(bytes_left, bytes_left, bcount);
		CCC_ERROR_END_LOOP(ret);

		if (0U == bytes_left) {
			CCC_LOOP_STOP;
		}
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

	*out_bytes_p = pinput.output_size;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Process one data chunk with the SE engine
 *
 * Internally split the input buffer to MAX_SHA_ENGINE_CHUNK_SIZE_BSIZExx
 * calls to the SHA engine.
 *
 * Now the "max SHA input size" to a client call is 4 GB.
 *
 * Args verified by caller.
 */
static status_t se_sha_engine(const engine_t *engine,
			      struct se_data_params *input,
			      struct se_engine_sha_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t max_chunk_size = 0U;
	uint32_t out_bytes = 0U;

	LTRACEF("entry\n");

	ret = se_engine_get_sha_max_chunk_size(econtext, &max_chunk_size);
	CCC_ERROR_CHECK(ret);

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = se_sha_engine_chunk_data_locked(engine, input, econtext,
					      max_chunk_size, &out_bytes);
	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret);

	input->output_size = out_bytes;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_sha_engine_call(struct se_sha_context *sha_context,
				   const uint8_t *buf, uint8_t *hash_dst,
				   uint32_t outlen, uint32_t bytes)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	if (NULL == sha_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	input.src = buf;
	input.input_size = bytes;
	input.dst = hash_dst;
	input.output_size = outlen;

	if ((NULL != buf) || (bytes != 0U)) {
		/* This traps NULL buffers (even zero length)
		 */
		DEBUG_ASSERT_PHYS_DMA_OK(buf, bytes);
	}

	ret = se_sha_engine(sha_context->ec.engine, &input, &sha_context->ec);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_NC_REGIONS

static status_t sha_process_nc_check_args(
	const struct se_sha_context *sha_context,
	const uint8_t *kva, uint32_t remain)
{
	status_t ret = NO_ERROR;

	if ((NULL == sha_context) || (NULL == kva) || (0U == remain)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (sha_context->used != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	return ret;
}

/* Complexity reduction helper */
static status_t sha_process_nc_regions_intern(struct se_sha_context *sha_context,
					      uint32_t *remain_p,
					      uint32_t clength,
					      const uint8_t *kva_param,
					      uint32_t *kva_offset_p,
					      uint8_t *dst,
					      uint32_t dst_len,
					      uint32_t block_size,
					      bool is_last)
{
	status_t ret = NO_ERROR;
	uint32_t remain = *remain_p;
	uint32_t kva_offset = 0U;
	const uint8_t *kva = kva_param;

	LTRACEF("entry\n");

	if ((remain > 0U) && (clength > 0U)) {
		if ((remain <= clength) && BOOL_IS_TRUE(is_last)) {
			/* If this handles the last bytes of dofinal, set this back to what it was on entry
			 */
			sha_context->ec.is_last = is_last;

			/* digest the final bytes for this digest
			 * (this completes dofinal)
			 */
			ret = se_sha_engine_call(sha_context, kva,
						 dst, dst_len, remain);
			CCC_ERROR_CHECK(ret,
					LTRACEF("SHA digest failed: %d\n",
						ret));
			kva_offset += remain;
			remain  = 0U;
		} else {
			/* This does not handle last bytes of dofinal => ec.is_last must not be
			 * set true for the digest and partial bytes get copied to sha_context buf.
			 */
			uint32_t blocks = clength / block_size;
			uint32_t nbytes = blocks * block_size;

			if (blocks > 0U) {
				/* digest full blocks before non-contig
				 * page boundary
				 */
				ret = se_sha_engine_call(sha_context,
							 kva,
							 dst,
							 dst_len,
							 nbytes);
				CCC_ERROR_CHECK(ret,
						LTRACEF("SHA digest failed: %d\n",
							ret));
				kva     = &kva[nbytes];
				kva_offset += nbytes;
				remain -= nbytes;
			}

			/* Move the partial sha block before page boundary to sha_context->buf
			 */
			nbytes = clength % block_size;
			if (nbytes > 0U) {
				se_util_mem_move(sha_context->buf, kva, nbytes);
				remain -= nbytes;

				kva_offset += nbytes;

				sha_context->used = nbytes;
			}
		}
	}

	*remain_p = remain;

	/* Caller advances its kva to &kva[kva_offset] (using caller kva)
	 */
	*kva_offset_p = kva_offset;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Complexity reduction helper */
static status_t sha_process_nc_regions_cbuf(struct se_sha_context *sha_context,
					    uint8_t *dst,
					    uint32_t dst_len,
					    uint32_t remain,
					    bool is_last)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((sha_context->used == sha_context->ec.block_size) || BOOL_IS_TRUE(is_last)) {
		if (0U == remain) {
			/* If this handles the last bytes of this chunk
			 * set this back to what it was on entry, in case this is dofinal
			 */
			sha_context->ec.is_last = is_last;
		}

		ret = se_sha_engine_call(sha_context,
					 sha_context->buf,
					 dst,
					 dst_len,
					 sha_context->used);
		CCC_ERROR_CHECK(ret,
				LTRACEF("SHA digest failed: %d\n",
					ret));
		sha_context->used = 0U;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_process_nc_regions(struct se_sha_context *sha_context,
				       const uint8_t *kva_param, uint8_t *dst,
				       uint32_t dst_len, uint32_t remain_param)
{
	status_t ret	    = NO_ERROR;
	const uint8_t *kva = kva_param;
	uint32_t block_size = 0U;
	bool is_last	    = false;
	bool is_last_is_valid = false;
	uint32_t remain = remain_param;

	LTRACEF("entry\n");

	ret = sha_process_nc_check_args(sha_context, kva, remain);
	CCC_ERROR_CHECK(ret);

	/* Is this a dofinal() call */
	is_last = sha_context->ec.is_last;
	is_last_is_valid = true;

	/* This must not be set in partial updates for non-adjacent pages */
	sha_context->ec.is_last = false;

	block_size = sha_context->ec.block_size;

	while (remain > 0U) {
		uint32_t clength = 0U;

		/* Find the amount of bytes (up to REMAIN bytes) that
		 * can be accessed with a phys mem DMA before a
		 * non-contiguous page boundary.
		 *
		 * I.e. in a "contiguous phys memory region" from KVA.
		 */
		ret = ccc_nc_region_get_length(sha_context->ec.domain, kva, remain, &clength);
		if (NO_ERROR != ret) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Failed to get contiguous region length: %d\n",
						     ret));
		}

		if ((sha_context->used > 0U) || (clength < block_size)) {
			uint32_t fill_size = block_size - sha_context->used;

			/* clength may not be long enough to complete
			 * the digest block.
			 */
			if (clength < fill_size) {
				fill_size = clength;
			}

			se_util_mem_move(&sha_context->buf[sha_context->used], kva, fill_size);
			remain  -= fill_size;
			clength -= fill_size;
			kva      = &kva[fill_size];
			sha_context->used += fill_size;

			if (sha_context->used < block_size) {

				DEBUG_ASSERT(0U == clength);

				if (remain > 0U) {
					continue;
				}
			}

			/* if sha_context->buf is full OR this is the
			 * dofinal call and we have no more data =>
			 * digest last partial sha block.
			 */
			ret = sha_process_nc_regions_cbuf(sha_context,
							  dst, dst_len,
							  remain, is_last);
			CCC_ERROR_CHECK(ret);
		}

		/* At this point: sha_context->buf is empty; clength
		 * may still be nonzero (chunk has more data)
		 */
		{
			uint32_t kva_offset = 0U;
			ret = sha_process_nc_regions_intern(sha_context, &remain,
							    clength, kva, &kva_offset,
							    dst, dst_len, block_size,
							    is_last);
			CCC_ERROR_CHECK(ret);

			kva = &kva[kva_offset];
		}
	}

fail:
	if (BOOL_IS_TRUE(is_last_is_valid)) {
		sha_context->ec.is_last = is_last;
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_NC_REGIONS */

static status_t sha_process_check_args(
	const struct se_data_params *input_params,
	const struct se_sha_context *sha_context)
{
	status_t ret = NO_ERROR;

	if ((NULL == sha_context) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL == input_params->src) && (input_params->input_size > 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	return ret;
}

static status_t sha_process_input_last(
	struct se_data_params *input_params,
	struct se_sha_context *sha_context,
	const uint8_t *input_src,
	uint8_t *input_dst,
	uint32_t byte_count)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = byte_count;

	/* Context gather buffer is either empty or all input data
	 *  to handle are in the gather buffer at this point.
	 */
	const uint8_t *data = NULL;
	uint32_t digest_len = 0U;

	LTRACEF("entry\n");

	if (IS_SHAKE(sha_context->ec.hash_algorithm)) {
		digest_len = sha_context->ec.hash_size;
	} else {
		ret = get_message_digest_size(sha_context->ec.hash_algorithm, &digest_len);
		CCC_ERROR_CHECK(ret);
	}

	if (input_params->output_size < digest_len) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (sha_context->used > 0U) {
		if (bytes_left > 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
		data = sha_context->buf;
		bytes_left = sha_context->used;
	} else {
		// Note: bytes_left may also be ZERO here
		//
		data = input_src;
	}

#if HAVE_NC_REGIONS
	if ((data != sha_context->buf) && (bytes_left > 0U)) {
		ret = sha_process_nc_regions(sha_context, data,
					     sha_context->ec.tmphash,
					     sizeof_u32(sha_context->ec.tmphash), bytes_left);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("SHA digest failed: %d\n", ret));
	} else {
		ret = se_sha_engine_call(sha_context, data,
					 sha_context->ec.tmphash,
					 sizeof_u32(sha_context->ec.tmphash),
					 bytes_left);
		CCC_ERROR_CHECK(ret,
				LTRACEF("SHA digest failed: %d\n", ret));
	}
#else
	ret = se_sha_engine_call(sha_context, data,
				 sha_context->ec.tmphash,
				 sizeof_u32(sha_context->ec.tmphash),
				 bytes_left);
	CCC_ERROR_CHECK(ret,
			LTRACEF("SHA digest failed: %d\n", ret));
#endif	/* HAVE_NC_REGIONS */

	/* All data has now been handled and the final result is ready
	 * in context->ec.tmphash
	 */
	CCC_SAFE_ADD_U32(sha_context->byte_count, sha_context->byte_count, bytes_left);
	sha_context->used = 0U;

	/* Even if DST is NULL, tmphash contains the digest value.
	 */
	if (NULL != input_dst) {
		if (sha_context->ec.tmphash != input_dst) {
			se_util_mem_move(input_dst, sha_context->ec.tmphash, digest_len);
		}
		input_params->output_size = digest_len;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* update() enters here only if there is more data */
static status_t sha_process_input_update(
	struct se_sha_context *sha_context,
	const uint8_t *source,
	uint32_t bytes_left_param,
	uint32_t buf_size)
{
	status_t ret = NO_ERROR;
	uint32_t consumed = 0U;
	uint32_t full_blocks = 0U;
	uint32_t residue = 0U;
	uint32_t bytes_left = bytes_left_param;
	const uint8_t *input_src = source;

	LTRACEF("entry\n");

	/* For an update the context buffer must have been filled first =>
	 * here deal with the bytes that did not fit there. Since
	 * there are more bytes, the gather buffer was flushed above.
	 */
	if (sha_context->used > 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* CERT INT33-C avoid to divided by zero */
	if (0U == buf_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
	full_blocks = bytes_left / buf_size;
	residue = bytes_left % buf_size;

	if ((full_blocks > 0U) && (residue == 0U)) {
		/* Do not allow update to write all bytes since it is
		 * unknown if the following calls will provide any
		 * more input data. The HW engine can not cope
		 * with zero length dofinal call.
		 */
		full_blocks--;
	}

	/* Deal with the next N full gather buffers => i.e.
	 * N * (M * digest blocksize) of input
	 */
	if (full_blocks > 0U) {
		consumed = full_blocks * buf_size;
#if HAVE_NC_REGIONS
		ret = sha_process_nc_regions(sha_context, input_src,
					     NULL, 0U, consumed);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("SHA digest failed: %d\n", ret));
#else
		ret = se_sha_engine_call(sha_context, input_src, NULL, 0U,
					 consumed);
		CCC_ERROR_CHECK(ret,
				LTRACEF("SHA digest failed: %d\n", ret));
#endif
		CCC_SAFE_ADD_U32(sha_context->byte_count, sha_context->byte_count, consumed);
		input_src = &input_src[consumed];
		CCC_SAFE_SUB_U32(bytes_left, bytes_left, consumed);
	}

	/* Handle stray data later (next update or dofinal),
	 * just stuff it in the gather buffer.
	 */
	if (bytes_left > 0U) {
		consumed = bytes_left;

		se_util_mem_move(&sha_context->buf[sha_context->used],
				 input_src, consumed);

		sha_context->used += consumed;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_flush_full_context_buf(struct se_sha_context *sha_context,
					   bool is_last, uint32_t bytes_left,
					   uint32_t buf_size)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Gather buffer full and we still have more data to handle =>
	 * flush this.
	 */
	if (bytes_left > 0U) {
		if (sha_context->used != buf_size) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     LTRACEF("SHA flush full: sha_context buffer not full\n"));
		}
		sha_context->ec.is_last = false;
		ret = se_sha_engine_call(sha_context, sha_context->buf, NULL,
					 0U, sha_context->used);
		CCC_ERROR_CHECK(ret,
				LTRACEF("SHA digest failed: %d\n", ret));

		sha_context->ec.is_last = is_last;
		CCC_SAFE_ADD_U32(sha_context->byte_count, sha_context->byte_count,
				 sha_context->used);
		sha_context->used = 0U;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_update_context(
	struct se_sha_context *sha_context,
	const uint8_t **input_src_p,
	uint32_t *bytes_left_p,
	uint32_t buf_size)
{
	status_t ret = NO_ERROR;
	uint32_t consumed = 0U;
	uint32_t bytes_left = 0U;
	uint32_t total_bytes = 0U;
	const uint8_t *src = NULL;
	bool is_last = false;	// XXX for coverity...

	LTRACEF("entry\n");

	if ((NULL == input_src_p) || (NULL == *input_src_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	is_last = sha_context->ec.is_last;
	bytes_left = *bytes_left_p;
	src = *input_src_p;

	CCC_SAFE_ADD_U32(total_bytes, sha_context->used, bytes_left);
	if (total_bytes <= buf_size) {
		consumed = bytes_left;
	} else {
		consumed = buf_size - sha_context->used;
	}

	if (consumed > 0U) {
		se_util_mem_move(&sha_context->buf[sha_context->used],
				 src, consumed);
		CCC_SAFE_ADD_U32(sha_context->used, sha_context->used, consumed);
		CCC_SAFE_SUB_U32((bytes_left), (bytes_left), (consumed));
		src = &src[consumed];
	}

	/* Gather buffer full and we still have more data to handle =>
	 * flush this.
	 */
	ret = sha_flush_full_context_buf(sha_context, is_last, bytes_left, buf_size);
	CCC_ERROR_CHECK(ret);

	*bytes_left_p = bytes_left;
	*input_src_p = src;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_process_input(
	struct se_sha_context *sha_context,
	const uint8_t **input_src_p,
	uint32_t *bytes_left_p,
	uint32_t buf_size)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = 0U;

	LTRACEF("entry\n");

	if ((NULL == sha_context) || (NULL == bytes_left_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	bytes_left = *bytes_left_p;

	/* Reduce engine calls with a gather buffer which is N*algorithm block size long
	 * (but less than one page long to avoid page boundaries). Lwrrently N == 1.
	 *
	 * In case we have multiple update() calls before the dofinal() and all supplied
	 * data fits in the gather buffer, this minimizes the HW SE engine calls.
	 */
	if ((bytes_left > 0U) &&
	    ((sha_context->used > 0U) || (bytes_left < buf_size))) {
		ret = sha_update_context(sha_context, input_src_p,
					 bytes_left_p, buf_size);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* handle SHA digest data processing ops (update,dofinal)
 */
status_t se_sha_process_input(
	struct se_data_params *input_params,
	struct se_sha_context *sha_context)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = 0U;
	uint32_t buf_size = 0U;

	const uint8_t *input_src = NULL;
	uint8_t *input_dst = NULL;
	bool is_last = false;	// XXX for coverity...

	LTRACEF("entry\n");

	ret = sha_process_check_args(input_params, sha_context);
	CCC_ERROR_CHECK(ret);

#if HAVE_SE_ASYNC_SHA
	if ((SHA_ASYNC_MAGIC == sha_context->async_context.magic) &&
	    BOOL_IS_TRUE(sha_context->async_context.started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Synchronous SHA operation started while async operation is running\n"));
	}
#endif

	is_last = sha_context->ec.is_last;

	input_src = input_params->src;
	input_dst = input_params->dst;

	bytes_left = input_params->input_size;

	/* TODO: Could optimize later case when "buf size" / "block_size" > 1
	 */
	if (CCC_DMA_MEM_SHA_BUFFER_SIZE < sha_context->ec.block_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("SHA context buffer size %u < block size %u\n",
						       CCC_DMA_MEM_SHA_BUFFER_SIZE,
						       sha_context->ec.block_size));
	}

	/* Use a gather buffer matching the digest block size.
	 */
	buf_size = sha_context->ec.block_size;

	LTRACEF("DIGEST INPUT: src=%p, input_size=%u, hash_addr dst=%p (GBUF %u)\n",
		input_src, input_params->input_size, input_dst, buf_size);

	ret = sha_process_input(sha_context, &input_src,
				&bytes_left, buf_size);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(is_last)) {
		ret = sha_process_input_last(input_params, sha_context,
					     input_src, input_dst,
					     bytes_left);
		CCC_ERROR_CHECK(ret);
	} else {
		/* In case the update contains more bytes that did not fit in
		 * the context buf (the ones that fit were handled above).
		 */
		if (bytes_left > 0U) {
			ret = sha_process_input_update(sha_context, input_src,
						       bytes_left, buf_size);
			CCC_ERROR_CHECK(ret);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_ASYNC_SHA

status_t se_sha_process_async_check(const struct se_sha_context *sha_context, bool *done_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == sha_context) || (NULL == done_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*done_p = false;

	if (!BOOL_IS_TRUE(sha_context->async_context.started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Async SHA has not been started\n"));
	}

	ret = CCC_ENGINE_SWITCH_SHA_ASYNC_CHECK(sha_context->ec.engine,
					     &sha_context->async_context, done_p);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_setup_async_input_context(
	struct se_data_params *input_params,
	struct se_sha_context *sha_context)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = 0U;
	uint32_t block_size = 0U;
	bool     is_contiguous = false;

	LTRACEF("entry\n");

	ret = sha_process_check_args(input_params, sha_context);
	CCC_ERROR_CHECK(ret);

	/* SHA digest block size */
	block_size = sha_context->ec.block_size;

	LTRACEF("DIGEST INPUT: block size=%u, src=%p, input_size=%u, hash_addr dst=%p\n",
		block_size, input_params->src, input_params->input_size, input_params->dst);

	bytes_left = input_params->input_size;

	if (0U == bytes_left) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Async SHA does not support NULL data input\n"));
	}

	ret = se_is_contiguous_memory_region(input_params->src, bytes_left, &is_contiguous);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Async SHA data contiguous check failed: %d\n", ret));

	if (!BOOL_IS_TRUE(is_contiguous)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Async SHA does not support non-contiguous data (length %u)\n",
					     bytes_left));
	}

	if (!BOOL_IS_TRUE(sha_context->ec.is_last)) {
		/* Async operation requires that input data is
		 * passed in multiples of SHA blocksize. Async
		 * code does not do any kind of data
		 * reblocking, use the synchronous api if your
		 * data is not multiple of SHA block size.
		 *
		 * The last data chunk (for SHA dofinal) does not need
		 * to be a block multiple.
		 */
		if ((bytes_left % block_size) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Async SHA only supports only multiple of block size (%u != N*%u) data length\n",
						     bytes_left, block_size));
		}
	}

	CCC_OBJECT_ASSIGN(sha_context->async_context.input, *input_params);
	sha_context->async_context.econtext = &sha_context->ec;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Handle async SHA digest data processing ops (update,dofinal)
 *
 * input_params can be NULL if finishing an async operation (it is not used
 * in that case).
 */
status_t se_sha_process_async_input(
	struct se_data_params *input_params,
	struct se_sha_context *sha_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == sha_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (sha_context->ec.domain != TE_CRYPTO_DOMAIN_KERNEL) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Async SHA unsupported data domain: %u\n",
					     sha_context->ec.domain));
	}

	if (BOOL_IS_TRUE(sha_context->async_start)) {
		ret = sha_setup_async_input_context(input_params, sha_context);
		CCC_ERROR_CHECK(ret);

		HW_MUTEX_TAKE_ENGINE(sha_context->ec.engine);

		sha_context->async_context.magic = SHA_ASYNC_MAGIC;
		ret = CCC_ENGINE_SWITCH_SHA_ASYNC_START(sha_context->ec.engine, &sha_context->async_context);

		/* Engine is left locked; client is responsible for releasing it */
	} else {
		/* Engine is unlocked here */

		ret = CCC_ENGINE_SWITCH_SHA_ASYNC_FINISH(sha_context->ec.engine, &sha_context->async_context);
		sha_context->async_context.magic = 0U;

		HW_MUTEX_RELEASE_ENGINE(sha_context->ec.engine);

		LTRACEF("Async SHA finished: %d\n", ret);
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_SHA */
#endif /* HAVE_SE_SHA */
