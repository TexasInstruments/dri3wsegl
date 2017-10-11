/*
 * Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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

struct pvr_data
{
	PVRSRV_CONNECTION *services;
	PVRSRV_DEV_DATA dev_data;
	IMG_HANDLE h_dev_mem_context;
	IMG_HANDLE h_mapping_heap;
	PVRSRV_MISC_INFO misc_info;
};

bool InitialiseServices(struct pvr_data *pvr_data);
void DeInitialiseServices(struct pvr_data *pvr_data);
void WaitForOpsComplete(const struct pvr_data *pvr_data, const PVRSRV_CLIENT_SYNC_INFO *sync_info);
