/* A simple command line application that shows how to use libschrift with X11 via XRender. */
/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <schrift.h>

#include "util/utf8_to_utf32.h"
#include "util/arg.h"

#define APP_NAME "sftdemo"
#define MAX_LINES 40
#define LINE_LEN 200

char *argv0;

static char lines[MAX_LINES][LINE_LEN];
static int numlines;

static XRenderColor fgcolor, bgcolor;
static Display *dpy;
static int screen;
static Atom wmDeleteWindow;
static Window win;
static Pixmap fgpix;
static Picture pic, fgpic;
static GlyphSet glyphset;
static struct SFT sft;
static XRenderPictFormat *format;

static unsigned int **bitfield;

#define TOTAL_CODEPOINTS 0x110000
#define WORDS_IN_PAGE (1 << 9)
#define BITS_IN_WORD (8U * sizeof(unsigned int))
#define BITS_IN_PAGE (WORDS_IN_PAGE * BITS_IN_WORD)
#define TOTAL_PAGES ((TOTAL_CODEPOINTS - 1 + BITS_IN_PAGE) / BITS_IN_PAGE)

static void
bitfield_init(void)
{
	bitfield = calloc(TOTAL_PAGES, sizeof(*bitfield));
}

static unsigned int
bitfield_setbit(unsigned long codepoint)
{
	unsigned int value;
	unsigned long page, word, bit;

	if (codepoint >= TOTAL_CODEPOINTS) return 1;

	page = codepoint / BITS_IN_PAGE;
	codepoint %= BITS_IN_PAGE;
	word = codepoint / BITS_IN_WORD;
	codepoint %= BITS_IN_WORD;
	bit = codepoint;

	if (!bitfield[page]) {
		bitfield[page] = calloc(WORDS_IN_PAGE, sizeof(unsigned int));
	}

	value = bitfield[page][word];
	bitfield[page][word] = value | (1U << bit);
	return (value >> bit) & 1U;
}

static void
bitfield_free(void)
{
	unsigned long i;
	for (i = 0; i < TOTAL_PAGES; ++i) {
		if (bitfield[i]) {
			free(bitfield[i]);
		}
	}
	free(bitfield);
}

static void
die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: %s [-v] [-f font file] [-s size in px] [-P] [text file]\n", argv0);
}

static void
loadglyph(struct SFT *sft, unsigned int charCode)
{
	struct SFT_Char chr;
	XGlyphInfo info;
	Glyph glyph;
	unsigned int stride, i;

	if (sft_char(sft, charCode, &chr) < 0) {
		printf("Couldn't load character '%c' (0x%02X).\n", charCode, charCode);
		return;
	}

	glyph = charCode;
	info.x = (short) -chr.x;
	info.y = (short) -chr.y;
	info.width = (unsigned short) chr.width;
	info.height = (unsigned short) chr.height;
	info.xOff = (short) round(chr.advance);
	info.yOff = 0;

	stride = (chr.width + 3) & ~3U;
	char bitmap[stride * chr.height];
	memset(bitmap, 0, stride * chr.height);
	for (i = 0; i < chr.height; ++i)
		memcpy(bitmap + i * stride, (char *) chr.image + i * chr.width, chr.width);
	free(chr.image);

	XRenderAddGlyphs(dpy, glyphset, &glyph, &info, 1, bitmap, (int) (stride * chr.height));
}

static void
teardown(void)
{
	bitfield_free();
	sft_freefont(sft.font);
	XCloseDisplay(dpy);
	exit(0);
}

static void
drawtext(int x, int y, const char *text)
{
	uint32_t codepoints[256];
	int length = utf8_to_utf32((const uint8_t *) text, codepoints, 256);

	/* Strip non-printable characters. */
	int w = 0;
	for (int r = 0; r < length; ++r) {
		if (codepoints[r] >= 0x20)
			codepoints[w++] = codepoints[r];
	}
	length = w;

	for (int i = 0; i < length; ++i) {
		if (!bitfield_setbit(codepoints[i])) {
			loadglyph(&sft, codepoints[i]);
		}
	}

	XRenderCompositeString32(dpy, PictOpOver,
		fgpic, pic, NULL,
		glyphset, 0, 0, x, y, codepoints, length);
}

static void
draw(unsigned int width, unsigned int height)
{
	XRenderFillRectangle(dpy, PictOpOver,
		pic, &bgcolor, 0, 0, width, height);

	double ascent, descent, gap;
	/* TODO check return value! */
	sft_linemetrics(&sft, &ascent, &descent, &gap);

	double y = ascent + gap;
	for (int i = 0; i < numlines; ++i) {
		drawtext(20, (int) round(y), lines[i]);
		y += (ascent + descent + gap) * 1.5;
	}
}

static void
handleevent(XEvent *ev)
{
	switch (ev->type) {
	case Expose:
		draw((unsigned int) ev->xexpose.width, (unsigned int) ev->xexpose.height);
		break;
	case ClientMessage:
		if ((Atom) ev->xclient.data.l[0] == wmDeleteWindow)
			teardown();
		break;
	}
}

static void
setupx(void)
{
	XRenderPictFormat *format;
	XRenderPictureAttributes attr;

	if (!(dpy = XOpenDisplay(NULL)))
		die("Can't open X display\n");
	screen = DefaultScreen(dpy);

	/* TODO We probably should check here that the X11 server actually supports XRender. */

	win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 600, 400, 0,
	                    DefaultDepth(dpy, screen), InputOutput,
	                    CopyFromParent, 0, NULL);
	XStoreName(dpy, win, APP_NAME);
	XSelectInput(dpy, win, ExposureMask);
	wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmDeleteWindow, 1);
	XMapRaised(dpy, win);

	format = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, screen));
	pic = XRenderCreatePicture(dpy, win, format, 0, NULL);

	fgpix = XCreatePixmap(dpy, win, 1, 1, 24);
	format = XRenderFindStandardFormat(dpy, PictStandardRGB24);
	attr.repeat = True;
	fgpic = XRenderCreatePicture(dpy, fgpix, format, CPRepeat, &attr);
	XRenderFillRectangle(dpy, PictOpSrc, fgpic, &fgcolor, 0, 0, 1, 1);
}

static void
runx(void)
{
	XEvent ev;

	while (!XNextEvent(dpy, &ev))
		handleevent(&ev);
}

int
main(int argc, char *argv[])
{
	const char *filename, *textfile;
	double size;

	filename = "resources/Ubuntu-R.ttf";
	textfile = "resources/glass.utf8";
	size = 16.0;
	bgcolor = (XRenderColor) { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
	fgcolor = (XRenderColor) { 0x0000, 0x0000, 0x0000, 0xFFFF };

	ARGBEGIN {
	case 'f':
		filename = EARGF(usage());
		break;
	case 's':
		size = atof(EARGF(usage()));
		break;
	case 'v':
		printf("libschrift v%s\n", sft_version());
		exit(0);
	default:
		usage();
		exit(1);
	} ARGEND
	if (argc) {
		textfile = *argv;
		--argc, ++argv;
	}
	if (argc) {
		usage();
		exit(1);
	}

	FILE *file = fopen(textfile, "r");
	if (file == NULL)
		die("Can't open text file.");
	while (numlines < MAX_LINES && fgets(lines[numlines++], LINE_LEN, file) != NULL) {}
	fclose(file);

	setupx();

	format = XRenderFindStandardFormat(dpy, PictStandardA8);
	glyphset = XRenderCreateGlyphSet(dpy, format);

	if ((sft.font = sft_loadfile(filename)) == NULL)
		die("Can't load font file.");
	sft.xScale = size;
	sft.yScale = size;
	sft.flags = SFT_DOWNWARD_Y | SFT_RENDER_IMAGE;

	bitfield_init();

	runx();
	return 0;
}

