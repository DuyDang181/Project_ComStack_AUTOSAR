/**********************************************************
 * @file    CanIf.h
 * @brief   AUTOSAR CAN Interface (CanIf)
 *
 * @details Tầng ECU Abstraction trừu tượng hóa CAN Driver (MCAL)
 *          cho các module phía trên (PduR/COM).
 *
 * @version 1.0
 * @date    29/08/2026
 * @author  Duy Dang
 **********************************************************/
#ifndef CANIF_H
#define CANIF_H

 /* ===========================================================
 * INCLUDES
 * ===========================================================*/
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "CanIf_Cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Khởi tạo CanIf module
 * @details Set cờ CanIf_Initialized = TRUE
 */
void CanIf_Init (const CanIf_ConfigType* ConfigPtr);

/**
 * @brief   PduR gọi: yêu cầu gửi I-PDU qua CAN bus
 * @param   CanIfTxPduId  ID PDU phía CanIf (từ bảng route PduR)
 * @param   PduInfoPtr    Dữ liệu PDU (payload + length)
 * @return  E_OK: CAN Driver chấp nhận
 *          E_NOT_OK: lỗi
 */
Std_ReturnType CanIf_Transmit(PduIdType CanIfTxPduId, const PduInfoType* PduInfoPtr);

/**
 * @brief   Callback từ CAN Driver: TX đã hoàn tất
 * @param   CanTxPduId  Handle đã truyền (swPduHandle trong Can_PduType)
 */
void CanIf_TxConfirmation(PduIdType CanTxPduId);

/**
 * @brief   CAN Driver yêu cầu dữ liệu (pull model)
 * @param   CanTxPduId  PDU ID cần dữ liệu
 * @param   PduInfoPtr  [out] Buffer để điền dữ liệu
 * @return  E_OK: có dữ liệu
 *          E_NOT_OK: không có
 */
Std_ReturnType CanIf_TriggerTransmit(PduIdType CanTxPduId, PduInfoType* PduInfoPtr);

/**
 * @brief   Callback từ CAN Driver: RX đã hoàn tất
 * @param   canId      CAN ID của frame nhận được
 * @param   PduInfoPtr Thông tin frame (payload + length)
 */
void CanIf_RxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif
#endif /* CANIF_H */
