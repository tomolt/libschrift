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

#define SFT_DOWNWARD_Y 0x01
#define SFT_CHAR_IMAGE 0x02

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
	int (*transformation_hook)(void *hook_data, double advance, double transform[6]);
	void *hook_data;
};

struct SFT_Char
{
	void *image;
	double advance;
	int x;
	int y;
	int width;
	int height;
};

SFT_Font *sft_loadmem(const void *mem, unsigned long size);
SFT_Font *sft_loadfile(const char *filename);
void sft_freefont(SFT_Font *font);

int sft_linemetrics(const struct SFT *sft, double *ascent, double *descent, double *gap);
int sft_char(const struct SFT *sft, unsigned long charCode, struct SFT_Char *chr);

#endif

