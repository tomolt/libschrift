/* See LICENSE file for copyright and license details. */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "schrift.h"

/* macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SIGN(x) ((x) >= 0 ? 1 : -1)
/* Allocate values on the stack if they are small enough, else spill to heap. */
#define STACK_ALLOC(var, len, thresh) \
	uint8_t var##_stack_[thresh]; \
	var = (len) <= (thresh) ? (void *) var##_stack_ : malloc(len);
#define STACK_FREE(var) \
	if ((void *) var != (void *) var##_stack_) free(var);
#define INCREASE_ARRAY(arr, type) \
	((arr).num < (arr).cap ? 0 : stretch_array(&(arr), sizeof(type)))

enum { SrcMapping, SrcUser };

/* structs */
struct point   { double x, y; };
struct line    { struct point beg, end; };
struct curve   { struct point beg, ctrl, end; };
struct contour { int first, last; };
struct cell    { int16_t area, cover; };

struct array
{
	void *elems;
	int cap, num;
};

struct buffer
{
	struct cell **rows;
	int width, height;
};

struct SFT_Font
{
	const uint8_t *memory;
	unsigned long size;
	int source;
};

/* function declarations */
/* file loading */
static int  map_file(SFT_Font *font, const char *filename);
static void unmap_file(SFT_Font *font);
/* TTF parsing */
static int  init_array(struct array *array, int esize, int count);
static int  stretch_array(struct array *array, int esize);
static void *csearch(const void *key, const void *base,
	size_t nmemb, size_t size, int (*compar)(const void *, const void *));
static int  cmpu16(const void *a, const void *b);
static int  cmpu32(const void *a, const void *b);
static inline uint8_t  getu8 (SFT_Font *font, unsigned long offset);
static inline int8_t   geti8 (SFT_Font *font, unsigned long offset);
static inline uint16_t getu16(SFT_Font *font, unsigned long offset);
static inline int16_t  geti16(SFT_Font *font, unsigned long offset);
static inline uint32_t getu32(SFT_Font *font, unsigned long offset);
static long gettable(SFT_Font *font, char tag[4]);
static int  units_per_em(SFT_Font *font);
static long cmap_fmt4(SFT_Font *font, unsigned long table, unsigned long charCode);
static long cmap_fmt6(SFT_Font *font, unsigned long table, unsigned long charCode);
static long glyph_id(SFT_Font *font, unsigned long charCode);
static int  num_long_hmtx(SFT_Font *font);
static int  hor_metrics(const struct SFT *sft, long glyph, double *advanceWidth, double *leftSideBearing);
static int  loca_format(SFT_Font *font);
static long outline_offset(SFT_Font *font, long glyph);
static long simple_flags(SFT_Font *font, unsigned long offset, int numPts, uint8_t *flags);
static int  simple_points(SFT_Font *font, long offset, int numPts, uint8_t *flags, struct point *points);
static void transform_points(int numPts, struct point *points, double trf[6]);
static void clip_points(int numPts, struct point *points, struct buffer buf);
static int  draw_contours(struct buffer buf, int numContours, struct contour *contours, uint8_t *flags, struct point *points);
static int  draw_simple(const struct SFT *sft, long offset, int numContours, struct buffer buf, double transform[6]);
static int  draw_compound(const struct SFT *sft, unsigned long offset, struct buffer buf, double transform[6]);
static int  draw_outline(const struct SFT *sft, unsigned long offset, struct buffer buf, double transform[6]);
/* tesselation */
static struct point midpoint(struct point a, struct point b);
static int  is_flat(struct curve curve, double flatness);
static int  tesselate_curves(struct array curves, struct array *lines);
/* silhouette rasterization */
static inline int ifloor(double x);
static inline int quantize(double x);
static void draw_dot(struct buffer buf, int px, int py, double xAvg, double yDiff);
static void draw_line(struct buffer buf, struct line line);
/* post-processing */
static void post_process(struct buffer buf, uint8_t *image);

/* function implementations */

/* Loads a font from a user-supplied memory range. */
SFT_Font *
sft_loadmem(const void *mem, unsigned long size)
{
	SFT_Font *font;
	unsigned long scalerType;
	if ((font = calloc(1, sizeof(SFT_Font))) == NULL) {
		return NULL;
	}
	font->memory = mem;
	font->size = size;
	font->source = SrcUser;
	/* Check for a compatible scalerType (magic number). */
	scalerType = getu32(font, 0);
	if (scalerType != 0x00010000 && scalerType != 0x74727565) {
		sft_freefont(font);
		return NULL;
	}
	return font;
}

/* Loads a font from the file system. To do so, it has to map the entire font into memory. */
SFT_Font *
sft_loadfile(char const *filename)
{
	SFT_Font *font;
	unsigned long scalerType;
	if ((font = calloc(1, sizeof(SFT_Font))) == NULL) {
		return NULL;
	}
	if (map_file(font, filename) < 0) {
		free(font);
		return NULL;
	}
	/* Check for a compatible scalerType (magic number). */
	scalerType = getu32(font, 0);
	if (scalerType != 0x00010000 && scalerType != 0x74727565) {
		sft_freefont(font);
		return NULL;
	}
	return font;
}

void
sft_freefont(SFT_Font *font)
{
	if (font == NULL) return;
	/* Only unmap if we mapped it ourselves. */
	if (font->source == SrcMapping)
		unmap_file(font);
	free(font);
}

int
sft_linemetrics(const struct SFT *sft, double *ascent, double *descent, double *gap)
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

int
sft_char(const struct SFT *sft, unsigned long charCode, struct SFT_Char *chr)
{
	double transform[6];
	struct point corners[2];
	struct buffer buf;
	struct cell *cells, *ptr;
	double leftSideBearing;
	long glyph, glyf, offset, next;
	int unitsPerEm, x, y, w, h, i;

	if ((glyph = glyph_id(sft->font, charCode)) < 0)
		return -1;
	if (hor_metrics(sft, glyph, &chr->advance, &leftSideBearing) < 0)
		return -1;
	if ((offset = outline_offset(sft->font, glyph)) < 0)
		return -1;
	if ((next = outline_offset(sft->font, glyph + 1)) < 0)
		return -1;
	if (offset == next) {
		/* Glyph has completely empty outline. This is allowed by the spec. */
		chr->x = chr->y = 0;
		chr->width = chr->height = 0;
		chr->image = NULL;
		return 0;
	}
	/* Glyph has an outline. */
	if ((glyf = gettable(sft->font, "glyf")) < 0)
		return -1;
	offset += glyf;
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	/* Set up the linear transformation. */
	transform[0] = sft->xScale / unitsPerEm;
	transform[1] = 0.0;
	transform[2] = 0.0;
	transform[3] = sft->yScale / unitsPerEm;
	transform[4] = sft->x + leftSideBearing;
	transform[5] = sft->y;
	/* Calculate outline extents. */
	if (sft->font->size < (unsigned long) offset + 10)
		return -1;
	corners[0] = (struct point) { geti16(sft->font, offset + 2), geti16(sft->font, offset + 4) };
	corners[1] = (struct point) { geti16(sft->font, offset + 6), geti16(sft->font, offset + 8) };
	transform_points(2, corners, transform);
	/* Important: the following lines assume transform is an affine diagonal matrix at this point! */
	x = (int) floor(corners[0].x) - 1;
	y = (int) floor(corners[0].y) - 1;
	w = (int) ceil(corners[1].x) + 1 - x;
	h = (int) ceil(corners[1].y) + 1 - y;
	chr->x = x;
	chr->y = sft->flags & SFT_DOWNWARD_Y ? -(y + h) : y;
	chr->width = w;
	chr->height = h;
	/* Render the outline (if requested). */
	if (!(sft->flags & SFT_CHAR_IMAGE))
		return 0;
	/* Make transformation relative to min corner. */
	transform[4] -= x;
	transform[5] -= y;
	/* Allocate internal buffer for drawing into. */
	buf.width = w;
	buf.height = h;
	if ((cells = calloc(w * h, sizeof(cells[0]))) == NULL)
		return -1;
	if ((buf.rows = malloc(h * sizeof(buf.rows[0]))) == NULL)
		return -1;
	ptr = cells;
	for (i = 0; i < h; ++i) {
		buf.rows[i] = ptr;
		ptr += w;
	}
	if (draw_outline(sft, offset, buf, transform) < 0) {
		free(cells);
		free(buf.rows);
		return -1;
	}
	if (sft->flags & SFT_DOWNWARD_Y) {
		ptr = cells + w * h;
		for (i = 0; i < h; ++i) {
			ptr -= w;
			buf.rows[i] = ptr;
		}
	}
	if ((chr->image = calloc(w * h, 1)) == NULL) {
		free(cells);
		free(buf.rows);
		return -1;
	}
	post_process(buf, chr->image);
	free(cells);
	free(buf.rows);
	return 0;
}

static int
map_file(SFT_Font *font, const char *filename)
{
	struct stat info;
	int fd;
	font->memory = MAP_FAILED;
	font->size = 0;
	font->source = SrcMapping;
	if ((fd = open(filename, O_RDONLY)) < 0) {
		return -1;
	}
	if (fstat(fd, &info) < 0) {
		close(fd);
		return -1;
	}
	font->memory = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	font->size = info.st_size;
	close(fd);
	return font->memory == MAP_FAILED ? -1 : 0;
}

static void
unmap_file(SFT_Font *font)
{
	assert(font->memory != MAP_FAILED);
	munmap((void *) font->memory, font->size);
}

static int
init_array(struct array *array, int esize, int count)
{
	array->num = 0;
	array->cap = count;
	return (array->elems = malloc(count * esize)) == NULL ? -1 : 0;
}

static int
stretch_array(struct array *array, int esize)
{
	void *elems;
	int cap = array->cap * 2;
	/* TODO Use something safe like reallocarray() here! */
	if ((elems = realloc(array->elems, cap * esize)) == NULL)
		return -1;
	array->cap = cap;
	array->elems = elems;
	return 0;
}

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

/* Used as a comparison function for [bc]search(). */
static int
cmpu16(const void *a, const void *b)
{
	return memcmp(a, b, 2);
}

/* Used as a comparison function for [bc]search(). */
static int
cmpu32(const void *a, const void *b)
{
	return memcmp(a, b, 4);
}

static inline uint8_t
getu8(SFT_Font *font, unsigned long offset)
{
	assert(offset + 1 <= font->size);
	return *(font->memory + offset);
}

static inline int8_t
geti8(SFT_Font *font, unsigned long offset)
{
	return (int8_t) getu8(font, offset);
}

static inline uint16_t
getu16(SFT_Font *font, unsigned long offset)
{
	assert(offset + 2 <= font->size);
	const uint8_t *base = font->memory + offset;
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
	const uint8_t *base = font->memory + offset;
	uint32_t b3 = base[0], b2 = base[1], b1 = base[2], b0 = base[3]; 
	return (uint32_t) (b3 << 24 | b2 << 16 | b1 << 8 | b0);
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
units_per_em(SFT_Font *font)
{
	long head;
	if ((head = gettable(font, "head")) < 0)
		return -1;
	if (font->size < (unsigned long) head + 54)
		return -1;
	return getu16(font, head + 18);
}

static long
cmap_fmt4(SFT_Font *font, unsigned long table, unsigned long charCode)
{
	unsigned long endCodes, startCodes, idDeltas, idRangeOffsets, idOffset;
	unsigned int segCountX2, segIdxX2, startCode, idRangeOffset, id;
	int idDelta;
	uint8_t key[2] = { charCode >> 8, charCode };
	/* cmap format 4 only supports the Unicode BMP. */
	if (charCode > 0xFFFF)
		return 0;
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
cmap_fmt6(SFT_Font *font, unsigned long table, unsigned long charCode)
{
	unsigned int firstCode, entryCount;
	/* cmap format 6 only supports the Unicode BMP. */
	if (charCode > 0xFFFF)
		return 0;
	if (font->size < table + 4)
		return -1;
	firstCode = getu16(font, table);
	entryCount = getu16(font, table + 2);
	if (font->size < table + 4 + 2 * entryCount)
		return -1;
	if (charCode < firstCode)
		return -1;
	charCode -= firstCode;
	if (!(charCode < entryCount))
		return -1;
	return getu16(font, table + 4 + 2 * charCode);
}

/* Maps Unicode code points to glyph indices. */
static long
glyph_id(SFT_Font *font, unsigned long charCode)
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
			case 6:
				return cmap_fmt6(font, table + 6, charCode);
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
hor_metrics(const struct SFT *sft, long glyph, double *advanceWidth, double *leftSideBearing)
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

/* Returns the offset into the font that the glyph's outline is stored at. */
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

/* For a 'simple' outline, determines each point of the outline with a set of flags. */
static long
simple_flags(SFT_Font *font, unsigned long offset, int numPts, uint8_t *flags)
{
	int value = 0, repeat = 0, i;
	for (i = 0; i < numPts; ++i) {
		if (repeat) {
			--repeat;
		} else {
			if (font->size < offset + 1)
				return -1;
			value = getu8(font, offset++);
			if (value & 0x08) {
				if (font->size < offset + 1)
					return -1;
				repeat = getu8(font, offset++);
			}
		}
		flags[i] = value;
	}
	return offset;
}

/* For a 'simple' outline, decodes both X and Y coordinates for each point of the outline. */
static int
simple_points(SFT_Font *font, long offset, int numPts, uint8_t *flags, struct point *points)
{
	long xBytes = 0, yBytes = 0, xOffset, yOffset, x = 0, y = 0;
	int i;
	for (i = 0; i < numPts; ++i) {
		if (flags[i] & 0x02) xBytes += 1;
		else if (!(flags[i] & 0x10)) xBytes += 2;
		if (flags[i] & 0x04) yBytes += 1;
		else if (!(flags[i] & 0x20)) yBytes += 2;
	}
	if (font->size < (unsigned long) offset + xBytes + yBytes)
		return -1;
	xOffset = offset;
	yOffset = offset + xBytes;
	for (i = 0; i < numPts; ++i) {
		if (flags[i] & 0x02) {
			x += (long) getu8(font, xOffset++) * (flags[i] & 0x10 ? 1 : -1);
		} else if (!(flags[i] & 0x10)) {
			x += geti16(font, xOffset);
			xOffset += 2;
		}
		if (flags[i] & 0x04) {
			y += (long) getu8(font, yOffset++) * (flags[i] & 0x20 ? 1 : -1);
		} else if (!(flags[i] & 0x20)) {
			y += geti16(font, yOffset);
			yOffset += 2;
		}
		points[i] = (struct point) { x, y };
	}
	return 0;
}

/* Applies an affine linear transformation matrix to a set of points. */
static void
transform_points(int numPts, struct point *points, double trf[6])
{
	struct point *restrict pt;
	int i;
	for (i = 0; i < numPts; ++i) {
		pt = &points[i];
		*pt = (struct point) {
			pt->x * trf[0] + pt->y * trf[2] + trf[4],
			pt->x * trf[1] + pt->y * trf[3] + trf[5]
		};
	}
}

static void
clip_points(int numPts, struct point *points, struct buffer buf)
{
	struct point *restrict pt;
	double dv;
	uint64_t *ip = (void *) &dv;
	int i;
	for (i = 0; i < numPts; ++i) {
		pt = &points[i];
		if (pt->x < 0) {
			pt->x = 0;
		}
		if (pt->x >= buf.width) {
			dv = buf.width;
			--*ip;
			pt->x = dv;
		}
		if (pt->y < 0) {
			pt->y = 0;
		}
		if (pt->y >= buf.height) {
			dv = buf.height;
			--*ip;
			pt->y = dv;
		}
	}
}

static int
draw_contours(struct buffer buf, int numContours, struct contour *contours, uint8_t *flags, struct point *points)
{
#define LINES  ((struct line  *) lines.elems)
#define CURVES ((struct curve *) curves.elems)

	struct array lines, curves;
	struct point looseEnd, beg, ctrl, center;
	int c, f, l, firstOn, lastOn, gotCtrl, i;

	if (init_array(&lines, sizeof(struct line), 128) < 0)
		goto failure;
	if (init_array(&curves, sizeof(struct curve), 32) < 0)
		goto failure;

	for (c = 0; c < numContours; ++c) {
		f = contours[c].first;
		l = contours[c].last;
		firstOn = flags[f] & 0x01;
		lastOn = flags[l] & 0x01;
		looseEnd = firstOn ? points[f++] : lastOn ? points[l--] : midpoint(points[f], points[l]);
		beg = looseEnd;
		gotCtrl = 0;
		for (i = f; i <= l; ++i) {
			if (flags[i] & 0x01) {
				if (gotCtrl) {
					if (INCREASE_ARRAY(curves, struct curve) < 0)
						goto failure;
					CURVES[curves.num++] = (struct curve) { beg, ctrl, points[i] };
				} else if (beg.y != points[i].y) {
					if (INCREASE_ARRAY(lines, struct line) < 0)
						goto failure;
					LINES[lines.num++] = (struct line) { beg, points[i] };
				}
				beg = points[i];
				gotCtrl = 0;
			} else {
				if (gotCtrl) {
					center = midpoint(ctrl, points[i]);
					if (INCREASE_ARRAY(curves, struct curve) < 0)
						goto failure;
					CURVES[curves.num++] = (struct curve) { beg, ctrl, center };
					beg = center;
				}
				ctrl = points[i];
				gotCtrl = 1;
			}
		}
		if (gotCtrl) {
			if (INCREASE_ARRAY(curves, struct curve) < 0)
				goto failure;
			CURVES[curves.num++] = (struct curve) { beg, ctrl, looseEnd };
		} else if (beg.y != looseEnd.y) {
			if (INCREASE_ARRAY(lines, struct line) < 0)
				goto failure;
			LINES[lines.num++] = (struct line) { beg, looseEnd };
		}
	}

	if (tesselate_curves(curves, &lines) < 0)
		goto failure;
	for (i = 0; i < lines.num; ++i) {
		draw_line(buf, LINES[i]);
	}

	free(lines.elems);
	free(curves.elems);
	return 0;
failure:
	free(lines.elems);
	free(curves.elems);
	return -1;
	
#undef LINES
#undef CURVES
}

static int
draw_simple(const struct SFT *sft, long offset, int numContours, struct buffer buf, double transform[6])
{
	struct point *points;
	struct contour *contours = NULL;
	uint8_t *memory = NULL, *flags;
	unsigned long memLen;
	int i, numPts, top;

	if (sft->font->size < (unsigned long) offset + numContours * 2 + 2)
		goto failure;
	numPts = getu16(sft->font, offset + (numContours - 1) * 2) + 1;
	
	memLen  = numPts * sizeof(points[0]);
	memLen += numContours * sizeof(contours[0]);
	memLen += numPts * sizeof(flags[0]);
	STACK_ALLOC(memory, memLen, 2048) ;
	if (memory == NULL)
		goto failure;
	points = (struct point *) memory;
	contours = (struct contour *) (points + numPts);
	flags = (uint8_t *) (contours + numContours);

	for (top = i = 0; i < numContours; ++i) {
		contours[i].first = top;
		contours[i].last = getu16(sft->font, offset);
		if (contours[i].first > contours[i].last)
			goto failure;
		top = contours[i].last + 1;
		offset += 2;
	}
	offset += 2 + getu16(sft->font, offset);

	if ((offset = simple_flags(sft->font, offset, numPts, flags)) < 0)
		goto failure;
	if (simple_points(sft->font, offset, numPts, flags, points) < 0)
		goto failure;
	transform_points(numPts, points, transform);
	clip_points(numPts, points, buf);
	if (draw_contours(buf, numContours, contours, flags, points) < 0)
		goto failure;

	STACK_FREE(memory) ;
	return 0;
failure:
	STACK_FREE(memory) ;
	return -1;
}

static int
draw_compound(const struct SFT *sft, unsigned long offset, struct buffer buf, double transform[6])
{
	(void) buf, (void) transform;
	unsigned int flags, glyph;
	int xOffset, yOffset;
	int i = 0;
	do {
		if (sft->font->size < offset + 4)
			return -1;
		flags = getu16(sft->font, offset);
		glyph = getu16(sft->font, offset + 2);
		offset += 4;
		if (!(flags & 0x002))
			return -1;
		if (flags & 0x01) {
			if (sft->font->size < offset + 4)
				return -1;
			xOffset = geti16(sft->font, offset);
			yOffset = geti16(sft->font, offset + 2);
			offset += 4;
		} else {
			if (sft->font->size < offset + 2)
				return -1;
			xOffset = geti8(sft->font, offset);
			yOffset = geti8(sft->font, offset + 1);
			offset += 2;
		}
		if (flags & 0x008) {
			offset += 2;
		} else if (flags & 0x040) {
			offset += 4;
		} else if (flags & 0x80) {
			offset += 8;
		}
#if 0
		fprintf(stderr, "Compound component #%d is: %u\n", i, glyph);
		fprintf(stderr, "The component's flags are: 0x%03x\n", flags);
		fprintf(stderr, "The component's X offset is: %d FUnits\n", xOffset);
		fprintf(stderr, "The component's Y offset is: %d FUnits\n", yOffset);
#else
		(void) glyph, (void) xOffset, (void) yOffset, (void) i;
#endif
		++i;
	} while (flags & 0x020);

	return -1;
}

static int
draw_outline(const struct SFT *sft, unsigned long offset, struct buffer buf, double transform[6])
{
	int numContours;
	if (sft->font->size < offset + 10)
		return -1;
	numContours = geti16(sft->font, offset);
	if (numContours >= 0) {
		/* Glyph has a 'simple' outline consisting of a number of contours. */
		return draw_simple(sft, offset + 10, numContours, buf, transform);
	} else {
		/* Glyph has a compound outline combined from mutiple other outlines. */
		return draw_compound(sft, offset + 10, buf, transform);
	}
}

static struct point
midpoint(struct point a, struct point b)
{
	return (struct point) {
		0.5 * a.x + 0.5 * b.x,
		0.5 * a.y + 0.5 * b.y
	};
}

/* A heuristic to tell whether a given curve can be approximated closely enough by a line. */
static int
is_flat(struct curve curve, double flatness)
{
	struct point mid = midpoint(curve.beg, curve.end);
	double x = curve.ctrl.x - mid.x;
	double y = curve.ctrl.y - mid.y;
	return x * x + y * y <= flatness * flatness;
}

static int
tesselate_curves(struct array curves, struct array *lines)
{
	/*
	From my tests I can conclude that this stack barely reaches a top height
	of 4 elements even for the largest font sizes I'm willing to support. And
	as space requirements should only grow logarithmically, I think 10 is
	more than enough.
	*/
#define STACK_SIZE 10
#define LINES  ((struct line  *) lines->elems)
#define CURVES ((struct curve *) curves.elems)
	struct curve stack[STACK_SIZE], curve;
	struct point ctrl0, ctrl1, pivot;
	int top, i;
	for (i = 0; i < curves.num; ++i) {
		top = 0;
		curve = CURVES[i];
		for (;;) {
			if (is_flat(curve, 0.5) || top >= STACK_SIZE) {
				if (INCREASE_ARRAY(*lines, struct line) < 0)
					return -1;
				LINES[lines->num++] = (struct line) { curve.beg, curve.end };
				if (top == 0) break;
				curve = stack[--top];
			} else {
				ctrl0 = midpoint(curve.beg, curve.ctrl);
				ctrl1 = midpoint(curve.ctrl, curve.end);
				pivot = midpoint(ctrl0, ctrl1);
				stack[top++] = (struct curve) { curve.beg, ctrl0, pivot };
				curve = (struct curve) { pivot, ctrl1, curve.end };
			}
		}
	}
	return 0;
#undef STACK_SIZE
#undef LINES
#undef CURVES
}

/* Much faster than the builtin floor() function (because it doesn't check infinities etc.). 
 * Since it's called in a very hot loop, the speed difference is worth it. */
static inline int
ifloor(double x)
{
	return (int) x - (x < 0.0);
}

/* [0, 1] --> { 0, ..., 255 } */
static inline int
quantize(double x)
{
	return ifloor(x * 255 + 0.5);
}

static void
draw_dot(struct buffer buf, int px, int py, double xAvg, double yDiff)
{
	struct cell *restrict ptr = &buf.rows[py][px];
	struct cell cell = *ptr;
	cell.cover += quantize(yDiff);
	cell.area += quantize((1.0 - xAvg) * yDiff);
	*ptr = cell;
}

/* Draws a line into the buffer. Uses a custom 2D raycasting algorithm to do so. */
static void
draw_line(struct buffer buf, struct line line)
{
	double originX, originY;
	double goalX, goalY;
	double deltaX, deltaY;
	double nextCrossingX = 100.0, nextCrossingY = 100.0;
	double crossingGapX = 0.0, crossingGapY = 0.0;
	double prevDistance = 0.0;
	int pixelX, pixelY;
	int iter, numIters;

	originX = line.beg.x;
	goalX = line.end.x;
	deltaX = goalX - originX;
	pixelX = ifloor(originX);
	if (deltaX != 0.0) {
		crossingGapX = fabs(1.0 / deltaX);
		if (deltaX > 0.0) {
			nextCrossingX = (floor(originX) + 1.0 - originX) * crossingGapX;
		} else {
			nextCrossingX = (originX - floor(originX)) * crossingGapX;
		}
	}

	originY = line.beg.y;
	goalY = line.end.y;
	deltaY = goalY - originY;
	pixelY = ifloor(originY);
	if (deltaY != 0.0) {
		crossingGapY = fabs(1.0 / deltaY);
		if (deltaY > 0.0) {
			nextCrossingY = (floor(originY) + 1.0 - originY) * crossingGapY;
		} else {
			nextCrossingY = (originY - floor(originY)) * crossingGapY;
		}
	}

	numIters = abs(ifloor(goalX) - ifloor(originX)) + abs(ifloor(goalY) - ifloor(originY));
	for (iter = 0; iter < numIters; ++iter) {
		if (nextCrossingX < nextCrossingY) {
			double deltaDistance = nextCrossingX - prevDistance;
			double averageX = (deltaX > 0) - 0.5 * deltaX * deltaDistance;
			draw_dot(buf, pixelX, pixelY, averageX, deltaY * deltaDistance);
			pixelX += SIGN(deltaX);
			prevDistance = nextCrossingX;
			nextCrossingX += crossingGapX;
		} else {
			double deltaDistance = nextCrossingY - prevDistance;
			double x = originX - pixelX + nextCrossingY * deltaX;
			double averageX = x - 0.5 * deltaX * deltaDistance;
			draw_dot(buf, pixelX, pixelY, averageX, deltaY * deltaDistance);
			pixelY += SIGN(deltaY);
			prevDistance = nextCrossingY;
			nextCrossingY += crossingGapY;
		}
	}

	double deltaDistance = 1.0 - prevDistance;
	double averageX = (line.end.x - pixelX) - 0.5 * deltaX * deltaDistance;
	draw_dot(buf, pixelX, pixelY, averageX, deltaY * deltaDistance);
}

/* Integrate the values in the buffer to arrive at the final grayscale image. */
static void
post_process(struct buffer buf, uint8_t *image)
{
	struct cell *restrict in, cell;
	uint8_t *restrict out;
	int x, y, accum;
	out = image;
	for (y = 0; y < buf.height; ++y) {
		accum = 0;
		in = buf.rows[y];
		for (x = 0; x < buf.width; ++x) {
			cell = *in++;
			*out++ = MIN(abs(accum + cell.area), 255);
			accum += cell.cover;
		}
	}
}

