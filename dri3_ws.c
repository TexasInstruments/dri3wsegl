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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <X11/Xlibint.h>
#include <xcb/dri3.h>
#include <xcb/present.h>

#include <services.h>

#include "xhelpers.h"

typedef Display *NativeDisplayType;
typedef Window NativeWindowType;
typedef Pixmap NativePixmapType;

#include <wsegl.h>
#include "dri3_ws.h"

#ifdef DRI3WS_USE_GBM
#include <gbm.h>
#endif

#ifdef DRI3WS_USE_DUMB
#include <drm/drm_fourcc.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

// Capabilities of the X window system
static const WSEGLCaps s_driws_caps[] =
{
	{ WSEGL_CAP_MIN_SWAP_INTERVAL,	 1 },
	{ WSEGL_CAP_MAX_SWAP_INTERVAL,	 1 },
	{ WSEGL_CAP_WINDOWS_USE_HW_SYNC, 1 },
	{ WSEGL_NO_CAPS,		 0 }
};

static struct driws_display *s_displays;
static struct driws_buffer *s_buffers;

static void add_buffer_to_list(struct driws_buffer *buffer)
{
	buffer->next = s_buffers;
	s_buffers = buffer;
}

static void remove_buffer_from_list(struct driws_buffer *buffer)
{
	for (struct driws_buffer *b = s_buffers, *prev = NULL; b; prev = b, b = b->next)
	{
		if (b != buffer)
			continue;

		if (prev)
			prev->next = buffer->next;
		else
			s_buffers = buffer->next;

		break;
	}
}

static struct driws_buffer *find_buffer_from_list(xcb_pixmap_t pixmap)
{
	for (struct driws_buffer *b = s_buffers; b; b = b->next) {
		if (b->x_pixmap == pixmap)
			return b;
	}

	return NULL;
}

static void format2bytespp(WSEGLPixelFormat format, uint32_t *depth, uint32_t *bpp)
{
	switch (format) {
	case WSEGL_PIXELFORMAT_RGB565:
		*depth = *bpp = 16;
		break;
	case WSEGL_PIXELFORMAT_XRGB8888:
	case WSEGL_PIXELFORMAT_ARGB8888: // XXX BPP HACK
		*depth = 24;
		*bpp = 32;
		break;
// XXX BPP HACK
//	case WSEGL_PIXELFORMAT_ARGB8888:
//		*depth = *bpp = 32;
		break;
	default:
		FAIL("unknown wsegl format");
	}
}

#ifdef DRI3WS_USE_GBM
static uint32_t format2gbmformat(WSEGLPixelFormat format)
{
	switch (format) {
	case WSEGL_PIXELFORMAT_RGB565:
		return GBM_FORMAT_RGB565;
	case WSEGL_PIXELFORMAT_XRGB8888:
		return GBM_FORMAT_XRGB8888;
	case WSEGL_PIXELFORMAT_ARGB8888:
		return GBM_FORMAT_ARGB8888;
	default:
		FAIL("unknown wsegl format");
	}
}
#endif

__attribute__((unused))
static const char *format2str(WSEGLPixelFormat format)
{
	switch (format) {
	case WSEGL_PIXELFORMAT_RGB565:
		return "RGB565";
	case WSEGL_PIXELFORMAT_XRGB8888:
		return "XRGB8888";
	case WSEGL_PIXELFORMAT_ARGB8888:
		return "ARGB8888";
	default:
		FAIL("unknown wsegl format");
	}
}

static struct driws_buffer *create_buffer(struct driws_drawable *drawable)
{
	struct driws_display *display = drawable->display;
	uint32_t depth, bpp;

	format2bytespp(drawable->wsegl_pixel_format, &depth, &bpp);

	struct driws_buffer *buffer = calloc(1, sizeof(*buffer));

	buffer->drawable = drawable;

	uint32_t stride, width, height;

#ifdef DRI3WS_USE_GBM
	uint32_t gbm_format = format2gbmformat(drawable->wsegl_pixel_format);

	DBG("BACKEND %s", gbm_device_get_backend_name(display->gbm));

	struct gbm_bo* bo = gbm_bo_create(display->gbm, drawable->width, drawable->height,
					  gbm_format, GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);
	FAIL_IF(!bo, "no bo");


	buffer->dmabuf_fd = gbm_bo_get_fd(bo);

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	stride = gbm_bo_get_stride(bo);

	buffer->gbm_bo = bo;
#endif

#ifdef DRI3WS_USE_DUMB
	struct drm_mode_create_dumb creq = { };
	creq.width = drawable->width;
	creq.height = drawable->height;
	creq.bpp = bpp * 8;
	int r = ioctl(display->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
	FAIL_IF(r, "create dumb failed");

	struct drm_prime_handle args = { };
	args.fd = -1;
	args.handle = creq.handle;
	args.flags = DRM_CLOEXEC | O_RDWR;
	r = ioctl(display->drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &args);
	FAIL_IF(r, "prime failed");

	buffer->dmabuf_fd = args.fd;

	width = drawable->width;
	height = drawable->height;
	stride = creq.pitch;

	buffer->drm_handle = creq.handle;
	buffer->size = stride * height;
#endif

	FAIL_IF(buffer->dmabuf_fd < 0, "bad bo fd %d: %s\n", buffer->dmabuf_fd, strerror(errno));

	DBG("BO fd %d, %ux%u, stride %u", buffer->dmabuf_fd, drawable->width, drawable->height, stride);

	buffer->stride_bytes = stride;
	buffer->stride_pixels = stride / (bpp / 8);

	// pvr map
	PVRSRV_ERROR err = PVRSRVMapDmaBuf(&display->pvr_data.dev_data,
					   display->pvr_data.h_mapping_heap,
					   buffer->dmabuf_fd,
					   // we need to mmap manually to deal with DRM mmap offset
					   PVRSRV_MAP_NOUSERVIRTUAL,
					   &buffer->pvr_meminfo);

	FAIL_IF(err != PVRSRV_OK, "Couldn't map buffer: %s", PVRSRVGetErrorString(err));


	xcb_pixmap_t pixmap = xcb_generate_id(display->xcb_connection);
	xcb_void_cookie_t pixmap_cookie = xcb_dri3_pixmap_from_buffer_checked(display->xcb_connection, pixmap,
									      drawable->xcb_window,
									      0,
									      width, height,
									      stride, depth, bpp,
									      buffer->dmabuf_fd);
	xcb_generic_error_t *error;
	if ((error = xcb_request_check(display->xcb_connection, pixmap_cookie))) {
		FAIL("create pixmap failed");
	}

	buffer->x_pixmap = pixmap;

#ifdef DRI3WS_USE_GBM
	void *map_data = 0;
	uint32_t map_stride = 0;

	buffer->mmap = gbm_bo_map(bo, 0, 0, drawable->width, drawable->height, GBM_BO_TRANSFER_READ_WRITE,
			       &map_stride, &map_data);
	buffer->gbm_map_data = map_data;
#endif

#ifdef DRI3WS_USE_DUMB
	/* prepare buffer for memory mapping */
	struct drm_mode_map_dumb mreq = { 0 };
	mreq.handle = buffer->drm_handle;
	r = ioctl(display->drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	FAIL_IF(r, "DRM_IOCTL_MODE_MAP_DUMB failed");

	/* perform actual memory mapping */
	buffer->mmap = (uint8_t *)mmap(0, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED,
					  display->drm_fd, mreq.offset);
	FAIL_IF(buffer->mmap == MAP_FAILED, "mmap failed");
#endif

	DBG("bo=%p, pvLinAddr=%p, sDevVAddr=%u, uAllocSize=%u mapped",
	    buffer, buffer->mmap,
	    buffer->pvr_meminfo->sDevVAddr.uiAddr, buffer->pvr_meminfo->uAllocSize);

	add_buffer_to_list(buffer);

	return buffer;
}

static void destroy_buffer(struct driws_buffer *buffer)
{
	struct driws_drawable *drawable = buffer->drawable;
	struct driws_display *display = drawable->display;

	DBG("bo=%p", buffer);

	remove_buffer_from_list(buffer);

#ifdef DRI3WS_USE_GBM
	gbm_bo_unmap(buffer->gbm_bo, buffer->gbm_map_data);
#endif

#ifdef DRI3WS_USE_DUMB
	munmap(buffer->mmap, buffer->size);
#endif
	buffer->mmap = NULL;

	xcb_free_pixmap(display->xcb_connection, buffer->x_pixmap);

	PVRSRVUnmapDmaBuf(&display->pvr_data.dev_data, buffer->pvr_meminfo);
	buffer->pvr_meminfo = NULL;

	close(buffer->dmabuf_fd);
	buffer->dmabuf_fd = -1;

#ifdef DRI3WS_USE_GBM
	gbm_bo_destroy(buffer->gbm_bo);
#endif

#ifdef DRI3WS_USE_DUMB

	struct drm_mode_destroy_dumb dreq = { 0 };
	dreq.handle = buffer->drm_handle;
	ioctl(display->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
#endif

	free(buffer);
}

static bool create_buffers(struct driws_drawable *drawable)
{
	struct driws_display *display = drawable->display;
	xcb_connection_t *c = display->xcb_connection;

	//DBG("drawable %p", drawable);

	uint32_t width, height;

	x_get_drawable_data(c, drawable->xcb_window, &width, &height);

	if (drawable->buffers[0]) {
		if (drawable->width == width && drawable->height == height)
			return true;
	}

	DBG("Create new buffers %ux%u", width, height);

	// XXX don't free the buffer that's currently used? could be on screen?

	for (unsigned i = 0; i < ARRAY_SIZE(drawable->buffers); ++i) {
		struct driws_buffer *buffer = drawable->buffers[i];

		if (!buffer)
			continue;

		if (buffer->busy) {
			ERR("WAAAAARNING: buffer busy when re-creating. Leaving it around\n");
			continue;
		}

		destroy_buffer(buffer);
	}

	drawable->width = width;
	drawable->height = height;

	for (unsigned i = 0; i < ARRAY_SIZE(drawable->buffers); ++i)
		drawable->buffers[i] = create_buffer(drawable);

	DBG("new buffers allocated");

	return true;
}

__attribute__((unused))
static const char *get_complete_mode_str(uint8_t mode)
{
	switch (mode) {
	case XCB_PRESENT_COMPLETE_MODE_FLIP:
		return "FLIP";
	case XCB_PRESENT_COMPLETE_MODE_COPY:
		return "COPY";
	case XCB_PRESENT_COMPLETE_MODE_SKIP:
		return "SKIP";
	default:
		return "UNKNOWN";
	}
}

static void handle_special_event(struct driws_drawable *drawable, xcb_present_generic_event_t *ge)
{
	switch (ge->evtype) {
	case XCB_PRESENT_COMPLETE_NOTIFY: {
		__attribute__((unused))
		xcb_present_complete_notify_event_t *ce = (xcb_present_complete_notify_event_t*) ge;

		//struct driws_buffer *buffer = drawable->buffers[ce->serial];

		DBG("XCB_PRESENT_COMPLETE_NOTIFY %u, %s, msc %llu, ust %llu", ce->serial,
		    get_complete_mode_str(ce->mode),
		    (unsigned long long)ce->msc, (unsigned long long)ce->ust);

		break;
	}

	case XCB_PRESENT_EVENT_IDLE_NOTIFY: {
		xcb_present_idle_notify_event_t *ie = (xcb_present_idle_notify_event_t*) ge;

		DBG("XCB_PRESENT_EVENT_IDLE_NOTIFY %u", ie->serial);

		struct driws_buffer *buffer = find_buffer_from_list(ie->pixmap);

		FAIL_IF(!buffer, "no buffer");

		buffer->busy = false;

		break;
	}

	case XCB_PRESENT_EVENT_CONFIGURE_NOTIFY: {
		xcb_present_configure_notify_event_t *ce = (xcb_present_configure_notify_event_t*) ge;
		DBG("XCB_PRESENT_EVENT_CONFIGURE_NOTIFY %ux%u", ce->width, ce->height);

		drawable->width = ce->width;
		drawable->height = ce->height;

		//create_buffers(drawable);

		break;
	}

	case XCB_PRESENT_EVENT_REDIRECT_NOTIFY: {
		__attribute__((unused))
		xcb_present_redirect_notify_event_t *re = (xcb_present_redirect_notify_event_t*) ge;
		DBG("XCB_PRESENT_EVENT_REDIRECT_NOTIFY %u", re->serial);

		break;
	}

	}
	free(ge);
}

static void poll_special_events(struct driws_drawable *drawable)
{
	struct xcb_connection_t *c = drawable->display->xcb_connection;
	xcb_generic_event_t *ev;

	while ((ev = xcb_poll_for_special_event(c, drawable->special_ev)) != NULL) {
		xcb_present_generic_event_t *ge = (void *)ev;
		handle_special_event(drawable, ge);
	}
}

static void wait_special_event(struct driws_drawable *drawable)
{
	struct xcb_connection_t *c = drawable->display->xcb_connection;
	xcb_generic_event_t *ev;

	ev = xcb_wait_for_special_event(c, drawable->special_ev);
	handle_special_event(drawable, (xcb_present_generic_event_t*)ev);

	while ((ev = xcb_poll_for_special_event(c, drawable->special_ev)))
		handle_special_event(drawable, (xcb_present_generic_event_t*)ev);
}

static WSEGLError WSEGL_IsDisplayValid(NativeDisplayType hNativeDisplay)
{
	Display *dpy = (Display*)hNativeDisplay;

	DBG("native %p", dpy);

	if (dpy == NULL)
		return WSEGL_BAD_NATIVE_DISPLAY;

	xcb_connection_t *c = XGetXCBConnection(dpy);
	if (!c)
		return WSEGL_BAD_NATIVE_DISPLAY;

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_InitialiseDisplay(NativeDisplayType hNativeDisplay,
					  WSEGLDisplayHandle *phDisplay,
					  const WSEGLCaps **psCapabilities,
					  WSEGLConfig **psConfigs)
{
	Display *dpy = hNativeDisplay;

	DBG("native %p", hNativeDisplay);

	for (struct driws_display *d = s_displays; d; d = d->next)
	{
		if (d->xdisplay != dpy)
			continue;

		d->ref_count++;

		*phDisplay = (WSEGLDisplayHandle)d;
		*psCapabilities = s_driws_caps;
		*psConfigs = d->wsegl_configs;

		return WSEGL_SUCCESS;
	}

	xcb_connection_t *c = XGetXCBConnection(dpy);
	FAIL_IF(!c, "XGetXCBConnection failed");

	const xcb_setup_t *setup = xcb_get_setup(c);
	FAIL_IF(!setup, "xcb_get_setup failed");

	/* get the first screen */
	xcb_screen_t *screen = xcb_setup_roots_iterator(setup).data;
	FAIL_IF(!screen, "failed to find screen");

	x_check_dri3_ext(c, screen);
	x_check_present_ext(c, screen);

	int drm_fd = x_dri3_open(c, screen);
	FAIL_IF(drm_fd < 0, "drm fd failed");


	struct driws_display *display = calloc(1, sizeof(*display));


	unsigned num_cfgs = 0;

	for (xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen);
	     depth_iter.rem;
	     xcb_depth_next(&depth_iter)) {

		for (xcb_visualtype_iterator_t visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
		     visual_iter.rem;
		     xcb_visualtype_next(&visual_iter)) {
#if 0
			// XXX BPP HACK

			if (depth_iter.data->depth == 32 &&
			    (visual_iter.data->red_mask == 0x00FF0000) &&
			    (visual_iter.data->green_mask == 0x0000FF00) &&
			    (visual_iter.data->blue_mask == 0x000000FF)) {

				display->wsegl_configs[num_cfgs].ui32DrawableType = WSEGL_DRAWABLE_WINDOW;
				display->wsegl_configs[num_cfgs].ePixelFormat = WSEGL_PIXELFORMAT_ARGB8888;
				display->wsegl_configs[num_cfgs].ulNativeVisualID = visual_iter.data->visual_id;
				display->wsegl_configs[num_cfgs].ulNativeVisualType = visual_iter.data->_class;
				display->wsegl_configs[num_cfgs].eTransparentType = WSEGL_OPAQUE;

				num_cfgs++;

				break;
			}
#endif
			if (depth_iter.data->depth == 24 &&
			    (visual_iter.data->red_mask == 0x00FF0000) &&
			    (visual_iter.data->green_mask == 0x0000FF00) &&
			    (visual_iter.data->blue_mask == 0x000000FF)) {

				display->wsegl_configs[num_cfgs].ui32DrawableType = WSEGL_DRAWABLE_WINDOW;
				display->wsegl_configs[num_cfgs].ePixelFormat = WSEGL_PIXELFORMAT_XRGB8888;
				display->wsegl_configs[num_cfgs].ulNativeVisualID = visual_iter.data->visual_id;
				display->wsegl_configs[num_cfgs].ulNativeVisualType = visual_iter.data->_class;
				display->wsegl_configs[num_cfgs].eTransparentType = WSEGL_OPAQUE;

				num_cfgs++;

				// XXX BPP HACK
				display->wsegl_configs[num_cfgs].ui32DrawableType = WSEGL_DRAWABLE_WINDOW;
				display->wsegl_configs[num_cfgs].ePixelFormat = WSEGL_PIXELFORMAT_ARGB8888;
				display->wsegl_configs[num_cfgs].ulNativeVisualID = visual_iter.data->visual_id;
				display->wsegl_configs[num_cfgs].ulNativeVisualType = visual_iter.data->_class;
				display->wsegl_configs[num_cfgs].eTransparentType = WSEGL_OPAQUE;

				num_cfgs++;
				break;
			}

			if (depth_iter.data->depth == 16 &&
			    (visual_iter.data->red_mask == 0xF800) &&
			    (visual_iter.data->green_mask == 0x07E0) &&
			    (visual_iter.data->blue_mask == 0x001F)) {

				display->wsegl_configs[num_cfgs].ui32DrawableType = WSEGL_DRAWABLE_WINDOW;
				display->wsegl_configs[num_cfgs].ePixelFormat = WSEGL_PIXELFORMAT_RGB565;
				display->wsegl_configs[num_cfgs].ulNativeVisualID = visual_iter.data->visual_id;
				display->wsegl_configs[num_cfgs].ulNativeVisualType = visual_iter.data->_class;
				display->wsegl_configs[num_cfgs].eTransparentType = WSEGL_OPAQUE;

				num_cfgs++;

				break;
			}
		}
	}

	display->wsegl_configs[num_cfgs].ui32DrawableType = WSEGL_NO_DRAWABLE;

	if (!InitialiseServices(&display->pvr_data)) {
		//error = WSEGL_CANNOT_INITIALISE;
		FAIL("InitialiseServices failed");
	}

	// Keep a copy of the native display
	display->xdisplay = dpy;
	display->xcb_connection = c;
	display->xcb_screen = screen;

	// Initialise the ref count
	display->ref_count = 1;

	// Save the DRM file descriptor.
	display->drm_fd = drm_fd;

#ifdef DRI3WS_USE_GBM
	struct gbm_device* gbm = gbm_create_device(drm_fd);
	FAIL_IF(!gbm, "no gbm");

	display->gbm = gbm;
#endif
	if (!s_displays) {
		s_displays = display;
	} else {
		struct driws_display *d = s_displays;
		while (d->next)
			d = d->next;
		d->next = display;
	}

	// Return the address of the caps + configs structures
	*psCapabilities = s_driws_caps;
	*psConfigs = display->wsegl_configs;
	*phDisplay = (WSEGLDisplayHandle)display;

	DBG("Done, phDisplay %p", phDisplay);

	return WSEGL_SUCCESS;

	// XXX cleanup
}

static WSEGLError WSEGL_CloseDisplay(WSEGLDisplayHandle hDisplay)
{
	struct driws_display *display = (struct driws_display*)hDisplay;

	DBG("display=%p", display);

	// Only close displays with a ref count of 0
	display->ref_count--;
	if (display->ref_count)
		return WSEGL_SUCCESS;

#ifdef DRI3WS_USE_GBM
	gbm_device_destroy(display->gbm);
#endif
	close(display->drm_fd);

	DeInitialiseServices(&display->pvr_data);

	for (struct driws_display *d = s_displays, *prev = NULL; d; prev = d, d = d->next)
	{
		if (d != display)
			continue;

		if (prev)
			prev->next = display->next;
		else
			s_displays = display->next;

		break;
	}

	free(display);

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_CreateWindowDrawable(WSEGLDisplayHandle hDisplay,
					     WSEGLConfig *psConfig,
					     WSEGLDrawableHandle *phDrawable,
					     NativeWindowType hNativeWindow,
					     WSEGLRotationAngle *eRotationAngle)
{
	struct driws_display *display = (struct driws_display*)hDisplay;

	DBG("display=%p, native=%lx, format %s", hDisplay, hNativeWindow, format2str(psConfig->ePixelFormat));

	if (!psConfig || !(psConfig->ui32DrawableType & WSEGL_DRAWABLE_WINDOW))
		return WSEGL_BAD_MATCH;

	struct driws_drawable *drawable = calloc(1, sizeof(struct driws_drawable));
	if (!drawable)
		return WSEGL_OUT_OF_MEMORY;

	DBG("  allocated drawable=%p", drawable);

	drawable->drawable_type = DRI3WS_DRAWABLE_WINDOW;
	drawable->display = display;
	drawable->xcb_window = hNativeWindow;
	drawable->wsegl_pixel_format = psConfig->ePixelFormat;

	*phDrawable = (WSEGLDrawableHandle)drawable;

	*eRotationAngle = WSEGL_ROTATE_0;

	drawable->special_ev = x_init_special_event_queue(display->xcb_connection, drawable->xcb_window, NULL);

	DBG("drawable=%p created", drawable);

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_CreatePixmapDrawable(WSEGLDisplayHandle hDisplay,
					     WSEGLConfig *psConfig,
					     WSEGLDrawableHandle *phDrawable,
					     NativePixmapType hNativePixmap,
					     WSEGLRotationAngle *eRotationAngle)
{
	FAIL("Not implemented");
}

static WSEGLError WSEGL_DeleteDrawable(WSEGLDrawableHandle hDrawable)
{
	struct driws_drawable *drawable = (struct driws_drawable*)hDrawable;
	struct driws_display *display = drawable->display;

	DBG("drawable=%p", drawable);

	x_uninit_special_event_queue(display->xcb_connection, drawable->special_ev);

	for (unsigned i = 0; i < ARRAY_SIZE(drawable->buffers); ++i) {
		if (!drawable->buffers[i])
			continue;

		destroy_buffer(drawable->buffers[i]);

		drawable->buffers[i] = NULL;
	}

	free(drawable);

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_SwapDrawable(WSEGLDrawableHandle hDrawable, unsigned long ui32Data)
{
	struct driws_drawable *drawable = (struct driws_drawable*)hDrawable;
	struct driws_buffer *buffer = drawable->buffers[drawable->current_back_idx];
	struct driws_display *display = drawable->display;
	struct xcb_connection_t *c = display->xcb_connection;

	DBG("drawable=%p, current-back=%u", drawable, drawable->current_back_idx);

	// Wait for backbuffer render to finish
	WaitForOpsComplete(&display->pvr_data, buffer->pvr_meminfo->psClientSyncInfo);

	// XXX not needed, I think
	poll_special_events(drawable);

	buffer->busy = true;

	uint32_t options = XCB_PRESENT_OPTION_NONE;
	//if (swap_interval == 0)
	//options |= XCB_PRESENT_OPTION_ASYNC;
	//if (force_copy)
	//options |= XCB_PRESENT_OPTION_COPY;

	uint32_t serial = drawable->current_back_idx; /* scb */
	uint32_t target_msc = 0;
	uint32_t divisor = 3;
	uint32_t remainder = drawable->current_back_idx;

	xcb_void_cookie_t cookie = xcb_present_pixmap_checked(c,
							      drawable->xcb_window,
							      buffer->x_pixmap,
							      serial,
							      0, 0, 0, 0, // valid, update, x_off, y_off
							      None, /* target_crtc */
							      None, /* wait fence */
							      None, /* idle fence */
							      options,
							      target_msc,
							      divisor, /* divisor */
							      remainder, /* remainder */
							      0, /* notifiers len */
							      NULL); /* notifiers */

	xcb_generic_error_t *error;
	if ((error = xcb_request_check(c, cookie)))
		FAIL("present pixmap failed");

	xcb_flush(c);

	drawable->current_back_idx = (drawable->current_back_idx + 1) % ARRAY_SIZE(drawable->buffers);

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_SwapControlInterval(WSEGLDrawableHandle hDrawable, unsigned long ui32Interval)
{
	DBG();

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_WaitNative(WSEGLDrawableHandle hDrawable, unsigned long ui32Engine)
{
	FAIL("unimplemented");
}

static WSEGLError WSEGL_CopyFromDrawable(WSEGLDrawableHandle hDrawable, NativePixmapType hNativePixmap)
{
	FAIL("unimplemented");
}

static WSEGLError WSEGL_CopyFromPBuffer(void *pvAddress,
					unsigned long ui32Width,
					unsigned long ui32Height,
					unsigned long ui32Stride,
					WSEGLPixelFormat ePixelFormat,
					NativePixmapType hNativePixmap)
{
	FAIL("unimplemented");
}

static WSEGLError WSEGL_GetDrawableParameters(WSEGLDrawableHandle hDrawable,
					      WSEGLDrawableParams *psSourceParams,
					      WSEGLDrawableParams *psRenderParams,
					      unsigned long ulPlaneOffset)
{
	struct driws_drawable *drawable = (struct driws_drawable*)hDrawable;

	DBG("drawable=%p, current-back=%u", drawable, drawable->current_back_idx);

	if (!create_buffers(drawable)) {
		if (drawable->drawable_type == DRI3WS_DRAWABLE_WINDOW )
			return WSEGL_BAD_NATIVE_WINDOW;
		else
			return WSEGL_BAD_NATIVE_PIXMAP;
	}

	struct driws_buffer *buffer = drawable->buffers[drawable->current_back_idx];

	while (buffer->busy) {
		DBG("Buffer busy, waiting");
		wait_special_event(drawable);
	}

	if (drawable->drawable_type == DRI3WS_DRAWABLE_UNKNOWN)
		FAIL("bad drawable type");

	memset(psRenderParams, 0, sizeof(*psRenderParams));

	psRenderParams->ui32Width       = drawable->width;
	psRenderParams->ui32Height      = drawable->height;
	psRenderParams->ePixelFormat    = drawable->wsegl_pixel_format;
	psRenderParams->ui32Stride      = buffer->stride_pixels;
	psRenderParams->pvLinearAddress = buffer->mmap;
	psRenderParams->ui32HWAddress   = buffer->pvr_meminfo->sDevVAddr.uiAddr;
	psRenderParams->hMemInfo        = (IMG_HANDLE)buffer->pvr_meminfo;

	*psSourceParams                 = *psRenderParams;

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_ConnectDrawable(WSEGLDrawableHandle hDrawable)
{
	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_DisconnectDrawable(WSEGLDrawableHandle hDrawable)
{
	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_FlagStartFrame(WSEGLDrawableHandle hDrawable)
{
	return WSEGL_SUCCESS;
}

WSEGL_EXPORT const WSEGL_FunctionTable *WSEGL_GetFunctionTablePointer(void)
{
	static const WSEGL_FunctionTable sFunctionTable =
	{
		WSEGL_VERSION,
		WSEGL_IsDisplayValid,
		WSEGL_InitialiseDisplay,
		WSEGL_CloseDisplay,
		WSEGL_CreateWindowDrawable,
		WSEGL_CreatePixmapDrawable,
		WSEGL_DeleteDrawable,
		WSEGL_SwapDrawable,
		WSEGL_SwapControlInterval,
		WSEGL_WaitNative,
		WSEGL_CopyFromDrawable,
		WSEGL_CopyFromPBuffer,
		WSEGL_GetDrawableParameters,
		WSEGL_ConnectDrawable,
		WSEGL_DisconnectDrawable,
		WSEGL_FlagStartFrame,
	};

	return &sFunctionTable;
}
