#include "FastCompressBC7.h"
#include "vulkan/shared_sources/lwastc/ref_astc.h"
#include "vulkan/shared_sources/lwastc/vec4.h"
#include <cfloat>

typedef unsigned int uint;
typedef unsigned char uint8_t;
typedef unsigned short ushort;
typedef ASTCBlock BC7Block;

using namespace lwtt_astc;

#define NUM_TOP_PART_A 16
#define NUM_TOP_PART_B 4 // for mode 0


#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

namespace lwtt_internal
{
	static uint8_t s_shapes2[1024] =
	{
		0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
		0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1,
		0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1,
		0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1,

		0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0,
		0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1,
		0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1,
		0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,

		0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0,
		0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1,
		0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1,

		0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,

		0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1,
		1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1,
		1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1,
		1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,

		0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
		0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
		0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1,
		0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1,

		0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1,
		0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0,
		0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0,
		0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0,

		0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1,
		0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1,
		1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0,

		0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1,
		0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1,
		0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0,
		0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0,

		0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1,
		1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0,
		0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0,
		1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1,

		0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1,
		0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1,
		1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1,
		1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,

		0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0,
		1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0,
		1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0,
		0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0,

		0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0,
		0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0,

		0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1,
		1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1,
		1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0,
		0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0,

		0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1,
		1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0,
		1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0,
		1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1,

		0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0,
		1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0,
		0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1,
		0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1,

	};

#define	REGION2(x,y,si)	s_shapes2[((si)&3)*4+((si)>>2)*64+(x)+(y)*16]

	static int s_shapeindex_to_compressed_indices2[64] =
	{
		15, 15, 15, 15,
		15, 15, 15, 15,
		15, 15, 15, 15,
		15, 15, 15, 15,

		15, 2, 8, 2,
		2, 8, 8, 15,
		2, 8, 2, 2,
		8, 8, 2, 2,

		15, 15, 6, 8,
		2, 8, 15, 15,
		2, 8, 2, 2,
		2, 15, 15, 6,

		6, 2, 6, 8,
		15, 15, 2, 2,
		15, 15, 15, 15,
		15, 2, 2, 15

	};

	static float s_shapes2_pixcounts[64][2] =
	{
		{ 8.0f, 8.0f }, { 12.0f, 4.0f }, { 4.0f, 12.0f }, { 8.0f, 8.0f },
		{ 12.0f, 4.0f }, { 4.0f, 12.0f }, { 6.0f, 10.0f }, { 10.0f, 6.0f },
		{ 13.0f, 3.0f }, { 3.0f, 13.0f }, { 8.0f, 8.0f }, { 12.0f, 4.0f },
		{ 4.0f, 12.0f }, { 8.0f, 8.0f }, { 4.0f, 12.0f }, { 12.0f, 4.0f },

		{ 8.0f, 8.0f }, { 12.0f, 4.0f }, { 12.0f, 4.0f }, { 10.0f, 6.0f },
		{ 13.0f, 3.0f }, { 10.0f, 6.0f }, { 13.0f, 3.0f }, { 8.0f, 8.0f },
		{ 12.0f, 4.0f }, { 12.0f, 4.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f },
		{ 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f },

		{ 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f },
		{ 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f },
		{ 6.0f, 10.0f }, { 10.0f, 6.0f }, { 10.0f, 6.0f }, { 6.0f, 10.0f },
		{ 8.0f, 8.0f }, { 8.0f, 8.0f }, { 10.0f, 6.0f }, { 12.0f, 4.0f },

		{ 11.0f, 5.0f }, { 11.0f, 5.0f }, { 11.0f, 5.0f }, { 11.0f, 5.0f },
		{ 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f },
		{ 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f },
		{ 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f }, { 8.0f, 8.0f }

	};

	static uint8_t s_shapes3[1024] =
	{
		0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 2, 2,
		0, 0, 1, 1, 0, 0, 1, 1, 2, 0, 0, 1, 0, 0, 2, 2,
		0, 2, 2, 1, 2, 2, 1, 1, 2, 2, 1, 1, 0, 0, 1, 1,
		2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 1, 1, 0, 1, 1, 1,

		0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 1, 1,
		0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 1, 1,
		1, 1, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1,
		1, 1, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1,

		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2,
		0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 2,
		1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 1, 2,
		2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 1, 2,

		0, 1, 1, 2, 0, 1, 2, 2, 0, 0, 1, 1, 0, 0, 1, 1,
		0, 1, 1, 2, 0, 1, 2, 2, 0, 1, 1, 2, 2, 0, 0, 1,
		0, 1, 1, 2, 0, 1, 2, 2, 1, 1, 2, 2, 2, 2, 0, 0,
		0, 1, 1, 2, 0, 1, 2, 2, 1, 2, 2, 2, 2, 2, 2, 0,

		0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 2, 2,
		0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 0, 0, 2, 2,
		0, 1, 1, 2, 2, 0, 0, 1, 1, 1, 2, 2, 0, 0, 2, 2,
		1, 1, 2, 2, 2, 2, 0, 0, 1, 1, 2, 2, 1, 1, 1, 1,

		0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0,
		0, 2, 2, 2, 2, 2, 2, 1, 0, 1, 2, 2, 2, 2, 1, 0,
		0, 2, 2, 2, 2, 2, 2, 1, 0, 1, 2, 2, 2, 2, 1, 0,

		0, 1, 2, 2, 0, 0, 1, 2, 0, 1, 1, 0, 0, 0, 0, 0,
		0, 1, 2, 2, 0, 0, 1, 2, 1, 2, 2, 1, 0, 1, 1, 0,
		0, 0, 1, 1, 1, 1, 2, 2, 1, 2, 2, 1, 1, 2, 2, 1,
		0, 0, 0, 0, 2, 2, 2, 2, 0, 1, 1, 0, 1, 2, 2, 1,

		0, 0, 2, 2, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0,
		1, 1, 0, 2, 0, 1, 1, 0, 0, 1, 2, 2, 2, 0, 0, 0,
		1, 1, 0, 2, 2, 0, 0, 2, 0, 1, 2, 2, 2, 2, 1, 1,
		0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 1, 1, 2, 2, 2, 1,

		0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 1, 1, 0, 1, 2, 0,
		0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 1, 2, 0, 1, 2, 0,
		1, 1, 2, 2, 0, 0, 1, 2, 0, 0, 2, 2, 0, 1, 2, 0,
		1, 2, 2, 2, 0, 0, 1, 1, 0, 2, 2, 2, 0, 1, 2, 0,

		0, 0, 0, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 1,
		1, 1, 1, 1, 1, 2, 0, 1, 2, 0, 1, 2, 2, 2, 0, 0,
		2, 2, 2, 2, 2, 0, 1, 2, 1, 2, 0, 1, 1, 1, 2, 2,
		0, 0, 0, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 1,

		0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 2, 2,
		1, 1, 2, 2, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 2, 2,
		2, 2, 0, 0, 2, 2, 2, 2, 2, 1, 2, 1, 0, 0, 2, 2,
		0, 0, 1, 1, 2, 2, 2, 2, 2, 1, 2, 1, 1, 1, 2, 2,

		0, 0, 2, 2, 0, 2, 2, 0, 0, 1, 0, 1, 0, 0, 0, 0,
		0, 0, 1, 1, 1, 2, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1,
		0, 0, 2, 2, 0, 2, 2, 0, 2, 2, 2, 2, 2, 1, 2, 1,
		0, 0, 1, 1, 1, 2, 2, 1, 0, 1, 0, 1, 2, 1, 2, 1,

		0, 1, 0, 1, 0, 2, 2, 2, 0, 0, 0, 2, 0, 0, 0, 0,
		0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2,
		0, 1, 0, 1, 0, 2, 2, 2, 0, 0, 0, 2, 2, 1, 1, 2,
		2, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2,

		0, 2, 2, 2, 0, 0, 0, 2, 0, 1, 1, 0, 0, 0, 0, 0,
		0, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1, 0, 0, 0, 0, 0,
		0, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1, 0, 2, 1, 1, 2,
		0, 2, 2, 2, 0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 2,

		0, 1, 1, 0, 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 0, 0,
		0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0,
		2, 2, 2, 2, 0, 0, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0,
		2, 2, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2, 2, 1, 1, 2,

		0, 0, 0, 2, 0, 2, 2, 2, 0, 1, 0, 1, 0, 1, 1, 1,
		0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 1,
		0, 0, 0, 2, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1,
		0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0,
	};

#define	REGION3(x,y,si)	s_shapes3[((si)&3)*4+((si)>>2)*64+(x)+(y)*16]

	static int s_shapeindex_to_compressed_indices3_1[64] =
	{
		3, 3, 15, 15,
		8, 3, 15, 15,
		8, 8, 6, 6,
		6, 5, 3, 3,

		3, 3, 8, 15,
		3, 3, 6, 10,
		5, 8, 8, 6,
		8, 5, 15, 15,

		8, 15, 3, 5,
		6, 10, 8, 15,
		15, 3, 15, 5,
		15, 15, 15, 15,

		3, 15, 5, 5,
		5, 8, 5, 10,
		5, 10, 8, 13,
		15, 12, 3, 3,
	};

	static int s_shapeindex_to_compressed_indices3_2[64] =
	{
		15, 8, 8, 3,
		15, 15, 3, 8,
		15, 15, 15, 15,
		15, 15, 15, 8,

		15, 8, 15, 3,
		15, 8, 15, 8,
		3, 15, 6, 10,
		15, 15, 10, 8,

		15, 3, 15, 10,
		10, 8, 9, 10,
		6, 15, 8, 15,
		3, 6, 6, 8,

		15, 3, 15, 15,
		15, 15, 15, 15,
		15, 15, 15, 15,
		3, 15, 15, 8
	};

	static float s_shapes3_pixcounts[64][3] =
	{
		{ 5.0f, 5.0f, 6.0f }, { 5.0f, 6.0f, 5.0f }, { 6.0f, 5.0f, 5.0f }, { 6.0f, 5.0f, 5.0f },
		{ 8.0f, 4.0f, 4.0f }, { 8.0f, 4.0f, 4.0f }, { 4.0f, 8.0f, 4.0f }, { 4.0f, 8.0f, 4.0f },
		{ 8.0f, 4.0f, 4.0f }, { 4.0f, 8.0f, 4.0f }, { 4.0f, 4.0f, 8.0f }, { 8.0f, 4.0f, 4.0f },
		{ 4.0f, 8.0f, 4.0f }, { 4.0f, 4.0f, 8.0f }, { 3.0f, 7.0f, 6.0f }, { 7.0f, 3.0f, 6.0f },

		{ 6.0f, 7.0f, 3.0f }, { 7.0f, 6.0f, 3.0f }, { 4.0f, 6.0f, 6.0f }, { 6.0f, 4.0f, 6.0f },
		{ 4.0f, 6.0f, 6.0f }, { 6.0f, 4.0f, 6.0f }, { 8.0f, 4.0f, 4.0f }, { 8.0f, 4.0f, 4.0f },
		{ 8.0f, 4.0f, 4.0f }, { 4.0f, 4.0f, 8.0f }, { 4.0f, 8.0f, 4.0f }, { 6.0f, 6.0f, 4.0f },
		{ 6.0f, 4.0f, 8.0f }, { 6.0f, 4.0f, 6.0f }, { 6.0f, 6.0f, 4.0f }, { 7.0f, 3.0f, 6.0f },

		{ 7.0f, 3.0f, 6.0f }, { 7.0f, 3.0f, 6.0f }, { 7.0f, 3.0f, 6.0f }, { 8.0f, 4.0f, 4.0f },
		{ 8.0f, 4.0f, 4.0f }, { 6.0f, 5.0f, 5.0f }, { 6.0f, 5.0f, 5.0f }, { 6.0f, 6.0f, 4.0f },
		{ 6.0f, 6.0f, 4.0f }, { 4.0f, 4.0f, 8.0f }, { 8.0f, 4.0f, 4.0f }, { 4.0f, 4.0f, 8.0f },
		{ 8.0f, 4.0f, 4.0f }, { 4.0f, 4.0f, 8.0f }, { 4.0f, 4.0f, 8.0f }, { 4.0f, 6.0f, 6.0f },

		{ 6.0f, 6.0f, 4.0f }, { 4.0f, 6.0f, 6.0f }, { 6.0f, 6.0f, 4.0f }, { 4.0f, 6.0f, 6.0f },
		{ 4.0f, 6.0f, 6.0f }, { 6.0f, 6.0f, 4.0f }, { 6.0f, 6.0f, 4.0f }, { 8.0f, 4.0f, 4.0f },
		{ 4.0f, 4.0f, 8.0f }, { 8.0f, 4.0f, 4.0f }, { 4.0f, 4.0f, 8.0f }, { 12.0f, 2.0f, 2.0f },
		{ 12.0f, 2.0f, 2.0f }, { 2.0f, 2.0f, 12.0f }, { 2.0f, 2.0f, 12.0f }, { 4.0f, 6.0f, 6.0f }

	};



	static const uint8_t  IndexQuantize_4bit[65] = { 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15 };
	static const uint8_t  IndexUnquantize_4bit[16] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

	static uint8_t _IndexQuantizeF_4bit(float f_ind)
	{
		int indOut = (int)(f_ind*64.0f + 0.5);
		if (indOut < 0) indOut = 0;
		else if (indOut > 64) indOut = 64;
		uint8_t index_enc = IndexQuantize_4bit[indOut];

		float delta = fabsf((float)IndexUnquantize_4bit[index_enc] / 64.0f - f_ind);

		float deltal = FLT_MAX;
		float deltar = FLT_MAX;

		if (index_enc > 0)
			deltal = fabsf((float)IndexUnquantize_4bit[index_enc - 1] / 64.0f - f_ind);

		if (index_enc < 15)
			deltar = fabsf((float)IndexUnquantize_4bit[index_enc + 1] / 64.0f - f_ind);

		if ((delta <= deltal || index_enc == 0) && (delta <= deltar || index_enc == 15))
			return index_enc;
		else if (((deltal <= deltar && index_enc > 0) || index_enc == 15))
			return index_enc - 1;
		else
			return index_enc + 1;
	}

	static const uint8_t  IndexQuantize_3bit[65] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7 };
	static const uint8_t  IndexUnquantize_3bit[8] = { 0, 9, 18, 27, 37, 46, 55, 64 };

	static uint8_t _IndexQuantizeF_3bit(float f_ind)
	{
		int indOut = (int)(f_ind*64.0f + 0.5f);
		if (indOut < 0) indOut = 0;
		else if (indOut > 64) indOut = 64;
		uint8_t index_enc = IndexQuantize_3bit[indOut];

		float delta = fabsf((float)IndexUnquantize_3bit[index_enc] / 64.0f - f_ind);

		float deltal = FLT_MAX;
		float deltar = FLT_MAX;

		if (index_enc > 0)
			deltal = fabsf((float)IndexUnquantize_3bit[index_enc - 1] / 64.0f - f_ind);

		if (index_enc < 7)
			deltar = fabsf((float)IndexUnquantize_3bit[index_enc + 1] / 64.0f - f_ind);

		if ((delta <= deltal || index_enc == 0) && (delta <= deltar || index_enc == 7))
			return index_enc;
		else if (((deltal <= deltar && index_enc > 0) || index_enc == 7))
			return index_enc - 1;
		else
			return index_enc + 1;
	}

	static const uint8_t IndexUnquantize_2bit[4] = { 0, 21, 43, 64 };

	static uint8_t _IndexQuantizeF_2bit(float f_ind)
	{
		float err = FLT_MAX;

		uint8_t i = 0;
		while (true)
		{
			float err1 = fabsf((float)IndexUnquantize_2bit[i] / 64.0f - f_ind);
			if (err1 >= err)
			{
				i--;
				break;
			}
			err = err1;
			if (i == 3) break;
			i++;
		}
		return i;
	}

	static const uint8_t* IndexUnquantize[5] = { nullptr, nullptr, IndexUnquantize_2bit, IndexUnquantize_3bit, IndexUnquantize_4bit };
	typedef uint8_t(*_IndexQuantizeF_Func)(float f_ind);
	static const _IndexQuantizeF_Func _IndexQuantizeF[5] = { nullptr, nullptr, _IndexQuantizeF_2bit, _IndexQuantizeF_3bit, _IndexQuantizeF_4bit };

	static inline float Clamp01(float x)
	{
		if (x < 0.0f) return 0.0f;
		if (x > 1.0f) return 1.0f;
		return x;
	}

	static inline int _quantize_col(float value, int prec)
	{
		value = Clamp01(value);
		int q, unq;
		unq = (int)floorf(value*255.0f + 0.5f);
		q = (unq * ((1 << prec) - 1) + 127) / 255;
		return q;
	}

	static inline float _unquantize_col(int q, int prec)
	{
		int unq;
		unq = (q << (8 - prec)) | (q >> (2 * prec - 8));
		return (float)unq / 255.0f;
	}


	static inline int _unquantize_col_i(int q, int prec)
	{
		int unq;
		unq = (q << (8 - prec)) | (q >> (2 * prec - 8));
		return unq;
	}

	static inline int _lerp_4bit(int a, int b, int i)
	{
		return (a*IndexUnquantize_4bit[15 - i] + b * IndexUnquantize_4bit[i] + 32) >> 6;
	}
	static inline int _lerp_3bit(int a, int b, int i)
	{
		return (a*IndexUnquantize_3bit[7 - i] + b * IndexUnquantize_3bit[i] + 32) >> 6;
	}
	static inline int _lerp_2bit(int a, int b, int i)
	{
		return (a*IndexUnquantize_2bit[3 - i] + b * IndexUnquantize_2bit[i] + 32) >> 6;
	}


	static void _PCA_Endpoints_bc7_mode6(const Vec4* pixels, Vec4 &ep0, Vec4 &ep1, int vw, int vh)
	{
		Vec4 mean;
		Mat4x4 cov;

		mean = Vec4(0.0f);
		cov = Mat4x4();

		bool hasZeroAlpha = false;
		bool hasOneAlpha = false;

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				Vec4 x = pixels[t];

				hasZeroAlpha |= x[3] == 0.0f;
				hasOneAlpha |= x[3] > 0.998f;

				mean += x;

				for (int i = 0; i < 4; i++)
					for (int j = i; j < 4; j++)
						cov.elem(i, j) += x[i] * x[j];
			}
		}


		float mult = 1.0f / (float)(vh*vw);
		mean *= mult;
		for (int i = 0; i < 4; i++)
			for (int j = i + 1; j < 4; j++)
				cov.elem(j, i) = cov.elem(i, j);

		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
			{
				cov.elem(i, j) *= mult;
				cov.elem(i, j) = cov.elem(i, j) - mean[i] * mean[j];
			}


		Vec4 pc = cov.computePrincipleComponent();

		float pc2 = pc.sqLength();
		if (pc2 <= 0.0f)
		{
			pc = Vec4(1.0f, 1.0f, 1.0f, cov.elem(3, 3) > 0.00001f ? 1.0f : 0.0f);
			pc2 = pc.sqLength();
		}

		pc2 = 1.0f / pc2;

		float minW = FLT_MAX;
		float maxW = -FLT_MAX;

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				Vec4 pix = pixels[t] - mean;

				float w = dot(pix, pc)*pc2;
				if (w < minW)
				{
					minW = w;
				}
				if (w > maxW)
				{
					maxW = w;
				}
			}
		}

		if (minW == maxW)
		{
			minW -= 1.0f / 127.0f;
			maxW += 1.0f / 127.0f;
		}

		Vec4 z(0.0f), o(1.0f);

		ep0 = pc * Vec4(minW) + mean;
		ep1 = pc * Vec4(maxW) + mean;

		ep0 = Min(Max(ep0, z), o);
		ep1 = Min(Max(ep1, z), o);

		if (hasZeroAlpha)
		{
			if (ep0[3] > ep1[3]) ep1[3] = 0.0f;
			else ep0[3] = 0.0f;
		}

		if (hasOneAlpha)
		{
			if (ep0[3] > ep1[3]) ep0[3] = 1.0f;
			else ep1[3] = 1.0f;
		}
	}

	static void _OptimizeEnds(const float inds[16], float& first, float& last, unsigned char quantInd[16], int indexBits)
	{
		first = 0.0f;
		last = 1.0f;

		float old_err = FLT_MAX;

		while (true)
		{
			float sum_err = 0.0f;
			for (int i = 0; i < 16; i++)
			{
				float ind = inds[i];
				int label = _IndexQuantizeF[indexBits]((ind - first) / (last - first));
				float k = (float)IndexUnquantize[indexBits][label] / 64.0f;
				sum_err += fabsf(ind - (first*(1.0f - k) + last * k));
				quantInd[i] = label;
			}
			if (sum_err < old_err)
			{
				old_err = sum_err;
			}
			else break;

			/// min-square optimization (block grain)
			float a1(0.0f), b1(0.0f), c1(0.0f);
			float a2(0.0f), b2(0.0f), c2(0.0f);

			for (int i = 0; i < 16; i++)
			{
				float ind = inds[i];
				int label = quantInd[i];
				float k = (float)IndexUnquantize[indexBits][label] / 64.0f;

				a1 += (1.0f - k)*(1.0f - k);
				b1 += k * (1.0f - k);
				c1 += ind * (1.0f - k);

				//a2+= k*(1.0f-k);
				b2 += k * k;
				c2 += ind * k;
			}
			a2 = b1;
			float delta0 = a1 * b2 - a2 * b1;
			if (delta0 == 0.0f)
			{
				int label = quantInd[0];
				float k = (float)IndexUnquantize[indexBits][label] / 64.0f;
				float center = (c1 + c2) / 16.0f;
				float shift = center - (first*(1.0f - k) + last * k);
				first += shift;
				last += shift;
			}
			else
			{
				float delta1 = c1 * b2 - c2 * b1;
				float delta2 = c2 * a1 - c1 * a2;
				first = delta1 / delta0;
				last = delta2 / delta0;

				if (first == last) break;
			}
		}

	}

	static void _Process_Indices_bc7_mode6(const Vec4* pixels, Vec4 &ep0, Vec4 &ep1, unsigned char quantInd[16], int vw, int vh)
	{
		Vec4 axis = ep1 - ep0;
		float mul = axis.sqLength();
		if (mul > 0.0f) mul = 1.0f / mul;

		float inds[16];
		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				int t = tx + ty * 4;
				if (tx < vw && ty < vh)
				{
					Vec4 pix = pixels[t];
					inds[t] = dot((pix - ep0), axis) * mul;
				}
				else
				{
					inds[t] = 0.5f;
				}
			}
		}

		float first;
		float last;

		_OptimizeEnds(inds, first, last, quantInd, 4);

		Vec4 old_ep0 = ep0;
		Vec4 old_ep1 = ep1;
		ep0 = old_ep0 * Vec4(1.0f - first) + old_ep1 * Vec4(first);
		if (old_ep0[3] == 0.0f) ep0[3] = 0.0f;
		else if (old_ep0[3] > 0.998f) ep0[3] = 1.0f;
		ep1 = old_ep0 * Vec4(1.0f - last) + old_ep1 * Vec4(last);
		if (old_ep1[3] == 0.0f) ep1[3] = 0.0f;
		else if (old_ep1[3] > 0.998f) ep1[3] = 1.0f;

	}

	static void _Process_Colors_bc7_mode6(const Vec4* pixels, unsigned char quantInd[16], Vec4 &ep0, Vec4 &ep1, int vw, int vh)
	{
		Vec4 new_ep0, new_ep1;
		float A = 0.0f, B = 0.0f, C = 0.0f;
		Vec4 D(0.0f), E(0.0f);
		float F = 0.0f;

		float c164 = 1.0f / 64.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				uint8_t qInd = quantInd[t];
				float k = (float)IndexUnquantize_4bit[qInd] * c164;
				float k2 = 1.0f - k;
				A += k2 * k2;
				B += k * k;
				C += k * k2;

				Vec4 pix = pixels[t];
				D += pix * Vec4(k2);
				E += pix * Vec4(k);
				F += pix.sqLength();
			}
		}
		float d = A * B - C * C;
		if (d == 0.0f)
		{
			new_ep0 = ep0;
			new_ep1 = ep1;
		}
		else
		{
			d = 1.0f / d;
			Vec4 d1 = D * Vec4(B) - E * Vec4(C);
			Vec4 d2 = E * Vec4(A) - D * Vec4(C);

			new_ep0 = d1 * Vec4(d);
			new_ep1 = d2 * Vec4(d);

			if (ep0[3] == 0.0f) new_ep0[3] = 0.0f;
			else if (ep0[3] > 0.998f) new_ep0[3] = 1.0f;
			if (ep1[3] == 0.0f) new_ep1[3] = 0.0f;
			else if (ep1[3] > 0.998f) new_ep1[3] = 1.0f;
		}

		int qep0_x = _quantize_col(new_ep0[0], 8);
		int qep0_y = _quantize_col(new_ep0[1], 8);
		int qep0_z = _quantize_col(new_ep0[2], 8);
		int qep0_w = _quantize_col(new_ep0[3], 8);

		int lsb = new_ep0[3] == 0.0f ? 0 : (new_ep0[3] > 0.998f ? 1 : ((qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1) >= 2 ? 1 : 0));
		qep0_x = (qep0_x & 0xFE) | lsb;
		qep0_y = (qep0_y & 0xFE) | lsb;
		qep0_z = (qep0_z & 0xFE) | lsb;
		qep0_w = (qep0_w & 0xFE) | lsb;

		int qep1_x = _quantize_col(new_ep1[0], 8);
		int qep1_y = _quantize_col(new_ep1[1], 8);
		int qep1_z = _quantize_col(new_ep1[2], 8);
		int qep1_w = _quantize_col(new_ep1[3], 8);

		lsb = new_ep1[3] == 0.0f ? 0 : (new_ep1[3] > 0.998f ? 1 : ((qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 2 ? 1 : 0));
		qep1_x = (qep1_x & 0xFE) | lsb;
		qep1_y = (qep1_y & 0xFE) | lsb;
		qep1_z = (qep1_z & 0xFE) | lsb;
		qep1_w = (qep1_w & 0xFE) | lsb;

		ep0[0] = _unquantize_col(qep0_x, 8);
		ep0[1] = _unquantize_col(qep0_y, 8);
		ep0[2] = _unquantize_col(qep0_z, 8);
		ep0[3] = _unquantize_col(qep0_w, 8);

		ep1[0] = _unquantize_col(qep1_x, 8);
		ep1[1] = _unquantize_col(qep1_y, 8);
		ep1[2] = _unquantize_col(qep1_z, 8);
		ep1[3] = _unquantize_col(qep1_w, 8);

	}

	static void _Recalc_Indices_bc7_mode6(const Vec4* pixels, Vec4 &ep0, Vec4 &ep1, unsigned char quantInd[16], int vw, int vh)
	{
		Vec4 axis = ep1 - ep0;
		float mul = axis.sqLength();
		if (mul > 0.0f) mul = 1.0f / mul;

		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				int t = tx + ty * 4;
				float f_ind = 0.5f;

				if (tx < vw && ty < vh)
				{
					Vec4 pix = pixels[t];
					if (pix[3] == 0.0f && ((ep0[3] == 0.0f) ^ (ep1[3] == 0.0f)))
					{
						f_ind = ep0[3] == 0.0f ? 0.0f : 1.0f;
					}
					else if (pix[3] >= 0.998f && ((ep0[3] >= 0.998f) ^ (ep1[3] >= 0.998f)))
					{
						f_ind = ep0[3] >= 0.998f ? 0.0f : 1.0f;
					}
					else
					{
						f_ind = dot((pix - ep0), axis) * mul;
					}
				}
				quantInd[t] = _IndexQuantizeF_4bit(f_ind);
			}
		}

		if (quantInd[0] > 7) // flip
		{
			Vec4 ep_t = ep1;
			ep1 = ep0;
			ep0 = ep_t;

			for (int ty = 0; ty < 4; ty++)
			{
				for (int tx = 0; tx < 4; tx++)
				{
					int t = tx + ty * 4;
					quantInd[t] = 15 - quantInd[t];
				}
			}
		}
	}

	static void _Recalc_Colors_bc7_mode6(const Vec4 &ep0, const Vec4 &ep1, uint compr_ep[8])
	{
		int qep0_x = _quantize_col(ep0[0], 8);
		int qep0_y = _quantize_col(ep0[1], 8);
		int qep0_z = _quantize_col(ep0[2], 8);
		int qep0_w = _quantize_col(ep0[3], 8);

		int lsb = ep0[3] == 0.0f ? 0 : (ep0[3] > 0.998f ? 1 : ((qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1) >= 2 ? 1 : 0));
		qep0_x = (qep0_x & 0xFE) | lsb;
		qep0_y = (qep0_y & 0xFE) | lsb;
		qep0_z = (qep0_z & 0xFE) | lsb;
		qep0_w = (qep0_w & 0xFE) | lsb;

		int qep1_x = _quantize_col(ep1[0], 8);
		int qep1_y = _quantize_col(ep1[1], 8);
		int qep1_z = _quantize_col(ep1[2], 8);
		int qep1_w = _quantize_col(ep1[3], 8);

		lsb = ep1[3] == 0.0f ? 0 : (ep1[3] > 0.998f ? 1 : ((qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 2 ? 1 : 0));
		qep1_x = (qep1_x & 0xFE) | lsb;
		qep1_y = (qep1_y & 0xFE) | lsb;
		qep1_z = (qep1_z & 0xFE) | lsb;
		qep1_w = (qep1_w & 0xFE) | lsb;

		compr_ep[0] = (uint)qep0_x;
		compr_ep[1] = (uint)qep1_x;
		compr_ep[2] = (uint)qep0_y;
		compr_ep[3] = (uint)qep1_y;
		compr_ep[4] = (uint)qep0_z;
		compr_ep[5] = (uint)qep1_z;
		compr_ep[6] = (uint)qep0_w;
		compr_ep[7] = (uint)qep1_w;
	}

	static float _BlockErr_mode6(const Vec4* pixels, const uint compr_ep[8], const uint8_t quant_inds[16], int vw, int vh)
	{
		int intEp0_x = _unquantize_col_i((int)compr_ep[0], 8);
		int intEp0_y = _unquantize_col_i((int)compr_ep[2], 8);
		int intEp0_z = _unquantize_col_i((int)compr_ep[4], 8);
		int intEp0_w = _unquantize_col_i((int)compr_ep[6], 8);

		int intEp1_x = _unquantize_col_i((int)compr_ep[1], 8);
		int intEp1_y = _unquantize_col_i((int)compr_ep[3], 8);
		int intEp1_z = _unquantize_col_i((int)compr_ep[5], 8);
		int intEp1_w = _unquantize_col_i((int)compr_ep[7], 8);

		Vec4 palette[16];
		for (int i = 0; i < 16; i++)
		{
			Vec4& item = palette[i];
			item[0] = (float)_lerp_4bit(intEp0_x, intEp1_x, i) / 255.0f;
			item[1] = (float)_lerp_4bit(intEp0_y, intEp1_y, i) / 255.0f;
			item[2] = (float)_lerp_4bit(intEp0_z, intEp1_z, i) / 255.0f;
			item[3] = (float)_lerp_4bit(intEp0_w, intEp1_w, i) / 255.0f;
		}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				Vec4 diff = palette[quant_inds[t]] - pixels[t];
				err += diff.sqLength();
			}
		}

		return err;
	}

	static void _CompressBC7_mode6(const Vec4* pixels, BC7Block& block, float& err, int vw, int vh)
	{
		Vec4 ep0, ep1;
		_PCA_Endpoints_bc7_mode6(pixels, ep0, ep1, vw, vh);

		unsigned char quantInd[16];
		_Process_Indices_bc7_mode6(pixels, ep0, ep1, quantInd, vw, vh);
		_Process_Colors_bc7_mode6(pixels, quantInd, ep0, ep1, vw, vh);
		_Recalc_Indices_bc7_mode6(pixels, ep0, ep1, quantInd, vw, vh);

		uint compr_ep[8];
		_Recalc_Colors_bc7_mode6(ep0, ep1, compr_ep);

		float lwr_err = _BlockErr_mode6(pixels, compr_ep, quantInd, vw, vh);
		if (lwr_err < err)
		{
			int base = 0;
			block.setBits(base, 7, (uint)(1 << 6));

			for (unsigned j = 0; j < 8; j++)
			{
				block.setBits(base, 7, compr_ep[j] >> 1);
			}
			block.setBits(base, 1, compr_ep[0]);
			block.setBits(base, 1, compr_ep[1]);

			block.setBits(base, 3, (uint)quantInd[0]);

			for (unsigned j = 1; j < 16; j++)
				block.setBits(base, 4, (uint)quantInd[j]);

			err = lwr_err;

		}

	}

	static void _PCA_Endpoints2_bc7_mode_1_3(const Vec4* pixels, int partId, uint8_t p, Vec4& ep0, Vec4& ep1, int vw, int vh)
	{
		int total = 0;
		Vec4 acc = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
		float acc_xx(0.0f), acc_yy(0.0f), acc_zz(0.0f);
		float acc_xy(0.0f), acc_xz(0.0f);
		float acc_yz(0.0f);

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				if (REGION2(tx, ty, partId) == p)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t];
					acc += pix;

					acc_xx += pix[0] * pix[0]; acc_yy += pix[1] * pix[1]; acc_zz += pix[2] * pix[2];
					acc_xy += pix[0] * pix[1]; acc_xz += pix[0] * pix[2];
					acc_yz += pix[1] * pix[2];
					total++;
				}
			}
		}

		float mult = total > 0 ? 1.0f / (float)(total) : 0.0f;
		acc *= mult;
		acc_xx *= mult; acc_yy *= mult;  acc_zz *= mult;
		acc_xy *= mult; acc_xz *= mult;
		acc_yz *= mult;

		acc_xx = acc_xx - acc[0] * acc[0]; acc_yy = acc_yy - acc[1] * acc[1]; acc_zz = acc_zz - acc[2] * acc[2];
		acc_xy = acc_xy - acc[0] * acc[1]; acc_xz = acc_xz - acc[0] * acc[2];
		acc_yz = acc_yz - acc[1] * acc[2];

		// estimatePrincipleComponent
		Vec4 dir = Vec4(acc_xx, acc_xy, acc_xz, 0.0f);
		float bestLen = dir.sqLength();

		{
			Vec4 dir2 = Vec4(acc_xy, acc_yy, acc_yz, 0.0f);
			float len = dir2.sqLength();
			if (len > bestLen)
			{
				bestLen = len;
				dir = dir2;
			}

			dir2 = Vec4(acc_xz, acc_yz, acc_zz, 0.0f);
			len = dir2.sqLength();
			if (len > bestLen)
			{
				bestLen = len;
				dir = dir2;
			}
		}

		//computePrincipleComponent
		for (int i = 0; i < POWER_ITERATION_COUNT; i++)
		{
			dir = Vec4(
				acc_xx*dir[0] + acc_xy * dir[1] + acc_xz * dir[2],
				acc_xy*dir[0] + acc_yy * dir[1] + acc_yz * dir[2],
				acc_xz*dir[0] + acc_yz * dir[1] + acc_zz * dir[2],
				0.0f);

			float norm = fmaxf(fmaxf(dir[0], dir[1]), dir[2]);
			if (norm == 0.0f) break;
			float ilw = 1.0f / norm;
			dir *= ilw;
		}

		float pc2 = dir.sqLength();
		if (pc2 <= 0.0f)
		{
			dir = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
			pc2 = dir.sqLength();
		}

		pc2 = 1.0f / pc2;

		float minW = FLT_MAX;
		float maxW = -FLT_MAX;

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				if (REGION2(tx, ty, partId) == p)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t] - acc;

					float w = dot(pix, dir)*pc2;

					if (w < minW)
					{
						minW = w;
					}
					if (w > maxW)
					{
						maxW = w;
					}
				}
			}
		}

		if (minW == maxW)
		{
			minW -= 1.0f / 63.0f;
			maxW += 1.0f / 63.0f;
		}

		ep0 = Vec4(dir[0] * minW, dir[1] * minW, dir[2] * minW, 0.0f) + acc;
		ep1 = Vec4(dir[0] * maxW, dir[1] * maxW, dir[2] * maxW, 0.0f) + acc;

		ep0[0] = Clamp01(ep0[0]); ep0[1] = Clamp01(ep0[1]); ep0[2] = Clamp01(ep0[2]); ep0[3] = 0.0f;
		ep1[0] = Clamp01(ep1[0]); ep1[1] = Clamp01(ep1[1]); ep1[2] = Clamp01(ep1[2]); ep1[3] = 0.0f;

	}

	static void _SelectBestPartition_bc7_mode_1_3(const Vec4* pixels, uint8_t bestPart[NUM_TOP_PART_A], int vw, int vh)
	{
		float bestErrs[NUM_TOP_PART_A];
		for (int i = 0; i < NUM_TOP_PART_A; i++) bestErrs[i] = FLT_MAX;

		for (int partId = 0; partId < 64; partId++)
		{
			Vec4 ep0[2];
			Vec4 ep1[2];

			_PCA_Endpoints2_bc7_mode_1_3(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints2_bc7_mode_1_3(pixels, partId, 1, ep0[1], ep1[1], vw, vh);

			Vec4 axis[2];
			axis[0] = ep1[0] - ep0[0];
			axis[1] = ep1[1] - ep0[1];

			float mul[2];
			mul[0] = axis[0].sqLength();
			mul[1] = axis[1].sqLength();

			if (mul[0] > 0.0f) mul[0] = 1.0f / mul[0];
			if (mul[1] > 0.0f) mul[1] = 1.0f / mul[1];

			float err = 0.0f;

			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t];
					pix[3] = 0.0f;
					uint8_t p = REGION2(tx, ty, partId);
					float f_ind = dot((pix - ep0[p]), axis[p]) * mul[p];
					f_ind = Clamp01(f_ind);
					Vec4 pix2 = ep0[p] * Vec4(1.0f - f_ind) + ep1[p] * Vec4(f_ind);
					pix2[3] = 0.0f;
					err += (pix - pix2).sqLength();
				}
			}

			for (int i = 0; i < NUM_TOP_PART_A; i++)
			{
				if (err < bestErrs[i])
				{
					for (int j = NUM_TOP_PART_A - 1; j > i; j--)
					{
						bestErrs[j] = bestErrs[j - 1];
						bestPart[j] = bestPart[j - 1];
					}
					bestErrs[i] = err;
					bestPart[i] = partId;

					break;
				}
			}
		}
	}

	static void _Process_Indices2_bc7_mode_1_3(const Vec4* pixels, int partId, Vec4 ep0[2], Vec4 ep1[2], unsigned char quantInd[16], int indexBits, int vw, int vh)
	{
		Vec4 axis[2];
		axis[0] = ep1[0] - ep0[0];
		axis[1] = ep1[1] - ep0[1];

		float mul[2];
		mul[0] = axis[0].sqLength();
		if (mul[0] > 0.0f) mul[0] = 1.0f / mul[0];
		mul[1] = axis[1].sqLength();
		if (mul[1] > 0.0f) mul[1] = 1.0f / mul[1];

		float inds[16];
		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				int t = tx + ty * 4;
				if (tx < vw && ty < vh)
				{
					uint8_t p = REGION2(tx, ty, partId);
					Vec4 pix = pixels[t];
					inds[t] = dot((pix - ep0[p]), axis[p]) * mul[p];
				}
				else
				{
					inds[t] = 0.5f;
				}
			}
		}

		float first[2] = { 0.0f, 0.0f };
		float last[2] = { 1.0f, 1.0f };

		float old_err = FLT_MAX;

		while (true)
		{
			float sum_err = 0.0f;
			for (int ty = 0; ty < 4; ty++)
			{
				for (int tx = 0; tx < 4; tx++)
				{
					uint8_t p = REGION2(tx, ty, partId);
					int t = tx + ty * 4;
					float ind = inds[t];
					int label = _IndexQuantizeF[indexBits]((ind - first[p]) / (last[p] - first[p]));
					float k = (float)IndexUnquantize[indexBits][label] / 64.0f;
					sum_err += fabsf(ind - (first[p] * (1.0f - k) + last[p] * k));
					quantInd[t] = label;
				}
			}
			if (sum_err < old_err)
			{
				old_err = sum_err;
			}
			else break;

			float a1[2] = { 0.0f, 0.0f };
			float b1[2] = { 0.0f, 0.0f };
			float c1[2] = { 0.0f, 0.0f };
			float a2[2] = { 0.0f, 0.0f };
			float b2[2] = { 0.0f, 0.0f };
			float c2[2] = { 0.0f, 0.0f };

			int labels[2];

			for (int ty = 0; ty < 4; ty++)
			{
				for (int tx = 0; tx < 4; tx++)
				{
					uint8_t p = REGION2(tx, ty, partId);
					int t = tx + ty * 4;

					float ind = inds[t];
					int label = labels[p] = quantInd[t];
					float k = (float)IndexUnquantize[indexBits][label] / 64.0f;

					a1[p] += (1.0f - k)*(1.0f - k);
					b1[p] += k * (1.0f - k);
					c1[p] += ind * (1.0f - k);

					//a2[p]+= k*(1.0f-k);
					b2[p] += k * k;
					c2[p] += ind * k;
				}
			}
			a2[0] = b1[0];
			a2[1] = b1[1];

			for (uint8_t p = 0; p < 2; p++)
			{
				float delta0 = a1[p] * b2[p] - a2[p] * b1[p];
				if (delta0 == 0.0f)
				{
					int label = labels[p];
					float k = (float)IndexUnquantize[indexBits][label] / 64.0f;
					//float k = sqrtf(b2[p] / s_shapes2_pixcounts[partId][p]);
					float center = (c1[p] + c2[p]) / s_shapes2_pixcounts[partId][p];
					float shift = center - (first[p] * (1.0f - k) + last[p] * k);
					first[p] += shift;
					last[p] += shift;
				}
				else
				{
					float delta1 = c1[p] * b2[p] - c2[p] * b1[p];
					float delta2 = c2[p] * a1[p] - c1[p] * a2[p];

					first[p] = delta1 / delta0;
					last[p] = delta2 / delta0;
				}
			}
		}
		for (uint8_t p = 0; p < 2; p++)
		{
			Vec4 old_ep0 = ep0[p];
			Vec4 old_ep1 = ep1[p];
			ep0[p] = old_ep0 * Vec4(1.0f - first[p]) + old_ep1 * Vec4(first[p]);
			if (old_ep0[3] == 0.0f) ep0[p][3] = 0.0f;
			else if (old_ep0[3] > 0.998f) ep0[p][3] = 1.0f;
			ep1[p] = old_ep0 * Vec4(1.0f - last[p]) + old_ep1 * Vec4(last[p]);
			if (old_ep1[3] == 0.0f) ep1[p][3] = 0.0f;
			else if (old_ep1[3] > 0.998f) ep1[p][3] = 1.0f;
		}
	}

	static void _Process_Colors2_bc7_mode1(const Vec4* pixels, int partId, unsigned char quantInd[16], Vec4 ep0[2], Vec4 ep1[2], int vw, int vh)
	{
		for (int p = 0; p < 2; p++)
		{
			Vec4 new_ep0, new_ep1;
			float A = 0.0f, B = 0.0f, C = 0.0f;
			Vec4 D(0.0f), E(0.0f);
			float F = 0.0f;

			float c164 = 1.0f / 64.0f;
			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					uint8_t lwrP = REGION2(tx, ty, partId);
					if (p != lwrP) continue;

					int t = tx + ty * 4;
					uint8_t qInd = quantInd[t];
					float k = (float)IndexUnquantize_3bit[qInd] * c164;
					float k2 = 1.0f - k;
					A += k2 * k2;
					B += k * k;
					C += k * k2;

					Vec4 pix = pixels[t];
					pix[3] = 0.0f;
					D += pix * Vec4(k2);
					E += pix * Vec4(k);
					F += pix.sqLength();
				}
			}
			float d = A * B - C * C;
			if (d == 0.0f)
			{
				new_ep0 = ep0[p];
				new_ep1 = ep1[p];
			}
			else
			{
				d = 1.0f / d;
				Vec4 d1 = D * Vec4(B) - E * Vec4(C);
				Vec4 d2 = E * Vec4(A) - D * Vec4(C);

				new_ep0 = d1 * Vec4(d);
				new_ep1 = d2 * Vec4(d);
			}

			int qep0_x = _quantize_col(new_ep0[0], 7);
			int qep0_y = _quantize_col(new_ep0[1], 7);
			int qep0_z = _quantize_col(new_ep0[2], 7);
			int qep1_x = _quantize_col(new_ep1[0], 7);
			int qep1_y = _quantize_col(new_ep1[1], 7);
			int qep1_z = _quantize_col(new_ep1[2], 7);

			int lsb = (qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1)
				+ (qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 3 ? 1 : 0;

			qep0_x = (qep0_x & 0x7E) | lsb;
			qep0_y = (qep0_y & 0x7E) | lsb;
			qep0_z = (qep0_z & 0x7E) | lsb;
			qep1_x = (qep1_x & 0x7E) | lsb;
			qep1_y = (qep1_y & 0x7E) | lsb;
			qep1_z = (qep1_z & 0x7E) | lsb;

			ep0[p][0] = _unquantize_col(qep0_x, 7);
			ep0[p][1] = _unquantize_col(qep0_y, 7);
			ep0[p][2] = _unquantize_col(qep0_z, 7);

			ep1[p][0] = _unquantize_col(qep1_x, 7);
			ep1[p][1] = _unquantize_col(qep1_y, 7);
			ep1[p][2] = _unquantize_col(qep1_z, 7);

		}
	}

	static void _Recalc_Indices2_bc7_mode_1_3(const Vec4* pixels, int partId, Vec4 ep0[2], Vec4 ep1[2], unsigned char quantInd[16], int indexBits, int vw, int vh)
	{
		int compr_ind = s_shapeindex_to_compressed_indices2[partId];

		Vec4 axis[2];
		axis[0] = ep1[0] - ep0[0];
		axis[1] = ep1[1] - ep0[1];

		float mul[2];
		mul[0] = axis[0].sqLength();
		if (mul[0] > 0.0f) mul[0] = 1.0f / mul[0];
		mul[1] = axis[1].sqLength();
		if (mul[1] > 0.0f) mul[1] = 1.0f / mul[1];

		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				uint8_t p = REGION2(tx, ty, partId);
				int t = tx + ty * 4;
				Vec4 pix = pixels[t];
				float f_ind = 0.5f;

				if (tx < vw && ty < vh)
				{
					if (pix[3] == 0.0f && ((ep0[p][3] == 0.0f) ^ (ep1[p][3] == 0.0f)))
					{
						f_ind = ep0[p][3] == 0.0f ? 0.0f : 1.0f;
					}
					else if (pix[3] > 0.998f && ((ep0[p][3] > 0.998f) ^ (ep1[p][3] > 0.998f)))
					{
						f_ind = ep0[p][3] > 0.998f ? 0.0f : 1.0f;
					}
					else
					{
						f_ind = dot((pix - ep0[p]), axis[p]) * mul[p];
					}
				}
				quantInd[t] = _IndexQuantizeF[indexBits](f_ind);
			}
		}

		bool flip[2] = { false, false };
		if (quantInd[0] > ((1 << (indexBits - 1)) - 1))
		{
			flip[0] = true;
			Vec4 t = ep0[0];
			ep0[0] = ep1[0];
			ep1[0] = t;
		}
		if (quantInd[compr_ind] > ((1 << (indexBits - 1)) - 1))
		{
			flip[1] = true;
			Vec4 t = ep0[1];
			ep0[1] = ep1[1];
			ep1[1] = t;
		}

		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				uint8_t p = REGION2(tx, ty, partId);
				int t = tx + ty * 4;
				if (flip[p])
				{
					quantInd[t] = ((1 << indexBits) - 1) - quantInd[t];
				}
			}
		}
	}

	static void _Recalc_Colors2_bc7_mode1(const Vec4 ep0[2], const Vec4 ep1[2], uint compr_ep[12])
	{
		for (int p = 0; p < 2; p++)
		{
			int qep0_x = _quantize_col(ep0[p][0], 7);
			int qep0_y = _quantize_col(ep0[p][1], 7);
			int qep0_z = _quantize_col(ep0[p][2], 7);
			int qep1_x = _quantize_col(ep1[p][0], 7);
			int qep1_y = _quantize_col(ep1[p][1], 7);
			int qep1_z = _quantize_col(ep1[p][2], 7);

			int lsb = (qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1)
				+ (qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 3 ? 1 : 0;

			qep0_x = (qep0_x & 0x7E) | lsb;
			qep0_y = (qep0_y & 0x7E) | lsb;
			qep0_z = (qep0_z & 0x7E) | lsb;
			qep1_x = (qep1_x & 0x7E) | lsb;
			qep1_y = (qep1_y & 0x7E) | lsb;
			qep1_z = (qep1_z & 0x7E) | lsb;

			compr_ep[0 + p * 2] = (uint)qep0_x;
			compr_ep[1 + p * 2] = (uint)qep1_x;
			compr_ep[4 + p * 2] = (uint)qep0_y;
			compr_ep[5 + p * 2] = (uint)qep1_y;
			compr_ep[8 + p * 2] = (uint)qep0_z;
			compr_ep[9 + p * 2] = (uint)qep1_z;

		}
	}

	static float _BlockErr_mode_1(const Vec4* pixels, const uint compr_ep[12], const uint8_t quant_inds[16], int partId, int vw, int vh)
	{
		int uqcol[12];
		for (int i = 0; i < 12; i++)
		{
			uqcol[i] = _unquantize_col_i((int)compr_ep[i], 7);
		}

		Vec4 palettes[2][8];
		for (int p = 0; p < 2; p++)
			for (int i = 0; i < 8; i++)
			{
				palettes[p][i][0] = (float)_lerp_3bit(uqcol[0 + p * 2], uqcol[1 + p * 2], i) / 255.0f;
				palettes[p][i][1] = (float)_lerp_3bit(uqcol[4 + p * 2], uqcol[5 + p * 2], i) / 255.0f;
				palettes[p][i][2] = (float)_lerp_3bit(uqcol[8 + p * 2], uqcol[9 + p * 2], i) / 255.0f;
				palettes[p][i][3] = 1.0f;
			}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				uint8_t p = REGION2(tx, ty, partId);
				int t = tx + ty * 4;
				uint8_t qInd = quant_inds[t];

				Vec4 recon = palettes[p][qInd];
				Vec4 pix = pixels[t];
				Vec4 diff = recon - pix;
				err += diff.sqLength();
			}
		}
		return err;
	}

	static void _CompressBC7_mode_1(const Vec4* pixels, BC7Block& block, float& err, const uint8_t bestPart[NUM_TOP_PART_A], int vw, int vh)
	{
		float bestErr = FLT_MAX;
		int bestPartId = 0;
		unsigned char bestQuantInd[16];
		uint best_compr_ep[12];

		for (int topId = 0; topId < NUM_TOP_PART_A; topId++)
		{
			int partId = bestPart[topId];

			Vec4 ep0[2];
			Vec4 ep1[2];
			_PCA_Endpoints2_bc7_mode_1_3(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints2_bc7_mode_1_3(pixels, partId, 1, ep0[1], ep1[1], vw, vh);

			unsigned char quantInd[16];
			_Process_Indices2_bc7_mode_1_3(pixels, partId, ep0, ep1, quantInd, 3, vw, vh);
			_Process_Colors2_bc7_mode1(pixels, partId, quantInd, ep0, ep1, vw, vh);
			_Recalc_Indices2_bc7_mode_1_3(pixels, partId, ep0, ep1, quantInd, 3, vw, vh);

			uint compr_ep[12];
			_Recalc_Colors2_bc7_mode1(ep0, ep1, compr_ep);

			float lwr_err = _BlockErr_mode_1(pixels, compr_ep, quantInd, partId, vw, vh);
			if (lwr_err < bestErr)
			{
				bestErr = lwr_err;
				bestPartId = partId;
				memcpy(bestQuantInd, quantInd, 16);
				memcpy(best_compr_ep, compr_ep, sizeof(uint) * 12);
			}
		}

		if (bestErr < err)
		{
			int base = 0;
			block.setBits(base, 2, 2);
			block.setBits(base, 6, bestPartId);

			for (unsigned j = 0; j < 12; j++)
			{
				block.setBits(base, 6, best_compr_ep[j] >> 1);
			}
			block.setBits(base, 1, best_compr_ep[0]);
			block.setBits(base, 1, best_compr_ep[2]);

			for (unsigned j = 0; j < 16; j++)
			{
				if (j == 0 || j == (unsigned)s_shapeindex_to_compressed_indices2[bestPartId])
				{
					block.setBits(base, 2, (uint)bestQuantInd[j]);
				}
				else
				{
					block.setBits(base, 3, (uint)bestQuantInd[j]);
				}
			}
			err = bestErr;
		}
	}

	static void _Process_Colors2_bc7_mode3(const Vec4* pixels, int partId, unsigned char quantInd[16], Vec4 ep0[2], Vec4 ep1[2], int vw, int vh)
	{
		for (int p = 0; p < 2; p++)
		{
			Vec4 new_ep0, new_ep1;
			float A = 0.0f, B = 0.0f, C = 0.0f;
			Vec4 D(0.0f), E(0.0f);
			float F = 0.0f;

			float c164 = 1.0f / 64.0f;
			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					uint8_t lwrP = REGION2(tx, ty, partId);
					if (p != lwrP) continue;

					int t = tx + ty * 4;
					uint8_t qInd = quantInd[t];
					float k = (float)IndexUnquantize_2bit[qInd] * c164;
					float k2 = 1.0f - k;
					A += k2 * k2;
					B += k * k;
					C += k * k2;

					Vec4 pix = pixels[t];
					pix[3] = 0.0f;
					D += pix * Vec4(k2);
					E += pix * Vec4(k);
					F += pix.sqLength();
				}
			}
			float d = A * B - C * C;
			if (d == 0.0f)
			{
				new_ep0 = ep0[p];
				new_ep1 = ep1[p];
			}
			else
			{
				d = 1.0f / d;
				Vec4 d1 = D * Vec4(B) - E * Vec4(C);
				Vec4 d2 = E * Vec4(A) - D * Vec4(C);

				new_ep0 = d1 * Vec4(d);
				new_ep1 = d2 * Vec4(d);
			}

			int qep0_x = _quantize_col(new_ep0[0], 8);
			int qep0_y = _quantize_col(new_ep0[1], 8);
			int qep0_z = _quantize_col(new_ep0[2], 8);

			int lsb = (qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1) >= 2 ? 1 : 0;
			qep0_x = (qep0_x & 0xFE) | lsb;
			qep0_y = (qep0_y & 0xFE) | lsb;
			qep0_z = (qep0_z & 0xFE) | lsb;

			int qep1_x = _quantize_col(new_ep1[0], 8);
			int qep1_y = _quantize_col(new_ep1[1], 8);
			int qep1_z = _quantize_col(new_ep1[2], 8);

			lsb = (qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 2 ? 1 : 0;
			qep1_x = (qep1_x & 0xFE) | lsb;
			qep1_y = (qep1_y & 0xFE) | lsb;
			qep1_z = (qep1_z & 0xFE) | lsb;

			ep0[p][0] = _unquantize_col(qep0_x, 8);
			ep0[p][1] = _unquantize_col(qep0_y, 8);
			ep0[p][2] = _unquantize_col(qep0_z, 8);

			ep1[p][0] = _unquantize_col(qep1_x, 8);
			ep1[p][1] = _unquantize_col(qep1_y, 8);
			ep1[p][2] = _unquantize_col(qep1_z, 8);

		}
	}

	static void _Recalc_Colors2_bc7_mode3(const Vec4 ep0[2], const Vec4 ep1[2], uint compr_ep[12])
	{
		for (int p = 0; p < 2; p++)
		{
			int qep0_x = _quantize_col(ep0[p][0], 8);
			int qep0_y = _quantize_col(ep0[p][1], 8);
			int qep0_z = _quantize_col(ep0[p][2], 8);

			int lsb = (qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1) >= 2 ? 1 : 0;

			qep0_x = (qep0_x & 0xFE) | lsb;
			qep0_y = (qep0_y & 0xFE) | lsb;
			qep0_z = (qep0_z & 0xFE) | lsb;

			int qep1_x = _quantize_col(ep1[p][0], 8);
			int qep1_y = _quantize_col(ep1[p][1], 8);
			int qep1_z = _quantize_col(ep1[p][2], 8);

			lsb = (qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 2 ? 1 : 0;

			qep1_x = (qep1_x & 0xFE) | lsb;
			qep1_y = (qep1_y & 0xFE) | lsb;
			qep1_z = (qep1_z & 0xFE) | lsb;

			compr_ep[0 + p * 2] = (uint)qep0_x;
			compr_ep[1 + p * 2] = (uint)qep1_x;
			compr_ep[4 + p * 2] = (uint)qep0_y;
			compr_ep[5 + p * 2] = (uint)qep1_y;
			compr_ep[8 + p * 2] = (uint)qep0_z;
			compr_ep[9 + p * 2] = (uint)qep1_z;
		}
	}

	static float _BlockErr_mode_3(const Vec4* pixels, const uint compr_ep[12], const uint8_t quant_inds[16], int partId, int vw, int vh)
	{
		int uqcol[12];
		for (int i = 0; i < 12; i++)
		{
			uqcol[i] = _unquantize_col_i((int)compr_ep[i], 8);
		}

		Vec4 palettes[2][4];
		for (int p = 0; p < 2; p++)
			for (int i = 0; i < 4; i++)
			{
				palettes[p][i][0] = (float)_lerp_2bit(uqcol[0 + p * 2], uqcol[1 + p * 2], i) / 255.0f;
				palettes[p][i][1] = (float)_lerp_2bit(uqcol[4 + p * 2], uqcol[5 + p * 2], i) / 255.0f;
				palettes[p][i][2] = (float)_lerp_2bit(uqcol[8 + p * 2], uqcol[9 + p * 2], i) / 255.0f;
				palettes[p][i][3] = 1.0f;
			}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				uint8_t p = REGION2(tx, ty, partId);
				int t = tx + ty * 4;
				uint8_t qInd = quant_inds[t];

				Vec4 recon = palettes[p][qInd];
				Vec4 pix = pixels[t];
				Vec4 diff = recon - pix;
				err += diff.sqLength();

			}
		}
		return err;
	}

	static void _CompressBC7_mode_3(const Vec4* pixels, BC7Block& block, float& err, const uint8_t bestPart[NUM_TOP_PART_A], int vw, int vh)
	{
		float bestErr = FLT_MAX;
		int bestPartId = 0;
		unsigned char bestQuantInd[16];
		uint best_compr_ep[12];

		for (int topId = 0; topId < NUM_TOP_PART_A; topId++)
		{
			int partId = bestPart[topId];

			Vec4 ep0[2];
			Vec4 ep1[2];
			_PCA_Endpoints2_bc7_mode_1_3(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints2_bc7_mode_1_3(pixels, partId, 1, ep0[1], ep1[1], vw, vh);

			unsigned char quantInd[16];
			_Process_Indices2_bc7_mode_1_3(pixels, partId, ep0, ep1, quantInd, 2, vw, vh);
			_Process_Colors2_bc7_mode3(pixels, partId, quantInd, ep0, ep1, vw, vh);
			_Recalc_Indices2_bc7_mode_1_3(pixels, partId, ep0, ep1, quantInd, 2, vw, vh);

			uint compr_ep[12];
			_Recalc_Colors2_bc7_mode3(ep0, ep1, compr_ep);

			float lwr_err = _BlockErr_mode_3(pixels, compr_ep, quantInd, partId, vw, vh);
			if (lwr_err < bestErr)
			{
				bestErr = lwr_err;
				bestPartId = partId;
				memcpy(bestQuantInd, quantInd, 16);
				memcpy(best_compr_ep, compr_ep, sizeof(uint) * 12);
			}
		}

		if (bestErr < err)
		{
			int base = 0;
			block.setBits(base, 4, 8);
			block.setBits(base, 6, bestPartId);

			for (unsigned j = 0; j < 12; j++)
			{
				block.setBits(base, 7, best_compr_ep[j] >> 1);
			}
			block.setBits(base, 1, best_compr_ep[0]);
			block.setBits(base, 1, best_compr_ep[1]);
			block.setBits(base, 1, best_compr_ep[2]);
			block.setBits(base, 1, best_compr_ep[3]);

			for (unsigned j = 0; j < 16; j++)
			{
				if (j == 0 || j == (unsigned)s_shapeindex_to_compressed_indices2[bestPartId])
				{
					block.setBits(base, 1, (uint)bestQuantInd[j]);
				}
				else
				{
					block.setBits(base, 2, (uint)bestQuantInd[j]);
				}
			}

			err = bestErr;
		}

	}

	static void _CompressBC7_mode_1_3(const Vec4* pixels, BC7Block& block, float& err, int vw, int vh)
	{
		uint8_t bestPart[NUM_TOP_PART_A];
		_SelectBestPartition_bc7_mode_1_3(pixels, bestPart, vw, vh);

		_CompressBC7_mode_1(pixels, block, err, bestPart, vw, vh);
		_CompressBC7_mode_3(pixels, block, err, bestPart, vw, vh);

	}

	static void _PCA_Endpoints2_bc7_mode7(const Vec4* pixels, int partId, uint8_t p, Vec4& ep0, Vec4& ep1, int vw, int vh)
	{
		Vec4 mean;
		Mat4x4 cov;

		mean = Vec4(0.0f);
		cov = Mat4x4();

		int total = 0;

		bool hasZeroAlpha = false;
		bool hasOneAlpha = false;

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				if (REGION2(tx, ty, partId) == p)
				{
					int t = tx + ty * 4;
					Vec4 x = pixels[t];

					hasZeroAlpha |= x[3] == 0.0f;
					hasOneAlpha |= x[3] > 0.998f;

					mean += x;

					for (int i = 0; i < 4; i++)
						for (int j = i; j < 4; j++)
							cov.elem(i, j) += x[i] * x[j];

					total++;
				}
			}
		}

		float mult = total > 0 ? 1.0f / (float)(total) : 0.0f;
		mean *= mult;
		for (int i = 0; i < 4; i++)
			for (int j = i + 1; j < 4; j++)
				cov.elem(j, i) = cov.elem(i, j);

		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
			{
				cov.elem(i, j) *= mult;
				cov.elem(i, j) = cov.elem(i, j) - mean[i] * mean[j];
			}

		Vec4 pc = cov.computePrincipleComponent();

		float pc2 = pc.sqLength();
		if (pc2 <= 0.0f)
		{
			pc = Vec4(1.0f, 1.0f, 1.0f, cov.elem(3, 3) > 0.00001f ? 1.0f : 0.0f);
			pc2 = pc.sqLength();
		}

		pc2 = 1.0f / pc2;

		float minW = FLT_MAX;
		float maxW = -FLT_MAX;

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				if (REGION2(tx, ty, partId) == p)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t] - mean;

					float w = dot(pix, pc)*pc2;
					if (w < minW)
					{
						minW = w;
					}
					if (w > maxW)
					{
						maxW = w;
					}
				}
			}

			if (minW == maxW)
			{
				minW -= 1.0f / 31.0f;
				maxW += 1.0f / 31.0f;
			}

			Vec4 z(0.0f), o(1.0f);

			ep0 = pc * Vec4(minW) + mean;
			ep1 = pc * Vec4(maxW) + mean;

			ep0 = Min(Max(ep0, z), o);
			ep1 = Min(Max(ep1, z), o);

			if (hasZeroAlpha)
			{
				if (ep0[3] > ep1[3]) ep1[3] = 0.0f;
				else ep0[3] = 0.0f;
			}

			if (hasOneAlpha)
			{
				if (ep0[3] > ep1[3]) ep0[3] = 1.0f;
				else ep1[3] = 1.0f;
			}
		}

	}

	static void _SelectBestPartition_bc7_mode7(const Vec4* pixels, uint8_t bestPart[NUM_TOP_PART_A], int vw, int vh)
	{
		float bestErrs[NUM_TOP_PART_A];
		for (int i = 0; i < NUM_TOP_PART_A; i++) bestErrs[i] = FLT_MAX;

		for (int partId = 0; partId < 64; partId++)
		{
			Vec4 ep0[2];
			Vec4 ep1[2];

			_PCA_Endpoints2_bc7_mode7(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints2_bc7_mode7(pixels, partId, 1, ep0[1], ep1[1], vw, vh);

			Vec4 axis[2];
			axis[0] = ep1[0] - ep0[0];
			axis[1] = ep1[1] - ep0[1];

			float mul[2];
			mul[0] = axis[0].sqLength();
			mul[1] = axis[1].sqLength();


			if (mul[0] > 0.0f) mul[0] = 1.0f / mul[0];
			if (mul[1] > 0.0f) mul[1] = 1.0f / mul[1];

			float err = 0.0f;

			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t];
					uint8_t p = REGION2(tx, ty, partId);
					float f_ind = dot((pix - ep0[p]), axis[p]) * mul[p];
					f_ind = Clamp01(f_ind);
					Vec4 pix2 = ep0[p] * Vec4(1.0f - f_ind) + ep1[p] * Vec4(f_ind);
					err += (pix - pix2).sqLength();
				}
			}

			for (int i = 0; i < NUM_TOP_PART_A; i++)
			{
				if (err < bestErrs[i])
				{
					for (int j = NUM_TOP_PART_A - 1; j > i; j--)
					{
						bestErrs[j] = bestErrs[j - 1];
						bestPart[j] = bestPart[j - 1];
					}
					bestErrs[i] = err;
					bestPart[i] = partId;

					break;
				}

			}


		}

	}

	static void _Process_Colors2_bc7_mode7(const Vec4* pixels, int partId, unsigned char quantInd[16], Vec4 ep0[2], Vec4 ep1[2], int vw, int vh)
	{
		for (int p = 0; p < 2; p++)
		{
			Vec4 new_ep0, new_ep1;
			float A = 0.0f, B = 0.0f, C = 0.0f;
			Vec4 D(0.0f), E(0.0f);
			float F = 0.0f;

			float c164 = 1.0f / 64.0f;
			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					uint8_t lwrP = REGION2(tx, ty, partId);
					if (p != lwrP) continue;

					int t = tx + ty * 4;
					uint8_t qInd = quantInd[t];
					float k = (float)IndexUnquantize_2bit[qInd] * c164;
					float k2 = 1.0f - k;
					A += k2 * k2;
					B += k * k;
					C += k * k2;

					Vec4 pix = pixels[t];
					D += pix * Vec4(k2);
					E += pix * Vec4(k);
					F += pix.sqLength();
				}
			}
			float d = A * B - C * C;
			if (d == 0.0f)
			{
				new_ep0 = ep0[p];
				new_ep1 = ep1[p];
			}
			else
			{
				d = 1.0f / d;
				Vec4 d1 = D * Vec4(B) - E * Vec4(C);
				Vec4 d2 = E * Vec4(A) - D * Vec4(C);

				new_ep0 = d1 * Vec4(d);
				new_ep1 = d2 * Vec4(d);

				if (ep0[p][3] == 0.0f) new_ep0[3] = 0.0f;
				else if (ep0[p][3] > 0.998f) new_ep0[3] = 1.0f;
				if (ep1[p][3] == 0.0f) new_ep1[3] = 0.0f;
				else if (ep1[p][3] > 0.998f) new_ep1[3] = 1.0f;
			}

			int qep0_x = _quantize_col(new_ep0[0], 6);
			int qep0_y = _quantize_col(new_ep0[1], 6);
			int qep0_z = _quantize_col(new_ep0[2], 6);
			int qep0_w = _quantize_col(new_ep0[3], 6);

			int lsb = new_ep0[3] == 0.0f ? 0 : (new_ep0[3] > 0.998f ? 1 : ((qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1) >= 2 ? 1 : 0));
			qep0_x = (qep0_x & 0x3E) | lsb;
			qep0_y = (qep0_y & 0x3E) | lsb;
			qep0_z = (qep0_z & 0x3E) | lsb;
			qep0_w = (qep0_w & 0x3E) | lsb;

			int qep1_x = _quantize_col(new_ep1[0], 6);
			int qep1_y = _quantize_col(new_ep1[1], 6);
			int qep1_z = _quantize_col(new_ep1[2], 6);
			int qep1_w = _quantize_col(new_ep1[3], 6);

			lsb = new_ep1[3] == 0.0f ? 0 : (new_ep1[3] > 0.998f ? 1 : ((qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 2 ? 1 : 0));
			qep1_x = (qep1_x & 0x3E) | lsb;
			qep1_y = (qep1_y & 0x3E) | lsb;
			qep1_z = (qep1_z & 0x3E) | lsb;
			qep1_w = (qep1_w & 0x3E) | lsb;


			ep0[p][0] = _unquantize_col(qep0_x, 6);
			ep0[p][1] = _unquantize_col(qep0_y, 6);
			ep0[p][2] = _unquantize_col(qep0_z, 6);
			ep0[p][3] = _unquantize_col(qep0_w, 6);

			ep1[p][0] = _unquantize_col(qep1_x, 6);
			ep1[p][1] = _unquantize_col(qep1_y, 6);
			ep1[p][2] = _unquantize_col(qep1_z, 6);
			ep1[p][3] = _unquantize_col(qep1_w, 6);

		}
	}

	static void _Recalc_Colors2_bc7_mode7(const Vec4 ep0[2], const Vec4 ep1[2], uint compr_ep[16])
	{
		for (int p = 0; p < 2; p++)
		{
			int qep0_x = _quantize_col(ep0[p][0], 6);
			int qep0_y = _quantize_col(ep0[p][1], 6);
			int qep0_z = _quantize_col(ep0[p][2], 6);
			int qep0_w = _quantize_col(ep0[p][3], 6);

			int lsb = ep0[p][3] == 0.0f ? 0 : (ep0[p][3] > 0.998f ? 1 : ((qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1) >= 2 ? 1 : 0));

			qep0_x = (qep0_x & 0x3E) | lsb;
			qep0_y = (qep0_y & 0x3E) | lsb;
			qep0_z = (qep0_z & 0x3E) | lsb;
			qep0_w = (qep0_w & 0x3E) | lsb;

			int qep1_x = _quantize_col(ep1[p][0], 6);
			int qep1_y = _quantize_col(ep1[p][1], 6);
			int qep1_z = _quantize_col(ep1[p][2], 6);
			int qep1_w = _quantize_col(ep1[p][3], 6);

			lsb = ep1[p][3] == 0.0f ? 0 : (ep1[p][3] > 0.998f ? 1 : ((qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 2 ? 1 : 0));

			qep1_x = (qep1_x & 0x3E) | lsb;
			qep1_y = (qep1_y & 0x3E) | lsb;
			qep1_z = (qep1_z & 0x3E) | lsb;
			qep1_w = (qep1_w & 0x3E) | lsb;

			compr_ep[0 + p * 2] = (uint)qep0_x;
			compr_ep[1 + p * 2] = (uint)qep1_x;
			compr_ep[4 + p * 2] = (uint)qep0_y;
			compr_ep[5 + p * 2] = (uint)qep1_y;
			compr_ep[8 + p * 2] = (uint)qep0_z;
			compr_ep[9 + p * 2] = (uint)qep1_z;
			compr_ep[12 + p * 2] = (uint)qep0_w;
			compr_ep[13 + p * 2] = (uint)qep1_w;
		}
	}

	static float _BlockErr_mode_7(const Vec4* pixels, const uint compr_ep[16], const uint8_t quant_inds[16], int partId, int vw, int vh)
	{
		int uqcol[16];
		for (int i = 0; i < 16; i++)
		{
			uqcol[i] = _unquantize_col_i((int)compr_ep[i], 6);
		}

		Vec4 palettes[2][4];
		for (int p = 0; p < 2; p++)
			for (int i = 0; i < 4; i++)
			{
				palettes[p][i][0] = (float)_lerp_2bit(uqcol[0 + p * 2], uqcol[1 + p * 2], i) / 255.0f;
				palettes[p][i][1] = (float)_lerp_2bit(uqcol[4 + p * 2], uqcol[5 + p * 2], i) / 255.0f;
				palettes[p][i][2] = (float)_lerp_2bit(uqcol[8 + p * 2], uqcol[9 + p * 2], i) / 255.0f;
				palettes[p][i][3] = (float)_lerp_2bit(uqcol[12 + p * 2], uqcol[13 + p * 2], i) / 255.0f;
			}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				uint8_t p = REGION2(tx, ty, partId);
				int t = tx + ty * 4;
				uint8_t qInd = quant_inds[t];

				Vec4 recon = palettes[p][qInd];
				Vec4 pix = pixels[t];
				Vec4 diff = recon - pix;
				err += diff.sqLength();
			}
		}
		return err;
	}

	static void _CompressBC7_mode7(const Vec4* pixels, BC7Block& block, float& err, int vw, int vh)
	{
		uint8_t bestPart[NUM_TOP_PART_A];
		_SelectBestPartition_bc7_mode7(pixels, bestPart, vw, vh);

		float bestErr = FLT_MAX;
		int bestPartId = 0;
		unsigned char bestQuantInd[16];
		uint best_compr_ep[16];

		for (int topId = 0; topId < NUM_TOP_PART_A; topId++)
		{
			int partId = bestPart[topId];

			Vec4 ep0[2];
			Vec4 ep1[2];
			_PCA_Endpoints2_bc7_mode7(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints2_bc7_mode7(pixels, partId, 1, ep0[1], ep1[1], vw, vh);

			unsigned char quantInd[16];
			_Process_Indices2_bc7_mode_1_3(pixels, partId, ep0, ep1, quantInd, 2, vw, vh);
			_Process_Colors2_bc7_mode7(pixels, partId, quantInd, ep0, ep1, vw, vh);
			_Recalc_Indices2_bc7_mode_1_3(pixels, partId, ep0, ep1, quantInd, 2, vw, vh);

			uint compr_ep[16];
			_Recalc_Colors2_bc7_mode7(ep0, ep1, compr_ep);

			float lwr_err = _BlockErr_mode_7(pixels, compr_ep, quantInd, partId, vw, vh);
			if (lwr_err < bestErr)
			{
				bestErr = lwr_err;
				bestPartId = partId;
				memcpy(bestQuantInd, quantInd, 16);
				memcpy(best_compr_ep, compr_ep, sizeof(uint) * 16);
			}

		}

		if (bestErr < err)
		{
			int base = 0;
			block.setBits(base, 8, 0x80);
			block.setBits(base, 6, bestPartId);

			for (unsigned j = 0; j < 16; j++)
			{
				block.setBits(base, 5, best_compr_ep[j] >> 1);
			}
			block.setBits(base, 1, best_compr_ep[0]);
			block.setBits(base, 1, best_compr_ep[1]);
			block.setBits(base, 1, best_compr_ep[2]);
			block.setBits(base, 1, best_compr_ep[3]);

			for (unsigned j = 0; j < 16; j++)
			{
				if (j == 0 || j == (unsigned)s_shapeindex_to_compressed_indices2[bestPartId])
				{
					block.setBits(base, 1, (uint)bestQuantInd[j]);
				}
				else
				{
					block.setBits(base, 2, (uint)bestQuantInd[j]);
				}
			}
			err = bestErr;
		}

	}

	static void _PCA_Endpoints3_bc7_mode_0_2(const Vec4* pixels, int partId, uint8_t p, Vec4& ep0, Vec4& ep1, int vw, int vh)
	{
		int total = 0;
		Vec4 acc = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
		float acc_xx(0.0f), acc_yy(0.0f), acc_zz(0.0f);
		float acc_xy(0.0f), acc_xz(0.0f);
		float acc_yz(0.0f);

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				if (REGION3(tx, ty, partId) == p)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t];
					acc += pix;

					acc_xx += pix[0] * pix[0]; acc_yy += pix[1] * pix[1]; acc_zz += pix[2] * pix[2];
					acc_xy += pix[0] * pix[1]; acc_xz += pix[0] * pix[2];
					acc_yz += pix[1] * pix[2];
					total++;
				}
			}
		}

		float mult = total > 0 ? 1.0f / (float)(total) : 0.0f;
		acc *= mult;
		acc_xx *= mult; acc_yy *= mult;  acc_zz *= mult;
		acc_xy *= mult; acc_xz *= mult;
		acc_yz *= mult;

		acc_xx = acc_xx - acc[0] * acc[0]; acc_yy = acc_yy - acc[1] * acc[1]; acc_zz = acc_zz - acc[2] * acc[2];
		acc_xy = acc_xy - acc[0] * acc[1]; acc_xz = acc_xz - acc[0] * acc[2];
		acc_yz = acc_yz - acc[1] * acc[2];

		// estimatePrincipleComponent
		Vec4 dir = Vec4(acc_xx, acc_xy, acc_xz, 0.0f);
		float bestLen = dir.sqLength();

		{
			Vec4 dir2 = Vec4(acc_xy, acc_yy, acc_yz, 0.0f);
			float len = dir2.sqLength();
			if (len > bestLen)
			{
				bestLen = len;
				dir = dir2;
			}

			dir2 = Vec4(acc_xz, acc_yz, acc_zz, 0.0f);
			len = dir2.sqLength();
			if (len > bestLen)
			{
				bestLen = len;
				dir = dir2;
			}
		}

		//computePrincipleComponent
		for (int i = 0; i < POWER_ITERATION_COUNT; i++)
		{
			dir = Vec4(
				acc_xx*dir[0] + acc_xy * dir[1] + acc_xz * dir[2],
				acc_xy*dir[0] + acc_yy * dir[1] + acc_yz * dir[2],
				acc_xz*dir[0] + acc_yz * dir[1] + acc_zz * dir[2],
				0.0f);

			float norm = fmaxf(fmaxf(dir[0], dir[1]), dir[2]);
			if (norm == 0.0f) break;
			float ilw = 1.0f / norm;
			dir *= ilw;
		}

		float pc2 = dir.sqLength();
		if (pc2 <= 0.0f)
		{
			dir = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
			pc2 = dir.sqLength();
		}

		pc2 = 1.0f / pc2;

		float minW = FLT_MAX;
		float maxW = -FLT_MAX;

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				if (REGION3(tx, ty, partId) == p)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t] - acc;

					float w = dot(pix, dir)*pc2;

					if (w < minW)
					{
						minW = w;
					}
					if (w > maxW)
					{
						maxW = w;
					}
				}
			}
		}

		if (minW == maxW)
		{
			minW -= 1.0f / 15.0f;
			maxW += 1.0f / 15.0f;
		}

		ep0 = Vec4(dir[0] * minW, dir[1] * minW, dir[2] * minW, 0.0f) + acc;
		ep1 = Vec4(dir[0] * maxW, dir[1] * maxW, dir[2] * maxW, 0.0f) + acc;

		while ((ep0[0] < 0.0f && dir[0]>0.0f) || (ep0[1] < 0.0f && dir[1]>0.0f) || (ep0[2] < 0.0f && dir[2]>0.0f))
		{
			minW += 1.0f / 15.0f;
			ep0 = Vec4(dir[0] * minW, dir[1] * minW, dir[2] * minW, 0.0f) + acc;
		}

		while ((ep1[0] > 1.0f && dir[0] > 0.0f) || (ep1[1] > 1.0f && dir[1] > 0.0f) || (ep1[2] > 1.0f && dir[2] > 0.0f))
		{
			maxW -= 1.0f / 15.0f;
			ep1 = Vec4(dir[0] * maxW, dir[1] * maxW, dir[2] * maxW, 0.0f) + acc;
		}

		ep0[0] = Clamp01(ep0[0]); ep0[1] = Clamp01(ep0[1]); ep0[2] = Clamp01(ep0[2]); ep0[3] = 0.0f;
		ep1[0] = Clamp01(ep1[0]); ep1[1] = Clamp01(ep1[1]); ep1[2] = Clamp01(ep1[2]); ep1[3] = 0.0f;

	}

	static void _SelectBestPartition_bc7_mode0(const Vec4* pixels, uint8_t bestPart[NUM_TOP_PART_B], int vw, int vh)
	{
		float bestErrs[NUM_TOP_PART_B];
		for (int i = 0; i < NUM_TOP_PART_B; i++) bestErrs[i] = FLT_MAX;

		for (int partId = 0; partId < 16; partId++)
		{
			Vec4 ep0[3];
			Vec4 ep1[3];

			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 1, ep0[1], ep1[1], vw, vh);
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 2, ep0[2], ep1[2], vw, vh);

			Vec4 axis[3];
			axis[0] = ep1[0] - ep0[0];
			axis[1] = ep1[1] - ep0[1];
			axis[2] = ep1[2] - ep0[2];

			float mul[3];
			mul[0] = axis[0].sqLength();
			mul[1] = axis[1].sqLength();
			mul[2] = axis[2].sqLength();

			if (mul[0] > 0.0f) mul[0] = 1.0f / mul[0];
			if (mul[1] > 0.0f) mul[1] = 1.0f / mul[1];
			if (mul[2] > 0.0f) mul[2] = 1.0f / mul[2];

			float err = 0.0f;

			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t];
					pix[3] = 0.0f;
					uint8_t p = REGION3(tx, ty, partId);
					float f_ind = dot((pix - ep0[p]), axis[p]) * mul[p];
					f_ind = Clamp01(f_ind);
					Vec4 pix2 = ep0[p] * Vec4(1.0f - f_ind) + ep1[p] * Vec4(f_ind);
					pix2[3] = 0.0f;
					err += (pix - pix2).sqLength();
				}
			}

			for (int i = 0; i < NUM_TOP_PART_B; i++)
			{
				if (err < bestErrs[i])
				{
					for (int j = NUM_TOP_PART_B - 1; j > i; j--)
					{
						bestErrs[j] = bestErrs[j - 1];
						bestPart[j] = bestPart[j - 1];
					}
					bestErrs[i] = err;
					bestPart[i] = partId;

					break;
				}
			}
		}
	}

	static void _Process_Indices3_bc7_mode_0_2(const Vec4* pixels, int partId, Vec4 ep0[3], Vec4 ep1[3], unsigned char quantInd[16], int indexBits, int vw, int vh)
	{
		Vec4 axis[3];
		axis[0] = ep1[0] - ep0[0];
		axis[1] = ep1[1] - ep0[1];
		axis[2] = ep1[2] - ep0[2];

		float mul[3];
		mul[0] = axis[0].sqLength();
		if (mul[0] > 0.0f) mul[0] = 1.0f / mul[0];
		mul[1] = axis[1].sqLength();
		if (mul[1] > 0.0f) mul[1] = 1.0f / mul[1];
		mul[2] = axis[2].sqLength();
		if (mul[2] > 0.0f) mul[2] = 1.0f / mul[2];

		float inds[16];
		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				int t = tx + ty * 4;
				if (tx < vw && ty < vh)
				{
					uint8_t p = REGION3(tx, ty, partId);
					Vec4 pix = pixels[t];
					inds[t] = dot((pix - ep0[p]), axis[p]) * mul[p];
				}
				else
				{
					inds[t] = 0.5f;
				}
			}
		}

		float first[3] = { 0.0f, 0.0f, 0.0f };
		float last[3] = { 1.0f, 1.0f, 1.0f };

		float old_err = FLT_MAX;

		while (true)
		{
			float sum_err = 0.0f;
			for (int ty = 0; ty < 4; ty++)
			{
				for (int tx = 0; tx < 4; tx++)
				{
					uint8_t p = REGION3(tx, ty, partId);
					int t = tx + ty * 4;
					float ind = inds[t];
					int label = _IndexQuantizeF[indexBits]((ind - first[p]) / (last[p] - first[p]));
					float k = (float)IndexUnquantize[indexBits][label] / 64.0f;
					sum_err += fabsf(ind - (first[p] * (1.0f - k) + last[p] * k));
					quantInd[t] = label;
				}
			}
			if (sum_err < old_err)
			{
				old_err = sum_err;
			}
			else break;

			float a1[3] = { 0.0f, 0.0f, 0.0f };
			float b1[3] = { 0.0f, 0.0f, 0.0f };
			float c1[3] = { 0.0f, 0.0f, 0.0f };
			float a2[3] = { 0.0f, 0.0f, 0.0f };
			float b2[3] = { 0.0f, 0.0f, 0.0f };
			float c2[3] = { 0.0f, 0.0f, 0.0f };

			int labels[3];
			for (int ty = 0; ty < 4; ty++)
			{
				for (int tx = 0; tx < 4; tx++)
				{
					uint8_t p = REGION3(tx, ty, partId);
					int t = tx + ty * 4;

					float ind = inds[t];
					int label = labels[p] = quantInd[t];
					float k = (float)IndexUnquantize[indexBits][label] / 64.0f;

					a1[p] += (1.0f - k)*(1.0f - k);
					b1[p] += k * (1.0f - k);
					c1[p] += ind * (1.0f - k);

					//a2[p]+= k*(1.0f-k);
					b2[p] += k * k;
					c2[p] += ind * k;
				}
			}
			a2[0] = b1[0];
			a2[1] = b1[1];
			a2[2] = b1[2];

			for (uint8_t p = 0; p < 3; p++)
			{
				float delta0 = a1[p] * b2[p] - a2[p] * b1[p];
				if (delta0 == 0.0f)
				{
					int label = labels[p];
					float k = (float)IndexUnquantize[indexBits][label] / 64.0f;
					//float k = sqrtf(b2[p] / s_shapes3_pixcounts[partId][p]);
					float center = (c1[p] + c2[p]) / s_shapes3_pixcounts[partId][p];
					float shift = center - (first[p] * (1.0f - k) + last[p] * k);
					first[p] += shift;
					last[p] += shift;
				}
				else
				{
					float delta1 = c1[p] * b2[p] - c2[p] * b1[p];
					float delta2 = c2[p] * a1[p] - c1[p] * a2[p];

					first[p] = delta1 / delta0;
					last[p] = delta2 / delta0;
				}
			}
		}
		for (uint8_t p = 0; p < 3; p++)
		{
			Vec4 old_ep0 = ep0[p];
			Vec4 old_ep1 = ep1[p];
			ep0[p] = old_ep0 * Vec4(1.0f - first[p]) + old_ep1 * Vec4(first[p]);
			ep1[p] = old_ep0 * Vec4(1.0f - last[p]) + old_ep1 * Vec4(last[p]);
		}
	}

	static void _Process_Colors3_bc7_mode0(const Vec4* pixels, int partId, unsigned char quantInd[16], Vec4 ep0[3], Vec4 ep1[3], int vw, int vh)
	{
		for (int p = 0; p < 3; p++)
		{
			Vec4 new_ep0, new_ep1;
			float A = 0.0f, B = 0.0f, C = 0.0f;
			Vec4 D(0.0f), E(0.0f);
			float F = 0.0f;

			float c164 = 1.0f / 64.0f;
			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					uint8_t lwrP = REGION3(tx, ty, partId);
					if (p != lwrP) continue;

					int t = tx + ty * 4;
					uint8_t qInd = quantInd[t];
					float k = (float)IndexUnquantize_3bit[qInd] * c164;
					float k2 = 1.0f - k;
					A += k2 * k2;
					B += k * k;
					C += k * k2;

					Vec4 pix = pixels[t];
					pix[3] = 0.0f;
					D += pix * Vec4(k2);
					E += pix * Vec4(k);
					F += pix.sqLength();
				}
			}
			float d = A * B - C * C;
			if (d == 0.0f)
			{
				new_ep0 = ep0[p];
				new_ep1 = ep1[p];
			}
			else
			{
				d = 1.0f / d;
				Vec4 d1 = D * Vec4(B) - E * Vec4(C);
				Vec4 d2 = E * Vec4(A) - D * Vec4(C);

				new_ep0 = d1 * Vec4(d);
				new_ep1 = d2 * Vec4(d);
			}

			int qep0_x = _quantize_col(new_ep0[0], 5);
			int qep0_y = _quantize_col(new_ep0[1], 5);
			int qep0_z = _quantize_col(new_ep0[2], 5);

			int lsb = (qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1) >= 2 ? 1 : 0;
			qep0_x = (qep0_x & 0x1E) | lsb;
			qep0_y = (qep0_y & 0x1E) | lsb;
			qep0_z = (qep0_z & 0x1E) | lsb;

			int qep1_x = _quantize_col(new_ep1[0], 5);
			int qep1_y = _quantize_col(new_ep1[1], 5);
			int qep1_z = _quantize_col(new_ep1[2], 5);

			lsb = (qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 2 ? 1 : 0;
			qep1_x = (qep1_x & 0x1E) | lsb;
			qep1_y = (qep1_y & 0x1E) | lsb;
			qep1_z = (qep1_z & 0x1E) | lsb;

			ep0[p][0] = _unquantize_col(qep0_x, 5);
			ep0[p][1] = _unquantize_col(qep0_y, 5);
			ep0[p][2] = _unquantize_col(qep0_z, 5);

			ep1[p][0] = _unquantize_col(qep1_x, 5);
			ep1[p][1] = _unquantize_col(qep1_y, 5);
			ep1[p][2] = _unquantize_col(qep1_z, 5);
		}
	}

	static void _Recalc_Indices3_bc7_mode_0_2(const Vec4* pixels, int partId, Vec4 ep0[3], Vec4 ep1[3], unsigned char quantInd[16], int indexBits, int vw, int vh)
	{
		int compr_ind1 = s_shapeindex_to_compressed_indices3_1[partId];
		int compr_ind2 = s_shapeindex_to_compressed_indices3_2[partId];

		Vec4 axis[3];
		axis[0] = ep1[0] - ep0[0];
		axis[1] = ep1[1] - ep0[1];
		axis[2] = ep1[2] - ep0[2];

		float mul[3];
		mul[0] = axis[0].sqLength();
		if (mul[0] > 0.0f) mul[0] = 1.0f / mul[0];
		mul[1] = axis[1].sqLength();
		if (mul[1] > 0.0f) mul[1] = 1.0f / mul[1];
		mul[2] = axis[2].sqLength();
		if (mul[2] > 0.0f) mul[2] = 1.0f / mul[2];

		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				int t = tx + ty * 4;
				uint8_t p = REGION3(tx, ty, partId);
				Vec4 pix = pixels[t];
				float f_ind = 0.5f;
				if (tx < vw && ty < vh)
				{
					f_ind = dot((pix - ep0[p]), axis[p]) * mul[p];
				}
				quantInd[t] = _IndexQuantizeF[indexBits](f_ind);
			}
		}

		bool flip[3] = { false, false, false };
		if (quantInd[0] > ((1 << (indexBits - 1)) - 1))
		{
			flip[0] = true;
			Vec4 t = ep0[0];
			ep0[0] = ep1[0];
			ep1[0] = t;
		}
		if (quantInd[compr_ind1] > ((1 << (indexBits - 1)) - 1))
		{
			flip[1] = true;
			Vec4 t = ep0[1];
			ep0[1] = ep1[1];
			ep1[1] = t;
		}
		if (quantInd[compr_ind2] > ((1 << (indexBits - 1)) - 1))
		{
			flip[2] = true;
			Vec4 t = ep0[2];
			ep0[2] = ep1[2];
			ep1[2] = t;
		}
		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				uint8_t p = REGION3(tx, ty, partId);
				int t = tx + ty * 4;
				if (flip[p])
				{
					quantInd[t] = ((1 << indexBits) - 1) - quantInd[t];
				}
			}
		}
	}

	static void _Recalc_Colors3_bc7_mode0(const Vec4 ep0[3], const Vec4 ep1[3], uint compr_ep[18])
	{
		for (int p = 0; p < 3; p++)
		{
			int qep0_x = _quantize_col(ep0[p][0], 5);
			int qep0_y = _quantize_col(ep0[p][1], 5);
			int qep0_z = _quantize_col(ep0[p][2], 5);

			int lsb = (qep0_x & 1) + (qep0_y & 1) + (qep0_z & 1) >= 2 ? 1 : 0;

			qep0_x = (qep0_x & 0x1E) | lsb;
			qep0_y = (qep0_y & 0x1E) | lsb;
			qep0_z = (qep0_z & 0x1E) | lsb;

			int qep1_x = _quantize_col(ep1[p][0], 5);
			int qep1_y = _quantize_col(ep1[p][1], 5);
			int qep1_z = _quantize_col(ep1[p][2], 5);

			lsb = (qep1_x & 1) + (qep1_y & 1) + (qep1_z & 1) >= 2 ? 1 : 0;

			qep1_x = (qep1_x & 0x1E) | lsb;
			qep1_y = (qep1_y & 0x1E) | lsb;
			qep1_z = (qep1_z & 0x1E) | lsb;

			compr_ep[0 + p * 2] = (uint)qep0_x;
			compr_ep[1 + p * 2] = (uint)qep1_x;
			compr_ep[6 + p * 2] = (uint)qep0_y;
			compr_ep[7 + p * 2] = (uint)qep1_y;
			compr_ep[12 + p * 2] = (uint)qep0_z;
			compr_ep[13 + p * 2] = (uint)qep1_z;
		}
	}

	static float _BlockErr_mode_0(const Vec4* pixels, const uint compr_ep[18], const uint8_t quant_inds[16], int partId, int vw, int vh)
	{
		int uqcol[18];
		for (int i = 0; i < 18; i++)
		{
			uqcol[i] = _unquantize_col_i((int)compr_ep[i], 5);
		}

		Vec4 palettes[3][8];

		for (int p = 0; p < 3; p++)
			for (int i = 0; i < 8; i++)
			{
				palettes[p][i][0] = (float)_lerp_3bit(uqcol[0 + p * 2], uqcol[1 + p * 2], i) / 255.0f;
				palettes[p][i][1] = (float)_lerp_3bit(uqcol[6 + p * 2], uqcol[7 + p * 2], i) / 255.0f;
				palettes[p][i][2] = (float)_lerp_3bit(uqcol[12 + p * 2], uqcol[13 + p * 2], i) / 255.0f;
				palettes[p][i][3] = 1.0f;
			}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				uint8_t p = REGION3(tx, ty, partId);
				int t = tx + ty * 4;
				uint8_t qInd = quant_inds[t];

				Vec4 recon = palettes[p][qInd];
				Vec4 pix = pixels[t];
				Vec4 diff = recon - pix;
				err += diff.sqLength();
			}
		}
		return err;
	}

	static void _CompressBC7_mode0(const Vec4* pixels, BC7Block& block, float& err, int vw, int vh)
	{
		uint8_t bestPart[NUM_TOP_PART_B];

		_SelectBestPartition_bc7_mode0(pixels, bestPart, vw, vh);

		float bestErr = FLT_MAX;
		int bestPartId = 0;
		unsigned char bestQuantInd[16];
		uint best_compr_ep[18];

		for (int topId = 0; topId < NUM_TOP_PART_B; topId++)
		{
			int partId = bestPart[topId];

			Vec4 ep0[3];
			Vec4 ep1[3];
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 1, ep0[1], ep1[1], vw, vh);
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 2, ep0[2], ep1[2], vw, vh);

			unsigned char quantInd[16];
			_Process_Indices3_bc7_mode_0_2(pixels, partId, ep0, ep1, quantInd, 3, vw, vh);
			_Process_Colors3_bc7_mode0(pixels, partId, quantInd, ep0, ep1, vw, vh);
			_Recalc_Indices3_bc7_mode_0_2(pixels, partId, ep0, ep1, quantInd, 3, vw, vh);

			uint compr_ep[18];
			_Recalc_Colors3_bc7_mode0(ep0, ep1, compr_ep);

			float lwr_err = _BlockErr_mode_0(pixels, compr_ep, quantInd, partId, vw, vh);
			if (lwr_err < bestErr)
			{
				bestErr = lwr_err;
				bestPartId = partId;
				memcpy(bestQuantInd, quantInd, 16);
				memcpy(best_compr_ep, compr_ep, sizeof(uint) * 18);
			}
		}

		if (bestErr < err)
		{
			int base = 0;
			block.setBits(base, 1, 1);
			block.setBits(base, 4, bestPartId);

			for (unsigned j = 0; j < 18; j++)
			{
				block.setBits(base, 4, best_compr_ep[j] >> 1);
			}
			block.setBits(base, 1, best_compr_ep[0]);
			block.setBits(base, 1, best_compr_ep[1]);
			block.setBits(base, 1, best_compr_ep[2]);
			block.setBits(base, 1, best_compr_ep[3]);
			block.setBits(base, 1, best_compr_ep[4]);
			block.setBits(base, 1, best_compr_ep[5]);

			for (unsigned j = 0; j < 16; j++)
			{
				if (j == 0 ||
					j == (unsigned)s_shapeindex_to_compressed_indices3_1[bestPartId] ||
					j == (unsigned)s_shapeindex_to_compressed_indices3_2[bestPartId])
				{
					block.setBits(base, 2, (uint)bestQuantInd[j]);
				}
				else
				{
					block.setBits(base, 3, (uint)bestQuantInd[j]);
				}
			}
			err = bestErr;
		}
	}

	static void _SelectBestPartition_bc7_mode2(const Vec4* pixels, uint8_t bestPart[NUM_TOP_PART_A], int vw, int vh)
	{
		float bestErrs[NUM_TOP_PART_A];
		for (int i = 0; i < NUM_TOP_PART_A; i++) bestErrs[i] = FLT_MAX;

		for (int partId = 0; partId < 64; partId++)
		{
			Vec4 ep0[3];
			Vec4 ep1[3];

			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 1, ep0[1], ep1[1], vw, vh);
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 2, ep0[2], ep1[2], vw, vh);

			Vec4 axis[3];
			axis[0] = ep1[0] - ep0[0];
			axis[1] = ep1[1] - ep0[1];
			axis[2] = ep1[2] - ep0[2];

			float mul[3];
			mul[0] = axis[0].sqLength();
			mul[1] = axis[1].sqLength();
			mul[2] = axis[2].sqLength();

			if (mul[0] > 0.0f) mul[0] = 1.0f / mul[0];
			if (mul[1] > 0.0f) mul[1] = 1.0f / mul[1];
			if (mul[2] > 0.0f) mul[2] = 1.0f / mul[2];

			float err = 0.0f;

			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					int t = tx + ty * 4;
					Vec4 pix = pixels[t];
					pix[3] = 0.0f;
					uint8_t p = REGION3(tx, ty, partId);
					float f_ind = dot((pix - ep0[p]), axis[p]) * mul[p];
					f_ind = Clamp01(f_ind);
					Vec4 pix2 = ep0[p] * Vec4(1.0f - f_ind) + ep1[p] * Vec4(f_ind);
					pix2[3] = 0.0f;
					err += (pix - pix2).sqLength();
				}
			}


			for (int i = 0; i < NUM_TOP_PART_A; i++)
			{
				if (err < bestErrs[i])
				{
					for (int j = NUM_TOP_PART_A - 1; j > i; j--)
					{
						bestErrs[j] = bestErrs[j - 1];
						bestPart[j] = bestPart[j - 1];
					}
					bestErrs[i] = err;
					bestPart[i] = partId;

					break;
				}
			}
		}
	}

	static void _Process_Colors3_bc7_mode2(const Vec4* pixels, int partId, unsigned char quantInd[16], Vec4 ep0[3], Vec4 ep1[3], int vw, int vh)
	{
		for (int p = 0; p < 3; p++)
		{
			Vec4 new_ep0, new_ep1;
			float A = 0.0f, B = 0.0f, C = 0.0f;
			Vec4 D(0.0f), E(0.0f);
			float F = 0.0f;

			float c164 = 1.0f / 64.0f;
			for (int ty = 0; ty < vh; ty++)
			{
				for (int tx = 0; tx < vw; tx++)
				{
					uint8_t lwrP = REGION3(tx, ty, partId);
					if (p != lwrP) continue;

					int t = tx + ty * 4;
					uint8_t qInd = quantInd[t];
					float k = (float)IndexUnquantize_2bit[qInd] * c164;
					float k2 = 1.0f - k;
					A += k2 * k2;
					B += k * k;
					C += k * k2;

					Vec4 pix = pixels[t];
					pix[3] = 0.0f;
					D += pix * Vec4(k2);
					E += pix * Vec4(k);
					F += pix.sqLength();
				}
			}
			float d = A * B - C * C;
			if (d == 0.0f)
			{
				new_ep0 = ep0[p];
				new_ep1 = ep1[p];
			}
			else
			{
				d = 1.0f / d;
				Vec4 d1 = D * Vec4(B) - E * Vec4(C);
				Vec4 d2 = E * Vec4(A) - D * Vec4(C);

				new_ep0 = d1 * Vec4(d);
				new_ep1 = d2 * Vec4(d);
			}

			int qep0_x = _quantize_col(new_ep0[0], 5);
			int qep0_y = _quantize_col(new_ep0[1], 5);
			int qep0_z = _quantize_col(new_ep0[2], 5);
			int qep1_x = _quantize_col(new_ep1[0], 5);
			int qep1_y = _quantize_col(new_ep1[1], 5);
			int qep1_z = _quantize_col(new_ep1[2], 5);

			ep0[p][0] = _unquantize_col(qep0_x, 5);
			ep0[p][1] = _unquantize_col(qep0_y, 5);
			ep0[p][2] = _unquantize_col(qep0_z, 5);
			ep1[p][0] = _unquantize_col(qep1_x, 5);
			ep1[p][1] = _unquantize_col(qep1_y, 5);
			ep1[p][2] = _unquantize_col(qep1_z, 5);
		}
	}

	static void _Recalc_Colors3_bc7_mode2(const Vec4 ep0[3], const Vec4 ep1[3], uint compr_ep[18])
	{
		for (int p = 0; p < 3; p++)
		{
			int qep0_x = _quantize_col(ep0[p][0], 5);
			int qep0_y = _quantize_col(ep0[p][1], 5);
			int qep0_z = _quantize_col(ep0[p][2], 5);
			int qep1_x = _quantize_col(ep1[p][0], 5);
			int qep1_y = _quantize_col(ep1[p][1], 5);
			int qep1_z = _quantize_col(ep1[p][2], 5);

			compr_ep[0 + p * 2] = (uint)qep0_x;
			compr_ep[1 + p * 2] = (uint)qep1_x;
			compr_ep[6 + p * 2] = (uint)qep0_y;
			compr_ep[7 + p * 2] = (uint)qep1_y;
			compr_ep[12 + p * 2] = (uint)qep0_z;
			compr_ep[13 + p * 2] = (uint)qep1_z;
		}
	}

	static float _BlockErr_mode_2(const Vec4* pixels, const uint compr_ep[18], const uint8_t quant_inds[16], int partId, int vw, int vh)
	{
		int uqcol[18];
		for (int i = 0; i < 18; i++)
		{
			uqcol[i] = _unquantize_col_i((int)compr_ep[i], 5);
		}

		Vec4 palettes[3][4];

		for (int p = 0; p < 3; p++)
			for (int i = 0; i < 4; i++)
			{
				palettes[p][i][0] = (float)_lerp_2bit(uqcol[0 + p * 2], uqcol[1 + p * 2], i) / 255.0f;
				palettes[p][i][1] = (float)_lerp_2bit(uqcol[6 + p * 2], uqcol[7 + p * 2], i) / 255.0f;
				palettes[p][i][2] = (float)_lerp_2bit(uqcol[12 + p * 2], uqcol[13 + p * 2], i) / 255.0f;
				palettes[p][i][3] = 1.0f;
			}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				uint8_t p = REGION3(tx, ty, partId);
				int t = tx + ty * 4;
				uint8_t qInd = quant_inds[t];

				Vec4 recon = palettes[p][qInd];
				Vec4 pix = pixels[t];
				Vec4 diff = recon - pix;
				err += diff.sqLength();
			}
		}
		return err;
	}

	static void _CompressBC7_mode2(const Vec4* pixels, BC7Block& block, float& err, int vw, int vh)
	{
		uint8_t bestPart[NUM_TOP_PART_A];
		_SelectBestPartition_bc7_mode2(pixels, bestPart, vw, vh);

		float bestErr = FLT_MAX;
		int bestPartId = 0;
		unsigned char bestQuantInd[16];
		uint best_compr_ep[18];

		for (int topId = 0; topId < NUM_TOP_PART_A; topId++)
		{
			int partId = bestPart[topId];
			//int partId = 18;

			Vec4 ep0[3];
			Vec4 ep1[3];
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 0, ep0[0], ep1[0], vw, vh);
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 1, ep0[1], ep1[1], vw, vh);
			_PCA_Endpoints3_bc7_mode_0_2(pixels, partId, 2, ep0[2], ep1[2], vw, vh);

			unsigned char quantInd[16];
			_Process_Indices3_bc7_mode_0_2(pixels, partId, ep0, ep1, quantInd, 2, vw, vh);
			_Process_Colors3_bc7_mode2(pixels, partId, quantInd, ep0, ep1, vw, vh);
			_Recalc_Indices3_bc7_mode_0_2(pixels, partId, ep0, ep1, quantInd, 2, vw, vh);

			uint compr_ep[18];
			_Recalc_Colors3_bc7_mode2(ep0, ep1, compr_ep);

			float lwr_err = _BlockErr_mode_2(pixels, compr_ep, quantInd, partId, vw, vh);
			if (lwr_err < bestErr)
			{
				bestErr = lwr_err;
				bestPartId = partId;
				memcpy(bestQuantInd, quantInd, 16);
				memcpy(best_compr_ep, compr_ep, sizeof(uint) * 18);
			}
		}

		if (bestErr < err)
		{
			int base = 0;
			block.setBits(base, 3, 4);
			block.setBits(base, 6, bestPartId);

			for (unsigned j = 0; j < 18; j++)
			{
				block.setBits(base, 5, best_compr_ep[j]);
			}

			for (unsigned j = 0; j < 16; j++)
			{
				if (j == 0 ||
					j == (unsigned)s_shapeindex_to_compressed_indices3_1[bestPartId] ||
					j == (unsigned)s_shapeindex_to_compressed_indices3_2[bestPartId])
				{
					block.setBits(base, 1, (uint)bestQuantInd[j]);
				}
				else
				{
					block.setBits(base, 2, (uint)bestQuantInd[j]);
				}
			}
			err = bestErr;
		}

	}

	static const uint8_t s_RoatedRGBAChannels[4][4] = { { 0, 1, 2, 3 }, { 3, 1, 2, 0 }, { 0, 3, 2, 1 }, { 0, 1, 3, 2 } };
	static const uint8_t s_NewAlphaIndex[4] = { 3, 0, 1, 2 };

	static inline Vec4 _RotatePix(const Vec4& inPix, int rotation)
	{
		uint8_t ind_r = s_RoatedRGBAChannels[rotation][0];
		uint8_t ind_g = s_RoatedRGBAChannels[rotation][1];
		uint8_t ind_b = s_RoatedRGBAChannels[rotation][2];
		uint8_t ind_a = s_RoatedRGBAChannels[rotation][3];

		return Vec4(inPix[ind_r], inPix[ind_g], inPix[ind_b], inPix[ind_a]);
	}

	static void _PCA_Endpoints_bc7_mode_4_5(const Vec4* pixels, Vec4 &ep0, Vec4 &ep1, int rotate, int vw, int vh)
	{
		Vec4 acc = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
		float acc_xx(0.0f), acc_yy(0.0f), acc_zz(0.0f);
		float acc_xy(0.0f), acc_xz(0.0f);
		float acc_yz(0.0f);

		bool hasZeroAlpha = false;
		bool hasOneAlpha = false;

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				Vec4 rgbaPix = pixels[t];
				Vec4 pix = _RotatePix(rgbaPix, rotate);

				hasZeroAlpha |= rgbaPix[3] == 0.0f;
				hasOneAlpha |= rgbaPix[3] > 0.998f;

				acc += pix;

				acc_xx += pix[0] * pix[0]; acc_yy += pix[1] * pix[1]; acc_zz += pix[2] * pix[2];
				acc_xy += pix[0] * pix[1]; acc_xz += pix[0] * pix[2];
				acc_yz += pix[1] * pix[2];
			}
		}

		float mult = 1.0f / (float)(vh*vw);
		acc *= mult;
		acc_xx *= mult; acc_yy *= mult;  acc_zz *= mult;
		acc_xy *= mult; acc_xz *= mult;
		acc_yz *= mult;

		acc_xx = acc_xx - acc[0] * acc[0]; acc_yy = acc_yy - acc[1] * acc[1]; acc_zz = acc_zz - acc[2] * acc[2];
		acc_xy = acc_xy - acc[0] * acc[1]; acc_xz = acc_xz - acc[0] * acc[2];
		acc_yz = acc_yz - acc[1] * acc[2];

		// estimatePrincipleComponent
		Vec4 dir = Vec4(acc_xx, acc_xy, acc_xz, 0.0f);
		float bestLen = dir.sqLength();

		{
			Vec4 dir2 = Vec4(acc_xy, acc_yy, acc_yz, 0.0f);
			float len = dir2.sqLength();
			if (len > bestLen)
			{
				bestLen = len;
				dir = dir2;
			}

			dir2 = Vec4(acc_xz, acc_yz, acc_zz, 0.0f);
			len = dir2.sqLength();
			if (len > bestLen)
			{
				bestLen = len;
				dir = dir2;
			}
		}

		//computePrincipleComponent
		for (int i = 0; i < POWER_ITERATION_COUNT; i++)
		{
			dir = Vec4(
				acc_xx*dir[0] + acc_xy * dir[1] + acc_xz * dir[2],
				acc_xy*dir[0] + acc_yy * dir[1] + acc_yz * dir[2],
				acc_xz*dir[0] + acc_yz * dir[1] + acc_zz * dir[2],
				0.0f);

			float norm = fmaxf(fmaxf(dir[0], dir[1]), dir[2]);
			if (norm == 0.0f) break;
			float ilw = 1.0f / norm;
			dir *= ilw;
		}

		float pc2 = dir.sqLength();
		if (pc2 <= 0.0f)
		{
			dir = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
			pc2 = dir.sqLength();
		}
		pc2 = 1.0f / pc2;

		float minW = FLT_MAX;
		float maxW = -FLT_MAX;

		float minA = FLT_MAX;
		float maxA = -FLT_MAX;

		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				Vec4 pix = _RotatePix(pixels[t], rotate) - acc;
				float w = dot(pix, dir)*pc2;

				if (w < minW)
				{
					minW = w;
				}
				if (w > maxW)
				{
					maxW = w;
				}

				if (pix[3] < minA)
				{
					minA = pix[3];
				}
				if (pix[3] > maxA)
				{
					maxA = pix[3];
				}
			}
		}

		if (minW == maxW)
		{
			minW -= 1.0f / 31.0f;
			maxW += 1.0f / 31.0f;
		}

		ep0 = Vec4(dir[0] * minW, dir[1] * minW, dir[2] * minW, minA) + acc;
		ep1 = Vec4(dir[0] * maxW, dir[1] * maxW, dir[2] * maxW, maxA) + acc;

		ep0[0] = Clamp01(ep0[0]); ep0[1] = Clamp01(ep0[1]); ep0[2] = Clamp01(ep0[2]); ep0[3] = Clamp01(ep0[3]);
		ep1[0] = Clamp01(ep1[0]); ep1[1] = Clamp01(ep1[1]); ep1[2] = Clamp01(ep1[2]); ep1[3] = Clamp01(ep1[3]);

		if (hasZeroAlpha)
		{
			uint8_t newAlphaIndex = s_NewAlphaIndex[rotate];
			if (ep0[newAlphaIndex] > ep1[newAlphaIndex]) ep1[newAlphaIndex] = 0.0f;
			else ep0[newAlphaIndex] = 0.0f;
		}

		if (hasOneAlpha)
		{
			uint8_t newAlphaIndex = s_NewAlphaIndex[rotate];
			if (ep0[newAlphaIndex] > ep1[newAlphaIndex]) ep0[newAlphaIndex] = 1.0f;
			else ep1[newAlphaIndex] = 1.0f;
		}

	}

	static void _Process_Indices_bc7_mode_5_4(const Vec4* pixels, Vec4 &ep0, Vec4 &ep1, unsigned char quantInd0[16], unsigned char quantInd1[16], int rotate, int indexBits0, int indexBits1, int vw, int vh)
	{
		Vec4 axis = ep1 - ep0;
		axis[3] = 0.0f;
		float mul = axis.sqLength();
		if (mul > 0.0f) mul = 1.0f / mul;
		float mul2 = ep1[3] - ep0[3];
		if (mul2 != 0.0f) mul2 = 1.0f / mul2;

		float inds[16];
		float inds2[16];
		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				int t = tx + ty * 4;
				if (tx < vw && ty < vh)
				{
					Vec4 pix = _RotatePix(pixels[t], rotate);
					inds[t] = dot((pix - ep0), axis) * mul;
					inds2[t] = (pix[3] - ep0[3])*mul2;
				}
				else
				{
					inds[t] = 0.5f;
					inds2[t] = 0.5f;
				}
			}
		}

		Vec4 old_ep0 = ep0;
		Vec4 old_ep1 = ep1;

		float first;
		float last;

		_OptimizeEnds(inds, first, last, quantInd0, indexBits0);
		ep0 = old_ep0 * Vec4(1.0f - first) + old_ep1 * Vec4(first);
		ep1 = old_ep0 * Vec4(1.0f - last) + old_ep1 * Vec4(last);

		_OptimizeEnds(inds2, first, last, quantInd1, indexBits1);
		ep0[3] = old_ep0[3] * (1.0f - first) + old_ep1[3] * first;
		ep1[3] = old_ep0[3] * (1.0f - last) + old_ep1[3] * last;

		uint8_t newAlphaIndex = s_NewAlphaIndex[rotate];
		if (old_ep0[newAlphaIndex] == 0.0f) ep0[newAlphaIndex] = 0.0f;
		else if (old_ep0[newAlphaIndex] > 0.998f) ep0[newAlphaIndex] = 1.0f;
		if (old_ep1[newAlphaIndex] == 0.0f) ep1[newAlphaIndex] = 0.0f;
		else if (old_ep1[newAlphaIndex] > 0.998f) ep1[newAlphaIndex] = 1.0f;
	}

	static void _Process_Colors_bc7_mode5(const Vec4* pixels, unsigned char quantInd[16], Vec4 &ep0, Vec4 &ep1, int rotate, int vw, int vh)
	{
		Vec4 new_ep0, new_ep1;
		float A = 0.0f, B = 0.0f, C = 0.0f;
		Vec4 D(0.0f), E(0.0f);
		float F = 0.0f;

		float c164 = 1.0f / 64.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				uint8_t qInd = quantInd[t];
				float k = (float)IndexUnquantize_2bit[qInd] * c164;
				float k2 = 1.0f - k;
				A += k2 * k2;
				B += k * k;
				C += k * k2;

				Vec4 pix = _RotatePix(pixels[t], rotate);
				D += pix * Vec4(k2);
				E += pix * Vec4(k);
				F += pix.sqLength();
			}
		}
		float d = A * B - C * C;
		if (d == 0.0f)
		{
			new_ep0 = ep0;
			new_ep1 = ep1;
		}
		else
		{
			d = 1.0f / d;
			Vec4 d1 = D * Vec4(B) - E * Vec4(C);
			Vec4 d2 = E * Vec4(A) - D * Vec4(C);

			new_ep0 = d1 * Vec4(d);
			new_ep1 = d2 * Vec4(d);
		}
		new_ep0[3] = ep0[3];
		new_ep1[3] = ep1[3];

		uint8_t newAlphaIndex = s_NewAlphaIndex[rotate];
		if (ep0[newAlphaIndex] == 0.0f) new_ep0[newAlphaIndex] = 0.0f;
		else if (ep0[newAlphaIndex] > 0.998f) new_ep0[newAlphaIndex] = 1.0f;
		if (ep1[newAlphaIndex] == 0.0f) new_ep1[newAlphaIndex] = 0.0f;
		else if (ep1[newAlphaIndex] > 0.998f) new_ep1[newAlphaIndex] = 1.0f;

		int qep0_x = _quantize_col(new_ep0[0], 7);
		int qep0_y = _quantize_col(new_ep0[1], 7);
		int qep0_z = _quantize_col(new_ep0[2], 7);
		int qep0_w = _quantize_col(new_ep0[3], 8);

		int qep1_x = _quantize_col(new_ep1[0], 7);
		int qep1_y = _quantize_col(new_ep1[1], 7);
		int qep1_z = _quantize_col(new_ep1[2], 7);
		int qep1_w = _quantize_col(new_ep1[3], 8);

		ep0[0] = _unquantize_col(qep0_x, 7);
		ep0[1] = _unquantize_col(qep0_y, 7);
		ep0[2] = _unquantize_col(qep0_z, 7);
		ep0[3] = _unquantize_col(qep0_w, 8);

		ep1[0] = _unquantize_col(qep1_x, 7);
		ep1[1] = _unquantize_col(qep1_y, 7);
		ep1[2] = _unquantize_col(qep1_z, 7);
		ep1[3] = _unquantize_col(qep1_w, 8);
	}

	static void _Recalc_Indices_bc7_mode_5_4(const Vec4* pixels, Vec4 &ep0, Vec4 &ep1, unsigned char quantInd0[16], unsigned char quantInd1[16], int rotate, int indexBits0, int indexBits1, int vw, int vh)
	{
		Vec4 axis = ep1 - ep0;
		axis[3] = 0.0f;
		float mul = axis.sqLength();
		if (mul > 0.0f) mul = 1.0f / mul;
		float mul2 = ep1[3] - ep0[3];
		if (mul2 != 0.0f) mul2 = 1.0f / mul2;

		for (int ty = 0; ty < 4; ty++)
		{
			for (int tx = 0; tx < 4; tx++)
			{
				int t = tx + ty * 4;
				float f_ind0, f_ind1;

				if (tx < vw && ty < vh)
				{
					Vec4 rgbaPix = pixels[t];
					Vec4 pix = _RotatePix(rgbaPix, rotate);

					uint8_t newAlphaIndex = s_NewAlphaIndex[rotate];

					if (rgbaPix[3] == 0.0f && rotate > 0 && ((ep0[newAlphaIndex] == 0.0f) ^ (ep1[newAlphaIndex] == 0.0f)))
					{
						f_ind0 = ep0[newAlphaIndex] == 0.0f ? 0.0f : 1.0f;
					}
					else if (rgbaPix[3] > 0.998f && rotate > 0 && ((ep0[newAlphaIndex] > 0.998f) ^ (ep1[newAlphaIndex] > 0.998f)))
					{
						f_ind0 = ep0[newAlphaIndex] > 0.998f ? 0.0f : 1.0f;
					}
					else
					{
						f_ind0 = dot((pix - ep0), axis) * mul;
					}

					f_ind1 = (pix[3] - ep0[3])*mul2;
				}
				else
				{
					f_ind0 = 0.5f;
					f_ind1 = 0.5f;
				}
				quantInd0[t] = _IndexQuantizeF[indexBits0](f_ind0);
				quantInd1[t] = _IndexQuantizeF[indexBits1](f_ind1);
			}
		}

		if (quantInd0[0] > ((1 << (indexBits0 - 1)) - 1)) // flip
		{
			Vec4 ep_t = ep1;
			ep1 = Vec4(ep0[0], ep0[1], ep0[2], ep1[3]);
			ep0 = Vec4(ep_t[0], ep_t[1], ep_t[2], ep0[3]);

			for (int ty = 0; ty < 4; ty++)
			{
				for (int tx = 0; tx < 4; tx++)
				{
					int t = tx + ty * 4;
					quantInd0[t] = ((1 << indexBits0) - 1) - quantInd0[t];
				}
			}
		}

		if (quantInd1[0] > ((1 << (indexBits1 - 1)) - 1)) // flip
		{
			float t = ep1[3];
			ep1[3] = ep0[3];
			ep0[3] = t;

			for (int ty = 0; ty < 4; ty++)
			{
				for (int tx = 0; tx < 4; tx++)
				{
					int t = tx + ty * 4;
					quantInd1[t] = ((1 << indexBits1) - 1) - quantInd1[t];
				}
			}
		}
	}

	static void _Recalc_Colors_bc7_mode5(const Vec4 &ep0, const Vec4 &ep1, uint compr_ep[8])
	{
		int qep0_x = _quantize_col(ep0[0], 7);
		int qep0_y = _quantize_col(ep0[1], 7);
		int qep0_z = _quantize_col(ep0[2], 7);
		int qep0_w = _quantize_col(ep0[3], 8);

		int qep1_x = _quantize_col(ep1[0], 7);
		int qep1_y = _quantize_col(ep1[1], 7);
		int qep1_z = _quantize_col(ep1[2], 7);
		int qep1_w = _quantize_col(ep1[3], 8);

		compr_ep[0] = (uint)qep0_x;
		compr_ep[1] = (uint)qep1_x;
		compr_ep[2] = (uint)qep0_y;
		compr_ep[3] = (uint)qep1_y;
		compr_ep[4] = (uint)qep0_z;
		compr_ep[5] = (uint)qep1_z;
		compr_ep[6] = (uint)qep0_w;
		compr_ep[7] = (uint)qep1_w;
	}

	static float _BlockErr_mode5(const Vec4* pixels, const uint compr_ep[8], const uint8_t quant_inds0[16], const uint8_t quant_inds1[16], int rotate, int vw, int vh)
	{
		int intEp0_x = _unquantize_col_i((int)compr_ep[0], 7);
		int intEp0_y = _unquantize_col_i((int)compr_ep[2], 7);
		int intEp0_z = _unquantize_col_i((int)compr_ep[4], 7);
		int intEp0_w = _unquantize_col_i((int)compr_ep[6], 8);

		int intEp1_x = _unquantize_col_i((int)compr_ep[1], 7);
		int intEp1_y = _unquantize_col_i((int)compr_ep[3], 7);
		int intEp1_z = _unquantize_col_i((int)compr_ep[5], 7);
		int intEp1_w = _unquantize_col_i((int)compr_ep[7], 8);

		Vec4 palette[4];
		for (int i = 0; i < 4; i++)
		{
			Vec4& item = palette[i];
			item[0] = (float)_lerp_2bit(intEp0_x, intEp1_x, i) / 255.0f;
			item[1] = (float)_lerp_2bit(intEp0_y, intEp1_y, i) / 255.0f;
			item[2] = (float)_lerp_2bit(intEp0_z, intEp1_z, i) / 255.0f;
			item[3] = (float)_lerp_2bit(intEp0_w, intEp1_w, i) / 255.0f;

		}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				Vec4 recon0 = palette[quant_inds0[t]];
				Vec4 recon1 = palette[quant_inds1[t]];
				Vec4 recon = Vec4(recon0[0], recon0[1], recon0[2], recon1[3]);
				Vec4 diff = recon - _RotatePix(pixels[t], rotate);
				err += diff.sqLength();
			}
		}

		return err;
	}

	static void _CompressBC7_mode5(const Vec4* pixels, BC7Block& block, float& err, int vw, int vh)
	{
		for (int rotate = 0; rotate < 4; rotate++)
		{
			Vec4 ep0, ep1;
			_PCA_Endpoints_bc7_mode_4_5(pixels, ep0, ep1, rotate, vw, vh);

			unsigned char quantInd0[16];
			unsigned char quantInd1[16];
			_Process_Indices_bc7_mode_5_4(pixels, ep0, ep1, quantInd0, quantInd1, rotate, 2, 2, vw, vh);
			_Process_Colors_bc7_mode5(pixels, quantInd0, ep0, ep1, rotate, vw, vh);
			_Recalc_Indices_bc7_mode_5_4(pixels, ep0, ep1, quantInd0, quantInd1, rotate, 2, 2, vw, vh);

			uint compr_ep[8];
			_Recalc_Colors_bc7_mode5(ep0, ep1, compr_ep);

			float lwr_err = _BlockErr_mode5(pixels, compr_ep, quantInd0, quantInd1, rotate, vw, vh);
			if (lwr_err < err)
			{
				int base = 0;
				block.setBits(base, 6, 0x20);
				block.setBits(base, 2, (unsigned)rotate);

				for (unsigned j = 0; j < 6; j++)
				{
					block.setBits(base, 7, compr_ep[j]);
				}
				for (unsigned j = 6; j < 8; j++)
				{
					block.setBits(base, 8, compr_ep[j]);
				}

				for (unsigned j = 0; j < 16; j++)
				{
					if (j == 0)
					{
						block.setBits(base, 1, (uint)quantInd0[j]);
					}
					else
					{
						block.setBits(base, 2, (uint)quantInd0[j]);
					}
				}

				for (unsigned j = 0; j < 16; j++)
				{
					if (j == 0)
					{
						block.setBits(base, 1, (uint)quantInd1[j]);
					}
					else
					{
						block.setBits(base, 2, (uint)quantInd1[j]);
					}
				}
				err = lwr_err;
			}
		}
	}

	static void _Process_Colors_bc7_mode4(const Vec4* pixels, unsigned char quantInd[16], Vec4 &ep0, Vec4 &ep1, int rotate, int indexBits0, int vw, int vh)
	{
		Vec4 new_ep0, new_ep1;
		float A = 0.0f, B = 0.0f, C = 0.0f;
		Vec4 D(0.0f), E(0.0f);
		float F = 0.0f;

		float c164 = 1.0f / 64.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				uint8_t qInd = quantInd[t];
				float k = (float)IndexUnquantize[indexBits0][qInd] * c164;
				float k2 = 1.0f - k;
				A += k2 * k2;
				B += k * k;
				C += k * k2;

				Vec4 pix = _RotatePix(pixels[t], rotate);
				D += pix * Vec4(k2);
				E += pix * Vec4(k);
				F += pix.sqLength();

			}
		}
		float d = A * B - C * C;
		if (d == 0.0f)
		{
			new_ep0 = ep0;
			new_ep1 = ep1;
		}
		else
		{
			d = 1.0f / d;
			Vec4 d1 = D * Vec4(B) - E * Vec4(C);
			Vec4 d2 = E * Vec4(A) - D * Vec4(C);

			new_ep0 = d1 * Vec4(d);
			new_ep1 = d2 * Vec4(d);
		}
		new_ep0[3] = ep0[3];
		new_ep1[3] = ep1[3];

		uint8_t newAlphaIndex = s_NewAlphaIndex[rotate];
		if (ep0[newAlphaIndex] == 0.0f) new_ep0[newAlphaIndex] = 0.0f;
		else if (ep0[newAlphaIndex] > 0.998f) new_ep0[newAlphaIndex] = 1.0f;
		if (ep1[newAlphaIndex] == 0.0f) new_ep1[newAlphaIndex] = 0.0f;
		else if (ep1[newAlphaIndex] > 0.998f) new_ep1[newAlphaIndex] = 1.0f;

		int qep0_x = _quantize_col(new_ep0[0], 5);
		int qep0_y = _quantize_col(new_ep0[1], 5);
		int qep0_z = _quantize_col(new_ep0[2], 5);
		int qep0_w = _quantize_col(new_ep0[3], 6);

		int qep1_x = _quantize_col(new_ep1[0], 5);
		int qep1_y = _quantize_col(new_ep1[1], 5);
		int qep1_z = _quantize_col(new_ep1[2], 5);
		int qep1_w = _quantize_col(new_ep1[3], 6);

		ep0[0] = _unquantize_col(qep0_x, 5);
		ep0[1] = _unquantize_col(qep0_y, 5);
		ep0[2] = _unquantize_col(qep0_z, 5);
		ep0[3] = _unquantize_col(qep0_w, 6);

		ep1[0] = _unquantize_col(qep1_x, 5);
		ep1[1] = _unquantize_col(qep1_y, 5);
		ep1[2] = _unquantize_col(qep1_z, 5);
		ep1[3] = _unquantize_col(qep1_w, 6);
	}

	static void _Recalc_Colors_bc7_mode4(const Vec4 &ep0, const Vec4 &ep1, uint compr_ep[8])
	{
		int qep0_x = _quantize_col(ep0[0], 5);
		int qep0_y = _quantize_col(ep0[1], 5);
		int qep0_z = _quantize_col(ep0[2], 5);
		int qep0_w = _quantize_col(ep0[3], 6);

		int qep1_x = _quantize_col(ep1[0], 5);
		int qep1_y = _quantize_col(ep1[1], 5);
		int qep1_z = _quantize_col(ep1[2], 5);
		int qep1_w = _quantize_col(ep1[3], 6);

		compr_ep[0] = (uint)qep0_x;
		compr_ep[1] = (uint)qep1_x;
		compr_ep[2] = (uint)qep0_y;
		compr_ep[3] = (uint)qep1_y;
		compr_ep[4] = (uint)qep0_z;
		compr_ep[5] = (uint)qep1_z;
		compr_ep[6] = (uint)qep0_w;
		compr_ep[7] = (uint)qep1_w;
	}

	static float _BlockErr_mode4A(const Vec4* pixels, const uint compr_ep[8], const uint8_t quant_inds0[16], const uint8_t quant_inds1[16], int rotate, int vw, int vh)
	{
		int intEp0_x = _unquantize_col_i((int)compr_ep[0], 5);
		int intEp0_y = _unquantize_col_i((int)compr_ep[2], 5);
		int intEp0_z = _unquantize_col_i((int)compr_ep[4], 5);
		int intEp0_w = _unquantize_col_i((int)compr_ep[6], 6);

		int intEp1_x = _unquantize_col_i((int)compr_ep[1], 5);
		int intEp1_y = _unquantize_col_i((int)compr_ep[3], 5);
		int intEp1_z = _unquantize_col_i((int)compr_ep[5], 5);
		int intEp1_w = _unquantize_col_i((int)compr_ep[7], 6);

		Vec4 palette0[4];
		float palette1[8];
		for (int i = 0; i < 4; i++)
		{
			Vec4& item = palette0[i];
			item[0] = (float)_lerp_2bit(intEp0_x, intEp1_x, i) / 255.0f;
			item[1] = (float)_lerp_2bit(intEp0_y, intEp1_y, i) / 255.0f;
			item[2] = (float)_lerp_2bit(intEp0_z, intEp1_z, i) / 255.0f;
			item[3] = 1.0f;
		}
		for (int i = 0; i < 8; i++)
		{
			palette1[i] = (float)_lerp_3bit(intEp0_w, intEp1_w, i) / 255.0f;
		}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				Vec4 recon0 = palette0[quant_inds0[t]];
				float recon1 = palette1[quant_inds1[t]];
				Vec4 recon = Vec4(recon0[0], recon0[1], recon0[2], recon1);
				Vec4 diff = recon - _RotatePix(pixels[t], rotate);
				err += diff.sqLength();
			}
		}

		return err;
	}

	static void _CompressBC7_mode4A(const Vec4* pixels, BC7Block& block, float& err, int vw, int vh)
	{
		for (int rotate = 0; rotate < 4; rotate++)
		{
			Vec4 ep0, ep1;
			_PCA_Endpoints_bc7_mode_4_5(pixels, ep0, ep1, rotate, vw, vh);

			unsigned char quantInd0[16];
			unsigned char quantInd1[16];
			_Process_Indices_bc7_mode_5_4(pixels, ep0, ep1, quantInd0, quantInd1, rotate, 2, 3, vw, vh);
			_Process_Colors_bc7_mode4(pixels, quantInd0, ep0, ep1, rotate, 2, vw, vh);
			_Recalc_Indices_bc7_mode_5_4(pixels, ep0, ep1, quantInd0, quantInd1, rotate, 2, 3, vw, vh);

			uint compr_ep[8];
			_Recalc_Colors_bc7_mode4(ep0, ep1, compr_ep);

			float lwr_err = _BlockErr_mode4A(pixels, compr_ep, quantInd0, quantInd1, rotate, vw, vh);
			if (lwr_err < err)
			{
				int base = 0;
				block.setBits(base, 5, 0x10);
				block.setBits(base, 2, (unsigned)rotate);
				block.setBits(base, 1, 0);

				for (unsigned j = 0; j < 6; j++)
				{
					block.setBits(base, 5, compr_ep[j]);
				}
				for (unsigned j = 6; j < 8; j++)
				{
					block.setBits(base, 6, compr_ep[j]);
				}

				for (unsigned j = 0; j < 16; j++)
				{
					if (j == 0)
					{
						block.setBits(base, 1, (uint)quantInd0[j]);
					}
					else
					{
						block.setBits(base, 2, (uint)quantInd0[j]);
					}
				}

				for (unsigned j = 0; j < 16; j++)
				{
					if (j == 0)
					{
						block.setBits(base, 2, (uint)quantInd1[j]);
					}
					else
					{
						block.setBits(base, 3, (uint)quantInd1[j]);
					}
				}
				err = lwr_err;

			}
		}
	}

	static float _BlockErr_mode4B(const Vec4* pixels, const uint compr_ep[8], const uint8_t quant_inds0[16], const uint8_t quant_inds1[16], int rotate, int vw, int vh)
	{
		int intEp0_x = _unquantize_col_i((int)compr_ep[0], 5);
		int intEp0_y = _unquantize_col_i((int)compr_ep[2], 5);
		int intEp0_z = _unquantize_col_i((int)compr_ep[4], 5);
		int intEp0_w = _unquantize_col_i((int)compr_ep[6], 6);

		int intEp1_x = _unquantize_col_i((int)compr_ep[1], 5);
		int intEp1_y = _unquantize_col_i((int)compr_ep[3], 5);
		int intEp1_z = _unquantize_col_i((int)compr_ep[5], 5);
		int intEp1_w = _unquantize_col_i((int)compr_ep[7], 6);

		Vec4 palette0[8];
		float palette1[4];
		for (int i = 0; i < 8; i++)
		{
			Vec4& item = palette0[i];
			item[0] = (float)_lerp_3bit(intEp0_x, intEp1_x, i) / 255.0f;
			item[1] = (float)_lerp_3bit(intEp0_y, intEp1_y, i) / 255.0f;
			item[2] = (float)_lerp_3bit(intEp0_z, intEp1_z, i) / 255.0f;
			item[3] = 1.0f;
		}
		for (int i = 0; i < 4; i++)
		{
			palette1[i] = (float)_lerp_2bit(intEp0_w, intEp1_w, i) / 255.0f;
		}

		float err = 0.0f;
		for (int ty = 0; ty < vh; ty++)
		{
			for (int tx = 0; tx < vw; tx++)
			{
				int t = tx + ty * 4;
				Vec4 recon0 = palette0[quant_inds0[t]];
				float recon1 = palette1[quant_inds1[t]];
				Vec4 recon = Vec4(recon0[0], recon0[1], recon0[2], recon1);
				Vec4 diff = recon - _RotatePix(pixels[t], rotate);
				err += diff.sqLength();
			}
		}

		return err;
	}

	static void _CompressBC7_mode4B(const Vec4* pixels, BC7Block& block, float& err, int vw, int vh)
	{
		for (int rotate = 0; rotate < 4; rotate++)
		{
			Vec4 ep0, ep1;
			_PCA_Endpoints_bc7_mode_4_5(pixels, ep0, ep1, rotate, vw, vh);

			unsigned char quantInd0[16];
			unsigned char quantInd1[16];
			_Process_Indices_bc7_mode_5_4(pixels, ep0, ep1, quantInd0, quantInd1, rotate, 3, 2, vw, vh);
			_Process_Colors_bc7_mode4(pixels, quantInd0, ep0, ep1, rotate, 3, vw, vh);
			_Recalc_Indices_bc7_mode_5_4(pixels, ep0, ep1, quantInd0, quantInd1, rotate, 3, 2, vw, vh);

			uint compr_ep[8];
			_Recalc_Colors_bc7_mode4(ep0, ep1, compr_ep);

			float lwr_err = _BlockErr_mode4B(pixels, compr_ep, quantInd0, quantInd1, rotate, vw, vh);
			if (lwr_err < err)
			{
				int base = 0;
				block.setBits(base, 5, 0x10);
				block.setBits(base, 2, (unsigned)rotate);
				block.setBits(base, 1, 1);

				for (unsigned j = 0; j < 6; j++)
				{
					block.setBits(base, 5, compr_ep[j]);
				}
				for (unsigned j = 6; j < 8; j++)
				{
					block.setBits(base, 6, compr_ep[j]);
				}

				for (unsigned j = 0; j < 16; j++)
				{
					if (j == 0)
					{
						block.setBits(base, 1, (uint)quantInd1[j]);
					}
					else
					{
						block.setBits(base, 2, (uint)quantInd1[j]);
					}
				}

				for (unsigned j = 0; j < 16; j++)
				{
					if (j == 0)
					{
						block.setBits(base, 2, (uint)quantInd0[j]);
					}
					else
					{
						block.setBits(base, 3, (uint)quantInd0[j]);
					}
				}
				err = lwr_err;

			}

		}
	}

	void CompressBC7Block(bool imageHasAlpha, const float* colors, void * output, int vw, int vh)
	{
		BC7Block &block = *(BC7Block *)output;
		const Vec4* pixels = (const Vec4*)colors;
		float bestErr = FLT_MAX;

		bool isOpaque = true;
		if (imageHasAlpha)
		{
			for (unsigned i = 0; i < 16; i++)
			{
				if (i % 4 >= (unsigned)vw || i / 4 >= (unsigned)vh) continue;
				if (pixels[i][3] < 1.0f)
				{
					isOpaque = false;
					break;
				}
			}
		}

		_CompressBC7_mode6(pixels, block, bestErr, vw, vh);
		if (isOpaque)
		{
			_CompressBC7_mode_1_3(pixels, block, bestErr, vw, vh);
		}
		_CompressBC7_mode7(pixels, block, bestErr, vw, vh);
		if (isOpaque)
		{
			_CompressBC7_mode0(pixels, block, bestErr, vw, vh);
			_CompressBC7_mode2(pixels, block, bestErr, vw, vh);
		}
		_CompressBC7_mode5(pixels, block, bestErr, vw, vh);
		_CompressBC7_mode4A(pixels, block, bestErr, vw, vh);
		_CompressBC7_mode4B(pixels, block, bestErr, vw, vh);

	}

}