/**********************************************************
 * @file    Com_Cfg.c
 * @brief   Bảng cấu hình COM – 3 CAN I-PDU + 2 LIN I-PDU
 **********************************************************/
#include "Com_Cfg.h"

boolean Com_Initialized = FALSE;

const Com_IPduCfgType Com_IPduCfg[COM_NUM_IPDUS] =
{
    /* CAN I-PDUs */
    { .PduId = ComConf_ComIPdu_EngineCmd,  .Length = 4u, .Direction = COM_PDU_DIR_RX },/* Com nhận IPDU từ eVCU qua CanID 0x180 */
    { .PduId = ComConf_ComIPdu_BrakeCmd,   .Length = 3u, .Direction = COM_PDU_DIR_TX },
    { .PduId = ComConf_ComIPdu_BodyCmd,    .Length = 4u, .Direction = COM_PDU_DIR_TX },
    /* LIN I-PDUs */
    { .PduId = ComConf_ComIPdu_LightCtrl,  .Length = 4u, .Direction = COM_PDU_DIR_TX },
    { .PduId = ComConf_ComIPdu_HVACCtrl,   .Length = 3u, .Direction = COM_PDU_DIR_TX },
    /* RX I-PDUs */
    { .PduId = ComConf_ComIPdu_EngineStatus, .Length = 8u, .Direction = COM_PDU_DIR_TX }, /* COM ECU -> eVCU I-PDU của EngineStatus*/
    { .PduId = ComConf_ComIPdu_TempSensor,   .Length = 2u, .Direction = COM_PDU_DIR_RX }
};



const Com_SignalCfgType Com_SignalCfg[COM_NUM_SIGNALS] =
{
    /* ===== EngineCmd (CAN 0x180) ===== */
    [ComSig_EngineCmd_Throttle]    = { .PduId = ComConf_ComIPdu_EngineCmd, .ByteIndex = 0u, .BitOffset = 0u, .BitLength = 8u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_RX },
    [ComSig_EngineCmd_Start]       = { .PduId = ComConf_ComIPdu_EngineCmd, .ByteIndex = 1u, .BitOffset = 0u, .BitLength = 1u, .Type = COM_SIGTYPE_BOOLEAN, .Direction = COM_PDU_DIR_RX },
    [ComSig_EngineCmd_TorqueLimit] = { .PduId = ComConf_ComIPdu_EngineCmd, .ByteIndex = 2u, .BitOffset = 0u, .BitLength = 8u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_RX },
    [ComSig_EngineCmd_Alive]       = { .PduId = ComConf_ComIPdu_EngineCmd, .ByteIndex = 3u, .BitOffset = 0u, .BitLength = 4u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_RX },
    [ComSig_EngineCmd_CRC]         = { .PduId = ComConf_ComIPdu_EngineCmd, .ByteIndex = 3u, .BitOffset = 4u, .BitLength = 4u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_RX },

    /* ===== BrakeCmd (CAN 0x280) ===== */
    [ComSig_Brake_BrakeReq] = { .PduId = ComConf_ComIPdu_BrakeCmd, .ByteIndex = 0u, .BitOffset = 0u, .BitLength = 8u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },
    [ComSig_Brake_RegenReq] = { .PduId = ComConf_ComIPdu_BrakeCmd, .ByteIndex = 1u, .BitOffset = 0u, .BitLength = 8u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },
    [ComSig_Brake_Alive]    = { .PduId = ComConf_ComIPdu_BrakeCmd, .ByteIndex = 2u, .BitOffset = 0u, .BitLength = 4u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },
    [ComSig_Brake_CRC]      = { .PduId = ComConf_ComIPdu_BrakeCmd, .ByteIndex = 2u, .BitOffset = 4u, .BitLength = 4u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },

    /* ===== BodyCmd (CAN 0x380) ===== */
    [ComSig_Body_Headlamp] = { .PduId = ComConf_ComIPdu_BodyCmd, .ByteIndex = 0u, .BitOffset = 0u, .BitLength = 1u, .Type = COM_SIGTYPE_BOOLEAN, .Direction = COM_PDU_DIR_TX },
    [ComSig_Body_TurnL]    = { .PduId = ComConf_ComIPdu_BodyCmd, .ByteIndex = 0u, .BitOffset = 1u, .BitLength = 1u, .Type = COM_SIGTYPE_BOOLEAN, .Direction = COM_PDU_DIR_TX },
    [ComSig_Body_TurnR]    = { .PduId = ComConf_ComIPdu_BodyCmd, .ByteIndex = 0u, .BitOffset = 2u, .BitLength = 1u, .Type = COM_SIGTYPE_BOOLEAN, .Direction = COM_PDU_DIR_TX },
    [ComSig_Body_DoorLock] = { .PduId = ComConf_ComIPdu_BodyCmd, .ByteIndex = 0u, .BitOffset = 3u, .BitLength = 1u, .Type = COM_SIGTYPE_BOOLEAN, .Direction = COM_PDU_DIR_TX },

    /* ===== LightCtrl (LIN 0x10) ===== */
    [ComSig_Light_Headlamp]   = { .PduId = ComConf_ComIPdu_LightCtrl, .ByteIndex = 0u, .BitOffset = 0u, .BitLength = 1u, .Type = COM_SIGTYPE_BOOLEAN, .Direction = COM_PDU_DIR_TX },
    [ComSig_Light_DRL]        = { .PduId = ComConf_ComIPdu_LightCtrl, .ByteIndex = 0u, .BitOffset = 1u, .BitLength = 1u, .Type = COM_SIGTYPE_BOOLEAN, .Direction = COM_PDU_DIR_TX },
    [ComSig_Light_Brightness] = { .PduId = ComConf_ComIPdu_LightCtrl, .ByteIndex = 1u, .BitOffset = 0u, .BitLength = 8u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },

    /* ===== HVACCtrl (LIN 0x11) ===== */
    [ComSig_HVAC_FanSpeed]    = { .PduId = ComConf_ComIPdu_HVACCtrl,  .ByteIndex = 0u, .BitOffset = 0u, .BitLength = 8u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },

    /* ===== EngineStatus (CAN 0x181) - RX ===== */
    /* Pack Signal của EngineStatus (CAN 0x181) - TX COM ECU -> eVCU */
    [ComSig_Engine_RPM]       = { .PduId = ComConf_ComIPdu_EngineStatus, .ByteIndex = 0u, .BitOffset = 0u, .BitLength = 16u, .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },
    [ComSig_Engine_Temp]      = { .PduId = ComConf_ComIPdu_EngineStatus, .ByteIndex = 2u, .BitOffset = 0u, .BitLength = 8u,  .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },
    [ComSig_Engine_TorqueActual] = { .PduId = ComConf_ComIPdu_EngineStatus, .ByteIndex = 3u, .BitOffset = 0u, .BitLength = 8u,  .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },
    [ComSig_Engine_State]     = { .PduId = ComConf_ComIPdu_EngineStatus, .ByteIndex = 4u, .BitOffset = 0u, .BitLength = 8u,  .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },
    [ComSig_Engine_Alive]     = { .PduId = ComConf_ComIPdu_EngineStatus, .ByteIndex = 5u, .BitOffset = 0u, .BitLength = 4u,  .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },
    [ComSig_Engine_CRC]       = { .PduId = ComConf_ComIPdu_EngineStatus, .ByteIndex = 5u, .BitOffset = 4u, .BitLength = 4u,  .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_TX },

    /* ===== TempSensor (LIN 0x20) - RX ===== */
    [ComSig_Temp_Value]       = { .PduId = ComConf_ComIPdu_TempSensor,   .ByteIndex = 0u, .BitOffset = 0u, .BitLength = 8u,  .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_RX },
    [ComSig_Temp_Status]      = { .PduId = ComConf_ComIPdu_TempSensor,   .ByteIndex = 1u, .BitOffset = 0u, .BitLength = 8u,  .Type = COM_SIGTYPE_UINT8,   .Direction = COM_PDU_DIR_RX }
};

const Com_ConfigType Com_Config =
{
    .Com_IPduCfg = Com_IPduCfg,
    .Com_SignalCfg = Com_SignalCfg
};