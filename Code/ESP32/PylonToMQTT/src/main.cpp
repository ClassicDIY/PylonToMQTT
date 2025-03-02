#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "Enumerations.h"
#include "Log.h"
#include "IOT.h" 
#include "Pylon.h"

using namespace PylonToMQTT;

IOT _iot = IOT();	
unsigned long _lastPublishTimeStamp = 0;
Pylon _Pylon = Pylon();

void setup()
{
	Serial.begin(115200);
	while (!Serial) {}	
	_lastPublishTimeStamp = millis() + COMMAND_PUBLISH_RATE;
	// Set up object used to communicate with battery, provide callback to MQTT publish
	_Pylon.begin(&_iot);
	_iot.Init(&_Pylon);
	logd("Setup Done");
}

void loop()
{
	_Pylon.Process();
	if (_iot.Run()) {
		_Pylon.Receive(SERIAL_RECEIVE_TIMEOUT); 
		if (_lastPublishTimeStamp < millis())
		{
			unsigned long currentPublishRate = _Pylon.Transmit() == true ? _iot.PublishRate() : COMMAND_PUBLISH_RATE;
			_lastPublishTimeStamp = millis() + currentPublishRate;
		}
	}
}
