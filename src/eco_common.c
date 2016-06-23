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

#include "../include/eco_common.h"
#include <jerasure/reed_sol.h>
#include <jerasure/cauchy.h>

#define MAX_INFLIGHT_CALCS 2

pthread_mutex_t matrix_generator_mutex; // Jerasure's encode matrix allocation is not thread safe.

/**
 * Print matrix in uint8_t format.
 *
 * @param m                         Buffer contains the matrix.
 * @param rows                      Number of rows in the matrix.
 * @param cols                      Number of columns in the matrix.
 */
static void util_mlx_eco_print_matrix_u8(uint8_t *m, int rows, int cols)
{
	int i, j;

	dbg_log("Printing u8 matrix %p\n", m);
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			if (j != 0)
				dbg_log(" ");

			dbg_log("%#x  ", m[i*cols+j]);
		}
		dbg_log("\n");
	}
}

/**
 * Allocate encode matrix using Jerasure library.
 *
 * @param k                         Number of data blocks.
 * @param m                         Number of code blocks.
 * @param use_vandermonde_matrix    Boolean variable which determine the type of the encode matrix:
 *                                  0 for Cauchy coding matrix else for Vandermonde coding matrix -
 * @return                          Pointer to an initialize encode matrix if successful, else NULL.
 */
static uint8_t *util_mlx_eco_alloc_encode_matrix(struct eco_context *eco_ctx, int k, int m, int use_vandermonde_matrix)
{
	dbg_log("alloc_encode_matrix: k = %d, m = %d, use_vandermonde_matrix = %d\n", k, m, use_vandermonde_matrix);

	int *rs_mat;
	uint8_t *res;
	int i,j;

	res = calloc(k * m, sizeof(uint8_t));
	if (!res) {
		err_log("alloc_encode_matrix: failed to allocate encode matrix\n");
		return NULL;
	}

	pthread_mutex_lock(&matrix_generator_mutex);
	rs_mat = use_vandermonde_matrix ? reed_sol_vandermonde_coding_matrix(k, m, W) : cauchy_original_coding_matrix(k, m, W);
	pthread_mutex_unlock(&matrix_generator_mutex);

	if (!rs_mat) {
		err_log("alloc_encode_matrix: failed to allocate reed sol matrix\n");
		free(res);
		return NULL;
	}

	for (i = 0; i < m; i++)
		for (j = 0; j < k; j++)
			res[j*m+i] = (uint8_t)rs_mat[i*k+j];

	util_mlx_eco_print_matrix_u8(res, k, m);

	eco_ctx->int_encode_matrix = rs_mat;

	dbg_log("alloc_encode_matrix: completed successfully - k = %d, m = %d, use_vandermonde_matrix = %d\n", k, m, use_vandermonde_matrix);

	return res;
}

/**
 * Initialize ibv_exp_ec_mem object before allocating the calc
 *
 * @param eco_mem the               ibv_exp_ec_mem object.
 * @param k                         Number of data blocks.
 * @param m                         Number of code blocks.
 * @return                          0 successful, other fail.
 */
static int util_mlx_eco_init_mem(struct ibv_exp_ec_mem *eco_mem, int k, int m)
{
	eco_mem->data_blocks = calloc(k, sizeof(*eco_mem->data_blocks));
	if (!eco_mem->data_blocks) {
		err_log("util_mlx_eco_init_mem: Failed to allocate data sges\n");
		return -ENOMEM;
	}

	memset(eco_mem->data_blocks, 0, k * sizeof(*eco_mem->data_blocks));
	eco_mem->num_data_sge = k;

	eco_mem->code_blocks = calloc(m, sizeof(*eco_mem->code_blocks));
	if (!eco_mem->code_blocks) {
		err_log("util_mlx_eco_init_mem: Failed to allocate code sges\n");
		goto free_data_sges;
	}
	memset(eco_mem->code_blocks, 0, m * sizeof(*eco_mem->code_blocks));
	eco_mem->num_code_sge = m;

	return 0;

free_data_sges:
	free(eco_mem->data_blocks);

	return -ENOMEM;
}

/**
 * Find device by name.
 *
 * @param devname                   The name of the device.
 * @return                          ibv_device corresponding to devname, else NULL.
 */
static struct ibv_device *util_mlx_eco_find_device(const char *devname)
{
	struct ibv_device **dev_list = NULL;
	struct ibv_device *device = NULL;

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		err_log("util_mlx_eco_find_device: Failed to get IB devices list");
		return NULL;
	}

	if (!devname) {
		device = *dev_list;
		if (!device)
			err_log("util_mlx_eco_find_device: No IB devices found\n");
	} else {
		int i;

		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]),
					devname))
				break;
		device = dev_list[i];
		if (!device)
			err_log("util_mlx_eco_find_device: IB device %s not found\n", devname);
	}

	ibv_free_device_list(dev_list);

	return device;
}

/**
 * Update the sge content with the mr content.
 *
 * @param sge                        Pointer to sge object.
 * @param addr                       The address of the buffer
 * @param length                     The length of the buffer
 * @param lkey                       The lkey of the buffer
 */
static inline void util_mlx_eco_update_sge(struct ibv_sge *sge, uint8_t *addr, uint32_t length, uint32_t lkey)
{
	sge->addr = (uint64_t)addr;
	sge->length = length;
	sge->lkey = lkey;
}

/**
 * Register a new buffer, add it to the mr_list and update an input sge with the mr value.
 * The method assumes that the size of the buffer is equal to eco_ctx->alignment_mem.block_size
 *
 * @param eco_ctx                    Pointer to an initialized EC context.
 * @param buffer                     Pointer to a source buffer
 * @param sge                        Pointer to a source sge.
 * @return                           0 successful, other fail.
 */
static inline int utill_mlx_eco_alloc_mr(struct eco_context *eco_ctx, uint8_t *buffer, struct ibv_sge *sge)
{
	dbg_log("utill_mlx_eco_alloc_mr: eco_ctx = %p , buffer = %p block_size = %d\n", eco_ctx, buffer, eco_ctx->alignment_mem.block_size);

	struct ibv_mr *mr;
	int block_size = eco_ctx->alignment_mem.block_size;

	mr = ibv_reg_mr(eco_ctx->calc->pd, buffer, block_size, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		err_log("utill_mlx_eco_alloc_mr: Failed to allocate data MR\n");
		return -ENOMEM;
	}

	util_mlx_eco_update_sge(sge, buffer, block_size, mr->lkey);
	eco_list_add(&eco_ctx->mrs_list, mr);

	dbg_log("utill_mlx_eco_alloc_mr: completed successfully - eco_ctx = %p , buffer = %p block_size = %d\n", eco_ctx, buffer, block_size);

	return 0;
}

/**
 * Register buffers and update the memory layout context for future encode/decode operations.
 * Because register a new memory region is a expensive method, at first we check if each sge contains the buffer data.
 * else, we check if we already register the buffer using the mr list.
 * else, we will register a mr for this buffer.
 *
 * @param eco_ctx                    Pointer to an initialized EC context.
 * @param buffers_array              Pointer to an array of input buffers
 * @param sges                       Pointer to an continuous array of sge.
 * @param num_sges                   The size of the sges array.
 * @param buffers_array_size         The size of the buffers array.
 * @return                           0 successful, other fail.
 */
static int utill_mlx_eco_alloc_mrs(struct eco_context *eco_ctx, uint8_t **buffers_array, struct ibv_sge *sges, int *num_sges, int buffers_array_size)
{
	dbg_log("utill_mlx_eco_alloc_mrs: eco_ctx = %p , buffers_array = %p block_size = %d buffers_array_size = %d\n", eco_ctx, buffers_array, eco_ctx->alignment_mem.block_size, buffers_array_size);

	int i, err;
	uint32_t block_size = eco_ctx->alignment_mem.block_size;
	uint64_t buffer_addres_u64;
	struct ibv_mr * mr;

	for (i = 0 ; i < buffers_array_size ; i++, sges++){
		buffer_addres_u64 = (uint64_t)buffers_array[i];

		// The values of the sge and the values of the buffer are equal
		if (sges->addr == buffer_addres_u64 && sges->length == block_size) {
			continue;
		}

		// The mr contains the new buffer but the parameters are not equal , no need to call reg_mr
		if ((sges->addr <= buffer_addres_u64) && (sges->addr + sges->length >= buffer_addres_u64 + block_size)){
			sges->length = block_size;
			sges->addr = buffer_addres_u64;
			continue;
		}

		// else search for the mr in the list or call reg_mr
		mr = eco_list_get_mr(&eco_ctx->mrs_list, buffers_array[i], block_size);
		if (mr) {
			util_mlx_eco_update_sge(sges, buffers_array[i], block_size, mr->lkey);
		} else {
			err = utill_mlx_eco_alloc_mr(eco_ctx, buffers_array[i], sges);
			if (err) {
				return err;
			}
		}
	}

	*num_sges = buffers_array_size;
	dbg_log("utill_mlx_eco_alloc_mrs: completed successfully - eco_ctx = %p , buffers_array = %p block_size = %d buffers_array_size = %d\n", eco_ctx, buffers_array, block_size, buffers_array_size);

	return 0;
}

/**
 * Set the comp context.
 *
 * @param coder                      Pointer to an encoder/decoder.
 * @param comp                       Pointer to the comp context.
 * @param is_remainder_comp          Boolean variable which determine the type of the completion context.
 * @param comp_done_func             Pointer to the comp_done_func.
 */
static void util_mlx_eco_set_comp(void *coder, struct eco_coder_comp *comp, int is_remainder_comp, void (*comp_done_func)(struct ibv_exp_ec_comp *))
{
	comp->comp.done = comp_done_func;
	comp->eco_coder = coder;
	comp->is_remainder_comp = is_remainder_comp;
}

/**
 * Initialize the remainder from 64 bytes mem object before allocating the calc
 *
 * @param eco_ctx                    Pointer to an encoder/decoder.
 * @param pd                         Verbs protection domain.
 * @param k                          Number of data blocks.
 * @param m                          Number of code blocks.
 */
static int util_mlx_eco_init_remainder_mem(struct eco_context *eco_ctx, struct ibv_pd *pd, int k, int m)
{
	int i, err;

	eco_ctx->remainder_buffers = calloc(k + m, 64);
	if (!eco_ctx->remainder_buffers) {
		err_log("mlx_eco_init: Failed to allocate alignment buffers\n");
		err = -ENOMEM;
		goto remainder_buffers_error;
	}
	err = util_mlx_eco_init_mem(&eco_ctx->remainder_mem, k, m);
	if (err) {
		goto init_eco_mem_error;
	}

	eco_ctx->remainder_mem.block_size = 64;

	eco_ctx->remainder_mr = ibv_reg_mr(pd, eco_ctx->remainder_buffers, (k + m) * 64, IBV_ACCESS_LOCAL_WRITE);
	if (!eco_ctx->remainder_mr) {
		err_log("mlx_eco_init: Failed to allocate alignment MR\n");
		err = -ENOMEM;
		goto remainder_mr_error;
	}


	for (i = 0 ; i < k ; i++) {
		util_mlx_eco_update_sge(&eco_ctx->remainder_mem.data_blocks[i], eco_ctx->remainder_buffers + i * 64, 64, eco_ctx->remainder_mr->lkey);
	}

	for (i = 0 ; i < m ; i++) {
		util_mlx_eco_update_sge(&eco_ctx->remainder_mem.code_blocks[i], eco_ctx->remainder_buffers + ((k + i) * 64), 64, eco_ctx->remainder_mr->lkey);
	}

	return 0;

remainder_mr_error:
	free(eco_ctx->remainder_mem.code_blocks);
	free(eco_ctx->remainder_mem.data_blocks);
init_eco_mem_error:
	free(eco_ctx->remainder_buffers);
remainder_buffers_error:

	return err;
}

struct eco_context *mlx_eco_init(void *coder, int k, int m, int use_vandermonde_matrix, void (*comp_done_func)(struct ibv_exp_ec_comp *))
{
	dbg_log("mlx_eco_init: k = %d, m = %d, use_vandermonde_matrix = %d\n", k , m, use_vandermonde_matrix);

	struct eco_context *eco_ctx;
	struct ibv_context *ibv_context;
	struct ibv_device *device;
	struct ibv_pd *pd;
	struct ibv_exp_device_attr dattr;
	uint8_t *encode_matrix;
	int err;

	// 4-bit field allows us to redundancy blocks as long as k + m <= 16
	if (k + m > W * W) {
		err_log("mlx_eco_init: 4-bit field allows us to redundancy blocks as long as k + m <= 16\n");
		return NULL;
	}

	// find device mlx5_0
	device = util_mlx_eco_find_device("mlx5_0");
	if (!device) {
		goto find_device_error;
	}

	// open device
	ibv_context = ibv_open_device(device);
	if (!ibv_context) {
		err_log("mlx_eco_init: Couldn't get context for %s\n", ibv_get_device_name(device));
		goto open_device_error;
	}

	// allocate pd
	pd = ibv_alloc_pd(ibv_context);
	if (!pd) {
		err_log("mlx_eco_init: Failed to allocate PD\n");
		goto allocate_pd_error;
	}

	// query device for EC offload capabilities.
	memset(&dattr, 0, sizeof(dattr));
	dattr.comp_mask = IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS | IBV_EXP_DEVICE_ATTR_EC_CAPS;
	if (ibv_exp_query_device(ibv_context, &dattr)) {
		err_log("mlx_eco_init: Couldn't query device for EC offload caps.\n");
		goto query_device_error;
	}

	if (!(dattr.exp_device_cap_flags & IBV_EXP_DEVICE_EC_OFFLOAD)) {
		err_log("mlx_eco_init: EC offload not supported by driver.\n");
		goto query_device_error;
	}

	dbg_log("mlx_eco_init: EC offload supported by driver.\n");
	dbg_log("mlx_eco_init: max_ec_calc_inflight_calcs %d\n", dattr.ec_caps.max_ec_calc_inflight_calcs);
	dbg_log("mlx_eco_init: max_data_vector_count %d\n", dattr.ec_caps.max_ec_data_vector_count);

	// allocate ec context
	eco_ctx = calloc(1, sizeof(*eco_ctx));
	if (!eco_ctx) {
		err_log("mlx_eco_init: Failed to allocate EC context\n");
		goto calloc_context_error;
	}
	memset(eco_ctx, 0, sizeof(*eco_ctx));

	encode_matrix = util_mlx_eco_alloc_encode_matrix(eco_ctx, k, m, use_vandermonde_matrix);
	if (!encode_matrix) {
		goto encode_matrix_error;
	}

	err = util_mlx_eco_init_remainder_mem(eco_ctx, pd, k, m);
	if (err) {
		goto init_remainder_mem_error;
	}

	// set cacl initial attributes
	eco_ctx->attr.comp_mask = IBV_EXP_EC_CALC_ATTR_MAX_INFLIGHT |
			IBV_EXP_EC_CALC_ATTR_K |
			IBV_EXP_EC_CALC_ATTR_M |
			IBV_EXP_EC_CALC_ATTR_W |
			IBV_EXP_EC_CALC_ATTR_MAX_DATA_SGE |
			IBV_EXP_EC_CALC_ATTR_MAX_CODE_SGE |
			IBV_EXP_EC_CALC_ATTR_ENCODE_MAT |
			IBV_EXP_EC_CALC_ATTR_AFFINITY |
			IBV_EXP_EC_CALC_ATTR_POLLING;
	eco_ctx->attr.max_inflight_calcs = MAX_INFLIGHT_CALCS;
	eco_ctx->attr.k = k;
	eco_ctx->attr.m = m;
	eco_ctx->attr.w = W;
	eco_ctx->attr.max_data_sge = k;
	eco_ctx->attr.max_code_sge = m;
	eco_ctx->attr.affinity_hint = 0;
	eco_ctx->attr.encode_matrix = encode_matrix;

	err = util_mlx_eco_init_mem(&eco_ctx->alignment_mem, k, m);
	if (err) {
		goto init_alignment_mem_error;
	}

	err = pthread_mutex_init(&eco_ctx->async_mutex, NULL);
	if (err) {
		err_log("mlx_eco_init: Failed to init EC async_mutex\n");
		goto async_mutex_error;
	}

	err = pthread_cond_init(&eco_ctx->async_cond, NULL);
	if (err) {
		err_log("mlx_eco_init: Failed to init EC async_cond\n");
		goto async_cond_error;
	}

	eco_ctx->calc = ibv_exp_alloc_ec_calc(pd, &eco_ctx->attr);
	if (!eco_ctx->calc) {
		err_log("mlx_eco_init: Failed to allocate EC calc\n");
		goto calc_alloc_error;
	}

	init_eco_list(&eco_ctx->mrs_list);

	eco_ctx->async_ref_count = 0;

	util_mlx_eco_set_comp(coder, &eco_ctx->alignment_comp, 0, comp_done_func);
	util_mlx_eco_set_comp(coder, &eco_ctx->remainder_comp, 1, comp_done_func);

	dbg_log("mlx_eco_init: Completed successfully - eco_ctx = %p, k = %d, m = %d, use_vandermonde_matrix = %d\n", eco_ctx, k , m, use_vandermonde_matrix);

	return eco_ctx;

calc_alloc_error:
	pthread_cond_destroy(&eco_ctx->async_cond);
async_cond_error:
	pthread_mutex_destroy(&eco_ctx->async_mutex);
async_mutex_error:
	free(eco_ctx->alignment_mem.code_blocks);
	free(eco_ctx->alignment_mem.data_blocks);
init_alignment_mem_error:
	ibv_dereg_mr(eco_ctx->remainder_mr);
	free(eco_ctx->remainder_mem.code_blocks);
	free(eco_ctx->remainder_mem.data_blocks);
	free(eco_ctx->remainder_buffers);
init_remainder_mem_error:
	free(encode_matrix);
	free(eco_ctx->int_encode_matrix);
encode_matrix_error:
	free(eco_ctx);
calloc_context_error:
query_device_error:
	ibv_dealloc_pd(pd);
allocate_pd_error:
	ibv_close_device(ibv_context);
open_device_error:
find_device_error:

	err_log("mlx_eco_init: Failed during EC initialization - k = %d, m = %d, use_vandermonde_matrix = %d\n", k , m, use_vandermonde_matrix);

	return NULL;
}

int mlx_eco_register(struct eco_context *eco_ctx, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size)
{
	dbg_log("mlx_eco_register: eco_ctx = %p , data = %p, coding = %p, block_size = %d\n", eco_ctx, data, coding, block_size);

	int err;

	if (!eco_ctx) {
		err_log("mlx_eco_register: Got invalid EC context - cannot perform register_mem\n");
		return -1;
	}

	if (block_size <= 0) {
		err_log("mlx_eco_register: Got invalid block size - %d\n", block_size);
		return -1;
	}

	eco_ctx->block_size = block_size;

	if (block_size < 64) {
		goto success;
	}

	eco_ctx->alignment_mem.block_size = block_size - (block_size % 64);

	err = utill_mlx_eco_alloc_mrs(eco_ctx, data, eco_ctx->alignment_mem.data_blocks, &eco_ctx->alignment_mem.num_data_sge, data_size);
	if (err) {
		return err;
	}

	err = utill_mlx_eco_alloc_mrs(eco_ctx, coding, eco_ctx->alignment_mem.code_blocks, &eco_ctx->alignment_mem.num_code_sge, coding_size);
	if (err) {
		return err;
	}

success:

	dbg_log("mlx_eco_register: completed successfully - eco_ctx = %p , data = %p, coding = %p, block_size = %d\n", eco_ctx, data, coding, block_size);

	return 0;
}

int mlx_eco_release(struct eco_context *eco_ctx)
{
	dbg_log("mlx_eco_release: eco_ctx = %p \n", eco_ctx);

	if (!eco_ctx) {
		err_log("mlx_eco_release: got null eco_context\n");
		return -1;
	}

	struct ibv_pd *pd = eco_ctx->calc->pd;
	struct ibv_context *ibv_context = pd->context;

	if (eco_ctx->calc) {
		ibv_exp_dealloc_ec_calc(eco_ctx->calc);
		eco_ctx->calc = NULL;
	}

	 pthread_mutex_destroy(&eco_ctx->async_mutex);
	 pthread_cond_destroy(&eco_ctx->async_cond);

	if (eco_ctx->alignment_mem.code_blocks) {
		free(eco_ctx->alignment_mem.code_blocks);
		eco_ctx->alignment_mem.code_blocks = NULL;
	}

	if (eco_ctx->alignment_mem.data_blocks) {
		free(eco_ctx->alignment_mem.data_blocks);
		eco_ctx->alignment_mem.data_blocks = NULL;
	}

	if (eco_ctx->remainder_mem.code_blocks) {
		free(eco_ctx->remainder_mem.code_blocks);
		eco_ctx->remainder_mem.code_blocks = NULL;
	}

	if (eco_ctx->remainder_mem.data_blocks) {
		free(eco_ctx->remainder_mem.data_blocks);
		eco_ctx->remainder_mem.data_blocks = NULL;
	}

	if (eco_ctx->remainder_buffers) {
		free(eco_ctx->remainder_buffers);
		eco_ctx->remainder_buffers = NULL;
	}

	if (eco_ctx->remainder_mr) {
		ibv_dereg_mr(eco_ctx->remainder_mr);
		eco_ctx->remainder_mr = NULL;
	}

	if (eco_ctx->attr.encode_matrix) {
		free(eco_ctx->attr.encode_matrix);
		eco_ctx->attr.encode_matrix = NULL;
	}

	if (eco_ctx->int_encode_matrix) {
		free(eco_ctx->int_encode_matrix);
		eco_ctx->int_encode_matrix = NULL;
	}

	eco_list_delete_all(&eco_ctx->mrs_list);

	if (pd) {
		ibv_dealloc_pd(pd);
	}

	if (pd->context) {
		ibv_close_device(ibv_context);
	}

	free(eco_ctx);

	dbg_log("mlx_eco_release: completed successfully - eco_ctx = %p \n", eco_ctx);

	eco_ctx = NULL;

	return 0;
}
