#pragma once
#include "Arduino.h"

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

	AsyncSerial(AsyncSerialCallback OnReceivedOk, AsyncSerialCallback OnOverflow = nullptr, AsyncSerialCallback OnTimeout = nullptr );
	~AsyncSerial();
	void begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin);

	Status Receive(int timeOut);
	void Send(byte* data, size_t dataLength);
	byte* GetContent();
	uint8_t GetContentLength();
	inline bool IsExpired();

	AsyncSerialCallback OnReceivedOk;
	AsyncSerialCallback OnOverflow;
	AsyncSerialCallback OnTimeout;

	unsigned long Timeout = 0;
	char FinishChar = CARRIAGE_RETURN;

 protected:
	 Stream* _stream;
	 byte *_buffer;
	 size_t _bufferIndex;
	 size_t _bufferLength;
	 unsigned long _startTime;
	 Status _status;
};



