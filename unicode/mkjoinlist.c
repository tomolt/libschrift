#include <stdio.h>
#include <stdlib.h>

struct range {
	unsigned start, end;
};

static int
compare_ranges(const void *va, const void *vb)
{
	const struct range *ra = va, *rb = vb;
	return ra->start - rb->start;
}

int
main()
{
	size_t length = 0;
	size_t capac = 256;
	struct range *list = malloc(capac * sizeof *list);

	FILE *file = fopen("DerivedJoiningType.txt", "r");
	if (!file) return 1;

	char line[1000];
	while (fgets(line, sizeof line, file)) {
		struct range range;
		char type;
		range.end = (unsigned)-1;
		if (sscanf(line, "%x ; %c", &range.start, &type) == 2 ||
			sscanf(line, "%x..%x ; %c", &range.start, &range.end, &type) == 3)
		{
			if (range.end == (unsigned)-1) {
				range.end = range.start;
			}
			if (capac == length) {
				capac *= 2;
				list = realloc(list, capac * sizeof *list);
			}
			list[length++] = range;
		}
	}

	fclose(file);

	qsort(list, length, sizeof *list, compare_ranges);

	size_t w = 1;
	for (size_t i = 1; i < length; i++) {
		if (list[i-1].end + 1 != list[i].start) {
			list[w++] = list[i];
		}
	}
	length = w;

	size_t column;

	printf("static const uint16_t cursive_codepoint_starts[] = {\n\t");
	column = 4;
	for (size_t i = 0; i < length; i++) {
		if (column > 75) {
			printf("\n\t");
			column = 4;
		}
		column += printf("%u,", list[i].start);
	}
	printf("\n};\n");
	
	printf("static const uint16_t cursive_codepoint_ends[] = {\n\t");
	column = 4;
	for (size_t i = 0; i < length; i++) {
		if (column > 75) {
			printf("\n\t");
			column = 4;
		}
		column += printf("%u,", list[i].end);
	}
	printf("\n};\n");
	
	free(list);
	return 0;
}

