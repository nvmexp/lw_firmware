/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

/** @file
  compare_mem() implementation.
**/

#include "base.h"

/**
  Compares the contents of two buffers.

  This function compares length bytes of source_buffer to length bytes of destination_buffer.
  If all length bytes of the two buffers are identical, then 0 is returned.  Otherwise, the
  value returned is the first mismatched byte in source_buffer subtracted from the first
  mismatched byte in destination_buffer.

  If length > 0 and destination_buffer is NULL, then ASSERT().
  If length > 0 and source_buffer is NULL, then ASSERT().
  If length is greater than (MAX_ADDRESS - destination_buffer + 1), then ASSERT().
  If length is greater than (MAX_ADDRESS - source_buffer + 1), then ASSERT().

  @param  destination_buffer A pointer to the destination buffer to compare.
  @param  source_buffer      A pointer to the source buffer to compare.
  @param  length            The number of bytes to compare.

  @return 0                 All length bytes of the two buffers are identical.
  @retval Non-zero          The first mismatched byte in source_buffer subtracted from the first
                            mismatched byte in destination_buffer.

**/
intn compare_mem(IN const void *destination_buffer,
		 IN const void *source_buffer, IN uintn length)
{
	volatile uint8 *PointerDst;
	volatile uint8 *PointerSrc;
	intn Delta;

	PointerDst = (uint8 *)destination_buffer;
	PointerSrc = (uint8 *)source_buffer;
	Delta = 0;
	while ((length-- != 0) && (Delta == 0)) {
		Delta = *(PointerDst++) - *(PointerSrc++);
	}

	return Delta;
}
