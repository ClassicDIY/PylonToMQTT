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
#include "IOTServiceInterface.h"
#include "IOTCallbackInterface.h"

namespace PylonToMQTT
{
    class IOT : public IOTServiceInterface
    {
    public:
        IOT(WebServer* pWebServer);
        void Init(IOTCallbackInterface *iotCB);
        boolean Run();
        boolean Publish(const char *subtopic, const char *value, boolean retained = false);
        boolean Publish(const char *subtopic, JsonDocument &payload, boolean retained = false);
        boolean Publish(const char *subtopic, float value, boolean retained = false);
        boolean PublishMessage(const char *topic, JsonDocument &payload, boolean retained);
        boolean PublishHADiscovery(const char *bank, JsonDocument &payload);
        std::string getRootTopicPrefix();
        std::string getBankName();
        u_int getUniqueId() { return _uniqueId; };
        std::string getThingName();
        void Online();
        IOTCallbackInterface *IOTCB() { return _iotCB; }
        unsigned long PublishRate();

    private:
        bool _clientsConfigured = false;
        IOTCallbackInterface *_iotCB;
        u_int _uniqueId = 0; // unique id from mac address NIC segment
        bool _publishedOnline = false;
    };

} // namespace PylonToMQTT

extern PylonToMQTT::IOT _iot;
