#ifndef _FAST_COMPRESS_BC7_H
#define _FAST_COMPRESS_BC7_H

namespace lwtt_internal
{
	void CompressBC7Block(bool imageHasAlpha, const float* colors, void * output, int vw, int vh);

}

#endif

