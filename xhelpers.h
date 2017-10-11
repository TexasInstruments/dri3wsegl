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

void x_get_drawable_data(xcb_connection_t *c, xcb_drawable_t x_drawable, uint32_t *width, uint32_t *height);
void x_check_dri3_ext(xcb_connection_t *c, xcb_screen_t *screen);
void x_check_present_ext(xcb_connection_t *c, xcb_screen_t *screen);
int x_dri3_open(xcb_connection_t *c, xcb_screen_t *screen);
xcb_special_event_t *x_init_special_event_queue(xcb_connection_t *c, xcb_window_t window, uint32_t *special_ev_stamp);
void x_uninit_special_event_queue(xcb_connection_t *c, xcb_special_event_t *special_ev);
void x_draw_to_pixmap(xcb_connection_t *c, xcb_screen_t *screen, xcb_pixmap_t pixmap, uint32_t i);
