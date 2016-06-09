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

#include "ec_common.h"

void print_matrix_u8(uint8_t *m, int rows, int cols)
{
	int i, j;

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			if (j != 0)
				info_log(" ");

			info_log("%#x  ", m[i*cols+j]);
		}
		info_log("\n");
	}
}

void print_matrix_int(int *m, int rows, int cols)
{
	int i, j;

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			if (j != 0)
				info_log(" ");

			info_log("%#x  ", m[i*cols+j]);
		}
		info_log("\n");
	}
}

void free_encode_matrix(struct ec_context *ctx)
{
	free(ctx->en_mat);
}

int alloc_encode_matrix(struct ec_context *ctx)
{
	uint8_t *matrix;
	int *rs_mat;
	int i, j, k, m, w;

	k = ctx->attr.k;
	m = ctx->attr.m;
	w = ctx->attr.w;

	matrix = calloc(1, m * k);
	if (!matrix) {
		err_log("Failed to allocate encode matrix\n");
		return -ENOMEM;
	}

	rs_mat = reed_sol_vandermonde_coding_matrix(k, m, w);
	if (!rs_mat) {
		err_log("failed to allocate reed sol matrix\n");
		return -EINVAL;
	}

	for (i = 0; i < m; i++)
		for (j = 0; j < k; j++)
			matrix[j*m+i] = (uint8_t)rs_mat[i*k+j];
	print_matrix_u8(matrix, k, m);

	ctx->en_mat = matrix;
	ctx->encode_matrix = rs_mat;

	return 0;
}

int extract_erasures(char *failed_blocks, struct ec_context *ctx)
{
	char *pt;
	int i = 0, tot = 0;

	ctx->int_erasures = calloc(ctx->attr.k + ctx->attr.m, sizeof(int));
	if (!ctx->int_erasures) {
		err_log("failed to allocated int_erasures buffer\n");
		return -ENOMEM;
	}

        ctx->u8_erasures = calloc(ctx->attr.k + ctx->attr.m, sizeof(uint8_t));
        if (!ctx->u8_erasures) {
                err_log("failed to allocated u8_erasures buffer\n");
                return -ENOMEM;
        }

	ctx->survived_arr = calloc(ctx->attr.k + ctx->attr.m, sizeof(int));
	if (!ctx->survived_arr) {
		err_log("failed to allocated survived buffer\n");
		goto err_erasures;
	}

	pt = strtok (failed_blocks, ",");
	while (pt != NULL) {
		if (i >= ctx->attr.k + ctx->attr.m) {
			err_log("too many data nodes blocks given %d\n", i);
			return -EINVAL;
		}

		if (pt[0] == '1') {
			ctx->int_erasures[i] = 1;
			ctx->u8_erasures[i] = 1;
			if (++tot > ctx->attr.m) {
				err_log("too much erasures %d\n", tot);
				goto err_survived;

			}
		} else {
			ctx->survived_arr[i] = 1;
			ctx->survived |= (1 << i);
		}
		pt = strtok (NULL, ",");
		i++;
	}

	for (i = 0; i < ctx->attr.k + ctx->attr.m; i++) {
		if (ctx->int_erasures[i] == 0) {
			ctx->survived_arr[i] = 1;
			ctx->survived |= (1 << i);
		}
	}

	info_log("erasures: ");
	for (i = 0; i < ctx->attr.k + ctx->attr.m; i++)
		info_log("[%d]: %d ", i, ctx->int_erasures[i]);
	info_log("\n");

	info_log("survived: ");
	for (i = 0; i < ctx->attr.k + ctx->attr.m; i++)
		info_log("[%d]: %d ", i, ctx->survived_arr[i]);
	info_log("bitmap: 0x%x\n", ctx->survived);

	return 0;

err_survived:
	free(ctx->survived_arr);
err_erasures:
	free(ctx->int_erasures);
	free(ctx->u8_erasures);

	return -ENOMEM;
}

void free_decode_matrix(struct ec_context *ctx)
{
	free(ctx->de_mat);
}

int alloc_decode_matrix(struct ec_context *ctx)
{
	int *encode_matrix = ctx->encode_matrix;
	int *dec_mat;
	uint8_t *dematrix;
	int i, j, l = 0, k, m = 0, w;
	int err;

	k = ctx->attr.k;
	w = ctx->attr.w;
	for (i = 0; i < ctx->attr.k + ctx->attr.m; i++) {
		if (ctx->int_erasures[i])
			m++;
	}

	dematrix = calloc(m * k, 1);
	if (!dematrix) {
		err_log("Failed to allocate decode matrix\n");
		return -ENOMEM;
	}

	dec_mat = calloc(k * k, sizeof(int));
	if (!dec_mat) {
		err_log("Failed to allocate dec_mat\n");
		err = -ENOMEM;
		goto err_demat;
	}

	err = jerasure_make_decoding_matrix(k, m, w, encode_matrix,
					    ctx->int_erasures,
					    dec_mat, ctx->survived_arr);
	if (err) {
		err_log("failed making decoding matrix\n");
		goto err_decmat;
	}

	for (i = 0; i < k + m; i++) {
		if (ctx->int_erasures[i]) {
			for (j = 0; j < k; j++)
				dematrix[j*m+l] = (uint8_t)dec_mat[i*k+j];
			l++;
		}
	}
	print_matrix_u8(dematrix, k, l);
	ctx->de_mat = dematrix;

	return 0;

err_decmat:
	free(dec_mat);
err_demat:
	free(dematrix);

	return err;
}

static void free_ec_mrs(struct ec_context *ctx)
{
	ibv_dereg_mr(ctx->data.mr);
	ibv_dereg_mr(ctx->code.mr);
	free(ctx->data.buf);
	free(ctx->code.buf);
}

static int alloc_ec_mrs(struct ec_context *ctx)
{
	int dsize, csize, i;

	dsize = ctx->block_size * ctx->attr.k;
	csize = ctx->block_size * ctx->attr.m;
	info_log("data_size=%d, code_size=%d block_size=%d\n",
		dsize, csize, ctx->block_size);

	ctx->data.buf = calloc(1, dsize);
	if (!ctx->data.buf) {
		err_log("Failed to allocate data buffer\n");
		return -ENOMEM;
	}

	ctx->data.mr = ibv_reg_mr(ctx->pd, ctx->data.buf,
				  dsize, IBV_ACCESS_LOCAL_WRITE);
	if (!ctx->data.mr) {
		err_log("Failed to allocate data MR\n");
		goto free_dbuf;
	}

	ctx->data.sge = calloc(ctx->attr.k, sizeof(*ctx->data.sge));
	if (!ctx->data.sge) {
		err_log("Failed to allocate data sges\n");
		goto free_dbuf;
	}

	for (i = 0; i < ctx->attr.k; i++) {
		ctx->data.sge[i].lkey = ctx->data.mr->lkey;
		ctx->data.sge[i].addr = (uintptr_t)ctx->data.buf + i * ctx->block_size;
		ctx->data.sge[i].length = ctx->block_size;
	}

	ctx->code.buf = calloc(1, csize);
	if (!ctx->code.buf) {
		err_log("Failed to allocate code buffer\n");
		goto dereg_dmr;
	}

	ctx->code.mr = ibv_reg_mr(ctx->pd, ctx->code.buf, csize,
				  IBV_ACCESS_LOCAL_WRITE);
	if (!ctx->code.mr) {
		err_log("Failed to allocate code MR\n");
		goto free_cbuf;
	}

	ctx->code.sge = calloc(ctx->attr.m, sizeof(*ctx->code.sge));
	if (!ctx->code.sge) {
		err_log("Failed to allocate code sges\n");
		goto free_dbuf;
	}

	for (i = 0; i < ctx->attr.m; i++) {
		ctx->code.sge[i].lkey = ctx->code.mr->lkey;
		ctx->code.sge[i].addr = (uintptr_t)ctx->code.buf + i * ctx->block_size;
		ctx->code.sge[i].length = ctx->block_size;
	}

	ctx->mem.data_blocks = ctx->data.sge;
	ctx->mem.num_data_sge = ctx->attr.k;
	ctx->mem.code_blocks = ctx->code.sge;
	ctx->mem.num_code_sge = ctx->attr.m;
	ctx->mem.block_size = ctx->block_size;

	return 0;

free_dbuf:
	free(ctx->data.buf);
dereg_dmr:
	ibv_dereg_mr(ctx->data.mr);
free_cbuf:
	free(ctx->code.buf);

	return -ENOMEM;
}

struct ec_context *alloc_ec_ctx(struct ibv_pd *pd, int frame_size,
				int k, int m, int w,
				int max_inflight_calcs,
				char *failed_blocks)
{
	struct ec_context *ctx;
	struct ibv_exp_device_attr dattr;
	int err;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		err_log("Failed to allocate EC context\n");
		return NULL;
	}

	ctx->pd = pd;
	ctx->context = pd->context;

	memset(&dattr, 0, sizeof(dattr));
	dattr.comp_mask = IBV_EXP_DEVICE_ATTR_EC_CAPS;
	err = ibv_exp_query_device(ctx->context, &dattr);
	if (err) {
		err_log("Couldn't query device for EC offload caps.\n");
		goto free_ctx;
	}

	if (dattr.exp_device_cap_flags & IBV_EXP_DEVICE_EC_OFFLOAD) {
		err_log("EC offload not supported by driver.\n");
		goto free_ctx;
	}

	info_log("EC offload supported by driver.\n");
	info_log("max_ec_calc_inflight_calcs %d\n", dattr.ec_caps.max_ec_calc_inflight_calcs);
	info_log("max_data_vector_count %d\n", dattr.ec_caps.max_ec_data_vector_count);

	ctx->attr.comp_mask = IBV_EXP_EC_CALC_ATTR_MAX_INFLIGHT |
			IBV_EXP_EC_CALC_ATTR_K |
			IBV_EXP_EC_CALC_ATTR_M |
			IBV_EXP_EC_CALC_ATTR_W |
			IBV_EXP_EC_CALC_ATTR_MAX_DATA_SGE |
			IBV_EXP_EC_CALC_ATTR_MAX_CODE_SGE |
			IBV_EXP_EC_CALC_ATTR_ENCODE_MAT |
			IBV_EXP_EC_CALC_ATTR_AFFINITY |
			IBV_EXP_EC_CALC_ATTR_POLLING;
	ctx->attr.max_inflight_calcs = max_inflight_calcs;
	ctx->attr.k = k;
	ctx->attr.m = m; 
	ctx->attr.w = w;
	ctx->attr.max_data_sge = k;
	ctx->attr.max_code_sge = m;
	ctx->attr.affinity_hint = 0;
	ctx->block_size = align_any((frame_size + ctx->attr.k - 1) / ctx->attr.k, 64);

	err = alloc_ec_mrs(ctx);
	if (err)
		goto free_ctx;

	err = alloc_encode_matrix(ctx);
	if (err)
		goto free_mrs;

	ctx->attr.encode_matrix = ctx->en_mat;

	if (failed_blocks) {
		if (extract_erasures(failed_blocks, ctx))
			goto free_mrs;

		err = alloc_decode_matrix(ctx);
		if (err)
			goto clean_encode_mat;
	}

	ctx->calc = ibv_exp_alloc_ec_calc(ctx->pd, &ctx->attr);
	if (!ctx->calc) {
		err_log("Failed to allocate EC calc\n");
		goto clean_decode_mat;
	}

	return ctx;

clean_decode_mat:
	free_decode_matrix(ctx);
clean_encode_mat:
	free_encode_matrix(ctx);
free_mrs:
	free_ec_mrs(ctx);
free_ctx:
	free(ctx);

	return NULL;
}

void free_ec_ctx(struct ec_context *ctx)
{
	ibv_exp_dealloc_ec_calc(ctx->calc);
	free_ec_mrs(ctx);
	free(ctx->encode_matrix);
	free(ctx->attr.encode_matrix);
	free(ctx);
}

#define LOG_TABLE 0, 1, 4, 2, 8, 5, 10, 3, 14, 9, 7, 6, 13, 11, 12
#define ILOG_TABLE 1, 2, 4, 8, 3, 6, 12, 11, 5, 10, 7, 14, 15, 13, 9

const uint8_t gf_w4_log[]={LOG_TABLE};
const uint8_t gf_w4_ilog[]={ILOG_TABLE};

uint8_t gf_w4_mul(uint8_t x, uint8_t y)
{
        int log_x, log_y, log_r;

        if (!x || !y)
                return 0;

        log_x = gf_w4_log[x - 1];
        log_y = gf_w4_log[y - 1];
        log_r = (log_x + log_y) % 15;

        return gf_w4_ilog[log_r];
}

uint8_t galois_w4_mult(uint8_t x, uint8_t y4)
{
        uint8_t r_h, r_l;

        r_h = gf_w4_mul(x >> 4, y4 & 0xf);
        r_l = gf_w4_mul(x & 0xf, y4 & 0xf);

        return (r_h << 4) | r_l;

}

int sw_ec_encode(struct ec_context *ctx)
{
	uint8_t *data = (uint8_t *)ctx->data.buf;
	uint8_t *code = (uint8_t *)ctx->code.buf;
	uint8_t *matrix = (uint8_t *)ctx->attr.encode_matrix;
	int block_size = ctx->block_size, index, offset;
	int i, j, m = ctx->attr.m;

	for (i = 0; i < block_size * ctx->attr.k; i++) {
		index = i / block_size;
		offset = i % block_size;

		for (j = 0; j < m; j++)
			code[block_size*j + offset] ^=
			galois_w4_mult(data[i], matrix[index*m+j]);
	}

	return 0;
}

struct ibv_device *find_device(const char *devname)
{
	struct ibv_device **dev_list = NULL;
	struct ibv_device *device = NULL;

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		err_log("Failed to get IB devices list");
		return NULL;
	}

	if (!devname) {
		device = *dev_list;
		if (!device)
			err_log("No IB devices found\n");
	} else {
		int i;

		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]),
				    devname))
				break;
		device = dev_list[i];
		if (!device)
			err_log("IB device %s not found\n", devname);
	}

	ibv_free_device_list(dev_list);

	return device;
}
