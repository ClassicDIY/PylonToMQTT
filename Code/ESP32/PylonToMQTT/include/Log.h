#pragma once

#include <Arduino.h>
#include "esp_log.h"
#include <time.h>

int weblog(const char *format, ...);

#if APP_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
#define logv(format, ...) weblog(ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)
#else
#define logv(format, ...)
#endif

#if APP_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
#define logd(format, ...) weblog(ARDUHAL_LOG_FORMAT(D, format), ##__VA_ARGS__)
#else
#define logd(format, ...)
#endif

#if APP_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
#define logi(format, ...) weblog(ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__)
#else
#define logi(format, ...)
#endif

#if APP_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
#define logw(format, ...) weblog(ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__)
#else
#define logw(format, ...)
#endif

#if APP_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
#define loge(format, ...) weblog(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#else
#define loge(format, ...)
#endif

void inline printLocalTime()
{
#if APP_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        logi("Failed to obtain time");
        return;
    }
    char buf[64];
    buf[0] = 0;
    strftime(buf, 64, "%A, %B %d %Y %H:%M:%S", &timeinfo);
    logi("Date Time: %s", buf);
#endif
}


