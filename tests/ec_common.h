/*
 * Copyright (c) 2015 Mellanox Technologies.  All rights reserved.
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

#ifndef EC_COMMON_H
#define EC_COMMON_H

#include "common.h"
#include <infiniband/verbs_exp.h>
#include <jerasure.h>
#include <jerasure/reed_sol.h>
#include <gf_complete.h>

struct ec_mr {
	uint8_t				*buf;
	struct ibv_mr			*mr;
	struct ibv_sge			*sge;
};

struct ec_context {
	struct ibv_context		*context;
	struct ibv_pd			*pd;
	struct ibv_exp_ec_calc		*calc;
	struct ibv_exp_ec_calc_init_attr attr;
	int				block_size;
	struct ec_mr			data;
	struct ec_mr			code;
	uint8_t 			**data_arr;
	uint8_t 			**code_arr;
	struct ibv_exp_ec_mem		mem;
	uint8_t				*en_mat;
	uint8_t				*de_mat;
	int				*encode_matrix;
	int				*int_erasures;
	uint8_t 			*u8_erasures;
	int				*survived_arr;
	uint32_t			survived;
};

void free_encode_matrix(struct ec_context *ctx);
int alloc_encode_matrix(struct ec_context *ctx);
int extract_erasures(char *failed_blocks, struct ec_context *ctx);
void free_decode_matrix(struct ec_context *ctx);
int alloc_decode_matrix(struct ec_context *ctx);
struct ec_context *alloc_ec_ctx(struct ibv_pd *pd, int frame_size,
				int k, int m, int w,
				int max_inflight_calcs,
				char *failed_blocks);
void free_ec_ctx(struct ec_context *ctx);
int sw_ec_encode(struct ec_context *ctx);
void close_ec_ctx(struct ec_context *ctx);
void print_matrix_int(int *m, int rows, int cols);
void print_matrix_u8(uint8_t *m, int rows, int cols);
struct ibv_device *find_device(const char *devname);

#endif /* EC_COMMON_H */
