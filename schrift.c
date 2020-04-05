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

struct SFT_Font
{
	uint8_t *memory;
	size_t size;
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
	if (nmemb == 0) return NULL;
	const uint8_t *bytes = base;
	size_t low = 0, high = nmemb - 1;
	while (low != high) {
		size_t mid = low + (high - low) / 2;
		const void *sample = bytes + mid * size;
		if (compar(key, sample) > 0) {
			low = mid + 1;
		} else {
			high = mid;
		}
	}
	return (uint8_t *) bytes + low * size;
}

static inline uint8_t
getu8(SFT_Font *font, size_t offset)
{
	assert(offset + 1 <= font->size);
	return *(font->memory + offset);
}

static inline uint16_t
getu16(SFT_Font *font, size_t offset)
{
	assert(offset + 2 <= font->size);
	uint8_t *base = font->memory + offset;
	uint16_t b1 = base[0], b0 = base[1]; 
	return (uint16_t) (b1 << 8 | b0);
}

static inline int16_t
geti16(SFT_Font *font, size_t offset)
{
	return (int16_t) getu16(font, offset);
}

static inline uint32_t
getu32(SFT_Font *font, size_t offset)
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

static ssize_t
gettable(SFT_Font *font, char tag[4])
{
	if (font->size < 12) return -1;
	uint16_t numTables = getu16(font, 4);
	if (font->size < 12 + (size_t) numTables * 16) return -1;
	void *match = bsearch(tag, font->memory + 12, numTables, 16, cmpu32);
	if (match == NULL) return -1;
	ssize_t matchOffset = (uint8_t *) match - font->memory;
	return getu32(font, matchOffset + 8);
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
	uint32_t scalerType;
	if ((font = calloc(1, sizeof(SFT_Font))) == NULL) {
		return NULL;
	}
	if (readfile(font, filename) < 0) {
		free(font);
		return NULL;
	}
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

static int16_t
units_per_em(SFT_Font *font)
{
	ssize_t head;
	if ((head = gettable(font, "head")) < 0)
		return -1;
	if (font->size < (size_t) head + 54)
		return -1;
	return getu16(font, head + 18);
}

int
sft_linemetrics(SFT *sft, double *ascent, double *descent, double *gap)
{
	ssize_t hhea;
	double factor;
	int16_t unitsPerEm;
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	if ((hhea = gettable(sft->font, "hhea")) < 0)
		return -1;
	if (sft->font->size < (size_t) hhea + 36) return -1;
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
	sft->glyph = 0;
}

static long
cmap_fmt4(SFT_Font *font, size_t table, int charCode)
{
	size_t endCodes, startCodes, idDeltas, idRangeOffsets, idOffset;
	uintptr_t segIdxX2;
	uint16_t segCountX2, startCode, idRangeOffset, id;
	int16_t idDelta;
	uint8_t key[2] = { charCode >> 8, charCode };
	/* TODO Guard against too big charCode. */

	if (font->size < table + 8) return -1;
	segCountX2 = getu16(font, table);
	if ((segCountX2 & 1) || !segCountX2) return -1;

	endCodes = table + 8;
	startCodes = endCodes + segCountX2 + 2;
	idDeltas = startCodes + segCountX2;
	idRangeOffsets = idDeltas + segCountX2;
	if (font->size < idRangeOffsets + segCountX2) return -1;

	segIdxX2 = (uintptr_t) csearch(key,
		font->memory + endCodes,
		segCountX2 / 2, 2, cmpu16);
	segIdxX2 -= (uintptr_t) font->memory + endCodes;

	if ((startCode = getu16(font, startCodes + segIdxX2)) > charCode)
		return 0;
	idDelta = geti16(font, idDeltas + segIdxX2);
	if (!(idRangeOffset = getu16(font, idRangeOffsets + segIdxX2)))
		return (charCode + idDelta) & 0xFFFF;

	idOffset = idRangeOffsets + segIdxX2 + idRangeOffset + 2 * (charCode - startCode);
	if (font->size < idOffset + 2) return -1;
	id = getu16(font, idOffset);
	return id ? (id + idDelta) & 0xFFFF : 0L;
}

static long
glyph_id(SFT_Font *font, int charCode)
{
	size_t entry, table;
	ssize_t cmap;
	unsigned int i;
	int type;
	uint16_t numEntries, platformId, encodingId;

	cmap = gettable(font, "cmap");
	if (cmap < 0) return -1;

	if (font->size < (size_t) cmap + 4) return -1;
	numEntries = getu16(font, cmap + 2);
	
	if (font->size < (size_t) cmap + 4 + numEntries * 8) return -1;
	for (i = 0; i < numEntries; ++i) {
		entry = cmap + 4 + i * 8;
		
		platformId = getu16(font, entry);
		encodingId = getu16(font, entry + 2);
		type = platformId * 0100 + encodingId;
		
		if (type == 0003 || type == 0301) {
			
			table = cmap + getu32(font, entry + 4);
			if (font->size < table + 6) return 1;

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
	ssize_t hhea;
	if ((hhea = gettable(font, "hhea")) < 0)
		return -1;
	if (font->size < (size_t) hhea + 36)
		return -1;
	return getu16(font, hhea + 34);
}

static int
hor_metrics(SFT *sft, long glyph, double *advanceWidth, double *leftSideBearing)
{
	size_t offset, shmtx;
	ssize_t hmtx;
	double factor;
	int numLong;
	int16_t unitsPerEm;
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

static int16_t
loca_format(SFT_Font *font)
{
	ssize_t head;
	if ((head = gettable(font, "head")) < 0)
		return -1;
	if (font->size < (size_t) head + 54)
		return -1;
	return geti16(font, head + 50);
}

static ssize_t
outline_offset(SFT_Font *font, long glyph)
{
	ssize_t loca = gettable(font, "loca");
	if (loca < 0) return -1;
	switch (loca_format(font)) {
	case 0:
		return getu16(font, loca + 2 * glyph) * 2L;
	case 1:
		return getu32(font, loca + 4 * glyph);
	default:
		return -1;
	}
}

int
sft_char(SFT *sft, int charCode, int extents[4])
{
	struct affine xAffine, yAffine;
	uint32_t *buffer;
	size_t outline;
	ssize_t glyf, offset, next;
	long glyph;
	double advanceWidth, leftSideBearing;
	int16_t unitsPerEm, numContours;
	int width, height;

	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
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
	if (sft->font->size < (size_t) glyf + 10)
		return -1;

	if (offset == next) {
		memset(extents, 0, 4 * sizeof(int));
	} else {
		outline = glyf + offset;
		numContours = geti16(sft->font, outline);
		(void) numContours;
		xAffine = (struct affine) { sft->xScale / unitsPerEm, sft->x + leftSideBearing };
		yAffine = (struct affine) { sft->yScale / unitsPerEm, sft->y };
		extents[0] = (int) AFFINE(xAffine, geti16(sft->font, outline + 2) - 1);
		extents[1] = (int) AFFINE(yAffine, geti16(sft->font, outline + 4) - 1);
		extents[2] = (int) ceil(AFFINE(xAffine, geti16(sft->font, outline + 6) + 1));
		extents[3] = (int) ceil(AFFINE(yAffine, geti16(sft->font, outline + 8) + 1));

		if (sft->flags & SFT_CHAR_RENDER) {
			xAffine.move -= extents[0];
			yAffine.move -= extents[1];
			width = extents[2] - extents[0];
			height = extents[3] - extents[1];
			buffer = calloc(width * height, 4);
			if ((buffer = calloc(width * height, 4)) == NULL)
				return -1;
			free(buffer);
		}
	}

	if (sft->flags & SFT_CHAR_ADVANCE) {
		sft->x += advanceWidth;
		sft->glyph = glyph;
	}

	return 0;
}

