/* needs stdint.h */

#ifndef UTF8_TO_UTF32_H
#define UTF8_TO_UTF32_H

static int
utf8_to_utf32(const uint8_t *utf8, uint32_t *utf32, int max)
{
	unsigned int c;
	int i = 0;
	--max;
	while (*utf8) {
		if (i >= max)
			return 0;
		if (!(*utf8 & 0x80)) {
			utf32[i++] = *utf8++;
		} else if ((*utf8 & 0xe0) == 0xc0) {
			c = (*utf8++ & 0x1f) << 6;
			if ((*utf8 & 0xc0) != 0x80) return 0;
			utf32[i++] = c + (*utf8++ & 0x3f);
		} else if ((*utf8 & 0xf0) == 0xe0) {
			c = (*utf8++ & 0x0f) << 12;
			if ((*utf8 & 0xc0) != 0x80) return 0;
			c += (*utf8++ & 0x3f) << 6;
			if ((*utf8 & 0xc0) != 0x80) return 0;
			utf32[i++] = c + (*utf8++ & 0x3f);
		} else if ((*utf8 & 0xf8) == 0xf0) {
			c = (*utf8++ & 0x07) << 18;
			if ((*utf8 & 0xc0) != 0x80) return 0;
			c += (*utf8++ & 0x3f) << 12;
			if ((*utf8 & 0xc0) != 0x80) return 0;
			c += (*utf8++ & 0x3f) << 6;
			if ((*utf8 & 0xc0) != 0x80) return 0;
			c += (*utf8++ & 0x3f);
			if ((c & 0xFFFFF800) == 0xD800) return 0;
			if (c >= 0x10000) {
				c -= 0x10000;
				if (i + 2 > max) return 0;
				utf32[i++] = 0xD800 | (0x3ff & (c >> 10));
				utf32[i++] = 0xDC00 | (0x3ff & (c      ));
			}
		} else return 0;
	}
	utf32[i] = 0;
	return i;
}

#endif

