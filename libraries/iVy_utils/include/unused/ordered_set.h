/*
 * Credit for this file goes to https://github.com/Beosar/fast-cpp-containers/tree/main under the MIT License
 *
 * MIT License
 *
 * Copyright (c) 2024 Beosar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "binarytree.h"
#include "chunk_allocator.h"

template <typename T_ID>
struct tree_set_helper {

	template<typename... TArgs>
	tree_set_helper(TArgs&&... Args) :Value(std::forward<TArgs>(Args)...) {}


	using key_type = T_ID;
	using value_type = const T_ID;

	value_type Value;

	inline const auto& GetKey() const {
		return Value;
	}
};

template <typename T_ID, typename Compare = std::less<T_ID>, template<typename Ty> class TAlloc = std::allocator>
using ordered_set = tree<tree_set_helper<T_ID>, Compare, TAlloc>;

template <typename T_ID, size_t ChunkAllocatorNumElements = 1 << 16, typename Compare = std::less<T_ID>, template<typename Ty> class TAlloc = std::allocator>
using fast_ordered_set = tree<tree_set_helper<T_ID>, Compare, typename chunk_allocator_single_threaded_wrapper<ChunkAllocatorNumElements, TAlloc>::template allocator>;