/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <schrift.h>

#include "arg.h"

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
		"usage: %s [-f font file] [-s size in px] [message]\n", argv0);
}

static void
loadglyph(struct SFT *sft, unsigned int charCode)
{
	struct SFT_Char chr = { 0 };
	XGlyphInfo info;
	Glyph glyph;
	int stride, i;

	if (sft_char(sft, charCode, &chr) < 0)
		return;

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
		memcpy(bitmap + i * stride, chr.image + i * chr.width, chr.width);
	free(chr.image);

	XRenderAddGlyphs(dpy, glyphset, &glyph, &info, 1, bitmap, stride * chr.height);
}

static void
loadglyphset(const char *filename, double size)
{
	struct SFT sft = { 0 };
	SFT_Font *font;
	XRenderPictFormat *format;
	unsigned char c;

	format = XRenderFindStandardFormat(dpy, PictStandardA8);
	glyphset = XRenderCreateGlyphSet(dpy, format);

	if ((font = sft_loadfile(filename)) == NULL)
		die("Can't load font file.");

	sft.font = font;
	sft.xScale = size;
	sft.yScale = size;
	sft.flags = SFT_DOWNWARD_Y | SFT_CHAR_IMAGE;

	for (c = 32; c < 128; ++c)
		loadglyph(&sft, c);

	XSync(dpy, 0);
	sft_freefont(font);
}

static void
teardownx(void)
{
	XCloseDisplay(dpy);
	exit(0);
}

static void
draw(int width, int height)
{
	XRenderFillRectangle(dpy, PictOpOver,
		pic, &bgcolor, 0, 0, width, height);
	XRenderCompositeString8(dpy, PictOpOver,
		fgpic, pic, NULL,
		glyphset, 0, 0, 20, 50, message, strlen(message));
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
			teardownx();
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
	loadglyphset(filename, size);
	runx();
	return 0;
}

