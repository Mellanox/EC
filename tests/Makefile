# Define a common prefix where binaries and docs install
PREFIX = /usr
sbindir = bin

CC = gcc
CFLAGS += -g -ggdb -Wall -W -D_GNU_SOURCE -I /usr/include/jerasure
LDFLAGS = -libverbs -lgf_complete -lJerasure -lpthread -lrdmacm -lecOffload


OBJECTS_LAT = ec_encoder.o ec_decoder.o ec_common.o common.o ec_capability_test.o
TARGETS = ibv_ec_capability_test ibv_ec_encoder ibv_ec_decoder

all: $(TARGETS)

ibv_ec_capability_test: ec_capability_test.o
	$(CC) $(CFLAGS) -libverbs ec_capability_test.o -o $@

ibv_ec_encoder: ec_encoder.o ec_common.o common.o
	$(CC) $(CFLAGS) $(LDFLAGS) ec_encoder.o ec_common.o common.o -o $@

ibv_ec_decoder: ec_decoder.o ec_common.o common.o
	$(CC) $(CFLAGS) $(LDFLAGS) ec_decoder.o ec_common.o common.o -o $@

install:
	install -d -m 755 $(PREFIX)/$(sbindir)
	install -m 755 $(TARGETS) $(PREFIX)/$(sbindir)
clean:
	rm -f $(OBJECTS_LAT) $(TARGETS)
