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

#ifndef ECO_COMMON_H
#define ECO_COMMON_H

/**
 * @file eco_common.h
 * @brief Describe the common methods and structures between the encoder and decoder.
 *
 * Mellanox EC library used for Erasure Coding and RAID HW offload.
 * Erasure coding (EC) is a method of data protection in which data is broken into fragments,
 * expanded and encoded with redundant data pieces and stored across a set of different locations or storage media.
 * Currently supported by mlx5 only.
 */

#include "eco_list.h"
#include <string.h>
#include <jerasure.h>
#include <infiniband/verbs_exp.h>

#define dbg_log                               if (0) printf
#define err_log                               printf

#define W 4

/**
 * Erasure Coding Offload completion context. Used for async encode/decode operations.
 *
 * @comp                                     completion context of EC calculation.
 * @eco_coder                                Pointer to an encoder/decoder.
 * @is_remainder_comp                        Boolean variable which determine the type of the completion context.
 */
struct eco_coder_comp {
	struct ibv_exp_ec_comp                   comp;
	void                                     *eco_coder;
	int                                      is_remainder_comp;
};

/**
 * Erasure Coding Offload context structure.
 *
 * @calc                                       Verbs erasure coding engine context.
 * @attr                                       Verbs erasure coding engine initialization attributes.
 * @alignment_mem                              Verbs erasure coding memory layout context used for 64 bytes aligned buffers.
 * @mrs_list                                   Simple doubly linked list of lbv_mr objects.
 * @int_encode_matrx                           Used for Jerasure to calculate the decode matrix.
 * @remainder_buffers                          [k + m] buffers each of size 64 bytes used for the remainder from 64 bytes.
 * @remainder_mem                              Verbs erasure coding memory layout context used for the remainder from 64 bytes.
 * @block_size                                 The size of the input blocks.
 * @async_mutex                                Mutex used for async encode/decode operations.
 * @async_cond                                 Condition used for async encode/decode operations.
 * @async_ref_count                            Reference count used for async encode/decode operations.
 * @data                                       Array of pointers to source input buffers.
 * @coding                                     Array of pointers to coded output buffers.
 * @alignment_comp                             Erasure Coding Offload completion context used for 64 bytes aligned buffers.
 * @remainder_comp                             Erasure Coding Offload completion context used for the remainder from 64 bytes.
 */
struct eco_context {
	struct ibv_exp_ec_calc                    *calc;
	struct ibv_exp_ec_calc_init_attr          attr;
	struct ibv_exp_ec_mem                     alignment_mem;
	eco_list                                  mrs_list;
	int                                       *int_encode_matrix;
	uint8_t                                   *remainder_buffers;
	struct ibv_exp_ec_mem                     remainder_mem;
	struct ibv_mr                             *remainder_mr;
	int                                       block_size;
	pthread_mutex_t                           async_mutex;
	pthread_cond_t                            async_cond;
	int                                       async_ref_count;
	uint8_t                                   **data;
	uint8_t                                   **coding;
	struct eco_coder_comp                     alignment_comp;
	struct eco_coder_comp                     remainder_comp;
};

/**
 * Initialize verbs EC context for fast Erasure Coding HW offload.
 *
 * @param coder                              Pointer to an encoder/decoder.
 * @param k                                  Number of data blocks.
 * @param m                                  Number of code blocks.
 * @param use_vandermonde_matrix             Boolean variable which determine the type of the encode matrix:
 *                                           0 for Cauchy coding matrix else for Vandermonde coding matrix.
 * @param comp_done_func                     Function handle of the EC calculation completion.
 * @return                                   Pointer to an initialize EC context object if successful, else NULL.
 */
struct eco_context *mlx_eco_init(void *coder, int k, int m, int use_vandermonde_matrix, void (*comp_done_func)(struct ibv_exp_ec_comp *));

/**
 * Register buffers and update the alignment memory layout context for future encode/decode operations.
 * Because the HW can perform encode/decode operations only on 64 bytes aligned buffers, we will register only the aligned part of the buffers.
 * This function is optional - but it is recommended to use for better performance.
 *
 * @param eco_context                        Pointer to an initialized EC context.
 * @param data                               Array of pointers to source input buffers.
 * @param coding                             Array of pointers to coded output buffers.
 * @param data_size                          Size of data array (must be equal to the initial amount of data blocks).
 * @param coding_size                        Size of coding array (must be equal to the initial amount of code blocks).
 * @param block_size                         Length of each block of data.
 * @return                                   0 successful, other fail.
 */
int mlx_eco_register(struct eco_context *eco_ctx, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size);

/**
 * Release all EC context resources.
 *
 * @param eco_context                        Pointer to an initialized EC context.
 * @return                                   0 successful, other fail.
 */
int mlx_eco_release(struct eco_context *eco_ctx);

#endif /* ECO_COMMON_H */
