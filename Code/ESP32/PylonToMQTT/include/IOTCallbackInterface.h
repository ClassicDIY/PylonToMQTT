#pragma once
#include "Arduino.h"


class IOTCallbackInterface
{
public:

    virtual void Publish(const char *subtopic, const char *value, boolean retained) = 0;
    virtual void Publish(const char *topic, float value, boolean retained) = 0;
    virtual void PublishDiscovery(int numberOfPacks) = 0;
};