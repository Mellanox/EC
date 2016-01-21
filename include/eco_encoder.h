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

#ifndef ECO_ENCODER_H_
#define ECO_ENCODER_H_

/**
 * @file eco_encoder.h
 * @brief Generates redundancy blocks of encoded data as specified by the coding matrix of GF (2^4) coefficients.
 *
 * Mellanox EC library used for Erasure Coding and RAID HW offload.
 * Erasure coding (EC) is a method of data protection in which data is broken into fragments,
 * expanded and encoded with redundant data pieces and stored across a set of different locations or storage media.
 * All the arithmetic calculation are made in GF(2^4).
 * Currently supported by mlx5 only.
 */

#include "eco_common.h"

/**
* @eco_ctx                               Erasure Coding Offload context.
*/
struct eco_encoder {
	struct eco_context               *eco_ctx;
};

/**
 * Initialize verbs EC encoder object used for fast Erasure Coding HW offload.
 *
 * @param k                              Number of data blocks.
 * @param m                              Number of code blocks.
 * @param use_vandermonde_matrix         Boolean variable which determine the type of the encode matrix:
 *                                       0 for Cauchy coding matrix else for Vandermonde coding matrix -
 * @return                               Pointer to an initialize EC encoder object if successful, else NULL.
 */
struct eco_encoder *mlx_eco_encoder_init(int k, int m, int use_vandermonde_matrix);

/**
 * Register buffers and update the memory layout context for future encode/decode operations.
 * This function is optional - but it is recommended to use for better performance.
 *
 * @param eco_encoder                    Pointer to an initialized EC encoder.
 * @param data                           Array of pointers to source input buffers.
 * @param coding                         Array of pointers to coded output buffers.
 * @param data_size                      Size of data array (must be equal to the initial amount of data blocks).
 * @param coding_size                    Size of coding array (must be equal to the initial amount of code blocks).
 * @param block_size                     Length of each block of data.
 * @return                               0 successful, other fail.
 */
int mlx_eco_encoder_register(struct eco_encoder *eco_encoder, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size);

/**
 * Generates blocks of encoded data from the data buffers and store them in the coding array.
 * Using mlx_eco_encoder_register() if the buffers are not registered.
 *
 * @param eco_encoder                    Pointer to an initialized EC encoder.
 * @param data                           Array of pointers to source input buffers.
 * @param coding                         Array of pointers to coded output buffers.
 * @param data_size                      Size of data array (must be equal to the initial amount of data blocks).
 * @param coding_size                    Size of coding array (must be equal to the initial amount of code blocks).
 * @param block_size                     Length of each block of data.
 * @return                               0 successful, other fail.
 */
int mlx_eco_encoder_encode(struct eco_encoder *eco_encoder, uint8_t **data, uint8_t **coding, int data_size, int coding_size, int block_size);

/**
 * Release all EC encoder resources.
 *
 * @param eco_encoder                    Pointer to an initialized EC encoder.
 * @return                               0 successful, other fail.
 */
int mlx_eco_encoder_release(struct eco_encoder *eco_encoder);

#endif /* ECO_ENCODER_H_ */
