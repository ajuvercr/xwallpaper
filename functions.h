/*
 * Copyright (c) 2021 Tobias Stoeckmann <tobias@stoeckmann.org>
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

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <pixman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "config.h"

#define MODE_CENTER	1
#define MODE_FOCUS	2
#define MODE_MAXIMIZE	3
#define MODE_STRETCH	4
#define MODE_TILE	5
#define MODE_ZOOM	6

#define SOURCE_ATOMS	1

#define TARGET_ATOMS	1
#define TARGET_ROOT	2

#define SAFE_MUL(res, x, y) do {					 \
	if ((y) != 0 && SIZE_MAX / (y) < (x))				 \
		errx(1, "memory allocation would exceed system limits"); \
	res = (x) * (y);						 \
} while (0)

#define SAFE_MUL3(res, x, y, z) do {	\
	SAFE_MUL(res, (x), (z));	\
	SAFE_MUL(res, res, (y));	\
} while (0)

typedef struct pixman_image_it {
	pixman_image_t	**pixman_images;
	int size, at;
	long *times;
	long time;
} pixman_image_it_t;

static
pixman_image_t	*first_pixman_image(pixman_image_it_t *it)
{
	return it->pixman_images[0];
}
static
pixman_image_it_t *from_one(pixman_image_t *pixman_image)
{
	pixman_image_it_t * out = malloc(sizeof(pixman_image_it_t));
	out->at = 0;
	out->size = 1;
	out->pixman_images = malloc(sizeof(pixman_image_t *));
	out->pixman_images[0] = pixman_image;
	return out;
}

static
pixman_image_t *next_image(pixman_image_it_t *it)
{
	it->at++;
	if(it->at >= it->size)
		it->at = 0;

	it->time = it->times[it->at];
	pixman_image_t *out = it->pixman_images[it->at];
	return out;
}

static
pixman_image_t *current_image(pixman_image_it_t *it)
{
	return it->pixman_images[it->at];
}

typedef struct wp_box {
	uint16_t	width;
	uint16_t	height;
	uint16_t	x_off;
	uint16_t	y_off;
} wp_box_t;

typedef struct wp_buffer {
	FILE		*fp;
	pixman_image_it_t	*pixman_image;  // used
	dev_t		 st_dev;
	ino_t		 st_ino;
} wp_buffer_t;



typedef struct wp_option {
	wp_buffer_t	*buffer; // used
	char		*filename;
	int		 mode;	// used
	char		*output;
	int		 screen;
	wp_box_t	*trim; // used
} wp_option_t;

typedef struct wp_config {
	wp_option_t	*options;
	size_t		 count;
	int		 daemon;
	int		 source;
	int		 target;
	int      time;
} wp_config_t;

typedef struct wp_output {
	char *name;
	int16_t x, y;
	uint16_t width, height;
} wp_output_t;

typedef struct wallpaper_struct {
	uint32_t *pixels;
	wp_output_t *output;
	uint32_t sub_wall_papers, row_len, sub_height;
	xcb_image_t *subs[10];
	long sleep;
} wallpaper_struct_t;

typedef struct screen_conf {
	uint32_t freed;
	xcb_gcontext_t gc;
	xcb_screen_t *screen;
	xcb_pixmap_t pixmap;
} screen_conf_t;


extern int	 has_randr;
extern int	 show_debug;

void		 debug(const char *, ...);
void		 free_outputs(wp_output_t *);
wp_output_t	*get_output(wp_output_t *, char *);
wp_output_t	*get_outputs(xcb_connection_t *, xcb_screen_t *);
pixman_image_t	*load_jpeg(FILE *);
pixman_image_t	*load_png(FILE *);
pixman_image_it_t	*load_gif(FILE *);
pixman_image_t	*load_xpm(xcb_connection_t *, xcb_screen_t *, FILE *);
wp_config_t	*parse_config(char **);
void		 stage1_sandbox(void);
void		 stage2_sandbox(void);
void		*xmalloc(size_t);
