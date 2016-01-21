#installation parameters
INSTALL_INCLUDE_PATH = /usr/include
INSTALL_LIB_DIR = /ecOffload
INSTALL_LIB_PATH = /usr/lib64/

#build parameters
BUILD_DIR = build/
SRC_DIR = src/
INCLUDE_DIR = include/
LIB_NAME = libecOffload.so

#compilation flags
LDFLAGS = -lJerasure -libverbs
CFLAGS += -g -ggdb -Wall -W -D_GNU_SOURCE
CC = gcc

#include jerasure path
INC= -I /usr/include/jerasure

TARGETS = mlx_eco_library

all : $(TARGETS)

mlx_eco_library:
	mkdir $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INC) -shared -fpic $(LDFLAGS) -o $(BUILD_DIR)$(LIB_NAME) -fPIC $(SRC_DIR)*.c


install:
	install -d $(INSTALL_INCLUDE_PATH)$(INSTALL_LIB_DIR)
	install $(INCLUDE_DIR)*.h $(INSTALL_INCLUDE_PATH)$(INSTALL_LIB_DIR)
	install $(BUILD_DIR)$(LIB_NAME) $(INSTALL_LIB_PATH)$(LIB_NAME)


clean :
	rm -rf $(BUILD_DIR)
