#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "Log.h"
#include "Enumerations.h"
#include "IOT.h"
#include "Pylon.h"

using namespace PylonToMQTT;

WebServer _webServer(80);
IOT _iot = IOT(&_webServer);
unsigned long _lastPublishTimeStamp = 0;

PylonToMQTT::Pylon _Pylon = PylonToMQTT::Pylon();
hw_timer_t *_watchdogTimer = NULL;

void IRAM_ATTR resetModule()
{
	// ets_printf("watchdog timer expired - rebooting\n");
	esp_restart();
}

void init_watchdog()
{
	if (_watchdogTimer == NULL)
	{
		_watchdogTimer = timerBegin(0, 80, true);					   //timer 0, div 80
		timerAttachInterrupt(_watchdogTimer, &resetModule, true);	  //attach callback
		timerAlarmWrite(_watchdogTimer, WATCHDOG_TIMER * 1000, false); //set time in us
		timerAlarmEnable(_watchdogTimer);							   //enable interrupt
	}
}

void feed_watchdog()
{
	if (_watchdogTimer != NULL)
	{
		timerWrite(_watchdogTimer, 0); // feed the watchdog
	}
}

void setup()
{
	Serial.begin(115200);
	while (!Serial) {}
	_iot.Init(&_Pylon);
	init_watchdog();
	_lastPublishTimeStamp = millis() + COMMAND_PUBLISH_RATE;
	// Set up object used to communicate with battery, provide callback to MQTT publish
	_Pylon.begin(&_iot);
	logd("Setup Done");
}

void loop()
{

	if (_iot.Run()) {
		_Pylon.Receive(SERIAL_RECEIVE_TIMEOUT); 
		if (_lastPublishTimeStamp < millis())
		{
			feed_watchdog();
			unsigned long currentPublishRate = _Pylon.Transmit() == true ? _iot.PublishRate() : COMMAND_PUBLISH_RATE;
			_lastPublishTimeStamp = millis() + currentPublishRate;
		}
	}
	else {
		feed_watchdog(); // don't reset when not configured
	}
}
