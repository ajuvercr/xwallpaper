/*
 * Copyright (c) 2017 Tobias Stoeckmann <tobias@stoeckmann.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <xcb/xcb.h>
#ifdef WITH_RANDR
  #include <xcb/randr.h>
#endif

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "functions.h"

void
free_outputs(wp_output_t *outputs)
{
	wp_output_t *output;

	for (output = outputs; output->name != NULL; output++)
		free(output->name);
	free(outputs);
}

wp_output_t *
get_output(wp_output_t *outputs, char *name)
{
	wp_output_t *output;

	for (output = outputs; output->name != NULL; output++)
		if (name != NULL && strcmp(output->name, name) == 0)
			return output;

	if (name != NULL) {
		warnx("output %s was not found, ignoring", name);
		return NULL;
	}

	return output;
}

#ifdef WITH_RANDR
static int
check_randr(xcb_connection_t *c)
{
	const xcb_query_extension_reply_t *reply;

	reply = xcb_get_extension_data(c, &xcb_randr_id);
	return reply != NULL && reply->present;
}

static wp_output_t *
get_randr_outputs(xcb_connection_t *c, xcb_screen_t *screen)
{
	wp_output_t *outputs;
	int len;
	xcb_randr_get_screen_resources_cookie_t resources_cookie;
	xcb_randr_get_screen_resources_reply_t *resources_reply;
	xcb_randr_output_t *xcb_outputs;
	int i;
	size_t n;

	resources_cookie = xcb_randr_get_screen_resources(c, screen->root);
	resources_reply = xcb_randr_get_screen_resources_reply(c,
	    resources_cookie, NULL);

	xcb_outputs = xcb_randr_get_screen_resources_outputs(resources_reply);
	len = xcb_randr_get_screen_resources_outputs_length(resources_reply);
	if (len < 1)
		errx(1, "failed to retrieve randr outputs");

	SAFE_MUL(n, (size_t)len + 1, sizeof(*outputs));
	outputs = xmalloc(n);

	for (i = 0; i < len; i++) {
		xcb_randr_get_output_info_cookie_t output_cookie;
		xcb_randr_get_output_info_reply_t *output_reply;
		xcb_randr_get_crtc_info_cookie_t crtc_cookie;
		xcb_randr_get_crtc_info_reply_t *crtc_reply;
		xcb_randr_crtc_t *crtc;
		int crtc_len, name_len;
		uint8_t *name;

		output_cookie = xcb_randr_get_output_info(c, xcb_outputs[i],
		    XCB_CURRENT_TIME);
		output_reply = xcb_randr_get_output_info_reply(c, output_cookie,
		    NULL);

		name = xcb_randr_get_output_info_name(output_reply);
		name_len = xcb_randr_get_output_info_name_length(output_reply);

		crtc = xcb_randr_get_output_info_crtcs(output_reply);
		crtc_len = xcb_randr_get_output_info_crtcs_length(output_reply);

		outputs[i].name = xmalloc(name_len + 1);
		memcpy(outputs[i].name, name, name_len);
		outputs[i].name[name_len] = '\0';

		if (crtc_len < 1)
			errx(1, "failed to retrieve CRTCs for output %s",
			    outputs[i].name);

		crtc_cookie = xcb_randr_get_crtc_info(c, crtc[0],
		    XCB_CURRENT_TIME);
		crtc_reply = xcb_randr_get_crtc_info_reply(c, crtc_cookie,
		    NULL);

		outputs[i].x = crtc_reply->x;
		outputs[i].y = crtc_reply->y;
		outputs[i].width = crtc_reply->width;
		outputs[i].height = crtc_reply->height;

#ifdef DEBUG
		printf("output detected: %s, %dx%d+%d+%d\n", outputs[i].name,
		    outputs[i].width, outputs[i].height, outputs[i].x,
		    outputs[i].y);
#endif /* DEBUG */
	}

	outputs[len].name = NULL;
	outputs[len].x = 0;
	outputs[len].y = 0;
	outputs[len].width = screen->width_in_pixels;
	outputs[len].height = screen->height_in_pixels;

#ifdef DEBUG
	printf("(randr) screen dimensions: %dx%d+%d+%d\n", outputs[len].width,
	    outputs[len].height, outputs[len].x, outputs[len].y);
#endif /* DEBUG */

	return outputs;
}
#endif /* WITH_RANDR */

wp_output_t *
get_outputs(xcb_connection_t *c, xcb_screen_t *screen)
{
	wp_output_t *outputs;
#ifdef WITH_RANDR
	static int has_randr = -1;

	if (has_randr == -1)
		has_randr = check_randr(c);
	if (has_randr)
		return get_randr_outputs(c, screen);
#endif /* WITH_RANDR */

	outputs = xmalloc(sizeof(*outputs));

	outputs[0].name = NULL;
	outputs[0].x = 0;
	outputs[0].y = 0;
	outputs[0].width = screen->width_in_pixels;
	outputs[0].height = screen->height_in_pixels;

#ifdef DEBUG
	printf("(no randr) screen dimensions: %dx%d+%d+%d\n",
	    outputs[0].width, outputs[0].height, outputs[0].x, outputs[0].y);
#endif /* DEBUG */

	return outputs;
}
