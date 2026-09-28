/**********************************************************
 * @file    Diag_Cfg.h
 * @brief   AUTOSAR Diagnostic – Cấu hình các service Diagnostic
 * @details Định danh I-PDU và Signal cho các service Diagnostic:
 *          - DiagnosticSessionControl (0x10)
 *          - ReadDataByIdentifier (0x22)
 *          - ReadDTCInformation (0x19)
 *          - ClearDiagnosticInformation (0x14)
 *          - InvalidService (0xDD)
 **********************************************************/
#ifndef DIAG_CFG_H
#define DIAG_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ===== Số lượng ===== */
#define DIAG_NUM_IPDUS    (5u)    /* 5 Diagnostic TX I-PDUs */
#define DIAG_NUM_SIGNALS  (0u)    /* Không có signal nào cho các service Diagnostic */
#define DIAG_MAX_IPDU_LEN (8u)    /* max PDU length */

/* ===== Service IDs ===== */
#define DiagConf_DiagIPdu_UDS_SessionControl                ((uint8)0x10U)
#define DiagConf_DiagIPdu_UDS_ReadDataByIdentifier          ((uint8)0x22U)
#define DiagConf_DiagIPdu_UDS_ReadDTCInformation            ((uint8)0x19U)
#define DiagConf_DiagIPdu_UDS_ClearDiagnosticInformation    ((uint8)0x14U)
#define DiagConf_DiagIPdu_UDS_InvalidService                ((uint8)0x01U)

/* ===== ID N-SDU Diagnostic TX ===== */
#define DiagConf_CanTpTxNSdu_DiagTx (0u)
#define DiagConf_CanTpRxNSdu_DiagRx (0u)

/* ===== DID theo Service ReadDataByIdentifier ===== */
#define DiagConf_Did_ReadDataByIdentifier_VIN                   ((uint16)0xF190U)
#define DiagConf_Did_ReadDataByIdentifier_SoftwareVersion       ((uint16)0xF189U)
#define DiagConf_Did_ReadDataByIdentifier_EngineRPM             ((uint16)0x010CU)
#define DiagConf_Did_ReadDataByIdentifier_CoolantTemperature    ((uint16)0x0105U)
#define DiagConf_Did_ReadDataByIdentifier_eVCUSnapshot          ((uint16)0xF001U)

typedef enum {
    DIAG_PDU_DIR_TX = 0U,
    DIAG_PDU_DIR_RX = 1U
} Diag_PduDirection_e;

typedef enum {
    DIAG_SIGTYPE_UINT8 = 0U,
    DIAG_SIGTYPE_BOOLEAN = 1U
} Diag_SignalType_e;

typedef struct
{
    PduIdType          PduId;
    PduLengthType      Length;
    Diag_PduDirection_e Direction;
} Diag_IPduCfgType;

#ifdef __cplusplus
}
#endif

#endif /* DIAG_CFG_H */
