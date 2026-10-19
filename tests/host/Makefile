# Test host (flash gia + mat dien tai tung thao tac). Chay: make -C tests/host test
CC ?= gcc
CFLAGS = -std=gnu11 -O1 -w -Iinc -I../../shared_lib -I../../stm32h725_ota/Boot
SH = ../../shared_lib
COMMON = flash_sim.c $(SH)/crc32_sw.c $(SH)/crc16_ccitt.c $(SH)/ota_slot.c

cfg_store_test: cfg_store_test.c $(COMMON) $(SH)/cfg_store.c
	$(CC) $(CFLAGS) $^ -o $@

select_test: select_test.c $(COMMON) $(SH)/cfg_store.c $(SH)/ota_select.c
	$(CC) $(CFLAGS) $^ -o $@

APP = ../../PSU_stm32h7/App
self_test_test: self_test_test.c $(APP)/self_test.c
	$(CC) -std=gnu11 -O1 -Wall -Iinc_safety -I$(APP) -I../../PSU_stm32h7/BSP $^ -o $@

safety_test: safety_test.c $(APP)/safety.c $(APP)/psu_trip.c
	$(CC) -std=gnu11 -O1 -Wall -Wno-unused-function -Iinc_safety -I$(APP) -I../../PSU_stm32h7/BSP $^ -o $@

psu_frame_test: psu_frame_test.c $(SH)/psu_frame.c $(SH)/crc16_ccitt.c
	$(CC) $(CFLAGS) $^ -o $@

APPSRC = ../../PSU_stm32h7/App
psu_logic_test: psu_logic_test.c $(APPSRC)/psu_trip.c $(APPSRC)/psu_monitor.c $(APPSRC)/psu_cfg.c $(SH)/psu_frame.c $(SH)/cfg_store.c $(COMMON)
	$(CC) $(CFLAGS) -I$(APPSRC) -I../../PSU_stm32h7/BSP $^ -o $@

recovery_test: recovery_test.c ../../stm32h725_ota/Boot/boot_recovery.c $(SH)/psu_frame.c $(COMMON) $(SH)/cfg_store.c
	$(CC) $(CFLAGS) $^ -o $@

test: cfg_store_test recovery_test select_test safety_test self_test_test psu_frame_test psu_logic_test
	./cfg_store_test
	./recovery_test
	./select_test
	./safety_test
	./self_test_test
	./psu_frame_test
	./psu_logic_test

clean:
	rm -f cfg_store_test recovery_test select_test safety_test self_test_test psu_frame_test psu_logic_test
.PHONY: test clean
