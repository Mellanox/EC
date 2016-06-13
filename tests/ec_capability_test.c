/*
 * Copyright (c) 2015 Mellanox Technologies.  All rights reserved.
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

#include <string.h>
#include <infiniband/verbs_exp.h>

static struct ibv_device *find_device(const char *devname)
{
	struct ibv_device **dev_list = NULL;
	struct ibv_device *device = NULL;

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		printf("Failed to get IB devices list");
		return NULL;
	}

	if (!devname) {
		device = *dev_list;
		if (!device)
			printf("No IB devices found\n");
	} else {
		int i;

		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]),
				    devname))
				break;
		device = dev_list[i];
		if (!device)
			printf("IB device %s not found\n", devname);
	}

	ibv_free_device_list(dev_list);

	return device;
}

static void printAvailableDevices()
{
	struct ibv_device **dev_list = NULL;
	int i;

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		printf("Failed to get IB devices list");
		return;
	}

	printf("Available devices :");
	for (i = 0; dev_list[i]; ++i)
		printf(" %s", ibv_get_device_name(dev_list[i]));
	printf("\n");

	ibv_free_device_list(dev_list);
}

static void printUsage()
{
	printf("Usage = ./ec_capability_test <device_name>\n");

	printAvailableDevices();

	exit(1);
}

int main(int argc, char *argv[])
{
	struct ibv_device *device;
	struct ibv_context *context;
	struct ibv_exp_device_attr dattr;
	int err = 0;

	if (argc != 2) {
		printUsage();
	}

	printf("Checking EC capabilities for %s\n", argv[1]);

	device = find_device(argv[1]);

	if (!device) {
		printUsage();
	}

	context = ibv_open_device(device);
	if (!context) {
		printf("Couldn't get context for %s\n", argv[1]);
		err = 1;
		goto close_device;
	}

	memset(&dattr, 0, sizeof(dattr));
	dattr.comp_mask = IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS | IBV_EXP_DEVICE_ATTR_EC_CAPS;
	err = ibv_exp_query_device(context, &dattr);
	if (err) {
		printf("Couldn't query device for EC offload caps.\n");
		goto close_device;
	}

	if (!(dattr.exp_device_cap_flags & IBV_EXP_DEVICE_EC_OFFLOAD)) {
		printf("EC offload not supported by driver.\n");
		err = 1;
		goto close_device;
	}

	printf("EC offload supported by driver.\n");

close_device:
	ibv_close_device(context);

	return err;
}

