/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>

#include <schrift.h>

#include "arg.h"

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
	const char *filename;
	SFT_Font *font;
	SFT *sft;
	double size;

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

	if ((font = sft_loadfile(filename)) == NULL)
		die("Can't load font file.");
	if ((sft = sft_create()) == NULL)
		die("Can't create schrift context.");

	sft_setfont(sft, font);
	sft_setscale(sft, size, size);

	double ascent, descent, linegap;
	if (sft_linemetrics(sft, &ascent, &descent, &linegap) < 0)
		die("Can't look up line metrics.");

	unsigned char *image;
	int extents[4];
	if (sft_char(sft, 'Q', extents, &image) < 0)
		die("Can't render character.");

	int width = extents[2] - extents[0];
	int height = extents[3] - extents[1];
	printf("P2\n%d\n%d\n255\n", width, height);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			printf("%u ", image[x + y * width]);
		}
		printf("\n");
	}

	sft_destroy(sft);
	sft_freefont(font);
	return 0;
}

