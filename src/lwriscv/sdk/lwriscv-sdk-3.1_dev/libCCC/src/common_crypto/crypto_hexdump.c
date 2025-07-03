/*
 * Copyright (c) 2015-2021, LWPU Corporation.  All rights reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */
#include <crypto_system_config.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SE_UTIL_VPRINTF_AP

static uint32_t ccc_rd_log_cond_state = CCC_RD_LOG_COND_STATE_ENABLE;

void ccc_rd_log_cond_set_state(uint32_t set_state)
{
	ccc_rd_log_cond_state = set_state;

	LOG_ALWAYS("CCC R&D log value set to 0x%x\n", set_state);
}

void ccc_rd_log_cond(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (CCC_RD_LOG_COND_STATE_DISABLE != ccc_rd_log_cond_state) {
		CCC_VPRINTF_AP(fmt, ap);
	}
	va_end(ap);
}
#endif /* HAVE_SE_UTIL_VPRINTF_AP */

#if SE_RD_DEBUG || defined(CCC_HEXDUMP_LOG)

/* Allows redefining the hex dump log function e.g.
 * with make command line without other traces being enabled.
 *
 * Keep SE_RD_DEBUG as a mandatory option for HEXDUMP.
 */
#if !defined(CCC_HEXDUMP_LOG)
#define CCC_HEXDUMP_LOG LOG_INFO
#endif

#if !defined(CCC_HEX_LOG_LEN)

/* This should be odd and 1 longer than nibbles per line.
 *
 * Some R&D systems like VDK have line length limitations which may
 * require this to be set to e.g. 33U or the hex output will be
 * dropped.
 */
#if CCC_SOC_FAMILY_ON(CCC_SOC_ON_VDK)
#define CCC_HEX_LOG_LEN 33U
#else
#define CCC_HEX_LOG_LEN 65U
#endif
#endif /* !defined CCC_HEX_LOG_LEN */

/* hex dumper, (HEX_LOG_LEN-1) hex nibbles per line */
static void xdump_buf_hex(const uint8_t *base, uint32_t bytes_out)
{
	uint8_t buf[CCC_HEX_LOG_LEN];
	uint32_t i = 0U;
	uint32_t len = (sizeof_u32(buf) - 1U) / 2U;
	/* Cert-c compatible "0123456789ABCDEF" */
	const uint8_t hexval[] = {
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
		0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
	};

	while (i < bytes_out) {
		uint32_t j = 0U;
		uint32_t bi = 0U;

		while (j++ < len) {
			uint8_t byte = base[i];
			uint8_t lnibble = byte;
			lnibble >>= 4U;

			buf[bi] = hexval[ lnibble ];
			bi++;
			buf[bi] = hexval[ byte & 0x0FU ];
			bi++;

			i++;

			if (i >= bytes_out) {
				break;
			}
		}
		buf[bi] = 0U;
		CCC_HEXDUMP_LOG(" %s\n", buf);
	}
}

/* For testing only; print object in hex */
void dump_data_object(const char *name, const uint8_t *base,
		      uint32_t size, uint32_t bytes_out)
{
	uint32_t outlen = bytes_out;
	if (NULL != name) {
		CCC_HEXDUMP_LOG("Hex dump: %s [%u] @ %p\n",
				name, size, base);

		if ((NULL != base) && (size > 0U) && (outlen > 0U)) {

			if (size < outlen) {
				outlen = size;
			}

			xdump_buf_hex(base, outlen);
		}
	}
}
#endif /* SE_RD_DEBUG || defined(CCC_HEXDUMP_LOG) */
