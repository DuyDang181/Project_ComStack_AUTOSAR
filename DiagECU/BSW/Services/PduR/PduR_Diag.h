/**********************************************************
 * @file    PduR_Diag.h
 * @brief   Giao diện PduR cho upper layer Diagnostic
 * @details Theo SWS Diagnostic & SWS PduR:
 *          - Diagnostic gửi I-PDU qua PduR_DiagTransmit().
 *          - Khi lower cần data theo TriggerTransmit, PduR gọi
 *            Com_TriggerTransmit(). (callout chiều ngược)
 **********************************************************/
#ifndef PDUR_DIAG_H
#define PDUR_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ====== API cho Diagnostic ====== */
/**
 * @brief  Diagnostic yêu cầu truyền một I-PDU
 * @param  DiagTxPduId   I-PDU ID phía Diagnostic
 * @param  PduInfoPtr   Con trỏ dữ liệu payload/length
 * @return E_OK / E_NOT_OK (tổng hợp kết quả từ lower, 1:n có thể khác)
 */
Std_ReturnType PduR_DiagTransmit(PduIdType DiagTxPduId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* PDUR_DIAG_H */
