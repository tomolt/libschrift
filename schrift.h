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

struct SFT_Font;
struct SFT;

typedef struct SFT_Font SFT_Font;
typedef struct SFT SFT;

struct SFT_Style
{
	SFT_Font *font;
	double hdpi, vdpi, size;
	char units;
};

SFT_Font *sft_loadfile(char const *filename);
void sft_freefont(SFT_Font *font);

SFT *sft_create(void);
void sft_destroy(SFT *sft);
int sft_setstyle(SFT *sft, struct SFT_Style style);
int sft_linegap(SFT *sft, double *gap);
void sft_move(SFT *sft, double x, double y);
void *sft_char(SFT *sft, const char *ch, int bounds[4]);

#endif

