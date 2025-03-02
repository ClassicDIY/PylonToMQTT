#pragma once
#include "Arduino.h"
#include "ArduinoJson.h"


class IOTServiceInterface
{
public:

    virtual boolean Publish(const char *subtopic, const char *value, boolean retained) = 0;
    virtual boolean Publish(const char *subtopic, float value, boolean retained) = 0;
    virtual boolean PublishMessage(const char* topic, JsonDocument& payload, boolean retained) = 0;
    virtual boolean PublishHADiscovery(const char *bank, JsonDocument& payload) = 0;
    virtual std::string getRootTopicPrefix() = 0;
    virtual std::string getSubtopicName() = 0;
    virtual u_int getUniqueId() = 0;
    virtual std::string getThingName() = 0;
    virtual void Online() = 0;
};