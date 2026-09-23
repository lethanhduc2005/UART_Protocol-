/**
 * @file  ringbuf.h
 * @brief Bộ đệm vòng 1 nhà sản xuất – 1 người tiêu thụ (SPSC), lock-free.
 *
 * Quy ước AN TOÀN NGẮT (rất quan trọng, hay bị hỏi khi bảo vệ):
 *   - CHỈ một phía được ghi `head`  (bên đẩy dữ liệu vào)
 *   - CHỈ một phía được ghi `tail`  (bên lấy dữ liệu ra)
 *   - cả hai chỉ ĐỌC biến của phía kia
 * Nhờ vậy không cần tắt ngắt, miễn là head/tail là `volatile` và kích thước
 * bộ đệm là luỹ thừa của 2 (để phép & thay cho phép %).
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t          *buf;
    uint16_t          size;         /* PHẢI là luỹ thừa của 2 */
    volatile uint16_t head;         /* chỉ bên ghi được sửa   */
    volatile uint16_t tail;         /* chỉ bên đọc được sửa   */
} ringbuf_t;

void     rb_init (ringbuf_t *rb, uint8_t *storage, uint16_t size_pow2);
uint16_t rb_count(const ringbuf_t *rb);
uint16_t rb_free (const ringbuf_t *rb);

/** Ghi tối đa @p n byte. @return số byte thực sự ghi được. */
uint16_t rb_write(ringbuf_t *rb, const uint8_t *d, uint16_t n);

/** Đọc 1 byte. @return 1 nếu có dữ liệu, 0 nếu rỗng. */
uint8_t  rb_read1(ringbuf_t *rb, uint8_t *out);

/**
 * Trả về vùng dữ liệu LIÊN TỤC đang chờ đọc (để nạp thẳng cho DMA).
 * @param[out] ptr  con trỏ tới byte đầu tiên
 * @return          số byte liên tục (0 nếu rỗng)
 */
uint16_t rb_peek_linear(const ringbuf_t *rb, const uint8_t **ptr);

/** Báo cho bộ đệm biết đã tiêu thụ xong @p n byte lấy từ rb_peek_linear(). */
void     rb_consume(ringbuf_t *rb, uint16_t n);

#endif /* RINGBUF_H */
