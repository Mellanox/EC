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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MellanoxRSRawCoderUtils.h"

static void prepare_decoder_data(JNIEnv *env, struct mlx_decoder *mlx_decoder, jobjectArray inputs, int *input_erasures, int input_erasures_size, int block_size){
	int  i;
	int num_inputs = (*env)->GetArrayLength(env, inputs);

	memset(mlx_decoder->erasures_indexes, -1 , sizeof(int) * num_inputs);
	for (i = 0 ; i < input_erasures_size ; i++) {
		mlx_decoder->erasures_indexes[input_erasures[i]] = i;
	}

	// Allocate new tmp_arrays if needed.
	if (block_size > mlx_decoder->tmp_arrays_size) {
		int input_nulls = 0;
		jobject byteBuffer;

		for (i = 0 ; i < num_inputs ; i++) {
			byteBuffer = (*env)->GetObjectArrayElement(env, inputs, i);
			if (byteBuffer == NULL) {
				input_nulls++;
			}
		}

		if(input_nulls > input_erasures_size) {
			unsigned char *tmp_arrays = calloc(mlx_decoder->decoder_data.coding_size, block_size);
			if (!tmp_arrays) {
				THROW(env, "java/lang/InternalError", "Failed to allocate tmp_arrays object");
			}
			free(mlx_decoder->tmp_arrays);
			mlx_decoder->tmp_arrays = tmp_arrays;
			mlx_decoder->tmp_arrays_size = block_size;
		}
	}
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

static void decoder_get_buffers_helper(JNIEnv *env, jobjectArray inputs, int *input_offsets, jobjectArray outputs, int *output_offsets,
		unsigned char **dest_buffers, int **decode_erasures, int *erasures_indexes, unsigned char **tmp_arrays,
		int offset, int dest_size, int block_size) {
	jobject byteBuffer;
	int i;

	for (i = 0 ; i < dest_size ; i++) {
		byteBuffer = (*env)->GetObjectArrayElement(env, inputs, i + offset);
		if (byteBuffer != NULL) { // Available buffer
			dest_buffers[i] = (unsigned char *)((*env)->GetDirectBufferAddress(env, byteBuffer));
			dest_buffers[i] += input_offsets[i + offset];
			continue;
		}

		if (erasures_indexes[i + offset] == -1) { // NULL buffer - The client did not ask to compute it.
			dest_buffers[i] = *tmp_arrays;
			*tmp_arrays += block_size;
		}
		else { // NULL buffer - The client asked to compute it.
			byteBuffer = (*env)->GetObjectArrayElement(env, outputs, erasures_indexes[i + offset]);
			if (byteBuffer == NULL) {
				THROW(env, "java/lang/InternalError", "decoder_get_buffers got null output buffer");
			}
			dest_buffers[i] = (unsigned char *)((*env)->GetDirectBufferAddress(env, byteBuffer));
			dest_buffers[i] += output_offsets[erasures_indexes[i + offset]];
		}
		**decode_erasures = i  + offset;
		(*decode_erasures)++;
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

void decoder_get_buffers(JNIEnv *env,  struct mlx_decoder *mlx_decoder, jobjectArray inputs, jintArray inputOffsets, jobjectArray outputs,
		jintArray outputOffsets, jintArray erasedIndexes, int block_size) {
	int num_inputs, num_outputs, input_erasures_size;
	int  *input_erasures, *tmp_input_offsets, *tmp_output_offsets, *decode_erasures;
	unsigned char *tmp_arrays;
	struct mlx_coder_data *decoder_data = &mlx_decoder->decoder_data;

	num_inputs = (*env)->GetArrayLength(env, inputs);
	num_outputs = (*env)->GetArrayLength(env, outputs);
	input_erasures_size = (*env)->GetArrayLength(env, erasedIndexes);
	input_erasures = (int*)(*env)->GetIntArrayElements(env, erasedIndexes, NULL);
	decode_erasures = mlx_decoder->decode_erasures;

	if (num_inputs != decoder_data->data_size + decoder_data->coding_size) {
		THROW(env, "java/lang/InternalError", "Invalid inputs");
	}

	if (num_outputs != input_erasures_size) {
		THROW(env, "java/lang/InternalError", "Invalid outputs");
	}

	prepare_decoder_data(env, mlx_decoder, inputs, input_erasures, input_erasures_size, block_size);
	PASS_EXCEPTIONS(env);

	tmp_arrays = mlx_decoder->tmp_arrays;
	tmp_input_offsets = (int*)(*env)->GetIntArrayElements(env, inputOffsets, NULL);
	tmp_output_offsets = (int*)(*env)->GetIntArrayElements(env, outputOffsets, NULL);

	decoder_get_buffers_helper(env, inputs, tmp_input_offsets, outputs, tmp_output_offsets, decoder_data->data, &decode_erasures,
			mlx_decoder->erasures_indexes, &tmp_arrays, 0, decoder_data->data_size, block_size);
	PASS_EXCEPTIONS(env);
	decoder_get_buffers_helper(env, inputs, tmp_input_offsets, outputs, tmp_output_offsets, decoder_data->coding, &decode_erasures,
			mlx_decoder->erasures_indexes, &tmp_arrays, decoder_data->data_size, decoder_data->coding_size, block_size);

	mlx_decoder->decode_erasures_size =  decode_erasures - mlx_decoder->decode_erasures;
}

void encoder_get_buffers(JNIEnv *env, struct mlx_encoder *mlx_encoder, jobjectArray inputs, jintArray inputOffsets,
		jobjectArray outputs, jintArray outputOffsets) {
	struct mlx_coder_data *encoder_data = &mlx_encoder->encoder_data;

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

