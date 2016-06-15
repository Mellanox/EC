/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
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

#include "ec_common.h"
#include <ecOffload/eco_decoder.h>

struct decoder_context {
	struct ibv_context	*context;
	struct ibv_pd		*pd;
	struct ec_context	*ec_ctx;
	int			datafd;
	int			codefd;
	int			outfd_data_verbs;
	int			outfd_code_eco;
	int			outfd_data_eco;
};

static void close_io_files(struct decoder_context *ctx)
{
	close(ctx->outfd_data_verbs);
	close(ctx->outfd_code_eco);
	close(ctx->outfd_data_eco);
	close(ctx->codefd);
	close(ctx->datafd);
}

static int open_io_files(struct inargs *in, struct decoder_context *ctx)
{
	char *outfile, *outfile_code_lib, *outfile_data_lib;
	int err = 0;

	ctx->datafd = open(in->datafile, O_RDONLY);
	if (ctx->datafd < 0) {
		err_log("Failed to open data file\n");
		return -EIO;
	}

	ctx->codefd = open(in->codefile, O_RDONLY);
	if (ctx->codefd < 0) {
		err_log("Failed to open code file");
		err = -EIO;
		goto err_datafd;
	}

	outfile = calloc(1, strlen(in->datafile) + strlen(".decode.data.verbs") + 1);
	if (!outfile) {
		err_log("Failed to alloc outfile\n");
		err = -ENOMEM;
		goto err_codefd;
	}

	outfile = strcat(outfile, in->datafile);
	outfile = strcat(outfile, ".decode.data.verbs");
	unlink(outfile);
	ctx->outfd_data_verbs = open(outfile, O_RDWR | O_CREAT, 0666);
	if (ctx->outfd_data_verbs < 0) {
		err_log("Failed to open offload file");
		free(outfile);
		err = -EIO;
		goto err_codefd;
	}
	// datalib
	outfile_data_lib = calloc(1, strlen(in->datafile) + strlen(".decode.data.eco") + 1);
	if (!outfile_data_lib) {
		err_log("Failed to alloc data outfile\n");
		err = -ENOMEM;
		goto err_outfd;
	}

	outfile_data_lib = strcat(outfile_data_lib, in->datafile);
	outfile_data_lib = strcat(outfile_data_lib, ".decode.data.eco");
	unlink(outfile_data_lib);
	ctx->outfd_data_eco = open(outfile_data_lib, O_RDWR | O_CREAT, 0666);
	if (ctx->outfd_data_eco < 0) {
		err_log("Failed to open data offload library file");
		free(outfile_data_lib);
		err = -EIO;
		goto err_outfd;
	}
	free(outfile_data_lib);

	// code lib
	outfile_code_lib = calloc(1, strlen(in->datafile) + strlen(".decode.code.eco") + 1);
	if (!outfile_code_lib) {
		err_log("Failed to alloc code outfile\n");
		err = -ENOMEM;
		goto err_outfd;
	}

	outfile_code_lib = strcat(outfile_code_lib, in->datafile);
	outfile_code_lib = strcat(outfile_code_lib, ".decode.code.eco");
	unlink(outfile_code_lib);
	ctx->outfd_code_eco = open(outfile_code_lib, O_RDWR | O_CREAT, 0666);
	if (ctx->outfd_code_eco < 0) {
		err_log("Failed to open code offload library file");
		free(outfile_code_lib);
		err = -EIO;
		goto err_outfd;
	}
	free(outfile_code_lib);

	// done
	free(outfile);

	return 0;

err_outfd:
	close(ctx->outfd_data_verbs);
err_codefd:
	close(ctx->codefd);
err_datafd:
	close(ctx->datafd);

	return err;
}

static struct decoder_context *
init_ctx(struct ibv_device *ib_dev,
		struct inargs *in)
{
	struct decoder_context *ctx;
	int err;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		fprintf(stderr, "Failed to allocate EC context\n");
		return NULL;
	}

	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
				ibv_get_device_name(ib_dev));
		goto free_ctx;
	}

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Failed to allocate PD\n");
		goto close_device;
	}

	ctx->ec_ctx = alloc_ec_ctx(ctx->pd, in->frame_size,
			in->k, in->m, in->w, 1, in->failed_blocks);
	if (!ctx->ec_ctx) {
		fprintf(stderr, "Failed to allocate EC context\n");
		goto dealloc_pd;
	}

	err = open_io_files(in, ctx);
	if (err)
		goto free_ec;

	return ctx;

free_ec:
        free_ec_ctx(ctx->ec_ctx);
dealloc_pd:
	ibv_dealloc_pd(ctx->pd);
close_device:
	ibv_close_device(ctx->context);
free_ctx:
	free(ctx);

	return NULL;
}

static void close_ctx(struct decoder_context *ctx)
{
	free_ec_ctx(ctx->ec_ctx);
	ibv_dealloc_pd(ctx->pd);

	if (ibv_close_device(ctx->context))
		fprintf(stderr, "Couldn't release context\n");


	close_io_files(ctx);
	free(ctx);
}

static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s            start EC decoder\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -i, --ib-dev=<dev>         use IB device <dev> (default first device found)\n");
	printf("  -k, --data_blocks=<blocks> Number of data blocks\n");
	printf("  -m, --code_blocks=<blocks> Number of code blocks\n");
	printf("  -w, --gf=<gf>              Galois field GF(2^w)\n");
	printf("  -D, --datafile=<name>      Name of input data file\n");
	printf("  -C, --codefile=<name>      Name of input code file\n");
	printf("  -E, --erasures=<erasures>  Comma separated failed blocks\n");
	printf("  -s, --frame_size=<size>    size of EC frame\n");
	printf("  -d, --debug                print debug messages\n");
	printf("  -v, --verbose              add verbosity\n");
	printf("  -h, --help                 display this output\n");
}

static int process_inargs(int argc, char *argv[], struct inargs *in)
{
	int err;
	struct option long_options[] = {
			{ .name = "ib-dev",        .has_arg = 1, .val = 'i' },
			{ .name = "datafile",      .has_arg = 1, .val = 'D' },
			{ .name = "codefile",      .has_arg = 1, .val = 'C' },
			{ .name = "erasures",      .has_arg = 1, .val = 'E' },
			{ .name = "frame_size",    .has_arg = 1, .val = 's' },
			{ .name = "data_blocks",   .has_arg = 1, .val = 'k' },
			{ .name = "code_blocks",   .has_arg = 1, .val = 'm' },
			{ .name = "gf",            .has_arg = 1, .val = 'w' },
			{ .name = "debug",         .has_arg = 0, .val = 'd' },
			{ .name = "verbose",       .has_arg = 0, .val = 'v' },
			{ .name = "help",          .has_arg = 0, .val = 'h' },
			{ .name = 0, .has_arg = 0, .val = 0 }
	};

	err = common_process_inargs(argc, argv, "i:D:C:E:s:k:m:w:hdv",
			long_options, in, usage);
	if (err)
		return err;

	if (in->datafile == NULL) {
		fprintf(stderr, "No input data file was given\n");
		return -EINVAL;
	}

	if (in->codefile == NULL) {
		fprintf(stderr, "No input code file was given\n");
		return -EINVAL;
	}

	if (in->frame_size <= 0) {
		fprintf(stderr, "No frame_size given %d\n", in->frame_size);
		return -EINVAL;
	}

	return 0;
}

static void zero_erasures(struct ec_context *ctx, void *data_buf, void *code_buf)
{
	int i;

	for (i = 0; i < ctx->attr.k; i++)
		if (ctx->int_erasures[i])
			memset(data_buf + i * ctx->block_size, 0, ctx->block_size);

	for (i = 0; i < ctx->attr.m; i++) {
		if (ctx->int_erasures[i + ctx->attr.k])
			memset(code_buf + i * ctx->block_size, 0, ctx->block_size);
	}
}

static int decode_file(struct decoder_context *ctx, struct eco_decoder *lib_decoder, int *erasures_arr, int num_erasures)
{
	struct ec_context *ec_ctx = ctx->ec_ctx;
	int dbytes, cbytes, wbytes;
	int err;

	while (1) {
		dbytes = read(ctx->datafd, ec_ctx->data.buf,
				ec_ctx->block_size * ec_ctx->attr.k);
		if (dbytes <= 0)
			break;


		cbytes = read(ctx->codefd, ec_ctx->code.buf,
				ec_ctx->block_size * ec_ctx->attr.m);
		if (cbytes <= 0)
			break;

		zero_erasures(ec_ctx, ec_ctx->data.buf, ec_ctx->code.buf);
		err = ibv_exp_ec_decode_sync(ec_ctx->calc, &ec_ctx->mem,
				ec_ctx->u8_erasures, ec_ctx->de_mat);
		if (err) {
			fprintf(stderr, "Failed ibv_exp_ec_decode (%d)\n", err);
			return err;
		}

		wbytes = write(ctx->outfd_data_verbs, ec_ctx->data.buf, dbytes);
		if (wbytes < dbytes) {
			fprintf(stderr, "Failed write to fd (%d)\n", err);
			return err;
		}

		// LIB
		zero_erasures(ec_ctx, ec_ctx->data.buf, ec_ctx->code.buf);
		err = mlx_eco_decoder_decode(lib_decoder, ec_ctx->data_arr, ec_ctx->code_arr, ec_ctx->attr.k, ec_ctx->attr.m, ec_ctx->block_size, erasures_arr, num_erasures);

		wbytes = write(ctx->outfd_data_eco, ec_ctx->data.buf, dbytes);
		if (wbytes < dbytes) {
			fprintf(stderr, "Library : Failed write to fd (%d)\n", err);
			return err;
		}

		wbytes = write(ctx->outfd_code_eco, ec_ctx->code.buf, cbytes);
		if (wbytes < cbytes) {
			fprintf(stderr, "Library : Failed write to fd (%d)\n", err);
			return err;
		}
		// END

		memset(ec_ctx->data.buf, 0, ec_ctx->block_size * ec_ctx->attr.k);
		memset(ec_ctx->code.buf, 0, ec_ctx->block_size * ec_ctx->attr.m);
	}

	return 0;
}

static void set_buffers(struct decoder_context *ctx)
{
	struct ec_context *ec_ctx = ctx->ec_ctx;
	int i;

	ec_ctx->data_arr = calloc(ec_ctx->attr.k, sizeof(*ec_ctx->data_arr));
	ec_ctx->code_arr = calloc(ec_ctx->attr.m, sizeof(*ec_ctx->code_arr));

	for (i = 0; i < ec_ctx->attr.k ; i++) {
		ec_ctx->data_arr[i] = ec_ctx->data.buf + i * ec_ctx->block_size;
	}
	for (i = 0; i < ec_ctx->attr.m ; i++) {
		ec_ctx->code_arr[i] = ec_ctx->code.buf + i * ec_ctx->block_size;
	}
}

int main(int argc, char *argv[])
{
	struct decoder_context *ctx;
	struct ibv_device *device;
	struct inargs in;
	int err;

	err = process_inargs(argc, argv, &in);
	if (err)
		return err;

	device = find_device(in.devname);
	if (!device)
		return -EINVAL;

	ctx = init_ctx(device, &in);
	if (!ctx)
		return -ENOMEM;

	set_buffers(ctx);

	// library init
	struct eco_decoder *lib_decoder = mlx_eco_decoder_init(in.k, in.m, 1);
	if (!lib_decoder) {
		err_log("mlx_eco_decoder_init failed\n");
		return -ENOMEM;
	}


	int i;
	int num_erasures = 0;
	int erasures_arr[in.k + in.m];

	for (i = 0 ; i < in.k + in.m ; i++) {
		if (ctx->ec_ctx->int_erasures[i]) {
			erasures_arr[num_erasures] = i;
			num_erasures++;
		}
	}

	err = mlx_eco_decoder_generate_decode_matrix(lib_decoder, erasures_arr, num_erasures);
	if (err){
		err_log("mlx_ec_allocate_decode_matrix failed\n");
		return -ENOMEM;
	}

	// register buffers
	err = mlx_eco_decoder_register(lib_decoder, ctx->ec_ctx->data_arr, ctx->ec_ctx->code_arr, in.k, in.m, ctx->ec_ctx->block_size);
	if (err) {
		err_log("mlx_ec_register_mrs failed to register\n");
		return -ENOMEM;
	}

	err = decode_file(ctx, lib_decoder, erasures_arr, num_erasures);
	if (err)
		fprintf(stderr, "failed to encode file %s\n", in.datafile);

	// destroy
	err = mlx_eco_decoder_release(lib_decoder);
	if (err) {
		err_log("mlx_ec_register_mrs failed to destroy\n");
		return -ENOMEM;
	}

	close_ctx(ctx);

	return 0;
}
