#pragma once
#include "Arduino.h"
#include "Enumerations.h"

class AsyncSerialCallbackInterface
{
public:
    virtual void complete() = 0;
	virtual void overflow() = 0;
	virtual void timeout() = 0;
};

class AsyncSerial
{
 public:
	AsyncSerial();
	~AsyncSerial();
	void begin(AsyncSerialCallbackInterface* cbi, unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin);
	void Receive(int timeOut);
	void Send(CommandInformation cmd, byte* data, size_t dataLength);
	byte* GetContent();
	uint16_t GetContentLength();
	CommandInformation GetToken() { return _command; };

	unsigned long Timeout = 0;
	char EOIChar = '\r';
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
	 AsyncSerialCallbackInterface* _cbi;
};



