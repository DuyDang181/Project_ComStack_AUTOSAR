/**********************************************************
 * @file    CanIf.c
 * @brief   AUTOSAR CAN Interface (CanIf) – Triển khai TX path
 *
 * @details Tầng ECU Abstraction kết nối PduR (phía trên) với
 *          CAN Driver (phía dưới).
 *
 * @version 1.0
 * @date    18/06/2026
 * @author  Duy Dang
 **********************************************************/

#include "CanIf.h"
#include <stdint.h>
#include "Can.h"               /* Can_Write() – API CAN Driver       */
#include "PduR.h"
#include "PduR_CanIf.h"        /* PduR_CanIfTxConfirmation() callback */
#include <string.h>

/* API được thay thế khi Application sử dụng xử lý trực tiếp dữ liệu từ CanIF */
__attribute__((weak)) void App_CanRxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr)
{
    (void)canId;
    (void)PduInfoPtr;
}

/* ===========================================================
 * prv_find_txpdu – Tìm index TX PDU trong bảng cấu hình
 * -----------------------------------------------------------
 * Lookup table tìm ID cho CanIf truyền xuống Driver
 * ===========================================================*/
static inline int16_t prv_find_txpdu(PduIdType id)
{
    for (uint16 i = 0; i < CANIF_NUM_TX_PDUS; ++i) {
        if (CanIf_TxPduCfg[i].CanIfTxPduId == id) {
            return (int16_t)i;
        }
    }
    return (int16_t)-1;
}

/* ===========================================================
 * CanIf_Init – Khởi tạo CanIf module
 * ===========================================================*/
void CanIf_Init(void)
{
    /* Đánh dấu CanIf đã được khởi tạo load config, init state controller,... */
    CanIf_Initialized = TRUE;
}

/* ===========================================================
 * CanIf_Transmit – PduR gọi: gửi I-PDU qua CAN bus
 * -----------------------------------------------------------
 * ===========================================================*/
Std_ReturnType CanIf_Transmit(PduIdType CanIfTxPduId, const PduInfoType* PduInfoPtr)
{
    /* Bước 1: Kiểm tra điều kiện */
    if (!CanIf_Initialized)
    {
        return E_NOT_OK;
    }
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL))
    {
        return E_NOT_OK;
    }

    /* Bước 2: Tra bảng TX PDU config */
    int16_t idx = prv_find_txpdu(CanIfTxPduId);
    if (idx < 0) { return E_NOT_OK; }

    const CanIf_TxPduCfgType* cfg = &CanIf_TxPduCfg[(uint16)idx];

    /* Bước 3: Kiểm tra DLC không vượt config */
    if (PduInfoPtr->SduLength > cfg->DlcMax) {
        return E_NOT_OK;
    }

    /* Bước 4: Dựng Can_PduType cho CAN Driver */
    Can_PduType frame;
    frame.swPduHandle = CanIfTxPduId;                 /* Handle để callback ngược */
    frame.length      = (uint8)PduInfoPtr->SduLength; /* DLC */
    frame.id          = cfg->CanId;                   /* CAN ID từ config */
    frame.sdu         = (uint8*)PduInfoPtr->SduDataPtr; /* Payload */

    /* Bước 5: Gọi xuống CAN Driver (MCAL) */
    Can_ReturnType rc = Can_Write(cfg->Hth, &frame);
    return (rc == CAN_OK) ? E_OK : E_NOT_OK;
}

/* ===========================================================
 * CanIf_TxConfirmation – CAN Driver báo TX hoàn tất
 * -----------------------------------------------------------
 * Khi CAN Driver (Can_MainFunction_Write) phát hiện mailbox
 * đã truyền xong
 * ===========================================================*/
void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    PduR_CanIfTxConfirmation(CanTxPduId);
}

/* ===========================================================
 * CanIf_TriggerTransmit – CAN Driver yêu cầu dữ liệu (pull)
 * -----------------------------------------------------------
 * Goiij dữ liệu trực tiếp từ tầng trên mà không thông qua Can_Write
 * ===========================================================*/
Std_ReturnType CanIf_TriggerTransmit(PduIdType CanTxPduId, PduInfoType* PduInfoPtr)
{
    return PduR_CanIfTriggerTransmit(CanTxPduId, PduInfoPtr);
}

/* ===========================================================
 * CanIf_RxIndication – CAN Driver báo có RX Frame
 * -----------------------------------------------------------
 * CAN ID của frame nhận được (dùng để routing)
 * ===========================================================*/
volatile Can_IdType CanIf_LastRxIds[10] = {0};
volatile uint32 CanIf_RxIdIndex = 0;

void CanIf_RxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr)
{
    if (PduInfoPtr == NULL) return;

    CanIf_LastRxIds[CanIf_RxIdIndex % 10] = canId;
    CanIf_RxIdIndex++;
    App_CanRxIndication(canId, PduInfoPtr);

#ifdef EVCU_DIAG_ECU
    if (canId == 0x7E8) {
        return;
    }
#endif

    extern void CanTp_RxIndication(PduIdType CanTpRxPduId, const PduInfoType* CanTpRxPduPtr);

    /* Routing theo CAN ID */
    if (canId == 0x7DF || canId == 0x7E8 || canId == 0x7E0)
    {
        CanTp_RxIndication(0, PduInfoPtr);
    }
    else if (canId == 0x180)
    {
        PduR_CanIfRxIndication(CanIfConf_Pdu_EngineCmd, PduInfoPtr);
    }
    else if (canId == 0x181)
    {
        /* COM PDU: EngineStatus (giả định dùng chung PduId 5 cho RX) */
        PduR_CanIfRxIndication(CanIfConf_Pdu_EngineStatus, PduInfoPtr);
    }
}
