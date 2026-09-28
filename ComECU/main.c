/**********************************************************
 * @file    main.c
 * @brief   AUTOSAR Classic Diagnostic – STM32F103
 *
 * @details Ứng dụng phát triển xử lý gửi và nhận dữ liệu chẩn đoán UDS (Unified Diagnostic Services)
 *          trên vi điều khiển STM32F103
 *          trong kiến trúc AUTOSAR Classic Communication Stack.
 *
 * @version 1.0
 * @date    06/08/2026
 * @author  Duy Dang
 **********************************************************/

/* ===== AUTOSAR BSW Includes ===== */
#include "Com.h"               /* COM: Com_SendSignal, Com_TriggerIPDUSend */
#include "Com_Cfg.h"           /* Cấu hình COM: Signal ID, I-PDU ID  */
#include "PduR.h"              /* PDU Router: PduR_Init               */
#include "PduR_Cfg.h"          /* Cấu hình PduR: bảng route           */
#include "CanTp.h"             /* CAN Transport Protocol               */
#include "CanTp_Cfg.h"         /* Cấu hình CanTp                       */
#include "CanIf.h"             /* CAN Interface: CanIf_Init            */
#include "Can.h"               /* CAN Driver: Can_Init                 */
#include "Dcm.h"               /* Diagnostic Communication Manager     */

/* ===== SPL Includes ===== */
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include <string.h>
#include <stdio.h>

/***********************************************************/

/* Biến lưu trữ giá trị alive */
static uint8 s_alive = 0u;

/* GDB/tool control symbols used by evcu_tester.py. */
volatile uint8  com_trigger_engine_status = 0u;
volatile uint16 com_tx_rpm = 3000u;
volatile uint8  com_tx_temp = 90u;
volatile uint8  com_rx_vehicle_command_seen = 0u;

/* ===========================================================
 * Giả lập delay bằng NOP
 * 7200 vòng NOP ≈ 1ms tại 72MHz (mỗi NOP ≈ 1 cycle)
 * ===========================================================*/
static void delay_ms(volatile uint32 ms)
{
    while (ms--)
    {
        for (volatile uint32 i = 0; i < 7200u; i++)
        {
            __asm__("nop");
        }
    }
}

/* ===========================================================
 * Log_Init / Log_Print – In debug log qua USART1
 * -----------------------------------------------------------
 * Cấu hình USART1 trên PA9 (TX) để in log ra console.
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
    CanIf_Init(&CanIf_Config);

    /* 5. Service: PDU Router – nạp bảng routing CAN */
    PduR_Init(&PduR_ConfigPB);

    /* 7. Service: Com với các signal */
    Com_Init(&Com_Config);

}

/**
 * @brief Gửi Engine status qua COM với Pack signal đã cấu hình
 * @details Gửi các signal: RPM, Temp, Torque, State, Alive, CRC
 *          vào I-PDU EngineStatus (CAN 0x181)
 */
static void prv_SendEngineStatus(void)
{
    /* Engine RPM 2 byte: byte 0 và byte 1 */
    uint16 rpm = com_tx_rpm;
    Com_SendSignal(ComSig_Engine_RPM, &rpm);

    /* Engine Temperature 1 byte: byte 2 */
    uint8 temp = com_tx_temp;
    Com_SendSignal(ComSig_Engine_Temp, &temp);

    /* Engine Torque 1 byte: byte 3 */
    uint8 torque = 100u;
    Com_SendSignal(ComSig_Engine_TorqueActual, &torque);

    /* Engine state 1 byte: byte 4 */
    uint8 engineState = 2u;  /* Running */
    Com_SendSignal(ComSig_Engine_State, &engineState);

    /* Alive Counter: 4-bit (0..15), tự tăng mỗi lần gửi → byte 5 bit [0..3] */
    uint8 alive = s_alive & 0x0Fu;
    Com_SendSignal(ComSig_Engine_Alive, &alive);

    /* CRC: XOR đơn giản các giá trị → byte 5 bit [4..7] */
    uint8 crc = (uint8)(rpm ^ temp ^ torque ^ alive) & 0x0Fu;
    Com_SendSignal(ComSig_Engine_CRC, &crc);

    /* In log để check */
    char msg[128];
    sprintf(msg, "\r\n [COM ECU]: Sending EngineStatus (alive=%u) -> RPM=%u, Temp=%u, Torque=%u, EngineState=%u, CRC=%u \r\n",
        alive, rpm, temp, torque, engineState, crc);
    Log_Print(msg);

    /* Gửi I-PDU xuống tầng dưới */
    Com_TriggerIPDUSend(ComConf_ComIPdu_EngineStatus);

    /* Tăng alive counter cho lần gửi tiếp theo */
    s_alive++;
}

/**
 * @brief Xử lý khi nhận được I-PDU từ eVCU
 */
void App_ComRxIndication(PduIdType ComRxPduId)
{
    if (ComRxPduId == ComConf_ComIPdu_EngineCmd)
    {
        com_rx_vehicle_command_seen = 1u;

        /* Nhận I-PDU VehicleCmd */
        uint8 throttle = 0xFFu;
        boolean start = FALSE;
        uint8 torqueLimit = 0xFFu;
        uint8 alive = 0xFFu;
        uint8 crc = 0xFFu;

        /* Đọc các signal từ COM shadow buffer sau khi nhận I-PDU */
        Com_ReceiveSignal(ComSig_EngineCmd_Throttle, &throttle);
        Com_ReceiveSignal(ComSig_EngineCmd_Start, &start);
        Com_ReceiveSignal(ComSig_EngineCmd_TorqueLimit, &torqueLimit);
        Com_ReceiveSignal(ComSig_EngineCmd_Alive, &alive);
        Com_ReceiveSignal(ComSig_EngineCmd_CRC, &crc);

        /* In log để check */
        char msg[128];
        sprintf(msg, "\r\n [COM ECU]: Received VehicleCmd from eVCU -> Throttle=%u, Start=%u, TorqueLimit=%u, Alive=%u, CRC=%u \r\n",
                throttle, start, torqueLimit, alive, crc);
        Log_Print(msg);
    }
    else
    {
        /* XỬ lý các I-PDU khác */
        return;
    }
}

int main(void)
{
    /* Khởi tạo UART in log */
    Log_Init();
    Log_Print("\r\n--- AUTOSAR Classic COM ECU Booted ---\r\n");

    /* Khởi tạo các module AUTOSAR BSW */
    BSW_Init();

    while (1)
    {
        static uint32 tick_count = 0;
        tick_count++;

        /* Gửi Engine status từ COM ECU đến eVCU chu kỳ 100ms*/
        if (tick_count >=100)
        {
            tick_count = 0U;
            prv_SendEngineStatus();
        }

        /* ---- Polling main functions ---- */
        /* Duy trì vòng lặp để cập nhật state machine và xử lý dữ liệu */
        Can_MainFunction_Read();
        Can_MainFunction_Write();
    }
}
