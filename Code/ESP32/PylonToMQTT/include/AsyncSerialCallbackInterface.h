#pragma once
#include <Arduino.h>

class AsyncSerialCallbackInterface
{
public:
    virtual void complete() = 0;
	virtual void overflow() = 0;
	virtual void timeout() = 0;
};