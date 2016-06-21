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

struct eco_encoder *mlx_eco_encoder_init(int k, int m, int use_vandermonde_matrix)
{
	dbg_log("mlx_eco_encoder_init: k = %d, m = %d, use_vandermonde_matrix = %d\n", k , m, use_vandermonde_matrix);

	struct eco_encoder *eco_encoder;

	eco_encoder = calloc(1, sizeof(*eco_encoder));
	if (!eco_encoder) {
		err_log("mlx_eco_encoder_init: Failed to allocate EC encoder\n");
		goto allocate_encoder_error;
	}

	eco_encoder->eco_ctx = mlx_eco_init(k, m, use_vandermonde_matrix);
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

	int err;

	if (!eco_encoder) {
		err_log("mlx_eco_encoder_encode: Got invalid EC encoder - cannot encode data\n");
		return -1;
	}

	if (data_size != eco_encoder->eco_ctx->attr.k || coding_size != eco_encoder->eco_ctx->attr.m) {
		err_log("mlx_eco_encoder_encode: Warning got different parameters then expected - got k=%d, m=%d - expected data_size=%d coding_size=%d\n", data_size, coding_size, eco_encoder->eco_ctx->attr.k, eco_encoder->eco_ctx->attr.m);
		return -1;
	}

	err = mlx_eco_register(eco_encoder->eco_ctx, data, coding, data_size, coding_size, block_size);
	if (err) {
		err_log("mlx_eco_encoder_encode: MR allocation failed\n");
		return err;
	}

	err = ibv_exp_ec_encode_sync(eco_encoder->eco_ctx->calc, &eco_encoder->eco_ctx->mem);
	if (err) {
		err_log("mlx_eco_encoder_encode: Failed ibv_exp_ec_encode (%d) %m\n", err);
		return err;
	}

	dbg_log("mlx_eco_encoder_encode: completed successfully - eco_encoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d\n", eco_encoder, block_size , data, data_size, coding, coding_size);

	return 0;
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
