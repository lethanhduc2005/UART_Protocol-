/**
 * @file  app_cmd.h
 * @brief Tầng ỨNG DỤNG: thực thi lệnh và đóng gói phản hồi (SV2 sở hữu).
 */
#ifndef APP_CMD_H
#define APP_CMD_H

#include "protocol.h"

/** Khởi tạo tầng ứng dụng (gọi sau puart_init). */
void app_cmd_init(void);

/** Hàm được puart gọi mỗi khi có khung hợp lệ. Đăng ký ở puart_init(). */
void app_cmd_on_frame(const proto_frame_t *f);

/** Việc chạy nền của tầng ứng dụng (đọc DHT11 chậm...). Gọi trong main loop. */
void app_cmd_poll(void);

#endif /* APP_CMD_H */
