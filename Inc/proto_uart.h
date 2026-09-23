/**
 * @file  proto_uart.h
 * @brief Lớp keo giữa giao thức và HAL UART của STM32 (SV1 sở hữu).
 *
 * Trách nhiệm:
 *   - Nhận byte từ UART (bằng ngắt hoặc DMA circular) mà KHÔNG mất byte.
 *   - Đẩy từng byte vào parser; khi có khung hợp lệ thì gọi callback.
 *   - Gửi khung đi qua một HÀNG ĐỢI TX + DMA, để việc streaming 100 Hz
 *     không bao giờ chặn vòng lặp chính và không chen ngang khung khác.
 */
#ifndef PROTO_UART_H
#define PROTO_UART_H

#include "protocol.h"

struct __UART_HandleTypeDef;

/** Kiểu hàm xử lý khi nhận được một khung hợp lệ. */
typedef void (*proto_frame_cb_t)(const proto_frame_t *f);

/** Khởi tạo. Gọi SAU MX_USARTx_UART_Init(). */
void puart_init(struct __UART_HandleTypeDef *huart, proto_frame_cb_t cb);

/** Gọi liên tục trong vòng lặp chính: rút byte đã nhận và bơm hàng đợi TX. */
void puart_poll(void);

/**
 * Đóng gói và xếp một khung vào hàng đợi gửi.
 * @return 1 nếu xếp được, 0 nếu hàng đợi đầy (đếm vào stats.tx_drop).
 * Hàm này KHÔNG chặn (non-blocking).
 */
uint8_t puart_send(uint8_t cmd, const uint8_t *data, uint8_t len);

/** Gửi trả lời cho một khung đã nhận: giữ nguyên SEQ, CMD | 0x80. */
uint8_t puart_reply(const proto_frame_t *req, const uint8_t *data, uint8_t len);

/** Gửi NACK cho một khung: CMD = 0xFF, DATA = [cmd_gốc][mã lỗi]. */
uint8_t puart_nack(const proto_frame_t *req, proto_err_t err);

/** Gửi khung với SEQ tự tăng (dùng cho STREAM_DATA). */
uint8_t puart_send_auto(uint8_t cmd, const uint8_t *data, uint8_t len);

/** Số byte còn trống trong hàng đợi TX (để tầng streaming tự điều tiết). */
uint16_t puart_tx_space(void);

proto_stats_t *puart_stats(void);

#endif /* PROTO_UART_H */
