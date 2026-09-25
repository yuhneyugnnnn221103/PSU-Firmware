#ifndef CRC32_SW_H_
#define CRC32_SW_H_

#include <stdint.h>

/** @brief Khoi tao gia tri "crc" dau vao cho lan goi Crc32_Update() dau
 *         tien cua mot chuoi tinh moi. */
#define CRC32_INIT   0xFFFFFFFFu

/** @brief Cap nhat CRC32 (chuan IEEE 802.3/zlib, poly 0xEDB88320 dao bit)
 *         tren [data, data+len). Goi lien tiep qua nhieu doan, truyen lai
 *         gia tri tra ve lam dau vao cho doan tiep theo. */
uint32_t Crc32_Update(uint32_t crc, const uint8_t *data, uint32_t len);

/** @brief XOR ra ket qua CRC32 cuoi cung tu gia tri "crc" dang chay.
 *         CHI goi MOT LAN, sau khi da Update() het du lieu. */
static inline uint32_t Crc32_Finalize(uint32_t crc) { return crc ^ 0xFFFFFFFFu; }

/** @brief Tien ich: tinh CRC32 tron 1 lan tren vung nho (vd doc truc tiep
 *         tu Flash de verify). Tuong duong Crc32_Finalize(Crc32_Update(
 *         CRC32_INIT, data, len)). */
uint32_t Crc32_Compute(const uint8_t *data, uint32_t len);

#endif /* CRC32_SW_H_ */
