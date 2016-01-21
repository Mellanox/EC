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
