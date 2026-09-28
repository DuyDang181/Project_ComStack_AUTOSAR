/**********************************************************
 * @file    PduR.h
 * @brief   AUTOSAR PDU Router
 *
 * @version 1.0
 * @date    26/09/2026
 * @author  Duy Dang
 **********************************************************/
#ifndef PDUR_H
#define PDUR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

#define PDUR_VENDOR_ID      (0u)
#define PDUR_MODULE_ID      (51u)
#define PDUR_SW_MAJOR_VERSION (1u)
#define PDUR_SW_MINOR_VERSION (0u)
#define PDUR_SW_PATCH_VERSION (0u)

/* Trạng thái PduR */
typedef enum
{
    PDUR_UNINIT = 0U,
    PDUR_ONLINE = 1U
} PduR_StateType;

/* ===== Routing Group ID ===== */
typedef uint16 PduR_RoutingPathGroupIdType;

/* ===== Cấu hình Post-Build (PB) ===== */
typedef struct
{
    const void* DiagTxRoutingTable; /* Bảng route cho Diagnostic TX */
    const void* PduR_DCM_Routes;    /* Bảng route cho CanTp -> DCM */
    const void* ComTxRoutingTable;
    const void* CanIfRxRoutingTable;
    const void* LinIfRxRoutingTable;
    const void* CanIfTxConfRoutingTable;
    const void* CanIfTrigTxRoutingTable;
    uint32       ConfigId;          /* ID duy nhất (Enable/DisableRouting dùng) */
} PduR_PBConfigType;

/* ===== API ===== */
/** @brief Khởi tạo PDU Router với cấu hình PB. */
void PduR_Init(const PduR_PBConfigType* ConfigPtr);

/** @brief Bật routing theo RoutingPathGroup */
Std_ReturnType PduR_EnableRouting(PduR_RoutingPathGroupIdType Id);

/** @brief Tắt routing theo RoutingPathGroup */
Std_ReturnType PduR_DisableRouting(PduR_RoutingPathGroupIdType Id);

/** @brief Trạng thái hiện tại của PduR. */
PduR_StateType PduR_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* PDUR_H */
