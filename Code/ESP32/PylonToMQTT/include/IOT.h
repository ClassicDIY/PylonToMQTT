#pragma once

#include "WiFi.h"
#include "ArduinoJson.h"
#include <EEPROM.h>
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <sstream>
#include <string>
#include <AsyncMqttClient.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfESP32HTTPUpdateServer.h>
#include <IotWebConfOptionalGroup.h>
#include <IotWebConfTParameter.h>
#include "Defines.h"
#include "IOTServiceInterface.h"
#include "IOTCallbackInterface.h"

namespace PylonToMQTT
{
    class IOT : public IOTServiceInterface
    {
    public:
        IOT() {};
        void Init(IOTCallbackInterface *iotCB);

        boolean Run();
        boolean Publish(const char *subtopic, const char *value, boolean retained = false);
        boolean Publish(const char *subtopic, JsonDocument &payload, boolean retained = false);
        boolean Publish(const char *subtopic, float value, boolean retained = false);
        boolean PublishMessage(const char *topic, JsonDocument &payload, boolean retained);
        boolean PublishHADiscovery(const char *bank, JsonDocument &payload);
        std::string getRootTopicPrefix();
        std::string getSubtopicName();
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
        hw_timer_t *_watchdogTimer = NULL;
    };

    const char reboot_html[] PROGMEM = R"rawliteral(
        <!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>
        <title>ESP32 Reboot</title>
        </head><body>
        <h1>Rebooting ESP32</h1>
        <p><a href='settings'>Return to  Settings after reboot has completed.</a></p>
        </body></html>
        )rawliteral";
    
        const char redirect_html[] PROGMEM = R"rawliteral(
            <!DOCTYPE html><html lang=\"en\"><head><meta http-equiv="refresh" content="5;url={h}">
                <title>Redirecting...</title>
            </head>
            <body>
                <p><a href='/' onclick="javascript:event.target.port={hp}>Return to home page after reboot has completed.</a></p>
            </body>
            </html>
            )rawliteral";

} // namespace PylonToMQTT
