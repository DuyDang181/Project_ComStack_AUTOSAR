/**********************************************************
 * @file    Diag_Cfg.c
 * @brief   Bảng cấu hình Diagnostic – 5 I-PDU
 **********************************************************/
#include "Diag_Cfg.h"

const Diag_IPduCfgType Diag_IPduCfg[DIAG_NUM_IPDUS] = {
    { .PduId = DiagConf_DiagIPdu_UDS_SessionControl,                .Length = 2u, .Direction = DIAG_PDU_DIR_TX },
    { .PduId = DiagConf_DiagIPdu_UDS_ReadDataByIdentifier,          .Length = 3u, .Direction = DIAG_PDU_DIR_TX },
    { .PduId = DiagConf_DiagIPdu_UDS_ReadDTCInformation,            .Length = 3u, .Direction = DIAG_PDU_DIR_TX },
    { .PduId = DiagConf_DiagIPdu_UDS_ClearDiagnosticInformation,    .Length = 2u, .Direction = DIAG_PDU_DIR_TX },
    { .PduId = DiagConf_DiagIPdu_UDS_InvalidService,                .Length = 1u, .Direction = DIAG_PDU_DIR_TX }
};
