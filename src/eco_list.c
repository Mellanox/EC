/*
 ** Copyright (C) 2016 Mellanox Technologies
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at:
 **
 ** http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 ** either express or implied. See the License for the specific language
 ** governing permissions and  limitations under the License.
 **
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
