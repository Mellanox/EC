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

#ifndef _MellanoxRSRawCoderUtils_H_
#define _MellanoxRSRawCoderUtils_H_

#include <string.h>
#include <jni.h>

#define USE_VANDERMONDE_MATRIX 1
#define TMP_BUFFERS_SIZE 2097152 // 2MB

#define THROW(env, exception_name, message) \
		{ \
	jclass ecls = (*env)->FindClass(env, exception_name); \
	if (ecls) { \
		(*env)->ThrowNew(env, ecls, message); \
		(*env)->DeleteLocalRef(env, ecls); \
		return; \
	} \
		}

#define PASS_EXCEPTIONS(env) \
		{ \
	if ((*env)->ExceptionCheck(env)) return; \
		}

struct mlx_coder_data {
	unsigned char **data;
	unsigned char **coding;
	int data_size;
	int coding_size;
};

struct mlx_encoder {
	struct eco_encoder *encoder_ctx;
	struct mlx_coder_data encoder_data;
};

struct mlx_decoder {
	struct eco_decoder *decoder_ctx;
	struct mlx_coder_data decoder_data;
	unsigned char *tmp_arrays; // The amount of the buffers is equal to the coding array size.
	int tmp_arrays_size; // The size of each buffer in tmp_arrays.
	int *erasures_indexes; // The size is equal to data_size + coding_size.
	int *decode_erasures;
	int  decode_erasures_size; // The actual size of the decode_erasures array.
};

int allocate_coder_data(struct mlx_coder_data* coder_data, int data_size, int coding_size);

void free_coder_data(struct mlx_coder_data* coder_data);

void set_context(JNIEnv* env, jobject thiz, void* context);

void* get_context(JNIEnv* env, jobject thiz);

void encoder_get_buffers(JNIEnv *env, struct mlx_encoder *mlx_encoder, jobjectArray inputs, jintArray inputOffsets, jobjectArray outputs,
		jintArray outputOffsets);

void decoder_get_buffers(JNIEnv *env, struct mlx_decoder *mlx_decoder, jobjectArray inputs, jintArray inputOffsets, jobjectArray outputs,
		jintArray outputOffsets, jintArray erasedIndexes, int block_size);

#endif //_MellanoxRSRawCoderUtils_H
