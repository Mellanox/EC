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

#include "ec_utils.h"
#include <ecOffload/eco_encoder.h>
#include "com_mellanox_erasurecode_rawcoder_MellanoxRSRawEncoder.h"

struct mlx_encoder {
	struct eco_encoder *encoder_ctx;
	struct mlx_coder_data encoder_data;
};

JNIEXPORT void JNICALL
Java_com_mellanox_erasurecode_rawcoder_MellanoxRSRawEncoder_initImpl(
		JNIEnv *env, jobject thiz, jint numDataUnits, jint numParityUnits){
	struct mlx_encoder *mlx_encoder;
	int err;

	mlx_encoder = calloc(1, sizeof(*mlx_encoder));
	if (!mlx_encoder) {
		THROW(env, "java/lang/InternalError", "Failed to allocate encoder context");
	}

	err = allocate_coder_data(&mlx_encoder->encoder_data, numDataUnits, numParityUnits);
	if (err) {
		THROW(env, "java/lang/InternalError", "Failed to initialization encoder data");
	}

	mlx_encoder->encoder_ctx = mlx_eco_encoder_init(numDataUnits, numParityUnits, USE_VANDERMONDE_MATRIX);
	if (!mlx_encoder->encoder_ctx) {
		free_coder_data(&mlx_encoder->encoder_data);
		THROW(env, "java/lang/InternalError", "Encoder initialization failed");
	}

	set_context(env, thiz, (void *)mlx_encoder);
}

JNIEXPORT void JNICALL
Java_com_mellanox_erasurecode_rawcoder_MellanoxRSRawEncoder_encodeImpl(
		JNIEnv *env, jobject thiz, jobjectArray inputs, jintArray inputOffsets,
		jint dataLen, jobjectArray outputs, jintArray outputOffsets) {
	struct mlx_encoder *mlx_encoder = (struct mlx_encoder*) get_context(env, thiz);
	struct mlx_coder_data *encoder_data = &mlx_encoder->encoder_data;
	int err;

	encoder_get_buffers(env, inputs, inputOffsets, outputs, outputOffsets, encoder_data);
	PASS_EXCEPTIONS(env);

	err = mlx_eco_encoder_encode(mlx_encoder->encoder_ctx, encoder_data->data, encoder_data->coding,
			encoder_data->data_size, encoder_data->coding_size, dataLen);
	if (err) {
		THROW(env, "java/lang/InternalError", "Got error during encode");
	}
}

JNIEXPORT void JNICALL
Java_com_mellanox_erasurecode_rawcoder_MellanoxRSRawEncoder_destroyImpl(
		JNIEnv *env, jobject thiz) {
	struct mlx_encoder *mlx_encoder = (struct mlx_encoder*) get_context(env, thiz);
	mlx_eco_encoder_release(mlx_encoder->encoder_ctx);
	free_coder_data(&mlx_encoder->encoder_data);
	free(mlx_encoder);
	set_context(env, thiz, NULL);
}
