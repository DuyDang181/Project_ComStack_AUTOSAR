/**********************************************************
 * @file    PduR.c
 * @brief   AUTOSAR PDU Router – COM ↔ CanIf/LinIf
 *  Luồng TX (COM gửi xuống):
 *    COM → PduR_ComTransmit() → tra bảng route
 *      → nếu DestModule == CANIF → CanIf_Transmit()
 *      → nếu DestModule == LINIF → LinIf_Transmit()
 *
 *  Luồng callback (lower báo lên):
 *    CanIf → PduR_CanIfTxConfirmation() → Com_TxConfirmation()
 *    LinIf → PduR_LinIfTxConfirmation() → Com_TxConfirmation()
 **********************************************************/
#include "PduR.h"
#include "PduR_Cfg.h"
#include "PduR_Com.h"
#include "PduR_Diag.h"
#include "PduR_CanIf.h"
#include "PduR_LinIf.h"
#include "PduR_CanTp.h"

#include "Std_Types.h"
#include "ComStack_Types.h"

#include <stddef.h>
#include <stdint.h>

/* ===== EXTERN: Lower layers ===== */
extern Std_ReturnType CanIf_Transmit(PduIdType CanIfTxPduId, const PduInfoType* PduInfoPtr);
extern Std_ReturnType LinIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
extern Std_ReturnType CanTp_Transmit(PduIdType CanTpTxPduId, const PduInfoType* PduInfoPtr);

/* ===== EXTERN: Upper layer (COM) callbacks ===== */
extern void           Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr);
extern void           Com_TxConfirmation(PduIdType ComTxPduId);
extern Std_ReturnType Com_TriggerTransmit(PduIdType ComTxPduId, PduInfoType* PduInfoPtr);

/* ===== Trạng thái nội bộ ===== */
static PduR_StateType           s_State          = PDUR_UNINIT;
static const PduR_PBConfigType* s_Cfg            = NULL_PTR;
static boolean                  s_RoutingEnabled = FALSE;


/* ===== HELPER: Tìm route trong bảng COM TX ===== */
static inline int32_t prv_find_com_tx_route(PduIdType src)
{
    const PduR_Route1to1Type* tbl = (const PduR_Route1to1Type*)s_Cfg->ComTxRoutingTable;
    for (uint16 i = 0; i < PDUR_NUM_COM_TX_ROUTES; ++i) {
        if (tbl[i].SrcPduId == src) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* ===== HELPER: Tìm route trong bảng callback ===== */
static inline int32_t prv_find_callback_route(const PduR_CallbackRouteType* tbl,
                                               uint16 n, PduIdType src)
{
    for (uint16 i = 0; i < n; ++i) {
        if (tbl[i].SrcPduId == src) {
            return (int32_t)i;
        }
    }
    return -1;
}


/* ===== HELPER: Tìm route trong bảng DIAG TX ===== */
static inline int32 prv_find_diag_tx_route(PduIdType src)
{
    /* Tìm route trong bảng DIAG TX */
    const PduR_Route1to1Type* lookTable = (const PduR_Route1to1Type*)s_Cfg->DiagTxRoutingTable;

    for (uint16 index = 0; index < PDUR_NUM_DIAG_TX_ROUTES; ++index)
    {
        if (lookTable[index].SrcPduId == src)
        {
            return (int32)index;
        }
    }
    return -1;
}

/* =========================================================
 * Lifecycle
 * =======================================================*/

void PduR_Init(const PduR_PBConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL) {
        s_State = PDUR_UNINIT;
        s_Cfg   = NULL_PTR;
        s_RoutingEnabled = FALSE;
        return;
    }
    s_Cfg = ConfigPtr;
    s_State = PDUR_ONLINE;
    s_RoutingEnabled = TRUE;
}

Std_ReturnType PduR_EnableRouting(PduR_RoutingPathGroupIdType id)
{
    (void)id;
    if (s_State == PDUR_UNINIT) return E_NOT_OK;
    s_RoutingEnabled = TRUE;
    return E_OK;
}

Std_ReturnType PduR_DisableRouting(PduR_RoutingPathGroupIdType id)
{
    (void)id;
    if (s_State == PDUR_UNINIT) return E_NOT_OK;
    s_RoutingEnabled = FALSE;
    return E_OK;
}

PduR_StateType PduR_GetState(void)
{
    return s_State;
}

/* =========================================================
 * COM → PduR → CanIf HOẶC LinIf (dựa trên DestModule)
 * =======================================================*/

/**
 * @brief   COM yêu cầu truyền I-PDU
 * @details PduR tra bảng route, tìm DestModule:
 *          - PDUR_DEST_CANIF → gọi CanIf_Transmit()
 *          - PDUR_DEST_LINIF → gọi LinIf_Transmit()
 */
Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId, const PduInfoType* PduInfoPtr)
{
    if (s_State == PDUR_UNINIT)
    {
        return E_NOT_OK;
    }
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL)) {
        return E_NOT_OK;
    }
    if (!s_RoutingEnabled)
    {
        return E_NOT_OK;
    }
    if ((s_Cfg == NULL_PTR) || (s_Cfg->ComTxRoutingTable == NULL_PTR))
    {
        return E_NOT_OK;
    }

    /* Tra bảng route */
    int32_t idx = prv_find_com_tx_route(ComTxPduId);
    if (idx < 0)
    {
        return E_NOT_OK;
    }

    const PduR_Route1to1Type* tbl = (const PduR_Route1to1Type*)s_Cfg->ComTxRoutingTable;
    const PduIdType           dstId  = tbl[idx].DstPduId;
    const PduR_DestModuleType dest   = tbl[idx].DestModule;

    /* Dispatch theo DestModule */
    switch (dest) {
        case PDUR_DEST_CANIF:
            return CanIf_Transmit(dstId, PduInfoPtr);

        case PDUR_DEST_LINIF:
            return LinIf_Transmit(dstId, PduInfoPtr);

        case PDUR_DEST_CANTP:
            return CanTp_Transmit(dstId, PduInfoPtr);

        default:
            return E_NOT_OK;
    }
}

/* =========================================================
 * CanIf → PduR → COM (callback)
 * =======================================================*/

void PduR_CanIfRxIndication(PduIdType CanIfRxPduId, const PduInfoType* PduInfoPtr)
{
    if (s_State == PDUR_UNINIT || !s_RoutingEnabled) return;
    if ((s_Cfg == NULL_PTR) || (s_Cfg->CanIfRxRoutingTable == NULL_PTR)) return;

    /* Tra bảng route RX */
    int32_t idx = prv_find_callback_route((const PduR_CallbackRouteType*)s_Cfg->CanIfRxRoutingTable,
                                          PDUR_NUM_CANIF_RX_ROUTES,
                                          CanIfRxPduId);
    if (idx >= 0) {
        Com_RxIndication(((const PduR_CallbackRouteType*)s_Cfg->CanIfRxRoutingTable)[idx].DstPduId, PduInfoPtr);
    }
}

void PduR_CanIfTxConfirmation(PduIdType CanIfTxPduId)
{
    if (s_State == PDUR_UNINIT || !s_RoutingEnabled) return;

    /* Demo: Route CANTP Tx Confirmation */
    if (CanIfTxPduId == 3 /* DiagTx */ || CanIfTxPduId == 4 /* DiagRx_FC */) {
        extern void CanTp_TxConfirmation(PduIdType CanTpTxPduId);
        CanTp_TxConfirmation(0);
        return;
    }

    int32_t idx = prv_find_callback_route(PduR_CanIfTxConfRoutes,
                                          PDUR_NUM_CANIF_TXCONF_ROUTES,
                                          CanIfTxPduId);
    if (idx >= 0) {
        Com_TxConfirmation(PduR_CanIfTxConfRoutes[idx].DstPduId);
    }
}

Std_ReturnType PduR_CanIfTriggerTransmit(PduIdType CanIfTxPduId, PduInfoType* PduInfoPtr)
{
    if (s_State == PDUR_UNINIT || !s_RoutingEnabled) return E_NOT_OK;
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL)) return E_NOT_OK;

    int32_t idx = prv_find_callback_route(PduR_CanIfTrigTxRoutes,
                                          PDUR_NUM_CANIF_TRIGTX_ROUTES,
                                          CanIfTxPduId);
    if (idx < 0) return E_NOT_OK;

    return Com_TriggerTransmit(PduR_CanIfTrigTxRoutes[idx].DstPduId, PduInfoPtr);
}

/* =========================================================
 * LinIf → PduR → COM (callback)
 * =======================================================*/

void PduR_LinIfRxIndication(PduIdType LinIfRxPduId, const PduInfoType* PduInfoPtr)
{
    if (s_State == PDUR_UNINIT || !s_RoutingEnabled) return;
    if ((s_Cfg == NULL_PTR) || (s_Cfg->LinIfRxRoutingTable == NULL_PTR)) return;

    /* Tra bảng route RX cho LIN */
    int32_t idx = prv_find_callback_route((const PduR_CallbackRouteType*)s_Cfg->LinIfRxRoutingTable,
                                          PDUR_NUM_LINIF_RX_ROUTES,
                                          LinIfRxPduId);
    if (idx >= 0) {
        Com_RxIndication(((const PduR_CallbackRouteType*)s_Cfg->LinIfRxRoutingTable)[idx].DstPduId, PduInfoPtr);
    }
}

void PduR_LinIfTxConfirmation(PduIdType LinIfTxPduId)
{
    if (s_State == PDUR_UNINIT || !s_RoutingEnabled) return;

    int32_t idx = prv_find_callback_route(PduR_LinIfTxConfRoutes,
                                          PDUR_NUM_LINIF_TXCONF_ROUTES,
                                          LinIfTxPduId);
    if (idx >= 0) {
        Com_TxConfirmation(PduR_LinIfTxConfRoutes[idx].DstPduId);
    }
}

Std_ReturnType PduR_LinIfTriggerTransmit(PduIdType LinIfTxPduId, PduInfoType* PduInfoPtr)
{
    if (s_State == PDUR_UNINIT || !s_RoutingEnabled) return E_NOT_OK;
    if ((PduInfoPtr == NULL) || (PduInfoPtr->SduDataPtr == NULL)) return E_NOT_OK;

    int32_t idx = prv_find_callback_route(PduR_LinIfTrigTxRoutes,
                                          PDUR_NUM_LINIF_TRIGTX_ROUTES,
                                          LinIfTxPduId);
    if (idx < 0) return E_NOT_OK;

    return Com_TriggerTransmit(PduR_LinIfTrigTxRoutes[idx].DstPduId, PduInfoPtr);
}

/* =========================================================
 * CanTp → PduR → (COM / Dcm)
 * =======================================================*/
#include "PduR_CanTp.h"
#include "../dcm/Dcm.h"

void PduR_CanTpRxIndication(PduIdType RxPduId, NotifResultType Result)
{
    /* Định tuyến tới bộ định tuyến đích thực tế. Ở đây demo gọi Dcm */
    Dcm_TpRxIndication(RxPduId, Result);
}

BufReq_ReturnType PduR_CanTpStartOfReception(
    PduIdType id,
    const PduInfoType* info,
    PduLengthType TpSduLength,
    PduLengthType* bufferSizePtr)
{
    /* Định tuyến gọi thẳng sang Dcm_StartOfReception */
    return Dcm_StartOfReception(id, info, TpSduLength, bufferSizePtr);
}

BufReq_ReturnType PduR_CanTpCopyRxData(
    PduIdType id,
    const PduInfoType* info,
    PduLengthType* bufferSizePtr)
{
    /* Định tuyến gọi sang Dcm_CopyRxData */
    return Dcm_CopyRxData(id, info, bufferSizePtr);
}

void PduR_CanTpTxConfirmation(PduIdType TxPduId, NotifResultType Result)
{
    (void)TxPduId;
    (void)Result;
    /* Thực tế sẽ tra bảng và gọi Dcm_TxConfirmation() hoặc PDUR_COM_... */
}

/* =========================================================
 * Diagnostic ECU -> PduR -> CAN TP dựa trên bảng route
 * =======================================================*/

Std_ReturnType PduR_DiagTransmit(PduIdType DiagTxPduId, const PduInfoType* PduInfoPtr)
{
    /* Kiểm tra trạng thái của PduR */
    if (s_State == PDUR_UNINIT)
    {
        return E_NOT_OK;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (!s_RoutingEnabled)
    {
        return E_NOT_OK;
    }

    if ((s_Cfg == NULL_PTR) || (s_Cfg->ComTxRoutingTable == NULL_PTR))
    {
        return E_NOT_OK;
    }

    /* Tra bảng route */
    int32 index = prv_find_diag_tx_route(DiagTxPduId);

    if (index < 0U)
    {
        return E_NOT_OK;
    }

    /* Lấy thông tin đích */
    const PduR_Route1to1Type* lookUpTable = (const PduR_Route1to1Type*)s_Cfg->DiagTxRoutingTable;
    const PduIdType           destId  = lookUpTable[index].DstPduId;
    const PduR_DestModuleType destModule   = lookUpTable[index].DestModule;

    /* Dispatch theo DestModule */
    switch (destModule) {
        case PDUR_DEST_CANIF:
            return CanIf_Transmit(destId, PduInfoPtr);

        case PDUR_DEST_LINIF:
            return LinIf_Transmit(destId, PduInfoPtr);

        case PDUR_DEST_CANTP:
            return CanTp_Transmit(destId, PduInfoPtr);

        default:
            return E_NOT_OK;
    }
}