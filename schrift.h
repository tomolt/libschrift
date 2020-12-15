/* This file is part of libschrift.
 *
 * © 2019, 2020 Thomas Oltmann
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef SCHRIFT_H
#define SCHRIFT_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define SFT_DOWNWARD_Y    0x01
#define SFT_RENDER_IMAGE  0x02

#define SFT_BBOX_WIDTH(bbox)  ((bbox)[2] - (bbox)[0] + 1)
#define SFT_BBOX_HEIGHT(bbox) ((bbox)[3] - (bbox)[1] + 1)
#define SFT_BBOX_YOFFSET(sft, bbox) ((sft)->flags & SFT_DOWNWARD_Y ? -(bbox)[3] : (bbox)[1])

struct SFT_Font;
typedef struct SFT_Font SFT_Font;

struct SFT
{
	SFT_Font *font;
	double xScale;
	double yScale;
	double x;
	double y;
	unsigned int flags;
};

/* libschrift uses semantic versioning. */
const char *sft_version(void);

SFT_Font *sft_loadmem(const void *mem, unsigned long size);
SFT_Font *sft_loadfile(const char *filename);
void sft_freefont(SFT_Font *font);

int sft_linemetrics(const struct SFT *sft, double *ascent, double *descent, double *gap);
int sft_kerning(const struct SFT *sft, unsigned long leftChar, unsigned long rightChar, double kerning[2]);
int sft_codepoint_to_glyph(const struct SFT *sft, unsigned long codepoint, unsigned long *glyph);
int sft_glyph_hmtx(const struct SFT *sft, unsigned long glyph, int *advanceWidth, int *leftSideBearing);
int sft_glyph_outline(const struct SFT *sft, unsigned long glyph, unsigned long *outline);
int sft_outline_bbox(const struct SFT *sft, unsigned long outline, int bbox[4]);
int sft_render_outline(const struct SFT *sft, unsigned long outline, int bbox[4], unsigned int width, unsigned int height, void **image);

#ifdef __cplusplus
}
#endif

#endif

