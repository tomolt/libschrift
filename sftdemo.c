/* A simple command line application that shows how to use libschrift with X11 via XRender. */
/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* This demo uses the Xlib API to open windows on X11. */
#include <X11/Xlib.h>
/* We also need access to the XRender extension for displaying text on the screen. */
#include <X11/extensions/Xrender.h>

#include <schrift.h>

/* utf8_to_utf32.h is a single-header library for decoding UTF-8 into raw Unicode codepoints.
 * libschrift only deals with codepoints and does not do any decoding itself, so we have to
 * do that step here in the application. */
#include "util/utf8_to_utf32.h"
/* arg.h is a single-header library for parsing command line arguments.
 * It is not needed for applications using libschrift. */
#include "util/arg.h"

#define APP_NAME "libschrift X11 demo"
#define MAX_LINES 40
#define LINE_LEN 200

/* We need to define this global variable so we can use arg.h. */
char *argv0;

static char lines[MAX_LINES][LINE_LEN];
static int numlines;

/* We need the following variables to open an X11 window. */
static Display *dpy;
static int screen;
static Window win;
static Atom wmDeleteWindow;

/* We need these variables for displaying text with XRender. */
static Pixmap fgpix;
static Picture pic, fgpic;
static GlyphSet glyphset;
static XRenderColor fgcolor, bgcolor;
static XRenderPictFormat *format;

/* This is the only persistent state that we need for libschrift. */

/* A struct SFT is a kind of 'drawing context' for libschrift.
 * It bundles commonly needed parameters such as font and size and has to
 * be filled out by the application. Any fields that the application does
 * not need to set must be initialized with zero.
 * (Global variables are always zero-initialized in C.) */
static struct SFT sft;

/* When using XRender we have to manually keep track of which glyphs we already rendered
 * and uploaded to the X11 server.
 * We do this with a sparse / page-based bitfield.
 * Each bit corresponds to one codepoint.
 * If it is set, then we have already rendered and uploaded that glyph. */

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

/* If we encounter a problem, we call this function to print an error message and abort the program.
 * Proper applications might want to do more sophisticated error handling. */
static void
die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

/* Print the ways in which this program may be called. */
static void
usage(void)
{
	fprintf(stderr,
		"usage: %s [-v] [-f font file] [-s size in px] [-P] [text file]\n", argv0);
}

/* Render a character with libschrift and subsequently upload it to the X11 server. */
static void
loadglyph(struct SFT *sft, unsigned long codepoint)
{
	struct SFT_Char chr;
	XGlyphInfo info;
	Glyph glyph;
	unsigned long gid;
	unsigned int stride, i;
	void *image;

	/* Render the character with libschrift. If successfull, we get a struct SFT_Char back
	 * which contains various useful pieces of information about the character as well as
	 * a rendered image of the character. */
	if (sft_codepoint_to_glyph(sft, codepoint, &gid) < 0) {
		printf("Couldn't load codepoint 0x%02lX.\n", codepoint);
		return;
	}
	if (sft_glyph_dimensions(sft, gid, &chr) < 0) {
		printf("Couldn't load codepoint 0x%02lX.\n", codepoint);
		return;
	}
	if (sft_render_glyph(sft, gid, &chr, &image) < 0) {
		printf("Couldn't load codepoint 0x%02lX.\n", codepoint);
		return;
	}

	/* XRender expects every row of the glyph image to be aligned to a multiple of four.
	 * That means we have to copy the image into a separate buffer row by row,
	 * padding the end of each row with a couple of extra bytes.
	 * The stride is simply the number of bytes / pixels per row including the padding. */
	stride = (chr.width + 3) & ~3U;
	char paddedImage[stride * chr.height];
	memset(paddedImage, 0, stride * chr.height);
	for (i = 0; i < chr.height; ++i)
		memcpy(paddedImage + i * stride, (char *) image + i * chr.width, chr.width);
	free(image);

	/* Fill in the XRender XGlyphInfo struct with the info we get in the SFT_Char struct. */
	glyph = codepoint;
	info.x = (short) -chr.x;
	info.y = (short) -chr.y;
	info.width = (unsigned short) chr.width;
	info.height = (unsigned short) chr.height;
	info.xOff = (short) round(chr.advance);
	info.yOff = 0;
	/* Upload the XGlyphInfo and padded image to the X11 server. */
	XRenderAddGlyphs(dpy, glyphset, &glyph, &info, 1, paddedImage, (int) (stride * chr.height));
}

/* Free memory and exit cleanly. */
static void
teardown(void)
{
	bitfield_free();
	sft_freefont(sft.font);
	XCloseDisplay(dpy);
	exit(0);
}

/* Draw a line of UTF-8 text on our window. */
static void
drawtext(int x, int y, const char *text)
{
	uint32_t codepoints[LINE_LEN];
	
	/* Decode the UTF-8 string to UTF-32, aka an array of raw Unicode codepoints. */
	int length = utf8_to_utf32((const uint8_t *) text, codepoints, LINE_LEN);

	/* Strip non-printable characters. */
	int w = 0;
	for (int r = 0; r < length; ++r) {
		if (codepoints[r] >= 0x20)
			codepoints[w++] = codepoints[r];
	}
	length = w;

	/* Lazily render and upload all the characters that we want to draw for the first time. */
	for (int i = 0; i < length; ++i) {
		if (!bitfield_setbit(codepoints[i])) {
			loadglyph(&sft, codepoints[i]);
		}
	}

	/* Tell XRender to draw the line of text! */
	XRenderCompositeString32(dpy, PictOpOver,
		fgpic, pic, NULL,
		glyphset, 0, 0, x, y, codepoints, length);
}

/* (Re-)draw the contents of our application's window. */
static void
draw(unsigned int width, unsigned int height)
{
	/* Clear the window by overwriting everything with our background color. */
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

/* Process an event that we got from X11. */
static void
handleevent(XEvent *ev)
{
	switch (ev->type) {
	case Expose:
		draw((unsigned int) ev->xexpose.width, (unsigned int) ev->xexpose.height);
		break;
	case ClientMessage:
		if ((Atom) ev->xclient.data.l[0] == wmDeleteWindow) {
			teardown();
		}
		break;
	}
}

static void
setupx(void)
{
	XRenderPictFormat *format;
	XRenderPictureAttributes attr;

	if (!(dpy = XOpenDisplay(NULL))) {
		die("Can't open X display\n");
	}
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

	/* Initialize our parameters with some default values. */
	filename = "resources/Ubuntu-R.ttf";
	textfile = "resources/glass.utf8";
	size = 16.0;
	bgcolor = (XRenderColor) { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
	fgcolor = (XRenderColor) { 0x0000, 0x0000, 0x0000, 0xFFFF };

	/* Parse the user-supplied command line arguments and
	 * overwrite our parameters with them.
	 * We use the single-header library arg.h for this. */
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

	/* Read in the text to display on screen from a separate file. */
	FILE *file = fopen(textfile, "r");
	if (file == NULL) {
		die("Can't open text file.");
	}
	while (numlines < MAX_LINES && fgets(lines[numlines++], LINE_LEN, file)) {}
	fclose(file);

	setupx();

	format = XRenderFindStandardFormat(dpy, PictStandardA8);
	glyphset = XRenderCreateGlyphSet(dpy, format);

	/* Set up the SFT struct / 'drawing context' for libschrift. */
	/* First off, try to load the font from a file. */
	if ((sft.font = sft_loadfile(filename)) == NULL) {
		die("Can't load font file.");
	}
	sft.xScale = size;
	sft.yScale = size;
	/* Tell libschrift that our Y axis points downward and
	 * that we want it to actually render images of the characters
	 * (per default libschrift only returns information about characters). */
	sft.flags = SFT_DOWNWARD_Y | SFT_RENDER_IMAGE;

	bitfield_init();

	runx();
	return 0;
}

