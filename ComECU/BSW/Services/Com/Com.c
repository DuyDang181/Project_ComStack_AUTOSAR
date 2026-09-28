/**********************************************************
 * @file    Com.c
 * @brief   AUTOSAR COM – Các API cơ bản truyền nhận I-PDU (TX/RX)
 *
 * @version 1.0.0
 * @date    29/08/2026
 * @author  Duy Dang
 **********************************************************/

#include "Com.h"
#include "PduR_Com.h"

/* ===== Kiểm tra cấu hình ===== */
#ifndef COM_MAX_IPDU_LEN
#error "COM_MAX_IPDU_LEN must be defined in Com_Cfg.h"
#endif

/* ===== Các bảng cấu hình ===== */
static const Com_ConfigType* ComCurentCfg = NULL_PTR;
extern const Com_IPduCfgType   Com_IPduCfg[COM_NUM_IPDUS];
extern const Com_SignalCfgType Com_SignalCfg[COM_NUM_SIGNALS];

/* ===== Guard nếu header chưa định nghĩa SWS ===== */
#ifndef COM_SERVICE_NOT_AVAILABLE
#define COM_SERVICE_NOT_AVAILABLE   ((uint8)0x80u)
#endif
#ifndef COM_BUSY
#define COM_BUSY                    ((uint8)0x81u)
#endif

/* ===== Shadow buffer cho từng I-PDU (TX) ===== */
static uint8 Com_IpduBuff[COM_NUM_IPDUS][COM_MAX_IPDU_LEN];

/* =========================================================
 * HELPERS
 * =========================================================*/
/**
 * @brief  Tìm index I-PDU theo PduId trong bảng cấu hình
 * @param  pduId  ID I-PDU (ComConf_ComIPdu_*)
 * @return -1 nếu không thấy; ngược lại là index
 */
static int16 prv_Find_Ipdu_Index(PduIdType pduId)
{
    for (int16 i = 0u; i < COM_NUM_IPDUS; ++i)
    {
        if (ComCurentCfg->Com_IPduCfg[i].PduId == pduId)
        {
            return (int16)i;
        }
    }
    return (int16)-1;
}

/**
 * @brief  Lấy con trỏ buffer I-PDU kèm độ dài/hướng
 * @param  pduId   ID I-PDU
 * @param  outLen  [opt] độ dài I-PDU (byte)
 * @param  outDir  [opt] hướng I-PDU (TX/RX)
 * @return Con trỏ shadow buffer hoặc NULL nếu không hợp lệ
 */
static uint8* prv_Get_Ipdu_Buffer(PduIdType pduId,
                               PduLengthType* outLen,
                               Com_PduDirection_e* outDir)
{
    int16 idx = prv_Find_Ipdu_Index(pduId);
    if (idx < 0)
    {
        return NULL_PTR;
    }

    if (outLen)
    {
        *outLen = ComCurentCfg->Com_IPduCfg[(uint16)idx].Length;
    }

    if (outDir)
    {
        *outDir = ComCurentCfg->Com_IPduCfg[(uint16)idx].Direction;
    }

    return &Com_IpduBuff[(uint16)idx][0];
}

/* =========================================================
 * HELPERS – pack/unpack bit/byte
 * =======================================================*/

/** Ghi 8-bit vào vị trí byteIndex của đích */
static inline void put_u8(uint8* dst, uint16 byteIndex, uint8 v)
{
    dst[byteIndex] = v;
}

/** Ghi 4-bit (nibble) vào [byteIndex : bitOffset..bitOffset+3] */
static void put_nibble(uint8* dst, uint16 byteIndex, uint8 bitOffset, uint8 v4)
{
    const uint8 mask = (uint8)(0x0Fu << bitOffset);
    const uint8 val  = (uint8)((v4 & 0x0Fu) << bitOffset);
    dst[byteIndex] = (uint8)((dst[byteIndex] & (uint8)(~mask)) | val);
}

/** Ghi 16-bit vào vị trí byteIndex (Big Endian giả định cho RPM) */
static inline void put_u16(uint8* dst, uint16 byteIndex, uint16 v)
{
    dst[byteIndex]     = (uint8)(v >> 8);
    dst[byteIndex + 1] = (uint8)(v & 0xFFu);
}

/** Ghi 1-bit vào [byteIndex : bitOffset] */
static void put_bit(uint8* dst, uint16 byteIndex, uint8 bitOffset, boolean b)
{
    const uint8 mask = (uint8)(1u << bitOffset);
    if (b) { dst[byteIndex] |=  mask; }
    else   { dst[byteIndex] &= (uint8)(~mask); }
}

/** Đọc 8-bit từ vị trí byteIndex */
static inline uint8 get_u8(const uint8* src, uint16 byteIndex)
{
    return src[byteIndex];
}

/** Đọc 16-bit từ vị trí byteIndex (Big Endian giả định cho RPM) */
static inline uint16 get_u16(const uint8* src, uint16 byteIndex)
{
    return (uint16)(((uint16)src[byteIndex] << 8) | (uint16)src[byteIndex + 1]);
}

/** Đọc 1-bit từ [byteIndex : bitOffset] */
static inline boolean get_bit(const uint8* src, uint16 byteIndex, uint8 bitOffset)
{
    return (src[byteIndex] & (uint8)(1u << bitOffset)) ? TRUE : FALSE;
}

static inline uint8 get_nibble(const uint8* src, uint16 byteIndex, uint8 bitOffset)
{
    return (uint8)((src[byteIndex] >> bitOffset) & 0x0Fu);
}

__attribute__((weak)) void App_ComRxIndication(PduIdType ComRxPduId)
{
    (void)ComRxPduId;
}

/* =========================================================
 * LIFECYCLE
 * =======================================================*/

/**
 * @brief  Khởi tạo COM (shadow buffers/biến nội bộ)
 */
void Com_Init(const Com_ConfigType* config)
{
    if (config == NULL_PTR)
    {
        return;
    }

    /* Lưu con trỏ cấu hình hiện tại */
    ComCurentCfg = config;

    /* Khởi tạo buffer shadow */
    (void)memset(Com_IpduBuff, 0U, sizeof(Com_IpduBuff));

    /* Bật cờ Init */
    Com_Initialized = TRUE;
}

/* =========================================================
 * API – Gửi/nhận signal, trigger I-PDU
 * =======================================================*/

/**
 * @brief  Cập nhật giá trị signal vào shadow buffer của I-PDU
 * @param  SignalId      ID của signal (index vào Com_SignalCfg)
 * @param  SignalDataPtr Con trỏ dữ liệu nguồn
 * @return uint8  E_OK / COM_SERVICE_NOT_AVAILABLE / COM_BUSY
 * @note   Bản demo không có quản lý TP-buffer nên không phát sinh COM_BUSY.
 *         Các lỗi tham số/biên (ID không hợp lệ, hướng không đúng, vượt biên)
 *         được quy về COM_SERVICE_NOT_AVAILABLE để tuân kiểu trả về SWS.
 */
uint8 Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    if (SignalDataPtr == NULL_PTR)
    {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    if (SignalId >= (Com_SignalIdType)COM_NUM_SIGNALS)
    {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    const Com_SignalCfgType* cfg = &Com_SignalCfg[SignalId];

    PduLengthType ipduLen = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_RX;
    uint8* ipdu = prv_Get_Ipdu_Buffer(cfg->PduId, &ipduLen, &dir);

    if ((ipdu == NULL_PTR) || (dir != COM_PDU_DIR_TX) || (cfg->ByteIndex >= ipduLen))
    {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    /* Pack theo cấu hình độ dài/kiểu */
    switch (cfg->BitLength)
    {
        case 16u:
            put_u16(ipdu, cfg->ByteIndex, *(const uint16*)SignalDataPtr);
            break;

        case 8u:
            if (cfg->Type == COM_SIGTYPE_BOOLEAN)
            {
                const uint8 v = (*(const boolean*)SignalDataPtr) ? 1u : 0u;
                put_u8(ipdu, cfg->ByteIndex, v);
            }
            else
            {
                put_u8(ipdu, cfg->ByteIndex, *(const uint8*)SignalDataPtr);
            }
            break;

        case 4u:
        {
            const uint8 v4 = (uint8)(*(const uint8*)SignalDataPtr & 0x0Fu);
            put_nibble(ipdu, cfg->ByteIndex, cfg->BitOffset, v4);
        }
        break;

        case 1u:
        {
            const boolean b = (*(const boolean*)SignalDataPtr) ? TRUE : FALSE;
            put_bit(ipdu, cfg->ByteIndex, cfg->BitOffset, b);
        }
        break;

        default:
            return COM_SERVICE_NOT_AVAILABLE;
    }

    return E_OK;
}

/**
 * @brief  Kích phát gửi I-PDU qua PduR
 * @param  PduId ID I-PDU (TX)
 * @return Std_ReturnType  E_OK / E_NOT_OK
 * @details Kiểm tra buffer/hướng/độ dài rồi gọi PduR_ComTransmit().
 */
Std_ReturnType Com_TriggerIPDUSend(PduIdType PduId)
{
    PduLengthType length = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_RX;

    uint8* buffer = prv_Get_Ipdu_Buffer(PduId, &length, &dir);

    if ((buffer == NULL_PTR) || (dir != COM_PDU_DIR_TX) ||
        (length == 0u) || (length > (PduLengthType)COM_MAX_IPDU_LEN))
    {
        return E_NOT_OK;
    }

    PduInfoType PduInfo;
    PduInfo.SduDataPtr = buffer;
    PduInfo.SduLength  = length;

    return PduR_ComTransmit(PduId, &PduInfo);
}

/**
 * @brief  Đọc giá trị signal từ Rx shadow buffer của I-PDU
 * @param  SignalId      ID của signal
 * @param  SignalDataPtr Con trỏ đích để lưu dữ liệu
 * @return uint8  E_OK / COM_SERVICE_NOT_AVAILABLE
 */
uint8 Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    if (SignalDataPtr == NULL_PTR)
    {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    if (SignalId >= (Com_SignalIdType)COM_NUM_SIGNALS)
    {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    const Com_SignalCfgType* cfg = &Com_SignalCfg[SignalId];

    PduLengthType IpduLen = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_TX;
    uint8* ipdu = prv_Get_Ipdu_Buffer(cfg->PduId, &IpduLen, &dir);

    if ((ipdu == NULL_PTR) || (dir != COM_PDU_DIR_RX) || (cfg->ByteIndex >= IpduLen))
    {
        return COM_SERVICE_NOT_AVAILABLE;
    }

    /* Unpack theo cấu hình độ dài */
    switch (cfg->BitLength)
    {
        case 16u:
            *(uint16*)SignalDataPtr = get_u16(ipdu, cfg->ByteIndex);
            break;

        case 8u:
            if (cfg->Type == COM_SIGTYPE_BOOLEAN)
            {
                *(boolean*)SignalDataPtr = (get_u8(ipdu, cfg->ByteIndex) != 0u) ? TRUE : FALSE;
            }
            else
            {
                *(uint8*)SignalDataPtr = get_u8(ipdu, cfg->ByteIndex);
            }
            break;

        case 1u:
            *(boolean*)SignalDataPtr = get_bit(ipdu, cfg->ByteIndex, cfg->BitOffset);
            break;

        case 4u:
            *(uint8*)SignalDataPtr = get_nibble(ipdu, cfg->ByteIndex, cfg->BitOffset);
            break;

        default:
            return COM_SERVICE_NOT_AVAILABLE;
    }

    return E_OK;
}

/* =========================================================
 * Callback/Callout cho PduR/CanIf
 * =======================================================*/

void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr)
{
    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        return;
    }

    PduLengthType len = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_TX;
    uint8* buf = prv_Get_Ipdu_Buffer(ComRxPduId, &len, &dir);

    if ((buf != NULL_PTR) && (dir == COM_PDU_DIR_RX))
    {
        PduLengthType copyLen = (PduInfoPtr->SduLength < len) ? PduInfoPtr->SduLength : len;
        (void)memcpy(buf, PduInfoPtr->SduDataPtr, copyLen);
        App_ComRxIndication(ComRxPduId);
    }
}

void Com_TxConfirmation(PduIdType ComTxPduId)
{
    (void)ComTxPduId;
}

Std_ReturnType Com_TriggerTransmit(PduIdType ComTxPduId, PduInfoType* PduInfoPtr)
{
    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR))
    {
        return E_NOT_OK;
    }

    PduLengthType IpduLen = 0u;
    Com_PduDirection_e dir = COM_PDU_DIR_RX;
    uint8* buffer = prv_Get_Ipdu_Buffer(ComTxPduId, &IpduLen, &dir);

    if ((buffer == NULL_PTR) || (dir != COM_PDU_DIR_TX) || (IpduLen == 0u) || (IpduLen > COM_MAX_IPDU_LEN))
    {
        return E_NOT_OK;
    }

    (void)memcpy(PduInfoPtr->SduDataPtr, buffer, IpduLen);
    PduInfoPtr->SduLength = IpduLen;

    return E_OK;
}
