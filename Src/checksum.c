#include "checksum.h"

/* -------------------------------------------------------------------------
 * Sum-8 bù 2
 * ---------------------------------------------------------------------- */
uint8_t cks_sum8(const uint8_t *d, size_t n)
{
    uint8_t s = 0;
    while (n--) {
        s = (uint8_t)(s + *d++);
    }
    return (uint8_t)(~s + 1u);      /* bù 2 -> tổng toàn khung = 0 */
}

/* -------------------------------------------------------------------------
 * CRC-8/ATM  (poly 0x07, init 0x00)
 * ---------------------------------------------------------------------- */
uint8_t cks_crc8(const uint8_t *d, size_t n)
{
    uint8_t crc = 0x00u;
    while (n--) {
        crc ^= *d++;
        for (int i = 0; i < 8; i++) {
            crc = (uint8_t)((crc & 0x80u) ? (((unsigned)crc << 1) ^ 0x07u)
                                          :  ((unsigned)crc << 1));
        }
    }
    return crc;
}

/* -------------------------------------------------------------------------
 * CRC-16/CCITT-FALSE  (poly 0x1021, init 0xFFFF)
 *
 * Vì sao chọn biến thể này?
 *  - Là biến thể "chuẩn" mà mọi công cụ online (crccalc.com) gọi là
 *    CRC-16/CCITT-FALSE hoặc CRC-16/IBM-3740 -> dễ đối chiếu khi báo cáo.
 *  - Không đảo bit (reflect) -> cài trên C và trên Python giống hệt nhau,
 *    tránh lỗi "PC tính ra một đằng, board tính ra một nẻo".
 *  - Khoảng cách Hamming d=4 với khung < 32 KB: phát hiện được MỌI lỗi 1, 2, 3
 *    bit, mọi lỗi burst <= 16 bit, và mọi lỗi lẻ số bit.
 * ---------------------------------------------------------------------- */
uint16_t cks_crc16_update(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)b << 8;
    for (int i = 0; i < 8; i++) {
        crc = (uint16_t)((crc & 0x8000u) ? (((unsigned)crc << 1) ^ 0x1021u)
                                         :  ((unsigned)crc << 1));
    }
    return crc;
}

uint16_t cks_crc16(const uint8_t *d, size_t n)
{
    uint16_t crc = 0xFFFFu;
    while (n--) {
        crc = cks_crc16_update(crc, *d++);
    }
    return crc;
}
