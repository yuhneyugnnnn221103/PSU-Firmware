# Test host cho cfg_store (flash gia + mat dien tai tung thao tac). Chay: make -C tests/host
CC ?= gcc
SRC = cfg_store_test.c ../../shared_lib/cfg_store.c ../../shared_lib/crc32_sw.c ../../shared_lib/ota_slot.c

cfg_store_test: $(SRC)
	$(CC) -std=gnu11 -O1 -Iinc -I../../shared_lib $(SRC) -o $@

test: cfg_store_test
	./cfg_store_test

clean:
	rm -f cfg_store_test
.PHONY: test clean
