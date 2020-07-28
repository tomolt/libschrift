/* See LICENSE file for copyright and license details. */

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
# define restrict __restrict
#endif

#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
#else
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

#include "schrift.h"

#define SCHRIFT_VERSION "0.8.0"

#define FILE_MAGIC_ONE             0x00010000
#define FILE_MAGIC_TWO             0x74727565

#define HORIZONTAL_KERNING         0x01
#define MINIMUM_KERNING            0x02
#define CROSS_STREAM_KERNING       0x04
#define OVERRIDE_KERNING           0x08

#define POINT_IS_ON_CURVE          0x01
#define X_CHANGE_IS_SMALL          0x02
#define Y_CHANGE_IS_SMALL          0x04
#define REPEAT_FLAG                0x08
#define X_CHANGE_IS_ZERO           0x10
#define X_CHANGE_IS_POSITIVE       0x10
#define Y_CHANGE_IS_ZERO           0x20
#define Y_CHANGE_IS_POSITIVE       0x20

#define OFFSETS_ARE_LARGE          0x001
#define ACTUAL_XY_OFFSETS          0x002
#define GOT_A_SINGLE_SCALE         0x008
#define THERE_ARE_MORE_COMPONENTS  0x020
#define GOT_AN_X_AND_Y_SCALE       0x040
#define GOT_A_SCALE_MATRIX         0x080

/* macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SIGN(x) ((x) >= 0 ? 1 : -1)
/* Allocate values on the stack if they are small enough, else spill to heap. */
#define STACK_ALLOC(var, type, thresh, count) \
	type var##_stack_[thresh]; \
	var = (count) <= (thresh) ? var##_stack_ : calloc(sizeof(type), count);
#define STACK_FREE(var) \
	if (var != var##_stack_) free(var);

enum { SrcMapping, SrcUser };

/* structs */
struct point { double x, y; };
struct line  { uint_least16_t beg, end; };
struct curve { uint_least16_t beg, end, ctrl; };
struct cell  { double area, cover; };

struct outline
{
	struct point *points;
	struct curve *curves;
	struct line *lines;
	uint_least16_t numPoints, numCurves, numLines;
	uint_least16_t capPoints, capCurves, capLines;
};

struct buffer
{
	struct cell **rows;
	unsigned int width, height;
};

struct SFT_Font
{
	const uint8_t *memory;
	uint_fast32_t size;
#if defined(_WIN32)
	HANDLE mapping;
#endif
	int source;
	
	uint_least16_t unitsPerEm;
	int_least16_t locaFormat;
	uint_least16_t numLongHmtx;
};

/* function declarations */
/* generic utility functions */
static void *sft_reallocarray(void *optr, size_t nmemb, size_t size);
static inline int fast_floor(double x);
static inline int fast_ceil(double x);
/* file loading */
static int  map_file(SFT_Font *font, const char *filename);
static void unmap_file(SFT_Font *font);
static int  init_font(SFT_Font *font);
/* mathematical utilities */
static struct point midpoint(struct point a, struct point b);
static void transform_points(uint_fast16_t numPts, struct point *points, double trf[6]);
static void clip_points(uint_fast16_t numPts, struct point *points, unsigned int width, unsigned int height);
/* 'buffer' data structure management */
static int  init_buffer(struct buffer *buf, unsigned int width, unsigned int height);
static void free_buffer(struct buffer *buf);
static void flip_buffer(struct buffer *buf);
/* 'outline' data structure management */
static int  init_outline(struct outline *outl);
static void free_outline(struct outline *outl);
static int  grow_points(struct outline *outl);
static int  grow_curves(struct outline *outl);
static int  grow_lines(struct outline *outl);
/* TTF parsing utilities */
static void *csearch(const void *key, const void *base,
	size_t nmemb, size_t size, int (*compar)(const void *, const void *));
static int  cmpu16(const void *a, const void *b);
static int  cmpu32(const void *a, const void *b);
static inline uint8_t  getu8 (SFT_Font *font, uint_fast32_t offset);
static inline int8_t   geti8 (SFT_Font *font, uint_fast32_t offset);
static inline uint16_t getu16(SFT_Font *font, uint_fast32_t offset);
static inline int16_t  geti16(SFT_Font *font, uint_fast32_t offset);
static inline uint32_t getu32(SFT_Font *font, uint_fast32_t offset);
static int gettable(SFT_Font *font, char tag[4], uint_fast32_t *offset);
/* codepoint -> glyph */
static int  cmap_fmt4(SFT_Font *font, uint_fast32_t table, uint_fast32_t charCode, uint_fast32_t *glyph);
static int  cmap_fmt6(SFT_Font *font, uint_fast32_t table, uint_fast32_t charCode, uint_fast32_t *glyph);
static int  glyph_id(SFT_Font *font, uint_fast32_t charCode, uint_fast32_t *glyph);
/* glyph -> hmtx */
static int  hor_metrics(SFT_Font *font, uint_fast32_t glyph, int *advanceWidth, int *leftSideBearing);
/* glyph -> outline */
static int  outline_offset(SFT_Font *font, uint_fast32_t glyph, uint_fast32_t *offset);
/* decoding outlines */
static int  simple_flags(SFT_Font *font, uint_fast32_t *offset, uint_fast16_t numPts, uint8_t *flags);
static int  simple_points(SFT_Font *font, uint_fast32_t offset, uint_fast16_t numPts, uint8_t *flags, struct point *points);
static int  decode_contour(uint8_t *flags, uint_fast16_t basePoint, uint_fast16_t count, struct outline *outl);
static int  simple_outline(SFT_Font *font, uint_fast32_t offset, unsigned int numContours, struct outline *outl);
static int  compound_outline(SFT_Font *font, uint_fast32_t offset, int recDepth, struct outline *outl);
static int  decode_outline(SFT_Font *font, uint_fast32_t offset, int recDepth, struct outline *outl);
/* tesselation */
static int  is_flat(struct outline *outl, struct curve curve, double flatness);
static int  tesselate_curve(struct curve curve, struct outline *outl);
static int  tesselate_curves(struct outline *outl);
/* silhouette rasterization */
static void draw_dot(struct buffer buf, int px, int py, double xAvg, double yDiff);
static void draw_line(struct buffer buf, struct point origin, struct point goal);
static void draw_lines(struct outline *outl, struct buffer buf);
/* post-processing */
static void post_process(struct buffer buf, uint8_t *image);
/* glyph rendering */
static int render_image(const struct SFT *sft, uint_fast32_t offset, double transform[6], struct SFT_Char *chr);

/* function implementations */

const char *
sft_version(void)
{
	return SCHRIFT_VERSION;
}

/* Loads a font from a user-supplied memory range. */
SFT_Font *
sft_loadmem(const void *mem, unsigned long size)
{
	SFT_Font *font;
	if ((font = calloc(1, sizeof(SFT_Font))) == NULL) {
		return NULL;
	}
	font->memory = mem;
	if (size > UINT32_MAX) {
		sft_freefont(font);
		return NULL;
	}
	font->size = (uint_fast32_t) size;
	font->source = SrcUser;
	if (init_font(font) < 0) {
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
	if ((font = calloc(1, sizeof(SFT_Font))) == NULL) {
		return NULL;
	}
	if (map_file(font, filename) < 0) {
		free(font);
		return NULL;
	}
	if (init_font(font) < 0) {
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

static int
init_font(SFT_Font *font)
{
	uint_fast32_t scalerType, head, hhea;

	/* Check for a compatible scalerType (magic number). */
	scalerType = getu32(font, 0);
	if (scalerType != FILE_MAGIC_ONE && scalerType != FILE_MAGIC_TWO)
		return -1;

	if (gettable(font, "head", &head) < 0)
		return -1;
	if (font->size < head + 54)
		return -1;
	font->unitsPerEm = getu16(font, head + 18);
	font->locaFormat = geti16(font, head + 50);

	if (gettable(font, "hhea", &hhea) < 0)
		return -1;
	if (font->size < hhea + 36)
		return -1;
	font->numLongHmtx = getu16(font, hhea + 34);

	return 0;
}

int
sft_linemetrics(const struct SFT *sft, double *ascent, double *descent, double *gap)
{
	double factor;
	uint_fast32_t hhea;
	if (gettable(sft->font, "hhea", &hhea) < 0)
		return -1;
	if (sft->font->size < hhea + 36) return -1;
	factor = sft->yScale / sft->font->unitsPerEm;
	*ascent  = geti16(sft->font, hhea + 4) * factor;
	*descent = geti16(sft->font, hhea + 6) * factor;
	*gap     = geti16(sft->font, hhea + 8) * factor;
	return 0;
}

int
sft_kerning(const struct SFT *sft, unsigned long leftChar, unsigned long rightChar, double kerning[2])
{
	void *match;
	uint_fast32_t offset;
	unsigned int numTables, numPairs, length, format, flags;
	int value;
	uint8_t key[4];

	kerning[0] = 0.0;
	kerning[1] = 0.0;

	if (gettable(sft->font, "kern", &offset) < 0)
		return 0;

	/* Read kern table header. */
	if (sft->font->size < offset + 4)
		return -1;
	if (getu16(sft->font, offset) != 0)
		return 0;
	numTables = getu16(sft->font, offset + 2);
	offset += 4;

	while (numTables > 0) {
		/* Read subtable header. */
		if (sft->font->size < offset + 6)
			return -1;
		length = getu16(sft->font, offset + 2);
		format = getu8 (sft->font, offset + 4);
		flags  = getu8 (sft->font, offset + 5);
		offset += 6;

		if (format == 0 && (flags & HORIZONTAL_KERNING) && !(flags & MINIMUM_KERNING)) {
			/* Read format 0 header. */
			if (sft->font->size < offset + 8)
				return -1;
			numPairs = getu16(sft->font, offset);
			offset += 8;
			/* Look up character code pair via binary search. */
			key[0] = (leftChar >> 8) & 0xFF;
			key[1] = leftChar & 0xFF;
			key[2] = (rightChar >> 8) & 0xFF;
			key[3] = rightChar & 0xFF;
			if ((match = bsearch(key, sft->font->memory + offset,
				numPairs, 6, cmpu32)) != NULL) {
				
				value = geti16(sft->font, (uint_fast32_t) ((uint8_t *) match - sft->font->memory + 4));
				if (flags & CROSS_STREAM_KERNING) {
					kerning[1] += value;
				} else {
					kerning[0] += value;
				}
			}

		}

		offset += length;
		--numTables;
	}

	kerning[0] = kerning[0] / sft->font->unitsPerEm * sft->xScale;
	kerning[1] = kerning[1] / sft->font->unitsPerEm * sft->yScale;

	return 0;
}

int
sft_char(const struct SFT *sft, unsigned long charCode, struct SFT_Char *chr)
{
	double transform[6];
	double xScale, yScale, xOff, yOff;
	uint_fast32_t outline, glyph = 0;
	int advance, leftSideBearing;
	int x1, y1, x2, y2;

	memset(chr, 0, sizeof(*chr));

	if (charCode > UINT32_MAX)
		return -1;

	if (glyph_id(sft->font, (uint_fast32_t) charCode, &glyph) < 0)
		return -1;
	if (glyph == 0 && (sft->flags & SFT_CATCH_MISSING))
		return 1;

	/* Set up the initial transformation from
	 * glyph coordinate space to SFT coordinate space. */
	xScale = sft->xScale / sft->font->unitsPerEm;
	yScale = sft->yScale / sft->font->unitsPerEm;
	xOff = sft->x;
	yOff = sft->y;

	if (hor_metrics(sft->font, glyph, &advance, &leftSideBearing) < 0)
		return -1;
	/* We can compute the advance width early because the scaling factors
	 * won't be changed. This is neccessary for glyphs with completely
	 * empty outlines. */
	chr->advance = (int) round(advance * xScale);

	if (outline_offset(sft->font, glyph, &outline) < 0)
		return -1;
	/* A glyph may have a completely empty outline. */
	if (!outline)
		return 0;

	/* Read the bounding box from the font file verbatim. */
	if (sft->font->size < (uint_fast32_t) outline + 10)
		return -1;
	x1 = geti16(sft->font, outline + 2);
	y1 = geti16(sft->font, outline + 4);
	x2 = geti16(sft->font, outline + 6);
	y2 = geti16(sft->font, outline + 8);
	if (x2 <= x1 || y2 <= y1)
		return -1;

	/* Shift the transformation along the X axis such that
	 * x1 and leftSideBearing line up. Derivation:
	 *     lsb * xScale + xOff_1 = x1 * xScale + xOff_2
	 * <=> lsb * xScale + xOff_1 - x1 * xScale = xOff_2
	 * <=> (lsb - x1) * xScale + xOff_1 = xOff_2 */
	xOff += (leftSideBearing - x1) * xScale;

	/* Transform the bounding box into SFT coordinate space. */
	x1 = (int) floor(x1 * xScale + xOff);
	y1 = (int) floor(y1 * yScale + yOff);
	x2 = (int) ceil(x2 * xScale + xOff) + 1;
	y2 = (int) ceil(y2 * yScale + yOff) + 1;

	/* Compute the user-facing bounding box, respecting Y direction etc. */
	chr->x = x1;
	chr->y = sft->flags & SFT_DOWNWARD_Y ? -y2 : y1;
	chr->width = (unsigned int) (x2 - x1);
	chr->height = (unsigned int) (y2 - y1);

	/* Render the outline (if requested). */
	if (sft->flags & SFT_RENDER_IMAGE) {
		/* Set up the transformation matrix such that
		 * the transformed bounding boxes min corner lines
		 * up with the (0, 0) point. */
		transform[0] = xScale;
		transform[1] = 0.0;
		transform[2] = 0.0;
		transform[3] = yScale;
		transform[4] = xOff - x1;
		transform[5] = yOff - y1;
	
		if (render_image(sft, outline, transform, chr) < 0)
			return -1;
	}

	return glyph == 0;
}

/* This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW */
#define MUL_NO_OVERFLOW	((size_t)1 << (sizeof(size_t) * 4))

/* OpenBSD's reallocarray() standard libary function.
 * A wrapper for realloc() that takes two size args like calloc().
 * Useful because it eliminates common integer overflow bugs. */
static void *
sft_reallocarray(void *optr, size_t nmemb, size_t size)
{
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	    nmemb > 0 && SIZE_MAX / nmemb < size) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(optr, size * nmemb);
}

/* TODO maybe we should use long here instead of int. */
static inline int
fast_floor(double x)
{
	int i = (int) x;
	return i - (i > x);
}

static inline int
fast_ceil(double x)
{
	int i = (int) x;
	return i + (i < x);
}

#if defined(_WIN32)

static int
map_file(SFT_Font *font, const char *filename)
{
	HANDLE file;
	DWORD high, low;

	font->mapping = NULL;
	font->memory = NULL;

	file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return -1;
	}

	low = GetFileSize(file, &high);
	if (low == INVALID_FILE_SIZE) {
		CloseHandle(file);
		return -1;
	}

	font->size = (size_t)high << (8 * sizeof(DWORD)) | low;

	font->mapping = CreateFileMapping(file, NULL, PAGE_READONLY, high, low, NULL);
	if (font->mapping == NULL) {
		CloseHandle(file);
		return -1;
	}

	CloseHandle(file);

	font->memory = MapViewOfFile(font->mapping, FILE_MAP_READ, 0, 0, 0);
	if (font->memory == NULL) {
		CloseHandle(font->mapping);
		font->mapping = NULL;
		return -1;
	}

	return 0;
}

static void
unmap_file(SFT_Font *font)
{
	if (font->memory != NULL) {
		UnmapViewOfFile(font->memory);
		font->memory = NULL;
	}
	if (font->mapping != NULL) {
		CloseHandle(font->mapping);
		font->mapping = NULL;
	}
}

#else

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
	/* FIXME do some basic validation on info.st_size maybe - it is signed for example, so it *could* be negative .. */
	font->memory = mmap(NULL, (size_t) info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	font->size = (uint_fast32_t) info.st_size;
	close(fd);
	return font->memory == MAP_FAILED ? -1 : 0;
}

static void
unmap_file(SFT_Font *font)
{
	assert(font->memory != MAP_FAILED);
	munmap((void *) font->memory, font->size);
}

#endif

static struct point
midpoint(struct point a, struct point b)
{
	return (struct point) {
		0.5 * a.x + 0.5 * b.x,
		0.5 * a.y + 0.5 * b.y
	};
}

/* Applies an affine linear transformation matrix to a set of points. */
static void
transform_points(uint_fast16_t numPts, struct point *points, double trf[6])
{
	struct point *restrict pt;
	uint_fast16_t i;
	for (i = 0; i < numPts; ++i) {
		pt = &points[i];
		*pt = (struct point) {
			pt->x * trf[0] + pt->y * trf[2] + trf[4],
			pt->x * trf[1] + pt->y * trf[3] + trf[5]
		};
	}
}

static void
clip_points(uint_fast16_t numPts, struct point *points, unsigned int width, unsigned int height)
{
	struct point pt;
	uint_fast16_t i;

	for (i = 0; i < numPts; ++i) {
		pt = points[i];

		if (pt.x < 0.0) {
			points[i].x = 0.0;
		}
		if (pt.x >= width) {
			points[i].x = nextafter(width, 0.0);
		}
		if (pt.y < 0.0) {
			points[i].y = 0.0;
		}
		if (pt.y >= height) {
			points[i].y = nextafter(height, 0.0);
		}
	}
}

static int
init_buffer(struct buffer *buf, unsigned int width, unsigned int height)
{
	struct cell *ptr;
	size_t rowsSize, cellsSize;
	unsigned int i;

	buf->rows = NULL;
	buf->width = width;
	buf->height = height;

	rowsSize = (size_t) height * sizeof(buf->rows[0]);
	cellsSize = (size_t) width * height * sizeof(struct cell);
	if ((buf->rows = calloc(rowsSize + cellsSize, 1)) == NULL)
		return -1;

	ptr = (void *) (buf->rows + height);
	for (i = 0; i < height; ++i) {
		buf->rows[i] = ptr;
		ptr += width;
	}

	return 0;
}

static void
free_buffer(struct buffer *buf)
{
	free(buf->rows);
}

static void
flip_buffer(struct buffer *buf)
{
	struct cell *row;
	unsigned int front = 0, back = buf->height - 1;
	while (front < back) {
		row = buf->rows[front];
		buf->rows[front] = buf->rows[back];
		buf->rows[back] = row;
		++front, --back;
	}
}

static int
init_outline(struct outline *outl)
{
	outl->numPoints = 0;
	outl->capPoints = 64;
	if ((outl->points = malloc(outl->capPoints * sizeof(outl->points[0]))) == NULL)
		return -1;
	outl->numCurves = 0;
	outl->capCurves = 64;
	if ((outl->curves = malloc(outl->capCurves * sizeof(outl->curves[0]))) == NULL)
		return -1;
	outl->numLines = 0;
	outl->capLines = 64;
	if ((outl->lines = malloc(outl->capLines * sizeof(outl->lines[0]))) == NULL)
		return -1;
	return 0;
}

static void
free_outline(struct outline *outl)
{
	free(outl->points);
	free(outl->curves);
	free(outl->lines);
}

static int
grow_points(struct outline *outl)
{
	void *mem;
	uint_fast16_t cap;
	assert(outl->capPoints);
	/* Since we use uint_fast16_t for capacities, we have to be extra careful not to trigger integer overflow. */
	if (outl->capPoints > UINT16_MAX / 2)
		return -1;
	cap = (uint_fast16_t) (2U * outl->capPoints);
	if ((mem = sft_reallocarray(outl->points, cap, sizeof(outl->points[0]))) == NULL)
		return -1;
	outl->capPoints = (uint_least16_t) cap;
	outl->points = mem;
	return 0;
}

static int
grow_curves(struct outline *outl)
{
	void *mem;
	uint_fast16_t cap;
	assert(outl->capCurves);
	if (outl->capCurves > UINT16_MAX / 2)
		return -1;
	cap = (uint_fast16_t) (2U * outl->capCurves);
	if ((mem = sft_reallocarray(outl->curves, cap, sizeof(outl->curves[0]))) == NULL)
		return -1;
	outl->capCurves = (uint_least16_t) cap;
	outl->curves = mem;
	return 0;
}

static int
grow_lines(struct outline *outl)
{
	void *mem;
	uint_fast16_t cap;
	assert(outl->capLines);
	if (outl->capLines > UINT16_MAX / 2)
		return -1;
	cap = (uint_fast16_t) (2U * outl->capLines);
	if ((mem = sft_reallocarray(outl->lines, cap, sizeof(outl->lines[0]))) == NULL)
		return -1;
	outl->capLines = (uint_least16_t) cap;
	outl->lines = mem;
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
getu8(SFT_Font *font, uint_fast32_t offset)
{
	assert(offset + 1 <= font->size);
	return *(font->memory + offset);
}

static inline int8_t
geti8(SFT_Font *font, uint_fast32_t offset)
{
	return (int8_t) getu8(font, offset);
}

static inline uint16_t
getu16(SFT_Font *font, uint_fast32_t offset)
{
	assert(offset + 2 <= font->size);
	const uint8_t *base = font->memory + offset;
	uint16_t b1 = base[0], b0 = base[1]; 
	return (uint16_t) (b1 << 8 | b0);
}

static inline int16_t
geti16(SFT_Font *font, uint_fast32_t offset)
{
	return (int16_t) getu16(font, offset);
}

static inline uint32_t
getu32(SFT_Font *font, uint_fast32_t offset)
{
	assert(offset + 4 <= font->size);
	const uint8_t *base = font->memory + offset;
	uint32_t b3 = base[0], b2 = base[1], b1 = base[2], b0 = base[3]; 
	return (uint32_t) (b3 << 24 | b2 << 16 | b1 << 8 | b0);
}

static int
gettable(SFT_Font *font, char tag[4], uint_fast32_t *offset)
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
	*offset = getu32(font, (uint_fast32_t) ((uint8_t *) match - font->memory + 8));
	return 0;
}

static int
cmap_fmt4(SFT_Font *font, uint_fast32_t table, uint_fast32_t charCode, uint_fast32_t *glyph)
{
	uintptr_t segIdxX2;
	uint_fast32_t endCodes, startCodes, idDeltas, idRangeOffsets, idOffset;
	uint_fast16_t segCountX2, idRangeOffset, startCode, shortCode, idDelta, id;
	uint8_t key[2] = { (uint8_t) (charCode >> 8), (uint8_t) charCode };
	/* cmap format 4 only supports the Unicode BMP. */
	if (charCode > 0xFFFF) {
		*glyph = 0;
		return 0;
	}
	shortCode = (uint_fast16_t) charCode;
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
	/* Find the segment that contains shortCode by binary searching over the highest codes in the segments. */
	segIdxX2 = (uintptr_t) csearch(key, font->memory + endCodes,
		segCountX2 / 2, 2, cmpu16) - (uintptr_t) (font->memory + endCodes);
	/* Look up segment info from the arrays & short circuit if the spec requires. */
	if ((startCode = getu16(font, startCodes + segIdxX2)) > shortCode)
		return 0;
	idDelta = getu16(font, idDeltas + segIdxX2);
	if (!(idRangeOffset = getu16(font, idRangeOffsets + segIdxX2))) {
		/* Intentional integer under- and overflow. */
		*glyph = (shortCode + idDelta) & 0xFFFF;
		return 0;
	}
	/* Calculate offset into glyph array and determine ultimate value. */
	idOffset = idRangeOffsets + segIdxX2 + idRangeOffset + 2U * (unsigned int) (shortCode - startCode);
	if (font->size < idOffset + 2)
		return -1;
	id = getu16(font, idOffset);
	/* Intentional integer under- and overflow. */
	*glyph = id ? (id + idDelta) & 0xFFFF : 0;
	return 0;
}

static int
cmap_fmt6(SFT_Font *font, uint_fast32_t table, uint_fast32_t charCode, uint_fast32_t *glyph)
{
	unsigned int firstCode, entryCount;
	/* cmap format 6 only supports the Unicode BMP. */
	if (charCode > 0xFFFF) {
		*glyph = 0;
		return 0;
	}
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
	*glyph = getu16(font, table + 4 + 2 * charCode);
	return 0;
}

/* Maps Unicode code points to glyph indices. */
static int
glyph_id(SFT_Font *font, uint_fast32_t charCode, uint_fast32_t *glyph)
{
	uint_fast32_t cmap, entry, table;
	unsigned int idx, numEntries;
	int type;

	if (gettable(font, "cmap", &cmap) < 0)
		return -1;

	if (font->size < cmap + 4)
		return -1;
	numEntries = getu16(font, cmap + 2);
	
	if (font->size < cmap + 4 + numEntries * 8)
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
				return cmap_fmt4(font, table + 6, charCode, glyph);
			case 6:
				return cmap_fmt6(font, table + 6, charCode, glyph);
			default:
				return -1;
			}
		}
	}

	return -1;
}

static int
hor_metrics(SFT_Font *font, uint_fast32_t glyph, int *advanceWidth, int *leftSideBearing)
{
	uint_fast32_t hmtx, offset, boundary;
	if (gettable(font, "hmtx", &hmtx) < 0)
		return -1;
	if (glyph < font->numLongHmtx) {
		/* glyph is inside long metrics segment. */
		offset = hmtx + 4 * glyph;
		if (font->size < offset + 4)
			return -1;
		*advanceWidth = getu16(font, offset);
		*leftSideBearing = geti16(font, offset + 2);
		return 0;
	} else {
		/* glyph is inside short metrics segment. */
		boundary = hmtx + 4U * font->numLongHmtx;
		if (boundary < 4)
			return -1;
		
		offset = boundary - 4;
		if (font->size < offset + 4)
			return -1;
		*advanceWidth = getu16(font, offset);
		
		offset = boundary + 2 * (glyph - font->numLongHmtx);
		if (font->size < offset + 2)
			return -1;
		*leftSideBearing = geti16(font, offset);
		return 0;
	}
}

/* Returns the offset into the font that the glyph's outline is stored at. */
static int
outline_offset(SFT_Font *font, uint_fast32_t glyph, uint_fast32_t *offset)
{
	uint_fast32_t loca, glyf;
	uint_fast32_t base, this, next;

	if (gettable(font, "loca", &loca) < 0)
		return -1;
	if (gettable(font, "glyf", &glyf) < 0)
		return -1;

	if (font->locaFormat == 0) {
		base = loca + 2 * glyph;

		if (font->size < base + 4)
			return -1;
		
		this = 2U * getu16(font, base);
		next = 2U * getu16(font, base + 2);
	} else {
		base = loca + 4 * glyph;

		if (font->size < base + 8)
			return -1;

		this = getu32(font, base);
		next = getu32(font, base + 4);
	}

	*offset = this == next ? 0 : glyf + this;
	return 0;
}

/* For a 'simple' outline, determines each point of the outline with a set of flags. */
static int
simple_flags(SFT_Font *font, uint_fast32_t *offset, uint_fast16_t numPts, uint8_t *flags)
{
	uint_fast32_t off = *offset;
	uint_fast16_t i;
	uint8_t value = 0, repeat = 0;
	for (i = 0; i < numPts; ++i) {
		if (repeat) {
			--repeat;
		} else {
			if (font->size < off + 1)
				return -1;
			value = getu8(font, off++);
			if (value & REPEAT_FLAG) {
				if (font->size < off + 1)
					return -1;
				repeat = getu8(font, off++);
			}
		}
		flags[i] = value;
	}
	*offset = off;
	return 0;
}

/* For a 'simple' outline, decodes both X and Y coordinates for each point of the outline. */
static int
simple_points(SFT_Font *font, uint_fast32_t offset, uint_fast16_t numPts, uint8_t *flags, struct point *points)
{
	long accum, value, bit;
	uint_fast16_t i;

	assert(numPts > 0);

	accum = 0L;
	for (i = 0; i < numPts; ++i) {
		if (flags[i] & X_CHANGE_IS_SMALL) {
			if (font->size < offset + 1)
				return -1;
			value = (long) getu8(font, offset++);
			bit = !!(flags[i] & X_CHANGE_IS_POSITIVE);
			accum -= (value ^ -bit) + bit;
		} else if (!(flags[i] & X_CHANGE_IS_ZERO)) {
			if (font->size < offset + 2)
				return -1;
			accum += geti16(font, offset);
			offset += 2;
		}
		points[i].x = (double) accum;
	}

	accum = 0L;
	for (i = 0; i < numPts; ++i) {
		if (flags[i] & Y_CHANGE_IS_SMALL) {
			if (font->size < offset + 1)
				return -1;
			value = (long) getu8(font, offset++);
			bit = !!(flags[i] & Y_CHANGE_IS_POSITIVE);
			accum -= (value ^ -bit) + bit;
		} else if (!(flags[i] & Y_CHANGE_IS_ZERO)) {
			if (font->size < offset + 2)
				return -1;
			accum += geti16(font, offset);
			offset += 2;
		}
		points[i].y = (double) accum;
	}

	return 0;
}

static int
decode_contour(uint8_t *flags, uint_fast16_t basePoint, uint_fast16_t count, struct outline *outl)
{
	uint_fast16_t looseEnd, beg, ctrl, center, i;
	unsigned int gotCtrl;

	/* Skip contours with less than two points, since the following algorithm can't handle them and
	 * they should appear invisible either way (because they don't have any area). */
	if (count < 2) return 0;

	if (flags[0] & POINT_IS_ON_CURVE) {
		looseEnd = basePoint++;
		++flags;
		--count;
	} else if (flags[count - 1] & POINT_IS_ON_CURVE) {
		looseEnd = basePoint + --count;
	} else {
		if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
			return -1;

		looseEnd = outl->numPoints;
		outl->points[outl->numPoints++] = midpoint(
			outl->points[basePoint],
			outl->points[basePoint + count - 1]);
	}
	beg = looseEnd;
	gotCtrl = 0;
	for (i = 0; i < count; ++i) {
		if (flags[i] & POINT_IS_ON_CURVE) {
			if (gotCtrl) {
				if (outl->numCurves >= outl->capCurves && grow_curves(outl) < 0)
					return -1;
				outl->curves[outl->numCurves++] = (struct curve) { beg, basePoint + i, ctrl };
			} else {
				if (outl->numLines >= outl->capLines && grow_lines(outl) < 0)
					return -1;
				outl->lines[outl->numLines++] = (struct line) { beg, basePoint + i };
			}
			beg = basePoint + i;
			gotCtrl = 0;
		} else {
			if (gotCtrl) {
				center = outl->numPoints;
				if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
					return -1;
				outl->points[center] = midpoint(outl->points[ctrl], outl->points[basePoint + i]);
				++outl->numPoints;

				if (outl->numCurves >= outl->capCurves && grow_curves(outl) < 0)
					return -1;
				outl->curves[outl->numCurves++] = (struct curve) { beg, center, ctrl };

				beg = center;
			}
			ctrl = basePoint + i;
			gotCtrl = 1;
		}
	}
	if (gotCtrl) {
		if (outl->numCurves >= outl->capCurves && grow_curves(outl) < 0)
			return -1;
		outl->curves[outl->numCurves++] = (struct curve) { beg, looseEnd, ctrl };
	} else {
		if (outl->numLines >= outl->capLines && grow_lines(outl) < 0)
			return -1;
		outl->lines[outl->numLines++] = (struct line) { beg, looseEnd };
	}

	return 0;
}

static int
simple_outline(SFT_Font *font, uint_fast32_t offset, unsigned int numContours, struct outline *outl)
{
	uint_fast16_t *endPts = NULL;
	uint8_t *flags = NULL;
	uint_fast16_t numPts;
	unsigned int i;

	uint_fast16_t basePoint = outl->numPoints;

	if (font->size < offset + numContours * 2 + 2)
		goto failure;
	numPts = getu16(font, offset + (numContours - 1) * 2);
	if (numPts == 0xFFFF)
		goto failure;
	numPts++;
	if (outl->numPoints > UINT16_MAX - numPts)
		goto failure;

	while (outl->capPoints < basePoint + numPts) {
		if (grow_points(outl) < 0)
			return -1;
	}
	
	STACK_ALLOC(endPts, uint_fast16_t, 16, numContours);
	if (endPts == NULL)
		goto failure;
	STACK_ALLOC(flags, uint8_t, 128, numPts);
	if (flags == NULL)
		goto failure;

	for (i = 0; i < numContours; ++i) {
		endPts[i] = getu16(font, offset);
		offset += 2;
	}
	/* Ensure that endPts are never falling.
	 * Falling endPts have no sensible interpretation and most likely only occur in malicious input.
	 * Therefore, we bail, should we ever encounter such input. */
	for (i = 0; i < numContours - 1; ++i) {
		if (endPts[i + 1] < endPts[i] + 1)
			goto failure;
	}
	offset += 2U + getu16(font, offset);

	if (simple_flags(font, &offset, numPts, flags) < 0)
		goto failure;
	if (simple_points(font, offset, numPts, flags, outl->points + basePoint) < 0)
		goto failure;
	outl->numPoints += numPts;
	
	uint8_t *flagsPtr = flags;
	uint_fast16_t contourBase = 0;
	for (i = 0; i < numContours; ++i) {
		uint_fast16_t count = endPts[i] - contourBase + 1;
		if (decode_contour(flagsPtr, basePoint, count, outl) < 0)
			goto failure;
		flagsPtr += count;
		basePoint += count;
		contourBase += count;
	}

	STACK_FREE(endPts);
	STACK_FREE(flags);
	return 0;
failure:
	STACK_FREE(endPts);
	STACK_FREE(flags);
	return -1;
}

static int
compound_outline(SFT_Font *font, uint_fast32_t offset, int recDepth, struct outline *outl)
{
	double local[6];
	uint_fast32_t outline;
	unsigned int flags, glyph;
	/* Guard against infinite recursion (compound glyphs that have themselves as component). */
	if (recDepth >= 4)
		return -1;
	do {
		memset(local, 0, sizeof(local));
		if (font->size < offset + 4)
			return -1;
		flags = getu16(font, offset);
		glyph = getu16(font, offset + 2);
		offset += 4;
		/* We don't implement point matching, and neither does stb_truetype for that matter. */
		if (!(flags & ACTUAL_XY_OFFSETS))
			return -1;
		/* Read additional X and Y offsets (in FUnits) of this component. */
		if (flags & OFFSETS_ARE_LARGE) {
			if (font->size < offset + 4)
				return -1;
			local[4] = geti16(font, offset);
			local[5] = geti16(font, offset + 2);
			offset += 4;
		} else {
			if (font->size < offset + 2)
				return -1;
			local[4] = geti8(font, offset);
			local[5] = geti8(font, offset + 1);
			offset += 2;
		}
		if (flags & GOT_A_SINGLE_SCALE) {
			if (font->size < offset + 2)
				return -1;
			local[0] = geti16(font, offset) / 16384.0;
			local[3] = local[0];
			offset += 2;
		} else if (flags & GOT_AN_X_AND_Y_SCALE) {
			if (font->size < offset + 4)
				return -1;
			local[0] = geti16(font, offset + 0) / 16384.0;
			local[3] = geti16(font, offset + 2) / 16384.0;
			offset += 4;
		} else if (flags & GOT_A_SCALE_MATRIX) {
			if (font->size < offset + 8)
				return -1;
			local[0] = geti16(font, offset + 0) / 16384.0;
			local[1] = geti16(font, offset + 2) / 16384.0;
			local[2] = geti16(font, offset + 4) / 16384.0;
			local[3] = geti16(font, offset + 6) / 16384.0;
			offset += 8;
		} else {
			local[0] = 1.0;
			local[3] = 1.0;
		}
		/* At this point, Apple's spec more or less tells you to scale the matrix by its own L1 norm.
		 * But stb_truetype scales by the L2 norm. And FreeType2 doesn't scale at all.
		 * Furthermore, Microsoft's spec doesn't even mention anything like this.
		 * It's almost as if nobody ever uses this feature anyway. */
		if (outline_offset(font, glyph, &outline) < 0)
			return -1;
		if (outline) {
			unsigned int basePoint = outl->numPoints;
			if (decode_outline(font, outline, recDepth + 1, outl) < 0)
				return -1;
			transform_points(outl->numPoints - basePoint, outl->points + basePoint, local);
		}
	} while (flags & THERE_ARE_MORE_COMPONENTS);

	return 0;
}

static int
decode_outline(SFT_Font *font, uint_fast32_t offset, int recDepth, struct outline *outl)
{
	int numContours;
	if (font->size < offset + 10)
		return -1;
	numContours = geti16(font, offset);
	if (numContours >= 0) {
		/* Glyph has a 'simple' outline consisting of a number of contours. */
		return simple_outline(font, offset + 10, (unsigned int) numContours, outl);
	} else {
		/* Glyph has a compound outline combined from mutiple other outlines. */
		return compound_outline(font, offset + 10, recDepth, outl);
	}
}

/* A heuristic to tell whether a given curve can be approximated closely enough by a line. */
static int
is_flat(struct outline *outl, struct curve curve, double flatness)
{
	struct point beg = outl->points[curve.beg];
	struct point end = outl->points[curve.end];
	struct point ctrl = outl->points[curve.ctrl];
	struct point mid = midpoint(beg, end);
	double x = ctrl.x - mid.x;
	double y = ctrl.y - mid.y;
	return x * x + y * y <= flatness * flatness;
}

static int
tesselate_curve(struct curve curve, struct outline *outl)
{
	/* From my tests I can conclude that this stack barely reaches a top height
	 * of 4 elements even for the largest font sizes I'm willing to support. And
	 * as space requirements should only grow logarithmically, I think 10 is
	 * more than enough. */
#define STACK_SIZE 10
	struct curve stack[STACK_SIZE];
	unsigned int top = 0;
	for (;;) {
		if (is_flat(outl, curve, 0.5) || top >= STACK_SIZE) {
			if (outl->numLines >= outl->capLines && grow_lines(outl) < 0)
				return -1;
			outl->lines[outl->numLines++] = (struct line) { curve.beg, curve.end };
			if (top == 0) break;
			curve = stack[--top];
		} else {
			uint_fast16_t ctrl0 = outl->numPoints;
			if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
				return -1;
			outl->points[ctrl0] = midpoint(outl->points[curve.beg], outl->points[curve.ctrl]);
			++outl->numPoints;

			uint_fast16_t ctrl1 = outl->numPoints;
			if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
				return -1;
			outl->points[ctrl1] = midpoint(outl->points[curve.ctrl], outl->points[curve.end]);
			++outl->numPoints;

			uint_fast16_t pivot = outl->numPoints;
			if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
				return -1;
			outl->points[pivot] = midpoint(outl->points[ctrl0], outl->points[ctrl1]);
			++outl->numPoints;

			stack[top++] = (struct curve) { curve.beg, pivot, ctrl0 };
			curve = (struct curve) { pivot, curve.end, ctrl1 };
		}
	}
	return 0;
#undef STACK_SIZE
}

static int
tesselate_curves(struct outline *outl)
{
	unsigned int i;
	for (i = 0; i < outl->numCurves; ++i) {
		if (tesselate_curve(outl->curves[i], outl) < 0)
			return -1;
	}
	return 0;
}

static void
draw_dot(struct buffer buf, int px, int py, double xAvg, double yDiff)
{
	struct cell *restrict ptr = &buf.rows[py][px];
	struct cell cell = *ptr;
	cell.cover += yDiff;
	cell.area += (1.0 - xAvg) * yDiff;
	*ptr = cell;
}

/* Draws a line into the buffer. Uses a custom 2D raycasting algorithm to do so. */
static void
draw_line(struct buffer buf, struct point origin, struct point goal)
{
	double originX, originY;
	double goalX, goalY;
	double deltaX, deltaY;
	double nextCrossingX, nextCrossingY;
	double crossingGapX, crossingGapY;
	double prevDistance = 0.0;
	int pixelX, pixelY;
	int iter, numIters = 0;

	originX = origin.x;
	goalX = goal.x;
	deltaX = goalX - originX;
	if (deltaX > 0.0) {
		crossingGapX = 1.0 / deltaX;
		pixelX = fast_floor(originX);
		nextCrossingX = (1.0 - (originX - pixelX)) * crossingGapX;
		numIters += fast_ceil(goalX) - fast_floor(originX) - 1;
	} else if (deltaX < 0.0) {
		crossingGapX = -(1.0 / deltaX);
		pixelX = fast_ceil(originX) - 1;
		nextCrossingX = (originX - pixelX) * crossingGapX;
		numIters += fast_ceil(originX) - fast_floor(goalX) - 1;
	} else {
		crossingGapX = 0.0;
		pixelX = fast_floor(originX);
		nextCrossingX = 100.0;
	}

	originY = origin.y;
	goalY = goal.y;
	deltaY = goalY - originY;
	if (deltaY > 0.0) {
		crossingGapY = 1.0 / deltaY;
		pixelY = fast_floor(originY);
		nextCrossingY = (1.0 - (originY - pixelY)) * crossingGapY;
		numIters += fast_ceil(goalY) - fast_floor(originY) - 1;
	} else if (deltaY < 0.0) {
		crossingGapY = -(1.0 / deltaY);
		pixelY = fast_ceil(originY) - 1;
		nextCrossingY = (originY - pixelY) * crossingGapY;
		numIters += fast_ceil(originY) - fast_floor(goalY) - 1;
	} else {
		return;
	}

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
	double averageX = (goalX - pixelX) - 0.5 * deltaX * deltaDistance;
	draw_dot(buf, pixelX, pixelY, averageX, deltaY * deltaDistance);
}

static void
draw_lines(struct outline *outl, struct buffer buf)
{
	unsigned int i;
	for (i = 0; i < outl->numLines; ++i) {
		struct line line = outl->lines[i];
		struct point origin = outl->points[line.beg];
		struct point goal = outl->points[line.end];
		if (origin.y != goal.y) {
			draw_line(buf, origin, goal);
		}
	}
}

/* Integrate the values in the buffer to arrive at the final grayscale image. */
static void
post_process(struct buffer buf, uint8_t *image)
{
	struct cell *restrict in, cell;
	uint8_t *restrict out;
	double accum, value;
	unsigned int x, y;
	out = image;
	for (y = 0; y < buf.height; ++y) {
		accum = 0.0;
		in = buf.rows[y];
		for (x = 0; x < buf.width; ++x) {
			cell = *in++;
			value = fabs(accum + cell.area);
			value = MIN(value, 1.0);
			value = value * 255.0 + 0.5;
			*out++ = (uint8_t) value;
			accum += cell.cover;
		}
	}
}

static int
render_image(const struct SFT *sft, uint_fast32_t offset, double transform[6], struct SFT_Char *chr)
{
	struct outline outl;
	struct buffer buf;
	int err = 0;

	memset(&outl, 0, sizeof(outl));
	memset(&buf, 0, sizeof(buf));

	err = err || init_outline(&outl) < 0;
	err = err || decode_outline(sft->font, offset, 0, &outl) < 0;
	if (!err) transform_points(outl.numPoints, outl.points, transform);
	if (!err) clip_points(outl.numPoints, outl.points, chr->width, chr->height);
	err = err || tesselate_curves(&outl) < 0;

	err = err || init_buffer(&buf, chr->width, chr->height) < 0;
	if (!err) draw_lines(&outl, buf);
	free_outline(&outl);
	if (!err && sft->flags & SFT_DOWNWARD_Y)
		flip_buffer(&buf);

	err = err || (chr->image = calloc(chr->width * chr->height, 1)) == NULL;
	if (!err) post_process(buf, chr->image);

	free_buffer(&buf);

	return err ? -1 : 0;
}
