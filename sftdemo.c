/* A simple command line application that shows how to use libschrift with X11 via XRender. */
/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <schrift.h>

#include "util/utf8_to_utf32.h"
#include "util/aa_tree.h"
#include "util/arg.h"

#define APP_NAME "sftdemo"

char *argv0;

static const char *message;
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
static struct aa_tree loadedset;

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
		"usage: %s [-v] [-f font file] [-s size in px] [-P] [message]\n", argv0);
}

static int
compare32(const void *v1, const void *v2, const void *userdata)
{
	(void) userdata;
	const uint32_t *u1 = v1, *u2 = v2;
	return *u1 - *u2;
}

static void
loadglyph(struct SFT *sft, unsigned int charCode)
{
	struct SFT_Char chr;
	XGlyphInfo info;
	Glyph glyph;
	int stride, i;

	if (sft_char(sft, charCode, &chr) < 0) {
		printf("Couldn't load character '%c' (0x%02X).\n", charCode, charCode);
		return;
	}

	glyph = charCode;
	info.x = -chr.x;
	info.y = -chr.y;
	info.width = chr.width;
	info.height = chr.height;
	info.xOff = (int) (chr.advance + 0.5); /* You *should* use round() here instead. */
	info.yOff = 0;

	stride = (chr.width + 3) & ~3;
	char bitmap[stride * chr.height];
	memset(bitmap, 0, stride * chr.height);
	for (i = 0; i < chr.height; ++i)
		memcpy(bitmap + i * stride, (char *) chr.image + i * chr.width, chr.width);
	free(chr.image);

	XRenderAddGlyphs(dpy, glyphset, &glyph, &info, 1, bitmap, stride * chr.height);
}

static void
teardown(void)
{
	aa_free(&loadedset);
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
		if (aa_get(&loadedset, &codepoints[i], NULL)) continue;
		loadglyph(&sft, codepoints[i]);
		aa_put(&loadedset, &codepoints[i], NULL);
	}

	XRenderCompositeString32(dpy, PictOpOver,
		fgpic, pic, NULL,
		glyphset, 0, 0, x, y, codepoints, length);
}

static void
draw(int width, int height)
{
	XRenderFillRectangle(dpy, PictOpOver,
		pic, &bgcolor, 0, 0, width, height);
	drawtext(20, 50, message);
}

static void
handleevent(XEvent *ev)
{
	switch (ev->type) {
	case Expose:
		draw(ev->xexpose.width, ev->xexpose.height);
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

	win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 200, 100, 0,
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
	const char *filename;
	double size;

	message = "Hello, World!";
	filename = "resources/Ubuntu-R.ttf";
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
		message = *argv;
		--argc, ++argv;
	}
	if (argc) {
		usage();
		exit(1);
	}

	setupx();

	format = XRenderFindStandardFormat(dpy, PictStandardA8);
	glyphset = XRenderCreateGlyphSet(dpy, format);

	if ((sft.font = sft_loadfile(filename)) == NULL)
		die("Can't load font file.");
	sft.xScale = size;
	sft.yScale = size;
	sft.flags = SFT_DOWNWARD_Y | SFT_RENDER_IMAGE;

	aa_init(&loadedset, 4, compare32, NULL);

	runx();
	return 0;
}

