#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "schrift.h"

/* So as it turns out, these first three naive macros are actually
 * faster than any bit-tricks or specialized functions on amd64. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

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
compare_tables(const void *a, const void *b)
{
	return memcmp(a, b, 4);
}

static ssize_t
gettable(SFT_Font *font, char tag[4])
{
	if (font->size < 12) return -1;
	uint16_t numTables = getu16(font, 4);
	if (font->size < 12 + (size_t) numTables * 16) return -1;
	void *match = bsearch(tag, font->memory + 12, numTables, 16, compare_tables);
	if (match == NULL) return -1;
	ssize_t matchOffset = (uint8_t *) match - font->memory;
	return getu32(font, matchOffset + 8);
}

static int
readfile(SFT_Font *font, const char *filename)
{
	struct stat info;
	if (stat(filename, &info) < 0) {
		return -1;
	}
	font->size = info.st_size;
	font->memory = malloc(font->size);
	if (font->memory == NULL) {
		return -1;
	}
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
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
	SFT_Font *font = calloc(1, sizeof(SFT_Font));
	if (font == NULL) {
		return NULL;
	}
	if (readfile(font, filename) < 0) {
		free(font);
		return NULL;
	}
	uint32_t scalerType = getu32(font, 0);
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
	SFT *sft = calloc(1, sizeof(SFT));
	if (sft == NULL) return NULL;
	return sft;
}

void
sft_destroy(SFT *sft)
{
	if (sft == NULL) return;
	free(sft);
}

static int16_t
units_per_em(SFT *sft)
{
	ssize_t head = gettable(sft->font, "head");
	if (head < 0) return -1;
	if (sft->font->size < (size_t) head + 54) return -1;
	return getu16(sft->font, head + 18);
}

int
sft_setstyle(SFT *sft, struct SFT_Style style)
{
	sft->font = style.font;
	double sizeInPixels = 0.0;
	switch (style.units) {
	case 'd':
		sizeInPixels = style.size * style.vdpi / 72.0;
		break;
	case 'x':
	default:
		sizeInPixels = style.size;
		break;
	}
	int16_t unitsPerEm;
	if ((unitsPerEm = units_per_em(sft)) < 0) return -1;
	double unitsToPixels = sizeInPixels / unitsPerEm;
	double ratio = style.hdpi / style.vdpi;
	sft->xScale = unitsToPixels * ratio;
	sft->yScale = unitsToPixels;
	return 0;
}

int
sft_linegap(SFT *sft, double *gap)
{
	ssize_t hhea = gettable(sft->font, "hhea");
	if (hhea < 0) return -1;

	if (sft->font->size < (size_t) hhea + 36) return -1;
	*gap = geti16(sft->font, hhea + 8) * sft->yScale;
	return 0;
}

void
sft_move(SFT *sft, double x, double y)
{
	sft->x = x;
	sft->y = y;
	sft->glyph = 0;
}

static int
compare_segments(const void *a, const void *b)
{
	return memcmp(a, b, 2);
}

static long
cmap_fmt4(SFT *sft, size_t table, int charCode)
{
	/* TODO Guard against too big charCode. */

	if (sft->font->size < table + 8) return -1;
	uint16_t segCountX2 = getu16(sft->font, table);
	if ((segCountX2 & 1) || !segCountX2) return -1;

	size_t endCodes = table + 8;
	size_t startCodes = endCodes + segCountX2 + 2;
	size_t idDeltas = startCodes + segCountX2;
	size_t idRangeOffsets = idDeltas + segCountX2;
	if (sft->font->size < idRangeOffsets + segCountX2) return -1;

	uint8_t key[2] = { charCode >> 8, charCode };
	uintptr_t segIdxX2 = (uintptr_t) csearch(key,
		sft->font->memory + endCodes,
		segCountX2 / 2, 2, compare_segments);
	segIdxX2 -= (uintptr_t) sft->font->memory + endCodes;

	uint16_t startCode = getu16(sft->font, startCodes + segIdxX2);
	int16_t  idDelta = geti16(sft->font, idDeltas + segIdxX2);
	uint16_t idRangeOffset = getu16(sft->font, idRangeOffsets + segIdxX2);

	if (startCode > charCode) {
		return 0;
	}
	if (!idRangeOffset) {
		return (charCode + idDelta) & 0xFFFF;
	}

	size_t idOffset = idRangeOffsets + segIdxX2 + idRangeOffset + 2 * (charCode - startCode);
	if (sft->font->size < idOffset + 2) return -1;

	uint16_t id = getu16(sft->font, idOffset);

	return id ? (id + idDelta) & 0xFFFF : 0L;
}

long sft_transcribe(SFT *sft, int charCode)
{
	ssize_t cmap = gettable(sft->font, "cmap");
	if (cmap < 0) return -1;

	if (sft->font->size < (size_t) cmap + 4) return -1;
	uint16_t numEntries = getu16(sft->font, cmap + 2);
	
	if (sft->font->size < (size_t) cmap + 4 + numEntries * 8) return -1;
	for (unsigned int i = 0; i < numEntries; ++i) {
		size_t entry = cmap + 4 + i * 8;
		
		uint16_t platformId = getu16(sft->font, entry);
		uint16_t encodingId = getu16(sft->font, entry + 2);
		int type = platformId * 0100 + encodingId;
		
		if (type == 0003 || type == 0301) {
			
			size_t table = cmap + getu32(sft->font, entry + 4);
			if (sft->font->size < table + 6) return 1;

			switch (getu16(sft->font, table)) {
			case 4:
				return cmap_fmt4(sft, table + 6, charCode);
			default:
				return -1;
			}
		}
	}

	return -1;
}

void *
sft_char(SFT *sft, const char *ch, int len, int bounds[4])
{
	(void) sft;
	(void) ch;
	(void) len;
	(void) bounds;
	/* Translate codepoint to glyph id. */
	/* Look up offset into glyf table. */
	/* Compute glyph bounds. */
	/* Draw glyph outlines into buffer. */
	/* Render grayscale image from buffer. */
	/* Advance into position for the next char. */
	return NULL;
}

