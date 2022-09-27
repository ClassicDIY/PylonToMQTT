#pragma once
#include <stdint.h>
#include <WString.h>

struct JakiperInfo
{
	float BatCurrent = 0;
    float BatVoltage = 0;
	uint16_t SOC = 0;
    uint16_t SOH = 0;
    float RemainingCapacity = 0;
    float FullCapacity = 0;
    uint16_t CycleCount = 0;
    float Cell1 = 0;
    float Cell2 = 0;
    float Cell3 = 0;
    float Cell4 = 0;
    float Cell5 = 0;
    float Cell6 = 0;
    float Cell7 = 0;
    float Cell8 = 0;
    float Cell9 = 0;
    float Cell10 = 0;
    float Cell11 = 0;
    float Cell12 = 0;
    float Cell13 = 0;
    float Cell14 = 0;
    float Cell15 = 0;
    float Cell16 = 0;
    float Cell1Temp = 0;
    float Cell2Temp = 0;
    float Cell3Temp = 0;
    float Cell4Temp = 0;
    float MosTemp = 0;
    float EnvTemp = 0;

};