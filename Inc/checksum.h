/**
 * @file  checksum.h
 * @brief Ba kiểu mã kiểm tra dùng cho giao thức V01: Sum-8, CRC-8, CRC-16.
 *
 * Module này KHÔNG phụ thuộc HAL -> biên dịch và unit-test được trên PC (gcc).
 */
#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include <stddef.h>

/** Sum-8 bù 2: tổng của (dữ liệu + checksum) = 0. */
uint8_t  cks_sum8(const uint8_t *d, size_t n);

/** CRC-8/ATM: poly 0x07, init 0x00, không đảo bit, xorout 0x00. */
uint8_t  cks_crc8(const uint8_t *d, size_t n);

/** CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, không đảo bit, xorout 0x0000.
 *  Kiểm chứng: crc16("123456789") == 0x29B1 */
uint16_t cks_crc16(const uint8_t *d, size_t n);

/** Phiên bản cộng dồn từng byte của CRC-16 (dùng khi muốn tính trên luồng). */
uint16_t cks_crc16_update(uint16_t crc, uint8_t b);

#endif /* CHECKSUM_H */
