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

#include "../include/eco_decoder.h"

#define LOG_TABLE 0, 1, 4, 2, 8, 5, 10, 3, 14, 9, 7, 6, 13, 11, 12
#define ILOG_TABLE 1, 2, 4, 8, 3, 6, 12, 11, 5, 10, 7, 14, 15, 13, 9

const uint8_t gf_w4_log[]={LOG_TABLE};
const uint8_t gf_w4_ilog[]={ILOG_TABLE};

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

static uint8_t util_mlx_eco_gf_w4_mul(uint8_t x, uint8_t y)
{
	int log_x, log_y, log_r;

	if (!x || !y)
		return 0;

	log_x = gf_w4_log[x - 1];
	log_y = gf_w4_log[y - 1];
	log_r = (log_x + log_y) % 15;

	return gf_w4_ilog[log_r];
}

static uint8_t util_mlx_eco_galois_w4_mult(uint8_t x, uint8_t y4)
{
	uint8_t r_h, r_l;

	r_h = util_mlx_eco_gf_w4_mul(x >> 4, y4 & 0xf);
	r_l = util_mlx_eco_gf_w4_mul(x & 0xf, y4 & 0xf);

	return (r_h << 4) | r_l;
}

static int util_mlx_eco_should_update_decode_matrix(struct eco_decoder *eco_decoder, int *erasures, int erasures_size)
{
	uint32_t input_erasures = 0, last_erasures = 0;
	int i, total_blocks = eco_decoder->eco_ctx->attr.k + eco_decoder->eco_ctx->attr.m;

	for (i = 0 ; i < erasures_size ; i++) {
		input_erasures |= (1 << erasures[i]);
	}

	for (i = 0 ; i < total_blocks ; i++) {
		last_erasures |= eco_decoder->int_erasures[i] << i;
	}

	return input_erasures != last_erasures ? 1 : 0;
}

static int util_mlx_eco_extract_erasures(struct eco_decoder *eco_decoder, int *erasures, int erasures_size)
{
    dbg_log("util_mlx_eco_extract_erasures: eco_decoder = %p , erasures = %p erasures_size = %d\n", eco_decoder, erasures, erasures_size);

	int i, total_blocks = eco_decoder->eco_ctx->attr.k + eco_decoder->eco_ctx->attr.m;

	memset(eco_decoder->int_erasures, 0, sizeof(int) * total_blocks);
	memset(eco_decoder->u8_erasures, 0, sizeof(uint8_t) * total_blocks);

	for (i = 0 ; i < erasures_size ; i++) {
		eco_decoder->int_erasures[erasures[i]] = 1;
		eco_decoder->u8_erasures[erasures[i]] = 1;
	}

	dbg_log("util_mlx_eco_extract_erasures: erasures: [");
	for (i = 0; i < total_blocks; i++)
		dbg_log(" %d ", eco_decoder->int_erasures[i]);
	dbg_log("]\n");

	dbg_log("util_mlx_eco_extract_erasures completed successfully: ! eco_decoder = %p , erasures = %p erasures_size = %d\n", eco_decoder, erasures, erasures_size);

	return 0;
}

static int util_mlx_eco_create_decode_matrix(struct eco_decoder *eco_decoder, int *erasures_arr, int num_erasures)
{
	int i, j, p, s, l = 0, data_erasures = 0, k = eco_decoder->eco_ctx->attr.k, m = eco_decoder->eco_ctx->attr.m;
	int err;

	dbg_log("util_mlx_eco_create_decode_matrix: ! eco_decoder = %p , num_erasures = %d\n", eco_decoder, num_erasures);

	for (i = 0; i < k; i++) {
		if (eco_decoder->int_erasures[i])
			data_erasures++;
	}

	memset(eco_decoder->int_decode_matrix, 0, sizeof(int) * k * k);
	memset(eco_decoder->survived, 0, sizeof(int) * (k + m));
	memset(eco_decoder->u8_decode_matrix, 0, m * k);

	err = jerasure_make_decoding_matrix(k, data_erasures, W, eco_decoder->eco_ctx->int_encode_matrix, eco_decoder->int_erasures, eco_decoder->int_decode_matrix, eco_decoder->survived);
	if (err) {
		err_log("util_mlx_eco_create_decode_matrix: Jerasure failed making decoding matrix\n");
		return -1;
	}

	for (i = 0; i < k; i++) {
		if (eco_decoder->int_erasures[i]) {
			for (j = 0; j < k; j++)
				eco_decoder->u8_decode_matrix[j*num_erasures+l] = (uint8_t)eco_decoder->int_decode_matrix[i*k+j];
			l++;
		}
	}

	for (p = l; p < num_erasures; p++) {
		for (i = 0; i < k; i++) {
			s = 0;
			for (j = 0; j < k; j++) {
				s ^= util_mlx_eco_galois_w4_mult(eco_decoder->int_decode_matrix[j * k + i], eco_decoder->eco_ctx->int_encode_matrix[k * (erasures_arr[p] - k) + j]);
			}
			eco_decoder->u8_decode_matrix[i*num_erasures+l] = (uint8_t)s;
		}
		l++;
	}

	util_mlx_eco_print_matrix_u8(eco_decoder->u8_decode_matrix, k, l);

	dbg_log("util_mlx_eco_create_decode_matrix completed successfully: ! eco_decoder = %p , num_erasures = %d\n", eco_decoder, num_erasures);

	return 0;
}

struct eco_decoder *mlx_eco_decoder_init(int k, int m, int use_vandermonde_matrix)
{
	dbg_log("mlx_eco_decoder_init: k = %d, m = %d, use_vandermonde_matrix = %d\n", k , m, use_vandermonde_matrix);

	struct eco_decoder *eco_decoder;

	eco_decoder = calloc(1, sizeof(*eco_decoder));
	if (!eco_decoder) {
		err_log("mlx_eco_decoder_init: Failed to allocate EC decoder\n");
		goto allocate_decoder_error;
	}

	eco_decoder->int_decode_matrix = calloc(k * k, sizeof(int));
	if (!eco_decoder->int_decode_matrix) {
		err_log("mlx_eco_decoder_init: Failed to allocate int_decode_matrix\n");
		goto allocate_int_decode_matrix_error;
	}

	eco_decoder->u8_decode_matrix = calloc(m * k, sizeof(uint8_t));
	if (!eco_decoder->u8_decode_matrix) {
		err_log("mlx_eco_decoder_init: Failed to allocate u8_decode_matrix\n");
		goto allocate_u8_decode_matrix_error;
	}

	eco_decoder->int_erasures = calloc(k + m, sizeof(int));
	if (!eco_decoder->int_erasures) {
		err_log("mlx_eco_decoder_init: Failed to allocated int_erasures buffer\n");
		goto allocate_int_erasures_error;
	}

	eco_decoder->u8_erasures = calloc(k + m, sizeof(uint8_t));
	if (!eco_decoder->u8_erasures) {
		err_log("mlx_eco_decoder_init: Failed to allocated u8_erasures buffer\n");
		goto allocate_u8_erasures_error;
	}

	eco_decoder->survived = calloc(k + m, sizeof(int));
	if (!eco_decoder->survived) {
		err_log("failed to allocated survived buffer\n");
		goto allocate_survived_error;
	}

	eco_decoder->eco_ctx = mlx_eco_init(k, m, use_vandermonde_matrix);
	if (!eco_decoder->eco_ctx) {
		err_log("mlx_eco_decoder_init: Failed to initialize eco_decoder\n");
		goto decoder_initialize_error;
	}

	dbg_log("mlx_eco_decoder_init: Completed successfully - eco_ctx = %p, k = %d, m = %d, use_vandermonde_matrix = %d\n", eco_decoder, k , m, use_vandermonde_matrix);

	return eco_decoder;

decoder_initialize_error:
	free(eco_decoder->survived);
allocate_survived_error:
	free(eco_decoder->u8_erasures);
allocate_u8_erasures_error:
	free(eco_decoder->int_erasures);
allocate_int_erasures_error:
	free(eco_decoder->u8_decode_matrix);
allocate_u8_decode_matrix_error:
	free(eco_decoder->int_decode_matrix);
allocate_int_decode_matrix_error:
	free(eco_decoder);
allocate_decoder_error:

	dbg_log("mlx_eco_decoder_init: Failed - k = %d, m = %d, use_vandermonde_matrix = %d\n", k , m, use_vandermonde_matrix);

	return NULL;
}

int mlx_eco_decoder_register(struct eco_decoder *eco_decoder, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size)
{
	dbg_log("mlx_eco_decoder_register: eco_decoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d\n", eco_decoder, block_size , data, data_size, coding, coding_size);

	int err;
	err = mlx_eco_register(eco_decoder->eco_ctx, data, coding, data_size, coding_size, block_size);

	dbg_log("mlx_eco_decoder_register: completed with result = %d, eco_decoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d\n", err, eco_decoder, block_size , data, data_size, coding, coding_size);

	return err;
}

int mlx_eco_decoder_generate_decode_matrix(struct eco_decoder *eco_decoder, int *erasures, int erasures_size)
{
	dbg_log("mlx_eco_decoder_generate_decode_matrix: eco_decoder = %p , erasures = %p, erasures_size = %d\n", eco_decoder, erasures, erasures_size);

	if (!eco_decoder) {
		err_log("mlx_eco_decoder_generate_decode_matrix: got null eco_decoder\n");
		return -1;
	}

	if (util_mlx_eco_should_update_decode_matrix(eco_decoder, erasures, erasures_size)) {
		util_mlx_eco_extract_erasures(eco_decoder, erasures, erasures_size);
		util_mlx_eco_create_decode_matrix(eco_decoder, erasures, erasures_size);
	}

	dbg_log("mlx_eco_decoder_generate_decode_matrix: completed successfully: eco_decoder = %p , erasures = %p, erasures_size = %d\n", eco_decoder, erasures, erasures_size);

	return 0;
}

int mlx_eco_decoder_decode(struct eco_decoder *eco_decoder, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size, int *erasures, int erasures_size)
{
	dbg_log("mlx_eco_decoder_decode: eco_decoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d, erasures = %p, erasures_size = %d\n", eco_decoder, block_size , data, data_size, coding, coding_size, erasures, erasures_size);

	int err;

	if (!eco_decoder) {
		err_log("mlx_eco_decoder_decode: Got invalid EC decoder - cannot decode data\n");
		return -1;
	}

	if (data_size != eco_decoder->eco_ctx->attr.k || coding_size != eco_decoder->eco_ctx->attr.m) {
		err_log("mlx_eco_decoder_decode: Warning got different parameters then expected - got k=%d, m=%d - expected data_size=%d coding_size=%d\n", data_size, coding_size, eco_decoder->eco_ctx->attr.k, eco_decoder->eco_ctx->attr.m);
		return -1;
	}

	err = mlx_eco_decoder_generate_decode_matrix(eco_decoder, erasures, erasures_size);
	if (err) {
		err_log("mlx_eco_decoder_decode: generate decode matrix failed\n");
		return err;
	}

	err = mlx_eco_register(eco_decoder->eco_ctx, data, coding, data_size, coding_size, block_size);
	if (err) {
		err_log("mlx_eco_decoder_decode: MR allocation failed\n");
		return err;
	}

	err = ibv_exp_ec_decode_sync(eco_decoder->eco_ctx->calc, &eco_decoder->eco_ctx->mem, eco_decoder->u8_erasures, eco_decoder->u8_decode_matrix);
	if (err) {
		err_log("mlx_eco_decoder_decode: Failed ibv_exp_ec_decode (%d) %m\n", err);
		return err;
	}

	dbg_log("mlx_eco_decoder_decode: completed successfully - eco_decoder = %p , block_size = %d, data = %p, data_size = %d, coding = %p, coding_size = %d, erasures = %p, erasures_size = %d\n", eco_decoder, block_size , data, data_size, coding, coding_size, erasures, erasures_size);

	return 0;
}

int mlx_eco_decoder_release(struct eco_decoder *eco_decoder)
{
	dbg_log("mlx_eco_decoder_release: eco_decoder = %p\n", eco_decoder);

	int err = 0;

	if(!eco_decoder) {
		err_log("mlx_eco_decoder_release: got null eco_decoder\n");
		return -1;
	}

	if(eco_decoder->eco_ctx) {
		err = mlx_eco_release(eco_decoder->eco_ctx);
		eco_decoder->eco_ctx = NULL;
	}

	if(eco_decoder->survived) {
		free(eco_decoder->survived);
		eco_decoder->survived = NULL;
	}

	if(eco_decoder->u8_erasures) {
		free(eco_decoder->u8_erasures);
		eco_decoder->u8_erasures = NULL;
	}

	if(eco_decoder->int_erasures) {
		free(eco_decoder->int_erasures);
		eco_decoder->int_erasures = NULL;
	}

	if (eco_decoder->u8_decode_matrix) {
		free(eco_decoder->u8_decode_matrix);
		eco_decoder->u8_decode_matrix = NULL;
	}

	if (eco_decoder->int_decode_matrix) {
		free(eco_decoder->int_decode_matrix);
		eco_decoder->int_decode_matrix = NULL;
	}

	free(eco_decoder);

	dbg_log("mlx_eco_decoder_release: completed with result = %d, eco_decoder = %p\n", err, eco_decoder);

	eco_decoder = NULL;

	return err;
}
