/**********************************************************
 * @file    PduR_Com.h
 * @brief   Giao diện PduR cho upper layer COM (non-TP)
 *
 * @version 1.0
 * @date    29/08/2026
 * @author  Duy Dang
 **********************************************************/
#ifndef PDUR_COM_H
#define PDUR_COM_H

#ifdef __cplusplus
extern "C" {
#endif

/* ====== Includes ====== */
#include "Std_Types.h"
#include "ComStack_Types.h"

/* ====== API ====== */
/**
 * @brief  COM yêu cầu truyền một I-PDU
 * @param  ComTxPduId   I-PDU ID phía COM
 * @param  PduInfoPtr   trỏ dữ liệu payload/length
 * @return E_OK / E_NOT_OK
 */
Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId, const PduInfoType* PduInfoPtr);

#ifdef __cplusplus
}
#endif

#endif /* PDUR_COM_H */
