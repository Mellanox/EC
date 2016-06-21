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

