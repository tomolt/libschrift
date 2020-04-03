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
		"usage: %s [-f font file] [-h hdpi] [-v vdpi] "
		"[-d size in pt] [-x size in px]\n", argv0);
}

int
main(int argc, char *argv[])
{
	const char *filename;
	SFT_Font *font;
	struct SFT_Style style;
	SFT *sft;

	filename = "resources/Ubuntu-R.ttf";
	style.hdpi = 96.0;
	style.vdpi = 96.0;
	style.size = 64.0;
	style.units = 'x';

	ARGBEGIN {
	case 'f':
		filename = EARGF(usage());
		break;
	case 'h':
		style.hdpi = atof(EARGF(usage()));
		break;
	case 'v':
		style.vdpi = atof(EARGF(usage()));
		break;
	case 'd':
		style.size = atof(EARGF(usage()));
		style.units = 'd';
		break;
	case 'x':
		style.size = atof(EARGF(usage()));
		style.units = 'x';
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
	style.font = font;
	if ((sft = sft_create()) == NULL)
		die("Can't create schrift context.");
	if (sft_setstyle(sft, style) < 0)
		die("Can't set text style.");

	double linegap;
	if (sft_linegap(sft, &linegap) < 0)
		die("Can't look up line gap.");
	printf("line gap: %f\n", linegap);

	sft_destroy(sft);
	sft_freefont(font);
	return 0;
}

