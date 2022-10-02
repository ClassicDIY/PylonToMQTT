#pragma once
#include "Arduino.h"
#include "Enumerations.h"

const char CARRIAGE_RETURN = '\r';

class AsyncSerial
{
	typedef void(*AsyncSerialCallback)(AsyncSerial& sender);

 public:
	 typedef enum
	 {
		 IDDLE,
		 RECEIVING_DATA,
		 RECEIVING_DATA_OVERFLOW,
		 MESSAGE_RECEIVED,
		 TIMEOUT,
	}  Status;

	AsyncSerial(AsyncSerialCallback OnReceivedOk, AsyncSerialCallback OnTimeout = nullptr , AsyncSerialCallback OnOverflow = nullptr);
	~AsyncSerial();
	void begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin);

	void Receive(int timeOut);
	void Send(CommandInformation cmd, byte* data, size_t dataLength);
	byte* GetContent();
	uint16_t GetContentLength();
	CommandInformation GetToken() { return _command; };

	AsyncSerialCallback OnReceivedOk;
	AsyncSerialCallback OnOverflow;
	AsyncSerialCallback OnTimeout;

	unsigned long Timeout = 0;
	char EOIChar = CARRIAGE_RETURN;
	char SOIChar = '~';

 protected:
 	inline bool IsExpired();
	 Stream* _stream;
	 byte *_buffer;
	 size_t _bufferIndex;
	 size_t _bufferLength;
	 unsigned long _startTime;
	 Status _status;
	 CommandInformation _command; 
};



