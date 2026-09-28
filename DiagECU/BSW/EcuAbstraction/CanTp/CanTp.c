/*
 * ============================================================================
 * CanTp.c - Module vận chuyển CAN (CAN Transport Protocol)
 * ============================================================================
 * Giao thức CAN TP theo chuẩn ISO 15765-2 (AUTOSAR)
 * ============================================================================
 */

#include "CanTp.h"
#include "PduR_CanTp.h"
#include "CanIf.h"
#include <string.h>

extern void Log_Print(const char* str);

/* ===== Định nghĩa các trạng thái của máy trạng thái CanTp ===== */
typedef enum {
    CANTP_IDLE,              /* Trạng thái rảnh rỗi, sẵn sàng truyền/nhận */
    CANTP_TX_WAIT_FF_CONF,   /* Đang đợi xác nhận SF đã gửi thành công */
    CANTP_TX_WAIT_FC,        /* Đã gửi FF, đang đợi Flow Control từ node nhận */
    CANTP_TX_WAIT_CF_CONF,   /* Đang đợi xác nhận CF đã gửi thành công */
    CANTP_TX_SEND_CF,        /* Đang trong quá trình gửi các CF liên tiếp */
    CANTP_RX_WAIT_CF         /* Đã nhận FF, đang đợi các CF từ node gửi */
} CanTp_StateType;

/* ===== Cấu trúc quản lý trạng thái cho mỗi kênh truyền (TX) =====
 * Mỗi N-SDU (Network Service Data Unit) TX có một trạng thái riêng
 */
typedef struct {
    CanTp_StateType State;       /* Trạng thái hiện tại của máy trạng thái TX */
    PduIdType       SduId;       /* Định danh của PDU đang được gửi */
    uint32          TotalLength; /* Tổng độ dài dữ liệu cần gửi (bytes) */
    uint32          SentLength;  /* Số bytes đã gửi thành công */
    uint8           SN;          /* Sequence Number - số thứ tự CF (1-15, quay vòng) */
    uint16          BlockSize;   /* Số CF tối đa gửi trước khi đợi FC mới */
    uint8           STmin;       /* Thời gian tối thiểu giữa 2 CF liên tiếp */
    uint16          Timer_N_Bs;  /* Timeout đợi Flow Control (đếm ngược) */
    uint16          Timer_N_Cs;  /* Timeout giữa các CF liên tiếp (đếm ngược) */
    uint8           TxBuffer[64];/* Buffer lưu dữ liệu cần gửi (tối đa 64 bytes) */
} CanTp_TxStateType;

/* ===== Cấu trúc quản lý trạng thái cho kênh nhận (RX) =====
 * Đơn giản hóa: chỉ hỗ trợ 1 kênh nhận tại một thời điểm
 */
typedef struct {
    CanTp_StateType State;       /* Trạng thái hiện tại của máy trạng thái RX */
    uint32          TotalLength; /* Tổng độ dài dữ liệu mong đợi (bytes) - lấy từ FF */
    uint32          ReceivedLength; /* Số bytes đã nhận được tính đến hiện tại */
    uint8           SN;          /* Sequence Number kỳ vọng của CF tiếp theo */
    boolean         FcPending;   /* Đang chờ gửi Flow Control */
    uint8           RxBuffer[64];/* Buffer lưu dữ liệu đang lắp ráp */
} CanTp_RxStateType;

/* ===== Mảng quản lý trạng thái cho tất cả kênh TX =====
 */
static CanTp_TxStateType TxState[CANTP_NUM_TX_SDUS];

/* ===== Biến quản lý trạng thái cho kênh RX duy nhất ===== */
static CanTp_RxStateType RxState;

/* Cờ đánh dấu CanTp đã được khởi tạo hay chưa */
boolean CanTp_Initialized = FALSE;

/* Biến debug: đếm số lần reassembly (lắp ráp dữ liệu nhận) thành công */
volatile uint32 CanTp_RxDoneCount = 0;

/* Biến debug: đếm số lần callback RxIndication được gọi */
volatile uint32 CanTp_RxIndicationCount = 0;

/* Biến debug: theo dõi việc gửi Flow Control cho response multi-frame */
volatile uint32 CanTp_FcTxOkCount = 0;
volatile uint32 CanTp_FcTxFailCount = 0;
volatile uint8  CanTp_FcLastResult = E_NOT_OK;

/* ===== Hàm debug: Trả về con trỏ tới buffer RX ===== */
uint8_t* Debug_Get_CanTp_RxBuffer(void)
{
    return RxState.RxBuffer;
}

/* ============================================================================
 * CanTp_Init - Khởi tạo module CanTp
 * ============================================================================
 * Đặt tất cả kênh TX và RX về trạng thái IDLE, sẵn sàng hoạt động
 */
void CanTp_Init(void)
{
    /* Bước 1: Duyệt qua từng kênh TX và khởi tạo */
    for (uint16 i = 0; i < CANTP_NUM_TX_SDUS; i++)
    {
        TxState[i].State = CANTP_IDLE;    /* Đưa về trạng thái rảnh */
        TxState[i].SduId = i;             /* Gán ID cho kênh */
        TxState[i].Timer_N_Bs = 0;        /* Reset timeout */
        TxState[i].Timer_N_Cs = 0;        /* Reset timeout */
    }

    /* Bước 2: Khởi tạo kênh RX */
    RxState.State = CANTP_IDLE;           /* Đưa về trạng thái rảnh */
    RxState.FcPending = FALSE;             /* Chưa có Flow Control cần gửi */

    CanTp_FcTxOkCount = 0;
    CanTp_FcTxFailCount = 0;
    CanTp_FcLastResult = E_NOT_OK;

    /* Bước 3: Đánh dấu đã khởi tạo xong */
    CanTp_Initialized = TRUE;
}

/* ============================================================================
 * CanTp_Transmit - Gửi một PDU qua CAN bus
 * ============================================================================
 */
Std_ReturnType CanTp_Transmit(PduIdType CanTpTxSduId, const PduInfoType* CanTpTxInfoPtr)
{
    /* ===== Bước 1: Kiểm tra tham số đầu vào ===== */
    if (!CanTp_Initialized || CanTpTxInfoPtr == NULL || CanTpTxSduId >= CANTP_NUM_TX_SDUS)
    {
        return E_NOT_OK; /* Lỗi: chưa khởi tạo, con trỏ NULL, hoặc ID sai */
    }

    /* Lấy trạng thái của kênh TX được yêu cầu */
    CanTp_TxStateType* tx = &TxState[CanTpTxSduId];

    /* ===== Bước 2: Kiểm tra kênh rảnh ===== */
    if (tx->State != CANTP_IDLE)
    {
        return E_NOT_OK; /* Lỗi: Kênh đang bận gửi packet khác */
    }

    /* ===== Bước 3: Lưu thông tin dữ liệu cần gửi ===== */
    tx->TotalLength = CanTpTxInfoPtr->SduLength;  /* Tổng số bytes cần gửi */
    tx->SentLength = 0;                           /* Chưa gửi byte nào */
    tx->SN = 1;                                   /* Sequence Number bắt đầu từ 1 */

    /* ===== Bước 4: Copy dữ liệu vào buffer nội bộ ===== */
    if (tx->TotalLength <= 64)
    {
        memcpy(tx->TxBuffer, CanTpTxInfoPtr->SduDataPtr, tx->TotalLength);
    }

    /* Chuẩn bị cấu trúc để gửi xuống CanIf */
    PduInfoType canIfTxInfo;
    uint8 canPayload[8];              /* Payload tối đa 8 bytes cho CAN Classic */
    canIfTxInfo.SduDataPtr = canPayload;

    /* ===== Bước 5: Quyết định loại frame dựa vào độ dài ===== */
    if (tx->TotalLength <= 7) {
        /* ===== TRƯỜNG HỢP 1: Single Frame (SF) - Dữ liệu <= 7 bytes =====
         * Format SF: [PCI + Length] [Data1] [Data2] ... [DataN]
         * PCI = 0x00 (4 bit cao), Length = độ dài dữ liệu (4 bit thấp)
         */

        /* Tạo byte PCI: 0x00 | length */
        canPayload[0] = (uint8)(0x00 | tx->TotalLength);

        /* Copy dữ liệu vào 7 bytes tiếp theo */
        memcpy(&canPayload[1], tx->TxBuffer, tx->TotalLength);

        /* Tổng độ dài frame = 1 (PCI) + dữ liệu */
        canIfTxInfo.SduLength = 1 + tx->TotalLength;

        /* Gửi frame xuống tầng CANIf */
        CanIf_Transmit(CanTp_TxSduCfg[CanTpTxSduId].CanIfTxPduId, &canIfTxInfo);

        /* Báo kết quả lên tầng PduR ngay lập tức */
        PduR_CanTpTxConfirmation(CanTp_TxSduCfg[CanTpTxSduId].PduRTxSduId, NTFRSLT_OK);

        /* Quay về IDLE vì đã gửi xong */
        tx->State = CANTP_IDLE;

    }
    else
    {
        /* ===== TRƯỜNG HỢP 2: First Frame (FF) - Dữ liệu > 7 bytes =====
         * Format FF: [PCI + Length_MSB] [Length_LSB] [Data1] ... [Data6]
         * PCI = 0x10 (4 bit cao), Length_MSB = 4 bit thấp của độ dài
         * FF chứa 6 bytes dữ liệu đầu tiên
         */

        /* Tạo byte PCI đầu tiên: 0x10 | (Length >> 8) & 0x0F */
        canPayload[0] = (uint8)(0x10 | ((tx->TotalLength >> 8) & 0x0F));

        /* Byte thứ 2: Length LSB */
        canPayload[1] = (uint8)(tx->TotalLength & 0xFF);

        /* Copy 6 bytes dữ liệu đầu tiên vào FF */
        memcpy(&canPayload[2], tx->TxBuffer, 6);

        /* FF luôn đủ 8 bytes */
        canIfTxInfo.SduLength = 8;

        /* Cập nhật tiến trình: đã gửi 6 bytes */
        tx->SentLength = 6;

        /* Chuyển trạng thái: Đợi Flow Control từ node nhận */
        tx->State = CANTP_TX_WAIT_FC;

        /* Khởi tạo timeout N_Bs: thời gian tối đa đợi FC */
        tx->Timer_N_Bs = CanTp_TxSduCfg[CanTpTxSduId].N_Bs;

        /* Gửi FF xuống tầng CANIf */
        CanIf_Transmit(CanTp_TxSduCfg[CanTpTxSduId].CanIfTxPduId, &canIfTxInfo);
    }

    return E_OK;
}

static void CanTp_TrySendPendingFlowControl(void);

/* ============================================================================
 * CanTp_RxIndication - Xử lý khi nhận được frame CAN từ node khác
 */
void CanTp_RxIndication(PduIdType CanTpRxPduId, const PduInfoType* CanTpRxPduPtr)
{
    (void)CanTpRxPduId;

    /* ===== Bước 1: Kiểm tra tham số đầu vào ===== */
    if (!CanTp_Initialized || CanTpRxPduPtr == NULL || CanTpRxPduPtr->SduLength == 0)
    {
        return; /* Dữ liệu không hợp lệ */
    }

    /* Debug: đếm số lần callback này được gọi */
    CanTp_RxIndicationCount++;

    /* ===== Bước 2: Xác định loại frame từ PCI (Protocol Control Information) =====
     * PCI nằm ở byte đầu tiên, 4 bit cao cho biết loại frame:
     * 0x0 = Single Frame (SF)
     * 0x1 = First Frame (FF)
     * 0x2 = Consecutive Frame (CF)
     * 0x3 = Flow Control (FC)
     */
    uint8 pciType = (CanTpRxPduPtr->SduDataPtr[0] & 0xF0) >> 4;

    /* ===== Xử lý Single Frame (SF) ===== */
    if (pciType == 0x00) {
        uint8 sfLen = CanTpRxPduPtr->SduDataPtr[0] & 0x0F;

        PduLengthType bufferSize = 0;
        /* Báo bắt đầu nhận Single Frame */
        PduR_CanTpStartOfReception(CanTp_RxSduCfg[0].PduRRxSduId, NULL, sfLen, &bufferSize);

        /* Chuyển thẳng payload lên PduR / DCM */
        PduInfoType rxDataInfo;
        rxDataInfo.SduDataPtr = (uint8*)&CanTpRxPduPtr->SduDataPtr[1];
        rxDataInfo.SduLength = sfLen;
        PduR_CanTpCopyRxData(CanTp_RxSduCfg[0].PduRRxSduId, &rxDataInfo, &bufferSize);

        /* Báo hoàn tất nhận */
        PduR_CanTpRxIndication(CanTp_RxSduCfg[0].PduRRxSduId, NTFRSLT_OK);
        CanTp_RxDoneCount++;
    }
    /* ===== Xử lý First Frame (FF) ===== */
    else if (pciType == 0x01)
    {
        /* ===== BƯỚC 1: Giải mã thông tin từ FF =====
         * Format FF: [PCI+Len_MSB] [Len_LSB] [Data1] ... [Data6]
         * - Byte 0: 0x10 | (TotalLength >> 8) & 0x0F
         * - Byte 1: TotalLength & 0xFF
         * - Byte 2-7: 6 bytes dữ liệu đầu tiên
         */

        /* Extract độ dài tổng từ 2 byte đầu */
        uint8 fs = CanTpRxPduPtr->SduDataPtr[0] & 0x0F;      /* N-PDU length MSB */
        uint8 lenLSB = CanTpRxPduPtr->SduDataPtr[1];          /* N-PDU length LSB */
        uint16 totalRxLength = (fs << 8) | lenLSB;            /* Tổng độ dài dữ liệu */

        /* ===== BƯỚC 2: Khởi tạo trạng thái RX ===== */
        RxState.State = CANTP_RX_WAIT_CF;        /* Chuyển sang trạng thái đợi CF */
        RxState.TotalLength = totalRxLength;     /* Lưu tổng độ dài mong đợi */
        RxState.ReceivedLength = 6;              /* Đã nhận 6 bytes từ FF */
        RxState.SN = 1;                          /* CF tiếp theo phải có SN = 1 */

        /* Gọi StartOfReception lên PduR / DCM */
        PduLengthType bufferSize = 0;
        PduR_CanTpStartOfReception(CanTp_RxSduCfg[0].PduRRxSduId, NULL, totalRxLength, &bufferSize);

        /* Copy 6 bytes dữ liệu từ FF đẩy sang PduR / DCM thay vì buffer nội bộ */
        PduInfoType rxDataInfo;
        rxDataInfo.SduDataPtr = (uint8*)&CanTpRxPduPtr->SduDataPtr[2];
        rxDataInfo.SduLength = 6;
        PduR_CanTpCopyRxData(CanTp_RxSduCfg[0].PduRRxSduId, &rxDataInfo, &bufferSize);

        /* ===== BƯỚC 3: Gửi Flow Control (FC) về node TX =====
         * FC frame cho phép node TX tiếp tục gửi CF
         * Format FC: [PCI+FS] [BlockSize] [STmin]
         * - PCI+FS = 0x30: Flow Control, Clear To Send
         * - BlockSize: số CF tối đa trước khi cần FC mới (0 = không giới hạn)
         * - STmin: thời gian tối thiểu giữa 2 CF (ms)
         */
        /* Gửi FC ngay lập tức để đáp ứng thời gian chờ của node TX.
         * Nếu CAN mailbox đang bận, CanTp_MainFunction() sẽ retry.
         */
        RxState.FcPending = TRUE;
    }

    /* ===== Xử lý Consecutive Frame (CF) ===== */
    else if (pciType == 0x02 && RxState.State == CANTP_RX_WAIT_CF)
    {
        /* ===== BƯỚC 1: Giải mã thông tin từ CF =====
         * Format CF: [PCI+SN] [Data1] ... [Data7]
         * - Byte 0: 0x20 | SequenceNumber
         * - Byte 1-7: 7 bytes dữ liệu
         */

        /* Tính số bytes dữ liệu thực tế trong CF */
        uint8 len = CanTpRxPduPtr->SduLength - 1; /* Trừ 1 byte PCI */

        /* Đảm bảo không copy vượt quá tổng độ dài mong đợi */
        if (RxState.ReceivedLength + len > RxState.TotalLength) {
            len = RxState.TotalLength - RxState.ReceivedLength;
        }

        /* ===== BƯỚC 2: Copy dữ liệu từ CF đẩy lên PduR / DCM thay vì buffer nội bộ ===== */
        PduInfoType rxDataInfo;
        rxDataInfo.SduDataPtr = (uint8*)&CanTpRxPduPtr->SduDataPtr[1];  /* Skip PCI byte */
        rxDataInfo.SduLength = len;

        PduLengthType bufferSize = 0;
        PduR_CanTpCopyRxData(CanTp_RxSduCfg[0].PduRRxSduId, &rxDataInfo, &bufferSize);

        /* Cập nhật số bytes đã nhận */
        RxState.ReceivedLength += len;

        /* ===== BƯỚC 3: Kiểm tra đã nhận đủ dữ liệu chưa ===== */
        if (RxState.ReceivedLength >= RxState.TotalLength) {
            /* Đã nhận đủ dữ liệu! */
            RxState.State = CANTP_IDLE;            /* Quay về IDLE */
            RxState.FcPending = FALSE;             /* Không còn FC đang chờ */
            CanTp_RxDoneCount++;                   /* Debug: đếm số lần hoàn tất */

            /* Báo kết quả lên tầng PduR */
            PduR_CanTpRxIndication(CanTp_RxSduCfg[0].PduRRxSduId, NTFRSLT_OK);
        }
    }

    /* ===== Xử lý Flow Control (FC) - Nhận ở phía TX ===== */
    else if (pciType == 0x03)
    {
        /* ===== BƯỚC 1: Lấy trạng thái TX tương ứng ===== */
        CanTp_TxStateType* tx = &TxState[0];

        /* Chỉ xử lý nếu đang đợi Flow Control */
        if (tx->State == CANTP_TX_WAIT_FC) {
            /* ===== BƯỚC 2: Giải mã FC frame =====
             * Format FC: [PCI+FS] [BlockSize] [STmin]
             * - FS (Flow Status): 0 = CTS, 1 = WAIT, 2 = OVFLW
             */
            uint8 fs = CanTpRxPduPtr->SduDataPtr[0] & 0x0F; /* Flow Status */

            /* ===== BƯỚC 3: Xử lý theo Flow Status ===== */
            if (fs == 0) { /* Continue To Send (CTS) */
                /* Lưu thông tin từ FC */
                tx->BlockSize = CanTpRxPduPtr->SduDataPtr[1]; /* Block Size */
                tx->STmin = CanTpRxPduPtr->SduDataPtr[2];     /* STmin */

                /* Chuyển trạng thái */
                tx->State = CANTP_TX_WAIT_CF_CONF;

                /* ===== BƯỚC 4: Gửi CF đầu tiên ngay lập tức ===== */
                PduInfoType canIfTxInfo;
                uint8 canPayload[8];
                canIfTxInfo.SduDataPtr = canPayload;

                /* Tạo PCI cho CF: 0x20 | SequenceNumber */
                canPayload[0] = 0x20 | (tx->SN & 0x0F);

                /* Tính số bytes còn lại cần gửi */
                uint32 remain = tx->TotalLength - tx->SentLength;

                /* Mỗi CF chứa tối đa 7 bytes dữ liệu */
                uint8 len = (remain > 7) ? 7 : (uint8)remain;

                /* Copy dữ liệu từ buffer vào CF */
                memcpy(&canPayload[1], &tx->TxBuffer[tx->SentLength], len);
                canIfTxInfo.SduLength = 1 + len;

                /* Cập nhật tiến trình */
                tx->SentLength += len;
                tx->SN++;  /* Tăng sequence number */

                /* ===== BƯỚC 5: Kiểm tra đã gửi hết chưa ===== */
                if (tx->SentLength >= tx->TotalLength) {
                    /* Đã gửi hết dữ liệu! */
                    tx->State = CANTP_IDLE;

                    /* Gửi CF cuối cùng */
                    CanIf_Transmit(CanTp_TxSduCfg[0].CanIfTxPduId, &canIfTxInfo);

                    /* Báo hoàn tất lên PduR */
                    PduR_CanTpTxConfirmation(CanTp_TxSduCfg[0].PduRTxSduId, NTFRSLT_OK);
                } else {
                    /* Vẫn còn dữ liệu, gửi CF này và tiếp tục */
                    CanIf_Transmit(CanTp_TxSduCfg[0].CanIfTxPduId, &canIfTxInfo);
                    tx->State = CANTP_TX_SEND_CF; /* Chuyển sang trạng thái gửi CF tiếp theo */
                }
            }
            /* Lưu ý: Chưa xử lý FS = 1 (WAIT) và FS = 2 (OVFLW) */
        }
    }
}

/* ============================================================================
 * CanTp_TxConfirmation - Xác nhận đã gửi frame CAN thành công
 * ============================================================================
 *   CanTpTxPduId - ID của PDU đã gửi thành công
 * ============================================================================
 */
void CanTp_TxConfirmation(PduIdType CanTpTxPduId) {
    (void)CanTpTxPduId;

    /* Lấy trạng thái TX */
    CanTp_TxStateType* tx = &TxState[0];

    /* Kiểm tra nếu đang đợi xác nhận CF */
    if (tx->State == CANTP_TX_WAIT_CF_CONF)
    {
        /* Kiểm tra còn dữ liệu cần gửi không */
        if (tx->SentLength < tx->TotalLength)
        {
            /* Còn dữ liệu -> chuyển sang trạng thái gửi CF tiếp */
            tx->State = CANTP_TX_SEND_CF;
        }
        else
        {
            /* Hết dữ liệu -> quay về IDLE */
            tx->State = CANTP_IDLE;
        }
    }
}

/* ============================================================================
 * CanTp_MainFunction - Hàm xử lý tuần kỳ của CanTp
 * ============================================================================
 * Được gọi định kỳ từ Main Loop gửi các CF tiếp theo trong quá trình truyền
 * ============================================================================
 */
static void CanTp_TrySendPendingFlowControl(void)
{
    if (RxState.FcPending != TRUE)
    {
        return;
    }

    if (RxState.State != CANTP_RX_WAIT_CF)
    {
        RxState.FcPending = FALSE;
        return;
    }

    PduInfoType fcInfo;
    uint8 fcPayload[3];

    fcPayload[0] = 0x30;                        /* Continue To Send */
    fcPayload[1] = CanTp_RxSduCfg[0].BlockSize;
    fcPayload[2] = CanTp_RxSduCfg[0].STmin;

    fcInfo.SduDataPtr = fcPayload;
    fcInfo.SduLength = sizeof(fcPayload);

    CanTp_FcLastResult = (uint8)CanIf_Transmit(
        CanTp_RxSduCfg[0].CanIfTxFcPduId,
        &fcInfo);

    if (CanTp_FcLastResult == E_OK)
    {
        RxState.FcPending = FALSE;
        CanTp_FcTxOkCount++;
        Log_Print("[CanTp] FC TX OK\r\n");
    }
    else
    {
        /* Giữ pending để retry ở vòng lặp kế tiếp. */
        CanTp_FcTxFailCount++;
        Log_Print("[CanTp] FC TX FAIL\r\n");
    }
}

void CanTp_MainFunction(void) {
    /* Gửi hoặc retry Flow Control cho response multi-frame. */
    CanTp_TrySendPendingFlowControl();

    /* Lấy trạng thái TX đầu tiên */
    CanTp_TxStateType* tx = &TxState[0];

    /* ===== Kiểm tra nếu đang ở trạng thái gửi CF ===== */
    if (tx->State == CANTP_TX_SEND_CF)
    {
        /* ===== BƯỚC 1: Chuẩn bị frame CAN ===== */
        PduInfoType canIfTxInfo;
        uint8 canPayload[8];
        canIfTxInfo.SduDataPtr = canPayload;

        /* ===== BƯỚC 2: Tạo PCI cho CF =====
         * Format: 0x20 | SequenceNumber (4 bits thấp)
         * SN quay vòng 1-15, sau đó về 0
         */
        canPayload[0] = 0x20 | (tx->SN & 0x0F);

        /* ===== BƯỚC 3: Tính số bytes cần gửi trong CF này =====
         * Mỗi CF tối đa 7 bytes dữ liệu
         */
        uint32 remain = tx->TotalLength - tx->SentLength;
        uint8 len = (remain > 7) ? 7 : (uint8)remain;

        /* ===== BƯỚC 4: Copy dữ liệu từ buffer vào CF ===== */
        memcpy(&canPayload[1], &tx->TxBuffer[tx->SentLength], len);
        canIfTxInfo.SduLength = 1 + len; /* 1 byte PCI + data */

        /* ===== BƯỚC 5: Cập nhật tiến trình ===== */
        tx->SentLength += len;  /* Tăng số bytes đã gửi */
        tx->SN++;               /* Tăng sequence number cho CF tiếp theo */

        /* ===== BƯỚC 6: Gửi CF xuống CANIf ===== */
        // tx->State = CANTP_TX_WAIT_CF_CONF; /* Chuyển sang trạng thái đợi xác nhận */
        CanIf_Transmit(CanTp_TxSduCfg[0].CanIfTxPduId, &canIfTxInfo);

        /* ===== BƯỚC 7: Kiểm tra đã gửi hết chưa ===== */
        if (tx->SentLength >= tx->TotalLength)
        {
            /* Đã gửi toàn bộ dữ liệu! */
            tx->State = CANTP_IDLE; /* Quay về trạng thái IDLE */

            /* Báo kết quả lên tầng PduR */
            PduR_CanTpTxConfirmation(CanTp_TxSduCfg[0].PduRTxSduId, NTFRSLT_OK);
        }
        else
        {
            /* Vẫn còn dữ liệu, tiếp tục gửi CF trong vòng lặp kế tiếp */
            tx->State = CANTP_TX_SEND_CF; /* Giữ trạng thái gửi CF */
        }
    }

    /* Lưu ý: Chưa xử lý timeout N_Bs và N_Cs trong bản này */
}
