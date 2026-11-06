#!/usr/bin/env python3
"""Sinh docs/PSU_Message_Spec.xlsx: cau truc tung ban tin, y nghia tung byte (chia sheet theo nhom chuc nang).

Nguon su that la code firmware + docs/PSU_PROTOCOL.md. Khi doi ban tin: sua file nay, chay lai
    python tools/gen_message_spec.py
Cac khung vi du (sheet "Vi du khung") doc tu docs/PSU_FRAME_EXAMPLES.md (sinh bang `make -C tests/host examples`).
"""
import os
import re
import sys

from openpyxl import Workbook
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "PSU_Message_Spec.xlsx")
EXAMPLES = os.path.join(ROOT, "docs", "PSU_FRAME_EXAMPLES.md")

FONT = "Arial"
F_NORMAL = Font(name=FONT, size=10)
F_BOLD = Font(name=FONT, size=10, bold=True)
F_HEAD = Font(name=FONT, size=10, bold=True, color="FFFFFF")
F_TITLE = Font(name=FONT, size=14, bold=True)
F_MONO = Font(name="Consolas", size=10)
FILL_HEAD = PatternFill("solid", start_color="1F3864")
FILL_SEC = PatternFill("solid", start_color="D9E2F3")
FILL_OK = PatternFill("solid", start_color="E2EFDA")       # da hien thuc
FILL_TMP = PatternFill("solid", start_color="FFF2CC")      # tam thoi
FILL_NA = PatternFill("solid", start_color="EDEDED")       # chua co du lieu = 00
THIN = Side(style="thin", color="BFBFBF")
BORDER = Border(left=THIN, right=THIN, top=THIN, bottom=THIN)
WRAP = Alignment(wrap_text=True, vertical="top")

ST_OK = "Đã hiện thực"
ST_TMP = "Tạm thời (chờ bảng chính thức)"
ST_NA = "Chưa có dữ liệu = 00"
ST_RES = "Dự phòng = 00"
ST_FILL = {ST_OK: FILL_OK, ST_TMP: FILL_TMP, ST_NA: FILL_NA, ST_RES: FILL_NA}


def title(ws, text, sub=None):
    ws["A1"] = text
    ws["A1"].font = F_TITLE
    if sub:
        ws["A2"] = sub
        ws["A2"].font = Font(name=FONT, size=10, italic=True)
        ws["A2"].alignment = Alignment(wrap_text=False)


def header(ws, row, cols, widths=None):
    for i, c in enumerate(cols, 1):
        cell = ws.cell(row=row, column=i, value=c)
        cell.font = F_HEAD
        cell.fill = FILL_HEAD
        cell.border = BORDER
        cell.alignment = Alignment(wrap_text=True, vertical="center", horizontal="center")
    if widths:
        for i, w in enumerate(widths, 1):
            ws.column_dimensions[get_column_letter(i)].width = w
    ws.freeze_panes = ws.cell(row=row + 1, column=1)


def put(ws, row, values, status_col=None, mono_cols=(), bold=False):
    for i, v in enumerate(values, 1):
        cell = ws.cell(row=row, column=i, value=v)
        cell.font = F_MONO if i in mono_cols else (F_BOLD if bold else F_NORMAL)
        cell.alignment = WRAP
        cell.border = BORDER
    if status_col:
        st = values[status_col - 1]
        if st in ST_FILL:
            ws.cell(row=row, column=status_col).fill = ST_FILL[st]


def section(ws, row, text, ncols):
    for i in range(1, ncols + 1):
        c = ws.cell(row=row, column=i)
        c.fill = FILL_SEC
        c.border = BORDER
    ws.cell(row=row, column=1, value=text).font = F_BOLD


def bytemap(ws, start_row, rows, with_len_formula=True):
    """rows: (start, end, field, type, meaning, status). Cot: Bat dau | Ket thuc | So byte (cong thuc) | ..."""
    r = start_row
    for item in rows:
        if item[0] == "SEC":
            section(ws, r, item[1], 7)
        else:
            s, e, field, typ, meaning, status = item
            put(ws, r, [s, e, f"=B{r}-A{r}+1", field, typ, meaning, status], status_col=7)
            ws.cell(row=r, column=1).alignment = Alignment(horizontal="center", vertical="top")
            ws.cell(row=r, column=2).alignment = Alignment(horizontal="center", vertical="top")
            ws.cell(row=r, column=3).alignment = Alignment(horizontal="center", vertical="top")
        r += 1
    return r


BYTE_COLS = ["Byte bắt đầu (từ 0)", "Byte kết thúc", "Số byte", "Trường", "Kiểu / mã hoá", "Ý nghĩa", "Trạng thái"]
BYTE_W = [12, 12, 9, 26, 24, 80, 24]


def frame_head_rows(cmd_text, total):
    return [
        (0, 1, "Header", "AB CD", "Mở đầu khung", ST_OK),
        (2, 3, "CMD", cmd_text, "Mã bản tin (2 byte, big-endian)", ST_OK),
        (4, 4, "ADDR", "u8", "Địa chỉ PSU (cố định trong code: PSU_ADDR = 1). Sai địa chỉ: firmware bỏ qua, không phản hồi", ST_OK),
    ]


def frame_tail_rows(total):
    return [
        (total - 4, total - 3, "CRC16", "u16 big-endian",
         f"CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF) tính trên byte 4..{total - 5} (ADDR đến hết payload); bỏ qua AB CD, CMD, CRC, E1 E2", ST_OK),
        (total - 2, total - 1, "Tail", "E1 E2", "Kết thúc khung", ST_OK),
    ]


def build():
    wb = Workbook()

    # ------------------------------------------------------------------ 1. Tong quan
    ws = wb.active
    ws.title = "Tong quan"
    title(ws, "Giao thức PSU/APSU – cấu trúc bản tin (firmware phía PSU)",
          "Nguồn: code firmware + docs/PSU_PROTOCOL.md. Sinh bằng tools/gen_message_spec.py. Mọi số nhiều byte là big-endian. Byte đếm từ 0 (AB = byte 0).")
    r = 4
    header(ws, r, ["Mục", "Nội dung"], [34, 120])
    ws.freeze_panes = None
    rows = [
        ("Khung chung", "AB CD | CMD (2 byte) | ADDR (1 byte) | payload | CRC16 (2 byte) | E1 E2"),
        ("CRC16", "CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF, không đảo bit, không XOR cuối), tính từ byte ADDR đến hết payload. Vector kiểm tra: CRC16(\"123456789\") = 0x29B1. Bỏ qua AB CD, CMD, CRC, E1 E2. "
                  "Đổi về cách cũ (tính từ CMD): PSU_CRC_INCLUDE_CMD=1 trong shared_lib/psu_frame.h (phải đổi cả phía PC)."),
        ("CRC32 (nạp code)", "CRC32 chuẩn zlib/IEEE (poly 0xEDB88320 đảo bit, init 0xFFFFFFFF, XOR cuối 0xFFFFFFFF) trên toàn bộ nội dung file .bin. Vector: CRC32(\"123456789\") = 0xCBF43926."),
        ("Địa chỉ PSU", "Cố định trong code: PSU_ADDR = 1 (đặt khi build: -DPSU_ADDR=n, cho cả bootloader và app). Khung sai địa chỉ hoặc sai CRC bị bỏ, không phản hồi."),
        ("Chiều giao tiếp", "Firmware KHÔNG tự phát. Chỉ trả lời khi nhận khung hợp lệ, trả lời ra đúng cổng đã nhận."),
        ("Cổng (board bring-up)", "3 cổng, 115200 8N1: RS422 #1 = UART5, RS422 #2 = USART1, FT232 = UART4. Dùng cổng nào cũng được. (Bản đầy đủ: 4 cổng RS422.)"),
        ("Hai chế độ", "APP: giám sát, điều khiển, cấu hình, INFO, ENTER_REC, GET_VERSION. RECOVERY (bootloader): INFO, ERASE, BEGIN, DATA, END, COMMIT, SET_VERSION, GET_VERSION, ENTER_REC. "
                       "Phân biệt bằng byte slot trong ACK INFO: 'A' = app, 'R' = recovery."),
        ("Không ACK", "Điều khiển (01 01) và ghi cấu hình (04 04) không có ACK. PC kiểm tra bằng bản tin giám sát: byte 255..258 (lệnh gần nhất, kết quả, bộ đếm)."),
        ("Bộ tách khung", "Độ dài khung xác định theo CMD (xem sheet 'Danh sach ban tin'). CMD lạ hoặc khung dài quá bộ đệm: bỏ, tách lại tại AB CD kế tiếp. Im lặng > vài chục ms giữa khung: bỏ khung dở."),
        ("Cửa sổ vào recovery", "Sau mỗi lần cấp nguồn, nếu app khởi động được, bootloader nghe UART khoảng 300 ms. Nhận được một khung hợp lệ gửi tới PSU này (thường INFO) thì ở lại recovery."),
        ("Trường chưa có dữ liệu", "Để 00 (cụm 2..4, cụm Supply, RTC, VBUS, các trường mở rộng chưa có phần cứng)."),
        ("Phiên bản giao thức", "1 (byte 281 của bản tin giám sát)."),
    ]
    for i, (a, b) in enumerate(rows, r + 1):
        put(ws, i, [a, b], bold=False)
        ws.cell(row=i, column=1).font = F_BOLD
    r = r + len(rows) + 2
    ws.cell(row=r, column=1, value="Chú giải màu cột 'Trạng thái'").font = F_BOLD
    for j, (name, fill) in enumerate([(ST_OK, FILL_OK), (ST_TMP, FILL_TMP), (ST_NA, FILL_NA)], r + 1):
        c = ws.cell(row=j, column=1, value=name)
        c.fill = fill
        c.font = F_NORMAL
        c.border = BORDER
    r = r + 5
    ws.cell(row=r, column=1, value="Các sheet").font = F_BOLD
    sheets = [
        ("Danh sach ban tin", "Toàn bộ CMD: hướng, độ dài, chế độ, phản hồi, nhóm chức năng"),
        ("Giam sat", "02 02 (hỏi) và 81 81 (310 byte): ý nghĩa từng byte, vùng dự phòng"),
        ("Dieu khien", "01 01: Mask, ClearTrip, quy tắc, kết quả"),
        ("Cau hinh", "03 03, 04 04, 82 82: ảnh cấu hình 904 byte, ngưỡng, thanh ghi INA"),
        ("Nap code", "INFO, ENTER_REC, ERASE, BEGIN, DATA, SET/GET_VERSION, END, COMMIT, ACK + quy trình"),
        ("Ma va trang thai", "Status ACK, ý nghĩa 'info', slot, state, safe_state, kết quả lệnh"),
        ("Trip code", "TRIP1..TRIP10 từng bit (bảng TẠM THỜI)"),
        ("Vi du khung", "Khung mẫu có CRC do firmware sinh, kèm kiểm tra độ dài bằng công thức"),
    ]
    for j, (a, b) in enumerate(sheets, r + 1):
        put(ws, j, [a, b])

    # ------------------------------------------------------------------ 2. Danh sach ban tin
    ws = wb.create_sheet("Danh sach ban tin")
    title(ws, "Danh sách bản tin", "Nhóm: G = giám sát, D = điều khiển, C = cấu hình, N = nạp code, V = version")
    header(ws, 4, ["CMD", "Tên", "Nhóm", "Hướng", "Dài (byte)", "Chế độ", "Phản hồi", "Ghi chú"], [10, 24, 8, 11, 11, 18, 22, 90])
    msgs = [
        ("01 01", "Điều khiển", "D", "PC→PSU", 13, "App", "Không", "Mask bật/tắt từng cụm + ClearTrip"),
        ("02 02", "Hỏi giám sát", "G", "PC→PSU", 13, "App", "81 81", "[5..8] dự phòng = 00"),
        ("81 81", "Giám sát", "G", "PSU→PC", 310, "App", "-", "Dòng/nhiệt độ DCM, trip code, trạng thái 4 cụm, chẩn đoán"),
        ("03 03", "Hỏi cấu hình", "C", "PC→PSU", 13, "App", "82 82", "[5..8] dự phòng = 00"),
        ("82 82", "Trả lời cấu hình", "C", "PSU→PC", 915, "App", "-", "Cùng bố cục 04 04 (Mask = 0); trả cấu hình đang dùng (đã lưu hoặc mặc định)"),
        ("04 04", "Ghi cấu hình", "C", "PC→PSU", 915, "App (mọi cụm OFF)", "Không", "Lưu Flash rồi mới dùng; kiểm tra bằng 03 03 và 81 81 (+17..+19)"),
        ("95 95", "INFO", "N", "PC→PSU", 9, "App + Recovery", "94 94", "Kiểm tra kết nối, slot A/R, trạng thái trial"),
        ("98 98", "ENTER_REC", "N", "PC→PSU", 9, "App (Recovery: ACK OK)", "94 94", "App: ghi cờ, ACK, reset ~50 ms sau; chỉ khi mọi cụm OFF"),
        ("96 96", "ERASE", "N", "PC→PSU", 9, "Recovery", "94 94 khi xóa xong", "Xóa 5 sector vùng app. MỘT ACK khi xong, PC chờ >= 30 s (chưa đo)"),
        ("90 90", "BEGIN", "N", "PC→PSU", 21, "Recovery", "94 94", "Khai báo size, CRC32; yêu cầu đã ERASE"),
        ("91 91", "DATA", "N", "PC→PSU", 269, "Recovery", "94 94", "Gói 256 byte, stop-and-wait"),
        ("9A 9A", "SET_VERSION", "V", "PC→PSU", 41, "Recovery", "94 94", "Chuỗi version 32 byte, giữa ERASE và END"),
        ("92 92", "END", "N", "PC→PSU", 9, "Recovery", "94 94", "Kiểm tra tổng byte + CRC32, ghi header"),
        ("93 93", "COMMIT", "N", "PC→PSU", 9, "Recovery", "94 94 rồi reset", "Sau END thành công"),
        ("99 99", "GET_VERSION", "V", "PC→PSU", 9, "App + Recovery", "A9 A9", ""),
        ("A9 A9", "Trả lời GET_VERSION", "V", "PSU→PC", 41, "App + Recovery", "-", "Chuỗi ASCII 32 byte, rỗng = chưa đặt"),
        ("94 94", "ACK", "N", "PSU→PC", 17, "App + Recovery", "-", "Trả lời các lệnh 9x"),
    ]
    for i, m in enumerate(msgs, 5):
        put(ws, i, list(m), mono_cols=(1,))
        ws.cell(row=i, column=5).alignment = Alignment(horizontal="center", vertical="top")
    n = 5 + len(msgs)
    put(ws, n + 1, ["Ghi chú", "App nhận ERASE / BEGIN / DATA / END / COMMIT / SET_VERSION thì trả ACK status 12 (NOT_RECOVERY): phải ENTER_REC trước. "
                    "Lệnh đã bỏ: 97 97 ROLLBACK (không còn slot A/B)."], bold=True)
    ws.merge_cells(start_row=n + 1, start_column=2, end_row=n + 1, end_column=8)

    # ------------------------------------------------------------------ 3. Giam sat
    ws = wb.create_sheet("Giam sat")
    title(ws, "Giám sát: 02 02 (hỏi) và 81 81 (trả lời, 310 byte)")
    header(ws, 3, BYTE_COLS, BYTE_W)
    ws.freeze_panes = "A4"
    r = 4
    section(ws, r, "02 02 – Hỏi giám sát (PC→PSU, 13 byte)", 7)
    r += 1
    rows = frame_head_rows("02 02", 13) + [(5, 8, "Dự phòng", "4 byte", "Gửi 00", ST_RES)] + frame_tail_rows(13)
    r = bytemap(ws, r, rows)
    section(ws, r, "81 81 – Giám sát (PSU→PC, 310 byte)", 7)
    r += 1
    rows = frame_head_rows("81 81", 310) + [
        ("SEC", "Cụm 1: 3 DCM, mỗi DCM 6 byte (I_OUT_ADC 3 byte + TEMP_ADC 3 byte). Board bring-up: INA1..3 ↔ DCM1..3"),
        (5, 7, "I_OUT_ADC1", "s24 thô", "Thanh ghi CURRENT thô của INA1 (24 bit, dữ liệu ở bit 23..4, có dấu). PC tự dịch phải 4 và xử lý dấu. 00 nếu INA chưa có dữ liệu mới", ST_OK),
        (8, 10, "TEMP_ADC1", "00 hi lo (s16)", "Thanh ghi DIETEMP thô 16 bit của INA1 (nhiệt độ die INA, có dấu) đặt ở 2 byte thấp, byte cao = 00", ST_OK),
        (11, 13, "I_OUT_ADC2", "s24 thô", "Như I_OUT_ADC1, INA2 (DCM2)", ST_OK),
        (14, 16, "TEMP_ADC2", "00 hi lo (s16)", "Như TEMP_ADC1, INA2", ST_OK),
        (17, 19, "I_OUT_ADC3", "s24 thô", "Như I_OUT_ADC1, INA3 (DCM3)", ST_OK),
        (20, 22, "TEMP_ADC3", "00 hi lo (s16)", "Như TEMP_ADC1, INA3", ST_OK),
        ("SEC", "Phần theo đặc tả gốc psu_messages.md (chưa hiện thực)"),
        (23, 220, "Phần còn lại cụm 1, cụm DCM 2..4, cụm Supply", "-", "Không có phần cứng / dữ liệu. Không hiển thị VBUS. Giữ 00", ST_NA),
        (221, 227, "RTC", "7 byte", "Chưa đọc RTC. Giữ 00", ST_NA),
        ("SEC", "Trip code (xem sheet 'Trip code')"),
        (228, 237, "TRIP1..TRIP10", "10 byte, mỗi bit một sự kiện", "Latch cho đến khi nhận ClearTrip. TRIP1..6 đã gán bit (bảng tạm thời, chỉ cụm 1); TRIP7..10 = 00", ST_TMP),
        ("SEC", "Vùng dự phòng 68 byte (238..305) – mở rộng của firmware"),
        (238, 238, "Cụm 1: safe_state", "u8", "0 BOOT, 1 OFF, 2 ARMING, 3 ON, 4 TRIPPED", ST_OK),
        (239, 239, "Cụm 1: PWR_EN", "u8", "1 = chân PWR_EN đang bật, 0 = tắt", ST_OK),
        (240, 240, "Cụm 1: trip_count", "u8", "Số lần trip liên tiếp (về 0 sau 10 s chạy ổn ở ON). Chỉ để báo cáo", ST_OK),
        (241, 243, "Cụm 2: safe_state, PWR_EN, trip_count", "3 x u8", "Cụm chưa có phần cứng: luôn OFF (1), PWR_EN 0, trip 0", ST_OK),
        (244, 246, "Cụm 3: safe_state, PWR_EN, trip_count", "3 x u8", "Như cụm 2", ST_OK),
        (247, 249, "Cụm 4: safe_state, PWR_EN, trip_count", "3 x u8", "Như cụm 2", ST_OK),
        (250, 250, "Slot firmware", "ASCII", "'A' = app đang chạy, '?' = không xác định (link sai)", ST_OK),
        (251, 254, "Số version cũ", "u32", "Số do BEGIN khai báo (PC gửi 0 thì luôn 0). Version nay là chuỗi: dùng GET_VERSION (99 99)", ST_OK),
        (255, 255, "Lệnh gần nhất", "u8", "0x01 điều khiển, 0x04 ghi cấu hình, 0x00 chưa có lệnh nào", ST_OK),
        (256, 256, "Kết quả lệnh gần nhất", "u8", "0 OK; 1 ghi cấu hình bị từ chối (còn cụm chưa OFF); 2 lệnh bật bị từ chối (TRIPPED hoặc vừa ClearTrip cùng khung); 3 ghi Flash lỗi", ST_OK),
        (257, 258, "Bộ đếm lệnh", "u16", "Số lệnh đã ghi nhận ở 255 (tràn quay vòng). Tăng 1 sau mỗi lệnh điều khiển/ghi cấu hình", ST_OK),
        (259, 260, "rx_frames", "u16", "Số khung hợp lệ (đúng CRC, đúng địa chỉ), cộng gộp 3 cổng", ST_OK),
        (261, 262, "rx_badcrc", "u16", "Số khung sai CRC", ST_OK),
        (263, 264, "rx_badaddr", "u16", "Số khung đúng CRC nhưng sai địa chỉ", ST_OK),
        (265, 266, "rx_uart_err", "u16", "Lỗi UART (khung/nhiễu/tràn)", ST_OK),
        (267, 268, "rx_overrun", "u16", "Bộ đệm nhận DMA gần đầy", ST_OK),
        (269, 270, "tx_sent", "u16", "Số khung đã phát", ST_OK),
        (271, 272, "tx_skipped", "u16", "Bỏ phản hồi vì cổng bận (hoặc RTS# nếu COMM_FT232_CHECK_RTS=1)", ST_OK),
        (273, 274, "tx_error", "u16", "HAL từ chối phát", ST_OK),
        (275, 276, "tx_stuck", "u16", "Phát quá hạn, bị huỷ", ST_OK),
        (277, 280, "Thời gian chạy", "u32", "Giây kể từ lần reset gần nhất", ST_OK),
        (281, 281, "Phiên bản giao thức", "u8", "Hiện 1", ST_OK),
        (282, 305, "Dự phòng", "24 byte", "Giữ 00", ST_RES),
    ] + frame_tail_rows(310)
    r = bytemap(ws, r, rows)
    put(ws, r + 1, ["Ghi chú", "", "", "Kênh INA không 'tươi' (mất liên lạc) thì I_OUT_ADC/TEMP_ADC của DCM đó gửi 00. Bộ đếm 16 bit tràn quay vòng."])
    ws.merge_cells(start_row=r + 1, start_column=4, end_row=r + 1, end_column=7)

    # ------------------------------------------------------------------ 4. Dieu khien
    ws = wb.create_sheet("Dieu khien")
    title(ws, "Điều khiển: 01 01 (PC→PSU, 13 byte, không ACK)")
    header(ws, 3, BYTE_COLS, BYTE_W)
    ws.freeze_panes = "A4"
    rows = frame_head_rows("01 01", 13) + [
        (5, 5, "Mask", "u8, bit c = cụm c+1", "Bit c = 1: bật cụm c+1; 0: tắt. Bit0 = cụm 1 = chân PWR_EN. Bit 1..3 (cụm 2..4) chưa có phần cứng: bỏ qua. Mỗi lệnh ghi đè trạng thái bật/tắt", ST_OK),
        (6, 6, "ClearTrip", "u8", "0x01 = xoá trip code và mọi cờ tích luỹ, mọi cụm TRIPPED về OFF và huỷ yêu cầu bật (áp dụng toàn PSU). 0x00 = không xoá. Không tắt nguồn cụm đang ON", ST_OK),
        (7, 8, "Dự phòng", "2 byte", "Gửi 00", ST_RES),
    ] + frame_tail_rows(13)
    r = bytemap(ws, 4, rows)
    r += 1
    section(ws, r, "Quy tắc xử lý", 7)
    rules = [
        "Cụm đang TRIPPED: lệnh bật bị từ chối (im lặng). PC phải gửi ClearTrip trước, rồi gửi lệnh bật ở KHUNG SAU.",
        "ClearTrip và lệnh bật cùng một khung: cụm vừa thoát TRIPPED bỏ qua lệnh bật trong khung đó (cụm khác bật/tắt theo Mask).",
        "Trình tự bật: OFF -> ARMING (chờ SAFE_ARM_MS = 400 ms, tối thiểu 100 ms ở OFF) -> ON. Khi ON mới so ngưỡng và nạp ngưỡng dưới INA.",
        "Không có ACK: kiểm tra bằng 81 81: byte 238 (safe_state) và 255..258 (lệnh cuối = 0x01, kết quả 0/2, bộ đếm tăng).",
        "Ghi cấu hình, ENTER_REC và nạp code chỉ khi MỌI cụm OFF.",
        "Mọi cụm trip chỉ cắt nguồn và giữ TRIPPED đến ClearTrip (không còn LOCKOUT).",
    ]
    for t in rules:
        r += 1
        put(ws, r, ["•", "", "", t])
        ws.merge_cells(start_row=r, start_column=4, end_row=r, end_column=7)

    # ------------------------------------------------------------------ 5. Cau hinh
    ws = wb.create_sheet("Cau hinh")
    title(ws, "Cấu hình: 03 03 (hỏi), 04 04 (ghi), 82 82 (trả lời)",
          "Ảnh cấu hình 904 byte nằm trong khung 915 byte: byte 7..910. Lưu nguyên vào Flash (2 bank, chốt bằng CRC32).")
    header(ws, 4, BYTE_COLS, BYTE_W)
    ws.freeze_panes = "A5"
    r = 5
    section(ws, r, "03 03 – Hỏi cấu hình (PC→PSU, 13 byte)", 7)
    r += 1
    r = bytemap(ws, r, frame_head_rows("03 03", 13) + [(5, 8, "Dự phòng", "4 byte", "Gửi 00", ST_RES)] + frame_tail_rows(13))
    section(ws, r, "04 04 – Ghi cấu hình (PC→PSU, 915 byte, không ACK) / 82 82 – Trả lời (PSU→PC, 915 byte)", 7)
    r += 1
    rows = frame_head_rows("04 04 / 82 82", 915) + [
        (5, 6, "Mask", "u16", "Firmware bỏ qua, luôn ghi đè toàn bộ. 82 82: gửi 00", ST_OK),
        (7, 910, "Ảnh cấu hình", "904 byte", "Xem bảng bố cục bên dưới (offset trong ảnh = byte - 7)", ST_OK),
    ] + frame_tail_rows(915)
    r = bytemap(ws, r, rows)
    r += 1
    ws.cell(row=r, column=1, value="Bố cục ảnh cấu hình (offset trong ảnh 904 byte, = byte khung - 7)").font = F_BOLD
    r += 1
    img_hdr = ["Offset bắt đầu", "Offset kết thúc", "Số byte", "Khối", "Kiểu", "Ý nghĩa", "Trạng thái"]
    for i, c in enumerate(img_hdr, 1):
        cell = ws.cell(row=r, column=i, value=c)
        cell.font = F_HEAD; cell.fill = FILL_HEAD; cell.border = BORDER
        cell.alignment = Alignment(wrap_text=True, horizontal="center", vertical="center")
    r += 1
    img = [
        (0, 191, "Cụm DCM 1", "192 byte", "Khối cấu hình cụm 1: 36 byte đầu = ngưỡng (xem bên dưới); 156 byte còn lại chỉ lưu và trả lại (dành cho cấu hình DCM/ADC/XDP bản đầy đủ)", ST_OK),
        (192, 383, "Cụm DCM 2", "192 byte", "Cùng bố cục khối cụm. Chỉ lưu và trả lại (cụm chưa có phần cứng)", ST_NA),
        (384, 575, "Cụm DCM 3", "192 byte", "Như cụm 2", ST_NA),
        (576, 767, "Cụm DCM 4", "192 byte", "Như cụm 2", ST_NA),
        (768, 785, "INA 1", "9 x u16", "Thanh ghi INA228 kênh 1 (xem bảng thanh ghi). Nạp xuống IC INA1", ST_OK),
        (786, 803, "INA 2", "9 x u16", "Kênh 2, nạp xuống IC INA2", ST_OK),
        (804, 821, "INA 3", "9 x u16", "Kênh 3, nạp xuống IC INA3", ST_OK),
        (822, 839, "INA 4", "9 x u16", "Kênh 4: chỉ lưu và trả lại. INA4 bị bỏ qua hoàn toàn trên bring-up", ST_NA),
        (840, 887, "Ngưỡng Supply", "8 x (MAX 3 B, MIN 3 B)", "Chỉ lưu và trả lại (cụm Supply chưa có dữ liệu)", ST_NA),
        (888, 903, "Dự phòng", "16 byte", "Chỉ lưu và trả lại", ST_RES),
    ]
    for it in img:
        s, e, blk, typ, mean, st = it
        put(ws, r, [s, e, f"=B{r}-A{r}+1", blk, typ, mean, st], status_col=7)
        r += 1
    r += 1
    ws.cell(row=r, column=1, value="Ngưỡng trong khối cụm (36 byte đầu của mỗi khối 192 byte; offset trong khối)").font = F_BOLD
    r += 1
    for i, c in enumerate(["Offset bắt đầu", "Offset kết thúc", "Số byte", "Trường", "Kiểu", "Ý nghĩa", "Trạng thái"], 1):
        cell = ws.cell(row=r, column=i, value=c)
        cell.font = F_HEAD; cell.fill = FILL_HEAD; cell.border = BORDER
        cell.alignment = Alignment(wrap_text=True, horizontal="center", vertical="center")
    r += 1
    names = ["I_OUT_ADC1", "TEMP_ADC1", "I_OUT_ADC2", "TEMP_ADC2", "I_OUT_ADC3", "TEMP_ADC3"]
    for k, nm in enumerate(names):
        isc = nm.startswith("I_OUT")
        for j, lim in enumerate(["MAX", "MIN"]):
            off = 6 * k + 3 * j
            put(ws, r, [off, off + 2, f"=B{r}-A{r}+1", f"{nm} {lim}", "s24" if isc else "s16 (2 byte thấp)",
                        ("Ngưỡng dòng, so với thanh ghi CURRENT thô, có dấu 24 bit. " if isc else "Ngưỡng nhiệt độ, so với DIETEMP thô, có dấu 16 bit (lấy 2 byte thấp). ")
                        + ("Vượt MAX → trip" if lim == "MAX" else "Dưới MIN → trip") + ". Giá trị 0 = tắt ngưỡng đó", ST_OK], status_col=7)
            r += 1
    put(ws, r, [36, 191, f"=B{r}-A{r}+1", "Phần còn lại của khối cụm", "156 byte", "Chỉ lưu và trả lại", ST_NA], status_col=7)
    r += 2
    ws.cell(row=r, column=1, value="Quy tắc so ngưỡng trên STM").font = F_BOLD
    for t in [
        "Chỉ so sánh khi cụm ở trạng thái ON (ở OFF/ARMING bỏ qua và xoá bộ đếm).",
        "Cần 2 mẫu MỚI liên tiếp vi phạm (PSU_TRIP_CONFIRM_N = 2, mỗi mẫu ~60 ms) mới latch trip; mẫu bình thường xen vào xoá bộ đếm; dữ liệu không tươi cũng xoá bộ đếm.",
        "Hiện chỉ cụm 1 có bảng bit trip (cụm 2..4: ngưỡng được lưu nhưng không dùng).",
        "Trip giữ nguyên đến khi nhận ClearTrip (xem sheet 'Trip code').",
    ]:
        r += 1
        put(ws, r, ["•", "", "", t])
        ws.merge_cells(start_row=r, start_column=4, end_row=r, end_column=7)
    r += 2
    ws.cell(row=r, column=1, value="Khối INA: 9 thanh ghi x 2 byte (big-endian), thứ tự trong khối").font = F_BOLD
    r += 1
    for i, c in enumerate(["Offset trong khối", "Số byte", "Chỉ số", "Thanh ghi INA228", "Địa chỉ INA", "Ý nghĩa / giá trị mặc định", "Ghi chú"], 1):
        cell = ws.cell(row=r, column=i, value=c)
        cell.font = F_HEAD; cell.fill = FILL_HEAD; cell.border = BORDER
        cell.alignment = Alignment(wrap_text=True, horizontal="center", vertical="center")
    r += 1
    regs = [
        ("CONFIG", "0x00", "Mặc định 0x0010. Bit RST / RSTACC bị firmware xoá trước khi nạp", ""),
        ("ADC_CONFIG", "0x01", "Mặc định 0xFB6A", "Đọc ngược định kỳ để phát hiện IC bị reset"),
        ("SHUNT_CAL", "0x02", "Mặc định 0x1000. Bằng 0 nghĩa là 'chưa cấu hình' → cả khối dùng mặc định", ""),
        ("SHUNT_TEMPCO", "0x03", "Mặc định 0x0000", ""),
        ("SOVL", "0x0C", "Ngưỡng trên điện áp shunt. Mặc định 0x7FFF = tắt", ""),
        ("SUVL", "0x0D", "Ngưỡng dưới điện áp shunt. Mặc định 0x8000 = tắt", "Chỉ nạp khi cụm ON (giảm ALERT giả)"),
        ("BOVL", "0x0E", "Ngưỡng trên điện áp bus. Mặc định 0x7FFF = tắt", ""),
        ("BUVL", "0x0F", "Ngưỡng dưới điện áp bus. Mặc định 0x0000 = tắt", "Chỉ nạp khi cụm ON"),
        ("TEMP_LIMIT", "0x10", "Ngưỡng nhiệt độ. Mặc định 0x7FFF = tắt", ""),
    ]
    for k, (nm, addr, mean, note) in enumerate(regs):
        put(ws, r, [2 * k, 2, k, nm, addr, mean, note], mono_cols=(5,))
        r += 1
    r += 1
    for t in [
        "Ghi cấu hình chỉ nhận khi MỌI cụm OFF (nếu không: bị từ chối, 81 81 byte 256 = 1).",
        "Chưa lưu cấu hình thì 82 82 trả cấu hình mặc định (INA mặc định, còn lại 0). Ghi thất bại thì giữ cấu hình cũ (byte 256 = 3).",
        "82 82 trả bản trong RAM (nạp từ Flash khi khởi động, cập nhật sau mỗi lần ghi thành công); không đọc ngược thanh ghi từ IC.",
        "Mapping tạm thời: khối INA n ↔ INA n trên board bring-up. Bản đầy đủ: 4 INA228 nằm ở cụm Supply (rail 24 V, 5 V, 3,3 V, 3,3 V MCU).",
    ]:
        put(ws, r, ["•", "", "", t])
        ws.merge_cells(start_row=r, start_column=4, end_row=r, end_column=7)
        r += 1

    # ------------------------------------------------------------------ 6. Nap code
    ws = wb.create_sheet("Nap code")
    title(ws, "Nạp code và version (chỉ RECOVERY ghi flash; app chỉ INFO / ENTER_REC / GET_VERSION)",
          "Stop-and-wait: gửi một khung, chờ ACK rồi mới gửi khung kế.")
    header(ws, 4, BYTE_COLS, BYTE_W)
    ws.freeze_panes = "A5"
    r = 5

    def msg(title_text, rows):
        nonlocal r
        section(ws, r, title_text, 7)
        r += 1
        r = bytemap(ws, r, rows)

    for name, cmd, extra in [
        ("95 95 – INFO (9 byte, app và recovery)", "95 95", "Kiểm tra kết nối. Trả ACK 94 94 (INFO): app có slot 'A', info = số version cũ, [12] trạng thái trial; recovery có slot 'R'"),
        ("98 98 – ENTER_REC (9 byte)", "98 98", "App: nếu mọi cụm OFF → ghi cờ 'vào recovery' vào Flash, ACK, reset sau ~50 ms; bootloader ở lại recovery. Recovery: ACK OK, không làm gì"),
        ("96 96 – ERASE (9 byte, recovery)", "96 96", "Xóa 5 sector vùng app (640 KB). MỘT ACK duy nhất khi xóa xong (info = 1); lỗi thì ACK status 7 với info = mã lỗi HAL. Xóa luôn chuỗi version của phiên trước"),
        ("92 92 – END (9 byte, recovery)", "92 92", "Kiểm tra tổng số byte và CRC32 đã khai báo; cấp install_seq; ghi header (1 KB, ghi cuối cùng, kèm chuỗi version); kiểm lại CRC từ Flash + vector; ghi bản ghi boot (0, chưa confirm)"),
        ("93 93 – COMMIT (9 byte, recovery)", "93 93", "Chỉ sau END thành công (nếu không: ACK status 6). ACK rồi reset; bootloader chạy app mới"),
        ("99 99 – GET_VERSION (9 byte, app và recovery)", "99 99", "Trả khung A9 A9 (xem bên dưới)"),
    ]:
        msg(name, frame_head_rows(cmd, 9) + [("SEC", "Không có payload. " + extra)] + frame_tail_rows(9))
    msg("90 90 – BEGIN (21 byte, recovery, sau ERASE)", frame_head_rows("90 90", 21) + [
        (5, 8, "Kích thước file", "u32", "Số byte của file .bin (không có header, không đệm). 1 .. 0x9FC00 (639 KB), nếu không: ACK status 2 (info = 0x0009FC00)", ST_OK),
        (9, 12, "CRC32 file", "u32", "CRC32 chuẩn zlib của toàn bộ file .bin; so với CRC firmware tính được ở END", ST_OK),
        (13, 16, "Số version cũ", "u32", "Không dùng nữa, gửi 00 00 00 00 (còn lưu vào header và hiện ở INFO/81 81)", ST_OK),
    ] + frame_tail_rows(21))
    msg("91 91 – DATA (269 byte, recovery)", frame_head_rows("91 91", 269) + [
        (5, 6, "seq", "u16", "Số thứ tự gói, từ 0, liên tiếp. Sai: ACK status 3 (info = seq cuối nhận đúng)", ST_OK),
        (7, 8, "len", "u16", "Số byte hợp lệ trong gói (1..256). Chỉ gói cuối < 256", ST_OK),
        (9, 264, "Dữ liệu", "256 byte", "Nội dung file tại offset seq x 256. Gói cuối đệm FF. Gói 0: Reset_Handler (byte 4..7 của dữ liệu) phải nằm trong vùng app, nếu không ACK status 8 (info = giá trị đọc được)", ST_OK),
    ] + frame_tail_rows(269))
    msg("9A 9A – SET_VERSION (41 byte, recovery, giữa ERASE và END)", frame_head_rows("9A 9A", 41) + [
        (5, 36, "Chuỗi version", "ASCII 32 byte", "Tên file bỏ đuôi, ví dụ PSU_v1.2.3_abc123. Chỉ ký tự 0x20..0x7E; gặp byte khác thì cắt chuỗi tại đó; đệm 00. Ghi vào header khi END. Sai trạng thái: ACK status 5 (BUSY)", ST_OK),
    ] + frame_tail_rows(41))
    msg("A9 A9 – Trả lời GET_VERSION (41 byte)", frame_head_rows("A9 A9", 41) + [
        (5, 36, "Chuỗi version", "ASCII 32 byte", "Chuỗi trong header của ảnh đang có (app: ảnh đang chạy). Rỗng (toàn 00) = chưa đặt / không có ảnh hợp lệ / nạp bằng SWD", ST_OK),
    ] + frame_tail_rows(41))
    msg("94 94 – ACK (17 byte, PSU→PC)", frame_head_rows("94 94", 17) + [
        (5, 5, "AckCmd", "u8", "Byte lặp lại của lệnh được trả lời (95, 98, 96, 90, 91, 9A, 92, 93)", ST_OK),
        (6, 6, "Status", "u8", "0 = OK; mã lỗi xem sheet 'Ma va trang thai'", ST_OK),
        (7, 10, "Info", "u32", "Ý nghĩa phụ thuộc lệnh và status (xem sheet 'Ma va trang thai')", ST_OK),
        (11, 11, "Slot", "ASCII", "'A' = app, 'R' = recovery", ST_OK),
        (12, 12, "State", "u8", "Chỉ có ở ACK của INFO (app): bit7 = đã confirm, bit6..0 = boot_count (127 = đã yêu cầu vào recovery/lớn hơn). Còn lại 0", ST_OK),
    ] + frame_tail_rows(17))
    r += 1
    ws.cell(row=r, column=1, value="Quy trình nạp").font = F_BOLD
    r += 1
    for i, c in enumerate(["Bước", "Gửi", "Chế độ", "Mong đợi", "", "Ghi chú", ""], 1):
        cell = ws.cell(row=r, column=i, value=c)
        cell.font = F_HEAD; cell.fill = FILL_HEAD; cell.border = BORDER
    r += 1
    flow = [
        (1, "INFO", "App/Rec", "ACK, slot 'A' hoặc 'R'", "Nếu 'R' bỏ qua bước 2-3"),
        (2, "ENTER_REC", "App", "ACK status 0, PSU reset", "Chỉ khi mọi cụm OFF (status 1 nếu không)"),
        (3, "INFO lặp lại (200-500 ms)", "Recovery", "Slot 'R'", "Chờ vài giây"),
        (4, "ERASE", "Recovery", "Một ACK, info = 1", "Chờ >= 30 s (chưa đo)"),
        (5, "BEGIN", "Recovery", "ACK 0", "size, CRC32 của file"),
        (6, "SET_VERSION", "Recovery", "ACK 0", "Trước END (thường ngay sau BEGIN)"),
        (7, "DATA seq 0..N-1", "Recovery", "Mỗi gói ACK 0, info = seq", "Mất ACK: gửi lại cùng seq; nhận BAD_SEQ với info = seq đó nghĩa là gói đã ghi"),
        (8, "END", "Recovery", "ACK 0, info 1", "Sai CRC: ACK status 4 (info = CRC tính được), phải ERASE lại"),
        (9, "COMMIT", "Recovery", "ACK 0 rồi reset", ""),
        (10, "INFO + GET_VERSION", "App", "Slot 'A', version đúng", "Sau ~15 s bit7 của State = 1 (tự confirm; cần >= 1 INA)"),
    ]
    for f in flow:
        put(ws, r, [f[0], f[1], f[2], f[3], "", f[4], ""])
        r += 1
    put(ws, r + 1, ["Cứu hộ", "Cấp nguồn lại và gửi INFO mỗi 30-50 ms ngay từ trước khi cấp nguồn (kéo dài ~1 s): bootloader nghe 300 ms sau khi bật, nhận khung hợp lệ thì ở lại recovery."])
    ws.merge_cells(start_row=r + 1, start_column=2, end_row=r + 1, end_column=7)

    # ------------------------------------------------------------------ 7. Ma va trang thai
    ws = wb.create_sheet("Ma va trang thai")
    title(ws, "Mã trạng thái, mã lỗi và ý nghĩa 'info'")
    r = 3
    ws.column_dimensions["A"].width = 14
    ws.column_dimensions["B"].width = 26
    ws.column_dimensions["C"].width = 100

    def table(heads, rows_, r):
        for i, c in enumerate(heads, 1):
            cell = ws.cell(row=r, column=i, value=c)
            cell.font = F_HEAD; cell.fill = FILL_HEAD; cell.border = BORDER
        r += 1
        for row in rows_:
            put(ws, r, list(row))
            r += 1
        return r + 1

    ws.cell(row=r, column=1, value="Status trong ACK (byte 6)").font = F_BOLD
    r += 1
    r = table(["Mã", "Tên", "Ý nghĩa"], [
        (0, "OK", "Thành công"),
        (1, "REFUSED", "Có cụm chưa OFF (ENTER_REC); hoặc không cấp được install_seq / không ghi được bản ghi boot (END)"),
        (2, "BAD_SIZE", "BEGIN: size 0 hoặc > 0x9FC00; DATA: len sai"),
        (3, "BAD_SEQ", "DATA: seq không liên tiếp"),
        (4, "CRC_FAIL", "END: sai tổng byte hoặc CRC32 (hoặc kiểm lại ảnh từ Flash sai)"),
        (5, "BUSY", "Lệnh sai trạng thái (DATA khi chưa BEGIN, END khi chưa nhận, SET_VERSION ngoài ERASE..END)"),
        (6, "NO_COMMIT", "COMMIT khi chưa END thành công"),
        (7, "ERASE_FAIL", "Xóa Flash lỗi (info = mã lỗi HAL)"),
        (8, "WRONG_SLOT", "DATA chunk 0: Reset_Handler nằm ngoài vùng app (ảnh link sai); ENTER_REC: không biết đang chạy vùng nào"),
        (9, "PROG_FAIL", "Ghi Flash lỗi (info = địa chỉ hoặc mã lỗi HAL); phải ERASE lại"),
        (10, "(dự phòng)", "Trước đây NO_TARGET của ROLLBACK, đã bỏ"),
        (11, "NOT_ERASED", "BEGIN khi chưa ERASE"),
        (12, "NOT_RECOVERY", "App nhận lệnh nạp (ERASE/BEGIN/DATA/END/COMMIT/SET_VERSION): phải ENTER_REC trước"),
    ], r)
    ws.cell(row=r, column=1, value="Ý nghĩa 'info' (byte 7..10 của ACK)").font = F_BOLD
    r += 1
    r = table(["Lệnh", "Status", "Info"], [
        ("INFO", "0", "App: số version cũ (u32, header); Recovery: 0"),
        ("ERASE", "0 / 7", "OK: 1 (xóa xong); ERASE_FAIL: mã lỗi HAL Flash"),
        ("BEGIN", "2 / 11", "BAD_SIZE: kích thước tối đa 0x0009FC00; NOT_ERASED: 0"),
        ("DATA", "0", "Số seq vừa ghi"),
        ("DATA", "3", "seq cuối đã nhận đúng (0 nếu chưa nhận gói nào – trùng với seq 0 đã nhận)"),
        ("DATA", "8 / 9", "WRONG_SLOT: giá trị Reset_Handler đọc từ chunk 0; PROG_FAIL: địa chỉ Flash đang ghi"),
        ("END", "0", "1"),
        ("END", "4", "CRC32 tính được từ dữ liệu đã nhận (so với CRC khai báo ở BEGIN); 0 nếu lỗi ở bước kiểm lại từ Flash"),
        ("END", "9 / 1", "PROG_FAIL: mã lỗi HAL ghi header; REFUSED: 0 (không cấp được install_seq) hoặc 1 (không ghi được bản ghi boot)"),
        ("COMMIT / SET_VERSION / ENTER_REC / các lệnh khác", "bất kỳ", "0"),
    ], r)
    ws.cell(row=r, column=1, value="Byte slot và state (ACK byte 11, 12)").font = F_BOLD
    r += 1
    r = table(["Giá trị", "Trường", "Ý nghĩa"], [
        ("'A'", "Slot", "App đang chạy"),
        ("'R'", "Slot", "Recovery (bootloader)"),
        ("'?'", "Slot", "Không xác định (link sai vùng)"),
        ("bit7", "State (INFO của app)", "1 = app đã tự xác nhận (confirmed); không còn bị đếm lần thử"),
        ("bit6..0", "State", "boot_count: số lần khởi động chưa xác nhận (tối đa 3, từ lần 4 app bị loại và vào recovery); 127 = đã yêu cầu vào recovery"),
    ], r)
    ws.cell(row=r, column=1, value="safe_state của cụm (81 81 byte 238, 241, 244, 247)").font = F_BOLD
    r += 1
    r = table(["Giá trị", "Tên", "Ý nghĩa"], [
        (0, "BOOT", "Chưa armed, bỏ qua ALERT; giữ ~500 ms sau khi khởi động (3 đèn LED sáng)"),
        (1, "OFF", "Nguồn tắt theo lệnh PC"),
        (2, "ARMING", "Vừa bật, đang chờ qua inrush (400 ms)"),
        (3, "ON", "Chạy bình thường, giám sát đầy đủ"),
        (4, "TRIPPED", "Đã cắt nguồn do lỗi; giữ đến khi nhận ClearTrip"),
    ], r)
    ws.cell(row=r, column=1, value="Kết quả lệnh gần nhất (81 81 byte 256)").font = F_BOLD
    r += 1
    r = table(["Giá trị", "Tên", "Ý nghĩa"], [
        (0, "OK", "Lệnh được chấp nhận"),
        (1, "NOT_OFF", "Ghi cấu hình bị từ chối: còn cụm chưa OFF"),
        (2, "REFUSED", "Lệnh bật bị từ chối: cụm đang TRIPPED, hoặc vừa ClearTrip trong cùng khung"),
        (3, "FLASH_FAIL", "Ghi cấu hình vào Flash thất bại (giữ cấu hình cũ)"),
    ], r)

    # ------------------------------------------------------------------ 8. Trip code
    ws = wb.create_sheet("Trip code")
    title(ws, "Trip code (81 81 byte 228..237) – BẢNG TẠM THỜI chờ bảng chính thức",
          "Chỉ cụm 1 có bảng bit. Trip latch đến khi ClearTrip. Sửa bảng: PSU_stm32h7/App/psu_trip.h.")
    header(ws, 4, ["Tên", "Byte trong khung", "Bit", "Nguồn / điều kiện", "Trạng thái"], [10, 16, 8, 90, 30])
    ws.freeze_panes = "A5"
    r = 5
    trip = []
    for n in range(3):
        trip.append(("TRIP1", 228, n, f"I_OUT_ADC{n+1} vượt MAX (so sánh trên STM)"))
    for n in range(3):
        trip.append(("TRIP1", 228, 3 + n, f"TEMP_ADC{n+1} vượt MAX (so sánh trên STM)"))
    for n in range(3):
        trip.append(("TRIP2", 229, n, f"I_OUT_ADC{n+1} dưới MIN (so sánh trên STM)"))
    for n in range(3):
        trip.append(("TRIP2", 229, 3 + n, f"TEMP_ADC{n+1} dưới MIN (so sánh trên STM)"))
    for n in range(3):
        trip.append(("TRIP3", 230, n, f"INA{n+1} báo SHNTOL/POL (quá dòng / công suất)"))
    for n in range(3):
        trip.append(("TRIP3", 230, 3 + n, f"INA{n+1} báo SHNTUL (dưới ngưỡng shunt)"))
    for n in range(3):
        trip.append(("TRIP4", 231, n, f"INA{n+1} báo BUSOL (quá áp bus)"))
    for n in range(3):
        trip.append(("TRIP4", 231, 3 + n, f"INA{n+1} báo BUSUL (sụt áp bus)"))
    for n in range(3):
        trip.append(("TRIP5", 232, n, f"INA{n+1} báo TMPOL (quá nhiệt INA)"))
    for n in range(4):
        trip.append(("TRIP5", 232, 3 + n, f"Chân FT của DCM{n+1} báo lỗi"))
    trip += [
        ("TRIP6", 233, 0, "Mất liên lạc I2C với INA (chỉ khi SAFE_TRIP_ON_BUS_LOSS=1; mặc định 0 = chỉ cảnh báo LED, không trip)"),
        ("TRIP6", 233, 1, "Cắt do ALERT nhưng không xác nhận được nguyên nhân"),
        ("TRIP6", 233, 3, "Cấu hình INA không hợp lệ"),
    ]
    for t in trip:
        put(ws, r, [t[0], t[1], t[2], t[3], ST_TMP], status_col=5)
        ws.cell(row=r, column=3).alignment = Alignment(horizontal="center", vertical="top")
        r += 1
    for k, byte in zip(range(7, 11), range(234, 238)):
        put(ws, r, [f"TRIP{k}", byte, "0..7", "Dự phòng", ST_RES], status_col=5)
        r += 1
    r += 1
    put(ws, r, ["Ghi chú", "", "", "Các bit không liệt kê = 0. Các nguồn lỗi của bản đầy đủ (điện áp vào 300 V, XDP710, ADC, rail Supply...) chưa có bit. Bảng chính thức do người dùng quy định."])
    ws.merge_cells(start_row=r, start_column=4, end_row=r, end_column=5)

    # ------------------------------------------------------------------ 9. Vi du khung
    ws = wb.create_sheet("Vi du khung")
    title(ws, "Khung mẫu có CRC (địa chỉ PSU = 1), do firmware sinh",
          "Nguồn: docs/PSU_FRAME_EXAMPLES.md (make -C tests/host examples). Cột kiểm tra: độ dài tính từ chuỗi hex phải bằng độ dài khai báo.")
    header(ws, 4, ["Bản tin", "Dài khai báo", "Dài tính từ hex", "Kiểm tra", "Ghi chú", "Khung (hex)"], [44, 13, 15, 10, 70, 120])
    ws.freeze_panes = "A5"
    r = 5
    if os.path.exists(EXAMPLES):
        text = open(EXAMPLES, encoding="utf-8").read()
        for m in re.finditer(r"### (.*?) \((\d+) byte\)\n(.*?)\n\n```\n(.*?)```", text, re.S):
            name, length, note, hexs = m.group(1), int(m.group(2)), m.group(3).strip(), " ".join(m.group(4).split())
            put(ws, r, [name, length, f"=(LEN(F{r})+1)/3", f'=IF(B{r}=C{r},"OK","SAI")', note, hexs], mono_cols=(6,))
            ws.cell(row=r, column=6).alignment = Alignment(wrap_text=True, vertical="top")
            r += 1
    else:
        put(ws, r, ["(không tìm thấy docs/PSU_FRAME_EXAMPLES.md)"])

    from openpyxl.worksheet.properties import PageSetupProperties
    for w in wb.worksheets:
        w.sheet_view.zoomScale = 100
        w.page_setup.orientation = "landscape"
        w.page_setup.fitToWidth = 1          # in ra vua kho giay, nhieu trang theo chieu doc
        w.page_setup.fitToHeight = 0
        w.sheet_properties.pageSetUpPr = PageSetupProperties(fitToPage=True)
    return wb


if __name__ == "__main__":
    wb = build()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    wb.save(OUT)
    print("wrote", OUT)
