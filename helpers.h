/*
 * Copyright (c) 2017 Texas Instruments Incorporated.
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define unlikely(x) __builtin_expect(!!(x), 0)

#if 0
#define DBG(fmt, ...) \
	do { \
		fprintf(stderr, "%s:%d: %s: " fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ## __VA_ARGS__); \
	} while (0)
#else
#define DBG(fmt, ...)
#endif

#define ERR(fmt, ...) \
	do { \
		fprintf(stderr, "%s:%d: %s: " fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ## __VA_ARGS__); \
	} while (0)

#define FAIL(fmt, ...) \
	do { \
		fprintf(stderr, "%s:%d: %s: " fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ## __VA_ARGS__); \
		abort(); \
	} while (0)

#define FAIL_IF(x, fmt, ...) \
	if (unlikely(x)) { \
		fprintf(stderr, "%s:%d: %s: " fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ## __VA_ARGS__); \
		abort(); \
	}

#define ASSERT(x) \
	if (unlikely(!(x))) { \
		fprintf(stderr, "%s:%d: %s: ASSERT(%s) failed\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, __STRING(x)); \
		abort(); \
	}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
