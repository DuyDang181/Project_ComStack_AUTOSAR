/**********************************************************
 * @file    Can_GeneralTypes.h
 * @brief   Kiểu dữ liệu chung cho CAN Stack (AUTOSAR)
 *
 * @details Định nghĩa kiểu dữ liệu được dùng
 *          chung giữa CAN Driver và CAN Interface (CanIf):
 *
 * @version 1.0
 * @date    26/08/2026
 * @author  Duy Dang
 **********************************************************/

#ifndef CAN_GENERAL_TYPES_H
#define CAN_GENERAL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"
#include "ComStack_Types.h"

/* ===========================================================
 * Can_ReturnType – Return type của CAN Driver
 * ===========================================================*/
typedef enum
{
    CAN_OK = 0,         /* Thành công */
    CAN_NOT_OK,         /* Lỗi chung */
    CAN_BUSY            /* Mailbox TX bận */
} Can_ReturnType;

/* ===========================================================
 * Can_IdType – CAN Identifier
 * -----------------------------------------------------------
 * CAN ID:
 *   - Standard ID: 11-bit (0x000..0x7FF)
 *   - Extended ID: 29-bit (0x00000000..0x1FFFFFFF)
 * ===========================================================*/
typedef uint32 Can_IdType;

/* ===========================================================
 * Can_HwHandleType – Handle phần cứng CAN
 * -----------------------------------------------------------
 * Hardware Transmit Handle ứng với mailbox -> HTH = 0, 1, 2
 * ===========================================================*/
typedef uint16 Can_HwHandleType;

/* ===========================================================
 * Can_PduType – CAN Protocol Data Unit
 * -----------------------------------------------------------
 * Cấu trúc CanIf truyền CAN Driver khi muốn gửi
 * một CAN frame. CAN Driver nhận qua Can_Write(HTH, &pdu).
 * ===========================================================*/
typedef struct
{
    PduIdType   swPduHandle; /* Handle được cung cấp bời CanIf */
    uint8       length;      /* Chiều dài data */
    Can_IdType  id;          /* CAN Identifier (11-bit hoặc 29-bit) */
    uint8*      sdu;         /* Con trỏ payload */
} Can_PduType;

#ifdef __cplusplus
}
#endif

#endif /* CAN_GENERAL_TYPES_H */
