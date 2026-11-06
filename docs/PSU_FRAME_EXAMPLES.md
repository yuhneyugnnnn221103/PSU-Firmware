# Khung mau PSU (do firmware sinh, `make -C tests/host examples`)

Dia chi PSU = 1. Khung: `AB CD | CMD(2) | ADDR | payload | CRC16 | E1 E2`. CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF) tinh tu byte ADDR den het payload (bo AB CD, CMD, CRC, E1 E2). Moi so nhieu byte la big-endian.

Vector kiem tra CRC: CRC16-CCITT-FALSE("123456789") = 0x29B1; CRC32("123456789") = 0xCBF43926.

## PC -> PSU

### 02 02 Hoi giam sat (13 byte)
`[5..8]` du phong = 00.

```
AB CD 02 02 01 00 00 00 00 BB 5D E1 E2
```

### 03 03 Hoi cau hinh (13 byte)
`[5..8]` du phong = 00.

```
AB CD 03 03 01 00 00 00 00 BB 5D E1 E2
```

### 01 01 Bat cum 1 (13 byte)
`[5]` Mask (bit0 = cum 1), `[6]` ClearTrip = 00. Khong co ACK.

```
AB CD 01 01 01 01 00 00 00 CD E9 E1 E2
```

### 01 01 Tat moi cum (13 byte)
Mask = 00.

```
AB CD 01 01 01 00 00 00 00 BB 5D E1 E2
```

### 01 01 ClearTrip (13 byte)
`[6]` = 01: xoa trip code, moi cum TRIPPED ve OFF (toan PSU).

```
AB CD 01 01 01 00 01 00 00 8C 6D E1 E2
```

### 95 95 INFO (9 byte)
Chay o app va recovery.

```
AB CD 95 95 01 F1 D1 E1 E2
```

### 98 98 ENTER_REC (9 byte)
App: ghi co, ACK, reset sau ~50 ms (chi khi moi cum OFF).

```
AB CD 98 98 01 F1 D1 E1 E2
```

### 96 96 ERASE (9 byte)
Chi recovery. Mot ACK khi xoa xong (timeout PC >= 30 s).

```
AB CD 96 96 01 F1 D1 E1 E2
```

### 90 90 BEGIN (21 byte)
`[5..8]` size = 0x00001000, `[9..12]` CRC32 cua file (vi du DEADBEEF), `[13..16]` so version cu = 0 (khong dung).

```
AB CD 90 90 01 00 00 10 00 DE AD BE EF 00 00 00 00 5B F2 E1 E2
```

### 9A 9A SET_VERSION (41 byte)
Chi recovery, giua ERASE va END. `[5..36]` chuoi ASCII 32 byte (ten file bin bo duoi), dem 00.

```
AB CD 9A 9A 01 50 53 55 5F 76 31 2E 32 2E 33 5F 61 62 63 31 32 33 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 EA 86 E1 E2
```

### 91 91 DATA (seq 0) (269 byte)
`[5..6]` seq = 0, `[7..8]` len = 0x0100, `[9..264]` 256 byte (vi du 00..FF). Chunk cuoi: len < 256, dem FF.

```
AB CD 91 91 01 00 00 01 00 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16
17 18 19 1A 1B 1C 1D 1E 1F 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F 30 31 32 33 34 35 36
37 38 39 3A 3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A 4B 4C 4D 4E 4F 50 51 52 53 54 55 56
57 58 59 5A 5B 5C 5D 5E 5F 60 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E 6F 70 71 72 73 74 75 76
77 78 79 7A 7B 7C 7D 7E 7F 80 81 82 83 84 85 86 87 88 89 8A 8B 8C 8D 8E 8F 90 91 92 93 94 95 96
97 98 99 9A 9B 9C 9D 9E 9F A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF B0 B1 B2 B3 B4 B5 B6
B7 B8 B9 BA BB BC BD BE BF C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF D0 D1 D2 D3 D4 D5 D6
D7 D8 D9 DA DB DC DD DE DF E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA EB EC ED EE EF F0 F1 F2 F3 F4 F5 F6
F7 F8 F9 FA FB FC FD FE FF A1 34 E1 E2
```

### 92 92 END (9 byte)
Kiem tong byte + CRC32, ghi header.

```
AB CD 92 92 01 F1 D1 E1 E2
```

### 93 93 COMMIT (9 byte)
Sau END thanh cong: ACK roi reset.

```
AB CD 93 93 01 F1 D1 E1 E2
```

### 99 99 GET_VERSION (9 byte)
Chay o app va recovery.

```
AB CD 99 99 01 F1 D1 E1 E2
```

## PSU -> PC

### 94 94 ACK cho INFO (app, da confirm) (17 byte)
`[5]`=95, `[6]` status 0, `[7..10]` so version cu = 0, `[11]` slot `A`, `[12]` 0x80 = confirmed, boot_count 0.

```
AB CD 94 94 01 95 00 00 00 00 00 41 80 D4 AF E1 E2
```

### 94 94 ACK cho INFO (recovery) (17 byte)
slot `R`.

```
AB CD 94 94 01 95 00 00 00 00 00 52 00 13 07 E1 E2
```

### 94 94 ACK cho ERASE (xong) (17 byte)
`[5]`=96, status 0, info = 1.

```
AB CD 94 94 01 96 00 00 00 00 01 52 00 EC 42 E1 E2
```

### 94 94 ACK cho DATA seq 0 (17 byte)
`[5]`=91, info = seq vua ghi.

```
AB CD 94 94 01 91 00 00 00 00 00 52 00 1C 6A E1 E2
```

### 94 94 ACK loi: BEGIN khi chua ERASE (17 byte)
status 11 (NotErased).

```
AB CD 94 94 01 90 0B 00 00 00 00 52 00 10 96 E1 E2
```

### 94 94 ACK cua app khi nhan lenh nap (17 byte)
status 12 (NotRecovery): phai ENTER_REC truoc.

```
AB CD 94 94 01 96 0C 00 00 00 00 41 00 DF 39 E1 E2
```

### A9 A9 Tra loi GET_VERSION (41 byte)
`[5..36]` chuoi ASCII, dem 00 (rong = chua dat).

```
AB CD A9 A9 01 50 53 55 5F 76 31 2E 32 2E 33 5F 61 62 63 31 32 33 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 EA 86 E1 E2
```

### 81 81 Giam sat (vi du: cum 1 OFF, DCM1 co du lieu, 5 khung da nhan) (310 byte)
Byte 5..7 = I_OUT_ADC1 (00123456), 8..10 = TEMP_ADC1 (000A00). Byte 228..237 = TRIP1..10. Vung du phong tu byte 238 (xem PSU_PROTOCOL.md muc 3). Truong khong co du lieu = 00.

```
AB CD 81 81 01 12 34 56 00 0A 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 01 00 00 01 00 00 01 00 00 41 00 00 00 00 01
00 00 01 00 05 00 00 00 00 00 00 00 00 00 05 00 00 00 00 00 00 00 00 00 3C 01 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 E0 DD E1 E2
```

