/**********************************************************
 * @file    main.c
 * @brief   AUTOSAR Classic Diagnostic – STM32F103
 *
 * @version 1.0
 * @date    06/08/2026
 * @author  Duy Dang
 **********************************************************/

/* ===== AUTOSAR BSW Includes ===== */
#include "Com.h"
#include "Com_Cfg.h"
#include "PduR.h"
#include "PduR_Diag.h"
#include "PduR_Cfg.h"
#include "CanTp.h"
#include "CanTp_Cfg.h"
#include "CanIf.h"
#include "Can.h"
#include "Diag_Cfg.h"
#include "Dcm.h"

/* ===== SPL Includes ===== */
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include <string.h>
#include <stdio.h>

/***********************************************************/
/* cờ này sẽ được phần mềm Python / GDB ghi đè từ 0 -> 1 để kích hoạt gửi */
volatile uint8 diag_trigger_session = 0;
volatile uint8 diag_trigger_vin = 0u;
volatile uint8 diag_trigger_snapshot = 0u;
volatile uint8 diag_trigger_dtc = 0u;
volatile uint8 diag_trigger_clear_dtc = 0u;
volatile uint8 diag_trigger_rpm = 0u;
volatile uint8 diag_trigger_temp = 0u;
volatile uint8 diag_trigger_unsupported = 0u;
volatile uint8 diag_last_response[64];
volatile uint16 diag_last_response_len = 0u;
volatile uint8 diag_last_complete = 0u;
volatile uint8 diag_last_sid = 0u;


/* ===========================================================
 * Delay demo dựa trên f_CPU
 * -----------------------------------------------------------
 * 7200 vòng NOP ≈ 1ms tại 72MHz (mỗi NOP ≈ 1 cycle)
 * ===========================================================*/
static void delay_ms(volatile uint32 ms)
{
    while (ms--) {
        for (volatile uint32 i = 0; i < 7200u; i++) {
            __asm__("nop");
        }
    }
}

/* ===========================================================
 * Cấu hình USART1 trên PA9 (TX) để debug.
 * ===========================================================*/
static void Log_Init(void)
{
    /* Bật clock cho USART1 và GPIOA */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* Cấu hình TX (PA9): Alternate Function Push-Pull */
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* Cấu hình USART1: 115200 baud, 8-bit, 1 stop bit, no parity */
    USART_InitTypeDef usart;
    usart.USART_BaudRate            = 115200;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx;
    USART_Init(USART1, &usart);

    /* Bật USART1 */
    USART_Cmd(USART1, ENABLE);
}

/* Ghi chuỗi log ra file */
void Log_Print(const char* str)
{
    while (*str)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

/***********************************************************/
static void BSW_Init(void)
{
    /* 1. MCAL: CAN Driver – cấu hình bxCAN (CAN1) */
    Can_Init();

    /* 2. ECU Abstraction: CAN Interface */
    CanIf_Init();

    /* 4. Service: Transport Protocol */
    CanTp_Init();

    /* 5. Service: PDU Router – nạp bảng routing CAN */
    PduR_Init(&PduR_ConfigPB);

    /* 7. Service: Diagnostic Response */
    Dcm_Init();

}

/* Hàm gửi yêu cầu chẩn đoán chuẩn UDS*/
Std_ReturnType Diagnostic_Send_Request(PduIdType serviceId, const uint8* requestData, PduLengthType requestLength)
{

    /* Kiểm tra requestData */
    if (requestData == NULL_PTR || requestLength > DCM_RX_BUFFER_SIZE)
    {
        return E_NOT_OK;
    }

    /* Kiểm tra requestLength */
    if (requestLength == 0U)
    {
        return E_NOT_OK;
    }

    /* Dựng gói tin PDU chuẩn truyển cho xuống phía dưới */
    static PduInfoType pduInfo;
    pduInfo.SduDataPtr = (uint8 *)requestData;
    pduInfo.SduLength = requestLength;

    /* Cập nhật var debug */
    diag_last_response_len = 0u;
    diag_last_complete = 0u;
    diag_last_sid = 0u;

    /* Gọi PduR để gửi dữ liệu với ID là ID CanTp */
    if (serviceId == DiagConf_DiagIPdu_UDS_SessionControl || serviceId == DiagConf_DiagIPdu_UDS_ReadDataByIdentifier
        || serviceId == DiagConf_DiagIPdu_UDS_ReadDTCInformation
        || serviceId == DiagConf_DiagIPdu_UDS_ClearDiagnosticInformation || serviceId == DiagConf_DiagIPdu_UDS_InvalidService)
    {
        /* Gửi yêu cầu chẩn đoán xuống PduR */
        return PduR_DiagTransmit(DiagConf_CanTpTxNSdu_DiagTx, &pduInfo);
    }
    else
    {
        return E_NOT_OK;
    }
}


/* ===========================================================
 * Dcm_MainFunction – Xử lý dữ liệu nhận được từ DCM
 * ===========================================================*/
void Dcm_MainFunction(void)
{

    /* Chỉ xử lý khi nhận dữ liệu đã hoàn tất */
    if (Dcm_IsReceiving || Dcm_RxLength == 0U)
    {
        return;
    }

    char msg[128];
    uint8 serviceId = Dcm_RxBuffer[0];
    uint16 DID = 0;

    /* Cập nhật biến debug */
    diag_last_response_len = (Dcm_RxLength <= sizeof(diag_last_response))
                           ? Dcm_RxLength
                           : sizeof(diag_last_response);
    diag_last_sid = serviceId;

    for (uint16 i = 0; i < diag_last_response_len; i++)
    {
        diag_last_response[i] = Dcm_RxBuffer[i];
    }
    diag_last_complete = 1U;

    /* Filter data từ SID của response - byte đầu của data */
    switch (serviceId)
    {
        /* Positive Response */
        case 0x50: /* Diagnostic Session Control - 0x10 */
            sprintf(msg, "[DCM Response] Response UDS Message! Length=%u bytes, ServiceID=0x%02X\r\n", Dcm_RxLength, serviceId);
            Log_Print(msg);

            /* Hiển thị payload thực tế */
            Log_Print("[DCM Response] Payload Hex: ");
            for (PduLengthType i = 0; i < Dcm_RxLength && i < 16; i++)
            {
                sprintf(msg, "%02X ", Dcm_RxBuffer[i]);
                Log_Print(msg);
            }
            Log_Print("\r\n");
            break;

        case 0x62: /* Read Data by Identifier */
            if (Dcm_RxLength < 3U)
            {
                Log_Print("[DCM Response] Invalid ReadDataByIdentifier response length\r\n");
                break;
            }

            DID = (uint16)((Dcm_RxBuffer[1] << 8) | Dcm_RxBuffer[2]);
            sprintf(msg, "[DCM Response] Response UDS Message! Length=%u bytes, ServiceID=0x%02X, DID=0x%04X\r\n", Dcm_RxLength, serviceId, DID);
            Log_Print(msg);

            /* Hiển thị payload thực tế */
            Log_Print("[DCM Response] Payload Hex: ");
            for (PduLengthType i = 0; i < Dcm_RxLength && i < sizeof(Dcm_RxBuffer); i++)
            {
                sprintf(msg, "%02X ", Dcm_RxBuffer[i]);
                Log_Print(msg);
            }
            Log_Print("\r\n");

            /* Xử lý theo từng DID của response */
            if (DID == DiagConf_Did_ReadDataByIdentifier_VIN)
            {
                Log_Print("[DCM Response] VIN: ");
                for (PduLengthType i = 3; i < Dcm_RxLength; i++)
                {
                    sprintf(msg, "%c", Dcm_RxBuffer[i]);
                    Log_Print(msg);
                }
                Log_Print("\r\n");
            }
            else if (DID == DiagConf_Did_ReadDataByIdentifier_SoftwareVersion)
            {
                Log_Print("[DCM Response] Software Version: ");
                for (PduLengthType i = 3; i < Dcm_RxLength; i++)
                {
                    sprintf(msg, "%c", Dcm_RxBuffer[i]);
                    Log_Print(msg);
                }
                Log_Print("\r\n");
            }
            else if (DID == DiagConf_Did_ReadDataByIdentifier_EngineRPM)
            {
                if (Dcm_RxLength < 5U)
                {
                    Log_Print("[DCM Response] Invalid Engine RPM response length\r\n");
                    break;
                }

                uint16 rpm = (Dcm_RxBuffer[3] << 8) | Dcm_RxBuffer[4];
                sprintf(msg, "[DCM Response] Engine RPM: %u\r\n", rpm);
                Log_Print(msg);
            }
            else if (DID == DiagConf_Did_ReadDataByIdentifier_CoolantTemperature)
            {
                if (Dcm_RxLength < 4U)
                {
                    Log_Print("[DCM Response] Invalid Coolant Temperature response length\r\n");
                    break;
                }

                int8 temp = (int8)Dcm_RxBuffer[3];
                sprintf(msg, "[DCM Response] Coolant Temperature: %d °C\r\n", temp);
                Log_Print(msg);
            }
            else if (DID == DiagConf_Did_ReadDataByIdentifier_eVCUSnapshot)
            {
                Log_Print("[DCM Response] eVCU Snapshot Data: ");
                for (PduLengthType i = 3; i < Dcm_RxLength; i++)
                {
                    sprintf(msg, "%02X ", Dcm_RxBuffer[i]);
                    Log_Print(msg);
                }
                Log_Print("\r\n");
            }
            break;

        case 0x59: /* Read DTC Information */
            /* Xử lý phản hồi cho Read DTC Information */
            if (Dcm_RxLength < 3U)
            {
                Log_Print("[DCM Response] Invalid ReadDTCInformation response length\r\n");
                break;
            }

            sprintf(msg, "[DCM Response] Response UDS Message! Length=%u bytes, Sub-function=0x%02X\r\n", Dcm_RxLength, Dcm_RxBuffer[1]);
            Log_Print(msg);
            for (uint16 i = 3; i < Dcm_RxLength; i += 4)
            {
                if (i + 3 < Dcm_RxLength)
                {
                    uint32 dtc = ((uint32)Dcm_RxBuffer[i] << 16)
                               | ((uint32)Dcm_RxBuffer[i + 1] << 8)
                               | (uint32)Dcm_RxBuffer[i + 2];
                    uint8 status = Dcm_RxBuffer[i + 3];
                    sprintf(msg, "[DCM Response] DTC: 0x%06lX, Status: 0x%02X\r\n", (unsigned long)dtc, status);
                    Log_Print(msg);
                }
            }
            break;

        case 0x54: /* Clear Diagnostic Information */
            if (Dcm_RxLength < 1U)
            {
                Log_Print("[DCM Response] Invalid ClearDiagnosticInformation response length\r\n");
                break;
            }
            Log_Print("[DCM Response] Clear DTC Success! (0x54)\r\n");
            sprintf(msg, "[DCM Response] Response UDS Message! Length=%u bytes, Sub-function=0x%02X\r\n", Dcm_RxLength, Dcm_RxBuffer[1]);
            Log_Print(msg);
            break;

        default:    /* Nhận các Negative Response hoặc lỗi khác*/
            sprintf(msg, "[DCM Response] Unknown or Negative Response! Length=%u bytes, ServiceID=0x%02X\r\n", Dcm_RxLength, Dcm_RxBuffer[0]);
            Log_Print(msg);
            break;
    }

    /* Reset cờ nhận dữ liệu và lenght data */
    Dcm_IsReceiving = FALSE;
    Dcm_RxLength = 0U;
}


/* ===========================================================
 * API dựng data theo service UDS - Không gồm ISO-TP PCI byte
 * ===========================================================*/

 /* ===========================================================
 * Request vào extended session - 0x1003
 * ===========================================================*/
void Diag_Send_SessionControl(void)
{
    static uint8 requestDiag[2] = {0x10, 0x03};
    PduLengthType requestLength = sizeof(requestDiag);

    /*  Gửi yêu cầu chẩn đoán */
    Log_Print("\r\n[Diagnostic ECU] UDS Request: Session Control \r\n\r\n");
    Diagnostic_Send_Request(DiagConf_DiagIPdu_UDS_SessionControl, requestDiag, requestLength);
}

 /* ===========================================================
 * Request ReadDataByIdentifier  - 0x22 DID_H DID_L
 * DID gồm: VIN:                    0xF190
 *          Software Version:       0xF189
 *          Engin RPM:              0x010C
 *          Coolant Temperature:    0x0105
 *          eVCU Snapshot:          0xF001
 * ===========================================================*/
void Diag_Send_ReadDataByIdentifier(uint16 DId)
{
    static uint8 requestDiag[3];
    PduLengthType requestLength = sizeof(requestDiag);

    /* Kiểm tra các DID */
    if (DId == DiagConf_Did_ReadDataByIdentifier_VIN || DId == DiagConf_Did_ReadDataByIdentifier_SoftwareVersion
        || DId == DiagConf_Did_ReadDataByIdentifier_EngineRPM || DId == DiagConf_Did_ReadDataByIdentifier_CoolantTemperature
        || DId == DiagConf_Did_ReadDataByIdentifier_eVCUSnapshot)
    {
        /* Dựng payload theo từng DID */
        requestDiag[0] = DiagConf_DiagIPdu_UDS_ReadDataByIdentifier;
        requestDiag[1] = (uint8)(DId >> 8);   /* DID_H */
        requestDiag[2] = (uint8)(DId & 0xFF); /* DID_L */
    }
    else
    {
        /* Không hợp lệ, không gửi */
        return;
    }

    /* Gửi yêu cầu chẩn đoán */
    Log_Print("\r\n [Diagnostic ECU] UDS Request: Read Data by Identifier \r\n");
    Diagnostic_Send_Request(DiagConf_DiagIPdu_UDS_ReadDataByIdentifier, requestDiag, requestLength);
}

/* ===========================================================
 * Request ReadDTCInformation - 0x19 02 FF
 * Gồm: 0x19: ReadDTCInformation
 *      0x02: ReportDTCByStatusMask
 *      0xFF: Đọc tất cả DTC theo status mask
 * ===========================================================*/
void Diag_Send_ReadDTCInformation(void)
{
    static uint8 requestDiag[3] = {0x19, 0x02, 0xFF};
    PduLengthType requestLength = sizeof(requestDiag);

    /* Gửi yêu cầu chẩn đoán */
    Log_Print("\r\n [Diagnostic ECU] UDS Request: Read DTC Information 19 02 FF \r\n");
    Diagnostic_Send_Request(DiagConf_DiagIPdu_UDS_ReadDTCInformation, requestDiag, requestLength);
}

/* ===========================================================
 * Request ClearDiagnosticInformation - 0x14 FF FF FF
 * ===========================================================*/
void Diag_Send_ClearDiagnosticInformation(void)
{
    static uint8 requestDiag[4] = {0x14, 0xFF, 0xFF, 0xFF};
    PduLengthType requestLength = sizeof(requestDiag);

    /* Gửi yêu cầu chẩn đoán */
    Log_Print("\r\n [Diagnostic ECU] UDS Request: Clear Diagnostic Information \r\n");
    Diagnostic_Send_Request(DiagConf_DiagIPdu_UDS_ClearDiagnosticInformation, requestDiag, requestLength);
}

/* ===========================================================
 * Request Các Service không hỗ trợ - 0x27 01
 * ===========================================================*/
void Diag_Send_InvalidService(void)
{
    static uint8 requestDiag[2] = {0x27, 0x01};
    PduLengthType requestLength = sizeof(requestDiag);

    /* Gửi yêu cầu chẩn đoán */
    Log_Print("\r\n [Diagnostic ECU] UDS Request: Invalid Service \r\n");
    Diagnostic_Send_Request(DiagConf_DiagIPdu_UDS_InvalidService, requestDiag, requestLength);
}


int main(void)
{
    /* Khởi tạo UART in log */
    Log_Init();
    Log_Print("\r\n=== AUTOSAR Classic Diagnostic ECU Booted ===\r\n");

    /* Khởi tạo các module AUTOSAR BSW */
    BSW_Init();

    while (1)
    {
       /* Xử lý gửi Test UDS */
        static uint32 tick_count = 0;
        tick_count++;
        /* Chờ tín hiệu từ GDB hoặc tự động gửi mỗi 2 giây (200 ticks) */
        if (tick_count >= 200)
        {
            tick_count = 0;
            Diag_Send_ReadDTCInformation();
        }

        /* ---- Polling main functions ---- */
        /* Duy trì vòng lặp để cập nhật state machine và xử lý dữ liệu */
        Can_MainFunction_Read();
        CanTp_MainFunction();
        Can_MainFunction_Write();

        /* Sử dụng DCM để nhận và xử lý response */
        Dcm_MainFunction();

        /* Delay 1ms ổn định */
        delay_ms(1u);
    }
}
