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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ec_utils.h"

static int check_nulls(JNIEnv *env, jobjectArray inputs, int erasures_size) {
	int i, sum_nulls = 0;
	jobject byteBuffer;
	int num_buffers = (*env)->GetArrayLength(env, inputs);

	for (i = 0; i < num_buffers; i++) {
		byteBuffer = (*env)->GetObjectArrayElement(env, inputs, i);
		if (byteBuffer == NULL) {
			sum_nulls++;
		}
	}

	return sum_nulls != erasures_size;
}

static void encoder_get_buffers_helper(JNIEnv *env, jobjectArray buffers, jintArray buffersOffsets, unsigned char** dest_buffers, int bufffer_size) {
	int i, *tmp_buffers_offsets;
	jobject byteBuffer;

	tmp_buffers_offsets = (int*)(*env)->GetIntArrayElements(env, buffersOffsets, NULL);

	for (i = 0; i < bufffer_size; i++) {
		byteBuffer = (*env)->GetObjectArrayElement(env, buffers, i);
		if (byteBuffer == NULL) {
			THROW(env, "java/lang/InternalError", "encoder_get_buffers got null buffer");
		}

		dest_buffers[i] = (unsigned char *)((*env)->GetDirectBufferAddress(env, byteBuffer));
		dest_buffers[i] += tmp_buffers_offsets[i];
	}
}

static void decoder_get_buffers_helper(JNIEnv *env, jobjectArray inputs, int* input_offsets, jobjectArray outputs, int* output_offsets,
		unsigned char** dest_buffers, int* output_itr, int dest_size, int offset) {
	jobject byteBuffer;
	int i;

	for (i = 0 ; i < dest_size ; i++) {
		byteBuffer = (*env)->GetObjectArrayElement(env, inputs, i + offset);
		if (byteBuffer != NULL) {
			dest_buffers[i] = (unsigned char *)((*env)->GetDirectBufferAddress(env, byteBuffer));
			dest_buffers[i] += input_offsets[i + offset];
		} else {
			byteBuffer = (*env)->GetObjectArrayElement(env, outputs, *output_itr);
			if (byteBuffer == NULL) {
				THROW(env, "java/lang/InternalError", "decoder_get_buffers got null output buffer");
			}
			dest_buffers[i] = (unsigned char *)((*env)->GetDirectBufferAddress(env, byteBuffer));
			dest_buffers[i] += output_offsets[*output_itr];
			(*output_itr)++;
		}
	}
}

int allocate_coder_data(struct mlx_coder_data* coder_data, int data_size, int coding_size) {
	coder_data->data_size = data_size;
	coder_data->coding_size = coding_size;

	coder_data->data = calloc(data_size, sizeof(*coder_data->data));
	if (!coder_data->data) {
		goto data_allocation_error;
	}

	coder_data->coding = calloc(coding_size, sizeof(*coder_data->coding));
	if (!coder_data->coding) {
		goto coding_allocation_error;
	}

	return 0;

coding_allocation_error:
	free(coder_data->data);
data_allocation_error:
	return -1;
}

void free_coder_data(struct mlx_coder_data* coder_data) {
	if (coder_data) {
		if (coder_data->coding) {
			free(coder_data->coding);
			coder_data->coding = NULL;
		}
		if (coder_data->data) {
			free(coder_data->data);
			coder_data->data = NULL;
		}
	}
}

void set_context(JNIEnv* env, jobject thiz, void* context) {
	jclass clazz = (*env)->GetObjectClass(env, thiz);
	jfieldID __context = (*env)->GetFieldID(env, clazz, "__native_coder", "J");
	(*env)->SetLongField(env, thiz, __context, (jlong) context);
}

void* get_context(JNIEnv* env, jobject thiz) {
	jclass clazz = (*env)->GetObjectClass(env, thiz);
	jfieldID __context = (*env)->GetFieldID(env, clazz, "__native_coder", "J");
	void* context = (void*)(*env)->GetLongField(env, thiz, __context);

	return context;
}

void decoder_get_buffers(JNIEnv *env, jobjectArray inputs, jintArray inputOffsets, jobjectArray outputs,
		jintArray outputOffsets, int erasures_size, struct mlx_coder_data *decoder_data) {
	int num_inputs = (*env)->GetArrayLength(env, inputs);
	int num_outputs = (*env)->GetArrayLength(env, outputs);
	int* tmp_input_offsets, *tmp_output_offsets;
	int err, outputItr = 0;

	if (num_inputs != decoder_data->data_size + decoder_data->coding_size) {
		THROW(env, "java/lang/InternalError", "Invalid inputs");
	}

	if (num_outputs != erasures_size) {
		THROW(env, "java/lang/InternalError", "Invalid outputs");
	}

	err = check_nulls(env, inputs, erasures_size);
	if (err) {
		THROW(env, "java/lang/InternalError", "Got to many null input buffers");
	}

	tmp_input_offsets = (int*)(*env)->GetIntArrayElements(env, inputOffsets, NULL);
	tmp_output_offsets = (int*)(*env)->GetIntArrayElements(env, outputOffsets, NULL);

	decoder_get_buffers_helper(env, inputs, tmp_input_offsets, outputs, tmp_output_offsets, decoder_data->data, &outputItr, decoder_data->data_size, 0);
	PASS_EXCEPTIONS(env);
	decoder_get_buffers_helper(env, inputs, tmp_input_offsets, outputs, tmp_output_offsets, decoder_data->coding, &outputItr, decoder_data->coding_size , decoder_data->data_size);
}

void encoder_get_buffers(JNIEnv *env, jobjectArray inputs, jintArray inputOffsets, jobjectArray outputs,
		jintArray outputOffsets, struct mlx_coder_data *encoder_data) {
	int num_inputs = (*env)->GetArrayLength(env, inputs);
	int num_outputs = (*env)->GetArrayLength(env, outputs);

	if (num_inputs != encoder_data->data_size) {
		THROW(env, "java/lang/InternalError", "Invalid inputs");
	}

	if (num_outputs != encoder_data->coding_size) {
		THROW(env, "java/lang/InternalError", "Invalid outputs");
	}

	encoder_get_buffers_helper(env, inputs, inputOffsets, encoder_data->data, num_inputs);
	PASS_EXCEPTIONS(env);
	encoder_get_buffers_helper(env, outputs, outputOffsets, encoder_data->coding, num_outputs);
}

