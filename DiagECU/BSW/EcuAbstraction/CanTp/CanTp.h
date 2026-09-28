/**********************************************************
 * @file    CanTp.h
 * @brief   AUTOSAR CAN Transport Protocol (CanTp)
 * @version 1.0
 * @date    28/06/2026
 * @author  Duy Dang
 **********************************************************/
#ifndef CANTP_H
#define CANTP_H

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "CanTp_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Khởi tạo module CanTp
 * @details Đặt tất cả state machine của TX và RX về trạng thái IDLE,
 *          khởi tạo các thông số ISO-TP.
 */
void CanTp_Init(void);

/**
 * @brief   Main Function của CanTp
 * @details Hàm gọi theo chu kỳ (ví dụ: 5ms hoặc 10ms) để xử lý timeout
 *          (N_As, N_Bs, N_Cs, N_Ar, N_Br, N_Cr) và quản lý việc gửi
 *          Consecutive Frames (CF), gửi Flow Control (FC).
 */
void CanTp_MainFunction(void);

/**
 * @brief   PduR yêu cầu truyền dữ liệu (PduR -> CanTp)
 * @param   CanTpTxSduId ID của N-SDU muốn gửi (từ cấu hình)
 * @param   CanTpTxInfoPtr Con trỏ chứa độ dài và data (có thể null nếu data kéo sau)
 * @return  E_OK nếu chấp nhận, E_NOT_OK nếu bận hoặc lỗi
 */
Std_ReturnType CanTp_Transmit(PduIdType CanTpTxSduId, const PduInfoType* CanTpTxInfoPtr);

/**
 * @brief   CanIf báo đã nhận một CAN Frame (CanIf -> CanTp)
 * @param   CanTpRxPduId ID của N-PDU nhận (từ cấu hình CanTp)
 * @param   CanTpRxPduPtr Thông tin data nhận được (độ dài thực tế <= 8 byte, hoặc 64 byte với CAN-FD)
 */
void CanTp_RxIndication(PduIdType CanTpRxPduId, const PduInfoType* CanTpRxPduPtr);

/**
 * @brief   CanIf báo đã phát xong một CAN Frame (CanIf -> CanTp)
 * @param   CanTpTxPduId ID của N-PDU truyền (từ cấu hình CanTp)
 */
void CanTp_TxConfirmation(PduIdType CanTpTxPduId);


/* ===== Export Status Variable ===== */
extern boolean CanTp_Initialized;

#ifdef __cplusplus
}
#endif
#endif /* CANTP_H */
