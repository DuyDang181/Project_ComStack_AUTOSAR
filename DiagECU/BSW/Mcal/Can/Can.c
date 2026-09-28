/**********************************************************
 * @file    Can.c
 * @brief   AUTOSAR CAN Driver – bxCAN nội bộ STM32F103 (SPL)
 *
 * @details Module MCAL điều khiển bộ CAN tích hợp (bxCAN) trong
 *          STM32F103 thông qua thư viện SPL (Standard Peripheral
 *          Library)
 *          Cấu hình bit timing (500 kbps):
 *            APB1 = 36 MHz
 *            Prescaler = 4 → Time Quantum = 4/36MHz ≈ 111ns
 *            1 bit = (1 + BS1 + BS2) TQ = (1 + 11 + 6) × 111ns = 2µs
 *            Bit rate = 1/2µs = 500 kbps
 *
 * @version 1.0
 * @date    22/7/2026
 * @author  Duy Dang
 **********************************************************/

#include "Can.h"
#include "CanIf.h"             /* CanIf_TxConfirmation() – callback lên tầng trên */
#include "stm32f10x_rcc.h"     /* SPL: Quản lý clock (RCC)                         */
#include "stm32f10x_gpio.h"    /* SPL: Cấu hình chân GPIO                          */
#include "stm32f10x_can.h"     /* SPL: API điều khiển bxCAN                         */
#include "misc.h"              /* SPL: NVIC configuration                           */
#include <stddef.h>            /* NULL                                              */

/* ===========================================================
 * Define phần cứng
 * ===========================================================*/
#define CAN_PERIPH        CAN1
#define CAN_GPIO_PORT     GPIOA
#define CAN_RX_PIN        GPIO_Pin_11
#define CAN_TX_PIN        GPIO_Pin_12

/* ===========================================================
 * Biến trạng thái nội bộ
 * -----------------------------------------------------------
 * CAN_TX_SLOTS   : Số slot TX (bxCAN có 3 mailbox)
 * ===========================================================*/
#define CAN_TX_SLOTS      6u

static boolean            s_inited = FALSE;
static volatile PduIdType s_swPduHandle[CAN_TX_SLOTS];
static volatile boolean   s_txPending[CAN_TX_SLOTS];
static volatile uint8     s_txMailbox[CAN_TX_SLOTS];

/* ===========================================================
 * prv_CAN_GPIO_Init – Cấu hình chân GPIO cho CAN
 * -----------------------------------------------------------
 * Bật clock cho GPIOA, AFIO (Alternate Function), CAN1.
 * ===========================================================*/
static void prv_CAN_GPIO_Init(void)
{
    /* Bước 1: Bật clock cho các peripheral */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    /* Bước 2: Cấu hình PA12 (CAN_TX) – Alternate Function Push-Pull */
    GPIO_InitTypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin   = CAN_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(CAN_GPIO_PORT, &gpio);

    /* Bước 3: Cấu hình PA11 (CAN_RX) – Input Floating */
    gpio.GPIO_Pin  = CAN_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(CAN_GPIO_PORT, &gpio);
}

/* ===========================================================
 * prv_CAN_Periph_Init – Cấu hình bxCAN peripheral
 * ----------------------------------------------------------
 *
 * Tính toán bit timing cho 500 kbps:
 *   APB1 Clock : 36 MHz
 *   Prescaler  : 4
 *   TQ = Prescaler / APB1 = 4 / 36MHz ≈ 111ns
 *
 *   1 bit = Sync + BS1 + BS2
 *         = 1 + 11 + 6 = 18 TQ
 *         = 18 × 111ns = 2000ns = 2µs
 *
 *   Bit rate = 1 / 2µs = 500 kbps
 *   Sample point = (1 + 11) / 18 = 66.7%
 * ===========================================================*/
static void prv_CAN_Periph_Init(void)
{
    /* Reset CAN peripheral về trạng thái mặc định */
    CAN_DeInit(CAN_PERIPH);

    /* === A. Cấu hình CAN === */
    CAN_InitTypeDef canInit;
    CAN_StructInit(&canInit);

    /* Bit timing */
    canInit.CAN_Prescaler = 4u;           /* Chia clock: TQ = 4/36MHz         */
    canInit.CAN_SJW       = CAN_SJW_1tq;  /* Synchronization Jump Width: 1 TQ */
    canInit.CAN_BS1       = CAN_BS1_11tq;  /* Bit Segment 1: 11 TQ            */
    canInit.CAN_BS2       = CAN_BS2_6tq;   /* Bit Segment 2: 6 TQ             */

    /* Mode hoạt động: Mạng thật hoặc Renode Hub yêu cầu Normal Mode */
    canInit.CAN_Mode = CAN_Mode_Normal;

    /* Các advance feature */
    canInit.CAN_TTCM = DISABLE;  /* Time Triggered: không dùng       */
    canInit.CAN_ABOM = ENABLE;   /* Auto Bus-Off Management: tự phục hồi */
    canInit.CAN_AWUM = DISABLE;  /* Auto Wake-Up: không dùng          */
    canInit.CAN_NART = DISABLE;  /* Auto Retransmission: BẬT (mặc định) */
    canInit.CAN_RFLM = DISABLE;  /* Receive FIFO Locked: không khóa   */
    canInit.CAN_TXFP = DISABLE;  /* TX FIFO Priority: theo ID (mặc định) */

    /* Khởi tạo bxCAN với cấu hình trên */
    CAN_Init(CAN_PERIPH, &canInit);

    /* === B. Cấu hình Filter (Bộ lọc nhận) ===
     *
     * Filter 0: Chấp nhận TẤT CẢ CAN ID
     *   - Mode    : IdMask (so sánh ID với mask)
     *   - Scale   : 32-bit (lọc cả Standard và Extended ID)
     *   - Mask    : 0x0000 (mask = 0 → không lọc, chấp nhận tất cả)
     *   - FIFO    : Gán vào FIFO0
     *   - Kích hoạt: ENABLE */
    CAN_FilterInitTypeDef filterInit;
    filterInit.CAN_FilterNumber         = 0;
    filterInit.CAN_FilterMode           = CAN_FilterMode_IdMask;
    filterInit.CAN_FilterScale          = CAN_FilterScale_32bit;
    filterInit.CAN_FilterIdHigh         = 0x0000;
    filterInit.CAN_FilterIdLow          = 0x0000;
    filterInit.CAN_FilterMaskIdHigh     = 0x0000;
    filterInit.CAN_FilterMaskIdLow      = 0x0000;
    filterInit.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filterInit.CAN_FilterActivation     = ENABLE;
    CAN_FilterInit(&filterInit);
}

/* ===========================================================
 * Can_Init – API công khai: Khởi tạo CAN Driver
 * ===========================================================*/
void Can_Init(void)
{
    /* Bước 1: Cấu hình GPIO (PA11, PA12) */
    prv_CAN_GPIO_Init();

    /* Bước 2: Cấu hình bxCAN peripheral (bit timing, filter) */
    prv_CAN_Periph_Init();

    /* Bước 3: Clear trạng thái TX cho tất cả các slot */
    for (uint8 i = 0; i < CAN_TX_SLOTS; ++i) {
        s_swPduHandle[i] = 0u;
        s_txPending[i]   = FALSE;
        s_txMailbox[i]   = 0u;
    }

    /* Bước 4: Bật CAN RX FIFO0 interrupt (Renode STMCAN dùng interrupt) */
    CAN_ITConfig(CAN_PERIPH, CAN_IT_FMP0, ENABLE);

    /* Bước 5: Cấu hình NVIC cho CAN1 RX0 (IRQ 20 = USB_LP_CAN1_RX0) */
    NVIC_InitTypeDef nvicInit;
    nvicInit.NVIC_IRQChannel                   = USB_LP_CAN1_RX0_IRQn;
    nvicInit.NVIC_IRQChannelPreemptionPriority  = 1;
    nvicInit.NVIC_IRQChannelSubPriority         = 0;
    nvicInit.NVIC_IRQChannelCmd                 = ENABLE;
    NVIC_Init(&nvicInit);

    /* Bước 6: Đánh dấu driver đã sẵn sàng */
    s_inited = TRUE;
}

/* ===========================================================
 * Can_Write – API công khai: Gửi CAN PDU
 *
 * Mapping kiểu dữ liệu:

 *   │ id ≤ 0x7FF         │ StdId + CAN_Id_Standard│
 *   │ id > 0x7FF         │ ExtId + CAN_Id_Extended│
 *   │ length             │ DLC                   │
 *   │ sdu[i]             │ Data[i]               │
 * ===========================================================*/
Can_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)
{
    /* Bước 1: Kiểm tra điều kiện tiên quyết */
    if (!s_inited || (PduInfo == NULL) || (PduInfo->sdu == NULL)) {
        return CAN_NOT_OK;
    }

    /* Bước 2: Giới hạn chỉ số slot (bxCAN có 3 mailbox: 0, 1, 2) */
    uint8 slot = (Hth < CAN_TX_SLOTS) ? (uint8)Hth : 0u;

    /* Kiểm tra slot có đang chờ TX hoàn tất không */
    if (s_txPending[slot]) {
        return CAN_BUSY;
    }

    /* Bước 3: Chuyển đổi Can_PduType → CanTxMsg (SPL)
     * CanTxMsg là cấu trúc mà hàm CAN_Transmit() của SPL yêu cầu */
    CanTxMsg txMsg;

    /* 3a. DLC (Data Length Code): giới hạn tối đa 8 byte */
    txMsg.DLC = (PduInfo->length <= 8u) ? PduInfo->length : 8u;

    /* 3b. CAN ID: phân biệt Standard (11-bit) và Extended (29-bit)
     *
     *   Standard ID: 0x000..0x7FF (11 bit)
     *     → txMsg.IDE   = CAN_Id_Standard
     *     → txMsg.StdId = giá trị ID
     *
     *   Extended ID: > 0x7FF (29 bit)
     *     → txMsg.IDE   = CAN_Id_Extended
     *     → txMsg.ExtId = giá trị ID (29 bit thấp) */
    if (PduInfo->id > 0x7FFu) {
        txMsg.IDE   = CAN_Id_Extended;
        txMsg.ExtId = PduInfo->id & 0x1FFFFFFFu;
        txMsg.StdId = 0u;
    } else {
        txMsg.IDE   = CAN_Id_Standard;
        txMsg.StdId = PduInfo->id & 0x7FFu;
        txMsg.ExtId = 0u;
    }

    /* 3c. RTR: Data frame (không phải Remote frame) */
    txMsg.RTR = CAN_RTR_Data;

    /* 3d. Copy dữ liệu payload */
    for (uint8 i = 0; i < txMsg.DLC; i++) {
        txMsg.Data[i] = PduInfo->sdu[i];
    }

    /* Bước 4: Gọi SPL API để nạp frame vào mailbox bxCAN
     *
     * CAN_Transmit() trả về:
     *   0, 1, 2         : Số mailbox đã sử dụng (thành công) */
    uint8_t mailbox = CAN_Transmit(CAN_PERIPH, &txMsg);

    if (mailbox == CAN_TxStatus_NoMailBox) {
        return CAN_BUSY;
    }

    /* Bước 5: Lưu thông tin cho callback TxConfirmation
     * Khi Can_MainFunction_Write() phát hiện TX xong,
     * sẽ dùng s_swPduHandle[slot] để báo ngược lên CanIf */
    s_swPduHandle[slot] = PduInfo->swPduHandle;
    s_txMailbox[slot]   = mailbox;
    s_txPending[slot]   = TRUE;

    return CAN_OK;
}

/* ===========================================================
 * Can_MainFunction_Write – Polling kiểm tra TX hoàn tất
 * -----------------------------------------------------------
 * Được gọi định kỳ trong vòng lặp chính.
 *
 * Quy trình cho mỗi slot TX đang pending:
 *   1. Gọi CAN_TransmitStatus() để hỏi trạng thái mailbox
 *   2. Nếu CAN_TxStatus_Ok:
 *      → TX thành công, gọi CanIf_TxConfirmation(handle)
 *      → CanIf chuyển tiếp lên PduR → COM
 *   3. Nếu CAN_TxStatus_Failed:
 *      → TX thất bại, clear pending (upper layer có thể retry)
 *   4. Nếu CAN_TxStatus_Pending:
 *      → Đang truyền, chờ lần polling tiếp theo
 * ===========================================================*/
void Can_MainFunction_Write(void)
{
    if (!s_inited) return;

    for (uint8 i = 0; i < CAN_TX_SLOTS; ++i) {
        if (s_txPending[i]) {
            /* Hỏi SPL: mailbox này đã truyền xong chưa? */
            uint8_t status = CAN_TransmitStatus(CAN_PERIPH, s_txMailbox[i]);

            if (status == CAN_TxStatus_Ok) {
                /* TX thành công → báo lên CanIf qua callback */
                PduIdType handle = s_swPduHandle[i];
                s_txPending[i] = FALSE;
                CanIf_TxConfirmation(handle);
            }
            else if (status == CAN_TxStatus_Failed) {
                /* TX thất bại (bus error) */
                s_txPending[i] = FALSE;
            }
            /* CAN_TxStatus_Pending → bxCAN đang truyền, chờ tiếp */
        }
    }
}

/* ===========================================================
 * Can_MainFunction_Read – Polling kiểm tra RX
 * -----------------------------------------------------------
 * Trích xuất message từ FIFO0 và FIFO1, sau đó báo lên CanIf_RxIndication
 * ===========================================================*/
void Can_MainFunction_Read(void)
{
    if (!s_inited) return;

    CanRxMsg rxMsg;
    PduInfoType pduInfo;

    /* Kiểm tra FIFO0 (backup polling – chính đã xử lý qua ISR) */
    while (CAN_MessagePending(CAN_PERIPH, CAN_FIFO0) > 0) {
        CAN_Receive(CAN_PERIPH, CAN_FIFO0, &rxMsg);

        pduInfo.SduDataPtr = rxMsg.Data;
        pduInfo.SduLength  = rxMsg.DLC;

        CanIf_RxIndication(rxMsg.StdId, &pduInfo);
    }

    /* Kiểm tra FIFO1 */
    while (CAN_MessagePending(CAN_PERIPH, CAN_FIFO1) > 0) {
        CAN_Receive(CAN_PERIPH, CAN_FIFO1, &rxMsg);

        pduInfo.SduDataPtr = rxMsg.Data;
        pduInfo.SduLength  = rxMsg.DLC;

        CanIf_RxIndication(rxMsg.StdId, &pduInfo);
    }
}

/* ===========================================================
 * USB_LP_CAN1_RX0_IRQHandler – ISR nhận CAN frame từ FIFO0
 * -----------------------------------------------------------
 * Renode STMCAN model kích hoạt interrupt khi frame đến FIFO0.
 * ISR này đọc frame và chuyển lên CanIf_RxIndication.
 * ===========================================================*/
__attribute__((weak)) void USB_LP_CAN1_RX0_IRQHandler(void)
{
    CanRxMsg rxMsg;
    PduInfoType pduInfo;

    while (CAN_MessagePending(CAN_PERIPH, CAN_FIFO0) > 0) {
        CAN_Receive(CAN_PERIPH, CAN_FIFO0, &rxMsg);

        pduInfo.SduDataPtr = rxMsg.Data;
        pduInfo.SduLength  = rxMsg.DLC;

        CanIf_RxIndication(rxMsg.StdId, &pduInfo);
    }
}
