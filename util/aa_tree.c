/*
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

#include <stdlib.h>

#include "aa_tree.h"

struct aa_node
{
	struct aa_node *childs[2];
	void *value;
	unsigned char black; /* TODO pack memory more tightly */
	void key[];
};

static struct aa_node *
aa_skew(struct aa_node *node)
{
	struct aa_node *left = node->childs[0];
	if (left == NULL || left->black)
		return node;
	node->childs[0] = left->childs[1];
	left->childs[1] = node;
	left->black = node->black;
	node->black = 0;
	return left;
}

static struct aa_node *
aa_split(struct aa_node *node)
{
	struct aa_node *right1 = node->childs[1];
	if (right1 == NULL || right1->black)
		return node;
	struct aa_node *right2 = right1->childs[1];
	if (right2 == NULL || right2->black)
		return node;
	node->childs[1] = right1->childs[0];
	right1->childs[0] = node;
	right1->black = 0;
	node->black = 1;
	right2->black = 1;
	return right1;
}

static struct aa_node *
aa_put_rec(struct aa_tree *tree, struct aa_node *node, const void *key, void *value)
{
	if (node == NULL) {
		struct aa_node *new = calloc(1, sizeof(*new) + tree->keysize);
		memcpy(new->key, key, tree->keysize);
		new->value = value;
		return new;
	} else {
		int cmp = tree->compare(key, node->key, tree->userdata);
		if (cmp == 0) {
			node->value = value;
			return node;
		}
		node->childs[cmp > 0] = aa_put_rec(node->childs[cmp > 0], key, value);
		node = aa_skew(node);
		node = aa_split(node);
		return node;
	}
}

static void
aa_free_rec(struct aa_node *node)
{
	if (node == NULL)
		return;
	aa_free_rec(node->childs[0]);
	aa_free_rec(node->childs[1]);
	free(node);
}

void
aa_init(struct aa_tree *tree, int keysize, aa_compare_func compare, const void *userdata)
{
	tree->root = NULL;
	tree->keysize = keysize;
	tree->compare = compare;
	tree->userdata = userdata;
}

void
aa_put(struct aa_tree *tree, const void *key, void *value)
{
	tree->root = aa_put_rec(tree->root, key, value);
}

int
aa_get(struct aa_tree *tree, const void *key, void **value)
{
	struct aa_node *node = tree->root;
	while (node != NULL) {
		int cmp = tree->compare(key, node->key, tree->userdata);
		if (cmp == 0) {
			if (value != NULL)
				*value = node->value;
			return 1;
		}
		node = node->childs[cmp > 0];
	}
	return 0;
}

void
aa_free(struct aa_tree *tree)
{
	aa_free_rec(tree->root);
}

