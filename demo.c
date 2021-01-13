
// MINIMAL libschrift demo:  cc -Wall -lX11 -lXrender -lm -I. schrift.c demo.c -o t && ./t

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include <stdint.h>
#include "util/utf8_to_utf32.h"
#include "schrift.h"

static int
add_glyph(SFT *sft, Display *dpy, GlyphSet glyphset, unsigned long cp)
{
#define ABORT(cp, m) { fprintf(stderr, "codepoint 0x%04lX %s\n", cp, m); return -1; }

	SFT_Glyph gid;  //  unsigned long gid;
	if (sft_lookup(sft, cp, &gid) < 0)
		ABORT(cp, "missing")

	SFT_HMetrics hmtx;
	if (sft_hmetrics(sft, gid, &hmtx) < 0)
		ABORT(cp, "bad horizontal metrics")

	SFT_Extents e;
	if (sft_extents(sft, gid, &e) < 0)
		ABORT(cp, "no extents")

	SFT_Image i = { .width = (e.minWidth + 3) & ~3, .height = e.minHeight };
	char pixels[i.width * i.height];
	i.pixels = pixels;

	if (sft_render(sft, gid, i) < 0)
		ABORT(cp, "not rendered")

	XGlyphInfo info = {
		.x      = (short) -hmtx.leftSideBearing,
		.y      = (short) -e.yOffset,
		.width  = (unsigned short) i.width,
		.height = (unsigned short) i.height,
		.xOff   = (short) hmtx.advanceWidth,
		.yOff   = 0
	};
	Glyph g = cp;
	XRenderAddGlyphs(dpy, glyphset, &g, &info, 1,
		i.pixels, (int) (i.width * i.height));

	return 0;
}

int
main(int argc, char *argv[])
{
#define END(m) { fprintf(stderr, "%s\n", m); exit(1); }

	Display *dpy = XOpenDisplay(NULL);
	if(!dpy) END("Can't open X display")
	int s = 2, screen = DefaultScreen(dpy);

	Window win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 600*s, 440*s, 0,
			DefaultDepth(dpy, screen), InputOutput, CopyFromParent, 0, NULL);
	XSelectInput(dpy, win, ExposureMask);
	Atom wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmDeleteWindow, 1);
	XMapRaised(dpy, win);


	XRenderPictFormat *f = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, screen));
	Picture pic = XRenderCreatePicture(dpy, win, f, 0, NULL);
	XRenderColor fg = { 0, 0, 0, 0xFFFF }, bg = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

	Pixmap fgpix = XCreatePixmap(dpy, win, 1, 1, 24);
	XRenderPictureAttributes attr = { .repeat = True };
	f = XRenderFindStandardFormat(dpy, PictStandardRGB24);
	Picture fgpic = XRenderCreatePicture(dpy, fgpix, f, CPRepeat, &attr);
	XRenderFillRectangle(dpy, PictOpSrc, fgpic, &fg, 0, 0, 1, 1);

	f = XRenderFindStandardFormat(dpy, PictStandardA8);
	GlyphSet glyphset = XRenderCreateGlyphSet(dpy, f);

	SFT sft = { .flags = SFT_DOWNWARD_Y, .xScale = 16*s, .yScale = sft.xScale };
	if(!(sft.font = sft_loadfile("resources/Ubuntu-R.ttf")))
		END("TTF load failed")

	XEvent e;
	while (!XNextEvent(dpy, &e)) {
		if (e.type == Expose) {
			XRenderFillRectangle(dpy, PictOpOver, pic, &bg, 0, 0, e.xexpose.width, e.xexpose.height);

			FILE *f = fopen("resources/glass.utf8", "r");
			if (!f) END("Cannot open input text")

			SFT_LMetrics m;
			sft_lmetrics(&sft, &m);
			int y = 20 + m.ascender + m.lineGap;

			char t[256];
			while (fgets(t, sizeof(t), f)) {
				int n = strlen(t) - 1; t[n] = 0;  // '\n' => len>0

				unsigned u[sizeof(t)];
				n = utf8_to_utf32((unsigned char *) t, u, sizeof(t));  // (const uint8_t *)

				for (int i = 0; i < n; i++) add_glyph(&sft, dpy, glyphset, u[i]);
				XRenderCompositeString32(dpy, PictOpOver, fgpic, pic, NULL, glyphset, 0, 0, 20, y, u, n);

				y += 2 * (m.ascender + m.descender + m.lineGap);
			}

			fclose(f);
		} else if (e.type == ClientMessage) {
			if ((Atom) e.xclient.data.l[0] == wmDeleteWindow)
				break;
		}
	}

	sft_freefont(sft.font);
	XCloseDisplay(dpy);
	return 0;
}

