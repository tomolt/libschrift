/*
 * A simple command line application that shows how to
 * use libschrift with X11 via XRender.
 * See LICENSE file for copyright and license details.
 * Contributed by Andor Badi.
 */

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include <stdint.h>
#include "util/utf8_to_utf32.h"
#include "schrift.h"

static int add_glyph(Display *dpy, GlyphSet glyphset, SFT *sft, unsigned long cp)
{
#define ABORT(cp, m) do { fprintf(stderr, "codepoint 0x%04lX %s\n", cp, m); return -1; } while (0)

	SFT_Glyph gid;  //  unsigned long gid;
	if (sft_lookup(sft, cp, &gid) < 0)
		ABORT(cp, "missing");
	if (sft_substitute(sft, "medi", &gid) < 0)
		ABORT(cp, "Can't apply GSUB features");

	SFT_GMetrics mtx;
	if (sft_gmetrics(sft, gid, &mtx) < 0)
		ABORT(cp, "bad glyph metrics");

	SFT_Image img = {
		.width  = (mtx.minWidth + 3) & ~3,
		.height = mtx.minHeight,
	};
	char pixels[img.width * img.height];
	img.pixels = pixels;
	if (sft_render(sft, gid, img) < 0)
		ABORT(cp, "not rendered");

	XGlyphInfo info = {
		.x      = (short) -mtx.leftSideBearing,
		.y      = (short) -mtx.yOffset,
		.width  = (unsigned short) img.width,
		.height = (unsigned short) img.height,
		.xOff   = (short) mtx.advanceWidth,
		.yOff   = 0
	};
	Glyph g = cp;
	XRenderAddGlyphs(dpy, glyphset, &g, &info, 1,
		img.pixels, (int) (img.width * img.height));

	return 0;
}

int main()
{
#define END(m) do { fprintf(stderr, "%s\n", m); return 1; } while (0)

	Display *dpy = XOpenDisplay(NULL);
	if (dpy == NULL)
		END("Can't open X display");
	int s = 2, screen = DefaultScreen(dpy);

	Window win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 600*s, 440*s, 0,
		DefaultDepth(dpy, screen), InputOutput, CopyFromParent, 0, NULL);
	XSelectInput(dpy, win, ExposureMask);
	Atom wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmDeleteWindow, 1);
	XMapRaised(dpy, win);

	XRenderPictFormat *fmt = XRenderFindVisualFormat(dpy, DefaultVisual(dpy, screen));
	Picture pic = XRenderCreatePicture(dpy, win, fmt, 0, NULL);
	XRenderColor fg = { 0, 0, 0, 0xFFFF }, bg = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

	Pixmap fgpix = XCreatePixmap(dpy, win, 1, 1, 24);
	XRenderPictureAttributes attr = { .repeat = True };
	fmt = XRenderFindStandardFormat(dpy, PictStandardRGB24);
	Picture fgpic = XRenderCreatePicture(dpy, fgpix, fmt, CPRepeat, &attr);
	XRenderFillRectangle(dpy, PictOpSrc, fgpic, &fg, 0, 0, 1, 1);

	fmt = XRenderFindStandardFormat(dpy, PictStandardA8);
	GlyphSet glyphset = XRenderCreateGlyphSet(dpy, fmt);

	SFT sft = {
		.xScale = 16*s,
		.yScale = 16*s,
		.flags  = SFT_DOWNWARD_Y,
	};
	sft.font = sft_loadfile("resources/FiraGO-Regular_extended_with_NotoSansEgyptianHieroglyphs-Regular.ttf");
	if (sft.font == NULL)
		END("TTF load failed");

	if (sft_writingsystem(sft.font, "arab", "URD ", &sft.writingSystem) < 0)
		END("Can't select writing system!");

	XEvent event;
	while (!XNextEvent(dpy, &event)) {
		if (event.type == Expose) {
			XRenderFillRectangle(dpy, PictOpOver, pic, &bg, 0, 0, event.xexpose.width, event.xexpose.height);

			FILE *file = fopen("resources/glass.utf8", "r");
			if (file == NULL)
				END("Cannot open input text");

			SFT_LMetrics lmtx;
			sft_lmetrics(&sft, &lmtx);
			int y = 20 + lmtx.ascender + lmtx.lineGap;

			char text[256];
			while (fgets(text, sizeof(text), file)) {
				int n = strlen(text) - 1;
				text[n] = 0;  // '\n' => len>0

				unsigned codepoints[sizeof(text)];
				n = utf8_to_utf32((unsigned char *) text, codepoints, sizeof(text));  // (const uint8_t *)

				for (int i = 0; i < n; i++) {
					add_glyph(dpy, glyphset, &sft, codepoints[i]);
				}
				XRenderCompositeString32(dpy, PictOpOver, fgpic, pic, NULL, glyphset, 0, 0, 20, y, codepoints, n);

				y += 2 * (lmtx.ascender + lmtx.descender + lmtx.lineGap);
			}

			fclose(file);
		} else if (event.type == ClientMessage) {
			if ((Atom) event.xclient.data.l[0] == wmDeleteWindow)
				break;
		}
	}

	sft_freefont(sft.font);
	XCloseDisplay(dpy);
	return 0;
}

