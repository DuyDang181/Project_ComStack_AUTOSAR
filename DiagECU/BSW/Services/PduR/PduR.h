/**********************************************************
 * @file    PduR.h
 * @brief   AUTOSAR PDU Router
 *            - PduR_Com.h    (upper: COM)
 *            - PduR_CanIf.h  (lower: CanIf)
 *
 * @version 1.0
 * @date    3/8/2026
 * @author  Duy Dang
 **********************************************************/
#ifndef PDUR_H
#define PDUR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

/* Ví dụ AUTOSAR module */
#define PDUR_VENDOR_ID      (0u)
#define PDUR_MODULE_ID      (51u)
#define PDUR_SW_MAJOR_VERSION (1u)
#define PDUR_SW_MINOR_VERSION (0u)
#define PDUR_SW_PATCH_VERSION (0u)

/* Trạng thái PduR */
typedef enum {
    PDUR_UNINIT = 0,
    PDUR_ONLINE = 1
} PduR_StateType;

/* ===== Development errors (rút gọn, theo bảng SWS_PduR_00100) ===== */
#define PDUR_E_INIT_FAILED                     ((uint8)0x00)
#define PDUR_E_UNINIT                          ((uint8)0x01)
#define PDUR_E_PDU_ID_INVALID                  ((uint8)0x02)
#define PDUR_E_ROUTING_PATH_GROUP_ID_INVALID   ((uint8)0x08)
#define PDUR_E_PARAM_POINTER                   ((uint8)0x09)

/* ===== Routing Group ID (demo) ===== */
typedef uint16 PduR_RoutingPathGroupIdType;

/* ===== Cấu hình Post-Build (PB) rút gọn =====*/
typedef struct
{
    /* implementation-specific: trỏ đến bảng route COM→CanIf, CanIf→COM,...*/
    const void* DiagTxRoutingTable; /* Bảng route cho Diagnostic TX */
    const void* PduR_DCM_Routes;    /* Bảng route cho CanTp -> DCM */
    const void* ComTxRoutingTable;
    const void* CanIfRxRoutingTable;
    const void* LinIfRxRoutingTable;
    const void* CanIfTxConfRoutingTable;
    const void* CanIfTrigTxRoutingTable;
    uint32       ConfigId;   /* id duy nhất (Enable/DisableRouting dùng) */
} PduR_PBConfigType;

/* ===== API chung ===== */
/** @brief Khởi tạo PDU Router với cấu hình PB. */
void PduR_Init(const PduR_PBConfigType* ConfigPtr);

/** @brief Bật routing theo RoutingPathGroup (demo: bật toàn cục). */
Std_ReturnType PduR_EnableRouting(PduR_RoutingPathGroupIdType id);

/** @brief Tắt routing theo RoutingPathGroup (demo: tắt toàn cục). */
Std_ReturnType PduR_DisableRouting(PduR_RoutingPathGroupIdType id);

/** @brief Trạng thái hiện tại của PduR. */
PduR_StateType PduR_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* PDUR_H */
