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
#include "IOTCallbackInterface.h"

#define CONFIG_VERSION "V1.1.2" // major.minor.build (major or minor will invalidate the configuration)
#define HOME_ASSISTANT_PREFIX "homeassistant" // MQTT prefix used in autodiscovery
#define STR_LEN 255                            // general string buffer size
#define CONFIG_LEN 32                         // configuration string buffer size
#define NUMBER_CONFIG_LEN 6
#define AP_TIMEOUT 30000
#define MAX_PUBLISH_RATE 30000
#define MIN_PUBLISH_RATE 1000

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
    void PublishDiscovery(int numberOfPacks);
    unsigned long PublishRate();
    void SetPublishRate(unsigned long rate);
private:
    void PublishDiscoverySub(const char *device_class, const char *unit_of_meas, const char *entity, const char *item, const char *pack);
    bool _clientsConfigured = false;
    unsigned long _currentPublishRate;
    u_int _uniqueId = 0; // unique id from mac address NIC segment
};


} // namespace PylonToMQTT

extern PylonToMQTT::IOT _iot;