/**********************************************************
 * @file    Com.h
 * @brief   AUTOSAR COM – API rút gọn cho truyền tín hiệu
 * @details Lớp COM theo phong cách AUTOSAR Classic:
 *          - TX: ghi Signal vào shadow buffer I-PDU (Com_SendSignal).
 *          - Trigger gửi I-PDU (Com_TriggerIPDUSend) xuống PduR.
 *
 * @version 1.0
 * @date    3/8/2026
 * @author  Duy Dang
 **********************************************************/
#ifndef COM_H
#define COM_H

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Chuẩn AUTOSAR cơ bản & kiểu PDU ===== */
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Com_Cfg.h"

#ifndef COM_SERVICE_NOT_AVAILABLE
/** Dịch vụ không khả dụng (ví dụ I-PDU group đang stopped, điều kiện không thỏa). */
#define COM_SERVICE_NOT_AVAILABLE   ((uint8)0x80u)
#endif

#ifndef COM_BUSY
/** Tài nguyên bận (ví dụ TP-buffer đang khóa cho dữ liệu lớn). */
#define COM_BUSY                    ((uint8)0x81u)
#endif

/* =========================================================
 * Lifecycle
 * =======================================================*/

/**
 * @brief   Khởi tạo COM module.
 * @details Thiết lập/clear các shadow buffers & biến nội bộ.
 *          Phải được gọi trước khi dùng mọi API COM khác.
 * @note    Việc bật truyền nhận (start I-PDU groups) do hệ thống quản lý.
 */
void Com_Init(void);

/* (Tùy nhu cầu triển khai) Có thể bổ sung Com_DeInit(void) trong tương lai
 * nếu bạn quản lý tài nguyên động. Bản demo TX-only hiện không cần. */

/* =========================================================
 * TX API
 * =======================================================*/

/**
 * @brief   Ghi một Signal vào shadow buffer của I-PDU theo cấu hình.
 *
 * @param   SignalId       ID tín hiệu (index trong Com_SignalCfg).
 * @param   SignalDataPtr  Con trỏ dữ liệu nguồn (đúng kiểu của Signal).
 *
 * @return  uint8
 *          - E_OK: dịch vụ chấp nhận, đã pack vào shadow buffer.
 *          - COM_SERVICE_NOT_AVAILABLE: dịch vụ không khả dụng
 *            (ID/hướng/bộ đệm không hợp lệ, I-PDU không TX, v.v.).
 *          - COM_BUSY: tài nguyên bận (ví dụ TP-buffer), tùy hệ thống.
 *
 * @note    Khi cấu hình TransferProperty=TRIGGERED với TxMode=DIRECT/MIXED,
 *          hệ thống có thể kích phát gửi ngay (tối đa ở main function kế tiếp),
 *          tùy MDT/TMS. Trong demo, có thể chủ động gọi Com_TriggerIPDUSend().
 */
/**
 * @brief   Ghi một Signal vào shadow buffer của I-PDU theo cấu hình.
 * ...
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
 * @details Chữ ký & mã trả về theo SWS: API này dùng Std_ReturnType.
 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId);

/* =========================================================
 * Liên kết PduR
 * =======================================================*/
/* Callback/callout để PduR/CanIf gọi ngược lên COM */
void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr);
void Com_TxConfirmation(PduIdType ComTxPduId);
Std_ReturnType Com_TriggerTransmit(PduIdType ComTxPduId, PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* COM_H */
