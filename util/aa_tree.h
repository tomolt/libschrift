/* A simple balanced binary search tree for mapping integers to integers.
 *
 * ISC-License
 *
 * Copyright 2020 Thomas Oltmann
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef AA_TREE_H
#define AA_TREE_H

struct aa_node;

typedef int (*aa_compare_func)(const void *left, const void *right, const void *userdata);

struct aa_tree {
	struct aa_node *root;
	aa_compare_func compare;
	const void *userdata;
	unsigned int keysize;
};

void aa_init(struct aa_tree *tree, unsigned int keysize,
	aa_compare_func compare, const void *userdata);
void aa_put(struct aa_tree *tree, const void *key, void *value);
int  aa_get(struct aa_tree *tree, const void *key, void **value);
void aa_free(struct aa_tree *tree);

#endif

