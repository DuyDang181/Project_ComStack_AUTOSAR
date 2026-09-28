/**********************************************************
 * @file    Com.h
 * @brief   AUTOSAR COM – API cơ bản truyền nhận I-PDU
 *
 * @version 1.0.0
 * @date    29/08/2026
 * @author  Duy Dang
 **********************************************************/
#ifndef COM_H
#define COM_H

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Include ===== */
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Com_Cfg.h"

/* =========================================================
 * Lifecycle
 * =======================================================*/

/**
 * @brief   Khởi tạo COM module.
 * @details Thiết lập/clear các shadow buffers & biến nội bộ.
 *          Phải được gọi trước khi dùng mọi API COM khác.
 */
void Com_Init (const Com_ConfigType* config);

/**
 * @brief   Ghi một Signal vào shadow buffer của I-PDU theo cấu hình.
 *
 * @param   SignalId       ID tín hiệu (index trong Com_SignalCfg).
 * @param   SignalDataPtr  Con trỏ dữ liệu nguồn (đúng kiểu của Signal).
 *
 * @return  uint8
 *          - E_OK: dịch vụ chấp nhận, đã pack vào shadow buffer.
 *          - COM_SERVICE_NOT_AVAILABLE: dịch vụ không khả dụng
 *            (ID/hướng/bộ đệm không hợp lệ, I-PDU không TX,...
 *          - COM_BUSY: tài nguyên bận (ví dụ TP-buffer)
 *
 * @brief   Ghi một Signal vào shadow buffer của I-PDU theo cấu hình.
 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr);

/**
 * @brief   Đọc một Signal từ shadow buffer của I-PDU.
 *
 * @param   SignalId       ID tín hiệu (index trong Com_SignalCfg).
 * @param   SignalDataPtr  Con trỏ bộ nhớ để lưu dữ liệu đọc ra.
 *
 * @return  uint8
 *          - E_OK: đã đọc thành công.
 *          - COM_SERVICE_NOT_AVAILABLE: ID/hướng không hợp lệ.
 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr);

/**
 * @brief   Kích phát gửi I-PDU (TX) ngay xuống PduR.
 *
 * @param   PduId  ID I-PDU (TX) cần gửi.
 *
 * @return  Std_ReturnType
 *          - E_OK: đã gọi xuống PduR thành công.
 *          - E_NOT_OK: I-PDU không hợp lệ/chưa sẵn sàng (ví dụ group stopped).
 *
 * @details Chữ ký & mã trả về theo SWS
 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId);

/* =========================================================
 * Liên kết PduR: Callback/callout để PduR/CanIf gọi lên COM
 * =======================================================*/
void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr);
void Com_TxConfirmation(PduIdType ComTxPduId);
Std_ReturnType Com_TriggerTransmit(PduIdType ComTxPduId, PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* COM_H */
