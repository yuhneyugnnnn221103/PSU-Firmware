# Test host (flash gia + mat dien tai tung thao tac). Chay: make -C tests/host test
CC ?= gcc
CFLAGS = -std=gnu11 -O1 -w -Iinc -I../../shared_lib -I../../stm32h725_ota/Boot
SH = ../../shared_lib
COMMON = flash_sim.c $(SH)/crc32_sw.c $(SH)/crc16_ccitt.c $(SH)/ota_slot.c

cfg_store_test: cfg_store_test.c $(COMMON) $(SH)/cfg_store.c
	$(CC) $(CFLAGS) $^ -o $@

select_test: select_test.c $(COMMON) $(SH)/cfg_store.c $(SH)/ota_select.c
	$(CC) $(CFLAGS) $^ -o $@

recovery_test: recovery_test.c ../../stm32h725_ota/Boot/boot_recovery.c $(COMMON) $(SH)/cfg_store.c
	$(CC) $(CFLAGS) $^ -o $@

test: cfg_store_test recovery_test select_test
	./cfg_store_test
	./recovery_test
	./select_test

clean:
	rm -f cfg_store_test recovery_test select_test
.PHONY: test clean
