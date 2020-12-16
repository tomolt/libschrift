/* A stress testing program. Useful for profiling hot spots in libschrift. */
/* See LICENSE file for copyright and license details. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	struct SFT sft;
	SFT_Font *font;
	const char *filename;
	double size;
	unsigned long cp, gid;
	struct SFT_HMetrics hmtx;
	struct SFT_Box box;
	struct SFT_Image image;
	int i;

	filename = "resources/Ubuntu-R.ttf";
	size = 20.0;

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

	if (!(font = sft_loadfile(filename)))
		die("Can't load font file.");
	memset(&sft, 0, sizeof sft);
	sft.font = font;
	sft.xScale = size;
	sft.yScale = size;
	sft.flags = SFT_DOWNWARD_Y;
	for (i = 0; i < 5000; ++i) {
		for (cp = 32; cp < 128; ++cp) {
			if (sft_lookup(&sft, cp, &gid) < 0)
				continue;
			if (sft_hmetrics(&sft, gid, &hmtx) < 0)
				continue;
			if (sft_box(&sft, gid, &box) < 0)
				continue;
			image.width  = box.minWidth;
			image.height = box.minHeight;
			image.pixels = malloc(image.width * image.height);
			sft_render(&sft, gid, image);
			free(image.pixels);
		}
	}
	sft_freefont(font);
	return 0;
}

