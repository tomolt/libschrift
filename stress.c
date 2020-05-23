/* A stress testing program. Useful for profiling hot spots in libschrift. */
/* See LICENSE file for copyright and license details. */

#include <stdlib.h>
#include <stdio.h>

#include <schrift.h>

#include "util/arg.h"

char *argv0;

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

int
main(int argc, char *argv[])
{
	struct SFT sft = { 0 };
	struct SFT_Char chr;
	SFT_Font *font;
	const char *filename;
	double size;
	int i, c;

	filename = "resources/Ubuntu-R.ttf";
	size = 16.0;

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

	font = sft_loadfile(filename);
	if (font == NULL)
		die("Can't load font file.");
	sft.font = font;
	sft.xScale = size;
	sft.yScale = size;
	sft.flags = SFT_DOWNWARD_Y | SFT_RENDER_IMAGE;
	for (i = 0; i < 1000; ++i) {
		for (c = 32; c < 128; ++c) {
			if (!(sft_char(&sft, c, &chr) < 0))
				free(chr.image);
		}
	}
	sft_freefont(font);
	return 0;
}

