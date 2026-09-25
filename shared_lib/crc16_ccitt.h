#ifndef CRC16_CCITT_H_
#define CRC16_CCITT_H_

#include <stdint.h>

uint16_t Crc16_Compute(const uint8_t *data, uint32_t len);

#endif /* CRC16_CCITT_H_ */
