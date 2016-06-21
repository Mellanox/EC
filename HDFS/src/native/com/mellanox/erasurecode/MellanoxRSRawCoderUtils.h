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

int allocate_coder_data(struct mlx_coder_data* coder_data, int data_size, int coding_size);

void free_coder_data(struct mlx_coder_data* coder_data);

void set_context(JNIEnv* env, jobject thiz, void* context);

void* get_context(JNIEnv* env, jobject thiz);

void decoder_get_buffers(JNIEnv *env, jobjectArray inputs, jintArray inputOffsets, jobjectArray outputs,
		jintArray outputOffsets, int erasures_size, struct mlx_coder_data *decoder_data);

void encoder_get_buffers(JNIEnv *env, jobjectArray inputs, jintArray inputOffsets, jobjectArray outputs,
		jintArray outputOffsets, struct mlx_coder_data *encoder_data);

#endif //_MellanoxRSRawCoderUtils_H
