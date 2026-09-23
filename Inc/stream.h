/**
 * @file  stream.h
 * @brief Tầng TRUYỀN LIÊN TỤC (streaming) — phần MỞ RỘNG của đề V01.
 *
 * Ý tưởng cốt lõi để đạt 100 Hz (và hơn nữa) mà CPU gần như rảnh rỗi:
 *
 *   TIM2 (TRGO)  ->  ADC1  ->  DMA circular  ->  bộ đệm đôi
 *                                                  |
 *                              nửa đầy / đầy  ->   cờ  ->  vòng lặp chính
 *                                                  |
 *                                       gom N mẫu vào 1 khung STREAM_DATA
 *                                                  |
 *                                    hàng đợi TX  ->  UART TX DMA
 *
 * KHÔNG có HAL_Delay, KHÔNG có vòng lặp chờ, KHÔNG gửi từng mẫu một.
 * Gom N mẫu vào một khung là điểm mấu chốt: ở 100 Hz với N = 20, board chỉ
 * phát 5 khung/giây thay vì 100 khung/giây, chi phí đóng khung giảm 20 lần.
 */
#ifndef STREAM_H
#define STREAM_H

#include "protocol.h"

/* Nguồn dữ liệu */
#define STREAM_SRC_ADC1CH   0u  /* 1 kênh ADC (PA0)                        */
#define STREAM_SRC_ADC2CH   1u  /* 2 kênh ADC xen kẽ (PA0, PA1)            */
#define STREAM_SRC_TEST     2u  /* sóng sin sinh bằng phần mềm — demo/đo
                                   hiệu năng mà không cần cắm cảm biến     */

#define STREAM_BATCH_MAX    60u
#define STREAM_RATE_MAX     20000u

void        stream_init(void);

/** Bật streaming. @return ERR_OK hoặc mã lỗi để trả NACK. */
proto_err_t stream_start(uint8_t src, uint16_t rate_hz, uint8_t batch);

void        stream_stop(void);
uint8_t     stream_is_running(void);

/** Gọi trong vòng lặp chính: gom mẫu thành khung và xếp vào hàng đợi TX. */
void        stream_poll(void);

/** Số khung đã bị bỏ vì hàng đợi TX đầy (chứng minh cơ chế tự điều tiết). */
uint32_t    stream_dropped(void);

#endif /* STREAM_H */
