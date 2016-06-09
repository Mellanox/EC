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
#include <ecOffload/eco_decoder.h>
#include "com_mellanox_erasurecode_rawcoder_MellanoxRSRawDecoder.h"

struct mlx_decoder {
	struct eco_decoder *decoder_ctx;
	struct mlx_coder_data decoder_data;
};

JNIEXPORT void JNICALL
Java_com_mellanox_erasurecode_rawcoder_MellanoxRSRawDecoder_initImpl(
JNIEnv *env, jobject thiz, jint numDataUnits, jint numParityUnits) {
	struct mlx_decoder *mlx_decoder;
	int err;

	mlx_decoder = calloc(1, sizeof(*mlx_decoder));
	if (!mlx_decoder) {
		THROW(env, "java/lang/InternalError", "Failed to allocate decoder context");
	}

	err = allocate_coder_data(&mlx_decoder->decoder_data, numDataUnits, numParityUnits);
	if (err) {
		THROW(env, "java/lang/InternalError", "Failed to initialization decoder_input_data");
	}

	mlx_decoder->decoder_ctx = mlx_eco_decoder_init(numDataUnits, numParityUnits, USE_VANDERMONDE_MATRIX);
	if (!mlx_decoder->decoder_ctx) {
		free_coder_data(&mlx_decoder->decoder_data);
		THROW(env, "java/lang/InternalError", "Decoder initialization failed");
	}

	set_context(env, thiz, (void *)mlx_decoder);
}

JNIEXPORT void JNICALL
Java_com_mellanox_erasurecode_rawcoder_MellanoxRSRawDecoder_decodeImpl(
JNIEnv *env, jobject thiz, jobjectArray inputs, jintArray inputOffsets,
jint dataLen, jintArray erasedIndexes, jobjectArray outputs,
jintArray outputOffsets) {
	struct mlx_decoder *mlx_decoder = (struct mlx_decoder*) get_context(env, thiz);
	struct mlx_coder_data *decoder_data = &mlx_decoder->decoder_data;
	int erasures_size = (*env)->GetArrayLength(env, erasedIndexes);
	int* erasures = (int*)(*env)->GetIntArrayElements(env, erasedIndexes, NULL);
	int err;

	decoder_get_buffers(env, inputs, inputOffsets, outputs, outputOffsets, erasures_size, decoder_data);
	PASS_EXCEPTIONS(env);

	err = mlx_eco_decoder_decode(mlx_decoder->decoder_ctx, decoder_data->data,
			decoder_data->coding, decoder_data->data_size, decoder_data->coding_size, dataLen, erasures, erasures_size);
	if (err) {
		THROW(env, "java/lang/InternalError", "Got error during decode");
	}
}

JNIEXPORT void JNICALL
Java_com_mellanox_erasurecode_rawcoder_MellanoxRSRawDecoder_destroyImpl(
JNIEnv *env, jobject thiz) {
	struct mlx_decoder *mlx_decoder = (struct mlx_decoder*) get_context(env, thiz);
	mlx_eco_decoder_release(mlx_decoder->decoder_ctx);
	free_coder_data(&mlx_decoder->decoder_data);
	free(mlx_decoder);
	set_context(env, thiz, NULL);
}

