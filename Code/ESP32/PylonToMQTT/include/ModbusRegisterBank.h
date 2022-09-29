#pragma once
#include <stdint.h>
#include <WString.h>


typedef struct 
{
	bool received;
	uint32_t token;
	int address;
	int numberOfRegisters;
	std::function<void(ModbusMessage data)> func;
} ModbusRegisterBank;


