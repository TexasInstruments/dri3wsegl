/*
 * Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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

#include "helpers.h"
#include "pvrhelpers.h"
#include <wsegl.h>

#if !defined(DRI3WS_USE_DUMB) && !defined(DRI3WS_USE_GBM)
#error No BO type defined
#endif

enum driws_drawable_type {
	DRI3WS_DRAWABLE_UNKNOWN = 0,
	DRI3WS_DRAWABLE_WINDOW = 1,
	DRI3WS_DRAWABLE_PIXMAP = 2,
};

struct driws_display {
	struct driws_display *next;

	struct pvr_data pvr_data;

	uint32_t ref_count;

	Display *xdisplay;
	xcb_connection_t *xcb_connection;
	xcb_screen_t *xcb_screen;

	int drm_fd;

#ifdef DRI3WS_USE_GBM
	struct gbm_device* gbm;
#endif

	WSEGLConfig wsegl_configs[4];
};

struct driws_drawable;

struct driws_buffer {
	struct driws_buffer *next;

	struct driws_drawable *drawable;

	void *mmap;
	PVRSRV_CLIENT_MEM_INFO *pvr_meminfo;
	int dmabuf_fd;

	uint32_t stride_pixels;
	uint32_t stride_bytes;

	xcb_pixmap_t x_pixmap;

#ifdef DRI3WS_USE_GBM
	struct gbm_bo *gbm_bo;
	void **gbm_map_data;
#endif

#ifdef DRI3WS_USE_DUMB
	uint32_t drm_handle;
	uint32_t size;
#endif

	bool busy;
};

struct driws_drawable {
	struct driws_display *display;
	xcb_window_t xcb_window;

	uint32_t width;
	uint32_t height;

	WSEGLPixelFormat wsegl_pixel_format;

	uint32_t current_back_idx;
	struct driws_buffer *buffers[3];

	enum driws_drawable_type drawable_type;

	xcb_special_event_t* special_ev;

	bool size_changed;
};
