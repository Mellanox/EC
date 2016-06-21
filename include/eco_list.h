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

#ifndef ECO_LIST_H_
#define ECO_LIST_H_

/**
 * @file eco_list.h
 * @brief Define a simple doubly linked list holding ibv_mr objects. This list is based on Linux kernel list.h.
 *
 * Mellanox EC library used for Erasure Coding and RAID HW offload.
 * Erasure coding (EC) is a method of data protection in which data is broken into fragments,
 * expanded and encoded with redundant data pieces and stored across a set of different locations or storage media.
 * Currently supported by mlx5 only.
 */

#include "list.h"
#include <infiniband/verbs.h>

typedef struct list_head eco_list;

/**
 * Initialize a new list.
 *
 * @param eco_list                Pointer to an allocated eco_list object.
 */
void init_eco_list(eco_list* eco_list);

/**
 * Adds new ibv_mr to the list
 *
 * @param head                    The head of the list.
 * @param mr                      Pointer to ibv_mr object.
 * @return                        0 successful, other fail.
 */
int eco_list_add(eco_list *head, struct ibv_mr *mr);

/**
 * Prints all the elements in the list.
 *
 * @param head                    The head of the list.
 */
void eco_list_display(eco_list *head);

/**
 * Deletes all entries from the list.
 *
 * @param head                    The head of the list.
 */
void eco_list_delete_all(eco_list *head);

/**
 * Finds a ibv_mr that contains the buffer (arr + block_size).
 *
 * @param head                    The head of the list.
 * @param arr                     The address of the new buffer.
 * @param block_size              The size of the buffer.
 * @return                        ibv_mr which contains the whole buffer (address + size). else, NULL.
 */
struct ibv_mr * eco_list_get_mr(eco_list *head, void* arr, size_t block_size);

#endif /* ECO_LIST_H_ */
