#pragma once
#include <stdint.h>
#include <WString.h>

// struct JakiperInfo
// {
// 	float BatCurrent = 0;
//     float BatVoltage = 0;
// 	uint16_t SOC = 0;
//     uint16_t SOH = 0;
//     float RemainingCapacity = 0;
//     float FullCapacity = 0;
//     uint16_t CycleCount = 0;
//     float Cell1 = 0;
//     float Cell2 = 0;
//     float Cell3 = 0;
//     float Cell4 = 0;
//     float Cell5 = 0;
//     float Cell6 = 0;
//     float Cell7 = 0;
//     float Cell8 = 0;
//     float Cell9 = 0;
//     float Cell10 = 0;
//     float Cell11 = 0;
//     float Cell12 = 0;
//     float Cell13 = 0;
//     float Cell14 = 0;
//     float Cell15 = 0;
//     float Cell16 = 0;
//     float Cell1Temp = 0;
//     float Cell2Temp = 0;
//     float Cell3Temp = 0;
//     float Cell4Temp = 0;
//     float MosTemp = 0;
//     float EnvTemp = 0;

// };

enum CommandInformation : byte
{
    None = 0x00,
    AnalogValueFixedPoint = 0x42,
    AlarmInfo = 0x44,
    SystemParameterFixedPoint = 0x47,
    ProtocolVersion = 0x4f,
    ManufacturerInfo = 0x51,
    GetChargeDischargeManagementInfo = 0x92,
    Serialnumber = 0x93,
    FirmwareInfo = 0x96,
    GetVersionInfo = 0xc1,
    GetBarCode = 0xc2,
    GetPackCount = 0x90,
};

enum ResponseCode 
{
    Normal = 0x00,
    VER_error = 0x01,
    CHKSUM_error = 0x02,
    LCHKSUM_error = 0x03,
	CID2invalid = 0x04,
    CommandFormat_error = 0x05,
	InvalidData = 0x06,
    ADR_error = 0x90,
    CID2Communicationinvalid_error = 0x91
};