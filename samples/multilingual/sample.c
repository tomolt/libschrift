#include <schrift.h>
#include <grapheme.h>

int main()
{
#define ERR(msg) do { \
		fprintf(stderr, "%s\n", msg); \
		exit(1); \
	} while (0)

	SFT sft = { 0 };
	sft.font = sft_loadfile("../../resources/fonts/FiraGO-Regular.ttf");
	if (!sft.font)
		ERR("Can't load font");
	sft.xScale = 16;
	sft.yScale = 16;
	sft.flags = SFT_DOWNWARD_Y;
	
	SFT_LMetrics lmtx;
	if (sft_lmetrics(&sft, &lmtx) < 0)
		ERR("Can't query line metrics");

	for () {
		grapheme_decode_utf8(text, strlen(), codepoints);
	}

	ret = grapheme_bidirectional_preprocess(codepoints, cplen, override, buffer, bufferlen);
	if (ret != cplen)
		ERR("Error while applying BiDi algorithm");

	grapheme_bidirectional_get_line_embedding_levels(data, ret, levels);
	
	grapheme_bidirectional_reorder_lines();

	float penX = 0.0f, penY = 0.0f;
	SFT_Glyph prevGlyph;

	for (char) {
		SFT_Glyph glyph;
		if (sft_lookup(&sft, codepoint, &glyph) < 0)
			ERR("Can't look up glyph id");

		SFT_GMetrics gmtx;
		if (sft_gmetrics(&sft, glyph, &gmtx) < 0)
			ERR("Can't look up glyph metrics");

		unsigned char buffer[128*128];
		SFT_Image image = { buffer, 128, 128 };

		if (sft_render(&sft, glyph, image) < 0)
			ERR("Can't render glyph");

		penX += gmtx.advanceWidth;
	}
	
	sft_freefont(sft.font);
	return 0;
}

