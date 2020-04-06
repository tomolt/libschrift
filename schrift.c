/* See LICENSE file for copyright and license details. */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>

#include "schrift.h"

/* So as it turns out, these first three naive macros are actually
 * faster than any bit-tricks or specialized functions on amd64. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define AFFINE(affine, value) ((value) * (affine).scale + (affine).move)

struct point  { double x, y; };
struct line   { struct point beg, end; };
struct curve  { struct point beg, end, ctrl; };
struct affine { double scale, move; };
struct contour { int first, last; };

struct SFT_Font
{
	uint8_t *memory;
	unsigned long size;
};

struct SFT
{
	SFT_Font *font;
	double xScale, yScale;
	double x, y;
	long glyph;
	uint32_t flags;
};

/* Like bsearch(), but returns the next highest element if key could not be found. */
static void *
csearch(const void *key, const void *base,
	size_t nmemb, size_t size,
	int (*compar)(const void *, const void *))
{
	const uint8_t *bytes = base, *sample;
	size_t low = 0, high = nmemb - 1, mid;
	if (nmemb == 0)
		return NULL;
	while (low != high) {
		mid = low + (high - low) / 2;
		sample = bytes + mid * size;
		if (compar(key, sample) > 0) {
			low = mid + 1;
		} else {
			high = mid;
		}
	}
	return (uint8_t *) bytes + low * size;
}

static inline uint8_t
getu8(SFT_Font *font, unsigned long offset)
{
	assert(offset + 1 <= font->size);
	return *(font->memory + offset);
}

static inline uint16_t
getu16(SFT_Font *font, unsigned long offset)
{
	assert(offset + 2 <= font->size);
	uint8_t *base = font->memory + offset;
	uint16_t b1 = base[0], b0 = base[1]; 
	return (uint16_t) (b1 << 8 | b0);
}

static inline int16_t
geti16(SFT_Font *font, unsigned long offset)
{
	return (int16_t) getu16(font, offset);
}

static inline uint32_t
getu32(SFT_Font *font, unsigned long offset)
{
	assert(offset + 4 <= font->size);
	uint8_t *base = font->memory + offset;
	uint32_t b3 = base[0], b2 = base[1], b1 = base[2], b0 = base[3]; 
	return (uint32_t) (b3 << 24 | b2 << 16 | b1 << 8 | b0);
}

static int
cmpu16(const void *a, const void *b)
{
	return memcmp(a, b, 2);
}

static int
cmpu32(const void *a, const void *b)
{
	return memcmp(a, b, 4);
}

static long
gettable(SFT_Font *font, char tag[4])
{
	void *match;
	unsigned int numTables;
	if (font->size < 12)
		return -1;
	numTables = getu16(font, 4);
	if (font->size < 12 + (size_t) numTables * 16)
		return -1;
	if ((match = bsearch(tag, font->memory + 12, numTables, 16, cmpu32)) == NULL)
		return -1;
	return getu32(font, (uint8_t *) match - font->memory + 8);
}

static int
readfile(SFT_Font *font, const char *filename)
{
	FILE *file;
	struct stat info;
	if (stat(filename, &info) < 0) {
		return -1;
	}
	font->size = info.st_size;
	if ((font->memory = malloc(font->size)) == NULL) {
		return -1;
	}
	if ((file = fopen(filename, "rb")) == NULL) {
		free(font->memory);
		return -1;
	}
	/* TODO error handling here! */
	fread(font->memory, 1, font->size, file);
	fclose(file);
	return 0;
}

SFT_Font *
sft_loadfile(char const *filename)
{
	SFT_Font *font;
	unsigned long scalerType;
	if ((font = calloc(1, sizeof(SFT_Font))) == NULL) {
		return NULL;
	}
	if (readfile(font, filename) < 0) {
		free(font);
		return NULL;
	}
	/* Check for a compatible scalerType (magic number). */
	scalerType = getu32(font, 0);
	if (scalerType != 0x00010000 && scalerType != 0x74727565) {
		free(font);
		return NULL;
	}
	return font;
}

void
sft_freefont(SFT_Font *font)
{
	if (font == NULL) return;
	free(font->memory);
	free(font);
}

SFT *
sft_create(void)
{
	SFT *sft;
	if ((sft = calloc(1, sizeof(SFT))) == NULL)
		return NULL;
	/* Initially set all flags to true. */
	sft->flags = ~(uint32_t) 0;
	return sft;
}

void
sft_destroy(SFT *sft)
{
	if (sft == NULL) return;
	free(sft);
}

void
sft_setflag(SFT *sft, int flag, int value)
{
	if (value) {
		sft->flags |= (uint32_t) flag;
	} else {
		sft->flags &= ~(uint32_t) flag;
	}
}

void
sft_setfont(SFT *sft, SFT_Font *font)
{
	sft->font = font;
}

void
sft_setscale(SFT *sft, double xScale, double yScale)
{
	sft->xScale = xScale;
	sft->yScale = yScale;
}

static int
units_per_em(SFT_Font *font)
{
	long head;
	if ((head = gettable(font, "head")) < 0)
		return -1;
	if (font->size < (unsigned long) head + 54)
		return -1;
	return getu16(font, head + 18);
}

int
sft_linemetrics(SFT *sft, double *ascent, double *descent, double *gap)
{
	double factor;
	long hhea;
	int unitsPerEm;
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	if ((hhea = gettable(sft->font, "hhea")) < 0)
		return -1;
	if (sft->font->size < (unsigned long) hhea + 36) return -1;
	factor = sft->yScale / unitsPerEm;
	*ascent  = geti16(sft->font, hhea + 4) * factor;
	*descent = geti16(sft->font, hhea + 6) * factor;
	*gap     = geti16(sft->font, hhea + 8) * factor;
	return 0;
}

void
sft_move(SFT *sft, double x, double y)
{
	sft->x = x;
	sft->y = y;
	/* Reset kerning state. */
	sft->glyph = 0;
}

static long
cmap_fmt4(SFT_Font *font, unsigned long table, unsigned int charCode)
{
	unsigned long endCodes, startCodes, idDeltas, idRangeOffsets, idOffset;
	unsigned int segCountX2, segIdxX2, startCode, idRangeOffset, id;
	int idDelta;
	uint8_t key[2] = { charCode >> 8, charCode };
	/* TODO Guard against too big charCode. */

	if (font->size < table + 8)
		return -1;
	segCountX2 = getu16(font, table);
	if ((segCountX2 & 1) || !segCountX2)
		return -1;
	/* Find starting positions of the relevant arrays. */
	endCodes = table + 8;
	startCodes = endCodes + segCountX2 + 2;
	idDeltas = startCodes + segCountX2;
	idRangeOffsets = idDeltas + segCountX2;
	if (font->size < idRangeOffsets + segCountX2)
		return -1;
	/* Find the segment that contains charCode by binary searching over the highest codes in the segments. */
	segIdxX2 = (uintptr_t) csearch(key, font->memory + endCodes,
		segCountX2 / 2, 2, cmpu16) - (uintptr_t) (font->memory + endCodes);
	/* Look up segment info from the arrays & short circuit if the spec requires. */
	if ((startCode = getu16(font, startCodes + segIdxX2)) > charCode)
		return 0;
	idDelta = geti16(font, idDeltas + segIdxX2);
	if (!(idRangeOffset = getu16(font, idRangeOffsets + segIdxX2)))
		return (charCode + idDelta) & 0xFFFF;
	/* Calculate offset into glyph array and determine ultimate value. */
	idOffset = idRangeOffsets + segIdxX2 + idRangeOffset + 2 * (charCode - startCode);
	if (font->size < idOffset + 2)
		return -1;
	id = getu16(font, idOffset);
	return id ? (id + idDelta) & 0xFFFF : 0L;
}

static long
glyph_id(SFT_Font *font, unsigned int charCode)
{
	unsigned long entry, table;
	long cmap;
	unsigned int idx, numEntries;
	int type;

	if ((cmap = gettable(font, "cmap")) < 0)
		return -1;

	if (font->size < (unsigned long) cmap + 4)
		return -1;
	numEntries = getu16(font, cmap + 2);
	
	if (font->size < (unsigned long) cmap + 4 + numEntries * 8)
		return -1;
	/* Search for the first Unicode BMP entry. */
	for (idx = 0; idx < numEntries; ++idx) {
		entry = cmap + 4 + idx * 8;
		type = getu16(font, entry) * 0100 + getu16(font, entry + 2);
		if (type == 0003 || type == 0301) {
			table = cmap + getu32(font, entry + 4);
			if (font->size < table + 6)
				return -1;
			/* Dispatch based on cmap format. */
			switch (getu16(font, table)) {
			case 4:
				return cmap_fmt4(font, table + 6, charCode);
			default:
				return -1;
			}
		}
	}

	return -1;
}

static int
num_long_hmtx(SFT_Font *font)
{
	long hhea;
	if ((hhea = gettable(font, "hhea")) < 0)
		return -1;
	if (font->size < (unsigned long) hhea + 36)
		return -1;
	return getu16(font, hhea + 34);
}

static int
hor_metrics(SFT *sft, long glyph, double *advanceWidth, double *leftSideBearing)
{
	double factor;
	unsigned long offset, shmtx;
	long hmtx;
	int unitsPerEm, numLong;
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	factor = sft->xScale / unitsPerEm;
	if ((numLong = num_long_hmtx(sft->font)) < 0)
		return -1;
	if ((hmtx = gettable(sft->font, "hmtx")) < 0)
		return -1;
	if (glyph < numLong) {
		/* glyph is inside long metrics segment. */
		offset = hmtx + 4 * glyph;
		if (sft->font->size < offset + 4)
			return -1;
		*advanceWidth = getu16(sft->font, offset) * factor;
		*leftSideBearing = geti16(sft->font, offset + 2) * factor;
		return 0;
	} else {
		/* glyph is inside short metrics segment. */
		if ((shmtx = hmtx + 4 * numLong) < 4)
			return -1;
		offset = shmtx + 2 + (glyph - numLong);
		if (sft->font->size < offset + 2)
			return -1;
		*advanceWidth = getu16(sft->font, shmtx - 4) * factor;
		*leftSideBearing = geti16(sft->font, offset) * factor;
		return 0;
	}
}

static int
loca_format(SFT_Font *font)
{
	long head;
	if ((head = gettable(font, "head")) < 0)
		return -1;
	if (font->size < (unsigned long) head + 54)
		return -1;
	return geti16(font, head + 50);
}

static long
outline_offset(SFT_Font *font, long glyph)
{
	long loca;
	if ((loca = gettable(font, "loca")) < 0)
		return -1;
	switch (loca_format(font)) {
	case 0:
		return getu16(font, loca + 2 * glyph) * 2L;
	case 1:
		return getu32(font, loca + 4 * glyph);
	default:
		return -1;
	}
}

static int
draw_simple(SFT *sft, unsigned long offset, int numContours, struct affine xAffine, struct affine yAffine)
{
	struct contour *contours = NULL;
	double *xCoords, *yCoords;
	uint8_t *memory = NULL, *flags;
	int i, numPts, value, repeat, ret = 0;
	
	if ((contours = malloc(numContours * sizeof(contours[0]))) == NULL)
		goto failure;

	if (sft->font->size < offset + numContours * 2)
		goto failure;
	for (numPts = i = 0; i < numContours; ++i) {
		contours[i].first = numPts;
		contours[i].last = getu16(sft->font, offset);
		offset += 2;
		numPts = contours[i].last + 1;
	}

	if ((memory = malloc((numPts + 2) * 17)) == NULL)
		goto failure;
	flags = memory + 2;
	xCoords = (double *) (flags + numPts) + 2;
	yCoords = xCoords + numPts + 2;

	/* Skip hinting instructions. */
	if (sft->font->size < offset + 2)
		goto failure;
	offset += 2 + getu16(sft->font, offset);
	/* Read flags. */
	for (value = repeat = i = 0; i < numPts; ++i) {
		if (repeat) {
			--repeat;
		} else {
			if (sft->font->size < offset + 1)
				goto failure;
			value = getu8(sft->font, offset++);
			if (value & 0x08) {
				if (sft->font->size < offset + 1)
					goto failure;
				repeat = getu8(sft->font, offset++);
			}
		}
		flags[i] = value;
	}
	/* Read x coordinates. */
	for (value = i = 0; i < numPts; ++i) {
		if (flags[i] & 0x02) {
			if (sft->font->size < offset + 1)
				goto failure;
			if (flags[i] & 0x10) {
				value += getu8(sft->font, offset++);
			} else {
				value -= getu8(sft->font, offset++);
			}
		} else if (!(flags[i] & 0x10)) {
			if (sft->font->size < offset + 2)
				goto failure;
			value += geti16(sft->font, offset);
			offset += 2;
		}
		xCoords[i] = AFFINE(xAffine, value);
	}
	/* Read y coordinates. */
	for (value = i = 0; i < numPts; ++i) {
		if (flags[i] & 0x04) {
			if (sft->font->size < offset + 1)
				goto failure;
			if (flags[i] & 0x20) {
				value += getu8(sft->font, offset++);
			} else {
				value -= getu8(sft->font, offset++);
			}
		} else if (!(flags[i] & 0x20)) {
			if (sft->font->size < offset + 2)
				goto failure;
			value += geti16(sft->font, offset);
			offset += 2;
		}
		yCoords[i] = AFFINE(yAffine, value);
	}
	/* Print contours. */
	for (int j = 0; j < numContours; ++j) {
		printf("contour #%d:\n", j);
		/* --i;
		flags[i] = flags[endPts[j]];
		xCoords[i] = xCoords[endPts[j]];
		yCoords[i] = yCoords[endPts[j]]; */
		for (i = contours[j].first; i <= contours[j].last; ++i) {
			printf("%s, %f, %f\n", flags[i] & 0x01 ? "ON " : "OFF", xCoords[i], yCoords[i]);
		}
	}
cleanup:
	free(contours);
	free(memory);
	return ret;

failure:
	ret = -1;
	goto cleanup;
}

static int
proc_outline(SFT *sft, unsigned long offset, double leftSideBearing, int extents[4])
{
	struct affine xAffine, yAffine;
	uint32_t *buffer;
	int unitsPerEm, numContours, width, height;
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	numContours = geti16(sft->font, offset);
	/* Set up linear transformations. */
	xAffine = (struct affine) { sft->xScale / unitsPerEm, sft->x + leftSideBearing };
	yAffine = (struct affine) { sft->yScale / unitsPerEm, sft->y };
	/* Calculate outline extents. */
	extents[0] = (int) AFFINE(xAffine, geti16(sft->font, offset + 2) - 1);
	extents[1] = (int) AFFINE(yAffine, geti16(sft->font, offset + 4) - 1);
	extents[2] = (int) ceil(AFFINE(xAffine, geti16(sft->font, offset + 6) + 1));
	extents[3] = (int) ceil(AFFINE(yAffine, geti16(sft->font, offset + 8) + 1));
	/* Render the outline (if requested). */
	if (sft->flags & SFT_CHAR_RENDER) {
		/* Make transformations relative to min corner. */
		xAffine.move -= extents[0];
		yAffine.move -= extents[1];
		/* Allocate internal buffer for drawing into. */
		width = extents[2] - extents[0];
		height = extents[3] - extents[1];
		if ((buffer = calloc(width * height, 4)) == NULL)
			return -1;
		if (numContours >= 0) {
			/* Glyph has a 'simple' outline consisting of a number of contours. */
			if (draw_simple(sft, offset + 10, numContours, xAffine, yAffine) < 0) {
				free(buffer);
				return -1;
			}
		} else {
			/* Glyph has a compound outline combined from mutiple other outlines. */
			/* TODO Implement this path! */
			free(buffer);
			return -1;
		}
		free(buffer);
	}
	return 0;
}

int
sft_char(SFT *sft, unsigned int charCode, int extents[4])
{
	double advanceWidth, leftSideBearing;
	long glyph, glyf, offset, next;
	if ((glyph = glyph_id(sft->font, charCode)) < 0)
		return -1;
	if (hor_metrics(sft, glyph, &advanceWidth, &leftSideBearing) < 0)
		return -1;
	if ((glyf = gettable(sft->font, "glyf")) < 0)
		return -1;
	if ((offset = outline_offset(sft->font, glyph)) < 0)
		return -1;
	if ((next = outline_offset(sft->font, glyph + 1)) < 0)
		return -1;
	if (sft->font->size < (unsigned long) glyf + 10)
		return -1;
	if (offset == next) {
		/* Glyph has completely empty outline. This is allowed by the spec. */
		memset(extents, 0, 4 * sizeof(int));
	} else {
		/* Glyph has an outline. */
		if (proc_outline(sft, glyf + offset, leftSideBearing, extents) < 0)
			return -1;
	}
	/* Advance into position for the next character (if requested). */
	if (sft->flags & SFT_CHAR_ADVANCE) {
		sft->x += advanceWidth;
		sft->glyph = glyph;
	}
	return 0;
}

