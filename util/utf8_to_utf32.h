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
		if (!(*utf8 & 0x80U)) {
			utf32[i++] = *utf8++;
		} else if ((*utf8 & 0xe0U) == 0xc0U) {
			c = (*utf8++ & 0x1fU) << 6;
			if ((*utf8 & 0xc0U) != 0x80U) return 0;
			utf32[i++] = c + (*utf8++ & 0x3fU);
		} else if ((*utf8 & 0xf0U) == 0xe0U) {
			c = (*utf8++ & 0x0fU) << 12;
			if ((*utf8 & 0xc0U) != 0x80U) return 0;
			c += (*utf8++ & 0x3fU) << 6;
			if ((*utf8 & 0xc0U) != 0x80U) return 0;
			utf32[i++] = c + (*utf8++ & 0x3fU);
		} else if ((*utf8 & 0xf8U) == 0xf0U) {
			c = (*utf8++ & 0x07U) << 18;
			if ((*utf8 & 0xc0U) != 0x80U) return 0;
			c += (*utf8++ & 0x3fU) << 12;
			if ((*utf8 & 0xc0U) != 0x80U) return 0;
			c += (*utf8++ & 0x3fU) << 6;
			if ((*utf8 & 0xc0U) != 0x80U) return 0;
			c += (*utf8++ & 0x3fU);
			if ((c & 0xFFFFF800U) == 0xD800U) return 0;
            utf32[i++] = c;
		} else return 0;
	}
	utf32[i] = 0;
	return i;
}

#endif

