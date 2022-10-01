#include "AsyncSerial.h"

#define BufferSize 1024

AsyncSerial::AsyncSerial(AsyncSerialCallback onReceivedOk, AsyncSerialCallback onTimeout, AsyncSerialCallback onOverflow)
{
	_status = IDDLE;
	_buffer = (byte*)malloc(BufferSize);
	_bufferLength = BufferSize;
	_bufferIndex = 0;
	OnReceivedOk = onReceivedOk;
	OnTimeout = onTimeout;
	OnOverflow = onOverflow;
}

AsyncSerial::~AsyncSerial()
{
	free(_buffer);
}

void AsyncSerial::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin)
{
	Serial2.begin(baud, config, rxPin, txPin);
	_stream = &Serial2;
}

AsyncSerial::Status AsyncSerial::Receive(int timeOut)
{
	if (_status != RECEIVING_DATA) { return _status; }
	Timeout = timeOut;
	_startTime = millis();
	while (_status < MESSAGE_RECEIVED)
	{
		if (IsExpired())
		{
			if (OnTimeout != nullptr) OnTimeout(*this);
			_status = TIMEOUT;
		}
		if (_status == RECEIVING_DATA)
		{
			while (_stream->available())
			{
				byte newData = _stream->read();
				if (newData == (byte)FinishChar) {
					_status = MESSAGE_RECEIVED;
					if (OnReceivedOk != nullptr) OnReceivedOk(*this);
					_bufferIndex = 0;
					_status == IDDLE;
				}
				else {
					if (_bufferIndex >= _bufferLength)
					{
						_bufferIndex %= _bufferLength;
						_status = RECEIVING_DATA_OVERFLOW;
						if (OnOverflow != nullptr) OnOverflow(*this);
					}
					_buffer[_bufferIndex] = newData;
					_bufferIndex++;
				}
			}
		}
	}
	return _status;
}

void AsyncSerial::Send(byte* data, size_t dataLength)
{
	_stream->write(data, dataLength);
	_status = RECEIVING_DATA;
}

inline bool AsyncSerial::IsExpired()
{
	if (Timeout == 0) return false;
	return ((unsigned long)(millis() - _startTime) > Timeout);
}

byte * AsyncSerial::GetContent()
{
	return _buffer;
}

uint8_t AsyncSerial::GetContentLength()
{
	return _bufferIndex;
}
