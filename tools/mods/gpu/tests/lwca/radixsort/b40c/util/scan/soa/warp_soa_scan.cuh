/******************************************************************************
 * 
 * Copyright 2010-2012 Duane Merrill
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 * 
 * For more information, see our Google Code project site: 
 * http://code.google.com/p/back40computing/
 * 
 * Thanks!
 * 
 ******************************************************************************/

/******************************************************************************
 * Cooperative tuple warp-scan
 ******************************************************************************/

#pragma once

#include <b40c/util/soa_tuple.cuh>

namespace b40c {
namespace util {
namespace scan {
namespace soa {

/**
 * Structure-of-arrays tuple warpscan.  Performs STEPS steps of a
 * Kogge-Stone style prefix scan.
 *
 * This procedure assumes that no explicit barrier synchronization is needed
 * between steps (i.e., warp-synchronous programming)
 *
 * The type WarpscanSoa is a 2D structure-of-array of smem storage, each SOA array having
 * dimensions [2][NUM_ELEMENTS].
 *
 * IDENTITY_STRIDES = true means an "identity" region of warpscan storage
 * exists for strides to index into
 *
 */
template <
	int LOG_NUM_ELEMENTS,					// Log of number of elements to warp-reduce
	bool EXCLUSIVE = true,					// Whether or not this is an exclusive scan
	int STEPS = LOG_NUM_ELEMENTS>			// Number of steps to run, i.e., produce scanned segments of (1 << STEPS) elements
struct WarpSoaScan
{
	static const int NUM_ELEMENTS = 1 << LOG_NUM_ELEMENTS;

	//---------------------------------------------------------------------
	// Iteration Structures
	//---------------------------------------------------------------------

	// Iteration
	template <int OFFSET_LEFT, int WIDTH>
	struct Iterate
	{
		// Scan
		template <
			typename Tuple,
			typename WarpscanSoa,
			typename ReductionOp>
		static __device__ __forceinline__ Tuple Scan(
			Tuple partial,
			WarpscanSoa warpscan_partials,
			ReductionOp scan_op,
			int warpscan_tid)
		{
			// Store exclusive partial
			warpscan_partials.Set(partial, 1, warpscan_tid);

			if (!WarpscanSoa::VOLATILE) __threadfence_block();

			if (ReductionOp::IDENTITY_STRIDES || (warpscan_tid >= OFFSET_LEFT)) {

				// Load current partial
				Tuple current_partial;
				warpscan_partials.Get(current_partial, 1, warpscan_tid - OFFSET_LEFT);

				// Compute inclusive partial from exclusive and current partials
				partial = scan_op(current_partial, partial);
			}

			if (!WarpscanSoa::VOLATILE) __threadfence_block();

			// Recurse
			return Iterate<OFFSET_LEFT * 2, WIDTH>::Scan(
				partial, warpscan_partials, scan_op, warpscan_tid);
		}
	};

	// Termination
	template <int WIDTH>
	struct Iterate<WIDTH, WIDTH>
	{
		// Scan
		template <
			typename Tuple,
			typename WarpscanSoa,
			typename ReductionOp>
		static __device__ __forceinline__ Tuple Scan(
			Tuple exclusive_partial,
			WarpscanSoa warpscan_partials,
			ReductionOp scan_op,
			int warpscan_tid)
		{
			return exclusive_partial;
		}
	};


	//---------------------------------------------------------------------
	// Interface
	//---------------------------------------------------------------------

	/**
	 * Cooperative warp-scan.  Returns the calling thread's scanned partial.
	 */
	template <
		typename Tuple,
		typename WarpscanSoa,
		typename ReductionOp>
	static __device__ __forceinline__ Tuple Scan(
		Tuple current_partial,						// Input partial
		WarpscanSoa warpscan_partials,				// Smem for warpscanning containing at least two segments of size NUM_ELEMENTS (the first being initialized to zero's)
		ReductionOp scan_op,						// Scan operator
		int warpscan_tid = threadIdx.x)				// Thread's local index into a segment of NUM_ELEMENTS items
	{
		Tuple inclusive_partial = Iterate<1, (1 << STEPS)>::Scan(
			current_partial,
			warpscan_partials,
			scan_op,
			warpscan_tid);

		if (EXCLUSIVE) {

			// Write our inclusive partial
			warpscan_partials.Set(inclusive_partial, 1, warpscan_tid);

			if (!WarpscanSoa::VOLATILE) __threadfence_block();

			// Return exclusive partial
			Tuple exclusive_partial;
			warpscan_partials.Get(exclusive_partial, 1, warpscan_tid - 1);
			return exclusive_partial;

		} else {
			return inclusive_partial;
		}
	}

	/**
	 * Cooperative warp-scan.  Returns the calling thread's scanned partial.
	 */
	template <
		typename Tuple,
		typename WarpscanSoa,
		typename ReductionOp>
	static __device__ __forceinline__ Tuple Scan(
		Tuple current_partial,						// Input partial
		Tuple &total_reduction,						// Total reduction (out param)
		WarpscanSoa warpscan_partials,				// Smem for warpscanning containing at least two segments of size NUM_ELEMENTS (the first being initialized to zero's)
		ReductionOp scan_op,						// Scan operator
		int warpscan_tid = threadIdx.x)				// Thread's local index into a segment of NUM_ELEMENTS items
	{
		Tuple inclusive_partial = Iterate<1, (1 << STEPS)>::Scan(
			current_partial,
			warpscan_partials,
			scan_op,
			warpscan_tid);

		// Write our inclusive partial
		warpscan_partials.Set(inclusive_partial, 1, warpscan_tid);

		if (!WarpscanSoa::VOLATILE) __threadfence_block();

		// Set total to the last thread's inclusive partial
		warpscan_partials.Get(total_reduction, 1, NUM_ELEMENTS - 1);

		if (EXCLUSIVE) {

			// Return exclusive partial
			Tuple exclusive_partial;
			warpscan_partials.Get(exclusive_partial, 1, warpscan_tid - 1);
			return exclusive_partial;

		} else {
			return inclusive_partial;
		}
	}
};


} // namespace soa
} // namespace scan
} // namespace util
} // namespace b40c

