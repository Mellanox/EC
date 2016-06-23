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

#include "../include/eco_encoder.h"

static inline void util_mlx_eco_encoder_prepare_remainder_data(struct eco_context *eco_context, uint8_t **data, uint8_t **coding, int remainder, int aligned_block_size)
{
	int i;
	eco_context->coding = coding;

	for (i = 0 ; i < eco_context->attr.k ; i++) {
		memcpy((void *)eco_context->remainder_mem.data_blocks[i].addr, data[i] + aligned_block_size, remainder);
	}
}

static void util_mlx_eco_encoder_comp_done(struct ibv_exp_ec_comp *comp)
{
	struct eco_coder_comp *coder_comp = (void *)comp - offsetof(struct eco_coder_comp, comp);
	struct eco_context *eco_context = ((struct eco_encoder *)coder_comp->eco_coder)->eco_ctx;
	int i, remainder = eco_context->block_size % 64, aligned_block_size = eco_context->block_size - remainder;

	if (coder_comp->is_remainder_comp) {
		for (i = 0 ; i < eco_context->attr.m ; i++) {
			memcpy(eco_context->coding[i] + aligned_block_size, (void *)eco_context->remainder_mem.code_blocks[i].addr, remainder);
		}
	}

	pthread_mutex_lock(&eco_context->async_mutex);

	if (!--eco_context->async_ref_count) {
		pthread_cond_signal(&eco_context->async_cond);
	}

	pthread_mutex_unlock(&eco_context->async_mutex);
}

struct eco_encoder *mlx_eco_encoder_init(int k, int m, int use_vandermonde_matrix)
{
	dbg_log("mlx_eco_encoder_init: k = %d, m = %d, use_vandermonde_matrix = %d\n", k , m, use_vandermonde_matrix);

	struct eco_encoder *eco_encoder;

	eco_encoder = calloc(1, sizeof(*eco_encoder));
	if (!eco_encoder) {
		err_log("mlx_eco_encoder_init: Failed to allocate EC encoder\n");
		goto allocate_encoder_error;
	}

	eco_encoder->eco_ctx = mlx_eco_init(eco_encoder, k, m, use_vandermonde_matrix, util_mlx_eco_encoder_comp_done);
	if (!eco_encoder->eco_ctx) {
		err_log("mlx_eco_encoder_init: Failed to initialize eco_encoder\n");
		goto encoder_initialize_error;
	}

	dbg_log("mlx_eco_encoder_init: Completed successfully - eco_ctx = %p, k = %d, m = %d, use_vandermonde_matrix = %d\n", eco_encoder, k , m, use_vandermonde_matrix);

	return eco_encoder;

encoder_initialize_error:
	free(eco_encoder);
allocate_encoder_error:

	dbg_log("mlx_eco_encoder_init: Failed - k = %d, m = %d, use_vandermonde_matrix = %d\n", k , m, use_vandermonde_matrix);

	return NULL;
}

int mlx_eco_encoder_register(struct eco_encoder *eco_encoder, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size)
{
	dbg_log("mlx_eco_encoder_register: eco_encoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d\n", eco_encoder, block_size , data, data_size, coding, coding_size);

	int err;
	err = mlx_eco_register(eco_encoder->eco_ctx, data, coding, data_size, coding_size, block_size);

	dbg_log("mlx_eco_encoder_register: completed with result = %d, eco_encoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d\n", err, eco_encoder, block_size , data, data_size, coding, coding_size);

	return err;
}

int mlx_eco_encoder_encode(struct eco_encoder *eco_encoder, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size)
{
	dbg_log("mlx_eco_encoder_encode: eco_encoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d\n", eco_encoder, block_size , data, data_size, coding, coding_size);

	struct eco_context *eco_context;
	int err, remainder = block_size % 64, aligned_block_size = block_size - remainder;

	if (!eco_encoder) {
		err_log("mlx_eco_encoder_encode: Got invalid EC encoder - cannot encode data\n");
		return -1;
	}

	eco_context = eco_encoder->eco_ctx;

	if (data_size != eco_context->attr.k || coding_size != eco_context->attr.m) {
		err_log("mlx_eco_encoder_encode: Warning got different parameters then expected - got k=%d, m=%d - expected data_size=%d coding_size=%d\n", data_size, coding_size, eco_context->attr.k, eco_context->attr.m);
		return -1;
	}

	err = mlx_eco_register(eco_context, data, coding, data_size, coding_size, block_size);
	if (err) {
		err_log("mlx_eco_encoder_encode: MR allocation failed\n");
		return err;
	}

	pthread_mutex_lock(&eco_context->async_mutex);

	if (remainder) {
		util_mlx_eco_encoder_prepare_remainder_data(eco_context, data, coding, remainder, aligned_block_size);
		err = ibv_exp_ec_encode_async(eco_context->calc, &eco_context->remainder_mem, &eco_context->remainder_comp.comp);
		if (err) {
			goto encode_error;
		}
		eco_context->async_ref_count++;
	}

	if (aligned_block_size) {
		err = ibv_exp_ec_encode_async(eco_context->calc, &eco_context->alignment_mem, &eco_context->alignment_comp.comp);
		if (err) {
			goto encode_error;
		}
		eco_context->async_ref_count++;
	}

	pthread_cond_wait(&eco_context->async_cond, &eco_context->async_mutex);
	pthread_mutex_unlock(&eco_context->async_mutex);

	if ((err = (int)eco_context->alignment_comp.comp.status | (int)eco_context->remainder_comp.comp.status)) {
		goto encode_error;
	}

	dbg_log("mlx_eco_encoder_encode: completed successfully - eco_encoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d\n", eco_encoder, block_size , data, data_size, coding, coding_size);

	return 0;

encode_error:

	if (eco_context->async_ref_count) {
		pthread_cond_wait(&eco_context->async_cond, &eco_context->async_mutex);
	}
	pthread_mutex_unlock(&eco_context->async_mutex);

	err_log("mlx_eco_encoder_encode: Failed ibv_exp_ec_encode (%d) %m\n", err);
	return err;
}

int mlx_eco_encoder_release(struct eco_encoder *eco_encoder)
{
	dbg_log("mlx_eco_encoder_release: eco_encoder = %p\n", eco_encoder);

	int err = 0;

	if(!eco_encoder) {
		err_log("mlx_eco_encoder_release: got null eco_encoder\n");
		return -1;
	}

	if(eco_encoder->eco_ctx) {
		err = mlx_eco_release(eco_encoder->eco_ctx);
		eco_encoder->eco_ctx = NULL;
	}

	free(eco_encoder);

	dbg_log("mlx_eco_encoder_release: completed with result = %d, eco_encoder = %p\n", err, eco_encoder);

	eco_encoder = NULL;

	return err;
}
