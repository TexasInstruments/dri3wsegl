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

#include <X11/Xlib-xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "xhelpers.h"

#define FAIL_IF(x, fmt, ...) \
	if (x) { \
		fprintf(stderr, "%s:%d: %s: " fmt "\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, ## __VA_ARGS__); \
		abort(); \
	}

void x_get_drawable_data(xcb_connection_t *c, xcb_drawable_t x_drawable, uint32_t *width, uint32_t *height)
{
	xcb_get_geometry_cookie_t cookie;
	xcb_get_geometry_reply_t *reply;

	cookie = xcb_get_geometry(c, x_drawable);
	reply = xcb_get_geometry_reply(c, cookie, NULL);
	FAIL_IF(!reply, "failed to get geometry");

	*width = reply->width;
	*height = reply->height;

	uint32_t depth = reply->depth;
	FAIL_IF(depth != 24 && depth != 32, "bad depth %d", depth);

	free(reply);
}

void x_check_dri3_ext(xcb_connection_t *c, xcb_screen_t *screen)
{
	xcb_prefetch_extension_data (c, &xcb_dri3_id);

	const xcb_query_extension_reply_t *extension =
			xcb_get_extension_data(c, &xcb_dri3_id);
	FAIL_IF (!(extension && extension->present), "No DRI3");

	xcb_dri3_query_version_cookie_t cookie =
			xcb_dri3_query_version(c, XCB_DRI3_MAJOR_VERSION, XCB_DRI3_MINOR_VERSION);
	xcb_dri3_query_version_reply_t *reply =
			xcb_dri3_query_version_reply(c, cookie, NULL);
	FAIL_IF(!reply, "xcb_dri3_query_version failed");
	printf("DRI3 %u.%u\n", reply->major_version, reply->minor_version);
	free(reply);
}

void x_check_present_ext(xcb_connection_t *c, xcb_screen_t *screen)
{
	xcb_prefetch_extension_data (c, &xcb_present_id);

	const xcb_query_extension_reply_t *extension =
			xcb_get_extension_data(c, &xcb_present_id);
	FAIL_IF (!(extension && extension->present), "No present");

	xcb_present_query_version_cookie_t cookie =
			xcb_present_query_version(c, XCB_PRESENT_MAJOR_VERSION, XCB_PRESENT_MINOR_VERSION);
	xcb_present_query_version_reply_t *reply =
			xcb_present_query_version_reply(c, cookie, NULL);
	FAIL_IF(!reply, "xcb_present_query_version failed");
	printf("present %u.%u\n", reply->major_version, reply->minor_version);
	free(reply);
}

int x_dri3_open(xcb_connection_t *c, xcb_screen_t *screen)
{
	xcb_dri3_open_cookie_t cookie =
			xcb_dri3_open(c, screen->root, 0);
	xcb_dri3_open_reply_t *reply =
			xcb_dri3_open_reply(c, cookie, NULL);
	FAIL_IF(!reply, "dri3 open failed");

	int nfds = reply->nfd;
	FAIL_IF(nfds != 1, "bad number of fds");

	int *fds = xcb_dri3_open_reply_fds(c, reply);

	int fd = fds[0];

	free(reply);

	fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

	printf("DRM FD %d\n", fd);

	return fd;
}

xcb_special_event_t *x_init_special_event_queue(xcb_connection_t *c, xcb_window_t window, uint32_t *special_ev_stamp)
{
	uint32_t id = xcb_generate_id(c);
	xcb_void_cookie_t cookie;

	cookie = xcb_present_select_input_checked(c, id, window,
						  XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY |
						  XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY |
						  XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY/* |
						  XCB_PRESENT_EVENT_MASK_REDIRECT_NOTIFY*/);
	xcb_generic_error_t *error =
			xcb_request_check(c, cookie);
	FAIL_IF(error, "req xge failed");

	xcb_special_event_t *special_ev = xcb_register_for_special_xge(c, &xcb_present_id, id, special_ev_stamp);
	FAIL_IF(!special_ev, "no special ev");

	return special_ev;
}

void x_uninit_special_event_queue(xcb_connection_t *c, xcb_special_event_t *special_ev)
{
	xcb_unregister_for_special_event(c, special_ev);
}

// For debug
void x_draw_to_pixmap(xcb_connection_t *c, xcb_screen_t *screen, xcb_pixmap_t pixmap, uint32_t i)
{
	uint32_t width, height;

	x_get_drawable_data(c, pixmap, &width, &height);

	static xcb_gcontext_t erase_gc = 0;

	if (!erase_gc) {
		erase_gc = xcb_generate_id (c);
		uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
		uint32_t values[] = {
			screen->black_pixel,
			screen->black_pixel,
			0,
		};
		xcb_create_gc (c, erase_gc, screen->root, mask, values);
	}

	xcb_rectangle_t erase_rectangles[] = {
		{ (i-3) % (width - 20), 0, 20, height },
	};

	xcb_poly_fill_rectangle(c, pixmap, erase_gc, 1, erase_rectangles);


	static xcb_gcontext_t draw_gc = 0;

	if (!draw_gc) {
		draw_gc = xcb_generate_id (c);
		uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
		uint32_t values[] = {
			screen->white_pixel,
			screen->white_pixel,
			0,
		};
		xcb_create_gc (c, draw_gc, screen->root, mask, values);
	}

	xcb_rectangle_t rectangles[] = {
		{ i % (width - 20), 0, 20, height },
	};

	xcb_poly_fill_rectangle(c, pixmap, draw_gc, 1, rectangles);
}
