/**
 * @file    proto_cfg.h
 * @brief   Cấu hình biên dịch cho giao thức khung UART V01.
 *
 * TẤT CẢ các tuỳ chọn "đổi được" của giao thức nằm ở đây. Nhờ vậy nhóm có thể
 * build nhiều biến thể (SUM8 / CRC8 / CRC16, RX bằng ngắt / bằng DMA) từ cùng
 * một mã nguồn để so sánh trong báo cáo.
 */
#ifndef PROTO_CFG_H
#define PROTO_CFG_H

/* ---------------------------------------------------------------------------
 * 1. Kiểu mã kiểm tra (checksum)
 * ------------------------------------------------------------------------- */
#define PROTO_CKS_SUM8      0   /* Sum-8 bù 2   — 1 byte, yếu nhất           */
#define PROTO_CKS_CRC8      1   /* CRC-8/ATM    — 1 byte, mức "đạt" của đề   */
#define PROTO_CKS_CRC16     2   /* CRC-16/CCITT-FALSE — 2 byte, phần MỞ RỘNG */

#ifndef PROTO_CKS_MODE
#define PROTO_CKS_MODE      PROTO_CKS_CRC16
#endif

/* ---------------------------------------------------------------------------
 * 2. Chế độ nhận UART
 * ------------------------------------------------------------------------- */
#define PROTO_RX_IT         0   /* HAL_UART_Receive_IT 1 byte + ring buffer  */
#define PROTO_RX_DMA        1   /* DMA circular + quét NDTR (khuyên dùng)    */

#ifndef PROTO_RX_MODE
#define PROTO_RX_MODE       PROTO_RX_DMA
#endif

/* ---------------------------------------------------------------------------
 * 3. Kích thước
 * ------------------------------------------------------------------------- */
/* LEN tối đa mà parser CHẤP NHẬN.
 *
 * Đây là một quyết định thiết kế có chủ đích, không phải con số tuỳ tiện:
 * khung dài nhất mà ứng dụng cần là khung STREAM_DATA (4 byte header + 60
 * mẫu ADC 16-bit = 124 byte). Đặt trần đúng bằng nhu cầu thực tế giúp GIẢM
 * THIỆT HẠI khi parser bắt nhầm một cặp 0xAA 0x55 giả nằm trong rác: nó chỉ
 * có thể bị "nuốt" tối đa 128 byte thay vì 250 byte trước khi phát hiện sai.
 * Xem mục "Giới hạn đã biết" trong DAC_TA_GIAO_THUC.md.                     */
#define PROTO_MAX_DATA      128u
#define PROTO_RX_DMA_SIZE   512u    /* bộ đệm vòng DMA RX (luỹ thừa của 2)   */
#define PROTO_TX_RB_SIZE    2048u   /* hàng đợi TX (luỹ thừa của 2)          */

/* ---------------------------------------------------------------------------
 * 4. Thời gian
 * ------------------------------------------------------------------------- */
#define PROTO_IBT_MS        50u     /* inter-byte timeout: quá 50 ms giữa 2
                                       byte trong cùng khung -> huỷ khung    */

/* ---------------------------------------------------------------------------
 * 5. Phiên bản firmware / giao thức
 * ------------------------------------------------------------------------- */
#define PROTO_VERSION       0x11u   /* v1.1 */
#define FW_VERSION_MAJOR    1u
#define FW_VERSION_MINOR    0u

#endif /* PROTO_CFG_H */
