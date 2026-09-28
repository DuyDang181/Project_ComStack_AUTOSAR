/**********************************************************
 * @file    CanIf.c
 * @brief   AUTOSAR CAN Interface (CanIf)
 *
 * @version 1.0
 * @date    29/08/2026
 * @author  Duy Dang
 **********************************************************/

 /* ===========================================================
 * INCLUDES
 * ===========================================================*/
#include "CanIf.h"
#include <stdint.h>
#include "Can.h"
#include "PduR.h"
#include "PduR_CanIf.h"
#include <string.h>

__attribute__((weak)) void App_CanRxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr)
{
    (void)canId;
    (void)PduInfoPtr;
}

/* ===========================================================
 * HELPER: prv_find_txpdu – Tìm index TX PDU trong bảng cấu hình
 * Tra bảng CanIf_TxPduCfg[] để tìm entry có CanIfTxPduId
 * khớp với id truyền vào.
 * ===========================================================*/
static inline int16 prv_find_txpdu(PduIdType Id)
{
    for (uint16 i = 0; i < CANIF_NUM_TX_PDUS; ++i)
    {
        if (CanIf_TxPduCfg[i].CanIfTxPduId == Id)
        {
            return (int16)i;
        }
    }
    return (int16)-1;
}

/* ===========================================================
 * CanIf_Init – Khởi tạo CanIf module
 * ===========================================================*/
void CanIf_Init(const CanIf_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR)
    {
        return;
    }

    /* Bật cờ báo Init */
    CanIf_Initialized = TRUE;
}

/* ===========================================================
 * CanIf_Transmit – PduR gọi: gửi I-PDU qua CAN bus
 * ===========================================================*/
Std_ReturnType CanIf_Transmit(PduIdType CanIfTxPduId, const PduInfoType* PduInfoPtr)
{
    /* Kiểm tra điều kiện */
    if (!CanIf_Initialized)
    {
        return E_NOT_OK;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    /* Tra bảng TX PDU config */
    int16 idx = prv_find_txpdu(CanIfTxPduId);

    if (idx < 0)
    {
        return E_NOT_OK;
    }

    const CanIf_TxPduCfgType* cfg = &CanIf_TxPduCfg[(uint16)idx];

    /* Kiểm tra DLC không vượt config */
    if (PduInfoPtr->SduLength > cfg->DlcMax)
    {
        return E_NOT_OK;
    }

    /* Dựng Can_PduType cho CAN Driver */
    Can_PduType frame;
    frame.swPduHandle = CanIfTxPduId;                 /* Handle để callback ngược */
    frame.length      = (uint8)PduInfoPtr->SduLength; /* DLC */
    frame.id          = cfg->CanId;                   /* CAN ID từ config */
    frame.sdu         = (uint8*)PduInfoPtr->SduDataPtr; /* Payload */

    /* Gọi CAN Driver (MCAL) */
    Can_ReturnType rc = Can_Write(cfg->Hth, &frame);

    if (rc== CAN_OK)
    {
        return E_OK;
    }
    else
    {
        return E_NOT_OK;
    }
}

/* ===========================================================
 * CanIf_TxConfirmation – Khi CAN Driver (Can_MainFunction_Write)
 * phát hiện mailbox đã truyền xong
 * ===========================================================*/
void CanIf_TxConfirmation(PduIdType CanTxPduId)
{
    PduR_CanIfTxConfirmation(CanTxPduId);
}

/* ===========================================================
 * CanIf_TriggerTransmit – CAN Driver yêu cầu dữ liệu (pull)
 * ===========================================================*/
Std_ReturnType CanIf_TriggerTransmit(PduIdType CanTxPduId, PduInfoType* PduInfoPtr)
{
    return PduR_CanIfTriggerTransmit(CanTxPduId, PduInfoPtr);
}

/* ===========================================================
 * CanIf_RxIndication – CAN Driver báo có RX Frame
 * ===========================================================*/
volatile Can_IdType CanIf_LastRxIds[10] = {0};
volatile uint32 CanIf_RxIdIndex = 0;

void CanIf_RxIndication(Can_IdType canId, const PduInfoType* PduInfoPtr)
{
    if (PduInfoPtr == NULL_PTR)
    {
        return;
    }

    /* Lưu CAN ID của frame nhận được */
    CanIf_LastRxIds[CanIf_RxIdIndex % 10] = canId;
    CanIf_RxIdIndex++;
    App_CanRxIndication(canId, PduInfoPtr);

    extern void CanTp_RxIndication(PduIdType CanTpRxPduId, const PduInfoType* CanTpRxPduPtr);

    /* Routing CAN ID theo service */
    switch (canId)
    {
        case 0x7DF: /* Diagnostic request (functional)      */
        case 0x7E8: /* Diagnostic response (ECU)            */
        case 0x7E0: /* Diagnostic request (tester)          */
            CanTp_RxIndication(0, PduInfoPtr);
            break;

        case 0x180: /* EngineCmd I-PDU */
            PduR_CanIfRxIndication(CanIfConf_Pdu_EngineCmd, PduInfoPtr);
            break;

        case 0x181: /* EngineStatus I-PDU */
            PduR_CanIfRxIndication(CanIfConf_Pdu_EngineStatus, PduInfoPtr);
            break;

        default:
            /* Các CanID khác */
            break;
    }
}
