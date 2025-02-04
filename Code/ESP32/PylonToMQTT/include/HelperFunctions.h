#pragma once
#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include "Defines.h"

boolean inline requiredParam(iotwebconf::WebRequestWrapper* webRequestWrapper, iotwebconf::InputParameter& param) {
	boolean valid = true;
	int paramLength = webRequestWrapper->arg(param.getId()).length();
	if (paramLength == 0)
	{
		param.errorMessage = "This field is required\n";
		valid = false;
	}
	return valid;
}

std::string inline formatDuration(unsigned long milliseconds) {
    const unsigned long MS_PER_SECOND = 1000;
    const unsigned long MS_PER_MINUTE = MS_PER_SECOND * 60;
    const unsigned long MS_PER_HOUR = MS_PER_MINUTE * 60;
    const unsigned long MS_PER_DAY = MS_PER_HOUR * 24;

    unsigned long days = milliseconds / MS_PER_DAY;
    milliseconds %= MS_PER_DAY;
    unsigned long hours = milliseconds / MS_PER_HOUR;
    milliseconds %= MS_PER_HOUR;
    unsigned long minutes = milliseconds / MS_PER_MINUTE;
    milliseconds %= MS_PER_MINUTE;
    unsigned long seconds = milliseconds / MS_PER_SECOND;

    std::string result = std::to_string(days) + " days, " +
                         std::to_string(hours) + " hours, " +
                         std::to_string(minutes) + " minutes, " +
                         std::to_string(seconds) + " seconds";

    return result;
}

unsigned long inline getTime() {
	time_t now;
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)) {
	return(0);
	}
	time(&now);
	return now;
}

template <typename T> String htmlConfigEntry(const char* label, T val)
{
	String s = "<li>";
	s += label;
	s += ": ";
	s += val;
	s += "</li>";
    return s;
}

void inline light_sleep(uint32_t sec )
{
  esp_sleep_enable_timer_wakeup(sec * 1000000ULL);
  esp_light_sleep_start();
}