/**
 * @file  dht11.h
 * @brief Driver DHT11/DHT22 dùng đếm chu kỳ DWT (độ phân giải ~10 ns).
 *
 * Vì sao dùng DWT chứ không dùng HAL_Delay hay một Timer riêng?
 *   - HAL_Delay chỉ có độ phân giải 1 ms, trong khi DHT11 phân biệt bit 0/1
 *     bằng độ rộng xung 26 us và 70 us -> bắt buộc phải đo ở mức micro giây.
 *   - DWT->CYCCNT là bộ đếm chu kỳ CPU có sẵn trên Cortex-M4, không tốn
 *     thêm một Timer nào của nhóm.
 *
 * Hàm dht11_read() có chặn ~20 ms. Đây là lý do nó được gọi từ
 * app_cmd_poll() (vòng lặp chính) chứ không phải từ trong ngắt.
 */
#ifndef DHT11_H
#define DHT11_H

#include "protocol.h"

typedef struct {
    int16_t  temp_x10;      /* nhiệt độ  x10, ví dụ 275 = 27.5 °C */
    uint16_t hum_x10;       /* độ ẩm     x10, ví dụ 620 = 62.0 %  */
} dht11_data_t;

/** Bật bộ đếm chu kỳ DWT. Gọi một lần trong main() trước vòng lặp. */
void        dht11_init(void);

/** Đọc cảm biến. @return ERR_OK / ERR_SENSOR / ERR_TIMEOUT. */
proto_err_t dht11_read(dht11_data_t *out);

#endif /* DHT11_H */
