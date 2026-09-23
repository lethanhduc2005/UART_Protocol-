#include <string.h>
#include "main.h"                 /* CubeIDE sinh ra, kéo theo HAL */
#include "proto_uart.h"
#include "ringbuf.h"

/* ================================================================== */
/* Trạng thái module                                                   */
/* ================================================================== */
static UART_HandleTypeDef *s_h;
static proto_frame_cb_t    s_cb;
static proto_parser_t      s_parser;
static proto_stats_t       s_stats;
static uint8_t             s_seq_tx;

/* ---- đường nhận --------------------------------------------------- */
#if (PROTO_RX_MODE == PROTO_RX_DMA)
static uint8_t  s_rx_dma[PROTO_RX_DMA_SIZE];
static uint16_t s_rx_rd;                    /* vị trí đã đọc tới       */
#else
static uint8_t   s_rx_byte;
static uint8_t   s_rx_store[PROTO_RX_DMA_SIZE];
static ringbuf_t s_rx_rb;
#endif

/* ---- đường gửi ---------------------------------------------------- */
static uint8_t            s_tx_store[PROTO_TX_RB_SIZE];
static ringbuf_t          s_tx_rb;
static volatile uint8_t   s_tx_busy;
static volatile uint16_t  s_tx_chunk;

/* ================================================================== */
/* TX: bơm hàng đợi ra DMA                                             */
/* ================================================================== */
static void tx_pump(void)
{
    /* Hàm này được gọi từ CẢ vòng lặp chính LẪN ngắt TxCplt, nên phải
       bảo vệ cờ s_tx_busy bằng một đoạn găng rất ngắn.                */
    uint32_t prim = __get_PRIMASK();
    __disable_irq();
    if (s_tx_busy) {
        __set_PRIMASK(prim);
        return;
    }
    const uint8_t *p = 0;
    uint16_t n = rb_peek_linear(&s_tx_rb, &p);
    if (n == 0u) {
        __set_PRIMASK(prim);
        return;
    }
    s_tx_busy  = 1;
    s_tx_chunk = n;
    __set_PRIMASK(prim);

    if (HAL_UART_Transmit_DMA(s_h, (uint8_t *)p, n) != HAL_OK) {
        s_tx_busy = 0;              /* thử lại ở lần poll kế tiếp */
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != s_h) {
        return;
    }
    rb_consume(&s_tx_rb, s_tx_chunk);
    s_tx_busy = 0;
    tx_pump();                       /* gửi tiếp phần còn lại nếu có */
}

/* ================================================================== */
/* RX                                                                  */
/* ================================================================== */
#if (PROTO_RX_MODE == PROTO_RX_IT)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != s_h) {
        return;
    }
    rb_write(&s_rx_rb, (const uint8_t *)&s_rx_byte, 1);
    HAL_UART_Receive_IT(s_h, &s_rx_byte, 1);       /* nạp lại ngay */
}
#endif

/* Lỗi khung/parity/overrun: phải xoá cờ và KHỞI ĐỘNG LẠI việc nhận,
   nếu không UART sẽ "chết" sau lần đầu rút dây — đây là lỗi kinh điển. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != s_h) {
        return;
    }
    __HAL_UART_CLEAR_OREFLAG(s_h);
    __HAL_UART_CLEAR_NEFLAG(s_h);
    __HAL_UART_CLEAR_FEFLAG(s_h);
    __HAL_UART_CLEAR_PEFLAG(s_h);

#if (PROTO_RX_MODE == PROTO_RX_DMA)
    HAL_UART_DMAStop(s_h);
    s_rx_rd = 0;
    HAL_UART_Receive_DMA(s_h, s_rx_dma, PROTO_RX_DMA_SIZE);
#else
    HAL_UART_Receive_IT(s_h, &s_rx_byte, 1);
#endif
}

/* ================================================================== */
/* API                                                                 */
/* ================================================================== */
/* ------------------------------------------------------------------ */
/* Trả NACK khi khung bị loại.                                         */
/*                                                                     */
/* Lưu ý trung thực (nên viết hẳn vào báo cáo): khi CRC đã sai thì SEQ  */
/* và CMD đọc được CŨNG không đáng tin. NACK này chỉ mang tính "báo cho */
/* PC biết có khung hỏng", PC không được dùng SEQ trong đó để khớp lệnh.*/
/* Ngoài ra phải CHẶN TỐC ĐỘ: nếu đường truyền toàn rác, mỗi lần resync */
/* lại bắn một NACK sẽ làm nghẽn chính đường UART đang hỏng.            */
/* ------------------------------------------------------------------ */
#define NACK_MIN_GAP_MS   50u

static uint32_t s_nack_t;

static void on_parse_error(const proto_frame_t *partial, proto_err_t e)
{
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - s_nack_t) < NACK_MIN_GAP_MS) {
        return;
    }
    s_nack_t = now;
    uint8_t d[2] = { partial->cmd, (uint8_t)e };
    uint8_t frame[PROTO_MAX_FRAME];
    uint16_t n = proto_build(frame, partial->seq, CMD_NACK, d, 2);
    if (rb_free(&s_tx_rb) >= n) {
        rb_write(&s_tx_rb, frame, n);
        s_stats.tx_frames++;
    } else {
        s_stats.tx_drop++;
    }
}

void puart_init(UART_HandleTypeDef *huart, proto_frame_cb_t cb)
{
    s_h      = huart;
    s_cb     = cb;
    s_seq_tx = 0;
    s_tx_busy = 0;
    s_nack_t  = 0;

    proto_parser_init(&s_parser, &s_stats);
    s_parser.on_error = on_parse_error;
    rb_init(&s_tx_rb, s_tx_store, PROTO_TX_RB_SIZE);

#if (PROTO_RX_MODE == PROTO_RX_DMA)
    s_rx_rd = 0;
    HAL_UART_Receive_DMA(s_h, s_rx_dma, PROTO_RX_DMA_SIZE);
    /* Tắt ngắt "nửa bộ đệm" cho gọn — ta tự quét NDTR trong vòng lặp. */
    __HAL_DMA_DISABLE_IT(s_h->hdmarx, DMA_IT_HT);
#else
    rb_init(&s_rx_rb, s_rx_store, PROTO_RX_DMA_SIZE);
    HAL_UART_Receive_IT(s_h, &s_rx_byte, 1);
#endif
}

static void handle_byte(uint8_t b, uint32_t now)
{
    const proto_frame_t *f = proto_feed(&s_parser, b, now);
    if (f && s_cb) {
        s_cb(f);
    }
}

void puart_poll(void)
{
    uint32_t now = HAL_GetTick();

#if (PROTO_RX_MODE == PROTO_RX_DMA)
    /* Vị trí DMA đang ghi tới = size - số byte còn lại. */
    uint16_t wr = (uint16_t)(PROTO_RX_DMA_SIZE -
                             __HAL_DMA_GET_COUNTER(s_h->hdmarx));
    while (s_rx_rd != wr) {
        handle_byte(s_rx_dma[s_rx_rd], now);
        s_rx_rd = (uint16_t)((s_rx_rd + 1u) % PROTO_RX_DMA_SIZE);
    }
#else
    uint8_t b;
    while (rb_read1(&s_rx_rb, &b)) {
        handle_byte(b, now);
    }
#endif

    proto_tick(&s_parser, now);
    tx_pump();
}

static uint8_t enqueue(uint8_t seq, uint8_t cmd,
                       const uint8_t *data, uint8_t len)
{
    uint8_t  frame[PROTO_MAX_FRAME];
    uint16_t n = proto_build(frame, seq, cmd, data, len);
    if (n == 0u) {
        return 0;
    }
    /* Hoặc xếp TRỌN VẸN cả khung, hoặc không xếp gì cả — tuyệt đối không
       để một nửa khung nằm trong hàng đợi. */
    if (rb_free(&s_tx_rb) < n) {
        s_stats.tx_drop++;
        return 0;
    }
    rb_write(&s_tx_rb, frame, n);
    s_stats.tx_frames++;
    tx_pump();
    return 1;
}

uint8_t puart_send(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    return enqueue(s_seq_tx, cmd, data, len);
}

uint8_t puart_send_auto(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    return enqueue(s_seq_tx++, cmd, data, len);
}

uint8_t puart_reply(const proto_frame_t *req, const uint8_t *data, uint8_t len)
{
    return enqueue(req->seq, (uint8_t)(req->cmd | CMD_RESP_FLAG), data, len);
}

uint8_t puart_nack(const proto_frame_t *req, proto_err_t err)
{
    uint8_t d[2] = { req->cmd, (uint8_t)err };
    return enqueue(req->seq, CMD_NACK, d, 2);
}

uint16_t puart_tx_space(void)
{
    return rb_free(&s_tx_rb);
}

proto_stats_t *puart_stats(void)
{
    return &s_stats;
}
