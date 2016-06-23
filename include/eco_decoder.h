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

#ifndef ECO_DECODER_H_
#define ECO_DECODER_H_

/**
 * @file eco_decoder.h
 * @brief Decode a given set of data blocks and code blocks and place into output recovery blocks.
 *
 * Mellanox EC library used for Erasure Coding and RAID HW offload.
 * Erasure coding (EC) is a method of data protection in which data is broken into fragments,
 * expanded and encoded with redundant data pieces and stored across a set of different locations or storage media.
 * All the arithmetic calculation are made in GF(2^4).
 * Currently supported by mlx5 only.
 */

#include "eco_common.h"

/**
* @eco_ctx                          Erasure Coding Offload context.
* @int_decode_matrix                Registered buffer [k * k] of the decode matrix in int format used for Jerasure to calculate the decode matrix.
* @u8_decode_matrix                 Registered buffer [k * m] of the decode matrix used for ibv_exp_ec_decode_sync method.
* @int_erasures                     Pointer to byte-map of which blocks were erased and needs to be recovered - used for Jerasure.
* @u8_erasures                      Pointer to byte-map of which blocks were erased and needs to be recovered - used for verbs decode method.
* @survived                         Pointer to byte-map of which blocks were survived.
*/
struct eco_decoder {
	struct eco_context          *eco_ctx;
	int                         *int_decode_matrix;
	uint8_t                     *u8_decode_matrix;
	int                         *int_erasures;
	uint8_t                     *u8_erasures;
	int                         *survived;
};

/**
 * Initialize verbs EC decoder object used for fast Erasure Coding HW offload.
 *
 * @param k                         Number of data blocks.
 * @param m                         Number of code blocks.
 * @param use_vandermonde_matrix    Boolean variable which determine the type of the encode matrix:
 *                                  0 for Cauchy coding matrix else for Vandermonde coding matrix -
 * @return                          Pointer to an initialize EC decoder object if successful, else NULL.
 */
struct eco_decoder *mlx_eco_decoder_init(int k, int m, int use_vandermonde_matrix);

/**
 * Register buffers and update the memory layout context for future encode/decode operations.
 * Because the HW can perform encode/decode operations only on 64 bytes aligned buffers, we will register only the aligned part of the buffers.
 * This function is optional - but it is recommended to use for better performance.
 *
 * @param eco_decoder               Pointer to an initialized EC decoder.
 * @param data                      Array of pointers to source input buffers.
 * @param coding                    Array of pointers to coded output buffers.
 * @param data_size                 Size of data array (must be equal to the initial amount of data blocks).
 * @param coding_size               Size of coding array (must be equal to the initial amount of code blocks).
 * @param block_size                Length of each block of data.
 * @return                          0 successful, other fail.
 */
int mlx_eco_decoder_register(struct eco_decoder *eco_decoder, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size);

/**
 * Generate k*k decoding matrix by taking the rows corresponding to k non-erased devices of the
 * distribution matrix and store it in the decoder context.
 * This method should be call before first decoding operation or when there is a change in the erasures bit-map.
 * This function is optional - but it is recommended to use for better performance.
 *
 * @param eco_decoder               Pointer to an initialized EC decoder.
 * @param erasures                  Pointer to byte-map of which blocks were erased and needs to be recovered.
 * @param erasures_size             Size of erasures bit-map.
 * @return                          0 successful, other fail.
 */
int mlx_eco_decoder_generate_decode_matrix(struct eco_decoder *eco_decoder, int *erasures, int erasures_size);

/**
 * Decode a given set of data blocks and code_blocks and place into output recovery blocks.
 * Using mlx_eco_decoder_generate_decode_matrix() if the decode matrix is not compatible with the erasures.
 * Using mlx_eco_decoder_register() if the buffers are not registered.
 *
 * @param eco_decoder               Pointer to an initialized EC decoder.
 * @param data                      Array of pointers to source input buffers.
 * @param coding                    Array of pointers to coded output buffers.
 * @param data_size                 Size of data array (must be equal to the initial amount of data blocks).
 * @param coding_size               Size of coding array (must be equal to the initial amount of code blocks).
 * @param block_size                Length of each block of data.
 * @param erasures                  Pointer to byte-map of which blocks were erased and needs to be recovered.
 * @param erasures_size             Size of erasures bit-map.
 * @return                          0 successful, other fail.
 */
int mlx_eco_decoder_decode(struct eco_decoder *eco_decoder, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size, int *erasures, int erasures_size);

/**
 * Release all EC decoder resources.
 *
 * @param eco_decoder               Pointer to an initialized EC decoder.
 * @return                          0 successful, other fail.
 */
int mlx_eco_decoder_release(struct eco_decoder *eco_decoder);

#endif /* ECO_DECODER_H_ */
