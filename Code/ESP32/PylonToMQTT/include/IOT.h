#pragma once

#include "WiFi.h"
#include "ArduinoJson.h"
#include <EEPROM.h>
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include "Log.h"
#include "AsyncMqttClient.h"
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfESP32HTTPUpdateServer.h>
#include "Defines.h"
#include "Enumerations.h"
#include "IOTCallbackInterface.h"

namespace PylonToMQTT
{


class IOT : public IOTCallbackInterface
{
public:
    IOT();
    void Init();
    boolean Run();
    void Publish(const char *subtopic, const char *value, boolean retained = false);
    void Publish(const char *topic, float value, boolean retained = false);
    void PublishMessage(const char* topic, JsonDocument& payload);
    std::string getRootTopicPrefix();
    u_int getUniqueId() { return _uniqueId;};
    std::string getThingName();
    unsigned long PublishRate();
    void SetPublishRate(unsigned long rate);
private:


    bool _clientsConfigured = false;
    unsigned long _currentPublishRate;
    u_int _uniqueId = 0; // unique id from mac address NIC segment
};


} // namespace PylonToMQTT

extern PylonToMQTT::IOT _iot;
