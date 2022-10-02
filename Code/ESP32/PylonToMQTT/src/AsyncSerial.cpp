#include "AsyncSerial.h"
#include "Log.h"


#define BufferSize 2048

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
	while (!Serial2) {}
	_stream = &Serial2;
}

void AsyncSerial::Receive(int timeOut)
{
	if (_status != RECEIVING_DATA) { return; }
	Timeout = timeOut;
	_startTime = millis();
	bool SOIfound = false;
	while (_status < MESSAGE_RECEIVED)
	{
		if (IsExpired())
		{
			_status = TIMEOUT;
			if (OnTimeout != nullptr) OnTimeout(*this);
		}
		if (_status == RECEIVING_DATA)
		{
			while (_stream->available())
			{
				byte newData = _stream->read();
				if (SOIfound) {
					if (newData == (byte)EOIChar) {
						_status = MESSAGE_RECEIVED;
						_buffer[_bufferIndex] = 0;
						if (OnReceivedOk != nullptr) OnReceivedOk(*this); // call service function to handle payload
					}
					else {
						if (_bufferIndex >= _bufferLength) {
							_status = RECEIVING_DATA_OVERFLOW;
							if (OnOverflow != nullptr) OnOverflow(*this);
						}
						else {
							_buffer[_bufferIndex++] = newData;
						}
					}
				}
				else if (newData == (byte)SOIChar) { // discard until SOI received
					_bufferIndex = 0;
					memset(_buffer, 0, BufferSize); // clear buffer on new SOI
					_buffer[_bufferIndex++] = newData;
					SOIfound = true;
				}
			}
		}
	}
	_bufferIndex = 0; // recieved, timedout or overflowed - reset for next message
	_status = IDDLE;
	return;
}

void AsyncSerial::Send(CommandInformation cmd, byte* data, size_t dataLength)
{
	if (_status != IDDLE) { return; }
	logd("send cmd: %d data: %s", cmd, data);
	_stream->write(data, dataLength);
	_status = RECEIVING_DATA;
	_command = cmd;
	return;
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

uint16_t AsyncSerial::GetContentLength()
{
	return _bufferIndex;
}
