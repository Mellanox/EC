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

#ifndef _EC_UTILS_H_
#define _EC_UTILS_H_

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

#endif //_EC_UTILS_H
