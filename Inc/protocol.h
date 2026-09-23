/**
 * @file  protocol.h
 * @brief Tầng KHUNG (framing layer) của giao thức V01 — phần việc của SV1.
 *
 * Khung dữ liệu v1.1:
 *
 *   +------+------+-----+-----+-----+----------+----------+
 *   | SOF1 | SOF2 | LEN | SEQ | CMD | DATA[LEN]|   CKS    |
 *   | 0xAA | 0x55 |     |     |     |          | 1 or 2 B |
 *   +------+------+-----+-----+-----+----------+----------+
 *      0      1     2     3     4     5..         ...
 *
 *   - CKS được tính trên LEN, SEQ, CMD, DATA (KHÔNG gồm 2 byte SOF).
 *     Lý do: 2 byte SOF là hằng số, đưa vào CRC không tăng khả năng phát hiện
 *     lỗi mà lại khiến việc tự đồng bộ khó hơn.
 *   - CRC-16 truyền little-endian (byte thấp trước).
 *
 * Module này KHÔNG phụ thuộc HAL -> unit-test được bằng gcc trên PC.
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "proto_cfg.h"

/* ------------------------------------------------------------------ */
/* Hằng số khung                                                       */
/* ------------------------------------------------------------------ */
#define PROTO_SOF1          0xAAu
#define PROTO_SOF2          0x55u
#define PROTO_HDR_LEN       5u      /* SOF1 SOF2 LEN SEQ CMD */

#if (PROTO_CKS_MODE == PROTO_CKS_CRC16)
#  define PROTO_CKS_LEN     2u
#else
#  define PROTO_CKS_LEN     1u
#endif

#define PROTO_MAX_FRAME     (PROTO_HDR_LEN + PROTO_MAX_DATA + PROTO_CKS_LEN)

/* ------------------------------------------------------------------ */
/* Bảng lệnh                                                           */
/* ------------------------------------------------------------------ */
/* Quy ước: PC -> board dùng mã < 0x80.
 *          board -> PC trả lời bằng chính mã đó OR 0x80.
 *          0xFF = NACK, 0xF0 = STREAM_DATA (board tự phát, không ai hỏi). */
#define CMD_RESP_FLAG       0x80u

typedef enum {
    CMD_PING          = 0x01,   /* -> (rỗng)                 <- (rỗng)      */
    CMD_GET_VERSION   = 0x02,   /* -> (rỗng)                 <- 4 byte      */
    CMD_ECHO          = 0x05,   /* -> N byte                 <- N byte      */

    CMD_LED_SET       = 0x10,   /* -> [id][state]            <- [mask]      */
    CMD_LED_GET       = 0x11,   /* -> (rỗng)                 <- [mask]      */
    CMD_PWM_SET       = 0x12,   /* -> [ch][duty_lo][duty_hi] <- [ch][duty]  */

    CMD_ADC_READ      = 0x20,   /* -> [ch]        <- [ch][raw16][mv16]      */
    CMD_DHT_READ      = 0x21,   /* -> (rỗng)      <- [st][temp16][hum16]    */

    CMD_STREAM_START  = 0x30,   /* -> [src][rate16][batch]   <- (rỗng)      */
    CMD_STREAM_STOP   = 0x31,   /* -> (rỗng)                 <- (rỗng)      */

    CMD_STATS_GET     = 0x40,   /* -> (rỗng)      <- struct 22 byte         */
    CMD_STATS_RESET   = 0x41,   /* -> (rỗng)                 <- (rỗng)      */

    CMD_STREAM_DATA   = 0xF0,   /* board -> PC, không cần hỏi               */
    CMD_NACK          = 0xFF    /* board -> PC: [cmd_gốc][err]              */
} proto_cmd_t;

/* ------------------------------------------------------------------ */
/* Mã lỗi                                                              */
/* ------------------------------------------------------------------ */
typedef enum {
    ERR_OK            = 0x00,
    ERR_CKS           = 0x01,   /* sai checksum/CRC                        */
    ERR_LEN           = 0x02,   /* LEN vượt PROTO_MAX_DATA / sai độ dài    */
    ERR_UNKNOWN_CMD   = 0x03,
    ERR_PARAM         = 0x04,   /* tham số ngoài miền cho phép             */
    ERR_BUSY          = 0x05,   /* đang streaming, không nhận lệnh này     */
    ERR_TIMEOUT       = 0x06,   /* inter-byte timeout giữa khung           */
    ERR_NOT_SUPPORTED = 0x07,
    ERR_SENSOR        = 0x08,   /* cảm biến không phản hồi / sai checksum  */
    ERR_OVERRUN       = 0x09    /* hàng đợi TX đầy, khung bị bỏ            */
} proto_err_t;

/* ------------------------------------------------------------------ */
/* Khung đã giải mã                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t seq;
    uint8_t cmd;
    uint8_t len;
    uint8_t data[PROTO_MAX_DATA];
} proto_frame_t;

/* ------------------------------------------------------------------ */
/* Thống kê (SV4 dùng để đo tỉ lệ khung đúng)                          */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t rx_bytes;      /* tổng byte đã đưa vào parser                */
    uint32_t rx_ok;         /* khung hợp lệ                                */
    uint32_t err_cks;       /* khung sai CRC                               */
    uint32_t err_len;       /* LEN không hợp lệ                            */
    uint32_t err_timeout;   /* khung dở dang bị timeout                    */
    uint32_t resync;        /* số lần parser phải tự đồng bộ lại           */
    uint32_t tx_frames;     /* khung đã gửi đi                             */
    uint32_t tx_drop;       /* khung bị bỏ do hàng đợi TX đầy              */
} proto_stats_t;

/* ------------------------------------------------------------------ */
/* Máy trạng thái phân tích khung                                      */
/* ------------------------------------------------------------------ */
typedef enum {
    ST_SOF1 = 0, ST_SOF2, ST_LEN, ST_SEQ, ST_CMD, ST_DATA, ST_CKS
} proto_state_t;

typedef struct {
    proto_state_t state;
    proto_frame_t f;            /* khung đang dựng                         */
    uint8_t  idx;               /* chỉ số trong DATA hoặc trong CKS        */
    uint8_t  cks_buf[2];

    /* vùng đệm phục vụ tự đồng bộ lại: mọi byte đã nuốt sau SOF2         */
    uint8_t  swallowed[PROTO_MAX_FRAME];
    uint16_t swallowed_len;

    /* hàng chờ phát lại khi phải tự đồng bộ                               */
    uint8_t  pend[2u * PROTO_MAX_FRAME];
    uint16_t pend_head;             /* vị trí đọc  */
    uint16_t pend_tail;             /* vị trí ghi  */

    uint32_t t_last_ms;         /* mốc thời gian byte gần nhất             */
    proto_err_t last_err;
    proto_stats_t *stats;       /* có thể NULL                             */

    /* Được gọi khi một khung bị loại. @p partial là phần header đọc được —
       KHÔNG đáng tin (chính CRC đã sai), chỉ dùng để trả NACK "best effort"
       cho PC biết. Có thể NULL. */
    void (*on_error)(const proto_frame_t *partial, proto_err_t e);
} proto_parser_t;

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

/** Khởi tạo parser. @p stats có thể NULL nếu không cần thống kê. */
void proto_parser_init(proto_parser_t *p, proto_stats_t *stats);

/**
 * Nạp 1 byte vào parser.
 * @return con trỏ tới khung hợp lệ vừa hoàn tất, hoặc NULL.
 *         Con trỏ chỉ có hiệu lực cho tới lần gọi proto_feed() kế tiếp.
 */
const proto_frame_t *proto_feed(proto_parser_t *p, uint8_t b, uint32_t now_ms);

/** Gọi định kỳ trong vòng lặp chính để phát hiện inter-byte timeout. */
void proto_tick(proto_parser_t *p, uint32_t now_ms);

/**
 * Đóng gói một khung vào @p out.
 * @param out phải có ít nhất PROTO_MAX_FRAME byte.
 * @return tổng số byte của khung, hoặc 0 nếu len không hợp lệ.
 */
uint16_t proto_build(uint8_t *out, uint8_t seq, uint8_t cmd,
                     const uint8_t *data, uint8_t len);

/** Tên trạng thái (để in log / vẽ FSM trong báo cáo). */
const char *proto_state_name(proto_state_t s);

#endif /* PROTOCOL_H */
