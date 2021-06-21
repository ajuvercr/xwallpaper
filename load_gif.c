#include <err.h>
#include <limits.h>
#include <pixman.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#include "functions.h"

#include "gif_load.h"

typedef struct {
  void *data, *pict, *prev;
  int width, height;
  unsigned long size, last;
  pixman_image_it_t *it;
} STAT; /** #pragma avoids -Wpadded on 64-bit machines **/

void Frame(void *, struct GIF_WHDR *); /** keeps -Wmissing-prototypes happy **/
void Frame(void *data, struct GIF_WHDR *whdr) {
  uint32_t *pict, *prev, x, y, yoff, iter, ifin, dsrc, ddst;
  STAT *stat = (STAT *)data;

#define BGRA(i)                                                                \
  ((whdr->bptr[i] == whdr->tran)                                               \
       ? 0                                                                     \
       : ((uint32_t)(whdr->cpal[whdr->bptr[i]].R << ((GIF_BIGE) ? 8 : 16)) |   \
          (uint32_t)(whdr->cpal[whdr->bptr[i]].G << ((GIF_BIGE) ? 16 : 8)) |   \
          (uint32_t)(whdr->cpal[whdr->bptr[i]].B << ((GIF_BIGE) ? 24 : 0)) |   \
          ((GIF_BIGE) ? 0xFF : 0xFF000000)))

  if (!whdr->ifrm) { // first frame
    stat->it->pixman_images = calloc(whdr->nfrm, sizeof(pixman_image_t *));
    stat->it->size = whdr->nfrm - 1;

    ddst = (uint32_t)(whdr->xdim * whdr->ydim);
    stat->pict = calloc(sizeof(uint32_t), ddst);
    stat->prev = calloc(sizeof(uint32_t), ddst);
    stat->width = whdr->xdim;
    stat->height = whdr->ydim;
  }
  uint32_t *pict_buffer =
      calloc(sizeof(uint32_t), (uint32_t)(stat->width * stat->height));

  pict = (uint32_t *)stat->pict;
  ddst = (uint32_t)(whdr->xdim * whdr->fryo + whdr->frxo);
  // ddst = 0;
  int trans_count = 0;
  ifin = (!(iter = (whdr->intr) ? 0 : 4)) ? 4 : 5; /** interlacing support **/
  for (dsrc = (uint32_t)-1; iter < ifin; iter++)
    for (yoff = 16U >> ((iter > 1) ? iter : 1), y = (8 >> iter) & 7;
         y < (uint32_t)whdr->fryd; y += yoff)
      for (x = 0; x < (uint32_t)whdr->frxd; x++)
        if (whdr->tran != (long)whdr->bptr[++dsrc])
          pict[(uint32_t)whdr->xdim * y + x + ddst] = BGRA(dsrc);
        else
          trans_count++;

  memcpy(pict_buffer, pict, stat->width * stat->height * sizeof(uint32_t));

  pixman_image_t *img;
  img = pixman_image_create_bits(PIXMAN_a8r8g8b8, stat->width, stat->height,
                                 pict_buffer, stat->width * sizeof(uint32_t));

  stat->it->pixman_images[whdr->ifrm - 1] = img;

  if ((whdr->mode == GIF_PREV) && !stat->last) {
    whdr->frxd = whdr->xdim;
    whdr->fryd = whdr->ydim;
    whdr->mode = GIF_BKGD;
    ddst = 0;
  } else {
    stat->last =
        (whdr->mode == GIF_PREV) ? stat->last : (unsigned long)(whdr->ifrm + 1);
    pict = (uint32_t *)((whdr->mode == GIF_PREV) ? stat->pict : stat->prev);
    prev = (uint32_t *)((whdr->mode == GIF_PREV) ? stat->prev : stat->pict);
    int changed = 0;
    for (x = (uint32_t)(whdr->xdim * whdr->ydim); --x;) {
      if (pict[x - 1] != prev[x - 1])
        changed++;

      pict[x - 1] = prev[x - 1];
    }
  }

  if (whdr->mode == GIF_BKGD) /** cutting a hole for the next frame **/
  {
    for (whdr->bptr[0] = (uint8_t)((whdr->tran >= 0) ? whdr->tran : whdr->bkgd),
        y = 0, pict = (uint32_t *)stat->pict;
         y < (uint32_t)whdr->fryd; y++)
      for (x = 0; x < (uint32_t)whdr->frxd; x++)
        pict[(uint32_t)whdr->xdim * y + x + ddst] = BGRA(0);
  }

#undef BGRA
}

pixman_image_it_t *load_gif(FILE *fp) {
  pixman_image_it_t *out = malloc(sizeof(pixman_image_it_t));

  STAT stat = {0};
  stat.it = out;

  fseek(fp, 0L, SEEK_END);
  stat.size = (unsigned long)ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  fread(stat.data = realloc(0, stat.size), 1, stat.size, fp);

  GIF_Load(stat.data, (long)stat.size, Frame, 0, (void *)&stat, 0L);

  return out;
}
