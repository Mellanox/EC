/*
 * Copyright (c) 2016 Mellanox Technologies.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../include/eco_list.h"

typedef struct eco_node {
	struct ibv_mr *mr;
	eco_list node;
} eco_node;

void init_eco_list(eco_list* head)
{
	INIT_LIST_HEAD(head);
}

int eco_list_add(eco_list *head, struct ibv_mr *mr)
{
	eco_node *new_node = (eco_node *) malloc(sizeof(*new_node));
	if (!new_node) {
		return -1;
	}

	new_node->mr = mr;
	INIT_LIST_HEAD(&new_node->node);
	list_add(&new_node->node, head);
	return 0;
}

void eco_list_display(eco_list *head)
{
	eco_list *list_iter;
	eco_node *node_iter;

	printf("eco_list_display [%p] : \n", head);
	list_for_each(list_iter, head)
	{
		node_iter = list_entry(list_iter, eco_node, node);
		printf("{buffer=%p length=%lu lkey=%d} ", node_iter->mr->addr, node_iter->mr->length, node_iter->mr->lkey);
	}
	printf("\n");
}

void eco_list_delete_all(eco_list *head)
{
	eco_list *list_iter;
	eco_node *node_iter;

	redo:
	list_for_each(list_iter, head)
	{
		node_iter = list_entry(list_iter, eco_node, node);
		list_del(&node_iter->node);
		ibv_dereg_mr(node_iter->mr);
		free(node_iter);
		goto redo;
	}
}

struct ibv_mr * eco_list_get_mr(eco_list *head, void* arr, size_t block_size)
{
	eco_list *list_iter;
	struct ibv_mr *mr;

	list_for_each(list_iter, head)
	{
		mr = list_entry(list_iter, eco_node, node)->mr;
		if ((mr->addr <= arr) && (mr->addr + mr->length >= arr + block_size)) {
			return mr;
		}
	}

	return NULL;
}
