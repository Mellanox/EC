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

#include "MellanoxRSRawCoderUtils.h"
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

