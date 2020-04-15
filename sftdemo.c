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
		"usage: %s [-f font file] [-s size in px]\n", argv0);
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
	/* FIXME Properly handle this case! */
	if (chr.image == NULL)
		return;

	glyph = charCode;
	info.x = chr.x;
	info.y = chr.y + chr.height;
	info.width = chr.width;
	info.height = chr.height;
	info.xOff = (int) (chr.advance + 0.5); /* You *should* use round() here instead. */
	info.yOff = 0;

#if 0
	if (charCode == 'H') {
		fprintf(stderr, "P2\n%d\n%d\n255\n", chr.width, chr.height);
		for (int y = 0; y < chr.height; ++y) {
			for (int x = 0; x < chr.width; ++x) {
				fprintf(stderr, "%03u ", (unsigned char) chr.image[x + y * chr.width]);
			}
			fprintf(stderr, "\n");
		}
	}
#endif

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
	unsigned int i;

	format = XRenderFindStandardFormat(dpy, PictStandardA8);
	glyphset = XRenderCreateGlyphSet(dpy, format);

	if ((font = sft_loadfile(filename)) == NULL)
		die("Can't load font file.");

	sft.font = font;
	sft.xScale = size;
	sft.yScale = size;
	sft.flags = /* SFT_DOWNWARD_Y | */ SFT_CHAR_IMAGE;

	char *str = "H";
	for (i = 0; str[i]; ++i)
		loadglyph(&sft, str[i]);

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
		glyphset, 0, 0, 20, 50, "Hello,World!", 12);
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
		usage();
		exit(1);
	}

	setupx();
	loadglyphset(filename, size);
	runx();
	return 0;
}

