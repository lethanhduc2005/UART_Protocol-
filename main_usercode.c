/**
 * @file  main_usercode.c
 * @brief KHÔNG phải file để biên dịch — đây là các đoạn cần CHÉP vào
 *        main.c do CubeIDE sinh ra, đúng vị trí USER CODE tương ứng.
 *
 * ===========================================================================
 *  A. CẤU HÌNH TRONG CUBEMX (làm đúng thứ tự này)
 * ===========================================================================
 *
 *  Board mẫu: WeAct Blackpill STM32F411CEU6, HSE 25 MHz, SYSCLK 96 MHz.
 *  (Dùng Nucleo-F401RE/F411RE cũng được, chỉ đổi chân LED và USART.)
 *
 *  1) RCC        : HSE = Crystal/Ceramic Resonator
 *     Clock      : HCLK 96 MHz, APB1 48 MHz, APB2 96 MHz
 *
 *  2) SYS        : Debug = Serial Wire, Timebase Source = SysTick
 *
 *  3) USART1     : Asynchronous, 115200-8-N-1
 *                  PA9  = USART1_TX  -> chân RX của module USB-UART
 *                  PA10 = USART1_RX  -> chân TX của module USB-UART
 *                  DMA Settings:
 *                     USART1_RX : DMA2 Stream2 Ch4, Circular,  Byte/Byte
 *                     USART1_TX : DMA2 Stream7 Ch4, Normal,    Byte/Byte
 *                  NVIC: bật USART1 global interrupt  (BẮT BUỘC, kể cả khi
 *                        dùng DMA — HAL cần nó để chạy Error/TxCplt callback)
 *
 *  4) ADC1       : IN0 (PA0), IN1 (PA1)
 *                  Resolution 12 bit, Data Align Right
 *                  Scan Conversion Mode : Disabled (code tự bật khi cần)
 *                  Continuous Conversion: Disabled
 *                  External Trigger Conversion Source: Timer 2 Trigger Out
 *                  External Trigger Conversion Edge  : Rising edge
 *                  DMA: DMA2 Stream0 Ch0, CIRCULAR, Half Word / Half Word
 *                  NVIC: bật ADC1 global interrupt
 *
 *  5) TIM2       : Clock Source = Internal Clock
 *                  Trigger Event Selection (TRGO) = Update Event   <-- QUAN TRỌNG
 *                  PSC = 0, ARR = 1 (code sẽ ghi đè khi STREAM_START)
 *                  NVIC: bật TIM2 global interrupt (chỉ cần cho nguồn TEST)
 *
 *  6) TIM3       : PWM Generation CH1 (PA6), CH2 (PA7)
 *                  PSC = 95  -> 1 MHz ; ARR = 999 -> PWM 1 kHz, độ phân giải 0.1%
 *
 *  7) GPIO Output: PC13 (LED onboard), PB12, PB13, PB14
 *     GPIO Open-Drain + Pull-up: PA8 (DHT11 DATA, kèm trở 4k7 lên 3V3)
 *
 *  8) Project Manager -> Code Generator:
 *     [x] Generate peripheral initialization as a pair of .c/.h files
 *
 *  9) Chép thư mục firmware/Inc/*.h  vào  Core/Inc/
 *     Chép thư mục firmware/Src/*.c  vào  Core/Src/
 *     (CubeIDE tự thêm file .c mới trong Core/Src vào build.)
 *
 * ===========================================================================
 *  B. NỐI DÂY
 * ===========================================================================
 *   Blackpill        Thiết bị
 *   --------------------------------------------------------------
 *   PA9  (TX)   ->   RXD của CP2102/CH340
 *   PA10 (RX)   <-   TXD của CP2102/CH340
 *   GND         <->  GND của CP2102       (BẮT BUỘC chung mass)
 *   PA0         <-   con chạy biến trở 10k (2 đầu nối 3V3 và GND)
 *   PA1         <-   LM35 / biến trở thứ hai
 *   PA6         ->   LED qua trở 330R  (hoặc chân tín hiệu servo)
 *   PA7         ->   LED qua trở 330R
 *   PA8         <->  chân DATA của DHT11, kèm trở kéo lên 4k7 tới 3V3
 *   PC13        ->   LED onboard (tích cực mức thấp)
 *
 *   LƯU Ý: KHÔNG cấp 5 V vào chân ADC. Biến trở phải nối lên 3V3.
 * ===========================================================================
 */

/* ================== USER CODE BEGIN Includes ====================== */
#include "proto_uart.h"
#include "app_cmd.h"
#include "stream.h"
#include "dht11.h"
/* ================== USER CODE END Includes ======================== */


/* ================== USER CODE BEGIN 2 ============================= */
/* Đặt NGAY SAU các lệnh MX_xxx_Init() và TRƯỚC vòng while(1)          */

    dht11_init();                              /* bật bộ đếm DWT       */
    stream_init();
    puart_init(&huart1, app_cmd_on_frame);     /* mở đường nhận        */
    app_cmd_init();                            /* tắt LED, bật PWM     */

/* ================== USER CODE END 2 =============================== */


/* ================== USER CODE BEGIN WHILE ========================= */
  while (1)
  {
    puart_poll();      /* 1. rút byte đã nhận -> parser -> app_cmd_on_frame
                          2. kiểm tra inter-byte timeout
                          3. bơm hàng đợi TX ra DMA                     */
    stream_poll();     /* gom mẫu ADC thành khung STREAM_DATA           */
    app_cmd_poll();    /* việc chậm: đọc DHT11                          */

    /* TUYỆT ĐỐI không đặt HAL_Delay() ở đây.                           */
  }
/* ================== USER CODE END WHILE =========================== */


/* ===========================================================================
 *  C. ĐO TẢI CPU (số liệu cho báo cáo — SV4)
 * ===========================================================================
 * Cách đơn giản và đủ tin cậy: đếm số vòng lặp rảnh trong 1 giây.
 *
 *   static volatile uint32_t idle_cnt, idle_per_sec;
 *   static uint32_t t_mark;
 *
 *   trong while(1), cuối vòng lặp:
 *       idle_cnt++;
 *       if (HAL_GetTick() - t_mark >= 1000u) {
 *           t_mark      = HAL_GetTick();
 *           idle_per_sec = idle_cnt;
 *           idle_cnt     = 0;
 *       }
 *
 * Đo idle_per_sec ở 3 trạng thái rồi lập bảng:
 *       (a) không streaming                       -> N0 (mốc 0% tải)
 *       (b) streaming 100 Hz                      -> N1
 *       (c) streaming 5 kHz                       -> N2
 *   Tải CPU (%) = 100 * (1 - N/N0)
 *
 * Cách thứ hai, trực quan hơn khi quay video: đảo một chân GPIO lúc vào và ra
 * khỏi phần xử lý, rồi xem duty cycle trên logic analyzer.
 * =========================================================================== */
