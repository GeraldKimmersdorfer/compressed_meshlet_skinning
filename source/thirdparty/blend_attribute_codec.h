// Copyright (C) 2022, Christoph Peters, Karlsruhe Institute of Technology
//
// This source code file is licensed under both the three-clause BSD license
// and the GPLv3. You may select, at your option, one of the two licenses. The
// corresponding license headers follow:
//
//
// Three-clause BSD license:
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//
//
// GPLv3:
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.


#pragma once
#include <stdint.h>

//! These parameters completely characterize an encoding scheme for blend
//! attributes
typedef struct blend_attribute_codec_s {
	//! The maximal number of bones per vertex, minus one (i.e. the number of
	//! entries of the weight vector that are stored explicitly).
	uint32_t entry_count;
	/*! Integers from 0 to weight_value_count - 1 are stored per weight to
		encode them. To invest b bits per weight, set this to 2^b. Must be
		less than 2^16=65536.*/
	uint32_t weight_value_count;
	//! This array defines an increase in precision for individual weights. Set
	//! it to 1 to stick to the precision given by weight_value_count.
	uint32_t extra_value_counts[12];
	/*! The payload value count is the product of all extra_value_counts
		multiplied by the number of representable tuple indices (for the table
		of bone index tuples). This variable stores that count divided by the
		factorial of ENTRY_COUNT (rounded up). The payload value count must be
		at most 2^32.*/
	uint32_t payload_value_count_over_factorial;	// ONLY FOR VALIDATION
} blend_attribute_codec_t;
